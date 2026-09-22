/*
  ESP32-S3-CAM (N16R8) - Menu-driven Smart Camera v3  (INTERNAL FLASH storage version)
  - Boot -> styled MENU: Camera | Gallery | About
  - 2 buttons, context-based:
      NEXT (GPIO2)    -> move/next
      CAPTURE (GPIO0) -> select/action (tap) | in Gallery: HOLD 1.2s -> delete confirm
  - Photos ESP32 ki internal flash (FFat) me save hoti hain, SD card ki zaroorat nahi

  ===== LIBRARIES =====
  1) Arduino_GFX_Library   (moononournation)
  2) TJpg_Decoder          (Bodmer)

  ===== ARDUINO IDE SETTINGS (zaroori!) =====
  Board            : ESP32S3 Dev Module
  Flash Size       : 16MB (128Mb)
  Partition Scheme : 16M Flash (3MB APP/9.9MB FATFS)   <-- ye FFat storage ke liye zaroori hai
  PSRAM            : OPI PSRAM

  ===== WIRING =====
  Display: GPIO21->SCK, GPIO47->MOSI, GPIO45->RST, GPIO48->DC, 3.3V->VCC+BLK, GND->GND
  CAPTURE: GPIO0 -> button -> GND
  NEXT:    GPIO2 -> button -> GND
*/

#include "esp_camera.h"
#include "FS.h"
#include "FFat.h"
#include <Arduino_GFX_Library.h>
#include <TJpg_Decoder.h>

// ===== Camera pins =====
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      15
#define SIOD_GPIO_NUM       4
#define SIOC_GPIO_NUM       5
#define Y9_GPIO_NUM         16
#define Y8_GPIO_NUM         17
#define Y7_GPIO_NUM         18
#define Y6_GPIO_NUM         12
#define Y5_GPIO_NUM         10
#define Y4_GPIO_NUM          8
#define Y3_GPIO_NUM          9
#define Y2_GPIO_NUM         11
#define VSYNC_GPIO_NUM       6
#define HREF_GPIO_NUM        7
#define PCLK_GPIO_NUM       13

// ===== Display pins =====
#define TFT_SCK   21
#define TFT_MOSI  47
#define TFT_RST   45
#define TFT_DC    48

// ===== Buttons =====
#define CAPTURE_BUTTON_PIN  0
#define NEXT_BUTTON_PIN     2
#define LONG_PRESS_MS     1200
#define HINT_SHOW_MS       500
#define DEBOUNCE_MS        200

// ===== Colors (RGB565) =====
#define BLACK   0x0000
#define WHITE   0xFFFF
#define GRAY    0x8410
#define DKGRAY  0x39C7
#define TEAL    0x0451
#define ORANGE  0xFD20
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, GFX_NOT_DEFINED, TFT_SCK, TFT_MOSI, GFX_NOT_DEFINED);
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 0 /* default - Menu/Gallery/About use this */, true, 240, 240);

// ===== State machine =====
enum Mode { MENU, CAMERA, GALLERY, ABOUT, DELETE_CONFIRM };
Mode mode = MENU;
Mode modeBeforeConfirm = GALLERY;

const char *menuItems[] = {"Camera", "Gallery", "About"};
const int menuCount = 3;
int menuSelect = 0;

#define MAX_GALLERY 300
int galleryNumbers[MAX_GALLERY];
int galleryFileCount = 0;
int galleryIndex = -1;
int nextPhotoNumber = 0;
int savedSinceScan = 0;      // last scan ke baad kitni photos save hui
bool storageOK = false;      // FFat mount hua ya nahi

bool lastCapState = HIGH, lastNextState = HIGH;
unsigned long capPressStart = 0, lastNextDebounce = 0;
bool longFired = false, hintShown = false;

// ----- Proper debounce state (fixes intermittent button response) -----
bool stableCapState = HIGH, rawCapState = HIGH;
unsigned long capDebounceTimer = 0;
bool stableNextState = HIGH, rawNextState = HIGH;
unsigned long nextDebounceTimer = 0;
const unsigned long DEBOUNCE_STABLE_MS = 40;

bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
  return true;
}

// ===================== Icons =====================

void drawCameraIcon(int x, int y) {
  gfx->fillRoundRect(x, y + 4, 28, 18, 3, WHITE);
  gfx->fillRoundRect(x + 8, y, 12, 6, 2, WHITE);
  gfx->fillCircle(x + 14, y + 13, 6, TEAL);
  gfx->fillCircle(x + 14, y + 13, 3, WHITE);
}

