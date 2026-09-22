# esp32-cam-dslr-style-camera
A pocket-sized DSLR-style camera built on ESP32-S3-CAM with a menu-driven UI, live preview, and internal-flash photo gallery.

# 📷 ESP32-S3 Smart Camera

A compact menu-driven Smart Camera built using an ESP32-S3-CAM, ST7789 TFT display, MicroSD card and two push buttons.

The project features a live camera preview, photo capture, photo gallery and photo deletion system with a simple graphical user interface.

---

## ✨ Features

- 📷 Live camera preview
- 📸 Capture photos
- 🖼️ Built-in photo gallery
- 💾 Save photos to MicroSD card
- 🗑️ Delete photos using long press
- ✅ Delete confirmation screen
- 📱 Menu-driven user interface
- ℹ️ About screen
- 🎮 Two-button control
- 🔄 Button debounce for reliable operation

---

## 🛠️ Hardware

- ESP32-S3-CAM N16R8
- **Flash: 16MB (128Mb)**
- **PSRAM: Enabled**
- 240×240 ST7789 TFT Display
- Camera module
- MicroSD card
- 2 × Push buttons

---

## ⚙️ ESP32-S3 Configuration

- **Board:** ESP32-S3-CAM N16R8
- **Flash:** 16MB (128Mb)
- **PSRAM:** Enabled
- **Display:** 240×240 ST7789
- **Camera:** ESP32-S3 Camera Module
- **Storage:** MicroSD Card
- **Development:** Arduino IDE

---

## 🔌 Connections

### ST7789 Display

| Display Pin | ESP32-S3 |
|---|---|
| SCK | GPIO 21 |
| MOSI | GPIO 47 |
| RST | GPIO 45 |
| DC | GPIO 48 |
| VCC | 3.3V |
| GND | GND |

### Buttons

| Button | ESP32-S3 |
|---|---|
| CAPTURE | GPIO 1 |
| NEXT | GPIO 2 |

Buttons are connected between the GPIO pin and GND.

---

## 📸 Project Photos

### Breadboard Setup

![Breadboard Setup](images/breadboard-setup.jpg)

### Smart Camera

![Smart Camera](images/smart-camera-front.jpg)

### Menu

![Menu](images/display-menu.jpg)

### Camera Preview

![Camera Preview](images/camera-preview.jpg)

### Gallery

![Gallery](images/gallery.jpg)

### About Screen

![About Screen](images/about-screen.jpg)

---

## 📚 Libraries

- Arduino_GFX_Library
- TJpg_Decoder
- ESP32 Camera
- SD_MMC

---

## 🎮 Controls

### Menu

- **NEXT** → Move to the next option
- **CAPTURE** → Select

### Camera

- **CAPTURE** → Take photo
- **NEXT** → Open Gallery

### Gallery

- **NEXT** → Next photo
- **Short press CAPTURE** → Back to menu
- **Hold CAPTURE** → Delete confirmation

### Delete Confirmation

- **NEXT** → No / Cancel
- **CAPTURE** → Yes / Delete

---

## 💾 Photo Storage

Captured photos are saved to the MicroSD card inside:

```text
/photos/
