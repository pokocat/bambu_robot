[![Arduino Build](https://github.com/pokocat/bambu_robot/actions/workflows/arduino_build.yml/badge.svg?branch=main)](https://github.com/pokocat/bambu_robot/actions/workflows/arduino_build.yml)

# Bambu Lab Printer Monitor (ESP32-C3 Round Display)

This project is a dedicated hardware monitor for Bambu Lab 3D printers, built with an **ESP32-C3** microcontroller and a **1.28" GC9A01 Round LCD**. It displays real-time printing status, progress, temperatures, and remaining time in a sleek, circular interface.

## 📸 Features

- **Circular UI**: Optimized for 240x240 round displays.
- **Real-time Progress**: Visual ring progress bar and percentage.
- **Status Monitoring**: Displays current print stage (Printing, Bed Leveling, Homing, etc.) with smart text abbreviation.
- **Temperature Monitoring**:
  - Nozzle & Bed temperature (via MQTT).
  - Ambient Temperature & Humidity (via local DHT11 sensor).
- **Print Info**: Layer count, material type, and remaining time.
- **Auto-Recovery**: Automatic WiFi and MQTT reconnection handling.
- **Performance**: Optimized drawing algorithms to prevent screen flickering and ensure smooth updates.

## 🛠 Hardware Requirements

- **Microcontroller**: ESP32-C3 SuperMini (or generic ESP32-C3 dev board).
- **Display**: 1.28" GC9A01 Round LCD Module (SPI Interface).
- **Sensor**: DHT11 Temperature & Humidity Sensor (Optional).
- **Power**: USB-C or 3.3V/5V supply.

### Pin Configuration

| Component | Pin Name | ESP32-C3 GPIO | Notes |
|-----------|----------|---------------|-------|
| **LCD**   | SCL/SCK  | GPIO 4        | SPI Clock |
|           | SDA/MOSI | GPIO 5        | SPI Data |
|           | CS       | GPIO 6        | Chip Select |
|           | DC       | GPIO 7        | Data/Command |
|           | RES/RST  | GPIO 8        | Reset |
|           | BLK/BL   | GPIO 0        | Backlight |
| **DHT11** | DATA     | GPIO 3        | Signal |

> **Note**: Pin definitions can be modified in `config.h`.

## 📦 Software Dependencies

This project is built using the Arduino framework. You need to install the following libraries via the Arduino Library Manager:

1.  **GFX Library for Arduino** (by MoonOnOurNation) - Display driver.
2.  **PubSubClient** (by Nick O'Leary) - MQTT communication.
3.  **DHT sensor library** (by Adafruit) - For DHT11 sensor.
4.  **Adafruit Unified Sensor** (by Adafruit) - Dependency for DHT.

## ⚙️ Configuration

Since the configuration file contains sensitive WiFi and MQTT credentials, it is not included in the repository. You must create a `config.h` file in the project root.

1.  Copy the template below.
2.  Create a file named `config.h` next to `bambu_robot.ino`.
3.  Fill in your WiFi and MQTT details.

```cpp
// config.h template
#ifndef CONFIG_H
#define CONFIG_H

/* ------------------------------------------------------------------
 *                Wi-Fi & MQTT credentials
 * ------------------------------------------------------------------*/
static const char* WIFI_SSID     = "YOUR_WIFI_SSID";
static const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Note: If connecting directly to Bambu Printer, you might need an MQTT bridge 
// or SSL setup (port 8883). This project currently defaults to TCP port 1883.
static const char* MQTT_SERVER   = "192.168.1.xxx"; // Your MQTT Broker IP
static const char* MQTT_USER     = "bblp";          // Usually 'bblp' for Bambu
static const char* MQTT_PASS     = "access_code";   // Your Printer Access Code

/* ------------------------------------------------------------------
 *                Pin Definitions
 * ------------------------------------------------------------------*/
#define TFT_SCK    4
#define TFT_MOSI   5
#define TFT_CS     6
#define TFT_DC     7
#define TFT_RST    8
#define TFT_BL     0

#define DHTPIN     3
#define DHTTYPE    DHT11

#endif  /* CONFIG_H */
```

## 🚀 Installation

1.  **Clone the repository**:
    ```bash
    git clone https://github.com/yourusername/bambu_robot.git
    cd bambu_robot
    ```
2.  **Create `config.h`** as described above.
3.  **Open in IDE**:
    - **Arduino IDE**: Open `bambu_robot.ino`. Select board "ESP32C3 Dev Module".
    - **VS Code (PlatformIO/Arduino)**: Ensure your environment is set up for ESP32.
4.  **Install Libraries**: Use Library Manager to install dependencies.
5.  **Upload**: Connect your ESP32-C3 and click Upload.

## 🔄 CI/CD

This project includes a GitHub Actions workflow (`.github/workflows/arduino_build.yml`) that automatically compiles the code on every push to ensure build integrity.

## 📝 License

MIT License.
