#include "display.h"
#include <M5Unified.h>
#include <LittleFS.h>
#include <string.h>

static M5Canvas g_cv(&M5.Display);
static bool g_cvReady = false;
static bool g_pngMode = false;

static uint32_t g_lastMs = 0;
static int g_frame = 0;
static uint32_t g_nextBlink = 0, g_blinkEnd = 0, g_nextHop = 0, g_hopEnd = 0, g_nextLook = 0, g_lookEnd = 0;
static int g_lookDir = 0;

// 13クリップ（prefix）とフレーム数
static const char* PFX[] = {"breathe", "blink", "look", "coding", "think", "bounce",
                            "sway", "djmix", "bouncedj", "swaydj", "sleep", "surprise", "wink"};
static const int NPFX = 13;
static int g_cnt[NPFX] = {0};

static int countFrames(const char* prefix) {
  int n = 0;
  for (int i = 0; i < 64; i++) {
    if (LittleFS.exists(String("/") + prefix + i + ".png")) n++;
    else break;
  }
  return n;
}
static int countOf(const char* p) {
  for (int i = 0; i < NPFX; i++) if (strcmp(PFX[i], p) == 0) return g_cnt[i];
  return 0;
}

// プール定義
static const char* POOL_IDLE[] = {"breathe", "blink", "look"};
static const char* POOL_WORK[] = {"coding", "think"};
static const char* POOL_DONE[] = {"bounce", "sway", "djmix", "bouncedj", "swaydj"};

static String g_activePrefix = "look";
static int g_activeFrames = 0;
static String g_lastExpr = "";
static uint32_t g_rotateAt = 0;
static uint32_t g_forcedUntil = 0;

static bool isBase(const String& e) { return e == "idle" || e == "normal" || e == "working"; }

static void selectClip(const String& expr) {
  const char** pool;
  int pn;
  static const char* W[] = {"sleep"};
  static const char* S[] = {"surprise"};
  static const char* K[] = {"wink"};
  if (expr == "working") { pool = POOL_WORK; pn = 2; }
  else if (expr == "happy" || expr == "done") { pool = POOL_DONE; pn = 5; }
  else if (expr == "worried") { pool = W; pn = 1; }
  else if (expr == "surprised") { pool = S; pn = 1; }
  else if (expr == "wink") { pool = K; pn = 1; }
  else { pool = POOL_IDLE; pn = 3; }

  const char* avail[5];
  int k = 0;
  for (int i = 0; i < pn; i++) if (countOf(pool[i]) > 0) avail[k++] = pool[i];
  if (k > 0) {
    const char* p = avail[random(k)];
    g_activePrefix = p;
    g_activeFrames = countOf(p);
  } else {
    g_activePrefix = "look";
    g_activeFrames = countOf("look");
  }
}

static void renderPngFrame(int frame) {
  if (g_activeFrames <= 0) return;
  int idx = (frame / 2) % g_activeFrames;
  String path = "/" + g_activePrefix + String(idx) + ".png";
  File f = LittleFS.open(path, "r");
  if (!f) return;
  size_t sz = f.size();
  uint8_t* buf = (uint8_t*)malloc(sz);
  if (buf) { f.read(buf, sz); f.close(); g_cv.drawPng(buf, sz, 60, 0); free(buf); }
  else f.close();
}

