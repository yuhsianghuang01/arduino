/**
 * EPD Web Controller via Wi-Fi AP
 * - Starts AP mode
 * - Shows IP:Port on EPD top-left
 * - Web interface to draw shapes or upload image
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "epd_driver.h"
#include "utilities.h"

// ===== WiFi AP Config =====
const char* ssid = "EPD-Controller";
const char* password = "12345678";  // 至少8碼

// ===== Web Server =====
WebServer server(80);

// ===== Framebuffer =====
uint8_t *framebuffer = NULL;
const int FB_SIZE = EPD_WIDTH * EPD_HEIGHT / 2;  // 2-bit grayscale

// ===== Helper: Draw text on EPD (simple ASCII, top-left) =====
void epd_draw_text(const char* text, uint8_t *fb) {
    // 簡化：只顯示一行文字（需有字型支援，此處用模擬方式）
    // 若 epd_driver 支援文字，請替換為實際函式，例如：
    // epd_draw_string(0, 0, text, 0, fb);
    // 若無，可跳過或用圖形模擬（本範例假設有）
    if (fb) {
        // Placeholder: 假設有文字繪製函式
        // 若沒有，可先註解此函式調用，或自行實現簡易 5x7 字型
    }
}

// ===== Web Handlers =====

void handleRoot() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>EPD Controller</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; padding: 10px; }
    button { margin:5px; padding:10px; font-size:16px; }
    .upload { margin-top:20px; }
  </style>
</head>
<body>
  <h2>EPD Web Controller</h2>
  <button onclick="fetch('/clear')">Clear Screen</button>
  <button onclick="fetch('/draw/line')">Draw Random Line</button>
  <button onclick="fetch('/draw/rect')">Draw Random Rect</button>
  <button onclick="fetch('/draw/circle')">Draw Random Circle</button>

  <div class="upload">
    <h3>Upload 1-bit or 2-bit RAW Image</h3>
    <p>Size: %WIDTH%x%HEIGHT% (bytes: %BYTES%)</p>
    <form method="POST" action="/upload" enctype="multipart/form-data">
      <input type="file" name="image" accept=".bin,.raw">
      <input type="submit" value="Upload">
    </form>
  </div>
</body>
</html>
)rawliteral";

    html.replace("%WIDTH%", String(EPD_WIDTH));
    html.replace("%HEIGHT%", String(EPD_HEIGHT));
    html.replace("%BYTES%", String(FB_SIZE));

    server.send(200, "text/html", html);
}

void handleClear() {
    if (framebuffer) {
        memset(framebuffer, 0xFF, FB_SIZE);
        epd_poweron();
        epd_clear();
        epd_poweroff();
    }
    server.send(200, "text/plain", "OK");
}

void handleDrawLine() {
    if (framebuffer) {
        epd_poweron();
        epd_draw_hline(10, random(10, EPD_HEIGHT), EPD_WIDTH - 20, 0, framebuffer);
        epd_draw_grayscale_image(epd_full_screen(), framebuffer);
        epd_poweroff();
    }
    server.send(200, "text/plain", "Line drawn");
}

void handleDrawRect() {
    if (framebuffer) {
        epd_poweron();
        epd_draw_rect(10, random(10, EPD_HEIGHT), random(10, 60), random(10, 120), 0, framebuffer);
        epd_draw_grayscale_image(epd_full_screen(), framebuffer);
        epd_poweroff();
    }
    server.send(200, "text/plain", "Rect drawn");
}

void handleDrawCircle() {
    if (framebuffer) {
        epd_poweron();
        epd_draw_circle(random(10, EPD_WIDTH), random(10, EPD_HEIGHT), 20, 0, framebuffer);
        epd_draw_grayscale_image(epd_full_screen(), framebuffer);
        epd_poweroff();
    }
    server.send(200, "text/plain", "Circle drawn");
}

void handleUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Upload: %s\n", upload.filename.c_str());
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (framebuffer && upload.currentSize <= FB_SIZE) {
            memcpy(framebuffer + upload.totalSize - upload.currentSize, upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (framebuffer) {
            epd_poweron();
            epd_draw_grayscale_image(epd_full_screen(), framebuffer);
            epd_poweroff();
        }
        Serial.printf("Upload complete, size: %d\n", upload.totalSize);
    }
    server.send(200, "text/plain", "Upload complete");
}

void notFound() {
    server.send(404, "text/plain", "Not found");
}

// ===== Setup =====
void setup() {
    Serial.begin(115200);
    delay(1000);

    // 初始化 framebuffer
    framebuffer = (uint8_t *)ps_calloc(1, FB_SIZE);
    if (!framebuffer) {
        Serial.println("PSRAM alloc failed!");
        while (1) delay(100);
    }
    memset(framebuffer, 0xFF, FB_SIZE);

    // 初始化 EPD
    epd_init();
    epd_poweron();
    epd_clear();
    epd_poweroff();

    // 啟動 Wi-Fi AP
    WiFi.softAP(ssid, password);
    IPAddress IP = WiFi.softAPIP();
    String ipStr = "http://" + IP.toString() + ":80";

    Serial.println("AP started");
    Serial.print("IP address: ");
    Serial.println(ipStr);

    // 在 EPD 左上角顯示 IP（需有文字繪製功能）
    epd_poweron();
    // 假設有 epd_draw_string(x, y, text, color, fb)
    // 若無，可改用圖形方式模擬，或先跳過
    // 例如：epd_draw_string(0, 0, ipStr.c_str(), 0, framebuffer);
    // epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();

    // 設定 Web Server 路由
    server.on("/", HTTP_GET, handleRoot);
    server.on("/clear", HTTP_GET, handleClear);
    server.on("/draw/line", HTTP_GET, handleDrawLine);
    server.on("/draw/rect", HTTP_GET, handleDrawRect);
    server.on("/draw/circle", HTTP_GET, handleDrawCircle);
    server.on("/upload", HTTP_POST, []() {
        server.send(200);
    }, handleUpload);

    server.onNotFound(notFound);
    server.begin();
}

// ===== Loop =====
void loop() {
    server.handleClient();
    delay(1); // 讓出 CPU
}