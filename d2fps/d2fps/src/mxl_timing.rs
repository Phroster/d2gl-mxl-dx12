//! Guarded MXL client timing and bounded realm movement prediction.
//! The four client clock operands are still runtime patches. D2FPS's clock
//! choice and interpolation calculation are ordinary Rust code in this DLL.
use core::{ffi::c_void, mem::size_of, ptr};
use d2interface as d2;
use std::{ffi::OsString, fs, os::windows::ffi::OsStringExt};
use windows_sys::{
  s, w,
  Win32::{
    Foundation::{BOOL, HMODULE},
    Media::timeGetTime,
    Security::Cryptography::{
      CryptAcquireContextW, CryptCreateHash, CryptDestroyHash, CryptGetHashParam, CryptHashData,
      CryptReleaseContext, CRYPT_VERIFYCONTEXT, HP_HASHVAL, PROV_RSA_AES,
    },
    System::{
      Diagnostics::Debug::FlushInstructionCache,
      LibraryLoader::{GetModuleFileNameW, GetModuleHandleW, GetProcAddress},
      Memory::{
        VirtualProtect, VirtualQuery, MEMORY_BASIC_INFORMATION, MEM_COMMIT, PAGE_EXECUTE_READWRITE,
        PAGE_GUARD, PAGE_NOACCESS,
      },
      SystemInformation::{GetSystemInfo, GetTickCount, SYSTEM_INFO},
      Threading::GetCurrentProcess,
    },
  },
};

const CLIENT_SHA256: [u8; 32] = [
  0xdd, 0x8b, 0xc6, 0x02, 0x5d, 0xe9, 0x21, 0x21, 0x6a, 0x97, 0xc1, 0x7f, 0x97, 0xcd, 0x1a, 0x50,
  0xfb, 0xb8, 0x59, 0x26, 0xe8, 0x38, 0xec, 0x60, 0xe1, 0x34, 0x51, 0x44, 0x88, 0x36, 0xd9, 0x06,
];
const SITES: [(usize, [u8; 2]); 4] = [
  (0x44a81, [0x8b, 0x35]),
  (0x44b7c, [0xff, 0x15]),
  (0x44bb8, [0xff, 0x15]),
  (0x44c00, [0x8b, 0x3d]),
];
const OLD_IAT: usize = 0xcef5c;
const NEW_IAT: usize = 0xcf124;
type Protect = unsafe extern "system" fn(*const c_void, usize, u32, *mut u32) -> BOOL;

pub(crate) struct MxlTiming {
  client: HMODULE,
}

impl MxlTiming {
  pub const fn new() -> Self {
    Self { client: 0 }
  }

  /// Called only for v1.13c after normal D2FPS motion-smoothing hooks succeed.
  pub unsafe fn attach(client: HMODULE) -> Self {
    if GetModuleHandleW(w!("D2Sigma.dll")) == 0 {
      return Self::new();
    }
    let result: Result<(), &'static str> = (|| {
      verify_client(client)?;
      patch_clocks(client, false, VirtualProtect)?;
      Ok(())
    })();
    match result {
      Ok(()) => {
        log!(
          "MXL timing ACTIVE: four D2Client clocks verified; non-SP interpolation uses timeGetTime"
        );
        log!(
          "MXL timing: realm prediction +20 ms maximum; SP/LAN original clamp; simulation 25 Hz"
        );
        Self { client }
      }
      Err(reason) => {
        log!("MXL timing REFUSED: {reason}; keeping upstream timing behavior");
        Self::new()
      }
    }
  }

  pub unsafe fn reapply(&mut self) -> bool {
    if self.client == 0 {
      return true;
    }
    match patch_clocks(self.client, true, VirtualProtect) {
      Ok(()) => {
        log!("MXL timing: client clocks rechecked/reapplied after menu initialization");
        true
      }
      Err(reason) => {
        log!("MXL timing reapply REFUSED: {reason}");
        // Unknown code is never overwritten. The caller disables smoothing.
        false
      }
    }
  }

  pub fn current_time_ms(&self, is_sp: bool) -> u32 {
    unsafe {
      match clock_domain(self.client != 0, is_sp) {
        ClockDomain::SinglePlayer => timeGetTime() & 0x7fffffff,
        ClockDomain::PreciseClient => timeGetTime(),
        ClockDomain::LegacyClient => GetTickCount(),
      }
    }
  }

  pub fn interpolation_offset(&self, elapsed: i64, interval: i64, mode: d2::GameType) -> i64 {
    interpolation_offset(
      elapsed,
      interval,
      self.client != 0 && mode == d2::GameType::Bnet,
    )
  }
}

