/**
 * EPD Web Controller via Wi-Fi AP
 * - Starts AP mode
 * - Shows IP:Port on EPD top-left
 * - Web interface to draw shapes or upload image
 *
 * 🔧 DEBUG VERSION - Enhanced with comprehensive logging
 *
 * 📝 日誌功能說明：
 * - [INIT] 初始化階段日誌
 * - [WIFI] WiFi 連線相關日誌
 * - [SERVER] 網頁伺服器相關日誌
 * - [REQUEST] HTTP 請求處理日誌
 * - [DISPLAY] 電子紙顯示操作日誌
 * - [DRAW] 繪圖操作日誌
 * - [CANVAS] Canvas 數據處理日誌
 * - [TEXT] 文字繪製日誌
 * - [CLEAR] 清除操作日誌
 * - [MEMORY] 記憶體使用狀況日誌
 * - [ERROR] 錯誤訊息日誌
 * - [DEBUG] 除錯詳細資訊日誌
 * - [OK] 操作成功完成日誌
 * - [404] 404 錯誤請求日誌
 *
 * 🚀 使用方式：
 * 1. 設定 Serial Monitor 波特率為 115200
 * 2. 上傳程式並開啟 Serial Monitor
 * 3. 觀察初始化過程的詳細日誌
 * 4. 使用網頁功能時觀察對應的日誌輸出
 * 5. 遇到問題時查看相關的錯誤日誌
 *
 * ==========================================
 * LilyGo T5 EPD47 網頁控制器
 * ==========================================
 *
 * 程式功能：
 * 這是一個電子紙網頁控制器程式，提供多種繪圖和顯示功能：
 *
 * 🌐 網路功能：
 * - WiFi 熱點模式（AP Mode）
 * - 內建 HTTP 網頁伺服器
 * - 圖形繪製網頁介面
 * - 圖片上傳與顯示功能
 *
 * 🎨 繪圖功能：
 * - 線條、矩形、圓形繪製
 * - 文字顯示功能
 * - 圖片上傳與顯示
 * - 即時網頁控制介面
 *
 * 📱 顯示功能：
 * - 4.7 吋電子紙顯示
 * - 2 位元灰階顯示
 * - 即時畫面更新
 * - IP 位址資訊顯示
 *
 * 硬體需求：
 * - LilyGo T5 EPD47 開發板
 * - ESP32-S3 處理器
 * - 16MB PSRAM（必須啟用）
 * - 4.7 吋電子紙顯示器
 *
 * 使用方式：
 * 1. 上傳程式到開發板
 * 2. 連線到 "EPD-Controller" WiFi 熱點（密碼：12345678）
 * 3. 開啟瀏覽器存取顯示的 IP 位址
 * 4. 透過網頁介面控制繪圖
 *
 * 注意事項：
 * - 需要在 Arduino IDE 中啟用 PSRAM
 * - 大型程式，編譯時間較長
 * - 支援多人同時連線控制
 */

#include <Arduino.h>    // Arduino 核心函式庫
#include <WiFi.h>       // WiFi 功能函式庫
#include <WebServer.h>  // HTTP 網頁伺服器函式庫
#include "epd_driver.h" // 電子紙驅動程式庫

// 移除可能有問題的 utilities.h
// #include "utilities.h"

// ===== 日誌輔助函數 =====
void debugLog(const String &tag, const String &message)
{
  Serial.print("[");
  Serial.print(tag);
  Serial.print("] ");
  Serial.println(message);
}

void debugLogf(const String &tag, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  Serial.print("[");
  Serial.print(tag);
  Serial.print("] ");
  Serial.printf(format, args);
  va_end(args);
}

void memoryStatus()
{
  Serial.println("[MEMORY] Status:");
  Serial.printf("  Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("  Free PSRAM: %d bytes\n", ESP.getFreePsram());
  Serial.printf("  Largest free block: %d bytes\n", ESP.getMaxAllocHeap());
}

// 使用內建的 ASCII 點陣字體顯示文字

// ===== WiFi AP 設定 =====
const char *ssid = "EPD-Controller"; // WiFi 熱點名稱
const char *password = "12345678";   // WiFi 密碼（至少8碼）

// ===== 網頁伺服器 =====
WebServer server(80); // 建立 HTTP 伺服器，監聽埠 80

// ===== HTTP 響應輔助函數 =====
void sendUTF8Response(int code, const String &contentType, const String &content)
{
  server.sendHeader("Content-Type", contentType + "; charset=UTF-8");
  server.send(code, contentType + "; charset=UTF-8", content);
}

void sendTextResponse(int code, const String &message)
{
  server.sendHeader("Content-Type", "text/plain; charset=UTF-8");
  server.send(code, "text/plain; charset=UTF-8", message);
}

void sendHtmlResponse(int code, const String &html)
{
  server.sendHeader("Content-Type", "text/html; charset=UTF-8");
  server.send(code, "text/html; charset=UTF-8", html);
}

// ===== 影像緩衝區 =====
uint8_t *framebuffer = NULL;                    // 影像緩衝區指標
const int FB_SIZE = EPD_WIDTH * EPD_HEIGHT / 2; // 2 位元灰階緩衝區大小

// ===== 智能圖片處理結構體 =====
struct ImageParams
{
  int x, y, width, height; // 位置和尺寸
  uint8_t *grayData;       // 灰階資料指標
  size_t dataSize;         // 資料大小
  float contrast;          // 對比度
  int brightness;          // 亮度
  int grayLevels;          // 灰階級數
  bool inverted;           // 是否反相
  String filename;         // 檔案名稱
  size_t filesize;         // 檔案大小
};

// ===== 函數聲明 =====
void drawLine(int x0, int y0, int x1, int y1, int color, int thickness);
void handleUploadImage();                                           // 處理圖片上傳
bool parseImageParams(const String &jsonData, ImageParams &params); // 解析圖片參數
bool renderImageToEPD(const ImageParams &params);                   // 渲染圖片到EPD
void freeImageParams(ImageParams &params);                          // 釋放圖片參數記憶體
void logImageProcessing(const ImageParams &params);                 // 記錄圖片處理日誌

// ===== 簡易 ASCII 字體 (5x7 點陣) =====
// 基本的 ASCII 字符點陣數據
const uint8_t ascii_font_5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 空格 (32)
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // ! (33)
    {0x00, 0x07, 0x00, 0x07, 0x00}, // " (34)
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // # (35)
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $ (36)
    {0x23, 0x13, 0x08, 0x64, 0x62}, // % (37)
    {0x36, 0x49, 0x55, 0x22, 0x50}, // & (38)
    {0x00, 0x05, 0x03, 0x00, 0x00}, // ' (39)
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // ( (40)
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // ) (41)
    {0x08, 0x2A, 0x1C, 0x2A, 0x08}, // * (42)
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // + (43)
    {0x00, 0x50, 0x30, 0x00, 0x00}, // , (44)
    {0x08, 0x08, 0x08, 0x08, 0x08}, // - (45)
    {0x00, 0x60, 0x60, 0x00, 0x00}, // . (46)
    {0x20, 0x10, 0x08, 0x04, 0x02}, // / (47)
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0 (48)
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1 (49)
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2 (50)
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3 (51)
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4 (52)
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5 (53)
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6 (54)
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7 (55)
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8 (56)
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9 (57)
    {0x00, 0x36, 0x36, 0x00, 0x00}, // : (58)
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ; (59)
    {0x00, 0x08, 0x14, 0x22, 0x41}, // < (60)
    {0x14, 0x14, 0x14, 0x14, 0x14}, // = (61)
    {0x41, 0x22, 0x14, 0x08, 0x00}, // > (62)
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ? (63)
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @ (64)
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A (65)
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B (66)
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C (67)
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D (68)
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E (69)
    {0x7F, 0x09, 0x09, 0x01, 0x01}, // F (70)
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G (71)
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H (72)
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I (73)
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J (74)
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K (75)
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L (76)
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, // M (77)
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N (78)
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O (79)
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P (80)
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q (81)
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R (82)
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S (83)
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T (84)
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U (85)
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V (86)
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, // W (87)
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X (88)
    {0x03, 0x04, 0x78, 0x04, 0x03}, // Y (89)
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z (90)
};

// ===== 繪製單個字符的函式 =====
void draw_char_5x7(int x, int y, char c, uint8_t color, uint8_t *fb)
{
  if (c < 32 || c > 90)
    return; // 只支援基本 ASCII

  int char_index = c - 32;

  for (int col = 0; col < 5; col++)
  {
    uint8_t column = ascii_font_5x7[char_index][col];
    for (int row = 0; row < 7; row++)
    {
      if (column & (1 << row))
      {
        int px = x + col;
        int py = y + row;
        if (px >= 0 && px < EPD_WIDTH && py >= 0 && py < EPD_HEIGHT)
        {
          // 使用 1x1 矩形來繪製像素點
          epd_fill_rect(px, py, 1, 1, color, fb);
        }
      }
    }
  }
}

// ===== 繪製放大字符的函式 =====
void draw_char_5x7_scaled(int x, int y, char c, uint8_t color, int scale, uint8_t *fb)
{
  if (c < 32 || c > 90)
    return; // 只支援基本 ASCII

  int char_index = c - 32;

  for (int col = 0; col < 5; col++)
  {
    uint8_t column = ascii_font_5x7[char_index][col];
    for (int row = 0; row < 7; row++)
    {
      if (column & (1 << row))
      {
        int px = x + col * scale;
        int py = y + row * scale;
        if (px >= 0 && px < EPD_WIDTH && py >= 0 && py < EPD_HEIGHT)
        {
          // 繪製 scale x scale 大小的矩形
          int rect_width = min(scale, EPD_WIDTH - px);
          int rect_height = min(scale, EPD_HEIGHT - py);
          epd_fill_rect(px, py, rect_width, rect_height, color, fb);
        }
      }
    }
  }
}

