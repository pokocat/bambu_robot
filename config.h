#ifndef CONFIG_H
#define CONFIG_H

/* ------------------------------------------------------------------
 *                Wi-Fi & MQTT credentials
 * ------------------------------------------------------------------*/
static const char* WIFI_SSID     = "Cony's";
static const char* WIFI_PASSWORD = "juneismeinv";

static const char* MQTT_SERVER   = "192.168.31.84";
static const char* MQTT_USER     = "mqtt";
static const char* MQTT_PASS     = "mqtt";

/* ------------------------------------------------------------------
 *                GC9A01 240×240 Round LCD Pinout
 * ------------------------------------------------------ ------------*/
#define TFT_SCK    4   // SPI SCK  → GPIO4
#define TFT_MOSI   5   // SPI MOSI → GPIO5
#define TFT_CS     6   // Chip-Select
#define TFT_DC     7   // Data/Command
#define TFT_RST    8   // Reset
#define TFT_BL     0   // Back-Light (高电平点亮)

/* ------------------------------------------------------------------
 *                DHT11 Temperature & Humidity Sensor
 * ------------------------------------------------------------------*/
#define DHTPIN     3   // 信号线
#define DHTTYPE    DHT11

#endif  /* CONFIG_H */
