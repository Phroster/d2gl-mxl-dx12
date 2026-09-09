#pragma once
#include <windows.h>
#include <cstdint>

namespace mxl::diag {
enum SampleValid : uint32_t { ThreadTimes=1, ProcessIo=2, ProcessFaults=4, ThreadCycles=8 };
struct InputSample {
    uint64_t user=0,kernel=0,cycles=0;
    IO_COUNTERS io{};
    uint32_t faults=0,valid=0;
};
struct InputResult {
    uint64_t began=0,ended=0;
    double user_ms=-1,kernel_ms=-1;
    uint64_t cycles=0,read_bytes=0,read_ops=0,write_bytes=0,write_ops=0,other_bytes=0,other_ops=0;
    uint32_t faults=0,valid=0;
    char module[80]="unknown";
    uintptr_t module_offset=0;
};
InputSample input_sample() noexcept;
InputResult input_difference(const InputSample& before,const InputSample& after,uint64_t began,uint64_t ended) noexcept;
void input_owner(const void* procedure,InputResult& result) noexcept;
#if MXL_ENABLE_DIAGNOSTICS
void record_input(const InputResult& result) noexcept;
#else
inline void record_input(const InputResult&) noexcept {}
#endif
}
