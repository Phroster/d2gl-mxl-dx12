/* Median XL 1.13c multiplayer smoothing, integrated into D2GL.
 * Five clock operands plus a bounded, realm-only interpolation interval.
 * Does not access other processes, hook Windows globally, alter tick rates,
 * attach a debugger, bypass integrity checks, or modify original DLL files.
 */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "mxl_smoothing.h"
#include "mxl_visual_clock.h"
#include "mxl_render_clock.h"
#include <windows.h>
#include <wincrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef char RequireX86[(sizeof(void *) == 4) ? 1 : -1];
typedef struct {
    BYTE *instruction;
    BYTE opcode[2];
    DWORD old_iat;
    DWORD new_iat;
    const char *label;
} ClockSite;
typedef struct {
    BYTE *instruction;
    BYTE expected[32];
    BYTE replacement[32];
    SIZE_T length;
    const char *label;
} BytePatch;
typedef struct { BYTE *address; DWORD protection; BOOL writable; } ClockPage;
#if MXL_ENABLE_DIAGNOSTICS
static HANDLE log_file = INVALID_HANDLE_VALUE;
#endif
static LONG initialized;
static volatile LONG smoothing_active;
static const volatile DWORD *game_type;
static BYTE *epoch_client, *epoch_fps;
static uint64_t epoch_frequency;
static MxlVisualClock visual_clock;
static MxlRenderClock render_clock;
static void *render_update_original;

static void __cdecl restore_render_raw(void) {
    uint64_t current=*(const volatile uint64_t *)(epoch_fps+0x38070);
    if(render_clock.active && current==render_clock.visual)
        *(volatile uint64_t *)(epoch_fps+0x38070)=render_clock.raw;
    else render_clock.active=0; /* Menus or another native path took ownership. */
}
static void __stdcall finish_render_time(uint64_t now, DWORD drew) {
    volatile uint64_t *stamp=(volatile uint64_t *)(epoch_fps+0x38070);
    if(!(drew&255)) {
        if(render_clock.active && *stamp==render_clock.raw)*stamp=render_clock.visual;
        else render_clock.active=0;
        return;
    }
    *stamp=mxl_render_time(&render_clock,*stamp,now,epoch_frequency,
        *(const DWORD *)(epoch_fps+0x38078),*(const DWORD *)(epoch_fps+0x3807c),
        *(const volatile DWORD *)(epoch_client+0x11c310),*game_type);
}

/* The verified Rust call uses ECX:EDX for now and two caller-cleaned stack
 * arguments. Preserve its actual outputs, including SSE/flags, around helpers.
 * ESI:EDI in draw_game already contains our previous visual timestamp. */
__declspec(naked) static void update_render_time(void) {
    __asm {
        push ebp
        mov ebp,esp
        sub esp,8
        mov [ebp-8],ecx
        mov [ebp-4],edx
        pushfd
        pushad
        sub esp,128
        movdqu [esp],xmm0
        movdqu [esp+16],xmm1
        movdqu [esp+32],xmm2
        movdqu [esp+48],xmm3
        movdqu [esp+64],xmm4
        movdqu [esp+80],xmm5
        movdqu [esp+96],xmm6
        movdqu [esp+112],xmm7
        call restore_render_raw
        movdqu xmm0,[esp]
        movdqu xmm1,[esp+16]
        movdqu xmm2,[esp+32]
        movdqu xmm3,[esp+48]
        movdqu xmm4,[esp+64]
        movdqu xmm5,[esp+80]
        movdqu xmm6,[esp+96]
        movdqu xmm7,[esp+112]
        add esp,128
        popad
        popfd
        push [ebp+12]
        push [ebp+8]
        call render_update_original
        lea esp,[esp+8]
        pushfd
        pushad
        sub esp,128
        movdqu [esp],xmm0
        movdqu [esp+16],xmm1
        movdqu [esp+32],xmm2
        movdqu [esp+48],xmm3
        movdqu [esp+64],xmm4
        movdqu [esp+80],xmm5
        movdqu [esp+96],xmm6
        movdqu [esp+112],xmm7
        push eax
        push [ebp-4]
        push [ebp-8]
        call finish_render_time
        movdqu xmm0,[esp]
        movdqu xmm1,[esp+16]
        movdqu xmm2,[esp+32]
        movdqu xmm3,[esp+48]
        movdqu xmm4,[esp+64]
        movdqu xmm5,[esp+80]
        movdqu xmm6,[esp+96]
        movdqu xmm7,[esp+112]
        add esp,128
        popad
        popfd
        mov esp,ebp
        pop ebp
        ret
    }
}

static uint64_t __stdcall stable_epoch(uint64_t raw) {
    return mxl_visual_epoch(&visual_clock,raw,
        *(const volatile uint64_t *)(epoch_fps+0x38070),epoch_frequency,
        *(const volatile DWORD *)(epoch_client+0x1197e0+16),
        *(const volatile DWORD *)(epoch_client+0x1197e0+24),
        *(const volatile DWORD *)(epoch_client+0x11c310),*game_type);
}

/* D2FPS+EB20 stores ESI:EDI into its visual epoch. Retain all native live
 * registers/flags and SSE state across C code, then replay those two stores.
 * This runs on updates, not each rendered frame. It changes no client state. */
