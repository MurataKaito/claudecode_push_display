#pragma once
#include <Arduino.h>

void displayBegin();
// アイドル/通知時に毎ループ呼ぶ。内部で自前タイミング管理し、生き生きアニメを1tick描画。
void tickFace(const String& expr, const String& text);
String displayDebug();
void showUsage(int percent, int resetMin);
void showApprove(const String& title, const String& detail);
