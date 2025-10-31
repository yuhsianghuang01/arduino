/**
 * @copyright Copyright (c) 2024  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 *            版權所有 (c) 2024 深圳鑫源電子技術有限公司
 * @date      2024-04-05
 *            開發日期：2024年4月5日
 * @note      Arduino Setting - Arduino IDE 設定說明
 *            Tools -> 工具選單設定：
 *                  Board:"ESP32S3 Dev Module" - 開發板：ESP32S3 開發模組
 *                  USB CDC On Boot:"Enable" - 開機時啟用 USB CDC：啟用
 *                  USB DFU On Boot:"Disable" - 開機時啟用 USB DFU：停用
 *                  Flash Size : "16MB(128Mb)" - Flash 大小：16MB(128Mb)
 *                  Flash Mode"QIO 80MHz - Flash 模式：QIO 80MHz（四線輸入輸出，80MHz頻率）
 *                  Partition Scheme:"16M Flash(3M APP/9.9MB FATFS)" - 分割區方案：16M Flash（3M 應用程式/9.9MB 檔案系統）
 *                  PSRAM:"OPI PSRAM" - PSRAM：八線輸入輸出 PSRAM
 *                  Upload Mode:"UART0/Hardware CDC" - 上傳模式：UART0/硬體 CDC
 *                  USB Mode:"Hardware CDC and JTAG" - USB 模式：硬體 CDC 和 JTAG
 *
 */

#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> tools -> PSRAM -> OPI !!!"
// 檢查 PSRAM 是否啟用，如未啟用則顯示錯誤訊息
// 錯誤提示：請啟用 PSRAM，路徑：Arduino IDE -> 工具 -> PSRAM -> OPI !!!
#endif

/* Simple firmware for a ESP32 displaying a static image on an EPaper Screen.
 *
 * Write an image into a header file using a 3...2...1...0 format per pixel,
 * for 4 bits color (16 colors - well, greys.) MSB first.  At 80 MHz, screen
 * clears execute in 1.075 seconds and images are drawn in 1.531 seconds.
 */
/*
 * ESP32 電子紙灰階測試程式
 *
 * 程式功能：
 * - 測試電子紙的 16 級灰階顯示效果
 * - 比較 WHITE_ON_BLACK 和 BLACK_ON_WHITE 兩種顯示模式
 * - 生成漸層灰階條紋圖案進行視覺測試
 * - 測量不同顯示模式的執行時間
 *
 * 圖像資料格式說明：
 * - 每個像素使用 4 位元表示（16 種灰階：0-15）
 * - MSB（最高有效位元）優先排列
 * - 兩個像素共用一個位元組存儲
 * - 在 80 MHz 頻率下，螢幕清除需要 1.075 秒，圖像繪製需要 1.531 秒
 */

#include <Arduino.h>    // Arduino 核心函式庫
#include "epd_driver.h" // 電子紙驅動程式標頭檔
#include "utilities.h"  // 公用函式和工具程式

uint8_t *grayscale_img;  // 第一個灰階圖像緩衝區指標（正向漸層）
uint8_t *grayscale_img2; // 第二個灰階圖像緩衝區指標（反向漸層）

void setup()
{
    // 初始化串列通訊，波特率設為 115200
    Serial.begin(115200);

    // 初始化電子紙驅動程式，設定相關的 GPIO 腳位和通訊協定
    epd_init();

    // copy the image data to SRAM for faster display time
    // 在 PSRAM 中分配第一個影像緩衝區記憶體以提高顯示速度
    grayscale_img = (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM);
    if (grayscale_img == NULL)
    {
        Serial.println("Could not allocate framebuffer in PSRAM!");
        // 無法在 PSRAM 中分配影像緩衝區記憶體！
    }

    // 在 PSRAM 中分配第二個影像緩衝區記憶體
    grayscale_img2 = (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM);
    if (grayscale_img2 == NULL)
    {
        Serial.println("Could not allocate framebuffer in PSRAM!");
        // 無法在 PSRAM 中分配影像緩衝區記憶體！
    }

    // 創建灰階條紋圖案的臨時緩衝區
    uint8_t grayscale_line[EPD_WIDTH / 2];
    uint8_t value = 0; // 暫存變數（此處未使用）

    // ========== 建立第一個圖像：從左到右的正向漸層 ==========
    for (uint32_t i = 0; i < EPD_WIDTH / 2; i++)
    {
        // 計算當前像素屬於哪個灰階區段（16個區段，每個區段代表一個灰階等級）
        uint8_t segment = i / (EPD_WIDTH / 16 / 2);
        // 將同一個灰階值分別填入高 4 位元和低 4 位元（一個位元組包含兩個像素）
        grayscale_line[i] = (segment << 4) | segment;
    }
    // 將這一行的灰階資料複製到整個圖像的每一行，形成垂直條紋
    for (uint32_t y = 0; y < EPD_HEIGHT; y++)
    {
        memcpy(grayscale_img + EPD_WIDTH / 2 * y, grayscale_line, EPD_WIDTH / 2);
    }

    // ========== 建立第二個圖像：從右到左的反向漸層 ==========
    value = 0; // 重置暫存變數
    for (uint32_t i = 0; i < EPD_WIDTH / 2; i++)
    {
        // 計算反向漸層：從最右邊開始計算區段
        uint8_t segment = (EPD_WIDTH / 2 - i - 1) / (EPD_WIDTH / 16 / 2);
        // 將反向灰階值填入像素資料
        grayscale_line[i] = (segment << 4) | segment;
    }
    // 將反向漸層資料複製到第二個圖像緩衝區
    for (uint32_t y = 0; y < EPD_HEIGHT; y++)
    {
        memcpy(grayscale_img2 + EPD_WIDTH / 2 * y, grayscale_line, EPD_WIDTH / 2);
    }
}

