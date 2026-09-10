// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "world_objects.h"
#define MXL_ENABLE_DIAGNOSTICS 1
namespace fixture {
namespace d2 {
enum class UnitType { Object,Item };
struct UnitAny {UnitType dwType=UnitType::Object;struct {uint32_t dwUnitId=17,dwClassId=387,dwInitSeed=93,dwMode=0;void* pObjectData=nullptr;} v110;};
unsigned level=74,*level_no=&level;
UnitAny* getSelectedUnit() {return nullptr;}
}
bool objectIndicatorsEnabled=true;
int cx=17000,cy=8000,vs=0,*cameraX=&cx,*cameraY=&cy,*viewShift=&vs;
std::filesystem::path directory;
mxl::native_loot::ObjectLook objectLook(d2::UnitAny*) {return {mxl::native_loot::ObjectKind::Container,1,5,1,12};}
#include "modules/native_object_audit.inl"
void check(bool ok) {if(!ok) throw std::runtime_error("private object audit regression");}
void run() {
    directory=std::filesystem::temp_directory_path()/("mxl-object-test-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(directory);
    const auto path=directory/("mxl-object-audit-"+std::to_string(GetCurrentProcessId())+".log");
    std::array<uint8_t,448> table{};table[0xc4]=1;table[0x167]=8;table[0x1b3]=4;table[0x150]=1;
    const auto* ptr=table.data();d2::UnitAny unit;unit.v110.pObjectData=&ptr;
    auditObject(&unit,17512,8330,false);auditObject(&unit,0,0,true);
    mxl::native_loot::ObjectIndicator e{};e.identity={17,387,93,0,0,0,{74,1024,768,0,false}};e.x=512;e.y=338;
    auditObjectDraw(e,0);auditObjectDraw(e,1);auditObjectDraw(e,2);
    check(!std::filesystem::exists(path));
    const auto* r=auditRow(17,387,93,74);
    check(r && r->seen==1 && r->hovered==1 && r->captured==1 && r->painted==1 && r->named==1);
    check(r->operate==4 && r->subclass==8 && r->rawX==17512 && r->x==512);
    flushObjectAudit();check(std::filesystem::exists(path) && !objectAuditDirty);
    std::ifstream file(path);std::string text((std::istreambuf_iterator<char>(file)),{});file.close();
    check(text.find("17,387,93,74,0,8,4,1,1,0,1,5,1,1,1,1,1,17512,8330,512,338,17000,8000,0")!=std::string::npos);
    const auto length=std::filesystem::file_size(path);flushObjectAudit();check(std::filesystem::file_size(path)==length);
    std::filesystem::remove(path);std::filesystem::remove(directory);
    puts("PASS: private object observations buffered during play, complete on flush, no duplicate flush.");
}
}
int main() {try {fixture::run();return 0;}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