void drawGalleryIcon(int x, int y) {
  gfx->drawRoundRect(x + 4, y, 20, 16, 2, WHITE);
  gfx->fillRoundRect(x, y + 5, 20, 16, 2, WHITE);
  gfx->drawLine(x + 3, y + 16, x + 8, y + 10, TEAL);
  gfx->drawLine(x + 8, y + 10, x + 13, y + 15, TEAL);
  gfx->fillCircle(x + 15, y + 9, 2, TEAL);
}

void drawAboutIcon(int x, int y) {
  gfx->drawCircle(x + 12, y + 10, 11, WHITE);
  gfx->fillCircle(x + 12, y + 5, 1, WHITE);
  gfx->fillRect(x + 11, y + 9, 2, 8, WHITE);
}

void drawHeader(const char *title) {
  gfx->fillRect(0, 0, 240, 40, TEAL);
  gfx->drawFastHLine(0, 40, 240, ORANGE);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  int textW = strlen(title) * 12;
  gfx->setCursor((240 - textW) / 2, 12);
  gfx->print(title);
}

// ===================== Screens =====================

void drawMenu() {
  gfx->fillScreen(BLACK);
  drawHeader("SMART CAM");

  int y = 58;
  for (int i = 0; i < menuCount; i++) {
    bool sel = (i == menuSelect);
    uint16_t fg = sel ? BLACK : WHITE;

    if (sel) gfx->fillRoundRect(16, y, 208, 40, 10, ORANGE);
    else     gfx->drawRoundRect(16, y, 208, 40, 10, DKGRAY);

    if (i == 0) drawCameraIcon(28, y + 8);
    else if (i == 1) drawGalleryIcon(28, y + 8);
    else drawAboutIcon(28, y + 8);

    gfx->setTextColor(fg);
    gfx->setTextSize(2);
    gfx->setCursor(68, y + 12);
    gfx->print(menuItems[i]);

    y += 50;
  }

  gfx->fillRect(0, 210, 240, 30, DKGRAY);
  gfx->fillTriangle(20, 220, 20, 230, 28, 225, CYAN);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(36, 220);
  gfx->print("NEXT");
  gfx->fillCircle(150, 225, 5, GREEN);
  gfx->setCursor(162, 220);
  gfx->print("SELECT");
}

void drawAbout() {
  gfx->fillScreen(BLACK);
  drawHeader("ABOUT");

  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(20, 70);
  gfx->print("ESP32-S3 Smart Camera");
  gfx->setCursor(20, 90);
  gfx->setTextColor(CYAN);
  gfx->printf("Photos saved: %d", galleryFileCount);
  gfx->setTextColor(GRAY);
  gfx->setCursor(20, 120);
  gfx->print("Arduino_GFX + TJpg_Decoder");
  gfx->setCursor(20, 140);
  gfx->print("Internal flash storage (FFat)");
  gfx->setCursor(20, 160);
  if (storageOK) gfx->printf("Free: %u KB", (unsigned)(FFat.freeBytes() / 1024));
  else           gfx->print("Storage: NOT MOUNTED");

  gfx->fillRect(0, 210, 240, 30, DKGRAY);
  gfx->fillCircle(30, 225, 5, GREEN);
  gfx->setTextColor(WHITE);
  gfx->setCursor(42, 220);
  gfx->print("SELECT: back to menu");
}

void drawEmptyGallery() {
  gfx->fillScreen(BLACK);
  drawHeader("GALLERY");
  gfx->drawRoundRect(70, 80, 100, 70, 8, GRAY);
  gfx->drawLine(75, 140, 110, 105, GRAY);
  gfx->drawLine(110, 105, 135, 130, GRAY);
  gfx->fillCircle(150, 95, 6, GRAY);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(45, 165);
  gfx->print("No photos yet");
  gfx->fillRect(0, 210, 240, 30, DKGRAY);
  gfx->fillCircle(30, 225, 5, GREEN);
  gfx->setCursor(42, 220);
  gfx->print("SELECT: back to menu");
}

void showSavedPhoto(int index) {
  if (galleryFileCount == 0) { drawEmptyGallery(); return; }

  String path = "/photos/img_" + String(galleryNumbers[index]) + ".jpg";
  File f = FFat.open(path.c_str());
  if (!f) { Serial.println("Gallery: open FAILED - " + path); return; }
  size_t sz = f.size();
  uint8_t *buf = (uint8_t *)ps_malloc(sz);
  if (!buf) { Serial.println("Gallery: malloc FAILED"); f.close(); return; }
  f.read(buf, sz);
  f.close();

  gfx->fillScreen(BLACK);
  TJpgDec.drawJpg(0, 0, buf, sz);
  free(buf);

  gfx->fillRect(0, 208, 240, 32, DKGRAY);
  gfx->fillTriangle(8, 218, 8, 228, 16, 223, CYAN);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(24, 212);
  gfx->printf("Photo %d / %d", index + 1, galleryFileCount);
  gfx->setTextColor(GRAY);
  gfx->setCursor(24, 224);
  gfx->print("Hold SELECT to delete");
}

