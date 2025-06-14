#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <Arduino_GFX_Library.h>
#include <DHT.h>

// ==== 引脚定义 ====
#define TFT_SCK   4
#define TFT_MOSI  5
#define TFT_CS    6
#define TFT_DC    7
#define TFT_RST   8
#define TFT_BL    0

#define DHTPIN    3
#define DHTTYPE   DHT11

// ==== Display 初始化 ====
Arduino_DataBus *bus = new Arduino_SWSPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, -1);
Arduino_GFX     *gfx = new Arduino_GC9A01(bus, TFT_RST, 0, true);

DHT dht(DHTPIN, DHTTYPE);

// ==== MQTT 客户端 ====
WiFiClient   espClient;
PubSubClient client(espClient);

// ==== 状态变量及“上次”缓存 ====
String print_state       = "Idle", lastPrintState    = "";
String current_stage     = "",     lastStage         = "";
int    progress          = 0,      lastProgress      = -1;
String current_layer     = "0",    total_layer       = "0", lastLayerInfo = "";
String material          = "PLA",  lastMaterial      = "";
String time_left         = "00:00:00", lastTimeLeft  = "";
float  temperature       = 0,      humidity          = 0;
float  lastTemperature   = -999,   lastHumidity      = -999;

// ==== 刷新节流 ====
bool         dirty          = false;
unsigned long lastDrawTime  = 0;
const int    REFRESH_INTERVAL = 1000;   // ms
const int    MAX_MATERIAL_LEN = 12;

// ==== 工具函数 ====
// 截断过长字符串
String truncateMaterial(const String &s) {
  return s.length() <= MAX_MATERIAL_LEN
    ? s
    : s.substring(0, MAX_MATERIAL_LEN-1) + "…";
}

// 居中绘制默认 6×8 字体
void drawCenteredText(const String &txt, uint8_t size, int y) {
  gfx->setTextColor(BLACK);
  gfx->setTextSize(size);
  int w = txt.length() * 6 * size;
  int x = (240 - w) / 2;
  gfx->setCursor(x, y);
  gfx->print(txt);
}

// ==== 各区域局部更新 ====

// 1. 环形进度
void updateProgressArc(int newProg) {
  if (newProg == lastProgress) return;
  int cx=120, cy=120, r0=100, r1=110;
  // 擦旧
  int oldA = map(constrain(lastProgress,0,100),0,100,0,360);
  gfx->fillArc(cx, cy, r0, r1, 270, 270+oldA, LIGHTGREY);
  // 画新
  int newA = map(constrain(newProg,0,100),0,100,0,360);
  gfx->fillArc(cx, cy, r0, r1, 270, 270+newA, GREEN);
  lastProgress = newProg;
}

// 2. 百分比文字
void updatePercentText(int newProg) {
  static const int y=60, size=2;
  String t = String(newProg) + "%";
  if (newProg == lastProgress) return;
  int w=t.length()*6*size, h=8*size, x=(240-w)/2;
  gfx->fillRect(x, y, w, h, WHITE);
  drawCenteredText(t, size, y);
}

// 3. print_state
void updatePrintState(const String &st) {
  static const int y=78, size=1;
  String s=truncateMaterial(st);
  if (s==lastPrintState) return;
  int w=s.length()*6*size, h=8*size, x=(240-w)/2;
  gfx->fillRect(x, y, w, h, WHITE);
  drawCenteredText(s, size, y);
  lastPrintState = s;
}

// 4. 层数进度
void updateLayerInfo(const String &li) {
  static const int y=90, size=1;
  if (li==lastLayerInfo) return;
  int w=li.length()*6*size, h=8*size, x=(240-w)/2;
  gfx->fillRect(x, y, w, h, WHITE);
  drawCenteredText(li, size, y);
  lastLayerInfo = li;
}

// 5. current_stage
void updateStage(const String &st) {
  static const int y=110, size=2;
  String s=truncateMaterial(st);
  if (s==lastStage) return;
  int w=s.length()*6*size, h=8*size, x=(240-w)/2;
  gfx->fillRect(x, y, w, h, WHITE);
  drawCenteredText(s, size, y);
  lastStage = s;
}

// 6. 材料
void updateMaterial(const String &mat) {
  static const int y=138, size=1;
  String s="Mat:"+truncateMaterial(mat);
  if (s==lastMaterial) return;
  int w=s.length()*6*size, h=8*size, x=(240-w)/2;
  gfx->fillRect(x, y, w, h, WHITE);
  drawCenteredText(s, size, y);
  lastMaterial = s;
}

