#include "display.h"
#include <M5Unified.h>
#include <LittleFS.h>

static M5Canvas g_cv(&M5.Display);
static bool g_cvReady = false;
static int g_pngFrames = 0;  // /clawd0.png.. の枚数（公式素材）

static int countPngFrames() {
  int n = 0;
  for (int i = 0; i < 16; i++) {
    if (LittleFS.exists("/clawd" + String(i) + ".png")) n++;
    else break;
  }
  return n;
}

// 公式フレームPNG（あれば）を1枚、黒地canvasに描く
static void renderPngFrame(int frame) {
  int idx = (g_pngFrames > 0) ? (frame % g_pngFrames) : 0;
  String p = "/clawd" + String(idx) + ".png";
  File f = LittleFS.open(p, "r");
  if (!f) return;
  size_t sz = f.size();
  uint8_t* buf = (uint8_t*)malloc(sz);
  if (buf) {
    f.read(buf, sz);
    f.close();
    // 画面中央(顔エリア)に中央寄せで描画。どんなサイズのフレームでも中央に出る。
    g_cv.drawPng(buf, sz, 160, 100, 0, 0, 0, 0, 1.0f, 0.0f, middle_center);
    free(buf);
  } else {
    f.close();
  }
}

// オリジナルのドット絵クリーチャー（公式風）を黒地canvasに1フレーム描く（歩行アニメ）
static void renderCode(const String& expr, int frame) {
  uint16_t clay = M5.Display.color565(201, 119, 92);
  uint16_t ink = M5.Display.color565(20, 18, 16);
  uint16_t gray = M5.Display.color565(160, 160, 160);

  static const char* body[8] = {
      "................",
      ".111111111111...",
      ".111111111111...",
      ".111211112111...",
      ".111111111111...",
      ".111111111111...",
      ".111111111111...",
      ".111111111111...",
  };
  const int S = 14;
  const int ox = 48;
  const int bob = (frame % 4 == 1) ? -3 : (frame % 4 == 3) ? 3 : 0;
  const int oy = 34 + bob;

  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 16; c++) {
      char ch = body[r][c];
      if (ch == '1') {
        g_cv.fillRect(ox + c * S, oy + r * S, S, S, clay);
      } else if (ch == '2') {
        g_cv.fillRect(ox + c * S, oy + r * S, S, S, clay);
        int es = (expr == "surprised") ? S : S * 3 / 5;
        int off = (S - es) / 2;
        g_cv.fillRect(ox + c * S + off, oy + r * S + off, es, es, ink);
      }
    }
  if (expr == "worried") {  // ハの字眉
    g_cv.drawLine(ox + 3 * S, oy + 2 * S, ox + 5 * S, oy + 3 * S - 3, ink);
    g_cv.drawLine(ox + 12 * S, oy + 2 * S, ox + 10 * S, oy + 3 * S - 3, ink);
  }

  // 脚（歩行: フレームで上下交互）
  const int legCols[4] = {3, 6, 9, 12};
  for (int k = 0; k < 4; k++) {
    int len = ((k + frame) % 2 == 0) ? 2 : 1;
    g_cv.fillRect(ox + legCols[k] * S, oy + 8 * S, S, len * S, clay);
  }

  // しっぽ（happy=上で振る / worried=下 / 通常=軽く振る）
  int wag = frame % 2;
  if (expr == "happy") {
    g_cv.fillRect(ox + 13 * S, oy + wag * S, S, S, gray);
  } else if (expr == "worried") {
    g_cv.fillRect(ox + 13 * S, oy + 8 * S, S, S, gray);
  } else {
    g_cv.fillRect(ox + 13 * S, oy + (3 + wag) * S, S, S, gray);
  }
}

void displayBegin() {
  LittleFS.begin(true);
  M5.Display.setRotation(1);
  M5.Display.fillScreen(TFT_BLACK);
  g_cv.setColorDepth(16);
  g_cvReady = (g_cv.createSprite(320, 240) != nullptr);
  g_cv.setFont(&fonts::lgfxJapanGothic_20);
  g_pngFrames = countPngFrames();
}

void drawFaceFrame(const String& expr, const String& text, int frame) {
  if (!g_cvReady) return;
  g_cv.fillScreen(TFT_BLACK);
  if (g_pngFrames > 0) renderPngFrame(frame);
  else renderCode(expr, frame);

  g_cv.fillRect(0, 200, 320, 40, TFT_WHITE);
  g_cv.setTextColor(TFT_BLACK, TFT_WHITE);
  g_cv.setTextDatum(middle_center);
  g_cv.setTextSize(1);
  g_cv.drawString(text, 160, 220);
  g_cv.pushSprite(0, 0);
}

void showUsage(int percent, int resetMin) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextDatum(middle_center);

  M5.Display.setTextSize(2);
  M5.Display.drawString(String(percent) + "%", 160, 55);

  const int x = 20, y = 95, w = 280, h = 30;
  M5.Display.drawRect(x, y, w, h, TFT_WHITE);
  int fill = (w - 2) * percent / 100;
  uint16_t c = percent >= 80 ? TFT_RED : (percent >= 50 ? TFT_ORANGE : TFT_GREEN);
  M5.Display.fillRect(x + 1, y + 1, fill, h - 2, c);

  M5.Display.setTextSize(1);
  M5.Display.drawString("あと " + String(resetMin) + "分でリセットなのだ", 160, 160);
}

void showApprove(const String& title, const String& detail) {
  M5.Display.fillScreen(TFT_NAVY);
  M5.Display.setTextColor(TFT_WHITE, TFT_NAVY);
  M5.Display.setTextDatum(middle_center);

  M5.Display.setTextSize(2);
  M5.Display.drawString(title.length() ? title : "CLAUDE OK?", 160, 45);

  M5.Display.setTextSize(1);
  String d = detail;
  if (d.length() > 38) d = d.substring(0, 37) + "…";
  M5.Display.drawString(d, 160, 110);

  M5.Display.setTextColor(TFT_GREENYELLOW, TFT_NAVY);
  M5.Display.drawString("TAP = OK   /   SWIPE = NG", 160, 175);
}
