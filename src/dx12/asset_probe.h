#pragma once
#include <windows.h>
#include <cstdint>

namespace mxl::diag {
enum class AssetOperation : uint32_t { Open, Read, Close };
#if MXL_ENABLE_DIAGNOSTICS
bool assets_enabled() noexcept;
#else
inline constexpr bool assets_enabled() noexcept { return false; }
#endif
bool start_assets();
// File names are bounded snapshots. Status: 0=complete, 1=truncated,
// 2=unreadable, 3=not an open operation. Handle reuse is resolved offline.
#if MXL_ENABLE_DIAGNOSTICS
void asset_call(AssetOperation operation, unsigned source, uintptr_t handle,
    uint64_t began, uint64_t ended, const char* name, unsigned name_status,
    uint32_t requested, uint32_t completed, bool completed_valid, uint32_t result) noexcept;
#else
inline void asset_call(AssetOperation, unsigned, uintptr_t, uint64_t, uint64_t, const char*, unsigned, uint32_t, uint32_t, bool, uint32_t) noexcept {}
#endif

#ifdef MXL_ASSET_TEST
using AssetOpenFn=uint32_t(__fastcall*)(const char*,void**);
using AssetReadFn=uint32_t(__fastcall*)(void*,void*,uint32_t,uint32_t*,uint32_t,uint32_t,uint32_t);
using AssetCloseFn=uint32_t(__fastcall*)(void*);
// Six IAT slots ordered CMP open/read/close, Sound open/read/close.
bool test_asset_imports(void*** slots, AssetOpenFn open, AssetReadFn read, AssetCloseFn close);
#endif
}