/**
 * 主迴圈函式：循環測試兩種灰階顯示模式
 *
 * 測試流程：
 * 1. WHITE_ON_BLACK 模式：白色背景上的黑色圖像
 * 2. BLACK_ON_WHITE 模式：黑色背景上的白色圖像
 *
 * 每種模式都會：
 * - 測量顯示時間
 * - 顯示 3 秒供觀察
 * - 輸出執行時間統計
 */
void loop()
{
    uint32_t t1, t2; // 時間測量變數

    // ========== WHITE_ON_BLACK 顯示模式測試 ==========
    Serial.println("WHITE_ON_BLACK drawing:");
    // 輸出：WHITE_ON_BLACK 繪製模式

    // 開啟電子紙電源
    epd_poweron();

    // 清除電子紙螢幕
    epd_clear();

    // 記錄開始時間（微秒）
    t1 = esp_timer_get_time();

    // 執行三次像素推送操作（預處理步驟）
    // 這些操作用於優化電子紙的顯示效果
    epd_push_pixels(epd_full_screen(), 20, 0);
    epd_push_pixels(epd_full_screen(), 20, 0);
    epd_push_pixels(epd_full_screen(), 20, 0);

    // 以 WHITE_ON_BLACK 模式顯示第二個圖像（反向漸層）
    epd_draw_image(epd_full_screen(), grayscale_img2, WHITE_ON_BLACK);

    // 記錄結束時間
    t2 = esp_timer_get_time();

    // 計算並輸出繪製耗時（毫秒）
    Serial.printf("draw took %dms.\n", (t2 - t1) / 1000);

    // 關閉電子紙電源
    epd_poweroff();

    // 延遲 3 秒讓使用者觀察顯示效果
    delay(3000);

    // ========== BLACK_ON_WHITE 顯示模式測試 ==========
    Serial.println("BLACK_ON_WHITE drawing:");
    // 輸出：BLACK_ON_WHITE 繪製模式

    // 開啟電子紙電源
    epd_poweron();

    // 清除電子紙螢幕
    epd_clear();

    // 記錄開始時間
    t1 = esp_timer_get_time();

    // 以 BLACK_ON_WHITE 模式顯示第一個圖像（正向漸層）
    epd_draw_image(epd_full_screen(), grayscale_img, BLACK_ON_WHITE);

    // 記錄結束時間
    t2 = esp_timer_get_time();

    // 計算並輸出繪製耗時
    Serial.printf("draw took %dms.\n", (t2 - t1) / 1000);

    // 關閉電子紙電源
    epd_poweroff();

    // 延遲 3 秒後重複測試
    delay(3000);
}
/*
grayscale_test.ino 程式註解總結
1. 程式頭部和設定說明
    版權資訊：添加中英文對照的版權說明
    Arduino IDE 設定：詳細說明每個設定項目的用途
    PSRAM 檢查：說明為什麼需要啟用 PSRAM
2. 程式功能概述
    主要功能：ESP32 電子紙 16 級灰階顯示測試
    測試項目：WHITE_ON_BLACK 和 BLACK_ON_WHITE 兩種顯示模式
    視覺效果：生成漸層灰階條紋圖案
    效能測試：測量不同顯示模式的執行時間
3. 標頭檔案和全域變數
    函式庫說明：每個 include 檔案的用途
    緩衝區變數：兩個灰階圖像緩衝區的用途
4. setup() 函式詳解
    初始化流程：串列通訊、電子紙驅動程式
    記憶體分配：兩個 PSRAM 緩衝區配置和錯誤處理
    圖像生成算法：
    正向漸層：從左到右的灰階條紋
    反向漸層：從右到左的灰階條紋
    像素編碼：4位元灰階資料的打包方式
5. 灰階圖像生成原理
    區段計算：16個灰階等級的區段劃分
    像素打包：兩個 4 位元像素共用一個位元組
    條紋生成：垂直條紋圖案的建立方式
6. loop() 主迴圈功能
    雙模式測試：
    WHITE_ON_BLACK：白背景黑圖像，包含預處理步驟
    BLACK_ON_WHITE：黑背景白圖像，直接顯示
    效能監控：使用高精度計時器測量執行時間
    視覺比較：每種模式顯示 3 秒供觀察
7. 技術重點說明
    記憶體優化：使用 PSRAM 處理大容量灰階圖像
    電力管理：適時開關電子紙電源
    顯示技術：兩種顯示模式的差異和特點
    預處理優化：WHITE_ON_BLACK 模式的像素推送優化
8. 灰階顯示原理
    16 級灰階：從 0（黑）到 15（白）的灰階範圍
    MSB 優先：高位元優先的資料排列
    雙像素存儲：每個位元組存儲兩個像素資料
    視覺測試：漸層條紋提供直觀的灰階效果驗證
    這個程式現在包含了完整的中文技術文件，涵蓋了電子紙灰階顯示的核心技術：

    硬體設定：PSRAM 配置和電子紙初始化
    圖像處理：灰階資料的生成和編碼
    顯示技術：兩種顯示模式的比較測試
    效能分析：執行時間的精確測量
*/