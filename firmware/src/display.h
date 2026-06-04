#pragma once
#include <Arduino.h>

void displayBegin();
void showIdle();
// expr: "normal" | "happy" | "worried" | "surprised"
void showNotify(const String& expr, const String& text);
void showUsage(int percent, int resetMin);
void showApprove(const String& title, const String& detail);
