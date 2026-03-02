/*******************************************************
 *  Project   : ESP32 দিয়ে বানিয়ে ফেলুন Global FTP Server | OLED ডিসপ্লে ও SD কার্ড ফাইল ম্যানেজার সার্ভার |
 *  Author    : Electri Trend
 *  Date      : 19 October 2025
 *  
 *  Description:
 *  ---------------------------------
 *  - ESP32 দিয়ে SD Card Module কানেক্ট করা
 *  - OLED Display (SSD1306 I2C) তে রিয়েল-টাইম আপলোড প্রগ্রেস দেখানো
 *  - HTML + CSS + JavaScript দিয়ে গ্লোবাল Web UI তৈরি করা
 *  - File Upload, Delete, এবং Auto Refresh সিস্টেম বানানো
 *  - SD Card থেকে যেকোনো ফাইল (txt, jpg, mp4, zip ইত্যাদি) ট্রান্সফার করা
 *  - ESP32-কে Globl Wi-Fi Network এর মাধ্যমে সার্ভার হিসেবে চালানো
 *  - cloudflared ব্যবহার করে ওয়েব সার্ভারে লোকাল সার্ভারকে গ্লোবালি হোস্ট করা
 *  ---------------------------------
 *  Copyright © 2025 Electri Trend
 *  All rights reserved.
 *
 *  ⚡ For tutorials, visit:
 *     👉 YouTube:    https://www.youtube.com/@Electritrendbd
 *     👉 Facebook:   https://www.facebook.com/electritrendbd
 *     👉 Instagram:  https://www.instagram.com/electri.trend
 *     👉 Github:     https://github.com/ElectriTrend
 *     👉 Website:    https://sites.google.com/view/electritrend/home
 *******************************************************/
// ========== [ Required Libraries ] ==========
#include <WiFi.h>               // For WiFi connectivity
#include <WebServer.h>          // For hosting web server
#include <SD.h>                 // For SD card file handling
#include <SPI.h>                // For SPI communication with SD
#include <Adafruit_GFX.h>       // Graphics library for OLED
#include <Adafruit_SSD1306.h>   // OLED display driver
#include <Wire.h>               // I2C communication (used by OLED)

// =================== Hardware / Config ===================
// LED, Button, Buzzer, SD card এবং Display পিন সেটআপ

#define LED_GREEN 25
#define LED_BLUE 26
#define LED_RED 27
#define RESET_SWITCH 14
#define BUZZER_PIN 15
#define SD_CS 5
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi Configuration
const char* ssid = "not_allow";
const char* password = "#araf#arafin#alif";

// Access Point fallback mode
const char* apSSID = "ESP32_FileHub";
const char* apPass = "12345678";

// Built-in LED
#define LED_PIN 2
#define BUZZER_PIN 15

WebServer server(80);  // Web server runs on port 80
File uploadFile;       // Active file for upload

// =================== State Variables ===================
bool sdPresent = false;
unsigned long lastUpdate = 0;
unsigned long sdBlinkMillis = 0;
bool sdBlinkState = false;

volatile unsigned long uploadProgress = 0;
volatile unsigned long uploadTotal = 0;
volatile bool uploading = false;
String currentUploadFile = "";

float oledDisplayPercent = 0.0;
float oledTargetPercent = 0.0;
unsigned long lastSmoothMillis = 0;

// =================== Helper Functions ===================

// Display simple messages on OLED (Bangla: OLED এ বার্তা দেখানোর জন্য)
void showMessage(String line1, String line2 = "", String line3 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 6);
  display.println(line1);
  if (line2 != "") { display.setCursor(0, 22); display.println(line2); }
  if (line3 != "") { display.setCursor(0, 38); display.println(line3); }
  display.display();
}

// Calculate SD used bytes (মোট কত bytes ব্যবহার হয়েছে)
uint64_t computeSDUsedBytes() {
  uint64_t total = 0;
  File root = SD.open("/");
  if (!root) return 0;
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) total += file.size();
    file = root.openNextFile();
  }
  root.close();
  return total;
}

// Check if SD card is present or not
bool checkSDPresent() {
  File root = SD.open("/");
  if (!root) return false;
  root.close();
  return true;
}

// Detect content type based on file extension (Bangla: কোন ফাইল টাইপ তা শনাক্ত করে)
String getContentType(String filename) {
  filename.toLowerCase();
  if (filename.endsWith(".htm") || filename.endsWith(".html")) return "text/html";
  if (filename.endsWith(".css")) return "text/css";
  if (filename.endsWith(".js")) return "application/javascript";
  if (filename.endsWith(".png")) return "image/png";
  if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
  if (filename.endsWith(".gif")) return "image/gif";
  if (filename.endsWith(".txt")) return "text/plain";
  if (filename.endsWith(".json")) return "application/json";
  if (filename.endsWith(".pdf")) return "application/pdf";
  if (filename.endsWith(".zip")) return "application/zip";
  if (filename.endsWith(".csv")) return "text/csv";
  if (filename.endsWith(".mp4")) return "video/mp4";
  return "application/octet-stream";
}

