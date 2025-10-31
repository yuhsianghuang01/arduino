/**
 * EPD Web Controller via Wi-Fi AP
 * - Starts AP mode
 * - Shows IP:Port on EPD top-left
 * - Web interface to draw shapes or upload image
 *
 * ==========================================
 * LilyGo T5 EPD47 網頁控制器與遊戲平台
 * ==========================================
 *
 * 程式功能：
 * 這是一個功能豐富的電子紙網頁控制器程式，整合了多種互動功能：
 *
 * 🌐 網路功能：
 * - WiFi 熱點模式（AP Mode）
 * - 內建 HTTP 網頁伺服器
 * - 圖形繪製網頁介面
 * - 圖片上傳與顯示功能
 *
 * 🎮 遊戲功能：
 * - Chrome 小恐龍跳躍遊戲
 * - 彈球遊戲
 * - 推箱子遊戲（Sokoban）
 * - 觸控與網頁雙重控制
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
 * 4. 透過網頁介面控制繪圖或玩遊戲
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

// 嘗試包含字體檔案（如果可用）
// 常見的 EPD 字體檔案
#ifdef EPD_FONT_OPENSANS18B_H
#include "opensans18b.h" // 18pt 粗體字型
#endif
#ifdef EPD_FONT_OPENSANS12B_H
#include "opensans12b.h" // 12pt 粗體字型
#endif

// 如果沒有字體檔案，我們將使用圖形方式顯示數字和字母

// ===== WiFi AP 設定 =====
const char *ssid = "EPD-Controller"; // WiFi 熱點名稱
const char *password = "12345678";   // WiFi 密碼（至少8碼）

// ===== 網頁伺服器 =====
WebServer server(80); // 建立 HTTP 伺服器，監聽埠 80

// ===== 影像緩衝區 =====
uint8_t *framebuffer = NULL;                    // 影像緩衝區指標
const int FB_SIZE = EPD_WIDTH * EPD_HEIGHT / 2; // 2 位元灰階緩衝區大小

// ===== 遊戲狀態變數 =====
enum GameType // 遊戲類型列舉
{
  GAME_NONE,   // 無遊戲
  GAME_DINO,   // 小恐龍遊戲
  GAME_BALL,   // 彈球遊戲
  GAME_SOKOBAN // 推箱子遊戲
};
GameType currentGame = GAME_NONE; // 目前遊戲狀態

// Chrome小恐龍遊戲狀態結構
struct DinoGame // 小恐龍遊戲結構
{
  int x = 100;
  int y = 450; // 地面位置
  int groundY = 450;
  bool isJumping = false;
  bool isCrouching = false;
  int jumpHeight = 0;
  int score = 0;
  unsigned long lastUpdate = 0;
  unsigned long lastObstacle = 0;
  struct Obstacle
  {
    int x = EPD_WIDTH;
    int y = 450;
    int width = 20;
    int height = 30;
    bool active = false;
  } obstacles[3];
} dinoGame;

// 彈球遊戲狀態
struct BallGame
{
  float x = EPD_WIDTH / 2;
  float y = EPD_HEIGHT / 2;
  float vx = 2.0;
  float vy = 1.5;
  int radius = 8;
  unsigned long lastUpdate = 0;
} ballGame;

// 推箱子遊戲狀態
struct SokobanGame
{
  int playerX = 4;
  int playerY = 3;
  int moves = 0;
  struct Box
  {
    int x, y;
    bool onTarget = false;
  };
  Box boxes[2];
  struct Target
  {
    int x, y;
  };
  Target targets[2];
  char level[8][10] = {
      "#########",
      "#.......#",
      "#..###..#",
      "#..#.#..#",
      "#..#.#..#",
      "#..###..#",
      "#.......#",
      "#########"};
} sokobanGame;

// ===== 函數聲明 =====
void drawDinoGame(bool forceClear = false);
void drawBallGame(bool forceClear = false);
void drawSokobanGame(bool forceClear = false);
void updateDinoGame();
void updateBallGame();
void updateCurrentGame();

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

// ===== 簡化的 IP 顯示函式（只顯示數字和點號）=====
void draw_ip_simple(int x, int y, const char *ip_str, uint8_t color, uint8_t *fb)
{
  int current_x = x;
  int char_spacing = 8; // 字符間距

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

      draw_char_5x7(current_x, y, c, color, fb);
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
  Serial.println("handleRoot");
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>EPD Controller</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; padding: 10px; }
    button { margin:5px; padding:10px; font-size:16px; }
    .upload, .text-control { margin-top:20px; padding:15px; border:1px solid #ccc; border-radius:5px; }
    .form-row { margin:10px 0; }
    label { display:inline-block; width:80px; }
    input[type="text"], input[type="number"] { padding:5px; margin:5px; }
    input[type="range"] { width:200px; }
    .color-value { font-weight:bold; }
    
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
      <textarea id="grayscaleData" placeholder="貼入灰階數據，格式: 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,..." rows="6" style="width:100%; max-width:600px; font-family:monospace;"></textarea>
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
      currentBrushSize = document.getElementById('brushSize').value;
      document.getElementById('brushSizeValue').textContent = currentBrushSize;
    }
    
    function updateBrushColor() {
      currentBrushColor = document.getElementById('brushColor').value;
      const colorName = getColorName(currentBrushColor);
      document.getElementById('brushColorValue').textContent = currentBrushColor + ' (' + colorName + ')';
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
      const color = document.getElementById('textColor').value;
      const colorNames = ['黑色', '深灰', '中灰', '淺灰', '白色'];
      let colorName;
      if (color == 0) colorName = '黑色';
      else if (color <= 3) colorName = '深灰';
      else if (color <= 7) colorName = '中灰';
      else if (color <= 11) colorName = '淺灰';
      else colorName = '白色';
      
      document.getElementById('textColorValue').textContent = color + ' (' + colorName + ')';
    }
    
    function updateBgColorValue() {
      const color = document.getElementById('bgColor').value;
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
      
      document.getElementById('bgColorValue').textContent = colorName;
    }
    
    function updateFontSizeValue() {
      const size = document.getElementById('fontSize').value;
      document.getElementById('fontSizeValue').textContent = size + ' (像素倍數)';
    }
    
    function updateLineColorValue() {
      const color = document.getElementById('lineColor').value;
      const colorName = getColorName(color);
      document.getElementById('lineColorValue').textContent = color + ' (' + colorName + ')';
    }
    
    function updateLineThicknessValue() {
      const thickness = document.getElementById('lineThickness').value;
      document.getElementById('lineThicknessValue').textContent = thickness + ' 像素';
    }
    
    function updateCircleBorderColorValue() {
      const color = document.getElementById('circleBorderColor').value;
      const colorName = getColorName(color);
      document.getElementById('circleBorderColorValue').textContent = color + ' (' + colorName + ')';
    }
    
    function updateCircleBorderThicknessValue() {
      const thickness = document.getElementById('circleBorderThickness').value;
      document.getElementById('circleBorderThicknessValue').textContent = thickness + ' 像素';
    }
    
    function updateCircleFillColorValue() {
      const color = document.getElementById('circleFillColor').value;
      const colorName = getColorName(color);
      document.getElementById('circleFillColorValue').textContent = color + ' (' + colorName + ')';
    }
    
    function updateRectBorderColorValue() {
      const color = document.getElementById('rectBorderColor').value;
      const colorName = getColorName(color);
      document.getElementById('rectBorderColorValue').textContent = color + ' (' + colorName + ')';
    }
    
    function updateRectBorderThicknessValue() {
      const thickness = document.getElementById('rectBorderThickness').value;
      document.getElementById('rectBorderThicknessValue').textContent = thickness + ' 像素';
    }
    
    function updateRectFillColorValue() {
      const color = document.getElementById('rectFillColor').value;
      const colorName = getColorName(color);
      document.getElementById('rectFillColorValue').textContent = color + ' (' + colorName + ')';
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
    
    // ===== 遊戲相關變數 =====
    let currentGame = null;
    let gameState = {
      dino: { x: 50, y: 400, isJumping: false, isCrouching: false, score: 0 },
      ball: { x: 400, y: 300, vx: 3, vy: 2 },
      sokoban: { playerX: 4, playerY: 3, boxes: [{x:3,y:3}, {x:5,y:3}], moves: 0 }
    };
    
    // 切換遊戲模式
    function switchGame(gameType) {
      currentGame = gameType;
      fetch('/game/switch?type=' + gameType)
        .then(response => response.text())
        .then(data => {
          console.log('Game switched:', data);
          updateGameDisplay();
        });
    }
    
    // 更新遊戲顯示資訊
    function updateGameDisplay() {
      const statusDiv = document.getElementById('gameStatus');
      if (currentGame === 'dino') {
        statusDiv.innerHTML = '<h4>🦕 Chrome小恐龍遊戲</h4><p>分數: ' + gameState.dino.score + '</p>';
      } else if (currentGame === 'ball') {
        statusDiv.innerHTML = '<h4>⚽ 彈球遊戲</h4><p>球的位置: (' + Math.round(gameState.ball.x) + ', ' + Math.round(gameState.ball.y) + ')</p>';
      } else if (currentGame === 'sokoban') {
        statusDiv.innerHTML = '<h4>📦 推箱子遊戲</h4><p>移動次數: ' + gameState.sokoban.moves + '</p>';
      } else {
        statusDiv.innerHTML = '<h4>請選擇一個遊戲</h4>';
      }
    }
    
    // Chrome小恐龍控制
    function dinoJump() {
      if (currentGame !== 'dino') return;
      gameState.dino.isJumping = true;
      fetch('/game/dino/jump')
        .then(response => response.text())
        .then(data => {
          console.log('Dino jumped:', data);
          setTimeout(() => { gameState.dino.isJumping = false; }, 1000);
          updateGameDisplay();
        });
    }
    
    function dinoCrouch() {
      if (currentGame !== 'dino') return;
      gameState.dino.isCrouching = true;
      fetch('/game/dino/crouch')
        .then(response => response.text())
        .then(data => {
          console.log('Dino crouched:', data);
          updateGameDisplay();
        });
    }
    
    function dinoStandUp() {
      if (currentGame !== 'dino') return;
      gameState.dino.isCrouching = false;
      fetch('/game/dino/standup')
        .then(response => response.text())
        .then(data => {
          console.log('Dino stood up:', data);
          updateGameDisplay();
        });
    }
    
    // 推箱子控制
    function moveSokoban(direction) {
      if (currentGame !== 'sokoban') return;
      gameState.sokoban.moves++;
      fetch('/game/sokoban/move?dir=' + direction)
        .then(response => response.text())
        .then(data => {
          console.log('Sokoban moved:', data);
          updateGameDisplay();
        });
    }
    
    // 遊戲狀態更新（定期從伺服器獲取）
    function updateGameState() {
      if (currentGame) {
        fetch('/game/state')
          .then(response => response.json())
          .then(data => {
            if (data.game === currentGame) {
              Object.assign(gameState[currentGame], data.state);
              updateGameDisplay();
            }
          })
          .catch(err => console.log('Game state update failed:', err));
      }
    }
    
    // 定期更新遊戲狀態
    setInterval(updateGameState, 1000);
    
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
      initCanvas();
      updateGameDisplay();
      initImageConverter();
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
      if (currentWidth === 64 && currentHeight === 64) {
        presetButtons[0].classList.add('active');
      } else if (currentWidth === 128 && currentHeight === 128) {
        presetButtons[1].classList.add('active');
      } else if (currentWidth === 320 && currentHeight === 240) {
        presetButtons[2].classList.add('active');
      } else if (currentWidth === 480 && currentHeight === 800) {
        presetButtons[3].classList.add('active');
      } else if (currentWidth === 540 && currentHeight === 960) {
        presetButtons[4].classList.add('active');
      } else if (currentWidth === 800 && currentHeight === 600) {
        presetButtons[5].classList.add('active');
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
  </script>

  <!-- 遊戲控制區域 -->
  <div class="text-control">
    <h3>🎮 EPD遊戲控制中心</h3>
    <p>選擇遊戲後，遊戲畫面會顯示在EPD屏幕上，用手機控制操作！</p>
    
    <!-- 遊戲切換按鈕 -->
    <div style="margin: 15px 0; text-align: center;">
      <button onclick="switchGame('dino')" style="background-color: #4CAF50; color: white; padding: 15px 25px; margin: 8px; border: none; border-radius: 8px; cursor: pointer; font-size: 18px; font-weight: bold;">
        🦕 Chrome小恐龍
      </button>
      <button onclick="switchGame('ball')" style="background-color: #2196F3; color: white; padding: 15px 25px; margin: 8px; border: none; border-radius: 8px; cursor: pointer; font-size: 18px; font-weight: bold;">
        ⚽ 彈球遊戲
      </button>
      <button onclick="switchGame('sokoban')" style="background-color: #FF9800; color: white; padding: 15px 25px; margin: 8px; border: none; border-radius: 8px; cursor: pointer; font-size: 18px; font-weight: bold;">
        📦 推箱子
      </button>
      <button onclick="switchGame(null)" style="background-color: #f44336; color: white; padding: 15px 25px; margin: 8px; border: none; border-radius: 8px; cursor: pointer; font-size: 18px; font-weight: bold;">
        ⏹️ 停止遊戲
      </button>
    </div>

    <!-- 遊戲狀態顯示 -->
    <div id="gameStatus" style="margin: 20px 0; padding: 15px; background-color: #f0f0f0; border-radius: 8px; text-align: center;">
      <h4>請選擇一個遊戲</h4>
    </div>

    <!-- Chrome小恐龍控制 -->
    <div id="dinoControls" style="margin: 20px 0; padding: 15px; border: 2px solid #4CAF50; border-radius: 8px; background-color: #f9fff9;">
      <h4>🦕 小恐龍控制 (在EPD上遊玩)</h4>
      <div style="text-align: center;">
        <button onclick="dinoJump()" style="background-color: #4CAF50; color: white; padding: 12px 20px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px;">
          ⬆️ 跳躍
        </button><br>
        <button onmousedown="dinoCrouch()" onmouseup="dinoStandUp()" ontouchstart="dinoCrouch()" ontouchend="dinoStandUp()" style="background-color: #FF5722; color: white; padding: 12px 20px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px;">
          ⬇️ 蹲下 (按住)
        </button>
      </div>
      <p style="font-size: 14px; color: #666; text-align: center;">遊戲在EPD屏幕上顯示，用這些按鈕控制小恐龍避開障礙物！</p>
    </div>

    <!-- 推箱子控制 -->
    <div id="sokobanControls" style="margin: 20px 0; padding: 15px; border: 2px solid #FF9800; border-radius: 8px; background-color: #fff9f0;">
      <h4>📦 推箱子控制 (在EPD上遊玩)</h4>
      <div style="text-align: center;">
        <button onclick="moveSokoban('up')" style="background-color: #FF9800; color: white; padding: 12px 20px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px;">
          ⬆️ 上
        </button><br>
        <button onclick="moveSokoban('left')" style="background-color: #FF9800; color: white; padding: 12px 20px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px;">
          ⬅️ 左
        </button>
        <button onclick="moveSokoban('down')" style="background-color: #FF9800; color: white; padding: 12px 20px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px;">
          ⬇️ 下
        </button>
        <button onclick="moveSokoban('right')" style="background-color: #FF9800; color: white; padding: 12px 20px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px;">
          ➡️ 右
        </button>
      </div>
      <p style="font-size: 14px; color: #666; text-align: center;">把所有箱子推到目標位置就過關！</p>
    </div>

    <!-- 彈球遊戲資訊 -->
    <div id="ballGameInfo" style="margin: 20px 0; padding: 15px; border: 2px solid #2196F3; border-radius: 8px; background-color: #f0f8ff;">
      <h4>⚽ 彈球遊戲 (在EPD上遊玩)</h4>
      <p style="text-align: center; color: #666;">彈球會自動在EPD屏幕上的邊框內反彈，無需手動控制</p>
      <p style="text-align: center; font-size: 14px; color: #666;">享受視覺效果即可！</p>
    </div>
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
</body>
</html>
)rawliteral";

  html.replace("%WIDTH%", String(EPD_WIDTH));
  html.replace("%HEIGHT%", String(EPD_HEIGHT));
  html.replace("%BYTES%", String(FB_SIZE));

  server.send(200, "text/html", html);
}

void handleClear()
{
  Serial.println("handleClear");
  if (framebuffer)
  {
    memset(framebuffer, 0xFF, FB_SIZE);
    epd_poweron();
    epd_clear();
    epd_poweroff();
  }
  server.send(200, "text/plain", "OK");
}

void handleDrawLine()
{
  Serial.println("handleDrawLine");
  if (framebuffer)
  {
    epd_poweron();
    epd_draw_hline(10, random(10, EPD_HEIGHT), EPD_WIDTH - 20, 0, framebuffer);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
  }
  server.send(200, "text/plain", "Line drawn");
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
  Serial.printf("Drawing path: color=%d, size=%d, points='%s'\n", color, brushSize, pointsStr.substring(0, 50).c_str());

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

  Serial.printf("Drew path with %d points\n", pointCount);
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
  Serial.println("handleCanvasData");
  Serial.println("Canvas data handler called");

  // 顯示所有接收到的參數
  Serial.printf("Number of arguments: %d\n", server.args());
  for (int i = 0; i < server.args(); i++)
  {
    Serial.printf("Arg %d: %s = %s\n", i, server.argName(i).c_str(), server.arg(i).c_str());
  }

  if (!framebuffer)
  {
    Serial.println("ERROR: Framebuffer not available");
    server.send(400, "text/plain", "Framebuffer not available");
    return;
  }

  // 獲取 canvas 數據
  int canvasWidth = server.arg("width").toInt();
  int canvasHeight = server.arg("height").toInt();
  String dataStr = server.arg("data");
  bool isCompressed = server.hasArg("compressed") && server.arg("compressed").equals("1");
  bool isPathData = server.hasArg("paths") && server.arg("paths").equals("1");

  Serial.printf("Debug: isPathData = %s\n", isPathData ? "TRUE" : "FALSE");
  Serial.printf("Debug: server.hasArg('paths') = %s\n", server.hasArg("paths") ? "TRUE" : "FALSE");
  if (server.hasArg("paths"))
  {
    Serial.printf("Debug: server.arg('paths') = '%s'\n", server.arg("paths").c_str());
  }

  Serial.printf("Parsed parameters: width=%d, height=%d, data_length=%d, compressed=%s, paths=%s\n",
                canvasWidth, canvasHeight, dataStr.length(), isCompressed ? "yes" : "no", isPathData ? "yes" : "no");

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
  Serial.println("handleDrawText");
  if (!framebuffer)
  {
    server.send(400, "text/plain", "Framebuffer not available");
    return;
  }

  // 獲取參數
  String text = server.arg("text");
  int x = server.arg("x").toInt();
  int y = server.arg("y").toInt();
  int textColor = server.arg("textColor").toInt();
  int bgColor = server.arg("bgColor").toInt();
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

void notFound()
{
  Serial.println("notFound");
  server.send(404, "text/plain", "Not found");
}

// ===== 遊戲處理函數 =====

// 切換遊戲模式
void handleGameSwitch()
{
  String gameType = server.arg("type");
  Serial.printf("Switching to game: %s\n", gameType.c_str());

  if (gameType == "dino")
  {
    currentGame = GAME_DINO;
    // 重置恐龍遊戲
    dinoGame.x = 100;
    dinoGame.y = dinoGame.groundY;
    dinoGame.isJumping = false;
    dinoGame.isCrouching = false;
    dinoGame.jumpHeight = 0;
    dinoGame.score = 0;
    for (int i = 0; i < 3; i++)
    {
      dinoGame.obstacles[i].active = false;
    }
    drawDinoGame(true); // 遊戲切換時強制清除
  }
  else if (gameType == "ball")
  {
    currentGame = GAME_BALL;
    // 重置彈球遊戲
    ballGame.x = EPD_WIDTH / 2;
    ballGame.y = EPD_HEIGHT / 2;
    ballGame.vx = 2.0;
    ballGame.vy = 1.5;
    drawBallGame(true); // 遊戲切換時強制清除
  }
  else if (gameType == "sokoban")
  {
    currentGame = GAME_SOKOBAN;
    // 重置推箱子遊戲
    sokobanGame.playerX = 4;
    sokobanGame.playerY = 3;
    sokobanGame.moves = 0;
    sokobanGame.boxes[0].x = 3;
    sokobanGame.boxes[0].y = 3;
    sokobanGame.boxes[0].onTarget = false;
    sokobanGame.boxes[1].x = 5;
    sokobanGame.boxes[1].y = 3;
    sokobanGame.boxes[1].onTarget = false;
    drawSokobanGame(true); // 遊戲切換時強制清除
  }
  else
  {
    currentGame = GAME_NONE;
    if (framebuffer)
    {
      memset(framebuffer, 0xFF, FB_SIZE);
      epd_poweron();
      epd_clear();
      draw_ip_simple(10, 10, "Game Stopped", 0, framebuffer);
      epd_draw_grayscale_image(epd_full_screen(), framebuffer);
      epd_poweroff();
    }
  }

  server.send(200, "text/plain", "Game switched to " + gameType);
}

// Chrome小恐龍遊戲控制
void handleDinoJump()
{
  if (currentGame != GAME_DINO)
  {
    server.send(400, "text/plain", "Not in dino game");
    return;
  }

  if (!dinoGame.isJumping && !dinoGame.isCrouching)
  {
    dinoGame.isJumping = true;
    dinoGame.jumpHeight = 60; // 跳躍高度
    Serial.println("Dino jumping!");
    drawDinoGame(true); // 手動操作時強制清除
  }

  server.send(200, "text/plain", "Dino jumped");
}

void handleDinoCrouch()
{
  if (currentGame != GAME_DINO)
  {
    server.send(400, "text/plain", "Not in dino game");
    return;
  }

  dinoGame.isCrouching = true;
  Serial.println("Dino crouching!");
  drawDinoGame(true); // 手動操作時強制清除

  server.send(200, "text/plain", "Dino crouched");
}

void handleDinoStandUp()
{
  if (currentGame != GAME_DINO)
  {
    server.send(400, "text/plain", "Not in dino game");
    return;
  }

  dinoGame.isCrouching = false;
  Serial.println("Dino standing up!");
  drawDinoGame(true); // 手動操作時強制清除

  server.send(200, "text/plain", "Dino stood up");
}

// 推箱子遊戲控制
void handleSokobanMove()
{
  if (currentGame != GAME_SOKOBAN)
  {
    server.send(400, "text/plain", "Not in sokoban game");
    return;
  }

  String direction = server.arg("dir");
  int newX = sokobanGame.playerX;
  int newY = sokobanGame.playerY;

  if (direction == "up")
    newY--;
  else if (direction == "down")
    newY++;
  else if (direction == "left")
    newX--;
  else if (direction == "right")
    newX++;

  // 檢查邊界和牆壁
  if (newX >= 0 && newX < 9 && newY >= 0 && newY < 8 &&
      sokobanGame.level[newY][newX] != '#')
  {

    // 檢查是否推箱子
    bool boxMoved = false;
    for (int i = 0; i < 2; i++)
    {
      if (sokobanGame.boxes[i].x == newX && sokobanGame.boxes[i].y == newY)
      {
        int boxNewX = newX + (newX - sokobanGame.playerX);
        int boxNewY = newY + (newY - sokobanGame.playerY);

        // 檢查箱子能否移動
        if (boxNewX >= 0 && boxNewX < 9 && boxNewY >= 0 && boxNewY < 8 &&
            sokobanGame.level[boxNewY][boxNewX] != '#')
        {

          // 檢查目標位置是否有其他箱子
          bool blocked = false;
          for (int j = 0; j < 2; j++)
          {
            if (j != i && sokobanGame.boxes[j].x == boxNewX && sokobanGame.boxes[j].y == boxNewY)
            {
              blocked = true;
              break;
            }
          }

          if (!blocked)
          {
            sokobanGame.boxes[i].x = boxNewX;
            sokobanGame.boxes[i].y = boxNewY;
            boxMoved = true;
          }
        }

        if (!boxMoved)
        {
          server.send(200, "text/plain", "Cannot push box");
          return;
        }
        break;
      }
    }

    sokobanGame.playerX = newX;
    sokobanGame.playerY = newY;
    sokobanGame.moves++;

    Serial.printf("Sokoban moved %s to (%d,%d)\n", direction.c_str(), newX, newY);
    drawSokobanGame(true); // 手動操作時強制清除
  }

  server.send(200, "text/plain", "Player moved " + direction);
}

// 遊戲狀態查詢
void handleGameState()
{
  String json = "{";

  if (currentGame == GAME_DINO)
  {
    json += "\"game\":\"dino\",\"state\":{";
    json += "\"x\":" + String(dinoGame.x) + ",";
    json += "\"y\":" + String(dinoGame.y) + ",";
    json += "\"isJumping\":" + String(dinoGame.isJumping ? "true" : "false") + ",";
    json += "\"isCrouching\":" + String(dinoGame.isCrouching ? "true" : "false") + ",";
    json += "\"score\":" + String(dinoGame.score);
    json += "}";
  }
  else if (currentGame == GAME_BALL)
  {
    json += "\"game\":\"ball\",\"state\":{";
    json += "\"x\":" + String(ballGame.x) + ",";
    json += "\"y\":" + String(ballGame.y);
    json += "}";
  }
  else if (currentGame == GAME_SOKOBAN)
  {
    json += "\"game\":\"sokoban\",\"state\":{";
    json += "\"playerX\":" + String(sokobanGame.playerX) + ",";
    json += "\"playerY\":" + String(sokobanGame.playerY) + ",";
    json += "\"moves\":" + String(sokobanGame.moves);
    json += "}";
  }
  else
  {
    json += "\"game\":\"none\",\"state\":{}";
  }

  json += "}";

  server.send(200, "application/json", json);
}

// ===== 遊戲繪製函數 =====

void drawDinoGame(bool forceClear)
{
  if (!framebuffer)
    return;

  epd_poweron();

  // 清空framebuffer
  memset(framebuffer, 0xFF, FB_SIZE);

  // 只在需要時清除EPD（手動操作或遊戲重置時）
  if (forceClear)
  {
    epd_clear();
  }

  // 繪製地面
  epd_fill_rect(0, dinoGame.groundY + 20, EPD_WIDTH, 5, 0, framebuffer);

  // 計算恐龍位置
  int dinoY = dinoGame.y;
  if (dinoGame.isJumping)
  {
    dinoY = dinoGame.groundY - dinoGame.jumpHeight;
  }

  // 繪製恐龍
  int dinoHeight = dinoGame.isCrouching ? 20 : 40;
  int dinoWidth = 30;
  epd_fill_rect(dinoGame.x, dinoY, dinoWidth, dinoHeight, 0, framebuffer);

  // 繪製障礙物
  for (int i = 0; i < 3; i++)
  {
    if (dinoGame.obstacles[i].active)
    {
      epd_fill_rect(dinoGame.obstacles[i].x, dinoGame.obstacles[i].y,
                    dinoGame.obstacles[i].width, dinoGame.obstacles[i].height,
                    8, framebuffer); // 灰色障礙物
    }
  }

  // 顯示分數
  String scoreText = "Score: " + String(dinoGame.score);
  draw_ip_simple(10, 10, scoreText.c_str(), 0, framebuffer);

  // 顯示遊戲狀態
  if (dinoGame.isJumping)
  {
    draw_ip_simple(10, 30, "JUMPING!", 0, framebuffer);
  }
  else if (dinoGame.isCrouching)
  {
    draw_ip_simple(10, 30, "CROUCHING", 0, framebuffer);
  }

  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();
}

void drawBallGame(bool forceClear)
{
  if (!framebuffer)
    return;

  epd_poweron();

  // 清空framebuffer
  memset(framebuffer, 0xFF, FB_SIZE);

  // 只在需要時清除EPD
  if (forceClear)
  {
    epd_clear();
  }

  // 繪製邊框
  epd_draw_rect(5, 5, EPD_WIDTH - 10, EPD_HEIGHT - 10, 0, framebuffer);

  // 繪製球
  epd_fill_circle((int)ballGame.x, (int)ballGame.y, ballGame.radius, 0, framebuffer);

  // 顯示位置資訊
  String posText = "Ball: (" + String((int)ballGame.x) + "," + String((int)ballGame.y) + ")";
  draw_ip_simple(10, 10, posText.c_str(), 0, framebuffer);
  draw_ip_simple(10, 25, "Ball Game Running", 0, framebuffer);

  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();
}

void drawSokobanGame(bool forceClear)
{
  if (!framebuffer)
    return;

  epd_poweron();

  // 清空framebuffer
  memset(framebuffer, 0xFF, FB_SIZE);

  // 只在需要時清除EPD
  if (forceClear)
  {
    epd_clear();
  }

  int cellSize = 40;
  int offsetX = 100;
  int offsetY = 50;

  // 繪製地圖
  for (int y = 0; y < 8; y++)
  {
    for (int x = 0; x < 9; x++)
    {
      int pixelX = offsetX + x * cellSize;
      int pixelY = offsetY + y * cellSize;

      if (sokobanGame.level[y][x] == '#')
      {
        epd_fill_rect(pixelX, pixelY, cellSize, cellSize, 0, framebuffer); // 牆壁
      }
    }
  }

  // 繪製目標點
  for (int i = 0; i < 2; i++)
  {
    int pixelX = offsetX + sokobanGame.targets[i].x * cellSize + 5;
    int pixelY = offsetY + sokobanGame.targets[i].y * cellSize + 5;
    epd_fill_rect(pixelX, pixelY, cellSize - 10, cellSize - 10, 8, framebuffer); // 灰色目標
  }

  // 繪製箱子
  for (int i = 0; i < 2; i++)
  {
    int pixelX = offsetX + sokobanGame.boxes[i].x * cellSize + 3;
    int pixelY = offsetY + sokobanGame.boxes[i].y * cellSize + 3;
    epd_fill_rect(pixelX, pixelY, cellSize - 6, cellSize - 6, 4, framebuffer); // 深灰色箱子
  }

  // 繪製玩家
  int playerPixelX = offsetX + sokobanGame.playerX * cellSize + 8;
  int playerPixelY = offsetY + sokobanGame.playerY * cellSize + 8;
  epd_fill_rect(playerPixelX, playerPixelY, cellSize - 16, cellSize - 16, 0, framebuffer); // 黑色玩家

  // 顯示移動次數
  String movesText = "Moves: " + String(sokobanGame.moves);
  draw_ip_simple(10, 10, movesText.c_str(), 0, framebuffer);
  draw_ip_simple(10, 25, "Sokoban Game", 0, framebuffer);

  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();
}

// ===== 遊戲更新邏輯 =====

void updateDinoGame()
{
  unsigned long now = millis();
  if (now - dinoGame.lastUpdate < 100)
    return; // 更新頻率限制
  dinoGame.lastUpdate = now;

  // 更新跳躍
  if (dinoGame.isJumping)
  {
    dinoGame.jumpHeight -= 3;
    if (dinoGame.jumpHeight <= 0)
    {
      dinoGame.jumpHeight = 0;
      dinoGame.isJumping = false;
    }
  }

  // 生成障礙物
  if (now - dinoGame.lastObstacle > 3000)
  { // 每3秒生成一個障礙物
    for (int i = 0; i < 3; i++)
    {
      if (!dinoGame.obstacles[i].active)
      {
        dinoGame.obstacles[i].x = EPD_WIDTH;
        dinoGame.obstacles[i].y = dinoGame.groundY - 30;
        dinoGame.obstacles[i].active = true;
        dinoGame.lastObstacle = now;
        break;
      }
    }
  }

  // 更新障礙物位置
  for (int i = 0; i < 3; i++)
  {
    if (dinoGame.obstacles[i].active)
    {
      dinoGame.obstacles[i].x -= 5;
      if (dinoGame.obstacles[i].x < -dinoGame.obstacles[i].width)
      {
        dinoGame.obstacles[i].active = false;
        dinoGame.score += 10;
      }
    }
  }

  // 碰撞檢測
  int dinoY = dinoGame.isJumping ? dinoGame.groundY - dinoGame.jumpHeight : dinoGame.y;
  int dinoHeight = dinoGame.isCrouching ? 20 : 40;

  for (int i = 0; i < 3; i++)
  {
    if (dinoGame.obstacles[i].active)
    {
      if (dinoGame.x < dinoGame.obstacles[i].x + dinoGame.obstacles[i].width &&
          dinoGame.x + 30 > dinoGame.obstacles[i].x &&
          dinoY < dinoGame.obstacles[i].y + dinoGame.obstacles[i].height &&
          dinoY + dinoHeight > dinoGame.obstacles[i].y)
      {
        // 遊戲結束
        Serial.println("Game Over! Score: " + String(dinoGame.score));
        // 重置遊戲
        dinoGame.score = 0;
        for (int j = 0; j < 3; j++)
        {
          dinoGame.obstacles[j].active = false;
        }
      }
    }
  }
}

void updateBallGame()
{
  unsigned long now = millis();
  if (now - ballGame.lastUpdate < 50)
    return; // 更新頻率限制
  ballGame.lastUpdate = now;

  // 更新球的位置
  ballGame.x += ballGame.vx;
  ballGame.y += ballGame.vy;

  // 邊界碰撞檢測
  if (ballGame.x - ballGame.radius <= 5 || ballGame.x + ballGame.radius >= EPD_WIDTH - 5)
  {
    ballGame.vx = -ballGame.vx;
    ballGame.x = constrain(ballGame.x, 5 + ballGame.radius, EPD_WIDTH - 5 - ballGame.radius);
  }
  if (ballGame.y - ballGame.radius <= 5 || ballGame.y + ballGame.radius >= EPD_HEIGHT - 5)
  {
    ballGame.vy = -ballGame.vy;
    ballGame.y = constrain(ballGame.y, 5 + ballGame.radius, EPD_HEIGHT - 5 - ballGame.radius);
  }
}

void updateCurrentGame()
{
  static unsigned long lastGameUpdate = 0;
  unsigned long now = millis();

  // 限制遊戲更新頻率（每1000ms更新一次畫面，降低EPD負荷）
  if (now - lastGameUpdate < 1000)
    return;
  lastGameUpdate = now;

  if (currentGame == GAME_DINO)
  {
    updateDinoGame();
    drawDinoGame(false); // 自動更新不強制清除
  }
  else if (currentGame == GAME_BALL)
  {
    updateBallGame();
    drawBallGame(false); // 自動更新不強制清除
  }
}

// ===== Setup =====
void setup()
{
  Serial.begin(115200);
  delay(2000); // 增加延遲確保序列埠穩定

  Serial.println("Starting EPD Controller...");

  // 初始化遊戲狀態
  sokobanGame.boxes[0].x = 3;
  sokobanGame.boxes[0].y = 3;
  sokobanGame.boxes[0].onTarget = false;
  sokobanGame.boxes[1].x = 5;
  sokobanGame.boxes[1].y = 3;
  sokobanGame.boxes[1].onTarget = false;
  sokobanGame.targets[0].x = 2;
  sokobanGame.targets[0].y = 2;
  sokobanGame.targets[1].x = 6;
  sokobanGame.targets[1].y = 2;

  // 初始化 framebuffer
  Serial.println("Initializing framebuffer...");
  framebuffer = (uint8_t *)ps_calloc(1, FB_SIZE);
  if (!framebuffer)
  {
    Serial.println("PSRAM alloc failed!");
    while (1)
      delay(100);
  }
  memset(framebuffer, 0xFF, FB_SIZE);
  Serial.println("Framebuffer initialized");

  // 初始化 EPD
  Serial.println("Initializing EPD...");
  epd_init();
  Serial.println("EPD init done");

  epd_poweron();
  Serial.println("EPD powered on");

  epd_clear();
  Serial.println("EPD cleared");

  epd_poweroff();
  Serial.println("EPD powered off");

  // 啟動 Wi-Fi AP
  Serial.println("Starting WiFi AP...");
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  String ipStr = "http://" + IP.toString() + ":80";

  Serial.println("AP started");
  Serial.print("IP address: ");
  Serial.println(ipStr);

  // 在 EPD 左上角顯示 IP 地址
  Serial.println("Drawing IP on EPD...");
  epd_poweron();

  // 方法1: 使用簡化的 IP 顯示（推薦）
  String simpleIP = IP.toString() + ":80";
  draw_ip_simple(10, 10, simpleIP.c_str(), 0, framebuffer);

  // 在下方顯示 SSID
  draw_ip_simple(10, 25, ssid, 0, framebuffer);

  // 更新 EPD 顯示
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();
  Serial.println("IP displayed on EPD");

  // 設定 Web Server 路由
  Serial.println("Setting up web server routes...");
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

  // 遊戲控制路由
  server.on("/game/switch", HTTP_GET, handleGameSwitch);
  server.on("/game/dino/jump", HTTP_GET, handleDinoJump);
  server.on("/game/dino/crouch", HTTP_GET, handleDinoCrouch);
  server.on("/game/dino/standup", HTTP_GET, handleDinoStandUp);
  server.on("/game/sokoban/move", HTTP_GET, handleSokobanMove);
  server.on("/game/state", HTTP_GET, handleGameState);

  server.onNotFound(notFound);

  // 設定 WebServer 的緩衝區大小以處理大型 POST 數據
  const char *headerKeys[] = {"Content-Length"};
  server.collectHeaders(headerKeys, 1);

  // 增加 WebServer 的緩衝區大小限制
  // 預設可能只有 1KB，我們需要更大的緩衝區來處理路徑數據
  Serial.println("Configuring WebServer for large POST data...");

  server.begin();
  Serial.println("Web server started");
  Serial.println("Setup complete!");
  Serial.print("Connect to WiFi: ");
  Serial.println(ssid);
  Serial.print("Open browser: ");
  Serial.println(ipStr);
}

// ===== Loop =====
void loop()
{
  server.handleClient();

  // 更新當前遊戲
  updateCurrentGame();

  // 每10秒輸出一次心跳信號，確認程式在運行
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 10000)
  {
    Serial.println("System running... Waiting for connections");
    if (currentGame != GAME_NONE)
    {
      Serial.printf("Current game: %d\n", currentGame);
    }
    lastHeartbeat = millis();
  }

  delay(1); // 讓出 CPU
}