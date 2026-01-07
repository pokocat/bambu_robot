#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <Arduino_GFX_Library.h>
#include <DHT.h>

// ==== 引脚定义 ====
// 引脚定义已移至 config.h

// ==== 颜色常量 ====
// 自定义颜色定义，使用16位RGB565格式
// 修复溢出警告：所有颜色值必须是16位RGB565格式
#define TFT_BG      0xEECE  // 浅灰色背景 (正确的RGB565值)
#define TFT_ACCENT  0x03E0  // 深绿色强调色（用于环状进度条）
#define TFT_WARN    0xFB20  // 橙色警告色
#define TFT_OK      0x07E0  // 绿色成功色
#define TFT_RING_BG 0x6D96  // 中灰色环形背景 (正确的RGB565值)
#define TFT_LABEL   0x528A  // 深灰色标签（提高对比度）
#define TFT_TEXT    0x0000  // 黑色文本（提高小字可读性）
#define TFT_DEVICE_TEMP 0x001F  // 深蓝色（用于设备温度）
#define TFT_ENV_TEMP 0xF800  // 深红色（用于环境温湿度）
#define TFT_PROGRESS 0xF81F  // 粉红色进度条（可选替代色）

// ==== Display 初始化 ====
Arduino_DataBus *bus = new Arduino_SWSPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, -1);
Arduino_GFX     *gfx = new Arduino_GC9A01(bus, TFT_RST, 0);

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
  
  int rulesCount = sizeof(rules) / sizeof(rules[0]);
  for (int i = 0; i < rulesCount; i++) {
    s.replace(rules[i].a, rules[i].b);
  }
  
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
  
  // 调整环形尺寸，确保在圆形屏幕上完整显示
  int cx=120, cy=120, r0=95, r1=110;
  
  // 绘制完整的环形背景（使用drawCircle而不是fillRect，避免覆盖文字）
  gfx->drawCircle(cx, cy, r1, TFT_RING_BG);
  gfx->drawCircle(cx, cy, r0, TFT_BG);
  
  // 绘制环形进度，使用更密集的线条确保连续性
  int startAngle = 270; // 从顶部开始
  int sweepAngle = map(constrain(newProg,0,100), 0, 100, 0, 360);
  
  // 使用极高密度的角度间隔（0.1度）确保形成连续环形
  for (float angle = 0; angle <= sweepAngle; angle += 0.1) {
    float rad = (startAngle + angle) * PI / 180.0;
    int x1 = cx + cos(rad) * r0;
    int y1 = cy + sin(rad) * r0;
    int x2 = cx + cos(rad) * r1;
    int y2 = cy + sin(rad) * r1;
    gfx->drawLine(x1, y1, x2, y2, TFT_ACCENT);
  }
  
  // 特别处理360度的情况，确保完整闭合
  if (sweepAngle >= 360) {
    gfx->drawCircle(cx, cy, r1, TFT_ACCENT);
    gfx->drawCircle(cx, cy, r0, TFT_BG);
  }
  
  localLastProgress = newProg;
  // 同步全局变量，以防其他地方需要（虽然现在推荐各函数自治）
  lastProgress = newProg;
}

// 2. 顶部状态 (State) - 自动调整字号
void updatePrintState(const String &st) {
  // Y=50 (Top safe area)
  static const int y=50;
  
  // 调试：打印收到的状态值
  Serial.print("Print state received: ");
  Serial.println(st);
  
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
  
  // 只清除文字区域，避免覆盖环形进度条
  gfx->fillRect(x - 5, y - 2, w + 10, 16, TFT_BG);
  
  // 根据状态设置颜色，支持更多状态值
  uint16_t color = TFT_TEXT;
  if (st == "RUNNING" || st == "Printing" || st == "printing") {
    color = TFT_OK;
  } else if (st == "offline" || st == "Offline") {
    color = TFT_WARN;
  }
  
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
  
  // 只清除文字区域，避免覆盖环形进度条
  gfx->fillRect(maxX - 5, y - 5, maxW + 10, h + 10, TFT_BG);
  
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
  
  int textW = tl.length() * 6 * size;
  int textX = (240 - textW) / 2;
  int textH = 8 * size;
  
  // 只清除文字区域，避免覆盖环形进度条
  gfx->fillRect(textX - 5, y - 2, textW + 10, textH + 4, TFT_BG);
  
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

  int textW = info.length() * 6 * size;
  int textX = (240 - textW) / 2;
  int textH = 8 * size;
  
  // 只清除文字区域，避免覆盖环形进度条
  gfx->fillRect(textX - 5, y - 2, textW + 10, textH + 4, TFT_BG);
  
  drawCenteredText(info, size, y, TFT_LABEL);
  lastInfo = info;
}