__declspec(naked) static void store_stable_epoch(void) {
    __asm {
        pushfd
        pushad
        sub esp, 128
        movdqu [esp], xmm0
        movdqu [esp+16], xmm1
        movdqu [esp+32], xmm2
        movdqu [esp+48], xmm3
        movdqu [esp+64], xmm4
        movdqu [esp+80], xmm5
        movdqu [esp+96], xmm6
        movdqu [esp+112], xmm7
        push esi
        push edi
        call stable_epoch
        mov [esp+128], eax
        mov [esp+132], edx
        movdqu xmm0, [esp]
        movdqu xmm1, [esp+16]
        movdqu xmm2, [esp+32]
        movdqu xmm3, [esp+48]
        movdqu xmm4, [esp+64]
        movdqu xmm5, [esp+80]
        movdqu xmm6, [esp+96]
        movdqu xmm7, [esp+112]
        add esp, 128
        popad
        popfd
        push eax
        mov eax, epoch_fps
        mov [eax+380f8h], edi
        mov [eax+380fch], esi
        pop eax
        ret
    }
}
#if MXL_ENABLE_DIAGNOSTICS
// Private observations of our existing clamp; no additional native patch.
// Written/read on the game thread, without clock calls or locks in the clamp.
static volatile DWORD motion_samples;
static volatile DWORD motion_elapsed[2], motion_interval[2], motion_clamped[2];
static DWORD (WINAPI *motion_clock)(void);
static const BYTE *motion_fps;
#endif

/* At the original sign test/clamp: EAX:ECX = one simulation interval;
 * EDI:EDX = signed elapsed time. Preserve interval and other live registers.
 * D2FPS renders on a fixed presentation timeline that can precede the newest
 * client update by part of a render frame. Clipping that small negative phase
 * to zero causes a large step followed by a small step on steady movement.
 * Realm type 3 permits [-half a tick, one and a half ticks] of visual phase.
 * Other game types retain the original [0, one tick] interval.
 */
__declspec(naked) static void bounded_mp_interval(void) {
    __asm {
        push eax
        push ecx
        push ebx
        push esi
        mov ebx, ecx
        mov esi, eax
    }
#if MXL_ENABLE_DIAGNOSTICS
    __asm {
        inc dword ptr [motion_samples]
        mov dword ptr [motion_elapsed], edx
        mov dword ptr [motion_elapsed+4], edi
        mov dword ptr [motion_interval], ecx
        mov dword ptr [motion_interval+4], eax
    }
#endif
    __asm {
        test edi, edi
        js negative_interval
        mov eax, game_type
        test eax, eax
        jz compare_limit
        cmp dword ptr [eax], 3
        jne compare_limit
        mov eax, esi
        mov ecx, ebx
        shrd ecx, eax, 1
        shr eax, 1
        add ebx, ecx
        adc esi, eax
    compare_limit:
        cmp edx, ebx
        mov eax, edi
        sbb eax, esi
        cmovae edx, ebx
        cmovae edi, esi
        jmp interval_done
    negative_interval:
        mov eax, game_type
        test eax, eax
        jz zero_interval
        cmp dword ptr [eax], 3
        jne zero_interval
        // Form signed -floor(interval/2), including low-word carry.
        shrd ebx, esi, 1
        shr esi, 1
        neg ebx
        adc esi, 0
        neg esi
        cmp edx, ebx
        mov eax, edi
        sbb eax, esi
        cmovl edx, ebx
        cmovl edi, esi
        jmp interval_done
    zero_interval:
        xor edx, edx
        xor edi, edi
    interval_done:
    }
#if MXL_ENABLE_DIAGNOSTICS
    __asm {
        mov dword ptr [motion_clamped], edx
        mov dword ptr [motion_clamped+4], edi
    }
#endif
    __asm {
        pop esi
        pop ebx
        pop ecx
        pop eax
        ret
    }
}

static BytePatch clock_patch(const ClockSite *site) {
    BytePatch patch; memset(&patch,0,sizeof(patch));
    patch.instruction=site->instruction; patch.length=6; patch.label=site->label;
    memcpy(patch.expected,site->opcode,2); memcpy(patch.replacement,site->opcode,2);
    memcpy(patch.expected+2,&site->old_iat,4); memcpy(patch.replacement+2,&site->new_iat,4);
    return patch;
}

static BytePatch phase_patch(BYTE *instruction) {
    static const BYTE signature[18]={0x0f,0x88,0x46,0x02,0x00,0x00,0x39,0xca,0x89,0xfb,0x19,0xc3,0x0f,0x43,0xf8,0x0f,0x43,0xd1};
    BytePatch patch;memset(&patch,0,sizeof(patch));
    patch.instruction=instruction;patch.length=sizeof(signature);
    patch.label="D2FPS+EDB9 realm-only signed interpolation phase";
    memcpy(patch.expected,signature,sizeof(signature));
    memset(patch.replacement,0x90,sizeof(signature));patch.replacement[0]=0xe8;
    DWORD relative=(DWORD)((uintptr_t)bounded_mp_interval-(uintptr_t)(instruction+5));
    memcpy(patch.replacement+1,&relative,4);
    return patch;
}

