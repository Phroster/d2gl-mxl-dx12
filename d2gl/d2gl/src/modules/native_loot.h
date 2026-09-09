// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace d2gl::modules::NativeLoot {
void beginFrame();
void capture(int x, int y);
void hoverLabel(int left,int top,int right,int bottom);
void beforeLeftClick(int x,int y);
void inputMessage(unsigned message);
void drawLabels();
bool suppressHoverLabel();
void shutdown();
}
