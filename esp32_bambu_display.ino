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

// ==== 颜色常量 ====
#define TFT_BG      BLACK
#define TFT_ACCENT  0x07FF  // CYAN
#define TFT_WARN    0xFCA0  // ORANGE
#define TFT_OK      0x07E0  // GREEN
#define TFT_RING_BG 0x3333  // Dark Grey
#define TFT_LABEL   LIGHTGREY
#define TFT_TEXT    WHITE

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
String time_left         = "--:--", lastTimeLeft     = "";
float  temperature       = 0,      humidity          = 0;
float  nozzle_temp       = 0,      bed_temp          = 0;
float  lastTemperature   = -999,   lastHumidity      = -999;
float  lastNozzleTemp    = -999,   lastBedTemp       = -999;

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

// 术语压缩：把超长词替换为短写 
String compressTerms(String s) {
  // 先把下划线换成空格，便于阅读和处理
  s.replace("_", " ");
  
  // 针对特定全句的映射 (Priority High)
  if (s == "offline") return "Offline";
  if (s == "printing") return "Printing";
  if (s == "idle") return "Idle";
  if (s == "auto bed leveling") return "Bed Level";
  if (s == "heatbed preheating") return "Preheat Bed";
  if (s == "sweeping xy mech mode") return "Sweep XY";
  if (s == "changing filament") return "Change Fil";
  if (s == "m400 pause") return "Pause M400";
  if (s == "paused filament runout") return "Pause:NoFil";
  if (s == "heating hotend") return "Heat Hotend";
  if (s == "calibrating extrusion") return "Calib Extr";
  if (s == "scanning bed surface") return "Scan Bed";
  if (s == "inspecting first layer") return "Chk 1st Lyr";
  if (s == "identifying build plate type") return "ID Plate";
  if (s == "calibrating micro lidar") return "Calib Lidar";
  if (s == "homing toolhead") return "Homing";
  if (s == "cleaning nozzle tip") return "Clean Noz";
  if (s == "checking extruder temperature") return "Chk Extr T";
  if (s == "paused user") return "Pause:User";
  if (s == "paused front cover falling") return "Err:Cover";
  if (s == "calibrating extrusion flow") return "Calib Flow";
  if (s.startsWith("paused nozzle temperature")) return "Err:NozTemp";
  if (s.startsWith("paused heat bed temperature")) return "Err:BedTemp";
  if (s == "filament unloading") return "Unload Fil";
  if (s == "paused skipped step") return "Err:SkipStp";
  if (s == "filament loading") return "Load Fil";
  if (s == "calibrating motor noise") return "Calib Motor";
  if (s == "paused ams lost") return "Err:AMS";
  if (s == "paused low fan speed heat break") return "Err:Fan";
  if (s.startsWith("paused chamber temp")) return "Err:ChambT";
  if (s == "cooling chamber") return "Cool Chamb";
  if (s == "paused user gcode") return "Pause:Gcode";
  if (s == "motor noise showoff") return "Motor Demo";
  if (s.startsWith("paused nozzle filament covered")) return "Err:NozClog";
  if (s == "paused cutter error") return "Err:Cutter";
  if (s == "paused first layer error") return "Err:1stLyr";
  if (s == "paused nozzle clog") return "Err:Clog";
  if (s.startsWith("check absolute accuracy")) return "Chk Acc";
  if (s == "absolute accuracy calibration") return "Calib Acc";
  if (s == "calibrate nozzle offset") return "Calib Offs";
  if (s == "bed level high temperature") return "BedLvl High";
  if (s == "check quick release") return "Chk QuickRel";
  if (s == "check door and cover") return "Chk Door";
  if (s == "laser calibration") return "Calib Laser";
  if (s == "check plaform") return "Chk Plate";
  if (s.indexOf("birdeye") >= 0) return "Chk Camera";
  if (s == "bed level phase 1") return "BedLvl 1";
  if (s == "bed level phase 2") return "BedLvl 2";
  if (s == "heating chamber") return "Heat Chamb";
  if (s == "heated bedcooling") return "Cool Bed";
  if (s == "print calibration lines") return "Prn CalLine";

  // 通用单词替换 (Fallback)
  struct R { const char* a; const char* b; } rules[] = { 
    {"Temperature", "T"}, {"Temp", "T"},
    {"Calibration", "Calib"}, {"Calibrating", "Calib"}, {"Calibrate", "Calib"},
    {"Accuracy", "Acc"}, 
    {"Identifying", "ID"}, {"Identification", "ID"}, 
    {"Checking", "Chk"}, {"Check", "Chk"}, 
    {"Inspecting", "Chk"}, {"Inspection", "Chk"},
    {"Extrusion", "Extr"}, {"Extruder", "Extr"}, 
    {"Chamber", "Chamb"}, 
    {"Platform", "Plate"}, {"Build Plate", "Plate"}, 
    {"Filament", "Fil"}, 
    {"Nozzle", "Noz"}, 
    {"Leveling", "Lvl"}, {"Level", "Lvl"},
    {"Cooling", "Cool"}, {"Heating", "Heat"}, {"Heated", "Heat"},
    {"Homing", "Home"}, 
    {"First Layer", "1st Lyr"}, 
    {"Surface", "Surf"},
    {"Paused", "Pause"},
    {"Malfunction", "Err"}, {"Error", "Err"},
    {"Front Cover", "Cover"},
    {"Running", "Run"}
  }; 
  
  for (auto &r : rules) s.replace(r.a, r.b); 
  
  // 再次清理多余空格
  while (s.indexOf("  ") >= 0) s.replace("  ", " ");
  s.trim();
  
  return s; 
}

