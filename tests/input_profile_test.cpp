#include "diagnostics.h"
#include "input_profile.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

using namespace mxl::diag;
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
__declspec(noinline) uint64_t cpu_work() {
    uint64_t value=123,started=ticks();
    while(milliseconds(ticks()-started)<250)for(int i=0;i<10000;++i)value=(value*6364136223846793005ull+1)^(value>>17);
    return value;
}
template<class F> InputResult measure(F&& work) {
    const auto before=input_sample();const auto began=ticks();work();const auto ended=ticks();
    auto result=input_difference(before,input_sample(),began,ended);
    input_owner(reinterpret_cast<const void*>(&cpu_work),result);record_input(result);return result;
}
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==2,"Pass an output directory.");require(start(nullptr,argv[1]),"Start diagnostics.");
        SetLastError(1234);auto sample=input_sample();require(GetLastError()==1234,"Sampler changed last error.");
        require((sample.valid&15)==15,"Local test process counters unavailable.");
        const auto idle=measure([]{Sleep(150);});
        require(milliseconds(idle.ended-idle.began)>=100 && idle.user_ms+idle.kernel_ms<50,"Sleep was not distinguished from CPU work.");
        uint64_t value=0;const auto busy=measure([&]{value=cpu_work();});
        require(busy.user_ms+busy.kernel_ms>20 && busy.cycles>0,"CPU work was not observed.");
        require(std::string(busy.module).find("dx12_input_profile_test")!=std::string::npos && busy.module_offset>0,"Procedure module/RVA was not resolved.");
        const auto path=(std::filesystem::path(argv[1])/"io-fixture.bin").wstring();
        std::vector<uint8_t> data(1024*1024,0x56);DWORD transferred=0;
        HANDLE file=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY,nullptr);
        require(file!=INVALID_HANDLE_VALUE,"Create fixture.");
        require(WriteFile(file,data.data(),DWORD(data.size()),&transferred,nullptr)&&transferred==data.size(),"Write fixture.");
        SetFilePointer(file,0,nullptr,FILE_BEGIN);
        const auto io=measure([&]{require(ReadFile(file,data.data(),DWORD(data.size()),&transferred,nullptr)&&transferred==data.size(),"Read fixture.");});
        CloseHandle(file);
        require(io.read_bytes>=data.size() && io.read_ops>=1,"Cached reads were not counted.");
        auto* memory=static_cast<volatile uint8_t*>(VirtualAlloc(nullptr,1024*1024,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        require(memory!=nullptr,"Allocate test pages.");
        const auto faults=measure([&]{for(size_t i=0;i<1024*1024;i+=4096)memory[i]=1;});
        VirtualFree(const_cast<uint8_t*>(memory),0,MEM_RELEASE);
        require(faults.faults>=128,"Demand-zero faults were not counted.");
        InputSample missing;const auto now=ticks();auto unavailable=input_difference(sample,missing,now,now);
        require(unavailable.valid==0 && unavailable.user_ms==-1,"Unavailable counters reported as valid zero.");
        record_input(unavailable);
        stop(true);
        std::ifstream input(std::filesystem::path(argv[1])/"input.csv");
        const std::string text((std::istreambuf_iterator<char>(input)),{});
        require(text.find("wall_minus_cpu_ms")!=std::string::npos && text.find("dx12_input_profile_test.exe")!=std::string::npos,"Input report missing.");
        std::cout<<"Sleep: wall="<<milliseconds(idle.ended-idle.began)<<" ms CPU="<<idle.user_ms+idle.kernel_ms<<" ms.\n"
                 <<"Busy: wall="<<milliseconds(busy.ended-busy.began)<<" ms CPU="<<busy.user_ms+busy.kernel_ms<<" ms cycles="<<busy.cycles<<" checksum="<<value<<".\n"
                 <<"I/O: "<<io.read_bytes<<" cached read bytes. Faults: "<<faults.faults<<".\n"
                 <<"PASS: CPU/wait distinction, I/O, faults, module identity, unavailable flags and input CSV.\n";
        return 0;
    }catch(const std::exception& error){stop(true);std::cerr<<error.what()<<"\n";return 1;}
}
