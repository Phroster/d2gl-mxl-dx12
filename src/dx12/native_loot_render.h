// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace mxl::native_loot {
// Every rank contains premultiplied light, including potion rings. Native
// DrawMode 5 replaces covered pixels, making overlapping animations cut holes
// in each other. DrawMode 3 adds their light and is independent of draw order.
constexpr int effect_draw_mode(unsigned) { return 3; }
}
