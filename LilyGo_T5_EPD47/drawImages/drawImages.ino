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
 * ESP32 電子紙顯示靜態圖像的簡單韌體程式
 *
 * 圖像資料格式說明：
 * - 將圖像寫入標頭檔案，每個像素使用 3...2...1...0 格式
 * - 4 位元色彩深度（16 種顏色 - 灰階）
 * - MSB（最高有效位元）優先
 * - 在 80 MHz 頻率下，螢幕清除需要 1.075 秒，圖像繪製需要 1.531 秒
 *
 * 程式功能：
 * - 循環顯示三張預設圖像（pic1, pic2, pic3）
 * - 使用 PSRAM 作為影像緩衝區以處理大尺寸圖像
 * - 測量並顯示每次繪製操作的執行時間
 * - 示範兩種圖像顯示方式：直接顯示和緩衝區顯示
 */

#include <Arduino.h>    // Arduino 核心函式庫
#include "epd_driver.h" // 電子紙驅動程式標頭檔
#include "src/pic1.h"   // 第一張圖像資料檔案
#include "src/pic2.h"   // 第二張圖像資料檔案
#include "src/pic3.h"   // 第三張圖像資料檔案
#include "utilities.h"  // 公用函式和工具程式

uint8_t *framebuffer; // 影像緩衝區指標，用於存放電子紙顯示資料

void setup()
{
    // 初始化串列通訊，波特率設為 115200
    Serial.begin(115200);

    // 初始化電子紙驅動程式，設定相關的 GPIO 腳位和通訊協定
    epd_init();

    // 在 PSRAM 中分配影像緩衝區記憶體
    // 計算公式：EPD_WIDTH * EPD_HEIGHT / 2（每個像素 4 位元，兩個像素共用一個位元組）
    // MALLOC_CAP_SPIRAM 指定使用外部 PSRAM 而非內部 SRAM
    framebuffer = (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM);

    // 檢查記憶體分配是否成功
    if (!framebuffer)
    {
        Serial.println("alloc memory failed !!!"); // 記憶體分配失敗提示
        // 記憶體分配失敗時進入無限迴圈，程式停止執行
        while (1)
            ;
    }

    // 將影像緩衝區初始化為白色（0xFF 代表白色）
    // 每個位元組包含兩個像素的資料
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
}

/**
 * 更新電子紙顯示並測量執行時間的函式
 * @param delay_ms 顯示完成後的延遲時間（毫秒）
 *
 * 功能說明：
 * 1. 開啟電子紙電源
 * 2. 清除螢幕內容
 * 3. 繪製影像緩衝區的內容到電子紙
 * 4. 測量並輸出繪製時間
 * 5. 關閉電子紙電源以節能
 * 6. 延遲指定時間
 */
void update(uint32_t delay_ms)
{
    // 開啟電子紙電源，準備進行顯示操作
    epd_poweron();

    // 清除電子紙上的所有內容，將螢幕設為白色
    epd_clear();

    // 記錄開始繪製的時間點
    volatile uint32_t t1 = millis();

    // 將影像緩衝區的灰階圖像繪製到整個電子紙螢幕
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);

    // 記錄繪製完成的時間點
    volatile uint32_t t2 = millis();

    // 計算並輸出繪製耗時（毫秒）
    Serial.printf("EPD draw took %dms.\r\n", t2 - t1);

    // 關閉電子紙電源，節省電力消耗
    epd_poweroff();

    // 延遲指定時間，讓使用者觀看顯示結果
    delay(delay_ms);
}

/**
 * 主迴圈函式：循環顯示三張圖像的示範程式
 *
 * 顯示流程：
 * 1. 第一張圖像（pic1）：直接顯示模式，無緩衝區
 * 2. 第二張圖像（pic2）：使用緩衝區模式
 * 3. 第三張圖像（pic3）：使用緩衝區模式
 *
 * 兩種顯示模式的差異：
 * - 直接顯示：立即將圖像資料傳送到電子紙，適合單一圖像
 * - 緩衝區模式：先將圖像複製到記憶體緩衝區，可進行圖像合成或後處理
 */