// =================== OLED Display Functions ===================

// Show upload progress bar on OLED
void updateOLEDProgressBar() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Upload: ");
  if (currentUploadFile.length() > 18) {
    String s = currentUploadFile.substring(0, 15) + "...";
    display.println(s);
  } else display.println(currentUploadFile);

  display.setCursor(0, 14);
  display.print("Progress: ");
  display.print((int)oledDisplayPercent);
  display.println("%");

  int barX = 10, barY = 34, barW = 108, barH = 12;
  display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
  int filled = (int)((barW * oledDisplayPercent) / 100.0);
  if (filled > 0) display.fillRect(barX, barY, filled, barH, SSD1306_WHITE);
  display.display();
}

// Show current system status on OLED (WiFi, SD, uptime)
void updateDisplayStatus() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("WiFi: ");
  display.println(WiFi.SSID());
  display.setCursor(0, 10);
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.setCursor(0, 20);
  display.print("RSSI: ");
  display.print(WiFi.RSSI());
  display.println(" dBm");

  uint64_t usedMB = computeSDUsedBytes() / (1024ULL * 1024ULL);
  display.setCursor(0, 30);
  display.print("SD: ");
  display.println(sdPresent ? String(usedMB) + " MB" : "No SD");

  unsigned long uptime = millis() / 1000;
  display.setCursor(0, 46);
  display.print("Uptime: ");
  display.print(uptime / 60);
  display.println(" min");
  display.display();
}

// =================== HTTP Core Functions ===================

// Serve static files from SD card (index.html, css, js etc.)
void serveStaticFile(String path) {
  if (path.length() == 0) path = "/index.html";
  if (path.charAt(0) != '/') path = "/" + path;

  String ct = getContentType(path);
  File f = SD.open(path);
  if (!f) { server.send(404, "text/plain", "File not found"); return; }
  server.streamFile(f, ct);
  f.close();
}

// Certain files are protected from delete or overwrite
bool isProtectedFile(String name) {
  return name == "/index.html" ||
         name == "/about.html" ||
         name == "/files.html" ||
         name == "/system.html" ||
         name == "/style.css"  ||
         name == "/script.js";
}

// =================== API ROUTES ===================

// List all files on SD card as JSON
void handleList() {
  File root = SD.open("/");
  if (!root) {
    server.send(200, "application/json", "[]");
    return;
  }

  File file = root.openNextFile();
  String json = "[";
  bool first = true;

  while (file) {
    if (!file.isDirectory()) {
      if (!first) json += ",";
      String name = String(file.name());
      bool prot = isProtectedFile(name);
      json += "{\"name\":\"" + name + "\",\"size\":" + String(file.size()) +
              ",\"protected\":" + String(prot ? "true" : "false") + "}";
      first = false;
    }
    file = root.openNextFile();
  }

  root.close();
  json += "]";
  server.send(200, "application/json", json);
}

// Delete file from SD card (protected files cannot be deleted)
void handleDelete() {
  if (!server.hasArg("name")) return server.send(400, "text/plain", "Missing file name");
  String filename = server.arg("name");
  if (filename.length() > 0 && filename.charAt(0) == '/') filename = filename.substring(1);
  String full = "/" + filename;

  if (isProtectedFile(full)) {
    server.send(403, "text/plain", "Delete not allowed for protected files");
    return;
  }

  if (SD.exists(full)) {
    SD.remove(full);
    showMessage("File Deleted", filename);
    server.send(200, "text/plain", "Deleted");
  } else {
    server.send(404, "text/plain", "Not found");
  }
}

// Download file handler
void handleDownload() {
  if (!server.hasArg("name"))
    return server.send(400, "text/plain", "Missing file name");

  String filename = "/" + server.arg("name");
  if (!SD.exists(filename)) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  File f = SD.open(filename);
  if (!f) { server.send(404, "text/plain", "Open failed"); return; }

  String ct = getContentType(filename);
  server.sendHeader("Content-Type", ct);
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + server.arg("name") + "\"");
  server.streamFile(f, ct);
  f.close();
}

// Return system status as JSON (WiFi, SD, uptime, etc.)
void handleStatus() {
  uint64_t usedMB = computeSDUsedBytes() / (1024ULL * 1024ULL);
  unsigned long uptimeMin = (millis() / 1000UL) / 60UL;
  String json = "{";
  json += "\"ssid\":\"" + String(WiFi.SSID()) + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"uptime_min\":" + String(uptimeMin) + ",";
  json += "\"sd_present\":" + String(sdPresent ? "true" : "false") + ",";
  json += "\"sd_used_mb\":" + String(usedMB);
  json += "}";
  server.send(200, "application/json", json);
}

