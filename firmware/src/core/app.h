#pragma once

void appSetup();
void appLoop();
int appTouchReleases();  // /state 診断用カウンタ
int appLastGesture();    // /state 診断用（最後のジェスチャ 0NONE/1TAP/2DOUBLE/3SWIPE）
