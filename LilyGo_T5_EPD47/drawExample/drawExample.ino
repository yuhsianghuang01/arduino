/**
 * @copyright Copyright (c) 2024  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2024-04-05
 * @note      Arduino Setting
 *            Tools ->
 *                  Board:"ESP32S3 Dev Module"
 *                  USB CDC On Boot:"Enable"
 *                  USB DFU On Boot:"Disable"
 *                  Flash Size : "16MB(128Mb)"
 *                  Flash Mode"QIO 80MHz
 *                  Partition Scheme:"16M Flash(3M APP/9.9MB FATFS)"
 *                  PSRAM:"OPI PSRAM"
 *                  Upload Mode:"UART0/Hardware CDC"
 *                  USB Mode:"Hardware CDC and JTAG"
 *
 * ==========================================
 * LilyGo T5 EPD47 電子紙繪圖範例程式
 * ==========================================
 *
 * 程式功能：
 * 這是一個基本的電子紙繪圖示範程式，展示如何在 4.7 吋電子紙螢幕上
 * 進行基本的圖形繪製操作。程式會持續繪製隨機位置的水平線條，
 * 演示即時繪圖和畫面更新的功能。
 *
 * 主要特色：
 * - 基本的電子紙驅動程式使用
 * - 影像緩衝區管理
 * - 簡單的線條繪製
 * - 隨機位置生成
 * - 持續更新顯示
 *
 * 硬體需求：
 * - LilyGo T5 EPD47 開發板
 * - ESP32-S3 處理器
 * - 16MB PSRAM（必須啟用）
 * - 4.7 吋電子紙顯示器
 *
 * 注意事項：
 * - 需要在 Arduino IDE 中啟用 PSRAM
 * - 適合初學者學習電子紙基本操作
 * - 程式結構簡單，易於理解和修改
 */

#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> tools -> PSRAM -> OPI !!!"
// 錯誤訊息：請啟用 PSRAM，Arduino IDE -> 工具 -> PSRAM -> OPI !!!
#endif

#include <Arduino.h>    // Arduino 核心函式庫
#include "epd_driver.h" // 電子紙驅動程式庫
#include "utilities.h"  // 實用工具函式庫

uint8_t *framebuffer = NULL; // 影像緩衝區指標

/**
 * Arduino 初始化函數
 *
 * 功能：
 * 1. 初始化串列埠通訊
 * 2. 分配影像緩衝區記憶體
 * 3. 初始化電子紙驅動程式
 * 4. 清除螢幕準備繪圖
 */
void setup()
{
    Serial.begin(115200); // 初始化串列埠，鮑率 115200
    delay(1000);          // 等待系統穩定

    // 分配影像緩衝區記憶體（使用 PSRAM）
    framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    if (!framebuffer)
    {
        Serial.println("alloc memory failed !!!"); // 記憶體分配失敗
        while (1)
            ; // 停止執行
    }
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2); // 將緩衝區設為白色

    epd_init(); // 初始化電子紙驅動程式

    epd_poweron();  // 啟動電子紙電源
    epd_clear();    // 清除螢幕內容
    epd_poweroff(); // 關閉電子紙電源（節能）
}

/**
 * Arduino 主迴圈函數
 *
 * 功能：
 * 演示各種基本圖形繪製功能，包括：
 * 1. 水平線條繪製
 * 2. 矩形繪製（空心）
 * 3. 圓形繪製（空心）
 * 4. 矩形填滿
 * 5. 圓形填滿
 *
 * 每種圖形都會在隨機位置繪製，並更新到螢幕顯示
 * 完成一輪繪製後會清除螢幕，重新開始下一輪
 */
void loop()
{
    epd_poweron(); // 啟動電子紙電源

    // 繪製隨機位置的水平線條
    epd_draw_hline(10, random(10, EPD_HEIGHT), EPD_WIDTH - 20, 0, framebuffer);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer); // 更新顯示
    delay(1000);                                              // 等待 1 秒

    // 繪製隨機位置和大小的空心矩形
    epd_draw_rect(10, random(10, EPD_HEIGHT), random(10, 60), random(10, 120), 0, framebuffer);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer); // 更新顯示
    delay(1000);                                              // 等待 1 秒

    // 繪製隨機位置的空心圓形
    epd_draw_circle(random(10, EPD_WIDTH), random(10, EPD_HEIGHT), 120, 0, framebuffer);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer); // 更新顯示
    delay(1000);                                              // 等待 1 秒

    // 繪製隨機位置和大小的實心矩形
    epd_fill_rect(10, random(10, EPD_HEIGHT), random(10, 60), random(10, 120), 0, framebuffer);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer); // 更新顯示
    delay(1000);                                              // 等待 1 秒

    // 繪製隨機位置和大小的實心圓形
    epd_fill_circle(random(10, EPD_WIDTH), random(10, EPD_HEIGHT), random(10, 160), 0, framebuffer);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer); // 更新顯示
    delay(1000);                                              // 等待 1 秒

    // 清除緩衝區和螢幕，準備下一輪繪製
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2); // 將緩衝區設為白色
    epd_clear();                                           // 清除螢幕
    epd_poweroff();                                        // 關閉電子紙電源

} // loop() 函數結束