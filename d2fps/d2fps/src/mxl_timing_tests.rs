use super::*;
use core::sync::atomic::{AtomicUsize, Ordering};
use windows_sys::Win32::System::Memory::{
  VirtualAlloc, VirtualFree, MEM_RELEASE, MEM_RESERVE, PAGE_EXECUTE_READ, PAGE_READWRITE,
};

#[test]
fn clock_selection_preserves_upstream_until_the_producer_is_patched() {
  assert_eq!(clock_domain(false, false), ClockDomain::LegacyClient);
  assert_eq!(clock_domain(true, false), ClockDomain::PreciseClient);
  assert_eq!(clock_domain(false, true), ClockDomain::SinglePlayer);
  assert_eq!(clock_domain(true, true), ClockDomain::SinglePlayer);
}

#[test]
fn interpolation_matches_checkpoint_boundaries_and_game_modes() {
  let modes = [
    d2::GameType::Sp,
    d2::GameType::Sp2,
    d2::GameType::Bnet,
    d2::GameType::OpenBnetHost,
    d2::GameType::OpenBnet,
    d2::GameType::TcpHost,
    d2::GameType::Tcp,
  ];
  let mut cases = 0;
  for interval in [400000i64, 400001, 0x100000001] {
    for elapsed in [
      -1,
      0,
      interval / 2,
      interval - 1,
      interval,
      interval + 1,
      interval + interval / 4,
      interval + interval / 2,
      interval * 2,
    ] {
      for mode in modes {
        for active in [false, true] {
          let timing = MxlTiming { client: if active { 1 } else { 0 } };
          let extra = if active && mode == d2::GameType::Bnet {
            interval / 2
          } else {
            0
          };
          let expected = if elapsed < 0 {
            -interval
          } else if elapsed > interval + extra {
            extra
          } else {
            elapsed - interval
          };
          assert_eq!(
            timing.interpolation_offset(elapsed, interval, mode),
            expected
          );
          cases += 1;
        }
      }
    }
  }
  assert_eq!(cases, 378);
  // Exact visible fractions at the original endpoint and bounded prediction end.
  assert_eq!(
    (interpolation_offset(400000, 400000, true) << 16) / 400000,
    0
  );
  assert_eq!(
    (interpolation_offset(600000, 400000, true) << 16) / 400000,
    32768
  );
  // Negative elapsed time still uses the original lower clamp.
  assert_eq!(interpolation_offset(i64::MIN, 400000, true), -400000);
}

#[test]
fn sha256_known_vector_and_optional_original_file() {
  assert_eq!(
    sha256(b"abc").unwrap(),
    [
      0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22,
      0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00,
      0x15, 0xad
    ]
  );
  if let Some(path) = std::env::var_os("D2FPS_TEST_CLIENT_FILE") {
    assert_eq!(sha256(&fs::read(path).unwrap()).unwrap(), CLIENT_SHA256);
  }
}

unsafe extern "system" fn old_clock() -> u32 {
  17
}
unsafe extern "system" fn precise_clock() -> u32 {
  91
}

struct TestImage(*mut u8);
impl TestImage {
  unsafe fn new() -> Self {
    let base = VirtualAlloc(
      ptr::null(),
      0x135000,
      MEM_COMMIT | MEM_RESERVE,
      PAGE_READWRITE,
    )
    .cast::<u8>();
    assert!(!base.is_null());
    ptr::write_unaligned(
      base.add(OLD_IAT).cast::<u32>(),
      old_clock as *const () as usize as u32,
    );
    ptr::write_unaligned(
      base.add(NEW_IAT).cast::<u32>(),
      precise_clock as *const () as usize as u32,
    );
    for &(offset, opcode) in &SITES {
      let code = instruction(opcode, (base as usize + OLD_IAT) as u32);
      ptr::copy_nonoverlapping(code.as_ptr(), base.add(offset), 6);
      if opcode[0] == 0x8b {
        let (push, call, pop) = if opcode[1] == 0x35 {
          (0x56, 0xd6, 0x5e)
        } else {
          (0x57, 0xd7, 0x5f)
        };
        base.add(offset - 1).write(push);
        ptr::copy_nonoverlapping([0xff, call, pop, 0xc3].as_ptr(), base.add(offset + 6), 4);
      } else {
        base.add(offset + 6).write(0xc3);
      }
    }
    let mut old = 0;
    assert_ne!(
      VirtualProtect(base.cast(), 0x135000, PAGE_EXECUTE_READ, &mut old),
      0
    );
    assert_ne!(
      FlushInstructionCache(GetCurrentProcess(), base.cast(), 0x135000),
      0
    );
    Self(base)
  }
  unsafe fn results(&self) -> [u32; 4] {
    SITES.map(|(offset, opcode)| {
      let entry = self.0.add(offset - usize::from(opcode[0] == 0x8b));
      let call: unsafe extern "system" fn() -> u32 = core::mem::transmute(entry);
      call()
    })
  }
  unsafe fn bytes(&self) -> [[u8; 6]; 4] {
    SITES.map(|(offset, _)| ptr::read_unaligned(self.0.add(offset).cast()))
  }
  unsafe fn assert_protection(&self) {
    let mut region: MEMORY_BASIC_INFORMATION = core::mem::zeroed();
    assert_ne!(
      VirtualQuery(
        self.0.add(SITES[0].0).cast(),
        &mut region,
        size_of::<MEMORY_BASIC_INFORMATION>()
      ),
      0
    );
    assert_eq!(region.Protect, PAGE_EXECUTE_READ);
  }
}
impl Drop for TestImage {
  fn drop(&mut self) {
    unsafe {
      VirtualFree(self.0.cast(), 0, MEM_RELEASE);
    }
  }
}

