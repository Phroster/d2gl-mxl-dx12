#include "input_profile.h"
#include <psapi.h>
#include <cwchar>
#include <cstring>

namespace mxl::diag {
static uint64_t filetime(FILETIME value) {return (uint64_t(value.dwHighDateTime)<<32)|value.dwLowDateTime;}
InputSample input_sample() noexcept {
    const auto last_error=GetLastError();InputSample sample;
    FILETIME created{},exited{},kernel{},user{};
    if(GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user)) {
        sample.user=filetime(user);sample.kernel=filetime(kernel);sample.valid|=ThreadTimes;
    }
    ULONG64 cycles=0;
    if(QueryThreadCycleTime(GetCurrentThread(),&cycles)){sample.cycles=cycles;sample.valid|=ThreadCycles;}
    if(GetProcessIoCounters(GetCurrentProcess(),&sample.io))sample.valid|=ProcessIo;
    PROCESS_MEMORY_COUNTERS memory{};memory.cb=sizeof(memory);
    if(GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory))){sample.faults=memory.PageFaultCount;sample.valid|=ProcessFaults;}
    SetLastError(last_error);return sample;
}
InputResult input_difference(const InputSample& before,const InputSample& after,uint64_t began,uint64_t ended) noexcept {
    InputResult result;result.began=began;result.ended=ended;result.valid=before.valid&after.valid;
    if(result.valid&ThreadTimes) {
        if(after.user<before.user || after.kernel<before.kernel)result.valid&=~ThreadTimes;
        else {result.user_ms=double(after.user-before.user)/10000.0;result.kernel_ms=double(after.kernel-before.kernel)/10000.0;}
    }
    if(result.valid&ThreadCycles) {
        if(after.cycles<before.cycles)result.valid&=~ThreadCycles;else result.cycles=after.cycles-before.cycles;
    }
    if(result.valid&ProcessIo) {
        if(after.io.ReadTransferCount<before.io.ReadTransferCount || after.io.ReadOperationCount<before.io.ReadOperationCount ||
           after.io.WriteTransferCount<before.io.WriteTransferCount || after.io.WriteOperationCount<before.io.WriteOperationCount ||
           after.io.OtherTransferCount<before.io.OtherTransferCount || after.io.OtherOperationCount<before.io.OtherOperationCount)result.valid&=~ProcessIo;
        else {
            result.read_bytes=after.io.ReadTransferCount-before.io.ReadTransferCount;result.read_ops=after.io.ReadOperationCount-before.io.ReadOperationCount;
            result.write_bytes=after.io.WriteTransferCount-before.io.WriteTransferCount;result.write_ops=after.io.WriteOperationCount-before.io.WriteOperationCount;
            result.other_bytes=after.io.OtherTransferCount-before.io.OtherTransferCount;result.other_ops=after.io.OtherOperationCount-before.io.OtherOperationCount;
        }
    }
    if(result.valid&ProcessFaults)result.faults=after.faults-before.faults;
    return result;
}
void input_owner(const void* procedure,InputResult& result) noexcept {
    const auto last_error=GetLastError();HMODULE owner=nullptr;wchar_t path[32768]{};
    if(procedure && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(procedure),&owner) && GetModuleFileNameW(owner,path,32768)) {
        const auto slash=wcsrchr(path,L'\\');const auto name=slash?slash+1:path;
        if(WideCharToMultiByte(CP_UTF8,0,name,-1,result.module,sizeof(result.module),nullptr,nullptr)>0) {
            for(auto& ch:result.module){if(!ch)break;if(ch==','||ch=='"'||ch=='\r'||ch=='\n')ch='_';}
            result.module_offset=reinterpret_cast<uintptr_t>(procedure)-reinterpret_cast<uintptr_t>(owner);
        } else strcpy_s(result.module,"unknown");
    }
    SetLastError(last_error);
}
}
