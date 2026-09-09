// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdio>
#include <windows.h>
#include <filesystem>
#include <stdexcept>
#include "native_loot_pickup.h"
#include "native_loot_cells.h"
using namespace mxl::native_loot;
void require(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
int main(int argc,char** argv) {
    try {
        require(ground_label_scale(1)>=.95f,"quiet loot below the readable label floor");
        for(unsigned rank=2;rank<=4;++rank)
            require(ground_label_scale(rank)>ground_label_scale(rank-1),"valuable loot label did not grow with effect rank");
        // A drop becomes visible before it lands; names must follow its effect
        // through both states, then disappear when it returns to inventory.
        const unsigned dropModes[]={0,4,5,3,0};
        const bool labelVisible[]={false,false,true,true,false};
        for(unsigned i=0;i<std::size(dropModes);++i)
            require(ground_label_mode(dropModes[i])==labelVisible[i],"label waits for landing or survives pickup");
        require(refresh_ground_name(true,101,100),"empty native name retained until five-second cache expiry");
        require(!refresh_ground_name(false,101,100),"ready native name reformatted every draw");
        require(refresh_ground_name(false,5101,100),"ready name never refreshes");
        require(!refresh_ground_name(false,20,UINT32_MAX-20),"name cache timer rollover caused a false expiry");
        for(const auto size:{std::pair{640,480},std::pair{800,600},std::pair{1024,768},std::pair{1600,900},std::pair{1601,901}}) {
            const auto [w,h]=size;
            require(world_input_point(0,w,h,0,0)&&world_input_point(0,w,h,w-1,h-49),"uncovered world blocked");
            require(world_input_point(1,w,h,w/2-1,100),"inventory blocked visible world");
            require(!world_input_point(1,w,h,w/2,100)&&!world_input_point(1,w,h,w-1,100),"inventory slots became loot targets");
            require(world_input_point(2,w,h,w/2,100)&&world_input_point(2,w,h,w-1,100),"left panel blocked visible world");
            require(!world_input_point(2,w,h,w/2-1,100),"left panel became loot target");
            require(!world_input_point(3,w,h,10,100)&&!world_input_point(3,w,h,w-10,100),"both panels left stale world target");
            for(unsigned panels=0;panels<4;++panels) {
                require(!world_input_point(panels,w,h,-1,100)&&!world_input_point(panels,w,h,w,100),"outside window target");
                require(!world_input_point(panels,w,h,w/2,h-48),"belt/HUD became loot target");
            }
        }
        for(unsigned p=1;p<ProfileCount;++p) {
            const auto& profile=profiles[p];
            const auto size=effect_size(profile.rank,profile.style);
            const auto box=effect_hitbox(400,300,size.width,size.height);
            require(box.contains(357,320)&&box.contains(443,260),"small ground target");
            require(box.contains(400,300-size.height+12),"beam tip not pickable");
            require(!box.contains(box.right+1,300)&&!box.contains(400,325),"hitbox escapes bounds");
        }
        PickChoice pick;
        pick.offer(0,10,200,200,180,190,false);
        pick.offer(1,99,200,200,205,205,false);
        require(pick.index==1,"nearest overlapping item not chosen");
        pick.offer(2,55,200,200,195,195,false);
        require(pick.index==2,"tie depends on traversal order");
        pick.offer(3,66,200,200,100,100,true);
        require(pick.index==3,"visible name lost to a different beam");
        GroundEntry item{42,1242,777,1234,0,0,{1,1600,900,0,false}};
        HoverLabel label{item,{-80,-40,80,-20},100,true};
        require(label.contains(item,400,300,465,270,220),"name edge cannot be clicked");
        require(!label.contains(item,400,300,465,270,221),"stale name retained");
        auto other=item;other.seed++;
        require(!label.contains(other,400,300,465,270,110),"reused item id retained old name");
        other=item;other.view.panels=1;
        require(!label.contains(other,400,300,465,270,110),"panel transition kept label target");
        require(same_identity(item,other),"panel movement discarded a still-valid item name");
        other.seed++;require(!same_identity(item,other),"reused item id kept another item's name");
        require(label.contains(item,410,300,475,270,110),"label failed to follow camera");
        auto shifted=item;shifted.view.panels=1;
        HoverLabel shiftedLabel{shifted,{-80,-40,80,-20},200,true};
        require(shiftedLabel.contains(shifted,200,300,265,270,210),"fresh panel-shifted label not clickable");
        require(!shiftedLabel.contains(item,400,300,465,270,210),"inventory close reused shifted label");
        require(fresh_pick_frame(150,100,true,false,!world_input_point(1,1600,900,600,400),false,false,false),"inventory world pickup disabled");
        require(!fresh_pick_frame(150,100,true,false,!world_input_point(1,1600,900,1000,400),false,false,false),"inventory slot pickup enabled");
        require(fresh_pick_frame(150,100,true,false,false,false,false,false),"normal hover gated");
        require(!fresh_pick_frame(221,100,true,false,false,false,false,false),"stale rendered targets remain active");
        require(!fresh_pick_frame(150,100,false,false,false,false,false,false),"menu targets active");
        require(!fresh_pick_frame(150,100,true,true,false,false,false,false),"locked click retargeted");
        require(!fresh_pick_frame(150,100,true,false,true,false,false,false),"inventory stolen");
        require(!fresh_pick_frame(150,100,true,false,false,true,false,false),"menu stolen");
        require(!fresh_pick_frame(150,100,true,false,false,false,true,false),"Alt fallback stolen");
        require(!fresh_pick_frame(150,100,true,false,false,false,false,true),"combat input stolen");
        require(fresh_pick_frame(20,UINT32_MAX-20,true,false,false,false,false,false),"timer rollover breaks pickup");
        SelectionCache cache;
        SelectionKey state{1,1,0,0,0,0,0,100,200};
        require(cache.changed(state),"initial selection scan skipped");
        for(unsigned i=0;i<100000;++i) require(!cache.changed(state),"unchanged poll rescanned loot");
        state.x++;require(cache.changed(state),"mouse movement did not reconsider target");
        state.frame++;require(cache.changed(state),"new world frame ignored");
        state.input++;require(cache.changed(state),"button/key/focus input ignored");
        state.locked=1;require(cache.changed(state),"native action lock ignored");
        state.panels=1;require(cache.changed(state),"panel opening ignored");
        state.alt=1;require(cache.changed(state),"native label mode ignored");
        state.cursorAction=1;require(cache.changed(state),"dialog/cursor action ignored");
        state.cursor=0x0606;require(cache.changed(state),"identify cursor ignored");
        state.carryingItem=true;require(cache.changed(state),"item picked up onto cursor did not invalidate cached hover");
        state.carryingItem=false;require(cache.changed(state),"dropping cursor item did not resume world hover");
        unsigned healing=0,mana=0;
        constexpr unsigned cutoff[]={0,20,40,70,110};
        for(unsigned i=0;i<std::size(bases);++i) {
            const auto& b=bases[i];
            if(!b.potionGrade) continue;
            healing+=b.profile==P_HealingPotion;mana+=b.profile==P_ManaPotion;
            require(classify(4,3,i,2,0,1).rank==1,"leveling potion missing quiet effect");
            require(!classify(4,0,i,2,0,1).rank,"inventory potion glows");
            if(b.potionGrade<5) {
                require(classify(4,3,i,2,0,cutoff[b.potionGrade]-1).rank==1,"potion removed early");
                require(!classify(4,3,i,2,0,cutoff[b.potionGrade]).rank,"obsolete potion not removed");
            } else require(classify(4,3,i,2,0,150).rank==1,"best potion removed");
        }
        require(healing==5&&mana==5,"potion classification hit unrelated or quest items");
        if(argc>=2) {
            const auto path=std::filesystem::absolute(argv[1])/"D2Common.dll";
            SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
            auto dll=LoadLibraryExW(path.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
            require(dll!=nullptr,"cannot load installed D2Common in helper");
            using GetCursor=void*(__stdcall*)(void*);
            auto getCursor=reinterpret_cast<GetCursor>(GetProcAddress(dll,MAKEINTRESOURCEA(11017)));
            require(getCursor && reinterpret_cast<uint8_t*>(getCursor)-reinterpret_cast<uint8_t*>(dll)==0x1dfb0,"wrong native inventory getter");
            static_assert(sizeof(void*)==4);
            uint32_t inventory[9]{};
            require(!getCursor(nullptr)&&!getCursor(inventory),"invalid inventory accepted");
            inventory[0]=0x1020304;
            require(!getCursor(inventory),"empty cursor reported as held item");
            inventory[8]=0x12345678;
            require(getCursor(inventory)==reinterpret_cast<void*>(0x12345678),"held item was not detected");
            std::puts("PASS: installed D2Common cursor-item getter, valid/invalid inventory and held-item cases");
        }
        std::puts("PASS: native pickup targets, names, stale identity guards, UI gates and potion progression");
        return 0;
    } catch(const std::exception& e) { std::fprintf(stderr,"FAIL: %s\n",e.what());return 1; }
}
