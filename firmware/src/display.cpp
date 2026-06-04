#include "display.h"
#include <M5Unified.h>
#include <LittleFS.h>

static uint16_t exprColor(const String& expr) {
  if (expr == "happy") return TFT_GREEN;
  if (expr == "worried") return TFT_ORANGE;
  if (expr == "surprised") return TFT_CYAN;
  return TFT_DARKGREEN;  // normal
}

// 立ち絵があれば描画。無ければ簡易顔。
static void drawFace(const String& expr) {
  String path = "/" + expr + ".jpg";
  if (LittleFS.exists(path)) {
    // drawJpgFile(fs,...) は当バージョンのM5GFXでテンプレートが抽象型になり不可。
    // ファイルをメモリに読み込み、具象な drawJpg(buf,len) で描画する。
    File f = LittleFS.open(path, "r");
    if (f) {
      size_t sz = f.size();
      uint8_t* buf = (uint8_t*)malloc(sz);
      if (buf) {
        f.read(buf, sz);
        f.close();
        M5.Display.drawJpg(buf, sz, 0, 0, 320, 200);
        free(buf);
        return;
      }
      f.close();
    }
  }
  M5.Display.fillRect(0, 0, 320, 200, TFT_BLACK);
  M5.Display.fillCircle(160, 100, 70, exprColor(expr));   // 顔
  M5.Display.fillCircle(135, 90, 10, TFT_WHITE);          // 目
  M5.Display.fillCircle(185, 90, 10, TFT_WHITE);
  M5.Display.fillCircle(135, 90, 4, TFT_BLACK);
  M5.Display.fillCircle(185, 90, 4, TFT_BLACK);
}

static void drawTextBox(const String& text) {
  M5.Display.fillRect(0, 200, 320, 40, TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);
  M5.Display.drawString(text, 160, 220);
}

void displayBegin() {
  LittleFS.begin(true);
  M5.Display.setFont(&fonts::lgfxJapanGothic_20);
  M5.Display.setRotation(1);
  M5.Display.fillScreen(TFT_BLACK);
  showIdle();
}

void showIdle() {
  drawFace("normal");
  drawTextBox("まってるのだ");
}

void showNotify(const String& expr, const String& text) {
  drawFace(expr);
  drawTextBox(text.length() ? text : "おしらせなのだ");
}

void showUsage(int percent, int resetMin) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextDatum(middle_center);

  // 大きな%表示
  M5.Display.setTextSize(2);
  M5.Display.drawString(String(percent) + "%", 160, 55);

  // バーゲージ
  const int x = 20, y = 95, w = 280, h = 30;
  M5.Display.drawRect(x, y, w, h, TFT_WHITE);
  int fill = (w - 2) * percent / 100;
  uint16_t c = percent >= 80 ? TFT_RED : (percent >= 50 ? TFT_ORANGE : TFT_GREEN);
  M5.Display.fillRect(x + 1, y + 1, fill, h - 2, c);

  // リセットまでの時間
  M5.Display.setTextSize(1);
  M5.Display.drawString("あと " + String(resetMin) + "分でリセットなのだ", 160, 160);
}
