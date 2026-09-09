// Public builds have no recording state, writer, configuration or worker thread.
#include "diagnostics.h"
namespace mxl::diag {
uint64_t ticks() noexcept {
    LARGE_INTEGER value; QueryPerformanceCounter(&value); return uint64_t(value.QuadPart);
}
double milliseconds(uint64_t elapsed) noexcept {
    static const double scale=[] {
        LARGE_INTEGER frequency; QueryPerformanceFrequency(&frequency);
        return frequency.QuadPart ? 1000.0 / frequency.QuadPart : 0.0;
    }();
    return double(elapsed) * scale;
}
}
