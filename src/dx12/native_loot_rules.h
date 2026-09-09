// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <iterator>
namespace mxl::native_loot {
#include "native_loot_catalog.generated.h"
struct Appearance { unsigned rank=0,colour=0; Style style=Style::Beam; unsigned profile=0; };
inline Appearance appearance(unsigned profile) {
    const auto& p=profiles[profile];return {p.rank,p.colour,p.style,profile};
}
inline bool useful_potion(unsigned grade,unsigned playerLevel) {
    // Same progression as SimpleFilterSoftNotify+ Progression. Better grades
    // remain useful at lower levels; never infer value from a potion's name.
    constexpr unsigned cutoff[]={0,20,40,70,110,UINT32_MAX};
    return grade>=1 && grade<=5 && playerLevel<cutoff[grade];
}
inline Appearance classify(uint32_t type, uint32_t mode, uint32_t base, uint32_t quality, uint32_t flags, unsigned playerLevel=0,unsigned itemLevel=0)
{
    if (type != 4 || (mode != 3 && mode != 5) || base >= std::size(bases)) return {};
    const auto& b = bases[base];
    if(b.disabled) return {};
    if(b.potionGrade && !useful_potion(b.potionGrade,playerLevel)) return {};
    if(b.profile==P_GemLesser && playerLevel>=50) return {};
    if(b.profile==P_Angelic || b.profile==P_Mastercrafted) return appearance(b.profile);
    if (quality == 7) {
        if(b.gear) return appearance(b.sacred?(b.scythe?P_ScytheUnique:P_SacredUnique):P_Unique);
        if(!b.profile) return appearance(P_Unique);
    }
    if (quality == 5 && b.gear) return appearance(b.sacred?(b.scythe?P_ScytheSet:P_SacredSet):P_Set);
    if(b.profile && !b.jewel) return appearance(b.profile);
    if(quality==6 || quality==8 || quality==9) {
        if(b.sacred) return appearance(b.scythe?P_ScytheBase:P_SacredRare);
        constexpr unsigned cutoffs[]={0,31,51,77,90};
        if(quality==6 && b.tier && (playerLevel>=cutoffs[b.tier] || itemLevel>=cutoffs[b.tier])) return {};
        if(b.gear) return appearance(P_Rare);
    }
    if(b.sacred && (quality==2 || quality==3)) {
        if(b.scythe) return appearance(P_ScytheBase);
        if(flags&0x400000) return appearance(P_CraftBase);
        if(b.superior || (quality==3 && b.craft)) return appearance(P_SacredBase);
    }
    if(b.jewel) return appearance(P_Supply);
    return {};
}
// Fixed-size draw budget; major loot has its own capacity so common runes
// cannot consume its allowance. No tracking or allocations per dropped item.
struct Budget {
    unsigned minor=0, valuable=0, major=0;
    bool take(unsigned rank) {
        if (rank >= 3) { if (major == 24) return false; ++major; return true; }
        if (rank == 2) { if (valuable == 24) return false; ++valuable; return true; }
        if (rank == 1) { if (minor == 12) return false; ++minor; return true; }
        return false;
    }
};
}