// 6. 环境温湿度 (Env) + 打印头/热床温度
void updateEnv(float T, float H, float Noz, float Bed) {
  // Y=185 (Bottom edge, split into two lines for better readability)
  static const int y1=185, y2=195, size=1;
  
  // 只有当任一数值发生显著变化时才刷新
  if (fabs(T-lastTemperature)<0.1 && fabs(H-lastHumidity)<0.1 && 
      fabs(Noz-lastNozzleTemp)<1.0 && fabs(Bed-lastBedTemp)<1.0) return;
  
  // 重新排列环境信息：
  // 第一行显示打印相关温度（来自MQTT）
  // 第二行显示环境温湿度（来自DHT11）
  // 使用缩写和简化格式，避免乱码
  char buf1[15];
  char buf2[15];
  
  // 第一行：打印头温度和热床温度（来自MQTT），使用缩写
  sprintf(buf1,"N:%.0fC B:%.0fC", Noz, Bed);
  String line1(buf1);
  
  // 第二行：环境温度和湿度（来自DHT11），使用缩写
  sprintf(buf2,"E:%.1fC H:%.0f%%", T, H);
  String line2(buf2);
  
  // 计算第一行的位置
  int line1W = line1.length() * 6 * size;
  int line1X = (240 - line1W) / 2;
  int lineH = 8 * size;
  
  // 计算第二行的位置
  int line2W = line2.length() * 6 * size;
  int line2X = (240 - line2W) / 2;
  
  // 清除整个环境信息区域
  gfx->fillRect(0, y1 - 2, 240, (y2 + lineH) - (y1 - 2) + 4, TFT_BG);
  
  // 绘制两行文本，使用不同颜色区分
  gfx->setTextSize(size);
  
  // 设备温度使用深蓝色
  gfx->setTextColor(TFT_DEVICE_TEMP);
  gfx->setCursor(line1X, y1);
  gfx->print(line1);
  
  // 环境温湿度使用深红色
  gfx->setTextColor(TFT_ENV_TEMP);
  gfx->setCursor(line2X, y2);
  gfx->print(line2);
  
  lastTemperature = T; 
  lastHumidity = H;
  lastNozzleTemp = Noz;
  lastBedTemp = Bed;
}

// ==== MQTT 回调 ====
void callback(char* topic, byte* payload, unsigned int len) {
  String v; 
  for(unsigned int i=0;i<len;i++) {
    v+=(char)payload[i];
  }
  String t=String(topic);
  
  // 调试：打印收到的MQTT消息
  Serial.print("MQTT Topic: ");
  Serial.print(t);
  Serial.print(", Payload: ");
  Serial.println(v);

  if      (t.endsWith("print_progress/state")) {
    progress=v.toInt();
    Serial.print("Progress updated: ");
    Serial.println(progress);
  }
  else if (t.endsWith("print_status/state")) {
    print_state=v;
    Serial.print("Print state updated: ");
    Serial.println(print_state);
  }
  else if (t.endsWith("current_stage/state")) {
    current_stage=v;
    Serial.print("Current stage updated: ");
    Serial.println(current_stage);
  }
  else if (t.endsWith("current_layer/state")) {
    current_layer=v;
    Serial.print("Current layer updated: ");
    Serial.println(current_layer);
  }
  else if (t.endsWith("total_layer_count/state")) {
    total_layer=v;
    Serial.print("Total layer updated: ");
    Serial.println(total_layer);
  }
  else if (t.endsWith("remaining_time/state")) {
    int m=v.toInt(); 
    // 格式化为 HH:MM
    char b[16]; 
    if (m >= 60) sprintf(b, "%dh %02dm", m/60, m%60);
    else         sprintf(b, "%dm", m);
    time_left=String(b);
    Serial.print("Time left updated: ");
    Serial.println(time_left);
  }
  else if (t.endsWith("active_tray/state")||t.endsWith("external_spool/state")) {
    material=v;
    Serial.print("Material updated: ");
    Serial.println(material);
  }
  else if (t.endsWith("nozzle_temperature/state")) {
    nozzle_temp=v.toFloat();
    Serial.print("Nozzle temp updated: ");
    Serial.println(nozzle_temp);
  }
  else if (t.endsWith("bed_temperature/state")) {
    bed_temp=v.toFloat();
    Serial.print("Bed temp updated: ");
    Serial.println(bed_temp);
  }
  else {
    Serial.print("Unknown topic: ");
    Serial.println(t);
    return;
  }
  
  dirty=true;
  Serial.println("Dirty flag set to true");
}

