#include "asset_probe.h"
#include "diagnostics.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cstring>

using namespace mxl::diag;
namespace {
constexpr DWORD Incoming=0x12345678,Outgoing=0x76543210;
const char* expected_name=nullptr;
void* expected_handle=reinterpret_cast<void*>(0x12340000);
void** expected_output=nullptr;
uint8_t buffer[24]{};
uint32_t completed=0;
bool throws=false,fails=false;
unsigned opens=0,reads=0,closes=0;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
uint32_t __fastcall open_original(const char* path,void** output){
    const auto error=GetLastError();++opens;
    require(error==Incoming,"Open incoming LastError changed.");
    require(path==expected_name && output==expected_output,"Open register arguments changed.");
    if(throws)throw std::runtime_error("native open exception");
    if(!fails)*output=expected_handle;
    SetLastError(Outgoing);return fails?0:0x1234;
}
uint32_t __fastcall read_original(void* handle,void* target,uint32_t requested,uint32_t* amount,
    uint32_t fifth,uint32_t sixth,uint32_t seventh){
    const auto error=GetLastError();++reads;
    require(error==Incoming,"Read incoming LastError changed.");
    require(handle==expected_handle && target==buffer,"Read register arguments changed.");
    require(requested==17 && amount==&completed && fifth==0x155 && sixth==0x266 && seventh==0x377,"Read stack arguments changed.");
    if(throws)throw std::runtime_error("native read exception");
    if(!fails){memset(target,0x5a,17);*amount=17;}
    SetLastError(Outgoing);return fails?0:0x2345;
}
uint32_t __fastcall close_original(void* handle){
    const auto error=GetLastError();++closes;
    require(error==Incoming && handle==expected_handle,"Close input changed.");
    if(throws)throw std::runtime_error("native close exception");
    SetLastError(Outgoing);return fails?0:0x3456;
}
void verify_result(uint32_t result,uint32_t expected){
    const auto error=GetLastError();require(error==Outgoing,"Outgoing LastError changed.");require(result==expected,"Native return value changed.");
}
}
int wmain(int argc,wchar_t** argv){
    try {
        require(argc==2,"Expected output directory.");
        auto page=static_cast<void**>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        require(page!=nullptr,"Allocate test import page.");
        void* originals[]={reinterpret_cast<void*>(&open_original),reinterpret_cast<void*>(&read_original),reinterpret_cast<void*>(&close_original)};
        void** slots[6]{};for(unsigned i=0;i<6;++i){page[i]=originals[i%3];slots[i]=page+i;}
        page[5]=nullptr;
        require(!test_asset_imports(slots,open_original,read_original,close_original),"Mixed import table accepted.");
        for(unsigned i=0;i<5;++i)require(page[i]==originals[i%3],"Validation failure changed an earlier import.");
        page[5]=originals[2];slots[5]=reinterpret_cast<void**>(1);
        require(!test_asset_imports(slots,open_original,read_original,close_original),"Unreadable import accepted.");
        slots[5]=page+5;slots[3]=slots[0];
        require(!test_asset_imports(slots,open_original,read_original,close_original),"Duplicate import slot accepted.");
        slots[3]=page+3;
        DWORD old=0;require(VirtualProtect(page,4096,PAGE_READONLY,&old)!=0,"Protect imports.");
        require(start(nullptr,argv[1]),"Start logger.");
        require(assets_enabled(),"Asset fixture must explicitly enable recording.");
        require(test_asset_imports(slots,open_original,read_original,close_original),"Install verified imports.");
        MEMORY_BASIC_INFORMATION info{};VirtualQuery(page,&info,sizeof(info));require(info.Protect==PAGE_READONLY,"Import page protection changed.");
        void* handle=nullptr;expected_output=&handle;
        const char path[]="data\\global\\monsters\\co\"ld,first.dcc";
        expected_name=path;
        for(unsigned source=0;source<2;++source){
            auto open=reinterpret_cast<AssetOpenFn>(page[source*3]);auto read=reinterpret_cast<AssetReadFn>(page[source*3+1]);
            auto close=reinterpret_cast<AssetCloseFn>(page[source*3+2]);
            SetLastError(Incoming);verify_result(open(path,&handle),0x1234);require(handle==expected_handle,"Open output changed.");
            uintptr_t stack_before=0,stack_after=0;
            __asm mov stack_before,esp
            SetLastError(Incoming);verify_result(read(handle,buffer,17,&completed,0x155,0x266,0x377),0x2345);
            __asm mov stack_after,esp
            require(stack_before==stack_after,"Fastcall read stack was not restored.");
            require(completed==17 && buffer[0]==0x5a && buffer[16]==0x5a && buffer[17]==0,"Read output was altered.");
            SetLastError(Incoming);verify_result(close(handle),0x3456);
            throws=true;
            for(unsigned op=0;op<3;++op){
                bool caught=false;SetLastError(Incoming);
                try {if(op==0)open(path,&handle);else if(op==1)read(handle,buffer,17,&completed,0x155,0x266,0x377);else close(handle);}
                catch(const std::runtime_error& e){caught=std::string(e.what()).starts_with("native ");}
                require(caught,"Native exception was swallowed or replaced.");
            }
            throws=false;fails=true;
            SetLastError(Incoming);verify_result(open(path,&handle),0);
            SetLastError(Incoming);verify_result(read(handle,buffer,17,&completed,0x155,0x266,0x377),0);
            SetLastError(Incoming);verify_result(close(handle),0);fails=false;
        }
        auto open=reinterpret_cast<AssetOpenFn>(page[0]);
        expected_name=reinterpret_cast<const char*>(1);SetLastError(Incoming);verify_result(open(expected_name,&handle),0x1234);
        const std::string long_path(160,'x');expected_name=long_path.c_str();SetLastError(Incoming);verify_result(open(expected_name,&handle),0x1234);
        stop(true);
        std::ifstream input(std::filesystem::path(argv[1])/"assets.csv");std::ostringstream content;content<<input.rdbuf();const auto csv=content.str();
        require(csv.find("operation,source,handle,")==0,"Asset CSV missing header.");
        require(csv.find("co\"\"ld,first.dcc\"")!=std::string::npos,"Quoted filename was not escaped.");
        require(csv.find("open,D2CMP,")!=std::string::npos && csv.find("read,D2Sound,")!=std::string::npos,"Source attribution missing.");
        require(csv.find(",3,17,17,1,9029")!=std::string::npos,"Read byte/result accounting wrong.");
        require(csv.find(",3,17,0,0,0")!=std::string::npos,"Failed read output was treated as valid.");
        require(csv.find("\"\",2,0,0,1,4660")!=std::string::npos,"Unreadable filename not reported.");
        require(csv.find(",1,0,0,1,4660")!=std::string::npos,"Truncated filename not reported.");
        const auto size=std::filesystem::file_size(std::filesystem::path(argv[1])/"assets.csv");
        expected_name=path;SetLastError(Incoming);verify_result(open(path,&handle),0x1234);
        require(std::filesystem::file_size(std::filesystem::path(argv[1])/"assets.csv")==size,"Stopped probe wrote more asset records.");
        require(opens==9 && reads==6 && closes==6,"An original request was skipped or repeated.");
        std::cout<<"PASS: six guarded import pointers; unchanged fastcall arguments, ESP, return values, outputs and LastError; native exceptions; read-only page restoration; CSV escaping, bounded paths and transparent stopped forwarding.\n";
        return 0;
    }catch(const std::exception& e){stop(true);std::cerr<<e.what()<<'\n';return 1;}
}
