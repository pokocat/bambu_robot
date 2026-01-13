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
2.  Modify `config.h` to match your setup.
3.  Fill in your WiFi and MQTT details.


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

## ⚡️ Flashing Firmware

You can download the compiled `.bin` files from the **GitHub Releases** page or the **Actions** artifacts.

### Prerequisites

You need `esptool` installed. If you have Python installed, run:
```bash
pip install esptool
```

### Flashing Command (ESP32-C3)

To flash the firmware (App + Partitions + Bootloader) to a **ESP32-C3** board, connect your device via USB and run:

```bash
# Replace /dev/ttyUSB0 with your device port (e.g., COM3 on Windows, /dev/cu.usbserial... on macOS)
esptool.py --chip esp32c3 --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x0 bambu_robot.ino.bootloader.bin \
  0x8000 bambu_robot.ino.partitions.bin \
  0x10000 bambu_robot.ino.bin
```

> ⚠️ **Important Note**: The firmware built by GitHub Actions uses a **dummy `config.h`** (with fake WiFi credentials). 
> To make it work, you must either:
> 1.  Compile and flash locally with your real `config.h`.
> 2.  Or wait for future updates where we implement Web Config (WiFiManager).

## 🔄 CI/CD

This project includes a GitHub Actions workflow (`.github/workflows/arduino_build.yml`) that automatically compiles the code on every push to ensure build integrity.

## 📝 License

MIT License.