static BytePatch epoch_patch(BYTE *fps) {
    BytePatch patch;memset(&patch,0,sizeof(patch));
    patch.instruction=fps+0xeb20;patch.length=12;
    patch.label="D2FPS+EB20 realm visual clock anchor";
    patch.expected[0]=0x89;patch.expected[1]=0x3d;
    patch.expected[6]=0x89;patch.expected[7]=0x35;
    DWORD low=(DWORD)(uintptr_t)(fps+0x380f8),high=low+4;
    memcpy(patch.expected+2,&low,4);memcpy(patch.expected+8,&high,4);
    memset(patch.replacement,0x90,12);patch.replacement[0]=0xe8;
    DWORD relative=(DWORD)((uintptr_t)store_stable_epoch-(uintptr_t)(patch.instruction+5));
    memcpy(patch.replacement+1,&relative,4);return patch;
}

static BytePatch render_patch(BYTE *fps) {
    BytePatch patch;memset(&patch,0,sizeof(patch));
    patch.instruction=fps+0xe82c;patch.length=5;
    patch.label="D2FPS+E82C continuous realm render timeline";
    patch.expected[0]=patch.replacement[0]=0xe8;
    DWORD old_relative=0x11900-0xe831;
    DWORD relative=(DWORD)((uintptr_t)update_render_time-(uintptr_t)(patch.instruction+5));
    memcpy(patch.expected+1,&old_relative,4);memcpy(patch.replacement+1,&relative,4);
    return patch;
}

#if MXL_ENABLE_DIAGNOSTICS
static void log_line(const char *format, ...) {
    char text[1024]; DWORD written; va_list args;
    va_start(args, format);
    int count = vsnprintf(text, sizeof(text)-3, format, args);
    va_end(args);
    if (count < 0) return;
    size_t length = strlen(text);
    text[length++]='\r'; text[length++]='\n';
    if (log_file != INVALID_HANDLE_VALUE) {
        WriteFile(log_file, text, (DWORD)length, &written, NULL);
        FlushFileBuffers(log_file);
    }
}
#else
#define log_line(...) ((void)0)
#endif
#ifdef MXL_SMOOTHING_TEST
static int protect_calls, fail_protect_call;
#endif
static BOOL protect_page(void *address, SIZE_T size, DWORD desired, DWORD *old) {
#ifdef MXL_SMOOTHING_TEST
    if (++protect_calls == fail_protect_call) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
#endif
    return VirtualProtect(address, size, desired, old);
}

/* All signatures are checked before any writes. Each shared page is protected
 * exactly once, so multiple operands on one page restore the original mode. */
static BOOL apply_patches(BytePatch *sites, size_t count) {
    SYSTEM_INFO system; ClockPage pages[8]; size_t page_count=0, i, j;
    if (!count || count > 8) return FALSE;
    GetSystemInfo(&system);
    memset(pages, 0, sizeof(pages));
    for (i=0; i<count; ++i) {
        MEMORY_BASIC_INFORMATION region;
        if (!sites[i].length || sites[i].length>sizeof(sites[i].expected)) return FALSE;
        BYTE *page=(BYTE *)((uintptr_t)sites[i].instruction & ~((uintptr_t)system.dwPageSize-1));
        if (sites[i].instruction+sites[i].length > page+system.dwPageSize) return FALSE;
        if (!VirtualQuery(sites[i].instruction, &region, sizeof(region)) ||
            region.State != MEM_COMMIT || (region.Protect & (PAGE_NOACCESS|PAGE_GUARD))) return FALSE;
        if (memcmp(sites[i].instruction, sites[i].expected, sites[i].length)) {
            log_line("REFUSED: instruction mismatch at %s; zero changes.", sites[i].label);
            return FALSE;
        }
        for (j=0; j<page_count && pages[j].address != page; ++j) {}
        if (j==page_count) pages[page_count++].address=page;
    }
    for (i=0; i<page_count; ++i) {
        if (!protect_page(pages[i].address, system.dwPageSize, PAGE_EXECUTE_READWRITE, &pages[i].protection)) {
            DWORD error=GetLastError(), ignored;
            for (j=0; j<i; ++j) {
                if (!VirtualProtect(pages[j].address, system.dwPageSize, pages[j].protection, &ignored))
                    log_line("ERROR: could not restore page protection after failed preflight: %lu", GetLastError());
            }
            log_line("REFUSED: could not prepare code pages (%lu); zero changes.", error);
            return FALSE;
        }
        pages[i].writable=TRUE;
    }
    for (i=0; i<count; ++i) memcpy(sites[i].instruction, sites[i].replacement, sites[i].length);
    BOOL ok=FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    for (i=0; i<count; ++i) {
        if (memcmp(sites[i].instruction,sites[i].replacement,sites[i].length)) ok=FALSE;
    }
    if (!ok) {
        for (i=0; i<count; ++i) memcpy(sites[i].instruction, sites[i].expected, sites[i].length);
        FlushInstructionCache(GetCurrentProcess(), NULL, 0);
        log_line("ERROR: write verification failed; original operands restored.");
    }
    for (i=0; i<page_count; ++i) {
        DWORD ignored;
        if (!VirtualProtect(pages[i].address, system.dwPageSize, pages[i].protection, &ignored)) {
            log_line("ERROR: original page protection could not be restored (%lu).", GetLastError());
            ok=FALSE;
        }
    }
    if (ok) for (i=0; i<count; ++i)
        log_line("APPLIED %s (%lu-byte instruction region), readback verified.", sites[i].label,(DWORD)sites[i].length);
    return ok;
}

