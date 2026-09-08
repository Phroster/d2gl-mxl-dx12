#pragma once
#include <windows.h>
#include <cstdint>

namespace mxl::archive_hash {
struct Result {uint32_t eax=0,ecx=0,edx=0,flags=0;};
// Reuses native hash outputs only while one synchronous CMP open is running.
// No filename, file handle or archive result survives that open.
class Scope {
public:
    explicit Scope(const char* path,bool enabled=true) noexcept;
    ~Scope();
    void leave() noexcept; // Also called by the native-open SEH finally block.
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
private:
    friend bool dispatch(void* registers) noexcept;
    Scope* previous_=nullptr;
    const char* input_=nullptr;
    char path_[260]{};
    uint32_t length_=0;
    bool active_=false,entered_=true,valid_[3]{};
    Result values_[3]{};
};
// Caller has verified the Storm file identity. Checks relocated live code and
// the three archive-lookup CALLs before installing a single entry detour.
bool start(HMODULE storm);
void flush_stats();
struct Stats {uint64_t hits=0,native_calls=0;};
Stats take_stats();
#ifdef MXL_ARCHIVE_HASH_TEST
void test_bind(void* native,uintptr_t lookup_base);
bool test_dispatch(uint32_t* registers);
bool test_code_ready(uintptr_t base);
#endif
}