#[derive(Debug, PartialEq)]
enum ClockDomain {
  SinglePlayer,
  PreciseClient,
  LegacyClient,
}
fn clock_domain(active: bool, is_sp: bool) -> ClockDomain {
  if is_sp {
    ClockDomain::SinglePlayer
  } else if active {
    ClockDomain::PreciseClient
  } else {
    ClockDomain::LegacyClient
  }
}

fn interpolation_offset(elapsed: i64, interval: i64, realm: bool) -> i64 {
  // The original lower clamp, subtraction, and downstream fraction are retained.
  let limit = if realm {
    interval.saturating_add(interval / 2)
  } else {
    interval
  };
  elapsed.clamp(0, limit) - interval
}

fn sha256(bytes: &[u8]) -> Result<[u8; 32], &'static str> {
  unsafe {
    let mut provider = 0;
    if CryptAcquireContextW(
      &mut provider,
      ptr::null(),
      ptr::null(),
      PROV_RSA_AES,
      CRYPT_VERIFYCONTEXT,
    ) == 0
    {
      return Err("could not open SHA-256 provider");
    }
    let mut hash = 0;
    let mut digest = [0; 32];
    let mut length = 32;
    let ok = CryptCreateHash(provider, 0x800c /* CALG_SHA_256 */, 0, 0, &mut hash) != 0
      && CryptHashData(hash, bytes.as_ptr(), bytes.len() as u32, 0) != 0
      && CryptGetHashParam(hash, HP_HASHVAL, digest.as_mut_ptr(), &mut length, 0) != 0
      && length == 32;
    if hash != 0 {
      CryptDestroyHash(hash);
    }
    CryptReleaseContext(provider, 0);
    if ok {
      Ok(digest)
    } else {
      Err("could not hash D2Client")
    }
  }
}

unsafe fn verify_client(client: HMODULE) -> Result<(), &'static str> {
  if client == 0 {
    return Err("D2Client not loaded");
  }
  let mut path = [0; 32768];
  let length = GetModuleFileNameW(client, path.as_mut_ptr(), path.len() as u32) as usize;
  if length == 0 || length >= path.len() {
    return Err("could not locate D2Client file");
  }
  let bytes =
    fs::read(OsString::from_wide(&path[..length])).map_err(|_| "could not read D2Client")?;
  if bytes.len() != 1093632 {
    return Err("unsupported D2Client file size");
  }
  if sha256(&bytes)? != CLIENT_SHA256 {
    return Err("unsupported D2Client SHA-256");
  }
  let base = client as *const u8;
  let pe = read_u32(base.add(0x3c)) as usize;
  if ptr::read_unaligned(base.cast::<u16>()) != 0x5a4d
    || pe > 4096
    || read_u32(base.add(pe)) != 0x4550
    || ptr::read_unaligned(base.add(pe + 4).cast::<u16>()) != 0x14c
    || read_u32(base.add(pe + 8)) != 0x4b95ca3e
    || ptr::read_unaligned(base.add(pe + 24).cast::<u16>()) != 0x10b
    || read_u32(base.add(pe + 80)) != 0x135000
  {
    return Err("unexpected loaded D2Client PE identity");
  }
  Ok(())
}

unsafe fn read_u32(p: *const u8) -> u32 {
  ptr::read_unaligned(p.cast())
}

fn instruction(opcode: [u8; 2], operand: u32) -> [u8; 6] {
  let mut result = [0; 6];
  result[..2].copy_from_slice(&opcode);
  result[2..].copy_from_slice(&operand.to_le_bytes());
  result
}