#[test]
fn all_four_native_instruction_forms_and_menu_reapply() {
  unsafe {
    let image = TestImage::new();
    assert_eq!(image.results(), [17; 4]);
    patch_operands(image.0 as isize, false, VirtualProtect).unwrap();
    assert_eq!(image.results(), [91; 4]);
    image.assert_protection();
    patch_operands(image.0 as isize, true, VirtualProtect).unwrap();
    assert_eq!(image.results(), [91; 4]);
    // Simulate another mod restoring one known original operand before menu load.
    let mut old = 0;
    let target = image.0.add(SITES[2].0);
    VirtualProtect(target.cast(), 6, PAGE_EXECUTE_READWRITE, &mut old);
    let original = instruction(SITES[2].1, (image.0 as usize + OLD_IAT) as u32);
    ptr::copy_nonoverlapping(original.as_ptr(), target, 6);
    let mut ignored = 0;
    VirtualProtect(target.cast(), 6, old, &mut ignored);
    patch_operands(image.0 as isize, true, VirtualProtect).unwrap();
    assert_eq!(image.results(), [91; 4]);
    image.assert_protection();
    // A second independent installer must not claim ownership of these clocks.
    assert!(patch_operands(image.0 as isize, false, VirtualProtect).is_err());
  }
}

#[test]
fn last_signature_mismatch_leaves_every_site_unchanged() {
  unsafe {
    let image = TestImage::new();
    let target = image.0.add(SITES[3].0);
    let mut old = 0;
    VirtualProtect(target.cast(), 6, PAGE_EXECUTE_READWRITE, &mut old);
    target.write(0x90);
    let mut ignored = 0;
    VirtualProtect(target.cast(), 6, old, &mut ignored);
    let before = image.bytes();
    assert!(patch_operands(image.0 as isize, false, VirtualProtect).is_err());
    assert_eq!(image.bytes(), before);
    assert!(patch_operands(image.0 as isize, true, VirtualProtect).is_err());
    assert_eq!(image.bytes(), before);
    image.assert_protection();
  }
}

static PROTECT_CALLS: AtomicUsize = AtomicUsize::new(0);
static FAIL_PROTECT_AT: AtomicUsize = AtomicUsize::new(0);
unsafe extern "system" fn injected_protect(
  p: *const c_void,
  n: usize,
  mode: u32,
  old: *mut u32,
) -> BOOL {
  if PROTECT_CALLS.fetch_add(1, Ordering::SeqCst) + 1 == FAIL_PROTECT_AT.load(Ordering::SeqCst) {
    0
  } else {
    VirtualProtect(p, n, mode, old)
  }
}

#[test]
fn protection_failure_rolls_back_bytes_and_permissions() {
  unsafe {
    for fail_at in [1, 2] {
      let image = TestImage::new();
      let before = image.bytes();
      PROTECT_CALLS.store(0, Ordering::SeqCst);
      FAIL_PROTECT_AT.store(fail_at, Ordering::SeqCst);
      assert!(patch_operands(image.0 as isize, false, injected_protect).is_err());
      assert_eq!(image.bytes(), before);
      assert_eq!(image.results(), [17; 4]);
      image.assert_protection();
    }
  }
}

#[test]
fn wrong_clock_import_or_simulation_interval_prevents_all_writes() {
  unsafe {
    let image = TestImage::new();
    let before = image.bytes();
    assert!(patch_clocks(image.0 as isize, false, VirtualProtect).is_err());
    let _ = timeGetTime(); // Ensure winmm is loaded in the test executable too.
    let precise = GetProcAddress(GetModuleHandleW(w!("winmm.dll")), s!("timeGetTime")).unwrap();
    for interval in [39, 40] {
      let mut old = 0;
      VirtualProtect(image.0.cast(), 0x135000, PAGE_READWRITE, &mut old);
      ptr::write_unaligned(image.0.add(NEW_IAT).cast::<u32>(), precise as usize as u32);
      ptr::write_unaligned(image.0.add(0xf4318).cast::<u32>(), interval);
      let mut ignored = 0;
      VirtualProtect(image.0.cast(), 0x135000, old, &mut ignored);
      let result = patch_clocks(image.0 as isize, false, VirtualProtect);
      if interval == 39 {
        assert!(result.is_err());
        assert_eq!(image.bytes(), before);
      } else {
        assert!(result.is_ok());
      }
      image.assert_protection();
    }
    assert!(verify_client(0).is_err());
    assert!(verify_client(GetModuleHandleW(ptr::null())).is_err());
  }
}
