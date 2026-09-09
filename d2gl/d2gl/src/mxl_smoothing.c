/* Median XL 1.13c multiplayer smoothing, integrated into D2GL.
 * Five clock operands plus a bounded, realm-only interpolation extension.
 * Does not access other processes, hook Windows globally, alter tick rates,
 * attach a debugger, bypass integrity checks, or modify original DLL files.
 */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "mxl_smoothing.h"
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
    BYTE expected[16];
    BYTE replacement[16];
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

/* At the original clamp: EAX:ECX = one simulation interval;
 * EDI:EDX = nonnegative elapsed time. The original negative-time path remains
 * outside this patch. Preserve frame interval and all other live registers.
 * Realm type 3 permits at most half a tick of extra visual prediction.
 */
__declspec(naked) static void bounded_mp_interval(void) {
    __asm {
        push eax
        push ecx
        push ebx
        push esi
        mov ebx, ecx
        mov esi, eax
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
        if (!sites[i].length || sites[i].length>16) return FALSE;
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
    BytePatch patches[6];
    for (size_t i=0;i<5;++i) patches[i]=clock_patch(&sites[i]);
    const BYTE clamp_signature[12]={0x39,0xca,0x89,0xfb,0x19,0xc3,0x0f,0x43,0xf8,0x0f,0x43,0xd1};
    memset(&patches[5],0,sizeof(patches[5]));
    patches[5].instruction=fb+0xedbf; patches[5].length=12;
    patches[5].label="D2FPS+EDBF realm-only bounded interpolation";
    memcpy(patches[5].expected,clamp_signature,12);
    memset(patches[5].replacement,0x90,12); patches[5].replacement[0]=0xe8;
    DWORD relative=(DWORD)((uintptr_t)bounded_mp_interval-(uintptr_t)(fb+0xedbf+5));
    memcpy(patches[5].replacement+1,&relative,4);
    game_type=(const volatile DWORD *)(cb+0x11c394);
    if (apply_patches(patches,6)) {
        smoothing_active=1;
        log_line("SUCCESS: 6 regions changed and verified; simulation interval remains 40 ms.");
        log_line("Source and consumer both resolve to winmm!timeGetTime at %08lX.",precise);
        log_line("Realm type 3: maximum extra visual prediction is 20 ms. SP/LAN: original clamp.");
    } else log_line("FAILED: patch attempt did not pass all checks; see preceding reason.");
done:
#if MXL_ENABLE_DIAGNOSTICS
    if (log_file!=INVALID_HANDLE_VALUE) { CloseHandle(log_file); log_file=INVALID_HANDLE_VALUE; }
#else
    (void)0;
#endif

}

int __stdcall MxlSmoothing_IsActive(void) { return smoothing_active == 1; }

#ifdef MXL_SMOOTHING_TEST
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
        call bounded_mp_interval
        mov result_lo, edx
        mov result_hi, edi
        mov actual_il, ecx
        mov actual_ih, eax
        mov actual_bx, ebx
        mov actual_si, esi
    }
    *preserved=actual_il==il && actual_ih==ih && actual_bx==0x12345678 && actual_si==0x23456789;
    return ((uint64_t)result_hi<<32)|result_lo;
}
int main(void) {
    for (int i=0;i<3;++i) {
        int result=test_case(i);
        printf("case %d (%s): %s [%d]\n",i,i==0?"five executable sites + protection restore":i==1?"bad last signature leaves all unchanged":"second-page failure restores first page",result?"FAIL":"PASS",result);
        if (result) return result;
    }
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
    puts("All native x86 timing-patch tests passed."); return 0;
}
#endif