// 居中绘制默认 6×8 字体 (支持颜色)
void drawCenteredText(const String &txt, uint8_t size, int y, uint16_t color) {
  gfx->setTextColor(color);
  gfx->setTextSize(size);
  int w = txt.length() * 6 * size;
  int x = (240 - w) / 2;
  gfx->setCursor(x, y);
  gfx->print(txt);
}

// ==== 各区域局部更新 (UI Layout V2) ====

// 1. 环形进度 (含底环)
void updateProgressArc(int newProg) {
  // 使用本地静态变量跟踪上次绘制的进度，解耦全局依赖
  static int localLastProgress = -1;
  
  if (newProg == localLastProgress) return;
  
  int cx=120, cy=120, r0=100, r1=115;
  
  // 首次绘制画完整底环
  if (localLastProgress == -1) {
    gfx->fillArc(cx, cy, r0, r1, 0, 360, TFT_RING_BG);
  }

  int oldDeg = (localLastProgress < 0) ? 0 : map(constrain(localLastProgress,0,100), 0, 100, 0, 360);
  int newDeg = map(constrain(newProg,0,100), 0, 100, 0, 360);

  if (newDeg > oldDeg) {
    // 增加：绘制高亮色
    gfx->fillArc(cx, cy, r0, r1, 270.0 + oldDeg, 270.0 + newDeg, TFT_ACCENT);
  } else if (newDeg < oldDeg) {
    // 减少：绘制底环色擦除
    gfx->fillArc(cx, cy, r0, r1, 270.0 + newDeg, 270.0 + oldDeg, TFT_RING_BG);
  }
  
  localLastProgress = newProg;
  // 同步全局变量，以防其他地方需要（虽然现在推荐各函数自治）
  lastProgress = newProg;
}

// 2. 顶部状态 (State) - 自动调整字号
void updatePrintState(const String &st) {
  // Y=50 (Top safe area)
  static const int y=50;
  String s = truncateMaterial(compressTerms(st));
  if (s == lastPrintState) return;
  
  // Safe Width at Y=50 is ~184px
  // Size 2 char width = 12px. Max chars = 15.
  // Size 1 char width = 6px. Max chars = 30.
  
  int size = 2;
  int w = s.length() * 6 * size;
  
  if (w > 180) { 
    size = 1;
    w = s.length() * 6 * size;
  }
  
  int x = (240 - w) / 2;
  
  // Clear full safe width, height 16px
  gfx->fillRect(0, y, 240, 16, TFT_BG);
  
  uint16_t color = (st == "RUNNING" || st == "Printing") ? TFT_OK : TFT_TEXT;
  drawCenteredText(s, size, y, color);
  lastPrintState = s;
}

// 3. 中央百分比 (Percent)
void updatePercentText(int newProg) {
  // Y=90 (Widest area)
  static const int y=90, size=6;
  
  static int lastP = -1;
  if (newProg == lastP) return;

  String t = String(newProg) + "%";
  
  // Safe Width at Y=90 is ~222px.
  // 4 chars * 36px = 144px. Plenty of room.
  
  int maxW = 4 * 6 * size; 
  int maxX = (240 - maxW) / 2;
  int h = 8 * size;
  
  gfx->fillRect(maxX, y, maxW, h, TFT_BG);
  
  drawCenteredText(t, size, y, TFT_ACCENT);
  lastP = newProg;
}

// 4. 剩余时间 (Time Left)
void updateTimeLeft(const String &tl) {
  // Y=145 (Lower middle)
  static const int y=145, size=2;
  if (tl == lastTimeLeft) return;
  
  // Safe Width at Y=145 is ~215px.
  // "0h 00m" is 6 chars * 12px = 72px. Plenty.
  
  int maxW = 10 * 6 * size; 
  int maxX = (240 - maxW) / 2;
  gfx->fillRect(maxX, y, maxW, 8*size, TFT_BG);
  
  drawCenteredText(tl, size, y, TFT_TEXT);
  lastTimeLeft = tl;
}

// 5. 底部信息 (Layer / Material / Stage)
void updateBottomInfo(const String &stage, const String &layer, const String &mat) {
  // Y=170 (Bottom area)
  static const int y=170, size=1;
  
  // Safe Width at Y=170 is ~190px.
  // Max chars = 31.
  
  String prefix;
  if (stage.length() > 0 && stage != "Printing" && stage != "RUNNING" && stage != "Idle") {
    prefix = compressTerms(stage);
  } else {
    prefix = truncateMaterial(mat);
  }
  
  String info = prefix + " " + layer; 
  
  static String lastInfo = "";
  if (info == lastInfo) return;

  gfx->fillRect(0, y, 240, 8*size, TFT_BG);
  
  drawCenteredText(info, size, y, TFT_LABEL);
  lastInfo = info;
}

