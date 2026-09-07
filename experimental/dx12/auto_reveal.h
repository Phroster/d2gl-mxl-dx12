#pragma once
#include "reveal_probe.h"

namespace mxl::reveal {
// Called only after the supported MXL binary and reveal callback are verified.
bool start(HWND window, uintptr_t sigma, diag::RevealRootFn reveal);
void begin_frame(HWND window) noexcept;
void end_frame(HWND window, bool in_game) noexcept;
bool window_message(HWND window, UINT message, WPARAM token, bool in_game);
}