static BOOL file_hash_matches(const wchar_t *path, const char *expected) {
    HCRYPTPROV provider=0; HCRYPTHASH hash=0; BYTE buffer[32768], digest[32];
    DWORD got, bytes=sizeof(digest); char hex[65]; BOOL ok=FALSE;
    HANDLE file=CreateFileW(path, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file==INVALID_HANDLE_VALUE) return FALSE;
    if (!CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) goto done;
    if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) goto done;
    for (;;) {
        if (!ReadFile(file, buffer, sizeof(buffer), &got, NULL)) goto done;
        if (!got) break;
        if (!CryptHashData(hash, buffer, got, 0)) goto done;
    }
    if (!CryptGetHashParam(hash, HP_HASHVAL, digest, &bytes, 0) || bytes!=32) goto done;
    for (DWORD i=0; i<32; ++i) sprintf(hex+i*2, "%02x", digest[i]);
    hex[64]=0; ok=strcmp(hex,expected)==0;
done:
    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    CloseHandle(file); return ok;
}

static BOOL check_module(HMODULE module, const wchar_t *directory, const char *hash,
                         DWORD image_size, DWORD timestamp) {
    wchar_t path[MAX_PATH];
    if (!module || !GetModuleFileNameW(module, path, MAX_PATH)) return FALSE;
    size_t length=wcslen(directory);
    if (_wcsnicmp(path,directory,length) || !path[length] || wcschr(path+length,L'\\')) return FALSE;
    const IMAGE_DOS_HEADER *dos=(const IMAGE_DOS_HEADER *)module;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>4096) return FALSE;
    const IMAGE_NT_HEADERS32 *nt=(const IMAGE_NT_HEADERS32 *)((const BYTE *)module+dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt->OptionalHeader.SizeOfImage != image_size || nt->FileHeader.TimeDateStamp != timestamp) return FALSE;
    return file_hash_matches(path,hash);
}

