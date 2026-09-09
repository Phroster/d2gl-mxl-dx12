#pragma once
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
void __stdcall MxlSmoothing_Initialize(void);
int __stdcall MxlSmoothing_IsActive(void);
#if MXL_ENABLE_DIAGNOSTICS
typedef struct {
    DWORD samples, game_type, client_updates, client_update_ms, clock_ms;
    ULONGLONG elapsed_ticks, interval_ticks, clamped_ticks;
    ULONGLONG render_ticks, update_ticks, probe_ticks;
} MxlMotionSnapshot;
int __stdcall MxlSmoothing_ReadMotion(MxlMotionSnapshot *output);
#endif
#ifdef __cplusplus
}
#endif