// ==== 启动提示函数 ====
void doConnectWiFi() {
  gfx->fillScreen(TFT_BG);
  drawCenteredText("Connecting WiFi...", 2, 110, TFT_ACCENT);
  
  // 设置WiFi模式为STA
  WiFi.mode(WIFI_STA);
  // 清除之前的连接信息
  WiFi.disconnect();
  delay(100);
  
  // 开始连接WiFi
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  
  // 添加超时机制，最多尝试15秒
  unsigned long timeout = millis();
  int attempts = 0;
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    attempts++;
    
    // 每3秒更新一次屏幕显示
    if(attempts % 6 == 0) {
      gfx->fillScreen(TFT_BG);
      drawCenteredText("Connecting WiFi...", 2, 110, TFT_ACCENT);
      gfx->setTextSize(1);
      gfx->setCursor(50, 130);
      gfx->setTextColor(TFT_TEXT);
      gfx->print("Attempt: " + String(attempts/2));
    }
    
    // 超时处理
    if(millis() - timeout > 15000) {
      gfx->fillScreen(TFT_BG);
      drawCenteredText("WiFi Failed", 2, 110, TFT_WARN);
      delay(1000);
      // 重启设备重试
      ESP.restart();
      return;
    }
  }
  
  gfx->fillScreen(TFT_BG);
  drawCenteredText("WiFi OK", 2, 110, TFT_OK);
  delay(500);
}

void doConnectMQTT() {
  gfx->fillScreen(TFT_BG);
  drawCenteredText("Connecting MQTT...", 2, 110, TFT_ACCENT);
  
  Serial.println("=== MQTT Connection Attempt ===");
  Serial.print("MQTT Server: ");
  Serial.print(MQTT_SERVER);
  Serial.println(":1883");
  Serial.print("MQTT Client ID: ESP32C3");
  Serial.print(", User: ");
  Serial.println(MQTT_USER);
  
  // 添加超时机制，最多尝试10秒
  unsigned long timeout = millis();
  int attempts = 0;
  while(!client.connected()) {
    attempts++;
    Serial.print("MQTT Connect Attempt #");
    Serial.println(attempts);
    
    if(client.connect("ESP32C3",MQTT_USER,MQTT_PASS)) {
      Serial.println("MQTT Connected Successfully!");
      Serial.println("Subscribing to topic: bambu/#");
      client.subscribe("bambu/#");
      break;
    }
    
    Serial.print("MQTT Connect Failed, State: ");
    Serial.println(client.state());
    
    // 每2秒更新一次屏幕显示
    if(attempts % 2 == 0) {
      gfx->fillScreen(TFT_BG);
      drawCenteredText("Connecting MQTT...", 2, 110, TFT_ACCENT);
      gfx->setTextSize(1);
      gfx->setCursor(50, 130);
      gfx->setTextColor(TFT_TEXT);
      gfx->print("Attempt: " + String(attempts/2));
    }
    
    // 超时处理
    if(millis() - timeout > 10000) {
      Serial.println("MQTT Connection Timeout (10s)");
      gfx->fillScreen(TFT_BG);
      drawCenteredText("MQTT Failed", 2, 110, TFT_WARN);
      delay(1000);
      // 重启设备重试
      ESP.restart();
      return;
    }
    
    delay(1000);
  }
  
  gfx->fillScreen(TFT_BG);
  drawCenteredText("MQTT OK", 2, 110, TFT_OK);
  delay(500);
  Serial.println("MQTT Connection Complete");
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
  // 检查MQTT连接状态，如果断开则重连
  if (!client.connected()) {
    Serial.println("MQTT Disconnected, attempting to reconnect...");
    doConnectMQTT();
  }
  client.loop();

  // 每30秒输出一次MQTT状态信息
  static unsigned long lastStatusCheck = 0;
  if (millis() - lastStatusCheck > 30000) {
    Serial.print("MQTT Status - Connected: ");
    Serial.println(client.connected() ? "YES" : "NO");
    lastStatusCheck = millis();
  }

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
