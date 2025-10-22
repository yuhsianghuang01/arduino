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

// 嘗試包含字體檔案（如果可用）
// 常見的 EPD 字體檔案
#ifdef EPD_FONT_OPENSANS18B_H
#include "opensans18b.h"
#endif
#ifdef EPD_FONT_OPENSANS12B_H
#include "opensans12b.h"
#endif

// 如果沒有字體檔案，我們將使用圖形方式顯示數字和字母

// ===== WiFi AP Config =====
const char *ssid = "EPD-Controller";
const char *password = "12345678"; // 至少8碼

// ===== Web Server =====
WebServer server(80);

// ===== Framebuffer =====
uint8_t *framebuffer = NULL;
const int FB_SIZE = EPD_WIDTH * EPD_HEIGHT / 2; // 2-bit grayscale

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
    int base_char_width = 6;   // 基礎字符寬度
    int base_char_height = 8;  // 基礎字符高度
    
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
  </style>
</head>
<body>
  <h2>EPD Web Controller</h2>
  <button onclick="fetch('/clear')">Clear Screen</button>
  <button onclick="fetch('/draw/line')">Draw Random Line</button>
  <button onclick="fetch('/draw/rect')">Draw Random Rect</button>
  <button onclick="fetch('/draw/circle')">Draw Random Circle</button>

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

  <script>
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
  </script>
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
        int px = x + i - thickness/2;
        int py = y + j - thickness/2;
        if (px >= 0 && px < EPD_WIDTH && py >= 0 && py < EPD_HEIGHT)
        {
          epd_fill_rect(px, py, 1, 1, color, framebuffer);
        }

        if (x == x2 && y == y2) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
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
            {centerX + x, centerY + y}, {centerX - x, centerY + y},
            {centerX + x, centerY - y}, {centerX - x, centerY - y},
            {centerX + y, centerY + x}, {centerX - y, centerY + x},
            {centerX + y, centerY - x}, {centerX - y, centerY - x}
          };

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

void handleDrawText()
{
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

void notFound()
{
  server.send(404, "text/plain", "Not found");
}

// ===== Setup =====
void setup()
{
  Serial.begin(115200);
  delay(1000);

  // 初始化 framebuffer
  framebuffer = (uint8_t *)ps_calloc(1, FB_SIZE);
  if (!framebuffer)
  {
    Serial.println("PSRAM alloc failed!");
    while (1)
      delay(100);
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

  // 在 EPD 左上角顯示 IP 地址
  epd_poweron();

  // 方法1: 使用簡化的 IP 顯示（推薦）
  String simpleIP = IP.toString() + ":80";
  draw_ip_simple(10, 10, simpleIP.c_str(), 0, framebuffer);

  // 在下方顯示 SSID
  draw_ip_simple(10, 25, ssid, 0, framebuffer);

  // 方法2: 如果上面的方法不行，使用大號矩形顯示
  // 用矩形繪製 IP 的每個數字（備用方案）
  /*
  String ipOnly = IP.toString();
  epd_draw_rect(10, 40, ipOnly.length() * 12, 16, 0, framebuffer);
  for (int i = 0; i < ipOnly.length(); i++) {
      if (ipOnly[i] == '.') {
          epd_fill_rect(22 + i * 12, 50, 2, 2, 0, framebuffer);
      } else if (ipOnly[i] >= '0' && ipOnly[i] <= '9') {
          // 用矩形表示數字
          int digit = ipOnly[i] - '0';
          for (int j = 0; j < digit % 4; j++) {
              epd_draw_hline(12 + i * 12, 42 + j * 3, 8, 0, framebuffer);
          }
      }
  }
  */

  // 更新 EPD 顯示
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  // 設定 Web Server 路由
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
  server.on("/upload", HTTP_POST, []()
            { server.send(200); }, handleUpload);

  server.onNotFound(notFound);
  server.begin();
}

// ===== Loop =====
void loop()
{
  server.handleClient();
  delay(1); // 讓出 CPU
}