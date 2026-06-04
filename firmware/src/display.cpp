#include "display.h"
#include <M5Unified.h>
#include <LittleFS.h>

static M5Canvas g_cv(&M5.Display);
static bool g_cvReady = false;
static int g_pngFrames = 0;  // /clawd0.png.. の枚数（公式素材）

// アニメ用タイミング状態
static uint32_t g_lastMs = 0;
static int g_frame = 0;
static uint32_t g_nextBlink = 0, g_blinkEnd = 0;
static uint32_t g_nextHop = 0, g_hopEnd = 0;
static uint32_t g_nextLook = 0, g_lookEnd = 0;
static int g_lookDir = 0;

static int countPngFrames() {
  int n = 0;
  for (int i = 0; i < 16; i++) {
    if (LittleFS.exists("/clawd" + String(i) + ".png")) n++;
    else break;
  }
  return n;
}

// 公式フレームPNG（あれば）を中央寄せで描く
static void renderPngFrame(int frame) {
  int idx = (g_pngFrames > 0) ? ((frame / 2) % g_pngFrames) : 0;
  String p = "/clawd" + String(idx) + ".png";
  File f = LittleFS.open(p, "r");
  if (!f) return;
  size_t sz = f.size();
  uint8_t* buf = (uint8_t*)malloc(sz);
  if (buf) {
    f.read(buf, sz);
    f.close();
    g_cv.drawPng(buf, sz, 160, 100, 0, 0, 0, 0, 1.0f, 0.0f, middle_center);
    free(buf);
  } else {
    f.close();
  }
}

// オリジナルのドット絵クリーチャー（黒地canvasへ1フレーム）。歩行＋表情＋まばたき＋視線＋ジャンプ。
static void renderCreature(const String& expr, int frame, int hop, bool blink, int lookDx) {
  const uint16_t clay = M5.Display.color565(201, 119, 92);
  const uint16_t ink = M5.Display.color565(22, 20, 18);
  const uint16_t eyew = M5.Display.color565(245, 243, 236);
  const uint16_t gray = M5.Display.color565(165, 165, 165);
  static const int8_t bobtab[8] = {0, -1, -2, -2, -1, 0, 1, 1};
  const int S = 14, ox = 48;
  const int oy = 42 + bobtab[frame & 7] + hop;

  // 体
  for (int r = 1; r <= 7; r++)
    for (int c = 1; c <= 12; c++) g_cv.fillRect(ox + c * S, oy + r * S, S, S, clay);

  // 脚（歩行: 上下交互）
  const int legCols[4] = {3, 6, 9, 12};
  for (int k = 0; k < 4; k++) {
    int len = ((k + frame) & 1) ? 1 : 2;
    g_cv.fillRect(ox + legCols[k] * S, oy + 8 * S, S, len * S, clay);
  }

  // しっぽ
  int wag = frame & 1;
  if (expr == "happy")
    g_cv.fillRect(ox + 13 * S, oy + (0 + wag) * S, S, S, gray);
  else if (expr == "worried")
    g_cv.fillRect(ox + 13 * S, oy + 8 * S, S, S, gray);
  else
    g_cv.fillRect(ox + 13 * S, oy + (4 + wag) * S, S, S, gray);

  // 目
  const int eyeY = oy + 3 * S + S / 2;
  const int eyeXs[2] = {ox + 4 * S + S / 2, ox + 9 * S + S / 2};
  for (int e = 0; e < 2; e++) {
    int ex = eyeXs[e];
    if (blink) {
      g_cv.fillRect(ex - 7, eyeY - 1, 14, 3, ink);
    } else {
      int rw = (expr == "surprised") ? 9 : 7;
      int rp = (expr == "surprised") ? 5 : 4;
      g_cv.fillCircle(ex, eyeY, rw, eyew);
      g_cv.fillCircle(ex + lookDx, eyeY, rp, ink);
    }
  }
  if (expr == "worried") {
    g_cv.drawLine(eyeXs[0] - 9, eyeY - 14, eyeXs[0] + 5, eyeY - 9, ink);
    g_cv.drawLine(eyeXs[1] + 9, eyeY - 14, eyeXs[1] - 5, eyeY - 9, ink);
  }

  // 口
  const int cx = 146;
  const int my = oy + 5 * S + 7;
  if (expr == "happy") {
    g_cv.fillRect(cx - 12, my, 24, 3, ink);
    g_cv.fillRect(cx - 14, my - 4, 4, 4, ink);
    g_cv.fillRect(cx + 10, my - 4, 4, 4, ink);
  } else if (expr == "worried") {
    g_cv.fillRect(cx - 12, my - 4, 24, 3, ink);
    g_cv.fillRect(cx - 14, my, 4, 4, ink);
    g_cv.fillRect(cx + 10, my, 4, 4, ink);
  } else if (expr == "surprised") {
    g_cv.fillCircle(cx, my, 6, ink);
  } else {
    g_cv.fillRect(cx - 12, my - 2, 24, 4, ink);
  }
}

static void drawTextBoxInto(const String& text) {
  g_cv.fillRect(0, 200, 320, 40, TFT_WHITE);
  g_cv.setTextColor(TFT_BLACK, TFT_WHITE);
  g_cv.setTextDatum(middle_center);
  g_cv.setTextSize(1);
  g_cv.drawString(text, 160, 220);
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

void tickFace(const String& expr, const String& text) {
  if (!g_cvReady) return;
  uint32_t now = millis();
  if (now - g_lastMs < 90) return;  // ~11fps
  g_lastMs = now;
  g_frame++;

  // まばたき
  if (now > g_nextBlink) {
    g_blinkEnd = now + 140;
    g_nextBlink = now + 2200 + (uint32_t)random(3200);
  }
  bool blink = now < g_blinkEnd;

  // ジャンプ（happyは常時弾む / それ以外はたまに）
  int hop = 0;
  if (expr == "happy") {
    static const int8_t hb[4] = {0, -9, -3, 0};
    hop = hb[g_frame & 3];
  } else {
    if (now > g_nextHop) {
      g_hopEnd = now + 360;
      g_nextHop = now + 5000 + (uint32_t)random(6000);
    }
    int rem = (int)(g_hopEnd - now);
    if (rem > 0 && rem <= 360) {
      int prog = 360 - rem;
      if (prog < 120) hop = -prog / 12;
      else if (prog < 240) hop = -10;
      else hop = -(360 - prog) / 12;
    }
  }

  // 視線（たまにキョロッと）
  if (now > g_nextLook) {
    g_lookEnd = now + 850;
    g_nextLook = now + 3800 + (uint32_t)random(4500);
    g_lookDir = (random(2) ? 1 : -1);
  }
  int lookDx = (now < g_lookEnd) ? g_lookDir * 4 : 0;

  g_cv.fillScreen(TFT_BLACK);
  if (g_pngFrames > 0) renderPngFrame(g_frame);
  else renderCreature(expr, g_frame, hop, blink, lookDx);
  drawTextBoxInto(text);
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