// PNGが無い時のコード描画フォールバック
static void renderCreature(const String& expr, int frame, int hop, bool blink, int lookDx) {
  const uint16_t clay = M5.Display.color565(201, 119, 92);
  const uint16_t ink = M5.Display.color565(22, 20, 18);
  const uint16_t eyew = M5.Display.color565(245, 243, 236);
  const uint16_t gray = M5.Display.color565(165, 165, 165);
  static const int8_t bobtab[8] = {0, -1, -2, -2, -1, 0, 1, 1};
  const int S = 14, ox = 48;
  const int oy = 42 + bobtab[frame & 7] + hop;
  for (int r = 1; r <= 7; r++)
    for (int c = 1; c <= 12; c++) g_cv.fillRect(ox + c * S, oy + r * S, S, S, clay);
  const int legCols[4] = {3, 6, 9, 12};
  for (int kk = 0; kk < 4; kk++) {
    int len = ((kk + frame) & 1) ? 1 : 2;
    g_cv.fillRect(ox + legCols[kk] * S, oy + 8 * S, S, len * S, clay);
  }
  const int eyeY = oy + 3 * S + S / 2;
  const int eyeXs[2] = {ox + 4 * S + S / 2, ox + 9 * S + S / 2};
  for (int e = 0; e < 2; e++) {
    int ex = eyeXs[e];
    if (blink) g_cv.fillRect(ex - 7, eyeY - 1, 14, 3, ink);
    else { g_cv.fillCircle(ex, eyeY, 7, eyew); g_cv.fillCircle(ex + lookDx, eyeY, 4, ink); }
  }
  g_cv.fillRect(146 - 12, oy + 5 * S + 5, 24, 4, ink);
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
  M5.Display.fillScreen(M5.Display.color565(15, 15, 15));
  g_cv.setColorDepth(16);
  g_cvReady = (g_cv.createSprite(320, 240) != nullptr);
  g_cv.setFont(&fonts::lgfxJapanGothic_20);
  for (int i = 0; i < NPFX; i++) g_cnt[i] = countFrames(PFX[i]);
  g_pngMode = (countOf("look") > 0);
}

void tickFace(const String& expr, const String& text) {
  if (!g_cvReady) return;
  uint32_t now = millis();
  if (now - g_lastMs < 90) return;
  g_lastMs = now;
  g_frame++;

  if (g_pngMode && now < g_forcedUntil) {
    // /clip による強制表示中：選択ロジックをスキップ
  } else {
    if (g_forcedUntil != 0 && now >= g_forcedUntil) { g_forcedUntil = 0; g_lastExpr = ""; }
    if (expr != g_lastExpr) {
      g_lastExpr = expr;
      if (g_pngMode) selectClip(expr);
      g_frame = 0;
      g_rotateAt = now + 5000 + (uint32_t)random(4000);
    } else if (g_pngMode && isBase(expr) && now > g_rotateAt) {
      selectClip(expr);  // ベース状態は数秒ごとにプール内で切替（飽き防止）
      g_frame = 0;
      g_rotateAt = now + 5000 + (uint32_t)random(4000);
    }
  }

  g_cv.fillScreen(M5.Display.color565(15, 15, 15));
  if (g_pngMode) {
    renderPngFrame(g_frame);
  } else {
    if (now > g_nextBlink) { g_blinkEnd = now + 140; g_nextBlink = now + 2200 + (uint32_t)random(3200); }
    bool blink = now < g_blinkEnd;
    int hop = 0;
    if (now > g_nextHop) { g_hopEnd = now + 360; g_nextHop = now + 5000 + (uint32_t)random(6000); }
    int rem = (int)(g_hopEnd - now);
    if (rem > 0 && rem <= 360) { int prog = 360 - rem; hop = prog < 120 ? -prog / 12 : prog < 240 ? -10 : -(360 - prog) / 12; }
    if (now > g_nextLook) { g_lookEnd = now + 850; g_nextLook = now + 3800 + (uint32_t)random(4500); g_lookDir = (random(2) ? 1 : -1); }
    int lookDx = (now < g_lookEnd) ? g_lookDir * 4 : 0;
    renderCreature(expr, g_frame, hop, blink, lookDx);
  }
  drawTextBoxInto(text);
  g_cv.pushSprite(0, 0);
}

void displayForceClip(const String& prefix, uint32_t ms) {
  g_activePrefix = prefix;
  g_activeFrames = countOf(prefix.c_str());
  g_frame = 0;
  g_forcedUntil = millis() + ms;
}

String displayDebug() {
  String s = String("png=") + (g_pngMode ? 1 : 0) + " active=" + g_activePrefix + " af=" + g_activeFrames + " cnt=";
  for (int i = 0; i < NPFX; i++) s += String(PFX[i]) + ":" + g_cnt[i] + (i < NPFX - 1 ? "," : "");
  return s;
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