void drawHoldHint() {
  gfx->fillRect(0, 208, 240, 32, RED);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(30, 218);
  gfx->print("Keep holding to delete...");
}

void drawDeleteConfirm() {
  gfx->fillRect(0, 150, 240, 90, BLACK);
  gfx->fillRoundRect(10, 155, 220, 80, 8, RED);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(30, 168);
  gfx->print("Delete this photo?");
  gfx->fillTriangle(30, 200, 30, 210, 38, 205, CYAN);
  gfx->setCursor(46, 200);
  gfx->print("NEXT: No");
  gfx->fillCircle(160, 205, 5, GREEN);
  gfx->setCursor(172, 200);
  gfx->print("Yes");
}

// ===================== Storage helpers =====================

void scanGalleryList() {
  galleryFileCount = 0;
  savedSinceScan = 0;
  if (!storageOK) return;
  int maxSeen = -1;
  File root = FFat.open("/photos");
  if (!root) return;
  File f = root.openNextFile();
  while (f && galleryFileCount < MAX_GALLERY) {
    String name = String(f.name());
    int us = name.lastIndexOf("img_");
    int dot = name.lastIndexOf(".jpg");
    if (us >= 0 && dot > us) {
      int num = name.substring(us + 4, dot).toInt();
      galleryNumbers[galleryFileCount++] = num;
      if (num > maxSeen) maxSeen = num;
    }
    f.close();
    f = root.openNextFile();
  }
  root.close();
  for (int i = 1; i < galleryFileCount; i++) {
    int key = galleryNumbers[i], j = i - 1;
    while (j >= 0 && galleryNumbers[j] > key) { galleryNumbers[j + 1] = galleryNumbers[j]; j--; }
    galleryNumbers[j + 1] = key;
  }
  if (nextPhotoNumber <= maxSeen) nextPhotoNumber = maxSeen + 1;
  Serial.printf("Gallery scan: %d photos, next number %d\n", galleryFileCount, nextPhotoNumber);
}

void deleteCurrentPhoto() {
  if (galleryFileCount == 0 || galleryIndex < 0) return;
  String path = "/photos/img_" + String(galleryNumbers[galleryIndex]) + ".jpg";
  FFat.remove(path.c_str());
  Serial.println("Deleted: " + path);
  scanGalleryList();
  if (galleryFileCount == 0) galleryIndex = -1;
  else if (galleryIndex >= galleryFileCount) galleryIndex = galleryFileCount - 1;
}

void takePhotoFreeze() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) { Serial.println("Capture FAILED"); return; }

  if (!storageOK) {
    Serial.println("Storage not mounted - photo NOT saved");
  } else if (galleryFileCount + savedSinceScan >= MAX_GALLERY) {
    Serial.println("Gallery FULL (MAX_GALLERY) - photo NOT saved");
  } else if (FFat.freeBytes() < fb->len + 8192) {
    Serial.println("Storage FULL - photo NOT saved");
  } else {
    String path = "/photos/img_" + String(nextPhotoNumber) + ".jpg";
    File file = FFat.open(path.c_str(), FILE_WRITE);
    if (file) {
      size_t w = file.write(fb->buf, fb->len);
      file.close();
      Serial.printf("Saved %s (%u / %u bytes)\n", path.c_str(), (unsigned)w, (unsigned)fb->len);
      if (w == fb->len) {
        nextPhotoNumber++;
        savedSinceScan++;
      } else {
        FFat.remove(path.c_str());   // adhoori file hata do
        Serial.println("Incomplete write - file removed");
      }
    } else {
      Serial.println("File open FAILED: " + path);
    }
  }

  gfx->fillScreen(BLACK);
  TJpgDec.drawJpg(0, 0, fb->buf, fb->len);
  esp_camera_fb_return(fb);
  delay(2000);
}

// ===================== Mode transitions =====================

void enterMode(Mode m) {
  mode = m;
  gfx->setRotation(mode == CAMERA ? 1 : 0);
  switch (mode) {
    case CAMERA: gfx->fillScreen(BLACK); break;
    case GALLERY:
      scanGalleryList();
      galleryIndex = galleryFileCount > 0 ? 0 : -1;
      showSavedPhoto(galleryIndex);
      break;
    case ABOUT: scanGalleryList(); drawAbout(); break;
    case MENU: drawMenu(); break;
    case DELETE_CONFIRM: break;
  }
}