void __stdcall MxlSmoothing_Initialize(void) {
    wchar_t directory[MAX_PATH];
    if (InterlockedCompareExchange(&initialized,1,0)) return;
    if (!GetModuleFileNameW(NULL,directory,MAX_PATH)) return;
    wchar_t *name=wcsrchr(directory,L'\\');
    if (!name || _wcsicmp(name+1,L"Game.exe")) return;
    name[1]=0;
#if MXL_ENABLE_DIAGNOSTICS
    wchar_t log_path[MAX_PATH];
    if (swprintf(log_path,MAX_PATH,L"%lsmxl-smoothing.log",directory)<0) return;
    log_file=CreateFileW(log_path,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,
                        CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    SYSTEMTIME now; GetLocalTime(&now);
    log_line("MXL Smooth Motion DX12 1.15 / x86 / %04u-%02u-%02u %02u:%02u:%02u",
              now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond);
#endif
    HMODULE client=GetModuleHandleW(L"D2Client.dll"), fps=GetModuleHandleW(L"d2fps.dll");
    if (!check_module(client,directory,"dd8bc6025de921216a97c17f97cd1a50fbb85926e838ec60e13451448836d906",0x135000,0x4b95ca3e) ||
        !check_module(fps,directory,"db9de4d4d320a7b70e66fe6b4aaa0e6f1560a5300a4993cc81cf4512ab1240c1",0x3d000,0x68ed0db7)) {
        log_line("REFUSED: required DLL identity/version/hash does not match; zero changes."); goto done;
    }
    if (!GetModuleHandleW(L"D2Sigma.dll")) {
        log_line("REFUSED: Median XL is not loaded; zero changes."); goto done;
    }
    BYTE *cb=(BYTE *)client, *fb=(BYTE *)fps;
    DWORD precise=(DWORD)(uintptr_t)GetProcAddress(GetModuleHandleW(L"winmm.dll"),"timeGetTime");
    DWORD client_precise=*(const DWORD *)(cb+0xcf124), fps_precise=*(const DWORD *)(fb+0x30190);
    if (!precise || client_precise != precise || fps_precise != precise || *(const DWORD *)(cb+0xf4318)!=40) {
        log_line("REFUSED: precise clocks do not match or simulation interval differs from 40 ms; zero changes."); goto done;
    }
    ClockSite sites[]={
        {cb+0x44a81,{0x8b,0x35},(DWORD)(uintptr_t)(cb+0xcef5c),(DWORD)(uintptr_t)(cb+0xcf124),"D2Client+44A81 client step time"},
        {cb+0x44b7c,{0xff,0x15},(DWORD)(uintptr_t)(cb+0xcef5c),(DWORD)(uintptr_t)(cb+0xcf124),"D2Client+44B7C paused update time"},
        {cb+0x44bb8,{0xff,0x15},(DWORD)(uintptr_t)(cb+0xcef5c),(DWORD)(uintptr_t)(cb+0xcf124),"D2Client+44BB8 paused draw time"},
        {cb+0x44c00,{0x8b,0x3d},(DWORD)(uintptr_t)(cb+0xcef5c),(DWORD)(uintptr_t)(cb+0xcf124),"D2Client+44C00 client loop time"},
        {fb+0xeabc,{0xff,0x15},(DWORD)(uintptr_t)(fb+0x30114),(DWORD)(uintptr_t)(fb+0x30190),"D2FPS+EABC multiplayer interpolation clock"}
    };
    LARGE_INTEGER frequency;
    if(!QueryPerformanceFrequency(&frequency) || frequency.QuadPart<1000
        || frequency.QuadPart>10000000000LL || frequency.QuadPart%1000) {
        log_line("REFUSED: unsupported visual clock frequency; zero changes.");goto done;
    }
    BytePatch patches[8];
    for (size_t i=0;i<5;++i) patches[i]=clock_patch(&sites[i]);
    patches[5]=phase_patch(fb+0xedb9);
    patches[6]=epoch_patch(fb);
    patches[7]=render_patch(fb);
    game_type=(const volatile DWORD *)(cb+0x11c394);
    epoch_client=cb;epoch_fps=fb;epoch_frequency=(uint64_t)frequency.QuadPart;
    render_update_original=fb+0x11900;
    if (apply_patches(patches,8)) {
#if MXL_ENABLE_DIAGNOSTICS
        motion_clock=(DWORD(WINAPI *)(void))(uintptr_t)precise;
        motion_fps=fb;
#endif
        smoothing_active=1;
        log_line("SUCCESS: 8 regions changed and verified; simulation interval remains 40 ms.");
        log_line("Realm render timeline follows QPC continuously; native draw deadlines and skipped frames preserved.");
        log_line("Realm visual epoch anchored across normal client steps; resets on area, discontinuity or >20 ms drift.");
        log_line("Source and consumer both resolve to winmm!timeGetTime at %08lX.",precise);
        log_line("Realm type 3: signed visual phase -20..60 ms. SP/LAN: original 0..40 ms clamp.");
    } else log_line("FAILED: patch attempt did not pass all checks; see preceding reason.");
done:
#if MXL_ENABLE_DIAGNOSTICS
    if (log_file!=INVALID_HANDLE_VALUE) { CloseHandle(log_file); log_file=INVALID_HANDLE_VALUE; }
#else
    (void)0;
#endif

}

int __stdcall MxlSmoothing_IsActive(void) { return smoothing_active == 1; }

#if MXL_ENABLE_DIAGNOSTICS
int __stdcall MxlSmoothing_ReadMotion(MxlMotionSnapshot *output) {
    if (!output || !smoothing_active || !game_type || !motion_clock || !motion_fps) return 0;
    __try {
        // Initialization already verified this exact Client and D2FPS build.
        // Read existing loop globals only; never change update times or count.
        const BYTE *client=(const BYTE *)game_type-0x11c394;
        MxlMotionSnapshot sample;
        sample.samples=motion_samples;sample.game_type=*game_type;
        sample.client_updates=*(const volatile DWORD *)(client+0x1197e0+24);
        sample.client_update_ms=*(const volatile DWORD *)(client+0x1197e0+16);
        sample.clock_ms=motion_clock();
        sample.elapsed_ticks=((ULONGLONG)motion_elapsed[1]<<32)|motion_elapsed[0];
        sample.interval_ticks=((ULONGLONG)motion_interval[1]<<32)|motion_interval[0];
        sample.clamped_ticks=((ULONGLONG)motion_clamped[1]<<32)|motion_clamped[0];
        sample.render_ticks=*(const volatile ULONGLONG *)(motion_fps+0x38070);
        sample.update_ticks=*(const volatile ULONGLONG *)(motion_fps+0x380f8);
        LARGE_INTEGER now;QueryPerformanceCounter(&now);sample.probe_ticks=now.QuadPart;
        sample.epoch_raw_ticks=visual_clock.raw;sample.epoch_samples=visual_clock.samples;
        sample.epoch_resets=visual_clock.resets;sample.epoch_reason=visual_clock.reason;
        sample.epoch_active=visual_clock.active;
        sample.render_raw_ticks=render_clock.raw;sample.render_now_ticks=render_clock.now;
        sample.render_resets=render_clock.resets;sample.render_active=render_clock.active;
        *output=sample;return 1;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
#endif

#ifdef MXL_SMOOTHING_TEST
static void *test_phase_target;
static void *test_epoch_target;
static BYTE epoch_sse_before[128],epoch_sse_after[128];
static DWORD epoch_test_b,epoch_test_flags;
static int epoch_test_exception(EXCEPTION_POINTERS *e) {
    printf("native fixture exception=%08lx ip=%p image=%p target=%p stable=%p store=%p fault=%p eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx esp=%08lx ebp=%08lx\n",
        e->ExceptionRecord->ExceptionCode,e->ExceptionRecord->ExceptionAddress,GetModuleHandleW(NULL),test_epoch_target,
        stable_epoch,store_stable_epoch,(void *)e->ExceptionRecord->ExceptionInformation[1],e->ContextRecord->Eax,e->ContextRecord->Ebx,
        e->ContextRecord->Ecx,e->ContextRecord->Edx,e->ContextRecord->Esp,e->ContextRecord->Ebp);
    return EXCEPTION_EXECUTE_HANDLER;
}
static int test_epoch_store(uint64_t raw,uint64_t expected) {
    DWORD lo=(DWORD)raw,hi=(DWORD)(raw>>32),a,c,d,actual_si,actual_di,flags_before;
    for(unsigned i=0;i<128;++i)epoch_sse_before[i]=(BYTE)(i*37+11);
    __asm {
        lea eax,epoch_sse_before
        movdqu xmm0,[eax]
        movdqu xmm1,[eax+16]
        movdqu xmm2,[eax+32]
        movdqu xmm3,[eax+48]
        movdqu xmm4,[eax+64]
        movdqu xmm5,[eax+80]
        movdqu xmm6,[eax+96]
        movdqu xmm7,[eax+112]
        mov edi,lo
        mov esi,hi
        mov eax,12345678h
        mov ecx,3456789ah
        mov edx,456789abh
        cmp eax,eax
        stc
        pushfd
        pop flags_before
        // MSVC can use EBX as the aligned frame base. Save it before the
        // sentinel and restore it before addressing C locals again.
        push ebx
        mov ebx,23456789h
        call test_epoch_target
        pushfd
        pop epoch_test_flags
        mov epoch_test_b,ebx
        pop ebx
        mov a,eax
        mov c,ecx
        mov d,edx
        mov actual_si,esi
        mov actual_di,edi
        lea eax,epoch_sse_after
        movdqu [eax],xmm0
        movdqu [eax+16],xmm1
        movdqu [eax+32],xmm2
        movdqu [eax+48],xmm3
        movdqu [eax+64],xmm4
        movdqu [eax+80],xmm5
        movdqu [eax+96],xmm6
        movdqu [eax+112],xmm7
    }
    return a!=0x12345678 || epoch_test_b!=0x23456789 || c!=0x3456789a || d!=0x456789ab
        || (((uint64_t)actual_si<<32)|actual_di)!=expected || memcmp(epoch_sse_before,epoch_sse_after,128)
        || ((flags_before^epoch_test_flags)&0xcd5)
        || *(uint64_t *)(epoch_fps+0x380f8)!=expected;
}

static int test_native_epoch(void) {
    epoch_fps=(BYTE *)VirtualAlloc(NULL,0x3d000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    epoch_client=(BYTE *)VirtualAlloc(NULL,0x135000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!epoch_fps || !epoch_client)return 30;
    epoch_frequency=10000000;memset(&visual_clock,0,sizeof(visual_clock));
    game_type=(DWORD *)(epoch_client+0x11c394);*(DWORD *)game_type=3;
    *(DWORD *)(epoch_client+0x11c310)=1;
    BytePatch patch=epoch_patch(epoch_fps);
    memcpy(patch.instruction,patch.expected,patch.length);patch.instruction[12]=0xc3;
    DWORD old;VirtualProtect(patch.instruction,32,PAGE_EXECUTE_READ,&old);
    protect_calls=0;fail_protect_call=0;
    BytePatch bad=patch;bad.expected[11]^=1;
    if(apply_patches(&bad,1) || memcmp(patch.instruction,patch.expected,12))return 31;
    if(!apply_patches(&patch,1))return 32;
    test_epoch_target=patch.instruction;
    for(unsigned i=0;i<16;++i) {
        uint64_t expected=0xffff0000ULL+i*400000ULL;
        *(DWORD *)(epoch_client+0x1197e0+16)=1000+i*40;
        *(DWORD *)(epoch_client+0x1197e0+24)=100+i;
        *(uint64_t *)(epoch_fps+0x38070)=expected;
        uint64_t raw=expected+(i?(i%2?10000:-10000LL):0);
        if(test_epoch_store(raw,expected))return 33;
    }
    *(DWORD *)game_type=0;
    if(test_epoch_store(98765432100ULL,98765432100ULL))return 34;
    MEMORY_BASIC_INFORMATION region;VirtualQuery(patch.instruction,&region,sizeof(region));
    if(region.Protect!=PAGE_EXECUTE_READ)return 35;
    VirtualFree(epoch_fps,0,MEM_RELEASE);VirtualFree(epoch_client,0,MEM_RELEASE);
    epoch_client=NULL;epoch_fps=NULL;epoch_frequency=0;game_type=NULL;
    memset(&visual_clock,0,sizeof(visual_clock));
    puts("PASS: native guarded epoch stores, 64-bit carry, GP/SSE/flags, page restore, signature rejection and SP passthrough.");
    return 0;
}
static DWORD __stdcall old_clock(void) { return 17; }
static DWORD __stdcall new_clock(void) { return 91; }
static int test_case(int mode) {
    SYSTEM_INFO system; GetSystemInfo(&system); SIZE_T bytes=system.dwPageSize*2;
    BYTE *memory=(BYTE *)VirtualAlloc(NULL,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if (!memory) return 1;
    DWORD *old_slot=(DWORD *)(memory+16), *new_slot=(DWORD *)(memory+20);
    *old_slot=(DWORD)(uintptr_t)old_clock; *new_slot=(DWORD)(uintptr_t)new_clock;
    ClockSite sites[5]; BYTE *entries[5]; BYTE opcodes[5][2]={{0x8b,0x35},{0xff,0x15},{0xff,0x15},{0x8b,0x3d},{0xff,0x15}};
    for (size_t i=0;i<5;++i) {
        BYTE *p=memory+(i<4 ? 64+i*32 : system.dwPageSize+64);
        entries[i]=p;
        if (opcodes[i][0]==0x8b) *p++=(opcodes[i][1]==0x35 ? 0x56 : 0x57);
        sites[i].instruction=p; memcpy(sites[i].opcode,opcodes[i],2);
        sites[i].old_iat=(DWORD)(uintptr_t)old_slot; sites[i].new_iat=(DWORD)(uintptr_t)new_slot; sites[i].label="selftest";
        memcpy(p,opcodes[i],2); memcpy(p+2,&sites[i].old_iat,4); p+=6;
        if (opcodes[i][0]==0x8b) { *p++=0xff; *p++=(opcodes[i][1]==0x35 ? 0xd6 : 0xd7); *p++=(opcodes[i][1]==0x35 ? 0x5e : 0x5f); }
        *p=0xc3;
    }
    if (mode==1) sites[4].old_iat+=4;  /* Last signature wrong: none may change. */
    DWORD old; VirtualProtect(memory,bytes,PAGE_EXECUTE_READ,&old);
    for (size_t i=0;i<5;++i) if (((DWORD(__stdcall *)(void))entries[i])()!=17) return 2;
    protect_calls=0; fail_protect_call=(mode==2 ? 2 : 0);
    BytePatch patches[5]; for (size_t i=0;i<5;++i) patches[i]=clock_patch(&sites[i]);
    BOOL applied=apply_patches(patches,5);
    if (applied != (mode==0)) return 3;
    for (size_t i=0;i<5;++i) if (((DWORD(__stdcall *)(void))entries[i])()!=(mode==0 ? 91u : 17u)) return 4;
    for (size_t i=0;i<2;++i) {
        MEMORY_BASIC_INFORMATION region;
        VirtualQuery(memory+i*system.dwPageSize,&region,sizeof(region));
        if (region.Protect!=PAGE_EXECUTE_READ) return 5;
    }
    VirtualFree(memory,0,MEM_RELEASE); return 0;
}
static uint64_t test_clamp(uint64_t since, uint64_t interval, DWORD type, BOOL *preserved) {
    DWORD lo=(DWORD)since, hi=(DWORD)(since>>32), il=(DWORD)interval, ih=(DWORD)(interval>>32);
    DWORD result_lo, result_hi, actual_il, actual_ih, actual_bx, actual_si;
    game_type=&type;
    __asm {
        mov eax, ih
        mov ecx, il
        mov edx, lo
        mov edi, hi
        mov ebx, 12345678h
        mov esi, 23456789h
        call test_phase_target
        mov result_lo, edx
        mov result_hi, edi
        mov actual_il, ecx
        mov actual_ih, eax
        mov actual_bx, ebx
        mov actual_si, esi
    }
    *preserved=actual_il==il && actual_ih==ih && actual_bx==0x12345678 && actual_si==0x23456789;
#if MXL_ENABLE_DIAGNOSTICS
    *preserved=*preserved && motion_elapsed[0]==lo && motion_elapsed[1]==hi
        && motion_interval[0]==il && motion_interval[1]==ih
        && motion_clamped[0]==result_lo && motion_clamped[1]==result_hi;
#endif
    return ((uint64_t)result_hi<<32)|result_lo;
}
int main(void) {
    setvbuf(stdout,NULL,_IONBF,0);
    for (int i=0;i<3;++i) {
        int result=test_case(i);
        printf("case %d (%s): %s [%d]\n",i,i==0?"five executable sites + protection restore":i==1?"bad last signature leaves all unchanged":"second-page failure restores first page",result?"FAIL":"PASS",result);
        if (result) return result;
    }
    __try {int result=test_native_epoch();if(result){printf("FAIL native epoch: %d\n",result);return result;}}
    __except(epoch_test_exception(GetExceptionInformation())) {return 36;}
    BYTE *phase_code=(BYTE *)VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!phase_code)return 16;
    BytePatch phase=phase_patch(phase_code);
    memcpy(phase_code,phase.expected,phase.length);phase_code[phase.length]=0xc3;
    DWORD old_protection;VirtualProtect(phase_code,4096,PAGE_EXECUTE_READ,&old_protection);
    protect_calls=0;fail_protect_call=0;
    BytePatch rejected=phase;rejected.expected[17]^=1;
    if(apply_patches(&rejected,1) || memcmp(phase_code,phase.expected,phase.length))return 17;
    if(!apply_patches(&phase,1))return 18;
    MEMORY_BASIC_INFORMATION region;VirtualQuery(phase_code,&region,sizeof(region));
    if(region.Protect!=PAGE_EXECUTE_READ)return 19;
    test_phase_target=phase_code;
    const uint64_t intervals[]={400000,400001,0x100000001ULL};
    const DWORD types[]={0,1,3,6,7,8,9}; unsigned cases=0;
    for (size_t a=0;a<sizeof(intervals)/sizeof(intervals[0]);++a) {
        uint64_t interval=intervals[a];
        const uint64_t elapsed[]={0,interval/2,interval-1,interval,interval+1,interval+interval/4,interval+interval/2,interval*2};
        for (size_t b=0;b<sizeof(types)/sizeof(types[0]);++b) for (size_t c=0;c<sizeof(elapsed)/sizeof(elapsed[0]);++c) {
            BOOL preserved=FALSE; uint64_t limit=interval+(types[b]==3 ? interval/2 : 0);
            uint64_t expected=elapsed[c]<limit ? elapsed[c] : limit;
            uint64_t actual=test_clamp(elapsed[c],interval,types[b],&preserved);
            if (actual!=expected || !preserved) { printf("FAIL interpolation case %u\n",cases); return 10; }
            ++cases;
        }
    }
    printf("PASS: %u native clamp cases, 64-bit carry, SP/LAN preservation, and live registers.\n",cases);
    unsigned negative_cases=0;
    for (size_t a=0;a<sizeof(intervals)/sizeof(intervals[0]);++a) {
        int64_t interval=(int64_t)intervals[a];
        const int64_t elapsed[]={-1,-interval/4,-interval/2,-interval/2-1,-interval,-interval*2,INT64_MIN};
        for (size_t b=0;b<sizeof(types)/sizeof(types[0]);++b) for (size_t c=0;c<sizeof(elapsed)/sizeof(elapsed[0]);++c) {
            BOOL preserved=FALSE; int64_t minimum=types[b]==3 ? -interval/2 : 0;
            int64_t expected=elapsed[c]>minimum ? elapsed[c] : minimum;
            int64_t actual=(int64_t)test_clamp((uint64_t)elapsed[c],interval,types[b],&preserved);
            if(actual!=expected || !preserved) { printf("FAIL negative phase case %u\n",negative_cases);return 14; }
            ++negative_cases;
        }
    }
    printf("PASS: %u signed phase cases, negative limit, carry, SP/LAN and registers.\n",negative_cases);
    // Replay constant-velocity frames crossing an update boundary. The original
    // zero clamp jumps by 10 ms then 3.888 ms; signed phase gives 6.944 ms twice.
    for(unsigned realm=0;realm<2;++realm) {
        BOOL preserved;DWORD type=realm?3:0;
        int64_t a=(int64_t)test_clamp(300000,400000,type,&preserved)-400000;
        int64_t b=400000+(int64_t)test_clamp((uint64_t)(int64_t)-30556,400000,type,&preserved)-400000;
        int64_t c=400000+(int64_t)test_clamp(38889,400000,type,&preserved)-400000;
        if(realm ? (b-a!=69444 || c-b!=69445) : (b-a!=100000 || c-b!=38889))return 15;
    }
    puts("PASS: native phase replay removes the large/small step pair; SP behavior unchanged.");
#if MXL_ENABLE_DIAGNOSTICS
    {
        MxlMotionSnapshot sample;
        if(MxlSmoothing_ReadMotion(&sample)) return 11;
        BYTE *client=(BYTE *)VirtualAlloc(NULL,0x135000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        if(!client)return 12;
        game_type=(DWORD *)(client+0x11c394);*(DWORD *)game_type=3;
        *(DWORD *)(client+0x1197e0+24)=123;
        *(DWORD *)(client+0x1197e0+16)=70;
        motion_clock=new_clock;motion_fps=client;smoothing_active=1;
        *(ULONGLONG *)(client+0x38070)=123456789012ULL;
        *(ULONGLONG *)(client+0x380f8)=123456788888ULL;
        visual_clock.raw=123456788999ULL;visual_clock.samples=45;visual_clock.resets=2;
        visual_clock.reason=MXL_EPOCH_STEP;visual_clock.active=1;
        render_clock.raw=123456789010ULL;render_clock.now=123456789013ULL;
        render_clock.resets=3;render_clock.active=1;
        if(!MxlSmoothing_ReadMotion(&sample) || sample.samples!=cases+negative_cases+6 || sample.game_type!=3
            || sample.client_updates!=123 || sample.client_update_ms!=70 || sample.clock_ms!=91
            || sample.interval_ticks!=400000 || sample.elapsed_ticks!=38889
            || sample.clamped_ticks!=38889 || sample.render_ticks!=123456789012ULL
            || sample.update_ticks!=123456788888ULL || !sample.probe_ticks
            || sample.epoch_raw_ticks!=123456788999ULL || sample.epoch_samples!=45 || sample.epoch_resets!=2
            || sample.epoch_reason!=MXL_EPOCH_STEP || sample.epoch_active!=1
            || sample.render_raw_ticks!=123456789010ULL || sample.render_now_ticks!=123456789013ULL
            || sample.render_resets!=3 || sample.render_active!=1)return 13;
        smoothing_active=0;game_type=NULL;motion_clock=NULL;motion_fps=NULL;VirtualFree(client,0,MEM_RELEASE);
        puts("PASS: private motion snapshot reads loop state and exact clamp values without altering interpolation.");
    }
#endif
    VirtualFree(phase_code,0,MEM_RELEASE);
    puts("All native x86 timing-patch tests passed through the guarded 18-byte entry."); return 0;
}
#endif