void loop()
{
    // ========== 第一張圖像：直接顯示模式 ==========
    // 定義第一張圖像的顯示區域
    Rect_t area = {
        .x = 0,               // 起始 X 座標（左上角）
        .y = 0,               // 起始 Y 座標（左上角）
        .width = pic1_width,  // 圖像寬度（像素）
        .height = pic1_height // 圖像高度（像素）
    };

    // 開啟電子紙電源
    epd_poweron();

    // 清除電子紙螢幕內容
    epd_clear();

    // 直接將第一張圖像的灰階資料顯示到電子紙上
    // 此方法不使用緩衝區，直接將圖像資料傳送到顯示器
    epd_draw_grayscale_image(area, (uint8_t *)pic1_data);

    // 關閉電子紙電源
    epd_poweroff();

    // 顯示 3 秒讓使用者觀看
    delay(3000);

    // ========== 第二張圖像：緩衝區顯示模式 ==========
    // 定義第二張圖像的顯示區域
    Rect_t area1 = {
        .x = 0,               // 起始 X 座標
        .y = 0,               // 起始 Y 座標
        .width = pic2_width,  // 圖像寬度
        .height = pic2_height // 圖像高度
    };

    // 將第二張圖像資料複製到影像緩衝區
    // 這種方法允許在顯示前對圖像進行進一步處理或合成
    epd_copy_to_framebuffer(area1, (uint8_t *)pic2_data, framebuffer);

    // 使用 update 函式顯示緩衝區內容，並測量執行時間
    update(3000);

    // ========== 第三張圖像：緩衝區顯示模式 ==========
    // 定義第三張圖像的顯示區域
    Rect_t area2 = {
        .x = 0,               // 起始 X 座標
        .y = 0,               // 起始 Y 座標
        .width = pic3_width,  // 圖像寬度
        .height = pic3_height // 圖像高度
    };

    // 將第三張圖像資料複製到影像緩衝區
    // 注意：這會覆蓋之前的緩衝區內容
    epd_copy_to_framebuffer(area2, (uint8_t *)pic3_data, framebuffer);

    // 顯示第三張圖像並測量執行時間
    update(3000);

    // 迴圈結束後會自動重新開始，持續循環顯示三張圖像
}
/*
drawImages.ino 程式註解總結
1. 程式頭部和設定說明
    版權資訊：添加中英文對照的版權說明
    Arduino IDE 設定：詳細說明每個設定項目的用途
    PSRAM 檢查：說明為什麼需要啟用 PSRAM
2. 程式功能概述
    主要功能：ESP32 電子紙靜態圖像顯示
    圖像格式：4位元灰階（16級），MSB優先
    性能指標：80MHz下的執行時間測量
    核心特色：循環顯示三張圖像，示範不同顯示模式
3. 標頭檔案和全域變數
    函式庫說明：每個 include 檔案的用途
    影像緩衝區：PSRAM 記憶體管理說明
4. setup() 函式詳解
    初始化流程：串列通訊、電子紙驅動程式
    記憶體分配：PSRAM 緩衝區配置和錯誤處理
    緩衝區初始化：白色背景設定
5. update() 函式功能
    電源管理：開啟/關閉電子紙電源
    時間測量：繪製效能監控
    顯示流程：清除、繪製、延遲的完整流程
6. loop() 主迴圈說明
    三階段顯示：
    pic1：直接顯示模式
    pic2：緩衝區模式
    pic3：緩衝區模式
    兩種顯示方式比較：
    直接顯示：立即傳送，適合單一圖像
    緩衝區模式：支援圖像處理和合成
7. 技術重點說明
    記憶體優化：使用 PSRAM 處理大圖像
    電力管理：適時開關電子紙電源
    效能監控：測量和輸出執行時間
    圖像處理：支援多種顯示模式
    這個程式現在包含了完整的中文技術文件，涵蓋了電子紙圖像顯示的核心概念：

    硬體設定：PSRAM 配置和電子紙初始化
    記憶體管理：大容量圖像緩衝區處理
    顯示技術：直接顯示 vs 緩衝區顯示
    效能優化：電源管理和執行時間監控
*/