void handleCaptureShortPress() {
  switch (mode) {
    case MENU:
      if (menuSelect == 0) enterMode(CAMERA);
      else if (menuSelect == 1) enterMode(GALLERY);
      else enterMode(ABOUT);
      break;
    case CAMERA:
      takePhotoFreeze();
      gfx->fillScreen(BLACK);
      break;
    case GALLERY:
    case ABOUT:
      enterMode(MENU);
      break;
    case DELETE_CONFIRM:
      deleteCurrentPhoto();
      enterMode(GALLERY);
      break;
  }
}

// ===================== Setup =====================

void setup() {
  Serial.begin(115200);
  pinMode(CAPTURE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);

  gfx->begin();
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(20, 100);
  gfx->print("Starting...");

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(tftOutput);

  camera_config_t config = {};   // zero-init (garbage fields se bachne ke liye)
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_240X240;
  config.jpeg_quality = 12;
  config.fb_count = psramFound() ? 2 : 1;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) Serial.printf("Camera init FAILED 0x%x (%s)\n", err, esp_err_to_name(err));
  else Serial.println("Camera init OK");

  // ===== Internal flash storage (FFat) =====
  // true = pehli baar (ya mount fail hone par) automatically format karega, isme kuch second lagte hain
  storageOK = FFat.begin(true);
  if (!storageOK) {
    Serial.println("FFat mount FAILED -> Tools > Partition Scheme = '16M Flash (3MB APP/9.9MB FATFS)' karo");
  } else {
    Serial.printf("FFat OK: total %u KB, free %u KB\n",
                  (unsigned)(FFat.totalBytes() / 1024), (unsigned)(FFat.freeBytes() / 1024));
    if (!FFat.exists("/photos")) FFat.mkdir("/photos");
    scanGalleryList();
  }

  drawMenu();
}

// ===================== Loop =====================

void loop() {
  unsigned long now = millis();

  // ===== Debounce filter for CAPTURE button =====
  bool rawCap = digitalRead(CAPTURE_BUTTON_PIN);
  if (rawCap != rawCapState) {
    capDebounceTimer = now;
    rawCapState = rawCap;
  }
  bool capChanged = false;
  if ((now - capDebounceTimer) > DEBOUNCE_STABLE_MS && rawCapState != stableCapState) {
    stableCapState = rawCapState;
    capChanged = true;
  }

  // ===== Debounce filter for NEXT button =====
  bool rawNext = digitalRead(NEXT_BUTTON_PIN);
  if (rawNext != rawNextState) {
    nextDebounceTimer = now;
    rawNextState = rawNext;
  }
  bool nextChanged = false;
  if ((now - nextDebounceTimer) > DEBOUNCE_STABLE_MS && rawNextState != stableNextState) {
    stableNextState = rawNextState;
    nextChanged = true;
  }

  // ----- CAPTURE button: press/hold/release (using debounced state) -----
  if (stableCapState == LOW) {
    if (capChanged) {
      // just pressed (debounced)
      capPressStart = now;
      longFired = false;
      hintShown = false;
    } else {
      unsigned long held = now - capPressStart;
      if (mode == GALLERY && galleryFileCount > 0) {
        if (!hintShown && held > HINT_SHOW_MS) { hintShown = true; drawHoldHint(); }
        if (!longFired && held > LONG_PRESS_MS) {
          longFired = true;
          modeBeforeConfirm = mode;
          mode = DELETE_CONFIRM;
          drawDeleteConfirm();
        }
      }
    }
  } else {
    if (capChanged) {
      // just released (debounced)
      if (!longFired) handleCaptureShortPress();
      longFired = false;
    }
  }

  // ----- NEXT button (using debounced state) -----
  if (stableNextState == LOW && nextChanged) {
    switch (mode) {
      case MENU:
        menuSelect = (menuSelect + 1) % menuCount;
        drawMenu();
        break;
      case CAMERA:
        enterMode(GALLERY);
        break;
      case GALLERY:
        if (galleryFileCount > 0) {
          galleryIndex = (galleryIndex + 1) % galleryFileCount;
          showSavedPhoto(galleryIndex);
        }
        break;
      case ABOUT:
        break;
      case DELETE_CONFIRM:
        mode = GALLERY;
        showSavedPhoto(galleryIndex);
        break;
    }
  }

  // ----- Live streaming (CAMERA mode only) -----
  if (mode == CAMERA) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      TJpgDec.drawJpg(0, 0, fb->buf, fb->len);
      esp_camera_fb_return(fb);
    }
  }
}