// ===== 簡化的 IP 顯示函式（只顯示數字和點號）=====
void draw_ip_simple(int x, int y, const char *ip_str, uint8_t color, uint8_t *fb)
{
  int current_x = x;
  int scale = 5;                        // 字體縮放倍數：3倍大小，讓字體更清楚易讀
  int char_spacing = 5 * scale + scale; // 字符間距 = 字符寬度 + 間隔

  for (int i = 0; i < strlen(ip_str); i++)
  {
    char c = ip_str[i];

    // 只繪製數字、點號、冒號和部分字母
    if ((c >= '0' && c <= '9') || c == '.' || c == ':' || c == '/' ||
        (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
    {

      // 將小寫字母轉為大寫來使用我們的字體
      if (c >= 'a' && c <= 'z')
      {
        c = c - 'a' + 'A';
      }

      // 使用放大版本的字符繪製函式，讓 IP 地址更清楚
      draw_char_5x7_scaled(current_x, y, c, color, scale, fb);
      current_x += char_spacing;
    }
  }
}

// ===== Helper: Draw text on EPD with full control =====
void epd_draw_text_advanced(const char *text, int x, int y, uint8_t textColor, uint8_t bgColor, int fontSize, uint8_t *fb)
{
  if (fb && text && strlen(text) > 0)
  {
    int base_char_width = 6;  // 基礎字符寬度
    int base_char_height = 8; // 基礎字符高度

    // 計算實際字符尺寸（可以非常大）
    int char_width = base_char_width * fontSize;
    int char_height = base_char_height * fontSize;

    int text_width = strlen(text) * char_width;
    int text_height = char_height;

    // 確保至少有部分文字在屏幕內
    if (x < EPD_WIDTH && y < EPD_HEIGHT && x + text_width > 0 && y + text_height > 0)
    {
      // 繪製背景矩形
      if (bgColor != 255) // 255 表示透明背景
      {
        epd_fill_rect(x, y, text_width, text_height, bgColor, fb);
      }

      // 逐字符繪製
      for (int i = 0; i < strlen(text); i++)
      {
        char c = text[i];
        int char_x = x + i * char_width;

        // 將小寫轉大寫
        if (c >= 'a' && c <= 'z')
        {
          c = c - 'a' + 'A';
        }

        // 繪製放大的字符
        if (c >= 32 && c <= 90) // 有效字符範圍
        {
          int char_index = c - 32;

          // 遍歷字符點陣的每一列
          for (int col = 0; col < 5; col++)
          {
            uint8_t column = ascii_font_5x7[char_index][col];

            // 遍歷字符點陣的每一行
            for (int row = 0; row < 7; row++)
            {
              if (column & (1 << row))
              {
                // 計算放大後的像素塊位置和大小
                int block_x = char_x + col * fontSize;
                int block_y = y + row * fontSize;

                // 繪製放大的像素塊（fontSize x fontSize 的矩形）
                if (block_x >= 0 && block_y >= 0 &&
                    block_x < EPD_WIDTH && block_y < EPD_HEIGHT)
                {
                  // 計算實際繪製尺寸（避免超出邊界）
                  int block_width = min(fontSize, EPD_WIDTH - block_x);
                  int block_height = min(fontSize, EPD_HEIGHT - block_y);

                  epd_fill_rect(block_x, block_y, block_width, block_height, textColor, fb);
                }
              }
            }
          }
        }
      }

      Serial.printf("Drawing text '%s' at (%d,%d) size:%d textColor:%d bgColor:%d\n",
                    text, x, y, fontSize, textColor, bgColor);
    }
    else
    {
      Serial.printf("Text completely out of bounds: (%d,%d) size:(%d,%d)\n", x, y, text_width, text_height);
    }
  }
}

// ===== Web Handlers =====

void handleRoot()
{
  String clientIP = server.client().remoteIP().toString();
  Serial.println("[REQUEST] handleRoot - Serving main page");
  Serial.printf("[CLIENT] Request from IP: %s\n", clientIP.c_str());
  Serial.printf("[MEMORY] Free heap before page serve: %d bytes\n", ESP.getFreeHeap());

  unsigned long startTime = millis();
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>EPD Controller</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; padding: 10px; background: #f5f5f5; }
    button { margin:5px; padding:10px; font-size:16px; border: none; border-radius: 4px; cursor: pointer; }
    button:hover { opacity: 0.8; }
    button:disabled { opacity: 0.5; cursor: not-allowed; }
    .upload, .text-control { margin-top:20px; padding:15px; border:1px solid #ccc; border-radius:8px; background: white; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    .form-row { margin:10px 0; display: flex; align-items: center; flex-wrap: wrap; }
    label { display:inline-block; width:80px; font-weight: bold; margin-right: 10px; }
    input[type="text"], input[type="number"] { padding:8px; margin:5px; border: 1px solid #ddd; border-radius: 4px; }
    input[type="range"] { width:200px; margin: 0 10px; }
    input[type="file"] { padding: 8px; border: 2px dashed #ccc; border-radius: 4px; width: 100%; }
    input[type="file"]:hover { border-color: #4CAF50; }
    .color-value { font-weight:bold; color: #2196F3; margin-left: 10px; }
    
    /* 智能圖片控制專用樣式 */
    .upload h3 { color: #333; border-bottom: 2px solid #4CAF50; padding-bottom: 8px; }
    .upload h4 { color: #666; margin: 20px 0 10px 0; }
    #imagePreviewCanvas { 
      max-width: 100%; 
      height: auto; 
      border: 2px solid #333; 
      border-radius: 4px;
      background: white;
      box-shadow: 0 4px 8px rgba(0,0,0,0.2);
    }
    #positionInfo { 
      font-family: monospace; 
      background: #f0f0f0; 
      padding: 8px; 
      border-radius: 4px; 
      margin-top: 10px;
      border-left: 4px solid #2196F3;
    }
    #uploadProgress {
      background: #f9f9f9;
      border: 1px solid #ddd;
      border-radius: 8px;
      padding: 15px;
      margin-top: 15px;
    }
    #progressBar {
      background: linear-gradient(90deg, #4CAF50 0%, #45a049 100%);
      height: 24px;
      border-radius: 12px;
      transition: width 0.5s ease;
      position: relative;
      overflow: hidden;
    }
    #progressBar::after {
      content: '';
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background: linear-gradient(45deg, transparent 33%, rgba(255,255,255,.3) 33%, rgba(255,255,255,.3) 66%, transparent 66%);
      animation: progressShine 2s infinite;
    }
    @keyframes progressShine {
      0% { transform: translateX(-100%); }
      100% { transform: translateX(100%); }
    }
    small { color: #666; font-style: italic; margin-left: 10px; }
    
    /* 按鈕樣式增強 */
    button[onclick*="fit"] { background: #2196F3; color: white; }
    button[onclick*="keep"] { background: #FF9800; color: white; }
    button[onclick*="center"] { background: #9C27B0; color: white; }
    button[onclick*="invert"] { background: #607D8B; color: white; }
    button[onclick*="reset"] { background: #795548; color: white; }
    #sendImageBtn { 
      background: linear-gradient(45deg, #4CAF50, #45a049); 
      color: white; 
      font-weight: bold;
      border: none;
      padding: 15px 30px;
      font-size: 18px;
      border-radius: 8px;
      box-shadow: 0 4px 8px rgba(76, 175, 80, 0.3);
      transition: all 0.3s ease;
    }
    #sendImageBtn:hover:not(:disabled) { 
      transform: translateY(-2px);
      box-shadow: 0 6px 12px rgba(76, 175, 80, 0.4);
    }
    
    /* Canvas 手寫板樣式 */
    .canvas-container { 
      margin: 20px 0; 
      padding: 15px; 
      border: 1px solid #ccc; 
      border-radius: 5px;
      text-align: center;
    }
    #drawingCanvas {
      border: 2px solid #333;
      cursor: crosshair;
      width: 100%;
      max-width: 800px;
      background-color: white;
    }
    .canvas-controls {
      margin: 10px 0;
    }
    .canvas-controls button {
      margin: 2px 5px;
      padding: 8px 15px;
    }
  </style>
</head>
<body>
  <h2>EPD Web Controller</h2>
  <button onclick="fetch('/clear')">Clear Screen</button>
  <button onclick="fetch('/draw/line')">Draw Random Line</button>
  <button onclick="fetch('/draw/rect')">Draw Random Rect</button>
  <button onclick="fetch('/draw/circle')">Draw Random Circle</button>

  <div class="canvas-container">
    <h3>手寫繪圖板 (小尺寸測試版)</h3>
    <p>注意：使用100x60小尺寸確保數據傳輸穩定，將自動縮放到EPD</p>
    <canvas id="drawingCanvas" width="100" height="60"></canvas>
    <div class="canvas-controls">
      <button onclick="clearCanvas()">清除畫布</button>
      <button onclick="sendCanvasToEPD()">同步到EPD</button>
      <label>筆刷大小:</label>
      <input type="range" id="brushSize" min="1" max="20" value="3" oninput="updateBrushSize()">
      <span class="color-value" id="brushSizeValue">3</span>
      <label>筆刷顏色:</label>
      <input type="range" id="brushColor" min="0" max="15" value="0" oninput="updateBrushColor()">
      <span class="color-value" id="brushColorValue">0 (黑色)</span>
    </div>
  </div>

  <div class="text-control">
    <h3>文字控制</h3>
    <div class="form-row">
      <label>文字:</label>
      <input type="text" id="text" placeholder="輸入文字" value="Hello EPD">
    </div>
    <div class="form-row">
      <label>X座標:</label>
      <input type="number" id="x" min="0" max="%WIDTH%" value="50">
      <label>Y座標:</label>
      <input type="number" id="y" min="0" max="%HEIGHT%" value="50">
    </div>
    <div class="form-row">
      <label>字體大小:</label>
      <input type="range" id="fontSize" min="1" max="100" value="2" oninput="updateFontSizeValue()">
      <span class="color-value" id="fontSizeValue">2</span>
    </div>
    <div class="form-row">
      <label>文字顏色:</label>
      <input type="range" id="textColor" min="0" max="15" value="0" oninput="updateTextColorValue()">
      <span class="color-value" id="textColorValue">0 (黑色)</span>
    </div>
    <div class="form-row">
      <label>背景顏色:</label>
      <input type="range" id="bgColor" min="0" max="16" value="16" oninput="updateBgColorValue()">
      <span class="color-value" id="bgColorValue">透明</span>
    </div>
    
    <div class="form-row">
      <label>預設樣式:</label>
      <button onclick="setStyle('title')" style="margin:2px; padding:5px;">標題</button>
      <button onclick="setStyle('normal')" style="margin:2px; padding:5px;">正文</button>
      <button onclick="setStyle('highlight')" style="margin:2px; padding:5px;">強調</button>
      <button onclick="setStyle('subtitle')" style="margin:2px; padding:5px;">副標題</button>
    </div>
    
    <button onclick="drawText()">繪製文字</button>
    
    <h4>多行文字</h4>
    <div class="form-row">
      <label>多行文字:</label>
      <textarea id="multiText" placeholder="輸入多行文字，用分號(;)分隔" rows="3" style="width:300px;">第一行文字;第二行文字;第三行文字</textarea>
    </div>
    <div class="form-row">
      <label>起始X:</label>
      <input type="number" id="startX" min="0" max="%WIDTH%" value="50">
      <label>起始Y:</label>
      <input type="number" id="startY" min="0" max="%HEIGHT%" value="100">
    </div>
    <div class="form-row">
      <label>行高:</label>
      <input type="number" id="lineHeight" min="10" max="100" value="25">
    </div>
    <button onclick="drawMultiText()">繪製多行文字</button>
  </div>

  <div class="text-control">
    <h3>進階繪圖控制</h3>
    
    <h4>畫線控制</h4>
    <div class="form-row">
      <label>起點X:</label>
      <input type="number" id="lineX1" min="0" max="%WIDTH%" value="50">
      <label>起點Y:</label>
      <input type="number" id="lineY1" min="0" max="%HEIGHT%" value="50">
    </div>
    <div class="form-row">
      <label>終點X:</label>
      <input type="number" id="lineX2" min="0" max="%WIDTH%" value="200">
      <label>終點Y:</label>
      <input type="number" id="lineY2" min="0" max="%HEIGHT%" value="100">
    </div>
    <div class="form-row">
      <label>線條顏色:</label>
      <input type="range" id="lineColor" min="0" max="15" value="0" oninput="updateLineColorValue()">
      <span class="color-value" id="lineColorValue">0 (黑色)</span>
    </div>
    <div class="form-row">
      <label>線條粗細:</label>
      <input type="range" id="lineThickness" min="1" max="20" value="1" oninput="updateLineThicknessValue()">
      <span class="color-value" id="lineThicknessValue">1</span>
    </div>
    <button onclick="drawLineAdvanced()">繪製線條</button>

    <h4>畫圓控制</h4>
    <div class="form-row">
      <label>圓心X:</label>
      <input type="number" id="circleX" min="0" max="%WIDTH%" value="200">
      <label>圓心Y:</label>
      <input type="number" id="circleY" min="0" max="%HEIGHT%" value="200">
    </div>
    <div class="form-row">
      <label>半徑:</label>
      <input type="number" id="circleRadius" min="1" max="500" value="50">
    </div>
    <div class="form-row">
      <label>外框:</label>
      <input type="checkbox" id="circleBorder" checked>
      <label>外框顏色:</label>
      <input type="range" id="circleBorderColor" min="0" max="15" value="0" oninput="updateCircleBorderColorValue()">
      <span class="color-value" id="circleBorderColorValue">0 (黑色)</span>
    </div>
    <div class="form-row">
      <label>外框粗細:</label>
      <input type="range" id="circleBorderThickness" min="1" max="10" value="1" oninput="updateCircleBorderThicknessValue()">
      <span class="color-value" id="circleBorderThicknessValue">1</span>
    </div>
    <div class="form-row">
      <label>填滿:</label>
      <input type="checkbox" id="circleFill">
      <label>填充顏色:</label>
      <input type="range" id="circleFillColor" min="0" max="15" value="15" oninput="updateCircleFillColorValue()">
      <span class="color-value" id="circleFillColorValue">15 (白色)</span>
    </div>
    <button onclick="drawCircleAdvanced()">繪製圓形</button>

    <h4>畫矩形控制</h4>
    <div class="form-row">
      <label>左上X:</label>
      <input type="number" id="rectX" min="0" max="%WIDTH%" value="100">
      <label>左上Y:</label>
      <input type="number" id="rectY" min="0" max="%HEIGHT%" value="150">
    </div>
    <div class="form-row">
      <label>寬度:</label>
      <input type="number" id="rectWidth" min="1" max="%WIDTH%" value="100">
      <label>高度:</label>
      <input type="number" id="rectHeight" min="1" max="%HEIGHT%" value="80">
    </div>
    <div class="form-row">
      <label>外框:</label>
      <input type="checkbox" id="rectBorder" checked>
      <label>外框顏色:</label>
      <input type="range" id="rectBorderColor" min="0" max="15" value="0" oninput="updateRectBorderColorValue()">
      <span class="color-value" id="rectBorderColorValue">0 (黑色)</span>
    </div>
    <div class="form-row">
      <label>外框粗細:</label>
      <input type="range" id="rectBorderThickness" min="1" max="10" value="1" oninput="updateRectBorderThicknessValue()">
      <span class="color-value" id="rectBorderThicknessValue">1</span>
    </div>
    <div class="form-row">
      <label>填滿:</label>
      <input type="checkbox" id="rectFill">
      <label>填充顏色:</label>
      <input type="range" id="rectFillColor" min="0" max="15" value="15" oninput="updateRectFillColorValue()">
      <span class="color-value" id="rectFillColorValue">15 (白色)</span>
    </div>
    <button onclick="drawRectAdvanced()">繪製矩形</button>
  </div>

  <div class="upload">
    <h3>Upload 1-bit or 2-bit RAW Image</h3>
    <p>Size: %WIDTH%x%HEIGHT% (bytes: %BYTES%)</p>
    <form method="POST" action="/upload" enctype="multipart/form-data">
      <input type="file" name="image" accept=".bin,.raw">
      <input type="submit" value="Upload">
    </form>
  </div>

  <!-- EPD 圖片轉換器整合 -->
  <div class="text-control">
    <h3>🖼️ EPD 圖片轉換器</h3>
    <p>將任何圖片轉換為 EPD 可用的灰階數據，支援動態尺寸調整</p>
    
    <style>
      .converter-container {
        background: #f8f9fa;
        padding: 20px;
        border-radius: 8px;
        margin: 20px 0;
        border: 1px solid #e9ecef;
      }
      
      .size-settings {
        background: #e3f2fd;
        padding: 15px;
        border-radius: 6px;
        margin-bottom: 20px;
        border-left: 4px solid #2196F3;
      }
      
      .size-controls {
        display: flex;
        align-items: center;
        justify-content: center;
        gap: 15px;
        flex-wrap: wrap;
        margin: 10px 0;
      }
      
      .size-input-group {
        display: flex;
        align-items: center;
        gap: 8px;
      }
      
      .size-select {
        padding: 6px 10px;
        border: 1px solid #ced4da;
        border-radius: 4px;
        background: white;
        min-width: 70px;
      }
      
      .multiply-symbol {
        font-size: 18px;
        font-weight: bold;
        color: #6c757d;
      }
      
      .preset-buttons {
        display: flex;
        gap: 8px;
        flex-wrap: wrap;
        justify-content: center;
        margin-top: 10px;
      }
      
      .preset-btn {
        background: #6c757d;
        color: white;
        border: none;
        padding: 5px 10px;
        border-radius: 4px;
        cursor: pointer;
        font-size: 11px;
        transition: background 0.3s;
      }
      
      .preset-btn:hover {
        background: #5a6268;
      }
      
      .preset-btn.active {
        background: #4CAF50;
      }
      
      .canvas-container {
        text-align: center;
        margin: 20px 0;
        border: 2px dashed #ddd;
        padding: 15px;
        border-radius: 6px;
        background: white;
      }
      
      #imageCanvas {
        max-width: 100%;
        border: 1px solid #ccc;
        background: #f9f9f9;
      }
      
      .upload-section {
        text-align: center;
        margin: 15px 0;
      }
      
      .file-input-wrapper {
        display: inline-block;
        background: #4CAF50;
        color: white;
        padding: 10px 20px;
        border-radius: 4px;
        cursor: pointer;
        transition: background 0.3s;
      }
      
      .file-input-wrapper:hover {
        background: #45a049;
      }
      
      .converter-textarea {
        width: 100%;
        height: 150px;
        border: 1px solid #ddd;
        border-radius: 4px;
        padding: 8px;
        font-family: 'Courier New', monospace;
        font-size: 11px;
        resize: vertical;
        background: #fafafa;
      }
      
      .converter-info {
        margin: 10px 0;
        padding: 10px;
        background: #fff3cd;
        border-radius: 4px;
        border-left: 4px solid #ffc107;
        font-size: 12px;
      }
    </style>
    
    <div class="converter-container">
      <div class="size-settings">
        <h4>📐 圖片尺寸設定</h4>
        <div class="size-controls">
          <div class="size-input-group">
            <label>寬度:</label>
            <select id="widthSelect" class="size-select">
              <option value="4">4</option>
              <option value="8">8</option>
              <option value="16">16</option>
              <option value="32">32</option>
              <option value="64">64</option>
              <option value="128">128</option>
              <option value="200">200</option>
              <option value="320">320</option>
              <option value="480" selected>480</option>
              <option value="540">540</option>
              <option value="600">600</option>
              <option value="800">800</option>
              <option value="960">960</option>
            </select>
          </div>
          
          <span class="multiply-symbol">×</span>
          
          <div class="size-input-group">
            <label>高度:</label>
            <select id="heightSelect" class="size-select">
              <option value="4">4</option>
              <option value="8">8</option>
              <option value="16">16</option>
              <option value="32">32</option>
              <option value="64">64</option>
              <option value="128">128</option>
              <option value="200">200</option>
              <option value="320">320</option>
              <option value="480">480</option>
              <option value="540">540</option>
              <option value="600">600</option>
              <option value="800" selected>800</option>
              <option value="960">960</option>
            </select>
          </div>
        </div>
        
        <div class="preset-buttons">
          <button class="preset-btn" onclick="setPresetSize(4, 4)">4×4</button>
          <button class="preset-btn" onclick="setPresetSize(8, 8)">8×8</button>
          <button class="preset-btn" onclick="setPresetSize(16, 16)">16×16</button>
          <button class="preset-btn" onclick="setPresetSize(32, 32)">32×32</button>
          <button class="preset-btn" onclick="setPresetSize(64, 64)">64×64</button>
          <button class="preset-btn" onclick="setPresetSize(128, 128)">128×128</button>
          <button class="preset-btn" onclick="setPresetSize(320, 240)">320×240</button>
          <button class="preset-btn active" onclick="setPresetSize(480, 800)">480×800</button>
          <button class="preset-btn" onclick="setPresetSize(540, 960)">540×960</button>
          <button class="preset-btn" onclick="setPresetSize(800, 600)">800×600</button>
        </div>
      </div>
      
      <div class="upload-section">
        <div class="file-input-wrapper" onclick="document.getElementById('imageInput').click()">
          <input type="file" id="imageInput" accept="image/*" style="display: none;">
          🖼️ 選擇圖片檔案
        </div>
      </div>
      
      <div class="converter-info">
        <strong>使用說明：</strong>
        <ul style="margin: 5px 0; padding-left: 20px;">
          <li>選擇圖片後會自動轉換為指定尺寸的灰階圖片</li>
          <li>灰階值範圍：0-15 (0=黑色, 15=白色)</li>
          <li>在下方數據框雙擊可複製所有數據</li>
          <li>可直接將數據貼到 "灰階圖片數據傳送" 功能使用</li>
        </ul>
      </div>
      
      <div class="canvas-container">
        <canvas id="imageCanvas" width="480" height="800"></canvas>
        <div id="canvasInfo" style="margin-top: 10px; color: #666; font-size: 12px;">
          請先選擇圖片檔案
        </div>
      </div>
      
      <div style="margin-top: 15px;">
        <label style="display: block; margin-bottom: 8px; font-weight: bold;" id="dataLabel">
          灰階數據 (480×800 = 384,000 個值)：
        </label>
        <textarea id="converterDataTextarea" class="converter-textarea" placeholder="轉換後的灰階數據將顯示在這裡..." readonly></textarea>
        <div style="margin-top: 5px; font-size: 11px; color: #666; font-style: italic;">
          💡 雙擊文字框可複製所有數據到剪貼簿
        </div>
      </div>
      
      <div style="margin-top: 15px; text-align: center;">
        <button onclick="resetConverter()" style="background: #6c757d; color: white; padding: 8px 16px; border: none; border-radius: 4px; cursor: pointer; margin-right: 10px;">重置</button>
        <button onclick="downloadConverterData()" style="background: #17a2b8; color: white; padding: 8px 16px; border: none; border-radius: 4px; cursor: pointer;">下載數據</button>
      </div>
    </div>
  </div>
  <div class="text-control">
    <h3>灰階圖片數據傳送</h3>
    <p>從外部工具生成的灰階數據 (0-15，逗號分隔)</p>
    <div class="form-row">
      <label>X座標:</label>
      <input type="number" id="grayscaleX" min="0" max="%WIDTH%" value="0">
      <label>Y座標:</label>
      <input type="number" id="grayscaleY" min="0" max="%HEIGHT%" value="0">
    </div>
    <div class="form-row">
      <label>圖片寬度:</label>
      <input type="number" id="grayscaleWidth" min="1" max="%WIDTH%" value="100">
      <label>圖片高度:</label>
      <input type="number" id="grayscaleHeight" min="1" max="%HEIGHT%" value="100">
    </div>
    <div class="form-row">
      <label>灰階數據:</label>
      <textarea id="grayscaleData" placeholder="貼入灰階數據，格式: 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,..." 
      rows="6" style="width:100%; max-width:600px; font-family:monospace;"></textarea>
    </div>
    <div class="form-row">
      <button onclick="sendGrayscaleData()" style="background-color:#4CAF50; color:white; padding:10px 20px; font-size:16px;">傳送灰階圖片資料</button>
      <button onclick="clearGrayscaleData()" style="margin-left:10px;">清除數據</button>
    </div>
    <div class="form-row">
      <small>💡 提示: 從外部圖片轉換工具複製數據，設定好位置和尺寸後點擊傳送</small>
    </div>
  </div>
  

  <script>
    // ===== DOM 安全訪問輔助函數 =====
    function safeGetElement(id) {
      const element = document.getElementById(id);
      if (!element) {
        console.warn('Element not found:', id);
      }
      return element;
    }
    
    function safeGetValue(id, defaultValue = '') {
      const element = safeGetElement(id);
      return element ? element.value : defaultValue;
    }
    
    function safeSetValue(id, value) {
      const element = safeGetElement(id);
      if (element) {
        element.value = value;
        return true;
      }
      return false;
    }
    
    function safeSetText(id, text) {
      const element = safeGetElement(id);
      if (element) {
        element.textContent = text;
        return true;
      }
      return false;
    }
    
    // Canvas 繪圖變數
    let canvas = null;
    let ctx = null;
    let isDrawing = false;
    let lastX = 0;
    let lastY = 0;
    let currentBrushSize = 3;
    let currentBrushColor = 0;
    
    // 路徑記錄變數
    let strokePaths = [];
    let currentPath = null;
    
    // 初始化 Canvas
    function initCanvas() {
      canvas = document.getElementById('drawingCanvas');
      ctx = canvas.getContext('2d');
      
      // 設置 canvas 實際尺寸與顯示尺寸
      const epdRatio = %HEIGHT% / %WIDTH%; // EPD的高寬比
      const containerWidth = Math.min(800, window.innerWidth - 100);
      canvas.style.width = containerWidth + 'px';
      canvas.style.height = (containerWidth * epdRatio) + 'px';
      
      // 設置繪圖屬性
      ctx.fillStyle = 'white';
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      ctx.lineCap = 'round';
      ctx.lineJoin = 'round';
      
      // 設置預設繪圖樣式
      ctx.strokeStyle = 'black';
      ctx.lineWidth = 3;
      
      console.log('Canvas initialized:', canvas.width, 'x', canvas.height);
      
      // 滑鼠事件
      canvas.addEventListener('mousedown', startDrawing);
      canvas.addEventListener('mousemove', draw);
      canvas.addEventListener('mouseup', stopDrawing);
      canvas.addEventListener('mouseout', stopDrawing);
      
      // 觸控事件
      canvas.addEventListener('touchstart', handleTouch);
      canvas.addEventListener('touchmove', handleTouch);
      canvas.addEventListener('touchend', stopDrawing);
    }
    
    function startDrawing(e) {
      isDrawing = true;
      [lastX, lastY] = getMousePos(e);
      
      // 開始新的路徑記錄
      currentPath = {
        color: currentBrushColor,
        size: currentBrushSize,
        points: [[Math.round(lastX), Math.round(lastY)]]
      };
    }
    
    function draw(e) {
      if (!isDrawing) return;
      
      const [currentX, currentY] = getMousePos(e);
      
      ctx.globalCompositeOperation = 'source-over';
      ctx.strokeStyle = getCanvasColor(currentBrushColor);
      ctx.lineWidth = currentBrushSize;
      
      ctx.beginPath();
      ctx.moveTo(lastX, lastY);
      ctx.lineTo(currentX, currentY);
      ctx.stroke();
      
      // 記錄路徑點
      if (currentPath) {
        currentPath.points.push([Math.round(currentX), Math.round(currentY)]);
      }
      
      [lastX, lastY] = [currentX, currentY];
    }
    
    function stopDrawing() {
      if (isDrawing && currentPath && currentPath.points.length > 1) {
        // 完成路徑記錄
        strokePaths.push(currentPath);
        console.log('Path recorded:', currentPath);
      }
      isDrawing = false;
      currentPath = null;
    }
    
    function getMousePos(e) {
      const rect = canvas.getBoundingClientRect();
      const scaleX = canvas.width / rect.width;
      const scaleY = canvas.height / rect.height;
      
      return [
        (e.clientX - rect.left) * scaleX,
        (e.clientY - rect.top) * scaleY
      ];
    }
    
    function handleTouch(e) {
      e.preventDefault();
      const touch = e.touches[0];
      const mouseEvent = new MouseEvent(e.type === 'touchstart' ? 'mousedown' : 
                                       e.type === 'touchmove' ? 'mousemove' : 'mouseup', {
        clientX: touch.clientX,
        clientY: touch.clientY
      });
      canvas.dispatchEvent(mouseEvent);
    }
    
    function getCanvasColor(colorValue) {
      // 將 0-15 的顏色值轉換為 canvas 顏色
      // 0=黑色, 15=白色
      const gray = Math.round(colorValue * 255 / 15);
      return `rgb(${gray}, ${gray}, ${gray})`;
    }
    
    function clearCanvas() {
      ctx.fillStyle = 'white';
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      console.log('Canvas cleared');
    }
    
    function updateBrushSize() {
      currentBrushSize = safeGetValue('brushSize');
      safeSetText('brushSizeValue', currentBrushSize);
    }
    
    function updateBrushColor() {
      currentBrushColor = safeGetValue('brushColor');
      const colorName = getColorName(currentBrushColor);
      safeSetText('brushColorValue', currentBrushColor + ' (' + colorName + ')');
    }
    
    function sendCanvasToEPD() {
      console.log('sendCanvasToEPD called');
      
      // 檢查 canvas 是否已初始化
      if (!canvas || !ctx) {
        console.error('Canvas not initialized');
        alert('Canvas 尚未初始化');
        return;
      }
      
      console.log('Canvas size:', canvas.width, 'x', canvas.height);
      console.log('Total recorded paths:', strokePaths.length);
      
      // 優先使用路徑格式
      if (strokePaths.length > 0) {
        sendStrokePaths();
      } else {
        console.log('No paths recorded, using pixel analysis fallback');
        sendCanvasAsPixels();
      }
    }
    
    // 發送路徑數據 (壓縮格式)
    function sendStrokePaths() {
      console.log('Sending stroke paths');
      
      const pathStrings = [];
      for (let i = 0; i < strokePaths.length; i++) {
        const path = strokePaths[i];
        const pointsStr = path.points.map(p => p[0] + ',' + p[1]).join('|');
        const pathStr = 'P:' + path.color + ':' + path.size + ':' + pointsStr;
        pathStrings.push(pathStr);
      }
      
      const dataStr = pathStrings.join(';');
      console.log('Path data length:', dataStr.length);
      console.log('Path data preview:', dataStr.substring(0, 200));
      console.log('Path data ending:', dataStr.length > 100 ? dataStr.substring(dataStr.length - 100) : dataStr);
      console.log('Number of paths generated:', pathStrings.length);
      
      // 檢查每個路徑的完整性
      for (let i = 0; i < Math.min(pathStrings.length, 3); i++) {
        console.log('Path', i, 'sample:', pathStrings[i].substring(0, 100));
      }
      
      // 檢查數據大小
      if (dataStr.length > 50000) {
        console.warn('Path data too large:', dataStr.length, 'chars');
        alert('警告：路徑數據過大 (' + dataStr.length + ' 字符)，請減少繪圖內容。');
        return;
      }
      
      // 準備 POST 數據
      const postData = 'width=' + canvas.width + '&height=' + canvas.height + '&paths=1&data=' + encodeURIComponent(dataStr);
      console.log('POST data length:', postData.length);
      console.log('Encoded data preview (first 200):', postData.substring(0, 200));
      console.log('Encoded data ending (last 100):', postData.length > 100 ? postData.substring(postData.length - 100) : postData);
      
      // 檢查 POST 數據大小限制
      if (postData.length > 8000) {
        console.warn('POST data approaching ESP32 limits:', postData.length, 'chars');
        alert('警告：POST數據接近ESP32限制 (' + postData.length + ' 字符)，可能會被截斷！');
        // 仍然嘗試發送，但用戶已被警告
      }
      
      // 發送到服務器
      fetch('/draw/canvas', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: postData
      })
      .then(response => response.text())
      .then(data => {
        console.log('Response:', data);
        alert('路徑數據已送出！');
      })
      .catch(error => {
        console.error('Error:', error);
        alert('發送失敗：' + error);
      });
    }
    
    // 像素分析備用方案
    function sendCanvasAsPixels() {
      
      // 將 canvas 轉換為圖像數據並發送到 EPD
      const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
      const pixelData = imageData.data;
      
      console.log('Canvas size:', canvas.width, 'x', canvas.height);
      console.log('Total pixels:', canvas.width * canvas.height);
      console.log('ImageData length:', pixelData.length);
      
      // 使用壓縮格式：只傳送非白色像素的位置和顏色
      const compressedData = [];
      for (let i = 0; i < pixelData.length; i += 4) {
        const gray = pixelData[i];
        const epdGray = Math.round(gray * 15 / 255);
        
        // 只記錄非白色像素 (值不等於15)
        if (epdGray !== 15) {
          const pixelIndex = i / 4;
          const x = pixelIndex % canvas.width;
          const y = Math.floor(pixelIndex / canvas.width);
          compressedData.push(x + ',' + y + ',' + epdGray);
        }
      }
      
      console.log('Total pixels:', canvas.width * canvas.height);
      console.log('Non-white pixels:', compressedData.length);
      console.log('Compression ratio:', ((compressedData.length / (canvas.width * canvas.height)) * 100).toFixed(2) + '%');
      
      // 將壓縮數據編碼為字串
      const dataStr = compressedData.join(';');
      
      console.log('Compressed data string length:', dataStr.length);
      console.log('First 100 chars of compressed data:', dataStr.substring(0, 100));
      
      // 檢查數據大小
      if (dataStr.length > 10000) {
        console.warn('Warning: Compressed data still large:', dataStr.length, 'chars');
        if (dataStr.length > 50000) {
          alert('警告：壓縮後數據仍過大 (' + dataStr.length + ' 字符)，請減少繪圖內容。');
          return;
        }
      }
      
      // 準備 POST 數據 (使用壓縮格式)
      const postData = 'width=' + canvas.width + '&height=' + canvas.height + '&compressed=1&data=' + encodeURIComponent(dataStr);
      console.log('POST data length:', postData.length);
      console.log('POST data preview:', postData.substring(0, 200));
      
      // 最終數據大小檢查
      if (postData.length > 20000) {
        console.error('POST data too large:', postData.length, 'chars');
        alert('錯誤：POST數據過大 (' + postData.length + ' 字符)，請減少繪圖內容。');
        return;
      }
      
      // 發送到服務器
      fetch('/draw/canvas', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: postData
      })
      .then(response => {
        console.log('Response status:', response.status);
        console.log('Response headers:', response.headers);
        return response.text();
      })
      .then(data => {
        console.log('Server response:', data);
        alert('Canvas 已同步到 EPD: ' + data);
      })
      .catch(error => {
        console.error('Error:', error);
        alert('同步失敗: ' + error.message);
      });
    }
    function updateTextColorValue() {
      const color = safeGetValue('textColor');
      if (!color) return;
      
      let colorName;
      if (color == 0) colorName = '黑色';
      else if (color <= 3) colorName = '深灰';
      else if (color <= 7) colorName = '中灰';
      else if (color <= 11) colorName = '淺灰';
      else colorName = '白色';
      
      safeSetText('textColorValue', color + ' (' + colorName + ')');
    }
    
    function updateBgColorValue() {
      const color = safeGetValue('bgColor');
      if (!color) return;
      
      let colorName;
      if (color == 16) {
        colorName = '透明';
      } else {
        if (color == 0) colorName = '黑色';
        else if (color <= 3) colorName = '深灰';
        else if (color <= 7) colorName = '中灰';
        else if (color <= 11) colorName = '淺灰';
        else colorName = '白色';
        colorName = color + ' (' + colorName + ')';
      }
      
      safeSetText('bgColorValue', colorName);
    }
    
    function updateFontSizeValue() {
      const size = safeGetValue('fontSize');
      safeSetText('fontSizeValue', size + ' (像素倍數)');
    }
    
    function updateLineColorValue() {
      const color = safeGetValue('lineColor');
      if (!color) return;
      const colorName = getColorName(color);
      safeSetText('lineColorValue', color + ' (' + colorName + ')');
    }
    
    function updateLineThicknessValue() {
      const thickness = safeGetValue('lineThickness');
      if (!thickness) return;
      safeSetText('lineThicknessValue', thickness + ' 像素');
    }
    
    function updateCircleBorderColorValue() {
      const color = safeGetValue('circleBorderColor');
      if (!color) return;
      const colorName = getColorName(color);
      safeSetText('circleBorderColorValue', color + ' (' + colorName + ')');
    }
    
    function updateCircleBorderThicknessValue() {
      const thickness = safeGetValue('circleBorderThickness');
      if (!thickness) return;
      safeSetText('circleBorderThicknessValue', thickness + ' 像素');
    }
    
    function updateCircleFillColorValue() {
      const color = safeGetValue('circleFillColor');
      if (!color) return;
      const colorName = getColorName(color);
      safeSetText('circleFillColorValue', color + ' (' + colorName + ')');
    }
    
    function updateRectBorderColorValue() {
      const color = safeGetValue('rectBorderColor');
      if (!color) return;
      const colorName = getColorName(color);
      safeSetText('rectBorderColorValue', color + ' (' + colorName + ')');
    }
    
    function updateRectBorderThicknessValue() {
      const thickness = safeGetValue('rectBorderThickness');
      if (!thickness) return;
      safeSetText('rectBorderThicknessValue', thickness + ' 像素');
    }
    
    function updateRectFillColorValue() {
      const color = safeGetValue('rectFillColor');
      if (!color) return;
      const colorName = getColorName(color);
      safeSetText('rectFillColorValue', color + ' (' + colorName + ')');
    }
    
    function getColorName(color) {
      if (color == 0) return '黑色';
      else if (color <= 3) return '深灰';
      else if (color <= 7) return '中灰';
      else if (color <= 11) return '淺灰';
      else return '白色';
    }
    
    function setStyle(styleName) {
      const styles = {
        'title': { fontSize: 4, textColor: 0, bgColor: 16 },      // 大字黑色，透明背景
        'normal': { fontSize: 2, textColor: 0, bgColor: 16 },     // 中字黑色，透明背景
        'highlight': { fontSize: 3, textColor: 15, bgColor: 0 },  // 大字白色，黑色背景
        'subtitle': { fontSize: 3, textColor: 5, bgColor: 16 }    // 大字灰色，透明背景
      };
      
      if (styles[styleName]) {
        document.getElementById('fontSize').value = styles[styleName].fontSize;
        document.getElementById('textColor').value = styles[styleName].textColor;
        document.getElementById('bgColor').value = styles[styleName].bgColor;
        
        // 更新顯示值
        updateFontSizeValue();
        updateTextColorValue();
        updateBgColorValue();
      }
    }
    
    function drawText() {
      const text = document.getElementById('text').value;
      const x = document.getElementById('x').value;
      const y = document.getElementById('y').value;
      const textColor = document.getElementById('textColor').value;
      const bgColor = document.getElementById('bgColor').value;
      const fontSize = document.getElementById('fontSize').value;
      
      if (!text.trim()) {
        alert('請輸入文字');
        return;
      }
      
      // 16 表示透明背景，轉換為 255
      const actualBgColor = bgColor == 16 ? 255 : bgColor;
      
      const url = '/draw/text?text=' + encodeURIComponent(text) + 
                  '&x=' + x + '&y=' + y + 
                  '&textColor=' + textColor + '&bgColor=' + actualBgColor + 
                  '&fontSize=' + fontSize;
      
      fetch(url)
        .then(response => response.text())
        .then(data => {
          console.log(data);
          
        })
        .catch(error => {
          console.error('Error:', error);
          alert('繪製失敗');
        });
    }
    
    function drawMultiText() {
      const texts = document.getElementById('multiText').value;
      const startX = document.getElementById('startX').value;
      const startY = document.getElementById('startY').value;
      const lineHeight = document.getElementById('lineHeight').value;
      const textColor = document.getElementById('textColor').value;
      const bgColor = document.getElementById('bgColor').value;
      const fontSize = document.getElementById('fontSize').value;
      
      if (!texts.trim()) {
        alert('請輸入多行文字');
        return;
      }
      
      // 16 表示透明背景，轉換為 255
      const actualBgColor = bgColor == 16 ? 255 : bgColor;
      
      const url = '/draw/multitext?texts=' + encodeURIComponent(texts) + 
                  '&startX=' + startX + '&startY=' + startY + 
                  '&lineHeight=' + lineHeight + 
                  '&textColor=' + textColor + '&bgColor=' + actualBgColor + 
                  '&fontSize=' + fontSize;
      
      fetch(url)
        .then(response => response.text())
        .then(data => {
          console.log(data);
          alert('多行文字已繪製: ' + data);
        })
        .catch(error => {
          console.error('Error:', error);
          alert('繪製失敗');
        });
    }
    
    function drawLineAdvanced() {
      const x1 = document.getElementById('lineX1').value;
      const y1 = document.getElementById('lineY1').value;
      const x2 = document.getElementById('lineX2').value;
      const y2 = document.getElementById('lineY2').value;
      const color = document.getElementById('lineColor').value;
      const thickness = document.getElementById('lineThickness').value;
      
      const url = '/draw/line/advanced?x1=' + x1 + '&y1=' + y1 + 
                  '&x2=' + x2 + '&y2=' + y2 + 
                  '&color=' + color + '&thickness=' + thickness;
      
      fetch(url)
        .then(response => response.text())
        .then(data => {
          console.log(data);
          alert('線條已繪製: ' + data);
        })
        .catch(error => {
          console.error('Error:', error);
          alert('繪製失敗');
        });
    }
    
    function drawCircleAdvanced() {
      const centerX = document.getElementById('circleX').value;
      const centerY = document.getElementById('circleY').value;
      const radius = document.getElementById('circleRadius').value;
      const hasBorder = document.getElementById('circleBorder').checked;
      const borderColor = document.getElementById('circleBorderColor').value;
      const borderThickness = document.getElementById('circleBorderThickness').value;
      const isFilled = document.getElementById('circleFill').checked;
      const fillColor = document.getElementById('circleFillColor').value;
      
      const url = '/draw/circle/advanced?centerX=' + centerX + '&centerY=' + centerY + 
                  '&radius=' + radius + '&hasBorder=' + hasBorder + 
                  '&borderColor=' + borderColor + '&borderThickness=' + borderThickness + 
                  '&isFilled=' + isFilled + '&fillColor=' + fillColor;
      
      fetch(url)
        .then(response => response.text())
        .then(data => {
          console.log(data);
          alert('圓形已繪製: ' + data);
        })
        .catch(error => {
          console.error('Error:', error);
          alert('繪製失敗');
        });
    }
    
    function drawRectAdvanced() {
      const x = document.getElementById('rectX').value;
      const y = document.getElementById('rectY').value;
      const width = document.getElementById('rectWidth').value;
      const height = document.getElementById('rectHeight').value;
      const hasBorder = document.getElementById('rectBorder').checked;
      const borderColor = document.getElementById('rectBorderColor').value;
      const borderThickness = document.getElementById('rectBorderThickness').value;
      const isFilled = document.getElementById('rectFill').checked;
      const fillColor = document.getElementById('rectFillColor').value;
      
      const url = '/draw/rect/advanced?x=' + x + '&y=' + y + 
                  '&width=' + width + '&height=' + height + 
                  '&hasBorder=' + hasBorder + '&borderColor=' + borderColor + 
                  '&borderThickness=' + borderThickness + 
                  '&isFilled=' + isFilled + '&fillColor=' + fillColor;
      
      fetch(url)
        .then(response => response.text())
        .then(data => {
          console.log(data);
          alert('矩形已繪製: ' + data);
        })
        .catch(error => {
          console.error('Error:', error);
          alert('繪製失敗');
        });
    }
    
    // 初始化所有顯示值
    function initializeValues() {
      updateTextColorValue();
      updateBgColorValue();
      updateFontSizeValue();
      updateLineColorValue();
      updateLineThicknessValue();
      updateCircleBorderColorValue();
      updateCircleBorderThicknessValue();
      updateCircleFillColorValue();
      updateRectBorderColorValue();
      updateRectBorderThicknessValue();
      updateRectFillColorValue();
      updateBrushSize();
      updateBrushColor();
    }
    
    // 灰階數據傳送函數
    function sendGrayscaleData() {
      const x = document.getElementById('grayscaleX').value;
      const y = document.getElementById('grayscaleY').value;
      const width = document.getElementById('grayscaleWidth').value;
      const height = document.getElementById('grayscaleHeight').value;
      const data = document.getElementById('grayscaleData').value.trim();
      
      // 驗證輸入
      if (!data) {
        alert('請輸入灰階數據！');
        return;
      }
      
      if (parseInt(width) <= 0 || parseInt(height) <= 0) {
        alert('寬度和高度必須大於0！');
        return;
      }
      
      // 檢查數據格式
      const values = data.split(',').map(v => v.trim()).filter(v => v !== '');
      const expectedCount = parseInt(width) * parseInt(height);
      
      if (values.length !== expectedCount) {
        alert(`數據點數不符！預期: ${expectedCount} 個，實際: ${values.length} 個`);
        return;
      }
      
      // 檢查數值範圍
      for (let i = 0; i < values.length; i++) {
        const val = parseInt(values[i]);
        if (isNaN(val) || val < 0 || val > 15) {
          alert(`第 ${i+1} 個數值無效: "${values[i]}"，應該是 0-15 之間的整數`);
          return;
        }
      }
      
      console.log('Sending grayscale data:', {x, y, width, height, dataLength: values.length});
      
      // 準備發送數據
      const formData = new FormData();
      formData.append('x', x);
      formData.append('y', y);
      formData.append('width', width);
      formData.append('height', height);
      formData.append('data', data);
      
      // 發送到伺服器
      fetch('/draw/grayscale', {
        method: 'POST',
        body: formData
      })
      .then(response => response.text())
      .then(result => {
        console.log('Grayscale data sent successfully:', result);
        alert('灰階圖片已成功顯示在EPD上！');
      })
      .catch(error => {
        console.error('Error sending grayscale data:', error);
        alert('傳送失敗: ' + error.message);
      });
    }
    
    function clearGrayscaleData() {
      document.getElementById('grayscaleData').value = '';
      document.getElementById('grayscaleX').value = '0';
      document.getElementById('grayscaleY').value = '0';
      document.getElementById('grayscaleWidth').value = '100';
      document.getElementById('grayscaleHeight').value = '100';
    }
    
    // 初始化 Canvas
    window.onload = function() {
      console.log('Window loaded, initializing...');
      initCanvas();
      initImageConverter();
      initImageControls();
      initializeValues();
      console.log('All systems initialized');
    };
    
    // ===== 圖片轉換器功能 =====
    let currentWidth = 480;
    let currentHeight = 800;
    let imageCanvas = null;
    let imageCtx = null;
    let currentImageData = null;
    
    function initImageConverter() {
      imageCanvas = document.getElementById('imageCanvas');
      imageCtx = imageCanvas.getContext('2d');
      
      // 設置初始畫布
      resetImageCanvas();
      
      // 綁定事件
      document.getElementById('imageInput').addEventListener('change', handleImageUpload);
      document.getElementById('widthSelect').addEventListener('change', updateCanvasSize);
      document.getElementById('heightSelect').addEventListener('change', updateCanvasSize);
      document.getElementById('converterDataTextarea').addEventListener('dblclick', copyConverterData);
    }
    
    function updateCanvasSize() {
      currentWidth = parseInt(document.getElementById('widthSelect').value);
      currentHeight = parseInt(document.getElementById('heightSelect').value);
      
      // 更新畫布尺寸
      imageCanvas.width = currentWidth;
      imageCanvas.height = currentHeight;
      
      // 更新標籤
      const totalPixels = currentWidth * currentHeight;
      document.getElementById('dataLabel').textContent = 
        `灰階數據 (${currentWidth}×${currentHeight} = ${totalPixels.toLocaleString()} 個值)：`;
      
      // 重置畫布
      resetImageCanvas();
      
      // 更新預設按鈕狀態
      updatePresetButtons();
    }
    
    function setPresetSize(width, height) {
      document.getElementById('widthSelect').value = width;
      document.getElementById('heightSelect').value = height;
      updateCanvasSize();
    }
    
    function updatePresetButtons() {
      const presetButtons = document.querySelectorAll('.preset-btn');
      presetButtons.forEach(btn => btn.classList.remove('active'));
      
      // 檢查是否符合預設尺寸

      if (currentWidth === 4 && currentHeight === 4) {
        presetButtons[0].classList.add('active');
      } else if (currentWidth === 8 && currentHeight === 8) {
        presetButtons[1].classList.add('active');
      } else if (currentWidth === 16 && currentHeight === 16) {
        presetButtons[2].classList.add('active');
      } else if (currentWidth === 32 && currentHeight === 32) {
        presetButtons[3].classList.add('active');
      } else if (currentWidth === 64 && currentHeight === 64) {
        presetButtons[4].classList.add('active');
      } else if (currentWidth === 128 && currentHeight === 128) {
        presetButtons[5].classList.add('active');
      } else if (currentWidth === 320 && currentHeight === 240) {
        presetButtons[6].classList.add('active');
      } else if (currentWidth === 480 && currentHeight === 800) {
        presetButtons[7].classList.add('active');
      } else if (currentWidth === 540 && currentHeight === 960) {
        presetButtons[9].classList.add('active');
      } else if (currentWidth === 800 && currentHeight === 600) {
        presetButtons[10].classList.add('active');
      }
    }
    
    function handleImageUpload(e) {
      const file = e.target.files[0];
      if (file) {
        processImage(file);
      }
    }
    
    function processImage(file) {
      const reader = new FileReader();
      
      reader.onload = function(e) {
        const img = new Image();
        
        img.onload = function() {
          // 清除畫布
          resetImageCanvas();
          
          // 繪製並縮放圖片到指定尺寸
          imageCtx.drawImage(img, 0, 0, currentWidth, currentHeight);
          
          // 獲取圖片數據
          const imageData = imageCtx.getImageData(0, 0, currentWidth, currentHeight);
          
          // 轉換為灰階
          const grayscaleData = convertToGrayscale(imageData);
          
          // 重新繪製灰階圖片
          drawGrayscaleImageToCanvas(grayscaleData);
          
          // 轉換為數據格式
          const dataString = grayscaleData.join(',');
          document.getElementById('converterDataTextarea').value = dataString;
          currentImageData = dataString;
          
          // 更新資訊
          document.getElementById('canvasInfo').innerHTML = 
            `<strong>圖片資訊：</strong><br>
             原始尺寸: ${img.width} × ${img.height}<br>
             轉換尺寸: ${currentWidth} × ${currentHeight}<br>
             數據點數: ${grayscaleData.length.toLocaleString()}<br>
             檔案大小: ${(file.size / 1024).toFixed(1)} KB`;
          
          showToast('圖片轉換完成！');
        };
        
        img.src = e.target.result;
      };
      
      reader.readAsDataURL(file);
    }
    
    function convertToGrayscale(imageData) {
      const data = imageData.data;
      const grayscaleData = [];
      
      for (let i = 0; i < data.length; i += 4) {
        const r = data[i];
        const g = data[i + 1];
        const b = data[i + 2];
        
        // 使用標準灰階轉換公式
        const gray = 0.299 * r + 0.587 * g + 0.114 * b;
        
        // 轉換為 0-15 範圍 (EPD 4-bit 灰階)
        const grayscaleValue = Math.round((gray / 255) * 15);
        grayscaleData.push(grayscaleValue);
      }
      
      return grayscaleData;
    }
    
    function drawGrayscaleImageToCanvas(grayscaleData) {
      const imageData = imageCtx.createImageData(currentWidth, currentHeight);
      const data = imageData.data;
      
      for (let i = 0; i < grayscaleData.length; i++) {
        const grayValue = Math.round((grayscaleData[i] / 15) * 255);
        const pixelIndex = i * 4;
        
        data[pixelIndex] = grayValue;     // R
        data[pixelIndex + 1] = grayValue; // G
        data[pixelIndex + 2] = grayValue; // B
        data[pixelIndex + 3] = 255;       // A
      }
      
      imageCtx.putImageData(imageData, 0, 0);
    }
    
    function resetImageCanvas() {
      imageCtx.fillStyle = '#ffffff';
      imageCtx.fillRect(0, 0, currentWidth, currentHeight);
    }
    
    function copyConverterData() {
      const textarea = document.getElementById('converterDataTextarea');
      textarea.select();
      textarea.setSelectionRange(0, 99999);
      
      try {
        document.execCommand('copy');
        showToast('數據已複製到剪貼簿！');
      } catch (err) {
        navigator.clipboard.writeText(textarea.value).then(() => {
          showToast('數據已複製到剪貼簿！');
        }).catch(() => {
          showToast('複製失敗，請手動選取複製');
        });
      }
    }
    
    function resetConverter() {
      document.getElementById('converterDataTextarea').value = '';
      document.getElementById('canvasInfo').textContent = '請先選擇圖片檔案';
      document.getElementById('imageInput').value = '';
      currentImageData = null;
      resetImageCanvas();
    }
    
    function downloadConverterData() {
      if (currentImageData) {
        const blob = new Blob([currentImageData], { type: 'text/plain' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `epd_grayscale_data_${currentWidth}x${currentHeight}_${new Date().getTime()}.txt`;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        showToast('數據檔案下載完成！');
      } else {
        showToast('請先轉換圖片！');
      }
    }
    
    function showToast(message) {
      const toast = document.createElement('div');
      toast.textContent = message;
      toast.style.cssText = `
        position: fixed;
        top: 20px;
        right: 20px;
        background: #4CAF50;
        color: white;
        padding: 10px 20px;
        border-radius: 4px;
        z-index: 1000;
        box-shadow: 0 2px 10px rgba(0,0,0,0.2);
        transition: opacity 0.3s;
      `;
      
      document.body.appendChild(toast);
      
      setTimeout(() => {
        toast.style.opacity = '0';
        setTimeout(() => {
          document.body.removeChild(toast);
        }, 300);
      }, 3000);
    }

    // ===== 智能圖片控制系統 =====
    let originalImageData = null;
    let previewCanvas = null;
    let previewCtx = null;
    let currentImageFile = null;
    
    // 初始化圖片控制系統
    function initImageControls() {
      previewCanvas = document.getElementById('imagePreviewCanvas');
      if (previewCanvas) {
        previewCtx = previewCanvas.getContext('2d');
        // 設定畫布大小比例 (EPD: 540x960)
        previewCanvas.width = 270;  // 1:2 縮放預覽
        previewCanvas.height = 480;
      }
    }
    
    // 圖片上傳預覽
    function previewUploadedImage() {
      const fileInput = document.getElementById('uploadImageFile');
      const file = fileInput.files[0];
      
      if (!file) {
        showToast('請選擇圖片檔案');
        return;
      }
      
      // 檢查檔案類型
      if (!file.type.startsWith('image/')) {
        showToast('請選擇有效的圖片檔案');
        return;
      }
      
      currentImageFile = file;
      const reader = new FileReader();
      
      reader.onload = function(e) {
        const img = new Image();
        img.onload = function() {
          originalImageData = {
            image: img,
            width: img.width,
            height: img.height
          };
          
          // 顯示圖片資訊
          document.getElementById('imageInfo').textContent = 
            `原始尺寸: ${img.width} × ${img.height} 像素`;
          
          // 自動設定合理的尺寸
          const maxWidth = Math.min(img.width, 540);
          const maxHeight = Math.min(img.height, 960);
          const aspectRatio = img.width / img.height;
          
          let newWidth = maxWidth;
          let newHeight = Math.round(newWidth / aspectRatio);
          
          if (newHeight > maxHeight) {
            newHeight = maxHeight;
            newWidth = Math.round(newHeight * aspectRatio);
          }
          
          document.getElementById('imgX').value = Math.max(0, Math.floor((540 - newWidth) / 2));
          document.getElementById('imgY').value = Math.max(0, Math.floor((960 - newHeight) / 2));
          document.getElementById('imgWidth').value = newWidth;
          document.getElementById('imgHeight').value = newHeight;
          
          // 顯示預覽區域
          document.getElementById('imagePreview').style.display = 'block';
          document.getElementById('sendImageBtn').disabled = false;
          
          // 顯示控制區域
          showImageControls();
          
          updateImagePreview();
          showToast('圖片載入成功！');
        };
        img.src = e.target.result;
      };
      
      reader.onerror = function() {
        showToast('圖片讀取失敗，請重新選擇');
      };
      
      reader.readAsDataURL(file);
    }
    
    // 更新圖片預覽
    function updateImagePreview() {
      if (!originalImageData || !previewCanvas) return;
      
      const x = parseInt(document.getElementById('imgX').value) || 0;
      const y = parseInt(document.getElementById('imgY').value) || 0;
      const width = parseInt(document.getElementById('imgWidth').value) || 100;
      const height = parseInt(document.getElementById('imgHeight').value) || 100;
      
      // 檢查尺寸限制
      const maxPixels = 100000;
      const currentPixels = width * height;
      
      // 清除畫布
      previewCtx.fillStyle = 'white';
      previewCtx.fillRect(0, 0, previewCanvas.width, previewCanvas.height);
      
      // 繪製 EPD 邊界
      previewCtx.strokeStyle = '#ccc';
      previewCtx.lineWidth = 1;
      previewCtx.strokeRect(0, 0, previewCanvas.width, previewCanvas.height);
      
      // 計算預覽縮放比例
      const scaleX = previewCanvas.width / 540;
      const scaleY = previewCanvas.height / 960;
      
      // 繪製圖片預覽
      try {
        previewCtx.drawImage(
          originalImageData.image,
          x * scaleX, y * scaleY,
          width * scaleX, height * scaleY
        );
        
        // 應用圖片效果
        applyImageEffects();
        
        // 更新位置資訊（包含尺寸警告）
        let positionText = `位置: (${x}, ${y}), 尺寸: ${width} × ${height}`;
        if (currentPixels > maxPixels) {
          positionText += ` ⚠️ 超過限制 (${currentPixels.toLocaleString()} > 100,000)`;
        } else {
          positionText += ` ✓ 符合限制 (${currentPixels.toLocaleString()}/100,000)`;
        }
        
        document.getElementById('positionInfo').textContent = positionText;
          
      } catch (error) {
        console.error('預覽更新失敗:', error);
        showToast('預覽更新失敗');
      }
    }
    
    // 應用圖片效果
    function applyImageEffects() {
      if (!previewCanvas) return;
      
      const imageData = previewCtx.getImageData(0, 0, previewCanvas.width, previewCanvas.height);
      const data = imageData.data;
      
      const contrast = parseFloat(document.getElementById('contrast').value) / 100;
      const brightness = parseInt(document.getElementById('brightness').value);
      const grayLevels = parseInt(document.getElementById('grayLevels').value);
      
      for (let i = 0; i < data.length; i += 4) {
        // 轉換為灰階
        let gray = data[i] * 0.299 + data[i+1] * 0.587 + data[i+2] * 0.114;
        
        // 調整亮度和對比度
        gray = (gray - 128) * contrast + 128 + brightness;
        gray = Math.max(0, Math.min(255, gray));
        
        // 量化到指定灰階級數
        const level = Math.floor(gray / (256 / grayLevels));
        gray = Math.round((level * 255) / (grayLevels - 1));
        
        data[i] = data[i+1] = data[i+2] = gray;
      }
      
      previewCtx.putImageData(imageData, 0, 0);
    }
    
    // ===== 階段2：參數控制和效果調整函數 =====
    
    // 輔助功能：適應螢幕
    function fitToScreen() {
      if (!originalImageData) {
        showToast('請先選擇圖片');
        return;
      }
      
      document.getElementById('imgX').value = 0;
      document.getElementById('imgY').value = 0;
      document.getElementById('imgWidth').value = 540;
      document.getElementById('imgHeight').value = 960;
      updateImagePreview();
      showToast('已調整為全螢幕顯示');
    }
    
    // 輔助功能：保持寬高比例
    function keepAspectRatio() {
      if (!originalImageData) {
        showToast('請先選擇圖片');
        return;
      }
      
      const targetWidth = parseInt(document.getElementById('imgWidth').value);
      const aspectRatio = originalImageData.height / originalImageData.width;
      const newHeight = Math.round(targetWidth * aspectRatio);
      
      // 檢查像素限制
      const maxPixels = 100000;
      const currentPixels = targetWidth * newHeight;
      
      if (currentPixels > maxPixels) {
        // 自動縮放到合適尺寸
        const scale = Math.sqrt(maxPixels / currentPixels);
        const scaledWidth = Math.floor(targetWidth * scale);
        const scaledHeight = Math.floor(newHeight * scale);
        
        document.getElementById('imgWidth').value = scaledWidth;
        document.getElementById('imgHeight').value = scaledHeight;
        showToast('圖片已自動縮放至合適尺寸 (' + scaledWidth + 'x' + scaledHeight + ')');
      } else if (newHeight <= 960) {
        document.getElementById('imgHeight').value = newHeight;
        showToast('已調整為正確比例');
      } else {
        // 如果高度超出，則基於高度計算寬度
        const targetHeight = 960;
        const newWidth = Math.round(targetHeight / aspectRatio);
        const newPixels = newWidth * targetHeight;
        
        if (newPixels > maxPixels) {
          // 再次檢查並縮放
          const scale = Math.sqrt(maxPixels / newPixels);
          document.getElementById('imgWidth').value = Math.floor(newWidth * scale);
          document.getElementById('imgHeight').value = Math.floor(targetHeight * scale);
          showToast('圖片已自動縮放至合適尺寸');
        } else {
          document.getElementById('imgWidth').value = newWidth;
          document.getElementById('imgHeight').value = targetHeight;
          showToast('已調整為正確比例');
        }
      }
      
      updateImagePreview();
    }
    
    // 輔助功能：圖片置中
    function centerImage() {
      const width = parseInt(document.getElementById('imgWidth').value);
      const height = parseInt(document.getElementById('imgHeight').value);
      
      const centerX = Math.max(0, Math.floor((540 - width) / 2));
      const centerY = Math.max(0, Math.floor((960 - height) / 2));
      
      document.getElementById('imgX').value = centerX;
      document.getElementById('imgY').value = centerY;
      updateImagePreview();
      showToast('圖片已置中');
    }
    
    // 效果控制：對比度更新
    function updateContrast() {
      const value = document.getElementById('contrast').value;
      document.getElementById('contrastValue').textContent = value + '%';
      updateImagePreview();
    }
    
    // 效果控制：亮度更新
    function updateBrightness() {
      const value = document.getElementById('brightness').value;
      document.getElementById('brightnessValue').textContent = value;
      updateImagePreview();
    }
    
    // 效果控制：灰階級數更新
    function updateGrayLevels() {
      const value = document.getElementById('grayLevels').value;
      document.getElementById('grayLevelsValue').textContent = value + '級';
      updateImagePreview();
    }
    
    // 效果控制：顏色反相
    function invertColors() {
      if (!originalImageData) {
        showToast('請先選擇圖片');
        return;
      }
      
      // 切換反相狀態
      const isInverted = document.getElementById('contrast').dataset.inverted === 'true';
      
      if (!isInverted) {
        // 設定反相效果
        document.getElementById('contrast').value = 100;
        document.getElementById('brightness').value = 0;
        document.getElementById('contrast').dataset.inverted = 'true';
        showToast('已套用反相效果');
      } else {
        // 取消反相效果
        document.getElementById('contrast').dataset.inverted = 'false';
        showToast('已取消反相效果');
      }
      
      updateContrast();
      updateBrightness();
      updateImagePreview();
    }
    
    // 效果控制：重置所有效果
    function resetEffects() {
      document.getElementById('contrast').value = 100;
      document.getElementById('brightness').value = 0;
      document.getElementById('grayLevels').value = 16;
      document.getElementById('contrast').dataset.inverted = 'false';
      
      updateContrast();
      updateBrightness();
      updateGrayLevels();
      updateImagePreview();
      showToast('效果已重置');
    }
    
    // 顯示控制區域
    function showImageControls() {
      document.getElementById('positionControls').style.display = 'block';
      document.getElementById('effectControls').style.display = 'block';
    }
    
    // 隱藏控制區域
    function hideImageControls() {
      document.getElementById('positionControls').style.display = 'none';
      document.getElementById('effectControls').style.display = 'none';
    }
    
    // 更新預覽函數（改進版）
    function updateImagePreview() {
      if (!originalImageData || !previewCanvas) return;
      
      // 顯示控制區域
      showImageControls();
      
      const x = parseInt(document.getElementById('imgX').value) || 0;
      const y = parseInt(document.getElementById('imgY').value) || 0;
      const width = parseInt(document.getElementById('imgWidth').value) || 100;
      const height = parseInt(document.getElementById('imgHeight').value) || 100;
      
      // 參數驗證
      if (x + width > 540) {
        document.getElementById('imgWidth').value = 540 - x;
        showToast('寬度已自動調整以適應螢幕');
        return;
      }
      
      if (y + height > 960) {
        document.getElementById('imgHeight').value = 960 - y;
        showToast('高度已自動調整以適應螢幕');
        return;
      }
      
      // 清除畫布
      previewCtx.fillStyle = 'white';
      previewCtx.fillRect(0, 0, previewCanvas.width, previewCanvas.height);
      
      // 繪製 EPD 邊界線
      previewCtx.strokeStyle = '#ccc';
      previewCtx.lineWidth = 1;
      previewCtx.strokeRect(0, 0, previewCanvas.width, previewCanvas.height);
      
      // 計算預覽縮放比例
      const scaleX = previewCanvas.width / 540;
      const scaleY = previewCanvas.height / 960;
      
      // 繪製圖片預覽
      try {
        previewCtx.drawImage(
          originalImageData.image,
          x * scaleX, y * scaleY,
          width * scaleX, height * scaleY
        );
        
        // 應用圖片效果
        applyImageEffects();
        
        // 繪製位置指示框
        previewCtx.strokeStyle = '#ff4444';
        previewCtx.lineWidth = 2;
        previewCtx.strokeRect(
          x * scaleX, y * scaleY,
          width * scaleX, height * scaleY
        );
        
        // 更新位置資訊
        document.getElementById('positionInfo').textContent = 
          `位置: (${x}, ${y}), 尺寸: ${width} × ${height} 像素`;
          
      } catch (error) {
        console.error('預覽更新失敗:', error);
        showToast('預覽更新失敗：' + error.message);
      }
    }
    
    // ===== 階段3：圖片資料準備和發送 =====
    
    // 發送圖片到EPD
    async function sendImageToEPD() {
      if (!originalImageData) {
        showToast('請先選擇圖片');
        return;
      }
      
      const btn = document.getElementById('sendImageBtn');
      const progress = document.getElementById('uploadProgress');
      const progressBar = document.getElementById('progressBar');
      const progressText = document.getElementById('progressText');
      
      btn.disabled = true;
      progress.style.display = 'block';
      progressBar.style.width = '0%';
      progressText.textContent = '0%';
      
      try {
        // 準備圖片資料
        progressText.textContent = '正在處理圖片...';
        progressBar.style.width = '10%';
        
        const imageData = await prepareImageData();
        if (!imageData) {
          throw new Error('圖片資料處理失敗');
        }
        
        progressText.textContent = '正在傳輸資料...';
        progressBar.style.width = '30%';
        
        // 發送到Arduino
        const response = await fetch('/upload-image', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify(imageData)
        });
        
        progressBar.style.width = '80%';
        progressText.textContent = '正在渲染到EPD...';
        
        if (response.ok) {
          const result = await response.text();
          progressBar.style.width = '100%';
          progressText.textContent = '完成！';
          showToast('圖片已成功發送到EPD！');
          console.log('Server response:', result);
        } else {
          throw new Error(`伺服器錯誤: ${response.status}`);
        }
        
      } catch (error) {
        console.error('發送失敗:', error);
        showToast('發送失敗: ' + error.message);
        progressBar.style.width = '0%';
        progressText.textContent = '發送失敗';
      } finally {
        setTimeout(() => {
          btn.disabled = false;
          progress.style.display = 'none';
        }, 3000);
      }
    }
    
    // 準備圖片資料
    async function prepareImageData() {
      try {
        const canvas = document.createElement('canvas');
        const ctx = canvas.getContext('2d');
        
        const x = parseInt(document.getElementById('imgX').value) || 0;
        const y = parseInt(document.getElementById('imgY').value) || 0;
        const width = parseInt(document.getElementById('imgWidth').value) || 100;
        const height = parseInt(document.getElementById('imgHeight').value) || 100;
        
        canvas.width = width;
        canvas.height = height;
        
        // 繪製調整後的圖片
        ctx.drawImage(originalImageData.image, 0, 0, width, height);
        
        // 獲取圖片資料並應用效果
        const imageData = ctx.getImageData(0, 0, width, height);
        const data = imageData.data;
        
        const contrast = parseFloat(document.getElementById('contrast').value) / 100;
        const brightness = parseInt(document.getElementById('brightness').value);
        const grayLevels = parseInt(document.getElementById('grayLevels').value);
        const isInverted = document.getElementById('contrast').dataset.inverted === 'true';
        
        const grayData = [];
        
        // 逐像素處理
        for (let i = 0; i < data.length; i += 4) {
          // 轉換為灰階
          let gray = data[i] * 0.299 + data[i+1] * 0.587 + data[i+2] * 0.114;
          
          // 調整亮度和對比度
          gray = (gray - 128) * contrast + 128 + brightness;
          gray = Math.max(0, Math.min(255, gray));
          
          // 反相處理
          if (isInverted) {
            gray = 255 - gray;
          }
          
          // 量化到指定灰階級數
          const level = Math.floor(gray / (256 / grayLevels));
          const finalGray = Math.round((level * 15) / (grayLevels - 1));
          
          grayData.push(Math.max(0, Math.min(15, finalGray)));
        }
        
        return {
          x: x,
          y: y,
          width: width,
          height: height,
          grayData: grayData,
          contrast: contrast,
          brightness: brightness,
          grayLevels: grayLevels,
          inverted: isInverted,
          filename: currentImageFile ? currentImageFile.name : 'unknown',
          filesize: currentImageFile ? currentImageFile.size : 0
        };
        
      } catch (error) {
        console.error('圖片資料準備失敗:', error);
        return null;
      }
    }
    
    
    // 智能圖片控制變數
    var smartImageData = null;
    var originalImage = null;
    
    // 初始化圖片控制
    function initImageControls() {
      console.log('初始化智能圖片控制系統');
      // 延遲執行，確保DOM元素已載入
      setTimeout(function() {
        updatePositionInfo();
      }, 100);
    }
    
    // 預覽上傳的圖片
    function previewUploadedImage() {
      console.log('開始處理上傳圖片');
      var file = document.getElementById('uploadImageFile').files[0];
      if (!file) return;
      
      var reader = new FileReader();
      reader.onload = function(e) {
        var img = new Image();
        img.onload = function() {
          originalImage = img;
          console.log('圖片載入完成:', img.width + 'x' + img.height);
          
          // 自動設定尺寸
          document.getElementById('imgWidth').value = Math.min(img.width, 540);
          document.getElementById('imgHeight').value = Math.min(img.height, 960);
          
          // 顯示控制面板
          document.getElementById('positionControls').style.display = 'block';
          document.getElementById('effectControls').style.display = 'block';
          document.getElementById('sendImageBtn').disabled = false;
          
          updatePositionInfo();
          processImageForPreview();
        };
        img.src = e.target.result;
      };
      reader.readAsDataURL(file);
    }
    
    // 處理圖片預覽
    function processImageForPreview() {
      if (!originalImage) return;
      
      var canvas = document.getElementById('imagePreviewCanvas');
      if (!canvas) {
        console.log('Preview canvas not found');
        return;
      }
      
      var ctx = canvas.getContext('2d');
      if (!ctx) {
        console.log('Canvas context not available');
        return;
      }
      
      var imgWidthEl = document.getElementById('imgWidth');
      var imgHeightEl = document.getElementById('imgHeight');
      
      if (!imgWidthEl || !imgHeightEl) {
        console.log('Image dimension inputs not found');
        return;
      }
      
      var w = parseInt(imgWidthEl.value);
      var h = parseInt(imgHeightEl.value);
      
      canvas.width = w;
      canvas.height = h;
      
      // 繪製縮放後的圖片
      ctx.drawImage(originalImage, 0, 0, w, h);
      
      // 取得圖片資料
      smartImageData = ctx.getImageData(0, 0, w, h);
      console.log('圖片處理完成:', w + 'x' + h);
      
      // 套用濃淡調整
      applyImageAdjustments();
    }
    
    // 套用圖片調整
    function applyImageAdjustments() {
      if (!smartImageData) return;
      
      var canvas = document.getElementById('imagePreviewCanvas');
      var ctx = canvas.getContext('2d');
      
      var contrast = parseFloat(document.getElementById('contrast').value) / 100;
      var brightness = parseFloat(document.getElementById('brightness').value);
      
      var imageData = ctx.createImageData(smartImageData.width, smartImageData.height);
      var srcData = smartImageData.data;
      var destData = imageData.data;
      
      for (var i = 0; i < srcData.length; i += 4) {
        var gray = srcData[i] * 0.299 + srcData[i+1] * 0.587 + srcData[i+2] * 0.114;
        
        // 套用調整
        gray = (gray - 128) * contrast + 128 + brightness;
        gray = Math.max(0, Math.min(255, gray));
        
        // 轉換為16階灰階預覽
        var gray16 = Math.floor(gray / 16);
        gray16 = Math.max(0, Math.min(15, gray16));
        var displayGray = gray16 * 17;
        
        destData[i] = destData[i+1] = destData[i+2] = displayGray;
        destData[i+3] = 255;
      }
      
      ctx.putImageData(imageData, 0, 0);
    }
    
    // 更新位置資訊
    function updatePositionInfo() {
      // 安全檢查DOM元素是否存在
      var imgXEl = document.getElementById('imgX');
      var imgYEl = document.getElementById('imgY');
      var imgWidthEl = document.getElementById('imgWidth');
      var imgHeightEl = document.getElementById('imgHeight');
      var positionInfoEl = document.getElementById('positionInfo');
      
      if (!imgXEl || !imgYEl || !imgWidthEl || !imgHeightEl) {
        console.log('Position input elements not found, skipping update');
        return;
      }
      
      var x = parseInt(imgXEl.value) || 0;
      var y = parseInt(imgYEl.value) || 0;
      var w = parseInt(imgWidthEl.value) || 100;
      var h = parseInt(imgHeightEl.value) || 100;
      
      var info = '位置: (' + x + ', ' + y + ') 尺寸: ' + w + ' x ' + h + ' 像素';
      if (x + w > 540 || y + h > 960) {
        info += ' ⚠️ 超出螢幕範圍!';
      }
      
      if (positionInfoEl) {
        positionInfoEl.textContent = info;
      }
      
      if (originalImage) {
        processImageForPreview();
      }
    }
    
    // 為HTML控件提供的函數別名
    function updateImagePreview() {
      updatePositionInfo();
    }
    
    // 更新對比度顯示
    function updateContrast() {
      var contrast = document.getElementById('contrast').value;
      document.getElementById('contrastValue').textContent = contrast + '%';
      if (smartImageData) {
        applyImageAdjustments();
      }
    }
    
    // 更新亮度顯示
    function updateBrightness() {
      var brightness = document.getElementById('brightness').value;
      document.getElementById('brightnessValue').textContent = brightness;
      if (smartImageData) {
        applyImageAdjustments();
      }
    }
    
    // 更新灰階級數顯示
    function updateGrayLevels() {
      var levels = document.getElementById('grayLevels').value;
      document.getElementById('grayLevelsValue').textContent = levels + '級';
      if (smartImageData) {
        applyImageAdjustments();
      }
    }
    
    // 反相顏色
    function invertColors() {
      var contrast = document.getElementById('contrast');
      var brightness = document.getElementById('brightness');
      
      contrast.value = 200 - parseInt(contrast.value);
      brightness.value = -parseInt(brightness.value);
      
      updateContrast();
      updateBrightness();
    }
    
    // 重置效果
    function resetEffects() {
      document.getElementById('contrast').value = 100;
      document.getElementById('brightness').value = 0;
      document.getElementById('grayLevels').value = 16;
      
      updateContrast();
      updateBrightness();
      updateGrayLevels();
    }
    
    // 發送圖片到EPD (函數別名)
    function sendImageToEPD() {
      uploadImageToEPD();
    }
    
    // 自動適應設定
    function fitToScreen() {
      if (!originalImage) {
        alert('請先選擇圖片');
        return;
      }
      
      var scaleX = 540 / originalImage.width;
      var scaleY = 960 / originalImage.height;
      var scale = Math.min(scaleX, scaleY);
      
      document.getElementById('imgWidth').value = Math.floor(originalImage.width * scale);
      document.getElementById('imgHeight').value = Math.floor(originalImage.height * scale);
      updatePositionInfo();
    }
    
    // 保持比例
    function keepAspectRatio() {
      if (!originalImage) return;
      
      var w = parseInt(document.getElementById('imgWidth').value);
      var ratio = originalImage.height / originalImage.width;
      
      document.getElementById('imgHeight').value = Math.floor(w * ratio);
      updatePositionInfo();
    }
    
    // 置中
    function centerImage() {
      var w = parseInt(document.getElementById('imgWidth').value);
      var h = parseInt(document.getElementById('imgHeight').value);
      
      document.getElementById('imgX').value = Math.floor((540 - w) / 2);
      document.getElementById('imgY').value = Math.floor((960 - h) / 2);
      updatePositionInfo();
    }
    
    // 反相
    function invertImage() {
      var contrast = parseFloat(document.getElementById('imgContrast').value);
      var brightness = parseFloat(document.getElementById('imgBrightness').value);
      
      document.getElementById('imgContrast').value = -contrast;
      document.getElementById('imgBrightness').value = 255 - brightness;
      
      if (smartImageData) {
        applyImageAdjustments();
      }
    }
    
    // 重設
    function resetImageSettings() {
      document.getElementById('imgX').value = 0;
      document.getElementById('imgY').value = 0;
      document.getElementById('contrast').value = 100;
      document.getElementById('brightness').value = 0;
      
      if (originalImage) {
        document.getElementById('imgWidth').value = Math.min(originalImage.width, 540);
        document.getElementById('imgHeight').value = Math.min(originalImage.height, 960);
        processImageForPreview();
      }
      
      updatePositionInfo();
    }
    
    // 上傳圖片到EPD
    function uploadImageToEPD() {
      if (!smartImageData) {
        alert('請先選擇並處理圖片');
        return;
      }
      
      var settings = {
        x: parseInt(document.getElementById('imgX').value),
        y: parseInt(document.getElementById('imgY').value),
        w: parseInt(document.getElementById('imgWidth').value),
        h: parseInt(document.getElementById('imgHeight').value),
        contrast: parseFloat(document.getElementById('contrast').value) / 100,
        brightness: parseFloat(document.getElementById('brightness').value)
      };
      
      console.log('準備上傳圖片:', settings);
      
      // 顯示進度條
      var progressDiv = document.getElementById('uploadProgress');
      var progressBar = document.getElementById('progressBar');
      var sendBtn = document.getElementById('sendImageBtn');
      
      progressDiv.style.display = 'block';
      progressBar.style.width = '10%';
      sendBtn.disabled = true;
      sendBtn.textContent = '處理中...';
      
      var canvas = document.createElement('canvas');
      canvas.width = settings.w;
      canvas.height = settings.h;
      var ctx = canvas.getContext('2d');
      
      // 調整對比度和亮度
      var imageData = ctx.createImageData(settings.w, settings.h);
      var srcData = smartImageData.data;
      var destData = imageData.data;
      
      progressBar.style.width = '30%';
      
      for (var i = 0; i < srcData.length; i += 4) {
        var gray = srcData[i] * 0.299 + srcData[i+1] * 0.587 + srcData[i+2] * 0.114;
        
        // 套用亮度和對比度
        gray = (gray - 128) * settings.contrast + 128 + settings.brightness;
        gray = Math.max(0, Math.min(255, gray));
        
        // 轉換為4位元灰階 (0-15)
        var gray4bit = Math.floor(gray / 17);
        gray4bit = Math.max(0, Math.min(15, gray4bit));
        
        // 轉回8位元供顯示
        var displayGray = gray4bit * 17;
        
        destData[i] = destData[i+1] = destData[i+2] = displayGray;
        destData[i+3] = 255;
      }
      
      ctx.putImageData(imageData, 0, 0);
      progressBar.style.width = '60%';
      
      // 提取灰階數據（不使用base64，直接傳送數值陣列）
      var grayData = [];
      for (var i = 0; i < destData.length; i += 4) {
        var gray4bit = Math.round(destData[i] / 17);
        grayData.push(Math.max(0, Math.min(15, gray4bit)));
      }
      
      progressBar.style.width = '80%';
      
      // 檢查數據大小限制
      var dataSize = grayData.length;
      var maxSize = 100000; // 限制最大10萬像素
      
      if (dataSize > maxSize) {
        alert('圖片過大！當前: ' + dataSize + ' 像素，最大允許: ' + maxSize + ' 像素\n請縮小圖片尺寸或降低解析度');
        progressDiv.style.display = 'none';
        sendBtn.disabled = false;
        sendBtn.textContent = '上傳圖片到 EPD';
        return;
      }
      
      var uploadData = {
        x: settings.x,
        y: settings.y,
        width: settings.w,  // 改為 width 以匹配 Arduino 解析
        height: settings.h, // 改為 height 以匹配 Arduino 解析
        contrast: settings.contrast,
        brightness: settings.brightness,
        grayLevels: 16,     // 固定使用16級灰階
        inverted: false,    // 不反相
        grayData: grayData,
        filename: 'canvas_image',
        filesize: dataSize
      };
      
      console.log('上傳數據大小:', dataSize, '像素, JSON大小:', JSON.stringify(uploadData).length, '字節');
      
      // 創建AbortController用於超時控制
      var controller = new AbortController();
      var timeoutId = setTimeout(function() {
        controller.abort();
      }, 60000); // 60秒超時
      
      // 使用fetch API上傳
      fetch('/upload-image', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(uploadData),
        signal: controller.signal
      })
      .then(response => {
        clearTimeout(timeoutId); // 清除超時計時器
        if (!response.ok) {
          throw new Error('HTTP ' + response.status + ': ' + response.statusText);
        }
        return response.text();
      })
      .then(data => {
        progressBar.style.width = '100%';
        console.log('上傳成功:', data);
        setTimeout(function() {
          progressDiv.style.display = 'none';
          sendBtn.disabled = false;
          sendBtn.textContent = '上傳圖片到 EPD';
          alert('圖片上傳成功！EPD顯示更新完成\n' + data);
        }, 500);
      })
      .catch(error => {
        clearTimeout(timeoutId); // 清除超時計時器
        console.error('上傳錯誤:', error);
        progressDiv.style.display = 'none';
        sendBtn.disabled = false;
        sendBtn.textContent = '上傳圖片到 EPD';
        
        var errorMsg = '上傳失敗: ';
        if (error.name === 'AbortError') {
          errorMsg += '請求超時（60秒）- 圖片可能太大或處理時間過長';
        } else if (error.message.includes('Failed to fetch')) {
          errorMsg += '網路連線失敗 - 請檢查WiFi連線';
        } else if (error.message.includes('HTTP')) {
          errorMsg += '伺服器錯誤 - ' + error.message;
        } else {
          errorMsg += error.message;
        }
        
        alert(errorMsg);
      });
    }
    
    // 頁面載入完成初始化
    document.addEventListener('DOMContentLoaded', function() {
      initImageControls();
    });
  </script>
  
  <!-- 智能圖片控制上傳系統 -->
  <div class="upload">
    <h3>🖼️ 智能圖片控制器</h3>
    
    <!-- 使用說明 -->
    <div style="background: #e8f5e8; padding: 12px; border-radius: 6px; margin-bottom: 15px; border-left: 4px solid #4CAF50;">
      <strong>📌 使用限制說明：</strong>
      <ul style="margin: 8px 0; padding-left: 20px; font-size: 13px;">
        <li><strong>最大像素數：</strong> 100,000 像素 (例如：316×316)</li>
        <li><strong>處理時間：</strong> 最多60秒，大圖片需要更長時間</li>
        <li><strong>建議尺寸：</strong> 小於300×300像素可獲得最佳性能</li>
        <li><strong>格式支援：</strong> JPG、PNG、GIF、BMP等常見格式</li>
        <li><strong>顯示效果：</strong> 16階灰階，黑白效果最佳</li>
      </ul>
    </div>
    
    <!-- 圖片選擇區 -->
    <div class="form-row">
      <label>選擇圖片:</label>
      <input type="file" id="uploadImageFile" accept="image/*" onchange="previewUploadedImage()">
      <small>支援 JPG、PNG、GIF、BMP 等格式</small>
    </div>
    
    <!-- 圖片資訊顯示 -->
    <div id="imageInfo" style="margin: 10px 0; color: #666; font-size: 14px;">
      尚未選擇圖片
    </div>
    
    <!-- 圖片預覽區 -->
    <div id="imagePreview" style="display:none;">
      <h4>預覽效果</h4>
      <div style="text-align: center; margin: 15px 0;">
        <canvas id="imagePreviewCanvas" style="border: 2px solid #333; background: white;"></canvas>
        <div id="positionInfo" style="margin-top: 10px; color: #666; font-size: 12px;">
          位置資訊將在此顯示
        </div>
      </div>
    </div>
    
    <!-- 位置和尺寸控制 -->
    <div id="positionControls" style="display:none;">
      <h4>📍 位置和尺寸控制</h4>
      <div class="form-row">
        <label>X座標:</label>
        <input type="number" id="imgX" min="0" max="540" value="0" step="1" onchange="updateImagePreview()">
        <label>Y座標:</label>
        <input type="number" id="imgY" min="0" max="960" value="0" step="1" onchange="updateImagePreview()">
      </div>
      <div class="form-row">
        <label>寬度:</label>
        <input type="number" id="imgWidth" min="1" max="540" value="200" step="1" onchange="updateImagePreview()">
        <label>高度:</label>
        <input type="number" id="imgHeight" min="1" max="960" value="200" step="1" onchange="updateImagePreview()">
      </div>
      <div class="form-row">
        <button onclick="fitToScreen()">📱 適應螢幕</button>
        <button onclick="keepAspectRatio()">📐 保持比例</button>
        <button onclick="centerImage()">🎯 置中</button>
      </div>
    </div>
    
    <!-- 效果控制 -->
    <div id="effectControls" style="display:none;">
      <h4>🎨 效果控制</h4>
      <div class="form-row">
        <label>對比度:</label>
        <input type="range" id="contrast" min="0" max="200" value="100" step="5" oninput="updateContrast()">
        <span class="color-value" id="contrastValue">100%</span>
      </div>
      <div class="form-row">
        <label>亮度:</label>
        <input type="range" id="brightness" min="-100" max="100" value="0" step="5" oninput="updateBrightness()">
        <span class="color-value" id="brightnessValue">0</span>
      </div>
      <div class="form-row">
        <label>灰階級數:</label>
        <input type="range" id="grayLevels" min="2" max="16" value="16" step="1" oninput="updateGrayLevels()">
        <span class="color-value" id="grayLevelsValue">16級</span>
      </div>
      <div class="form-row">
        <button onclick="invertColors()">🔄 反相</button>
        <button onclick="resetEffects()">↺ 重置效果</button>
      </div>
    </div>
    
    <!-- 發送控制 -->
    <div class="form-row" style="margin-top: 20px;">
      <button onclick="sendImageToEPD()" id="sendImageBtn" disabled style="background: #4CAF50; color: white; font-size: 16px; padding: 12px 24px;">
        🚀 發送圖片到 EPD
      </button>
    </div>
    
    <!-- 進度顯示 -->
    <div id="uploadProgress" style="display:none; margin-top: 15px;">
      <div style="background: #f0f0f0; border-radius: 10px; padding: 10px;">
        <div>處理進度：<span id="progressText">0%</span></div>
        <div style="background: #ddd; border-radius: 5px; margin-top: 5px;">
          <div id="progressBar" style="background: #4CAF50; height: 20px; border-radius: 5px; width: 0%; transition: width 0.3s;"></div>
        </div>
      </div>
    </div>
  </div>

</body>
</html>
)rawliteral";

  html.replace("%WIDTH%", String(EPD_WIDTH));
  html.replace("%HEIGHT%", String(EPD_HEIGHT));
  html.replace("%BYTES%", String(FB_SIZE));

  // 使用輔助函數發送 UTF-8 響應
  sendHtmlResponse(200, html);

  unsigned long endTime = millis();
  Serial.printf("[OK] Main page served in %lu ms\n", endTime - startTime);
  Serial.printf("[MEMORY] Free heap after page serve: %d bytes\n", ESP.getFreeHeap());
}

void handleClear()
{
  Serial.println("[REQUEST] handleClear - Starting clear operation");
  unsigned long startTime = millis();

  if (framebuffer)
  {
    Serial.println("[CLEAR] Clearing framebuffer...");
    memset(framebuffer, 0xFF, FB_SIZE);

    Serial.println("[CLEAR] Powering on EPD...");
    epd_poweron();

    Serial.println("[CLEAR] Clearing EPD display...");
    epd_clear();

    Serial.println("[CLEAR] Powering off EPD...");
    epd_poweroff();

    Serial.print("[OK] Clear operation completed in ");
    Serial.print(millis() - startTime);
    Serial.println(" ms");
  }
  else
  {
    Serial.println("[ERROR] Framebuffer is null!");
  }

  sendTextResponse(200, "OK");
}

void handleDrawLine()
{
  Serial.println("[REQUEST] handleDrawLine - Drawing random line");
  unsigned long startTime = millis();

  if (framebuffer)
  {
    int y = random(10, EPD_HEIGHT);
    Serial.print("[DRAW] Drawing line at y=");
    Serial.println(y);

    epd_poweron();
    epd_draw_hline(10, y, EPD_WIDTH - 20, 0, framebuffer);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();

    Serial.print("[OK] Line drawn in ");
    Serial.print(millis() - startTime);
    Serial.println(" ms");
  }
  else
  {
    Serial.println("[ERROR] Framebuffer is null!");
  }

  sendTextResponse(200, "Line drawn");
}

void handleDrawRect()
{
  Serial.println("handleDrawRect");
  if (framebuffer)
  {
    epd_poweron();
    epd_draw_rect(10, random(10, EPD_HEIGHT), random(10, 60), random(10, 120), 0, framebuffer);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
  }
  server.send(200, "text/plain", "Rect drawn");
}

void handleDrawCircle()
{
  Serial.println("handleDrawCircle");
  if (framebuffer)
  {
    epd_poweron();
    epd_draw_circle(random(10, EPD_WIDTH), random(10, EPD_HEIGHT), 20, 0, framebuffer);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
  }
  server.send(200, "text/plain", "Circle drawn");
}

// ===== 進階畫線控制 =====
void handleDrawLineAdvanced()
{
  Serial.println("handleDrawLineAdvanced");
  if (!framebuffer)
  {
    server.send(400, "text/plain", "Framebuffer not available");
    return;
  }

  // 獲取參數
  int x1 = server.arg("x1").toInt();
  int y1 = server.arg("y1").toInt();
  int x2 = server.arg("x2").toInt();
  int y2 = server.arg("y2").toInt();
  int color = server.arg("color").toInt();
  int thickness = server.arg("thickness").toInt();

  // 限制參數範圍
  x1 = constrain(x1, 0, EPD_WIDTH - 1);
  y1 = constrain(y1, 0, EPD_HEIGHT - 1);
  x2 = constrain(x2, 0, EPD_WIDTH - 1);
  y2 = constrain(y2, 0, EPD_HEIGHT - 1);
  color = constrain(color, 0, 15);
  thickness = constrain(thickness, 1, 20);

  epd_poweron();

  // 繪製指定粗細的線條
  for (int i = 0; i < thickness; i++)
  {
    for (int j = 0; j < thickness; j++)
    {
      // 使用Bresenham算法繪製線條，並增加粗細
      int dx = abs(x2 - x1);
      int dy = abs(y2 - y1);
      int sx = (x1 < x2) ? 1 : -1;
      int sy = (y1 < y2) ? 1 : -1;
      int err = dx - dy;
      int x = x1, y = y1;

      while (true)
      {
        // 繪製粗線的每個點
        int px = x + i - thickness / 2;
        int py = y + j - thickness / 2;
        if (px >= 0 && px < EPD_WIDTH && py >= 0 && py < EPD_HEIGHT)
        {
          epd_fill_rect(px, py, 1, 1, color, framebuffer);
        }

        if (x == x2 && y == y2)
          break;

        int e2 = 2 * err;
        if (e2 > -dy)
        {
          err -= dy;
          x += sx;
        }
        if (e2 < dx)
        {
          err += dx;
          y += sy;
        }
      }
    }
  }

  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();

  String response = "Line drawn from (" + String(x1) + "," + String(y1) +
                    ") to (" + String(x2) + "," + String(y2) +
                    ") color:" + String(color) + " thickness:" + String(thickness);
  server.send(200, "text/plain", response);
}

// ===== 進階畫圓控制 =====
void handleDrawCircleAdvanced()
{
  Serial.println("handleDrawCircleAdvanced");
  if (!framebuffer)
  {
    server.send(400, "text/plain", "Framebuffer not available");
    return;
  }

  // 獲取參數
  int centerX = server.arg("centerX").toInt();
  int centerY = server.arg("centerY").toInt();
  int radius = server.arg("radius").toInt();
  int borderColor = server.arg("borderColor").toInt();
  int fillColor = server.arg("fillColor").toInt();
  int borderThickness = server.arg("borderThickness").toInt();
  bool hasBorder = server.arg("hasBorder").equals("true");
  bool isFilled = server.arg("isFilled").equals("true");

  // 限制參數範圍
  centerX = constrain(centerX, 0, EPD_WIDTH - 1);
  centerY = constrain(centerY, 0, EPD_HEIGHT - 1);
  radius = constrain(radius, 1, min(EPD_WIDTH, EPD_HEIGHT) / 2);
  borderColor = constrain(borderColor, 0, 15);
  fillColor = constrain(fillColor, 0, 15);
  borderThickness = constrain(borderThickness, 1, 10);

  epd_poweron();

  // 如果要填滿圓形
  if (isFilled)
  {
    for (int y = -radius; y <= radius; y++)
    {
      for (int x = -radius; x <= radius; x++)
      {
        if (x * x + y * y <= radius * radius)
        {
          int px = centerX + x;
          int py = centerY + y;
          if (px >= 0 && px < EPD_WIDTH && py >= 0 && py < EPD_HEIGHT)
          {
            epd_fill_rect(px, py, 1, 1, fillColor, framebuffer);
          }
        }
      }
    }
  }

  // 如果要繪製邊框
  if (hasBorder)
  {
    for (int t = 0; t < borderThickness; t++)
    {
      int r = radius - t;
      if (r > 0)
      {
        // 使用中點圓算法繪製圓周
        int x = 0;
        int y = r;
        int d = 1 - r;

        while (x <= y)
        {
          // 繪製8個對稱點
          int points[8][2] = {
              {centerX + x, centerY + y}, {centerX - x, centerY + y}, {centerX + x, centerY - y}, {centerX - x, centerY - y}, {centerX + y, centerY + x}, {centerX - y, centerY + x}, {centerX + y, centerY - x}, {centerX - y, centerY - x}};

          for (int i = 0; i < 8; i++)
          {
            int px = points[i][0];
            int py = points[i][1];
            if (px >= 0 && px < EPD_WIDTH && py >= 0 && py < EPD_HEIGHT)
            {
              epd_fill_rect(px, py, 1, 1, borderColor, framebuffer);
            }
          }

          if (d < 0)
          {
            d += 2 * x + 3;
          }
          else
          {
            d += 2 * (x - y) + 5;
            y--;
          }
          x++;
        }
      }
    }
  }

  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();

  String response = "Circle drawn at (" + String(centerX) + "," + String(centerY) +
                    ") radius:" + String(radius) + " border:" + (hasBorder ? "yes" : "no") +
                    " filled:" + (isFilled ? "yes" : "no");
  server.send(200, "text/plain", response);
}

// ===== 進階畫矩形控制 =====
void handleDrawRectAdvanced()
{
  Serial.println("handleDrawRectAdvanced");
  if (!framebuffer)
  {
    server.send(400, "text/plain", "Framebuffer not available");
    return;
  }

  // 獲取參數
  int x = server.arg("x").toInt();
  int y = server.arg("y").toInt();
  int width = server.arg("width").toInt();
  int height = server.arg("height").toInt();
  int borderColor = server.arg("borderColor").toInt();
  int fillColor = server.arg("fillColor").toInt();
  int borderThickness = server.arg("borderThickness").toInt();
  bool hasBorder = server.arg("hasBorder").equals("true");
  bool isFilled = server.arg("isFilled").equals("true");

  // 限制參數範圍
  x = constrain(x, 0, EPD_WIDTH - 1);
  y = constrain(y, 0, EPD_HEIGHT - 1);
  width = constrain(width, 1, EPD_WIDTH - x);
  height = constrain(height, 1, EPD_HEIGHT - y);
  borderColor = constrain(borderColor, 0, 15);
  fillColor = constrain(fillColor, 0, 15);
  borderThickness = constrain(borderThickness, 1, min(width, height) / 2);

  epd_poweron();

  // 如果要填滿矩形
  if (isFilled)
  {
    epd_fill_rect(x, y, width, height, fillColor, framebuffer);
  }

  // 如果要繪製邊框
  if (hasBorder)
  {
    for (int t = 0; t < borderThickness; t++)
    {
      // 上邊
      epd_fill_rect(x + t, y + t, width - 2 * t, 1, borderColor, framebuffer);
      // 下邊
      epd_fill_rect(x + t, y + height - 1 - t, width - 2 * t, 1, borderColor, framebuffer);
      // 左邊
      epd_fill_rect(x + t, y + t, 1, height - 2 * t, borderColor, framebuffer);
      // 右邊
      epd_fill_rect(x + width - 1 - t, y + t, 1, height - 2 * t, borderColor, framebuffer);
    }
  }

  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();

  String response = "Rectangle drawn at (" + String(x) + "," + String(y) +
                    ") size:" + String(width) + "x" + String(height) +
                    " border:" + (hasBorder ? "yes" : "no") +
                    " filled:" + (isFilled ? "yes" : "no");
  server.send(200, "text/plain", response);
}

// ===== 路徑繪製輔助函數 =====
void drawPathPoints(String pointsStr, int color, int brushSize, int canvasWidth, int canvasHeight)
{
  unsigned long startTime = millis();
  Serial.printf("[DRAW] Starting path drawing: color=%d, size=%d, canvas=%dx%d\n",
                color, brushSize, canvasWidth, canvasHeight);
  Serial.printf("[DRAW] Points data length: %d chars, preview: '%s'\n",
                pointsStr.length(), pointsStr.substring(0, 50).c_str());

  // 驗證輸入參數
  if (pointsStr.length() == 0)
  {
    Serial.println("[ERROR] Empty points string");
    return;
  }

  if (canvasWidth <= 0 || canvasHeight <= 0)
  {
    Serial.printf("[ERROR] Invalid canvas size: %dx%d\n", canvasWidth, canvasHeight);
    return;
  }

  // 解析點數據：x1,y1|x2,y2|...
  int pointCount = 0;
  int lastX = -1, lastY = -1;
  int startPos = 0;

  for (int i = 0; i <= pointsStr.length(); i++)
  {
    if (i == pointsStr.length() || pointsStr[i] == '|')
    {
      if (i > startPos)
      {
        String pointStr = pointsStr.substring(startPos, i);
        int commaPos = pointStr.indexOf(',');

        if (commaPos > 0)
        {
          int x = pointStr.substring(0, commaPos).toInt();
          int y = pointStr.substring(commaPos + 1).toInt();

          // 映射從Canvas座標到EPD座標
          int epdX = (x * EPD_WIDTH) / canvasWidth;
          int epdY = (y * EPD_HEIGHT) / canvasHeight;

          // 確保座標在有效範圍內
          epdX = constrain(epdX, 0, EPD_WIDTH - 1);
          epdY = constrain(epdY, 0, EPD_HEIGHT - 1);

          if (pointCount == 0)
          {
            // 第一個點，只記錄位置
            lastX = epdX;
            lastY = epdY;
          }
          else
          {
            // 從上一個點畫線到當前點
            drawLine(lastX, lastY, epdX, epdY, color, brushSize);
            lastX = epdX;
            lastY = epdY;
          }

          pointCount++;
        }
      }
      startPos = i + 1;
    }
  }

  unsigned long endTime = millis();
  Serial.printf("[OK] Path drawing completed: %d points in %lu ms\n",
                pointCount, endTime - startTime);
}

// 使用 Bresenham 算法繪製線條
void drawLine(int x0, int y0, int x1, int y1, int color, int thickness)
{
  int dx = abs(x1 - x0);
  int dy = abs(y1 - y0);
  int sx = (x0 < x1) ? 1 : -1;
  int sy = (y0 < y1) ? 1 : -1;
  int err = dx - dy;

  int x = x0;
  int y = y0;

  while (true)
  {
    // 繪製粗線條（以當前點為中心的小圓形）
    for (int dx = -thickness / 2; dx <= thickness / 2; dx++)
    {
      for (int dy = -thickness / 2; dy <= thickness / 2; dy++)
      {
        if (dx * dx + dy * dy <= (thickness * thickness) / 4)
        {
          int px = x + dx;
          int py = y + dy;
          if (px >= 0 && px < EPD_WIDTH && py >= 0 && py < EPD_HEIGHT)
          {
            epd_fill_rect(px, py, 1, 1, color, framebuffer);
          }
        }
      }
    }

    if (x == x1 && y == y1)
      break;

    int e2 = 2 * err;
    if (e2 > -dy)
    {
      err -= dy;
      x += sx;
    }
    if (e2 < dx)
    {
      err += dx;
      y += sy;
    }
  }
}

// ===== Canvas 繪圖數據處理 =====
void handleCanvasData()
{
  Serial.println("[REQUEST] handleCanvasData - Processing canvas data");
  unsigned long startTime = millis();
  Serial.print("[DEBUG] Request method: ");
  Serial.println(server.method() == HTTP_POST ? "POST" : "GET");
  Serial.print("[DEBUG] Content length: ");
  Serial.println(server.header("Content-Length"));

  // 顯示所有接收到的參數
  Serial.printf("[DEBUG] Number of arguments: %d\n", server.args());
  for (int i = 0; i < server.args(); i++)
  {
    String argName = server.argName(i);
    String argValue = server.arg(i);

    // 對於大數據只顯示前後部分
    if (argValue.length() > 100)
    {
      Serial.printf("[DEBUG] Arg %d: %s = [%d chars] %s...%s\n",
                    i, argName.c_str(), argValue.length(),
                    argValue.substring(0, 50).c_str(),
                    argValue.substring(argValue.length() - 50).c_str());
    }
    else
    {
      Serial.printf("[DEBUG] Arg %d: %s = %s\n", i, argName.c_str(), argValue.c_str());
    }
  }

  if (!framebuffer)
  {
    Serial.println("[ERROR] Framebuffer not available");
    server.send(400, "text/plain", "Framebuffer not available");
    return;
  }

  Serial.println("[CANVAS] Framebuffer check passed");

  // 獲取 canvas 數據
  int canvasWidth = server.arg("width").toInt();
  int canvasHeight = server.arg("height").toInt();
  String dataStr = server.arg("data");
  bool isCompressed = server.hasArg("compressed") && server.arg("compressed").equals("1");
  bool isPathData = server.hasArg("paths") && server.arg("paths").equals("1");

  Serial.printf("[CANVAS] Parsed parameters: width=%d, height=%d, data_length=%d\n",
                canvasWidth, canvasHeight, dataStr.length());
  Serial.printf("[CANVAS] Flags: compressed=%s, paths=%s\n",
                isCompressed ? "yes" : "no", isPathData ? "yes" : "no");

  Serial.printf("[DEBUG] isPathData = %s\n", isPathData ? "TRUE" : "FALSE");
  Serial.printf("[DEBUG] server.hasArg('paths') = %s\n", server.hasArg("paths") ? "TRUE" : "FALSE");
  if (server.hasArg("paths"))
  {
    Serial.printf("[DEBUG] server.arg('paths') = '%s'\n", server.arg("paths").c_str());
  }

  // 檢查路徑數據的完整性
  if (isPathData)
  {
    Serial.printf("Raw path data preview (first 200 chars): '%s'\n", dataStr.substring(0, 200).c_str());
    Serial.printf("Raw path data end (last 100 chars): '%s'\n",
                  dataStr.length() > 100 ? dataStr.substring(dataStr.length() - 100).c_str() : dataStr.c_str());

    // 檢查是否有明顯的截斷（最後一個字符應該是數字，不應該在路徑中間）
    int semicolonCount = 0;
    for (int i = 0; i < dataStr.length(); i++)
    {
      if (dataStr[i] == ';')
        semicolonCount++;
    }
    Serial.printf("Found %d path separators (semicolons)\n", semicolonCount);
  } // 檢查數據大小合理性
  if (canvasWidth > 0 && canvasHeight > 0)
  {
    int expectedPixels = canvasWidth * canvasHeight;
    Serial.printf("Expected pixels: %d\n", expectedPixels);

    if (expectedPixels > 100000)
    {
      Serial.printf("ERROR: Canvas too large: %dx%d = %d pixels\n", canvasWidth, canvasHeight, expectedPixels);
      server.send(400, "text/plain", "Canvas size too large");
      return;
    }
  }

  if (dataStr.length() == 0)
  {
    Serial.println("ERROR: No canvas data received");

    // 檢查是否有其他可能的參數名稱
    if (server.hasArg("width"))
    {
      Serial.printf("Width arg exists: %s\n", server.arg("width").c_str());
    }
    else
    {
      Serial.println("Width arg missing");
    }

    if (server.hasArg("height"))
    {
      Serial.printf("Height arg exists: %s\n", server.arg("height").c_str());
    }
    else
    {
      Serial.println("Height arg missing");
    }

    if (server.hasArg("data"))
    {
      Serial.printf("Data arg exists but empty\n");
    }
    else
    {
      Serial.println("Data arg missing completely");
    }

    server.send(400, "text/plain", "No canvas data received");
    return;
  }

  Serial.printf("Canvas data received: %dx%d, data length: %d\n", canvasWidth, canvasHeight, dataStr.length());
  Serial.printf("EPD size: %dx%d\n", EPD_WIDTH, EPD_HEIGHT);

  // 安全檢查
  if (canvasWidth <= 0 || canvasHeight <= 0 || canvasWidth > 2000 || canvasHeight > 2000)
  {
    Serial.printf("ERROR: Invalid canvas size: %dx%d\n", canvasWidth, canvasHeight);
    server.send(400, "text/plain", "Invalid canvas size");
    return;
  }

  epd_poweron();
  Serial.println("EPD powered on for canvas drawing");

  // 清除原有內容
  memset(framebuffer, 0xFF, FB_SIZE);
  Serial.println("Framebuffer cleared");

  int pixelCount = 0;
  int nonWhitePixels = 0;

  Serial.printf("About to check processing mode: isPathData=%s, isCompressed=%s\n",
                isPathData ? "TRUE" : "FALSE", isCompressed ? "TRUE" : "FALSE");

  if (isPathData)
  {
    Serial.println("Processing path data format");

    // 處理路徑格式：P:color:size:points;P:color:size:points;...
    int validPaths = 0;
    int startPos = 0;

    for (int i = 0; i <= dataStr.length(); i++)
    {
      if (i == dataStr.length() || dataStr[i] == ';')
      {
        if (i > startPos)
        {
          String pathStr = dataStr.substring(startPos, i);
          Serial.printf("Processing path string: '%s'\n", pathStr.substring(0, 50).c_str());

          if (pathStr.startsWith("P:"))
          {
            // 解析路徑：P:color:size:points
            int firstColon = pathStr.indexOf(':', 2);
            int secondColon = pathStr.indexOf(':', firstColon + 1);

            Serial.printf("Colon positions: first=%d, second=%d\n", firstColon, secondColon);
            Serial.printf("Full path string length: %d, content: '%s'\n", pathStr.length(), pathStr.c_str());

            if (firstColon > 0 && secondColon > firstColon)
            {
              String colorStr = pathStr.substring(2, firstColon);
              String sizeStr = pathStr.substring(firstColon + 1, secondColon);
              String pointsStr = pathStr.substring(secondColon + 1);

              int color = colorStr.toInt();
              int brushSize = sizeStr.toInt();

              color = constrain(color, 0, 15);
              brushSize = constrain(brushSize, 1, 20);

              Serial.printf("Path: colorStr='%s' sizeStr='%s' color=%d, size=%d, points data length=%d\n",
                            colorStr.c_str(), sizeStr.c_str(), color, brushSize, pointsStr.length());
              Serial.printf("Points string preview: '%s'\n", pointsStr.substring(0, 100).c_str());

              // 檢查點數據是否完整
              if (pointsStr.length() > 0)
              {
                // 處理點數據：x1,y1|x2,y2|...
                drawPathPoints(pointsStr, color, brushSize, canvasWidth, canvasHeight);
                validPaths++;
              }
              else
              {
                Serial.println("ERROR: Empty points data");
              }
            }
            else
            {
              Serial.printf("Invalid path format - insufficient colons (need at least 2)\n");
            }
          }
          else
          {
            Serial.printf("Path string doesn't start with 'P:': '%s'\n", pathStr.substring(0, 10).c_str());
          }
        }
        startPos = i + 1;
      }
    }

    Serial.printf("Processed %d valid paths\n", validPaths);

    if (validPaths == 0)
    {
      Serial.println("ERROR: No valid paths received");
      server.send(400, "text/plain", "No valid paths received");
      epd_poweroff();
      return;
    }

    // 路徑處理完成，直接跳到 EPD 更新
    Serial.println("Path processing complete, updating EPD display...");
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
    Serial.println("EPD display updated and powered off");

    server.send(200, "text/plain", "Path data processed successfully");
    return;
  }
  else if (isCompressed)
  {
    Serial.println("Processing compressed data format");

    // 處理壓縮格式：x,y,color;x,y,color;...
    int startPos = 0;
    for (int i = 0; i <= dataStr.length(); i++)
    {
      if (i == dataStr.length() || dataStr[i] == ';')
      {
        if (i > startPos)
        {
          String pixelStr = dataStr.substring(startPos, i);

          // 解析 x,y,color 格式
          int firstComma = pixelStr.indexOf(',');
          int secondComma = pixelStr.indexOf(',', firstComma + 1);

          if (firstComma > 0 && secondComma > firstComma)
          {
            int x = pixelStr.substring(0, firstComma).toInt();
            int y = pixelStr.substring(firstComma + 1, secondComma).toInt();
            int color = pixelStr.substring(secondComma + 1).toInt();

            // 限制範圍
            color = constrain(color, 0, 15);

            if (x >= 0 && x < canvasWidth && y >= 0 && y < canvasHeight)
            {
              // 映射到 EPD 座標
              int epdX = (x * EPD_WIDTH) / canvasWidth;
              int epdY = (y * EPD_HEIGHT) / canvasHeight;

              // 計算縮放區域大小
              int pixelWidth = max(1, EPD_WIDTH / canvasWidth);
              int pixelHeight = max(1, EPD_HEIGHT / canvasHeight);

              // 填充縮放區域
              for (int dx = 0; dx < pixelWidth && epdX + dx < EPD_WIDTH; dx++)
              {
                for (int dy = 0; dy < pixelHeight && epdY + dy < EPD_HEIGHT; dy++)
                {
                  epd_fill_rect(epdX + dx, epdY + dy, 1, 1, color, framebuffer);
                }
              }

              nonWhitePixels++;
            }
            pixelCount++;
          }
        }
        startPos = i + 1;
      }
    }

    Serial.printf("Compressed format: processed %d non-white pixels\n", pixelCount);
  }
  else
  {
    Serial.println("Processing uncompressed data format");

    // 原始格式處理邏輯
    int startPos = 0;
    for (int i = 0; i <= dataStr.length(); i++)
    {
      if (i == dataStr.length() || dataStr[i] == ',')
      {
        if (i > startPos)
        {
          String pixelStr = dataStr.substring(startPos, i);
          int pixelValue = pixelStr.toInt();

          // 限制像素值範圍
          pixelValue = constrain(pixelValue, 0, 15);

          // 如果是非白色像素，計數
          if (pixelValue != 15)
          {
            nonWhitePixels++;
          }

          // 計算在 EPD 上的位置
          int canvasX = pixelCount % canvasWidth;
          int canvasY = pixelCount / canvasWidth;

          // 直接映射到 EPD 尺寸
          int epdX = (canvasX * EPD_WIDTH) / canvasWidth;
          int epdY = (canvasY * EPD_HEIGHT) / canvasHeight;

          // 確保座標在有效範圍內
          if (epdX >= 0 && epdX < EPD_WIDTH && epdY >= 0 && epdY < EPD_HEIGHT)
          {
            // 計算每個 Canvas 像素對應的 EPD 像素區域大小
            int pixelWidth = max(1, EPD_WIDTH / canvasWidth);
            int pixelHeight = max(1, EPD_HEIGHT / canvasHeight);

            // 填充對應的矩形區域
            for (int dx = 0; dx < pixelWidth && epdX + dx < EPD_WIDTH; dx++)
            {
              for (int dy = 0; dy < pixelHeight && epdY + dy < EPD_HEIGHT; dy++)
              {
                epd_fill_rect(epdX + dx, epdY + dy, 1, 1, pixelValue, framebuffer);
              }
            }
          }

          pixelCount++;
        }
        startPos = i + 1;
      }
    }
  }

  // Debug 輸出
  Serial.printf("Parsed %d pixels, %d non-white pixels\n", pixelCount, nonWhitePixels);

  // 顯示 framebuffer 前 32 bytes 的內容
  Serial.print("Framebuffer sample: ");
  for (int i = 0; i < 32 && i < FB_SIZE; i++)
  {
    Serial.printf("%02X ", framebuffer[i]);
  }
  Serial.println();

  // 顯示一些像素值的樣本
  Serial.print("First 10 pixel values: ");
  int sampleStartPos = 0;
  int sampleCount = 0;
  for (int i = 0; i <= dataStr.length() && sampleCount < 10; i++)
  {
    if (i == dataStr.length() || dataStr[i] == ',')
    {
      if (i > sampleStartPos)
      {
        String pixelStr = dataStr.substring(sampleStartPos, i);
        Serial.print(pixelStr.toInt());
        Serial.print(" ");
        sampleCount++;
      }
      sampleStartPos = i + 1;
    }
  }
  Serial.println();

  // 檢查是否有有效數據 (路徑模式檢查 validPaths，其他模式檢查 pixelCount)
  if (isPathData)
  {
    // 路徑模式：檢查 validPaths (在路徑處理區塊中已經檢查過了)
    Serial.printf("Path mode: Successfully processed paths\n");
  }
  else if (pixelCount == 0)
  {
    Serial.println("ERROR: No valid pixels received");
    epd_poweroff();
    server.send(400, "text/plain", "No valid pixels received");
    return;
  }

  if (isPathData)
  {
    Serial.println("Path mode: Drawing complete, updating EPD");
  }
  else if (nonWhitePixels == 0)
  {
    Serial.println("Warning: All pixels are white (value 15)");
  }
  else
  {
    Serial.printf("Found %d non-white pixels out of %d total\n", nonWhitePixels, pixelCount);
  }

  // 更新 EPD 顯示
  Serial.println("Updating EPD display...");
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();
  Serial.println("EPD display updated and powered off");

  String response = "Canvas data processed: " + String(pixelCount) + " pixels from " +
                    String(canvasWidth) + "x" + String(canvasHeight) + " canvas, " +
                    String(nonWhitePixels) + " non-white pixels";
  server.send(200, "text/plain", response);
  Serial.println("Response sent to client");
}

void handleDrawText()
{
  Serial.println("[REQUEST] handleDrawText - Drawing text");
  unsigned long startTime = millis();

  if (!framebuffer)
  {
    Serial.println("[ERROR] Framebuffer not available");
    server.send(400, "text/plain", "Framebuffer not available");
    return;
  }

  // 獲取參數
  String text = server.arg("text");
  int x = server.arg("x").toInt();
  int y = server.arg("y").toInt();
  int textColor = server.arg("textColor").toInt();
  int bgColor = server.arg("bgColor").toInt();

  Serial.printf("[TEXT] Drawing: '%s' at (%d,%d), colors: text=%d, bg=%d\n",
                text.c_str(), x, y, textColor, bgColor);
  int fontSize = server.arg("fontSize").toInt();

  // 驗證參數
  if (text.length() == 0)
  {
    server.send(400, "text/plain", "Text parameter required");
    return;
  }

  // 限制顏色值範圍 (0-15，0=黑色，15=白色)
  if (textColor < 0)
    textColor = 0;
  if (textColor > 15)
    textColor = 15;
  if (bgColor < 0)
    bgColor = 255; // 255 = 透明背景
  if (bgColor > 15 && bgColor != 255)
    bgColor = 15;

  // 限制字體大小範圍 (1-100)
  if (fontSize < 1)
    fontSize = 1;
  if (fontSize > 100)
    fontSize = 100;

  // 限制座標範圍
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x >= EPD_WIDTH)
    x = EPD_WIDTH - 1;
  if (y >= EPD_HEIGHT)
    y = EPD_HEIGHT - 1;

  epd_poweron();
  epd_draw_text_advanced(text.c_str(), x, y, textColor, bgColor, fontSize, framebuffer);
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();

  String response = "Text drawn: '" + text + "' at (" + String(x) + "," + String(y) +
                    ") size:" + String(fontSize) + " textColor:" + String(textColor) +
                    " bgColor:" + (bgColor == 255 ? "transparent" : String(bgColor));
  server.send(200, "text/plain", response);
}

void handleDrawMultiText()
{
  Serial.println("handleDrawMultiText");
  if (!framebuffer)
  {
    server.send(400, "text/plain", "Framebuffer not available");
    return;
  }

  // 獲取參數（用分號分隔多行文字）
  String texts = server.arg("texts");
  int startX = server.arg("startX").toInt();
  int startY = server.arg("startY").toInt();
  int lineHeight = server.arg("lineHeight").toInt();
  int textColor = server.arg("textColor").toInt();
  int bgColor = server.arg("bgColor").toInt();
  int fontSize = server.arg("fontSize").toInt();

  if (texts.length() == 0)
  {
    server.send(400, "text/plain", "Texts parameter required");
    return;
  }

  if (lineHeight <= 0)
    lineHeight = 20 + (fontSize * 5); // 根據字體大小調整預設行高

  // 限制顏色值範圍
  if (textColor < 0)
    textColor = 0;
  if (textColor > 15)
    textColor = 15;
  if (bgColor < 0)
    bgColor = 255; // 255 = 透明背景
  if (bgColor > 15 && bgColor != 255)
    bgColor = 15;

  // 限制字體大小範圍
  if (fontSize < 1)
    fontSize = 1;
  if (fontSize > 100)
    fontSize = 100;

  epd_poweron();

  // 分割文字並逐行繪製
  int currentY = startY;
  int lineCount = 0;
  int startIndex = 0;

  for (int i = 0; i <= texts.length(); i++)
  {
    if (i == texts.length() || texts[i] == ';')
    {
      if (i > startIndex)
      {
        String line = texts.substring(startIndex, i);
        line.trim();

        if (line.length() > 0 && currentY < EPD_HEIGHT)
        {
          epd_draw_text_advanced(line.c_str(), startX, currentY, textColor, bgColor, fontSize, framebuffer);
          currentY += lineHeight;
          lineCount++;
        }
      }
      startIndex = i + 1;
    }
  }

  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();

  String response = "Multi-text drawn: " + String(lineCount) + " lines starting at (" +
                    String(startX) + "," + String(startY) + ") size:" + String(fontSize) +
                    " textColor:" + String(textColor) + " bgColor:" +
                    (bgColor == 255 ? "transparent" : String(bgColor));
  server.send(200, "text/plain", response);
}

void handleUpload()
{
  Serial.println("handleUpload");
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    Serial.printf("Upload: %s\n", upload.filename.c_str());
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (framebuffer && upload.currentSize <= FB_SIZE)
    {
      memcpy(framebuffer + upload.totalSize - upload.currentSize, upload.buf, upload.currentSize);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (framebuffer)
    {
      epd_poweron();
      epd_draw_grayscale_image(epd_full_screen(), framebuffer);
      epd_poweroff();
    }
    Serial.printf("Upload complete, size: %d\n", upload.totalSize);
  }
  server.send(200, "text/plain", "Upload complete");
}

// ===== 灰階圖片數據處理 =====
void handleGrayscaleData()
{
  Serial.println("handleGrayscaleData");
  if (!framebuffer)
  {
    server.send(400, "text/plain", "Framebuffer not available");
    return;
  }

  // 獲取參數
  int x = server.arg("x").toInt();
  int y = server.arg("y").toInt();
  int width = server.arg("width").toInt();
  int height = server.arg("height").toInt();
  String dataStr = server.arg("data");

  Serial.printf("Received grayscale data: x=%d, y=%d, size=%dx%d, data_length=%d\n",
                x, y, width, height, dataStr.length());

  // 驗證參數
  if (width <= 0 || height <= 0 || dataStr.length() == 0)
  {
    server.send(400, "text/plain", "Invalid parameters");
    return;
  }

  // 限制座標範圍
  x = constrain(x, 0, EPD_WIDTH - 1);
  y = constrain(y, 0, EPD_HEIGHT - 1);

  // 限制尺寸以免超出螢幕邊界
  if (x + width > EPD_WIDTH)
    width = EPD_WIDTH - x;
  if (y + height > EPD_HEIGHT)
    height = EPD_HEIGHT - y;

  // 解析數據
  int expectedCount = width * height;
  int dataIndex = 0;
  int pixelIndex = 0;
  String currentValue = "";

  Serial.printf("Expected pixel count: %d\n", expectedCount);

  epd_poweron();

  // 解析逗號分隔的灰階值
  for (int i = 0; i <= dataStr.length(); i++)
  {
    if (i == dataStr.length() || dataStr[i] == ',')
    {
      if (currentValue.length() > 0)
      {
        int grayValue = currentValue.toInt();

        // 限制灰階值範圍 (0-15)
        grayValue = constrain(grayValue, 0, 15);

        // 計算在 EPD 上的像素位置
        int pixelX = x + (pixelIndex % width);
        int pixelY = y + (pixelIndex / width);

        // 檢查是否在有效範圍內
        if (pixelX < EPD_WIDTH && pixelY < EPD_HEIGHT && pixelIndex < expectedCount)
        {
          // 在 framebuffer 中設置像素值
          epd_fill_rect(pixelX, pixelY, 1, 1, grayValue, framebuffer);
        }

        pixelIndex++;
        currentValue = "";
      }
    }
    else
    {
      currentValue += dataStr[i];
    }
  }

  Serial.printf("Processed %d pixels\n", pixelIndex);

  // 顯示到 EPD
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();

  String response = "Grayscale image drawn at (" + String(x) + "," + String(y) +
                    ") size " + String(width) + "x" + String(height) +
                    ", processed " + String(pixelIndex) + " pixels";
  server.send(200, "text/plain", response);
}

// ===== 智能圖片控制上傳處理函數 =====

void handleUploadImage()
{
  debugLog("IMAGE", "開始處理智能圖片上傳");
  String clientIP = server.client().remoteIP().toString();
  Serial.printf("[IMAGE] Request from IP: %s\n", clientIP.c_str());
  Serial.printf("[MEMORY] Free heap before processing: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("[MEMORY] Free PSRAM before processing: %d bytes\n", ESP.getFreePsram());

  unsigned long startTime = millis();

  if (!server.hasArg("plain"))
  {
    debugLog("ERROR", "沒有接收到圖片資料");
    sendTextResponse(400, "沒有圖片資料");
    return;
  }

  String jsonData = server.arg("plain");
  Serial.printf("[IMAGE] 接收到資料大小: %d bytes\n", jsonData.length());

  // 檢查數據大小限制
  if (jsonData.length() > 1000000) // 1MB 限制
  {
    debugLog("ERROR", "接收數據過大");
    sendTextResponse(413, "數據過大，請縮小圖片");
    return;
  }

  // 解析圖片參數
  ImageParams params;
  if (!parseImageParams(jsonData, params))
  {
    debugLog("ERROR", "圖片參數解析失敗");
    sendTextResponse(400, "參數解析失敗");
    return;
  }

  // 檢查圖片尺寸限制
  if (params.width * params.height > 100000) // 最大10萬像素
  {
    debugLog("ERROR", "圖片像素過多");
    String errorMsg = "圖片過大: " + String(params.width * params.height) + " 像素 > 100,000 限制";
    freeImageParams(params);
    sendTextResponse(413, errorMsg);
    return;
  }

  // 記錄圖片處理日誌
  logImageProcessing(params);

  // 記憶體檢查
  size_t freeHeap = ESP.getFreeHeap();
  size_t freePsram = ESP.getFreePsram();
  size_t imageSize = params.dataSize;

  Serial.printf("[MEMORY] Image size: %d bytes, Free heap: %d, Free PSRAM: %d\n",
                imageSize, freeHeap, freePsram);

  if (freeHeap < 50000 || freePsram < imageSize * 3) // 更嚴格的記憶體檢查
  {
    debugLog("ERROR", "記憶體不足，無法處理圖片");
    String memError = "記憶體不足 - 需要: " + String(imageSize * 3) + " 可用: " + String(freePsram);
    freeImageParams(params);
    sendTextResponse(507, memError);
    return;
  }

  // 設置處理超時
  unsigned long maxProcessingTime = 30000; // 30秒最大處理時間
  server.client().setTimeout(35000);       // 35秒客戶端超時

  // 渲染圖片到EPD
  if (renderImageToEPD(params))
  {
    unsigned long processingTime = millis() - startTime;
    Serial.printf("[IMAGE] 圖片處理完成，耗時 %lu ms\n", processingTime);
    debugLog("OK", "圖片已成功顯示到EPD");

    String response = "圖片上傳成功 - 位置:(" + String(params.x) + "," + String(params.y) +
                      ") 尺寸:" + String(params.width) + "x" + String(params.height) +
                      " 處理時間:" + String(processingTime) + "ms" +
                      " 記憶體使用:" + String(imageSize) + "bytes";
    sendTextResponse(200, response);
  }
  else
  {
    debugLog("ERROR", "圖片渲染失敗");
    sendTextResponse(500, "圖片渲染失敗");
  }

  // 清理記憶體
  freeImageParams(params);

  Serial.printf("[MEMORY] Free heap after processing: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("[MEMORY] Free PSRAM after processing: %d bytes\n", ESP.getFreePsram());
}

// 解析圖片參數
bool parseImageParams(const String &jsonData, ImageParams &params)
{
  debugLog("PARSE", "開始解析圖片參數");

  // 簡單的JSON解析 (可考慮使用ArduinoJson庫以獲得更好的解析能力)
  int xPos = jsonData.indexOf("\"x\":") + 4;
  int xEnd = jsonData.indexOf(",", xPos);
  if (xPos < 4 || xEnd < xPos)
    return false;
  params.x = jsonData.substring(xPos, xEnd).toInt();

  int yPos = jsonData.indexOf("\"y\":") + 4;
  int yEnd = jsonData.indexOf(",", yPos);
  if (yPos < 4 || yEnd < yPos)
    return false;
  params.y = jsonData.substring(yPos, yEnd).toInt();

  int wPos = jsonData.indexOf("\"width\":") + 8;
  int wEnd = jsonData.indexOf(",", wPos);
  if (wPos < 8 || wEnd < wPos)
    return false;
  params.width = jsonData.substring(wPos, wEnd).toInt();

  int hPos = jsonData.indexOf("\"height\":") + 9;
  int hEnd = jsonData.indexOf(",", hPos);
  if (hPos < 9 || hEnd < hPos)
    return false;
  params.height = jsonData.substring(hPos, hEnd).toInt();

  // 解析其他參數
  int contrastPos = jsonData.indexOf("\"contrast\":") + 11;
  int contrastEnd = jsonData.indexOf(",", contrastPos);
  if (contrastPos >= 11 && contrastEnd > contrastPos)
  {
    params.contrast = jsonData.substring(contrastPos, contrastEnd).toFloat();
  }
  else
  {
    params.contrast = 1.0;
  }

  int brightnessPos = jsonData.indexOf("\"brightness\":") + 13;
  int brightnessEnd = jsonData.indexOf(",", brightnessPos);
  if (brightnessPos >= 13 && brightnessEnd > brightnessPos)
  {
    params.brightness = jsonData.substring(brightnessPos, brightnessEnd).toInt();
  }
  else
  {
    params.brightness = 0;
  }

  int grayLevelsPos = jsonData.indexOf("\"grayLevels\":") + 13;
  int grayLevelsEnd = jsonData.indexOf(",", grayLevelsPos);
  if (grayLevelsPos >= 13 && grayLevelsEnd > grayLevelsPos)
  {
    params.grayLevels = jsonData.substring(grayLevelsPos, grayLevelsEnd).toInt();
  }
  else
  {
    params.grayLevels = 16;
  }

  // 解析檔案名稱
  int filenamePos = jsonData.indexOf("\"filename\":\"") + 12;
  int filenameEnd = jsonData.indexOf("\"", filenamePos);
  if (filenamePos >= 12 && filenameEnd > filenamePos)
  {
    params.filename = jsonData.substring(filenamePos, filenameEnd);
  }
  else
  {
    params.filename = "unknown";
  }

  // 參數驗證
  if (params.x < 0 || params.y < 0 || params.width <= 0 || params.height <= 0)
  {
    debugLog("ERROR", "無效的位置或尺寸參數");
    return false;
  }

  if (params.x + params.width > EPD_WIDTH || params.y + params.height > EPD_HEIGHT)
  {
    Serial.printf("[WARNING] 圖片超出顯示範圍: (%d,%d) %dx%d\n",
                  params.x, params.y, params.width, params.height);
    // 自動裁剪到合理範圍
    params.width = min(params.width, EPD_WIDTH - params.x);
    params.height = min(params.height, EPD_HEIGHT - params.y);
    debugLog("WARNING", "圖片已自動裁剪至顯示範圍內");
  }

  // 解析灰階資料
  int dataStart = jsonData.indexOf("\"grayData\":[") + 12;
  int dataEnd = jsonData.indexOf("]", dataStart);

  if (dataStart < 12 || dataEnd < dataStart)
  {
    debugLog("ERROR", "找不到灰階資料");
    return false;
  }

  String dataStr = jsonData.substring(dataStart, dataEnd);
  params.dataSize = params.width * params.height;

  // 使用PSRAM分配記憶體
  params.grayData = (uint8_t *)ps_malloc(params.dataSize);
  if (!params.grayData)
  {
    debugLog("ERROR", "無法分配PSRAM記憶體給圖片資料");
    return false;
  }

  // 解析逗號分隔的數值
  int index = 0;
  int pos = 0;

  while (pos < dataStr.length() && index < params.dataSize)
  {
    int nextComma = dataStr.indexOf(',', pos);
    if (nextComma == -1)
      nextComma = dataStr.length();

    String valueStr = dataStr.substring(pos, nextComma);
    valueStr.trim();

    if (valueStr.length() > 0)
    {
      int value = valueStr.toInt();
      params.grayData[index] = constrain(value, 0, 15);
      index++;
    }

    pos = nextComma + 1;
  }

  if (index != params.dataSize)
  {
    debugLog("ERROR", "灰階資料數量不匹配");
    Serial.printf("[ERROR] Expected %d pixels, got %d\n", params.dataSize, index);
    free(params.grayData);
    return false;
  }

  Serial.printf("[PARSE] 解析完成: 位置(%d,%d) 尺寸%dx%d 資料%d像素\n",
                params.x, params.y, params.width, params.height, index);
  debugLog("OK", "圖片參數解析成功");

  return true;
}

// 渲染圖片到EPD
bool renderImageToEPD(const ImageParams &params)
{
  if (!framebuffer)
  {
    debugLog("ERROR", "Framebuffer 未初始化");
    return false;
  }

  debugLog("DISPLAY", "開始渲染圖片到EPD");
  Serial.printf("[DISPLAY] 渲染區域: (%d,%d) %dx%d\n",
                params.x, params.y, params.width, params.height);

  unsigned long renderStart = millis();

  // 逐像素渲染到framebuffer
  int pixelsProcessed = 0;
  for (int y = 0; y < params.height; y++)
  {
    for (int x = 0; x < params.width; x++)
    {
      int srcIndex = y * params.width + x;
      int dstX = params.x + x;
      int dstY = params.y + y;

      if (srcIndex < params.dataSize &&
          dstX >= 0 && dstX < EPD_WIDTH &&
          dstY >= 0 && dstY < EPD_HEIGHT)
      {

        uint8_t grayLevel = params.grayData[srcIndex];
        epd_draw_pixel(dstX, dstY, grayLevel, framebuffer);
        pixelsProcessed++;
      }
    }

    // 每100行輸出一次進度（避免過多日誌）
    if (y % 100 == 0 && y > 0)
    {
      Serial.printf("[RENDER] Progress: %d/%d rows\n", y, params.height);
    }
  }

  unsigned long renderTime = millis() - renderStart;
  Serial.printf("[RENDER] Framebuffer updated: %d pixels in %lu ms\n",
                pixelsProcessed, renderTime);

  // 更新EPD顯示
  debugLog("DISPLAY", "更新EPD顯示");
  unsigned long displayStart = millis();

  // 定義更新區域
  Rect_t updateArea = {
      .x = params.x,
      .y = params.y,
      .width = params.width,
      .height = params.height};

  epd_poweron();

  // 清除指定區域
  epd_clear_area(updateArea);

  // 繪製灰階圖像
  epd_draw_grayscale_image(updateArea, framebuffer);

  epd_poweroff();

  unsigned long displayTime = millis() - displayStart;
  Serial.printf("[DISPLAY] EPD update completed in %lu ms\n", displayTime);

  debugLog("OK", "圖片渲染到EPD完成");
  return true;
}

// 釋放圖片參數記憶體
void freeImageParams(ImageParams &params)
{
  if (params.grayData)
  {
    free(params.grayData);
    params.grayData = NULL;
    debugLog("MEMORY", "圖片資料記憶體已釋放");
  }
}

// 記錄圖片處理日誌
void logImageProcessing(const ImageParams &params)
{
  Serial.println("[IMAGE] ===== 圖片處理資訊 =====");
  Serial.printf("[IMAGE] 檔案名稱: %s\n", params.filename.c_str());
  Serial.printf("[IMAGE] 位置: (%d, %d)\n", params.x, params.y);
  Serial.printf("[IMAGE] 尺寸: %d × %d 像素\n", params.width, params.height);
  Serial.printf("[IMAGE] 資料大小: %d bytes\n", params.dataSize);
  Serial.printf("[IMAGE] 對比度: %.2f\n", params.contrast);
  Serial.printf("[IMAGE] 亮度: %d\n", params.brightness);
  Serial.printf("[IMAGE] 灰階級數: %d\n", params.grayLevels);
  Serial.printf("[IMAGE] 反相: %s\n", params.inverted ? "是" : "否");
  Serial.println("[IMAGE] ========================");
}

void notFound()
{
  String uri = server.uri();
  String method = (server.method() == HTTP_GET) ? "GET" : (server.method() == HTTP_POST) ? "POST"
                                                                                         : "OTHER";

  Serial.print("[404] Request not found: ");
  Serial.print(method);
  Serial.print(" ");
  Serial.println(uri);

  if (server.args() > 0)
  {
    Serial.println("[404] Request arguments:");
    for (int i = 0; i < server.args(); i++)
    {
      Serial.printf("  %s = %s\n", server.argName(i).c_str(), server.arg(i).c_str());
    }
  }

  // 使用輔助函數發送 UTF-8 響應
  sendTextResponse(404, "Not found: " + method + " " + uri);
}

// ===== 函數宣告 =====
void drawLine(int x0, int y0, int x1, int y1, int color, int thickness);

// ===== Setup =====
void setup()
{
  Serial.begin(115200);
  delay(2000); // 增加延遲確保序列埠穩定

  Serial.println("=== EPD Controller Starting ===");
  Serial.print("Free heap at start: ");
  Serial.println(ESP.getFreeHeap());
  Serial.print("Free PSRAM at start: ");
  Serial.println(ESP.getFreePsram());

  // 初始化 framebuffer
  Serial.println("[INIT] Initializing framebuffer...");
  Serial.print("Required framebuffer size: ");
  Serial.println(FB_SIZE);

  framebuffer = (uint8_t *)ps_calloc(1, FB_SIZE);
  if (!framebuffer)
  {
    Serial.println("[ERROR] PSRAM alloc failed!");
    Serial.print("Free PSRAM: ");
    Serial.println(ESP.getFreePsram());
    while (1)
      delay(100);
  }
  memset(framebuffer, 0xFF, FB_SIZE);
  Serial.println("[OK] Framebuffer initialized successfully");
  Serial.print("Free PSRAM after allocation: ");
  Serial.println(ESP.getFreePsram());

  // 初始化 EPD
  Serial.println("[INIT] Initializing EPD...");
  epd_init();
  Serial.println("[OK] EPD init completed");

  Serial.println("[INIT] Powering on EPD...");
  epd_poweron();
  Serial.println("[OK] EPD powered on");

  Serial.println("[INIT] Clearing EPD display...");
  epd_clear();
  Serial.println("[OK] EPD cleared");

  Serial.println("[INIT] Powering off EPD...");
  epd_poweroff();
  Serial.println("[OK] EPD powered off");

  // 啟動 Wi-Fi AP
  Serial.println("[WIFI] Starting WiFi AP...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);

  bool apResult = WiFi.softAP(ssid, password);
  if (!apResult)
  {
    Serial.println("[ERROR] Failed to start WiFi AP!");
    return;
  }

  IPAddress IP = WiFi.softAPIP();
  String ipStr = "http://" + IP.toString() + ":80";

  Serial.println("[OK] WiFi AP started successfully");
  Serial.print("AP IP address: ");
  Serial.println(IP.toString());
  Serial.print("Full URL: ");
  Serial.println(ipStr);

  // 在 EPD 左上角顯示 IP 地址
  Serial.println("[DISPLAY] Drawing IP on EPD...");
  Serial.println("[DISPLAY] Powering on EPD for IP display...");
  epd_poweron();

  // 方法1: 使用簡化的 IP 顯示（推薦）
  String simpleIP = IP.toString() + ":80";
  Serial.print("[DISPLAY] Drawing IP: ");
  Serial.println(simpleIP);
  draw_ip_simple(10, 10, simpleIP.c_str(), 0, framebuffer);

  // 在下方顯示 SSID
  Serial.print("[DISPLAY] Drawing SSID: ");
  Serial.println(ssid);
  draw_ip_simple(10, 60, ssid, 0, framebuffer);

  // 更新 EPD 顯示
  Serial.println("[DISPLAY] Updating EPD display...");
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  Serial.println("[DISPLAY] Powering off EPD...");
  epd_poweroff();
  Serial.println("[OK] IP displayed on EPD successfully");

  // 設定 Web Server 路由
  Serial.println("[SERVER] Setting up web server routes...");
  server.on("/", HTTP_GET, handleRoot);
  server.on("/clear", HTTP_GET, handleClear);
  server.on("/draw/line", HTTP_GET, handleDrawLine);
  server.on("/draw/line/advanced", HTTP_GET, handleDrawLineAdvanced);
  server.on("/draw/rect", HTTP_GET, handleDrawRect);
  server.on("/draw/rect/advanced", HTTP_GET, handleDrawRectAdvanced);
  server.on("/draw/circle", HTTP_GET, handleDrawCircle);
  server.on("/draw/circle/advanced", HTTP_GET, handleDrawCircleAdvanced);
  server.on("/draw/text", HTTP_GET, handleDrawText);
  server.on("/draw/multitext", HTTP_GET, handleDrawMultiText);
  server.on("/draw/canvas", HTTP_POST, handleCanvasData);
  server.on("/draw/grayscale", HTTP_POST, handleGrayscaleData);
  server.on("/upload", HTTP_POST, []()
            { server.send(200); }, handleUpload);

  // 智能圖片控制上傳路由
  server.on("/upload-image", HTTP_POST, handleUploadImage);

  server.onNotFound(notFound);
  Serial.println("[OK] All routes configured (包含智能圖片上傳)");

  // 設定 WebServer 的緩衝區大小以處理大型 POST 數據
  const char *headerKeys[] = {"Content-Length"};
  server.collectHeaders(headerKeys, 1);

  // 增加 WebServer 的緩衝區大小限制
  // 預設可能只有 1KB，我們需要更大的緩衝區來處理路徑數據
  Serial.println("[SERVER] Configuring WebServer for large POST data...");

  Serial.println("[SERVER] Starting web server...");
  server.begin();
  Serial.println("[OK] Web server started successfully");

  Serial.println("=== Setup Complete ===");
  Serial.print("Connect to WiFi: ");
  Serial.println(ssid);
  Serial.print("Open browser: ");
  Serial.println(ipStr);
  Serial.print("Final free heap: ");
  Serial.println(ESP.getFreeHeap());
  Serial.print("Final free PSRAM: ");
  Serial.println(ESP.getFreePsram());
  Serial.println("=== System Ready ===");
}

// ===== Loop =====
void loop()
{
  server.handleClient();

  // 每10秒輸出一次心跳信號，確認程式在運行
  static unsigned long lastHeartbeat = 0;
  static int clientCount = 0;

  if (millis() - lastHeartbeat > 10000)
  {
    int currentClients = WiFi.softAPgetStationNum();

    Serial.println("=== System Status ===");
    Serial.print("Uptime: ");
    Serial.print(millis() / 1000);
    Serial.println(" seconds");
    Serial.print("Connected clients: ");
    Serial.println(currentClients);
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
    Serial.print("Free PSRAM: ");
    Serial.println(ESP.getFreePsram());

    if (currentClients != clientCount)
    {
      Serial.print("[WIFI] Client count changed: ");
      Serial.print(clientCount);
      Serial.print(" -> ");
      Serial.println(currentClients);
      clientCount = currentClients;
    }

    lastHeartbeat = millis();
  }

  delay(1); // 讓出 CPU
}