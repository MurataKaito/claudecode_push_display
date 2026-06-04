#pragma once
#include <Arduino.h>

void displayBegin();
// 顔(アニメ1フレーム)＋下部テキストを描画してpush。frameを進めると動く。
void drawFaceFrame(const String& expr, const String& text, int frame);
void showUsage(int percent, int resetMin);
void showApprove(const String& title, const String& detail);