/// All four operands occupy one code page. Validate every site and make that
/// page writable before any writes; restore the exact original bytes on failure.
unsafe fn patch_clocks(
  client: HMODULE,
  allow_installed: bool,
  protect: Protect,
) -> Result<(), &'static str> {
  let base = client as *mut u8;
  let precise = GetProcAddress(GetModuleHandleW(w!("winmm.dll")), s!("timeGetTime"))
    .ok_or("timeGetTime export missing")? as usize as u32;
  if read_u32(base.add(NEW_IAT)) != precise || read_u32(base.add(0xf4318)) != 40 {
    return Err("unexpected timeGetTime import or simulation interval");
  }
  patch_operands(client, allow_installed, protect)
}

unsafe fn patch_operands(
  client: HMODULE,
  allow_installed: bool,
  protect: Protect,
) -> Result<(), &'static str> {
  let base = client as *mut u8;
  let first = base.add(SITES[0].0);
  let span = SITES[3].0 + 6 - SITES[0].0;
  let mut system: SYSTEM_INFO = core::mem::zeroed();
  GetSystemInfo(&mut system);
  let page = system.dwPageSize as usize;
  if first as usize / page != (first as usize + span - 1) / page {
    return Err("clock sites do not share one code page");
  }
  let mut region: MEMORY_BASIC_INFORMATION = core::mem::zeroed();
  if VirtualQuery(
    first.cast(),
    &mut region,
    size_of::<MEMORY_BASIC_INFORMATION>(),
  ) == 0
    || region.State != MEM_COMMIT
    || region.Protect & (PAGE_NOACCESS | PAGE_GUARD) != 0
  {
    return Err("clock page is not readable");
  }
  let mut before = [[0u8; 6]; 4];
  let mut after = [[0u8; 6]; 4];
  for (i, &(offset, opcode)) in SITES.iter().enumerate() {
    before[i] = ptr::read_unaligned(base.add(offset).cast());
    let old = instruction(opcode, (client as usize + OLD_IAT) as u32);
    after[i] = instruction(opcode, (client as usize + NEW_IAT) as u32);
    if before[i] != old && !(allow_installed && before[i] == after[i]) {
      return Err("unexpected client clock instruction; no clock writes");
    }
  }
  let mut original_protection = 0;
  if protect(
    first.cast(),
    span,
    PAGE_EXECUTE_READWRITE,
    &mut original_protection,
  ) == 0
  {
    return Err("could not prepare clock page; no clock writes");
  }
  for (i, &(offset, _)) in SITES.iter().enumerate() {
    ptr::copy_nonoverlapping(after[i].as_ptr(), base.add(offset), 6);
  }
  let verified = FlushInstructionCache(GetCurrentProcess(), first.cast(), span) != 0
    && SITES
      .iter()
      .enumerate()
      .all(|(i, &(offset, _))| ptr::read_unaligned::<[u8; 6]>(base.add(offset).cast()) == after[i]);
  if !verified {
    for (i, &(offset, _)) in SITES.iter().enumerate() {
      ptr::copy_nonoverlapping(before[i].as_ptr(), base.add(offset), 6);
    }
    FlushInstructionCache(GetCurrentProcess(), first.cast(), span);
  }
  let mut ignored = 0;
  if protect(first.cast(), span, original_protection, &mut ignored) == 0 {
    for (i, &(offset, _)) in SITES.iter().enumerate() {
      ptr::copy_nonoverlapping(before[i].as_ptr(), base.add(offset), 6);
    }
    FlushInstructionCache(GetCurrentProcess(), first.cast(), span);
    if VirtualProtect(first.cast(), span, original_protection, &mut ignored) == 0 {
      log!("MXL timing ERROR: could not restore clock-page protection after rollback");
    }
    return Err("page-protection restore failed; prior clock bytes restored");
  }
  if verified {
    Ok(())
  } else {
    Err("clock readback failed; prior bytes restored")
  }
}

#[cfg(test)]
#[path = "mxl_timing_tests.rs"]
mod tests;