// 7. 剩余时间
void updateTimeLeft(const String &tl) {
  static const int y=150, size=1;
  if (tl==lastTimeLeft) return;
  String s="Tleft:"+tl;
  int w=s.length()*6*size, h=8*size, x=(240-w)/2;
  gfx->fillRect(x, y, w, h, WHITE);
  drawCenteredText(s, size, y);
  lastTimeLeft = tl;
}

// 8. 温湿度
void updateEnv(float T, float H) {
  static const int y=162, size=1;
  if (fabs(T-lastTemperature)<0.1 && fabs(H-lastHumidity)<0.1) return;
  char buf[32]; sprintf(buf,"T:%.1fC H:%.1f%%",T,H);
  String s(buf);
  int w=s.length()*6*size, h=8*size, x=(240-w)/2;
  gfx->fillRect(x, y, w, h, WHITE);
  drawCenteredText(s, size, y);
  lastTemperature=T; lastHumidity=H;
}

// ==== MQTT 回调 只关心特定 topic ====
void callback(char* topic, byte* payload, unsigned int len) {
  String v; for(unsigned int i=0;i<len;i++) v+=(char)payload[i];
  String t=String(topic);

  if      (t.endsWith("print_progress/state")) progress=v.toInt();
  else if (t.endsWith("print_status/state"))  print_state=v;
  else if (t.endsWith("current_stage/state")) current_stage=v;
  else if (t.endsWith("current_layer/state")) current_layer=v;
  else if (t.endsWith("total_layer_count/state")) total_layer=v;
  else if (t.endsWith("remaining_time/state")) {
    int m=v.toInt(); char b[16]; sprintf(b,"%02d:%02d:00",m/60,m%60);
    time_left=String(b);
  }
  else if (t.endsWith("active_tray/state")||t.endsWith("external_spool/state")) material=v;
  else return;
  dirty=true;
}

// ==== 启动提示函数 ====
void doConnectWiFi() {
  gfx->fillScreen(WHITE);
  drawCenteredText("Connecting WiFi...",2,120);
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  while(WiFi.status()!=WL_CONNECTED) delay(500);
  gfx->fillScreen(WHITE);
  drawCenteredText("WiFi OK",2,120);
  delay(500);
}

void doConnectMQTT() {
  gfx->fillScreen(WHITE);
  drawCenteredText("Connecting MQTT...",2,120);
  while(!client.connected()) {
    if(client.connect("ESP32C3",MQTT_USER,MQTT_PASS)) client.subscribe("bambu/#");
    else delay(2000);
  }
  gfx->fillScreen(WHITE);
  drawCenteredText("MQTT OK",2,120);
  delay(500);
}

// ==== setup 和 loop ====
void setup(){
  Serial.begin(115200);
  pinMode(TFT_BL,OUTPUT); digitalWrite(TFT_BL,HIGH);
  gfx->begin(); gfx->fillScreen(WHITE);
  dht.begin();

  lastDrawTime = millis() - REFRESH_INTERVAL;

  doConnectWiFi();
  client.setServer(MQTT_SERVER,1883);
  client.setCallback(callback);
  doConnectMQTT();

  // 启动完成，**再次清屏**并做一次初始局部更新
  gfx->fillScreen(WHITE);
  updateProgressArc(progress);
  updatePercentText(progress);
  updatePrintState(print_state);
  updateLayerInfo(current_layer + "/" + total_layer);
  updateStage(current_stage);
  updateMaterial(material);
  updateTimeLeft(time_left);
  updateEnv(temperature, humidity);
  dirty = false;
}

void loop(){
  client.loop();

  static unsigned long t0=0;
  if(millis()-t0>5000){
    float h=dht.readHumidity(), T=dht.readTemperature();
    if(!isnan(h)&&!isnan(T)){ humidity=h; temperature=T; dirty=true; }
    t0=millis();
  }

  if(dirty && millis()-lastDrawTime>REFRESH_INTERVAL){
    updateProgressArc(progress);
    updatePercentText(progress);
    updatePrintState(print_state);
    updateLayerInfo(current_layer + "/" + total_layer);
    updateStage(current_stage);
    updateMaterial(material);
    updateTimeLeft(time_left);
    updateEnv(temperature, humidity);
    lastDrawTime = millis();
    dirty = false;
  }
}