// =================== File Upload Handler ===================
void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    // File upload শুরু হলে SD তে ফাইল খোলা হয়
    String filename = "/" + upload.filename;
    uploadFile = SD.open(filename, FILE_WRITE);
    currentUploadFile = upload.filename;
    uploading = true;
    uploadProgress = 0;
    oledTargetPercent = 0;
    oledDisplayPercent = 0;

    digitalWrite(LED_BLUE, HIGH);
    showMessage("Uploading...", upload.filename);
  } 
  
  else if (upload.status == UPLOAD_FILE_WRITE) {
    // Writing file chunk to SD
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
    uploadProgress += upload.currentSize;
    oledTargetPercent = min(100.0f, ((float)uploadProgress / upload.totalSize) * 100.0f);
    updateOLEDProgressBar();
  } 
  
  else if (upload.status == UPLOAD_FILE_END) {
    // Upload completed
    if (uploadFile) uploadFile.close();
    showMessage("Upload Done", upload.filename);
    uploading = false;
    oledTargetPercent = 100;
    oledDisplayPercent = 100;
    updateOLEDProgressBar();

    digitalWrite(LED_BLUE, LOW);
    tone(BUZZER_PIN, 2000, 120);
    delay(400);
    noTone(BUZZER_PIN);

    updateDisplayStatus();
  }
}

// =================== Setup ===================
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);  // I2C pins for OLED
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(RESET_SWITCH, INPUT_PULLUP);  // Switch pressed = GND

  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_BLUE, LOW);
  digitalWrite(LED_RED, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED failed"));
    while (1);
  }

  // WiFi connection attempt
  showMessage("Connecting WiFi...", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 25) { delay(400); tries++; }

  // যদি WiFi না মিলে, AP mode এ চলে যায়
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPass);
    showMessage("AP Mode", "SSID: " + String(apSSID), "IP: " + WiFi.softAPIP().toString());
  } else {
    showMessage("WiFi OK", WiFi.localIP().toString());
  }

  // SD card mount check
  if (!SD.begin(SD_CS)) {
    sdPresent = false;
    showMessage("SD Mount Failed!");
  } else {
    sdPresent = checkSDPresent();
    showMessage("SD Ready", "Server starting...");
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, LOW);
  }

  // Define all web routes
  server.on("/", []() { serveStaticFile("/index.html"); });
  server.on("/file", []() { serveStaticFile("/files.html"); });
  server.on("/about", []() { serveStaticFile("/about.html"); });
  server.on("/system", []() { serveStaticFile("/system.html"); });
  server.on("/style.css", []() { serveStaticFile("/style.css"); });
  server.on("/script.js", []() { serveStaticFile("/script.js"); });

  server.on("/list", handleList);
  server.on("/delete", handleDelete);
  server.on("/download", handleDownload);
  server.on("/upload", HTTP_POST, []() { server.send(200, "text/plain", "OK"); }, handleFileUpload);
  server.on("/status", handleStatus);

  server.begin();
  showMessage("Server Ready", WiFi.localIP().toString());
  Serial.println("Server ready at " + WiFi.localIP().toString());

  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, LOW);
}

// =================== Fault Alert ===================
// If WiFi or SD missing, buzzer + red LED blink
void systemFaultAlert() {
  static unsigned long lastBeep = 0;
  static bool ledState = false;
  unsigned long now = millis();

  if (now - lastBeep >= 1000) {
    lastBeep = now;
    tone(BUZZER_PIN, 1500, 150);
    ledState = !ledState;
    digitalWrite(LED_RED, ledState);
  }
}

// =================== Manual Reset Button ===================
void checkManualReset() {
  static unsigned long lastPress = 0;
  if (digitalRead(RESET_SWITCH) == LOW) {
    if (millis() - lastPress > 1000) {
      ESP.restart();
    }
  } else {
    lastPress = millis();
  }
}

// OLED auto-update depending on mode (uploading/status)
void updateOLED() {
  if (uploading) {
    updateOLEDProgressBar();
  } else {
    updateDisplayStatus();
  }
}

// =================== Main Loop ===================
void loop() {
  checkManualReset();
  server.handleClient();
  updateOLED();

  // Check for WiFi/SD failure
  if (WiFi.status() != WL_CONNECTED || !sdPresent) {
    digitalWrite(LED_GREEN, LOW);
    systemFaultAlert();
  } else {
    digitalWrite(LED_RED, LOW);
  }

  server.handleClient();
  unsigned long now = millis();

  // Periodic SD card insert/remove detection
  if (now - lastUpdate > 2000) {
    bool presentNow = checkSDPresent();
    if (presentNow != sdPresent) {
      sdPresent = presentNow;
      showMessage(sdPresent ? "SD Inserted" : "SD Removed");
    }
    lastUpdate = now;
  }

  // Smooth OLED progress animation
  if (uploading) {
    unsigned long dt = now - lastSmoothMillis;
    if (dt > 50) {
      lastSmoothMillis = now;
      if (oledDisplayPercent < oledTargetPercent) oledDisplayPercent += 1.0;
      updateOLEDProgressBar();
    }
  } else {
    static unsigned long lastDisp = 0;
    if (now - lastDisp > 5000) {
      updateDisplayStatus();
      lastDisp = now;
    }
  }
}