// 6. 环境温湿度 (Env) + 打印头/热床温度
void updateEnv(float T, float H, float Noz, float Bed) {
  // Y=190 (Bottom edge)
  static const int y=190, size=1;
  
  // 只有当任一数值发生显著变化时才刷新
  if (fabs(T-lastTemperature)<0.1 && fabs(H-lastHumidity)<0.1 && 
      fabs(Noz-lastNozzleTemp)<1.0 && fabs(Bed-lastBedTemp)<1.0) return;
  
  // 格式: "N:220 B:60 | T:24C" (省略湿度以节省空间，或者轮播)
  // 或者挤在一起: "N220 B60 T24 H40"
  // Safe Width ~166px. 
  // 尝试紧凑格式: "N:220 B:60 T:24"
  
  char buf[32]; 
  // 优先显示打印核心温度
  sprintf(buf,"N:%.0f B:%.0f T:%.0f", Noz, Bed, T);
  String s(buf);
  
  gfx->fillRect(0, y, 240, 8*size, TFT_BG);
  drawCenteredText(s, size, y, TFT_WARN);
  
  lastTemperature = T; 
  lastHumidity = H;
  lastNozzleTemp = Noz;
  lastBedTemp = Bed;
}

// ==== MQTT 回调 ====
void callback(char* topic, byte* payload, unsigned int len) {
  String v; for(unsigned int i=0;i<len;i++) v+=(char)payload[i];
  String t=String(topic);

  if      (t.endsWith("print_progress/state")) progress=v.toInt();
  else if (t.endsWith("print_status/state"))  print_state=v;
  else if (t.endsWith("current_stage/state")) current_stage=v;
  else if (t.endsWith("current_layer/state")) current_layer=v;
  else if (t.endsWith("total_layer_count/state")) total_layer=v;
  else if (t.endsWith("remaining_time/state")) {
    int m=v.toInt(); 
    // 格式化为 HH:MM
    char b[16]; 
    if (m >= 60) sprintf(b, "%dh %02dm", m/60, m%60);
    else         sprintf(b, "%dm", m);
    time_left=String(b);
  }
  else if (t.endsWith("active_tray/state")||t.endsWith("external_spool/state")) material=v;
  else if (t.endsWith("nozzle_temper/state")) nozzle_temp=v.toFloat();
  else if (t.endsWith("bed_temper/state")) bed_temp=v.toFloat();
  else return;
  dirty=true;
}

// ==== 启动提示函数 ====
void doConnectWiFi() {
  gfx->fillScreen(TFT_BG);
  drawCenteredText("Connecting WiFi...", 2, 110, TFT_ACCENT);
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  while(WiFi.status()!=WL_CONNECTED) delay(500);
  gfx->fillScreen(TFT_BG);
  drawCenteredText("WiFi OK", 2, 110, TFT_OK);
  delay(500);
}

void doConnectMQTT() {
  gfx->fillScreen(TFT_BG);
  drawCenteredText("Connecting MQTT...", 2, 110, TFT_ACCENT);
  while(!client.connected()) {
    if(client.connect("ESP32C3",MQTT_USER,MQTT_PASS)) client.subscribe("bambu/#");
    else delay(2000);
  }
  gfx->fillScreen(TFT_BG);
  drawCenteredText("MQTT OK", 2, 110, TFT_OK);
  delay(500);
}

// ==== setup 和 loop ====
void setup(){
  Serial.begin(115200);
  pinMode(TFT_BL,OUTPUT); digitalWrite(TFT_BL,HIGH);
  
  gfx->begin(); 
  gfx->fillScreen(TFT_BG); // Black BG
  dht.begin();

  lastDrawTime = millis() - REFRESH_INTERVAL;

  doConnectWiFi();
  client.setServer(MQTT_SERVER,1883);
  client.setCallback(callback);
  doConnectMQTT();

  // 启动完成，初始化界面
  gfx->fillScreen(TFT_BG);
  
  // 强制刷新所有元素
  lastProgress = -1; // 触发底环绘制
  lastPrintState = "";
  lastTimeLeft = "";
  lastTemperature = -999;
  
  updateProgressArc(progress);
  updatePercentText(progress);
  updatePrintState(print_state);
  updateTimeLeft(time_left);
  updateBottomInfo(current_stage, current_layer + "/" + total_layer, material);
  updateEnv(temperature, humidity, nozzle_temp, bed_temp);
  
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
    // 注意：updatePercentText 内部依赖静态变量去重，所以这里直接调没事
    updatePercentText(progress);
    updatePrintState(print_state);
    updateTimeLeft(time_left);
    updateBottomInfo(current_stage, current_layer + "/" + total_layer, material);
    updateEnv(temperature, humidity, nozzle_temp, bed_temp);
    
    lastDrawTime = millis();
    dirty = false;
  }
}
