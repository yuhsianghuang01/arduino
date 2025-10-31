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

/*
 * ESP32 電子紙觸控介面示範程式
 *
 * 程式功能：
 * - 展示 GT911 觸控晶片的基本功能
 * - 在電子紙上顯示 ESP32 和螢幕的技術規格資訊
 * - 實現觸控座標偵測和顯示
 * - 提供多頁面資訊切換功能
 * - 示範電子紙與觸控面板的整合應用
 *
 * 技術特點：
 * - 使用 I2C 通訊控制 GT911 觸控晶片
 * - 支援多點觸控（最多 5 點）
 * - 實現觸控喚醒功能
 * - 使用 PSRAM 作為影像緩衝區
 * - 提供完整的文字排版和顯示功能
 *
 * 硬體需求：
 * - LilyGo T5 EPD47 開發板
 * - GT911 觸控面板
 * - 4.7 吋電子紙顯示器
 */

#include <Arduino.h>           // Arduino 核心函式庫
#include <esp_task_wdt.h>      // ESP32 任務看門狗
#include "freertos/FreeRTOS.h" // FreeRTOS 即時作業系統核心
#include "freertos/task.h"     // FreeRTOS 任務管理
#include "epd_driver.h"        // 電子紙驅動程式
#include "logo.h"              // 標誌圖像資料
#include "firasans.h"          // Fira Sans 字型檔案
#include <Wire.h>              // I2C 通訊函式庫
#include "lilygo.h"            // LilyGo 開發板硬體定義
#include <TouchDrvGT911.hpp>   // GT911 觸控驅動程式
#include "utilities.h"         // 公用函式和工具程式

TouchDrvGT911 touch;         // GT911 觸控驅動程式實例
uint8_t *framebuffer = NULL; // 影像緩衝區指標，用於存放電子紙顯示資料

// ESP32 系統概述文字內容
const char overview[] = {
    "   ESP32 is a single 2.4 GHz Wi-Fi-and-Bluetooth\n"
    "combo chip designed with the TSMC ultra-low-po\n"
    "wer 40 nm technology. It is designed to achieve \n"
    "the best power and RF performance, showing rob\n"
    "ustness versatility and reliability in a wide variet\n"
    "y of applications and power scenarios.\n"};
/*
 * ESP32 系統概述中文說明：
 * ESP32 是一款整合 2.4 GHz Wi-Fi 和藍牙功能的單晶片，
 * 採用台積電超低功耗 40 奈米製程技術設計。
 * 旨在實現最佳的功耗和射頻性能，
 * 在各種應用和功耗場景中展現出穩健性、多功能性和可靠性。
 */

// ESP32 微控制器功能特色
const char mcu_features[] = {
    "➸ Xtensa® dual-core 32-bit LX6 microprocessor\n"
    "➸ 448 KB ROM & External 16MBytes falsh\n"
    "➸ 520 KB SRAM & External 16MBytes PSRAM\n"
    "➸ 16 KB SRAM in RTC\n"
    "➸ Multi-connections in Classic BT and BLE\n"
    "➸ 802.11 n (2.4 GHz), up to 150 Mbps\n"};
/*
 * ESP32 微控制器功能特色中文說明：
 * ➸ Xtensa® 雙核心 32 位元 LX6 微處理器
 * ➸ 448 KB ROM & 外部 16MB Flash 快閃記憶體
 * ➸ 520 KB SRAM & 外部 16MB PSRAM 記憶體
 * ➸ 16 KB RTC 即時時鐘記憶體
 * ➸ 支援傳統藍牙和低功耗藍牙多重連線
 * ➸ 802.11 n (2.4 GHz) Wi-Fi，最高速度 150 Mbps
 */

// 電子紙螢幕功能特色
const char srceen_features[] = {
    "➸ 16 color grayscale\n"
    "➸ Use with 4.7\" EPDs\n"
    "➸ High-quality font rendering\n"
    "➸ ~630ms for full frame draw\n"};
/*
 * 電子紙螢幕功能特色中文說明：
 * ➸ 16 級灰階色彩顯示
 * ➸ 適用於 4.7 吋電子紙顯示器
 * ➸ 高品質字型渲染
 * ➸ 全畫面更新約需 630 毫秒
 */

// const char *string_array[] = {overview, mcu_features, srceen_features};
// 字串陣列（註解掉，可用於批次處理多個文字內容）

int cursor_x = 20; // 文字游標 X 座標，控制水平顯示位置
int cursor_y = 60; // 文字游標 Y 座標，控制垂直顯示位置

// 定義顯示區域的矩形範圍
Rect_t area1 = {
    .x = 10,                      // 起始 X 座標
    .y = 20,                      // 起始 Y 座標
    .width = EPD_WIDTH - 20,      // 寬度：螢幕寬度減去左右邊距
    .height = EPD_HEIGHT / 2 + 80 // 高度：螢幕高度的一半加上額外空間
};

uint8_t state = 1; // 狀態變數，用於控制顯示內容的切換

/**
 * 設定函式：初始化觸控電子紙系統
 *
 * 初始化流程：
 * 1. 初始化串列通訊
 * 2. 分配影像緩衝區記憶體
 * 3. 初始化電子紙驅動程式
 * 4. 處理深度睡眠喚醒延遲
 * 5. 初始化 I2C 通訊
 * 6. 喚醒觸控晶片
 * 7. 配置和啟動觸控驅動程式
 * 8. 顯示初始內容
 */
void setup()
{
    // 初始化串列通訊，波特率設為 115200
    Serial.begin(115200);

    // 在 PSRAM 中分配影像緩衝區記憶體
    // 使用 ps_calloc 確保記憶體被清零初始化
    framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    if (!framebuffer)
    {
        Serial.println("alloc memory failed !!!"); // 記憶體分配失敗提示
        while (1)
            ; // 記憶體分配失敗時停止執行
    }

    // 將影像緩衝區初始化為白色（0xFF 代表白色）
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    // 初始化電子紙驅動程式
    epd_init();

    //* Sleep wakeup must wait one second, otherwise the touch device cannot be addressed
    // 深度睡眠喚醒後必須等待一秒，否則無法正確存取觸控設備
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED)
    {
        delay(1000); // 等待觸控晶片穩定
    }

    // 初始化 I2C 通訊，連接觸控晶片
    Wire.begin(BOARD_SDA, BOARD_SCL);

    // Assuming that the previous touch was in sleep state, wake it up
    // 假設觸控晶片之前處於睡眠狀態，將其喚醒
    pinMode(TOUCH_INT, OUTPUT);    // 設定觸控中斷腳位為輸出模式
    digitalWrite(TOUCH_INT, HIGH); // 將觸控中斷腳位設為高電位，喚醒觸控晶片

    /*
     * The touch reset pin uses hardware pull-up,
     * and the function of setting the I2C device address cannot be used.
     * Use scanning to obtain the touch device address.*/
    /*
     * 觸控重置腳位使用硬體上拉電阻，
     * 無法使用設定 I2C 設備位址的功能。
     * 使用掃描方式取得觸控設備位址。
     */
    uint8_t touchAddress = 0; // 觸控晶片 I2C 位址

    // 嘗試連接 I2C 位址 0x14（GT911 的可能位址之一）
    Wire.beginTransmission(0x14);
    if (Wire.endTransmission() == 0)
    {
        touchAddress = 0x14; // 位址 0x14 回應正常
    }

    // 嘗試連接 I2C 位址 0x5D（GT911 的另一個可能位址）
    Wire.beginTransmission(0x5D);
    if (Wire.endTransmission() == 0)
    {
        touchAddress = 0x5D; // 位址 0x5D 回應正常
    }

    // 檢查是否找到有效的觸控設備位址
    if (touchAddress == 0)
    {
        while (1)
        {
            Serial.println("Failed to find GT911 - check your wiring!");
            // 找不到 GT911 觸控晶片 - 請檢查接線！
            delay(1000);
        }
    }

    // 配置觸控驅動程式腳位（重置腳位設為 -1，中斷腳位設為 TOUCH_INT）
    touch.setPins(-1, TOUCH_INT);

    // 初始化觸控驅動程式
    if (!touch.begin(Wire, touchAddress, BOARD_SDA, BOARD_SCL))
    {
        while (1)
        {
            Serial.println("Failed to find GT911 - check your wiring!");
            // 找不到 GT911 觸控晶片 - 請檢查接線！
            delay(1000);
        }
    }

    // 設定觸控座標的最大範圍（與電子紙螢幕尺寸一致）
    touch.setMaxCoordinates(EPD_WIDTH, EPD_HEIGHT);

    // 設定座標軸交換（適應不同的觸控面板方向）
    touch.setSwapXY(true);

    // 設定座標軸鏡像（X軸不鏡像，Y軸鏡像）
    touch.setMirrorXY(false, true);

    Serial.println("Started Touchscreen poll...");
    // 輸出：開始觸控螢幕輪詢...

    // 開啟電子紙電源並清除螢幕
    epd_poweron();
    epd_clear();

    // 在螢幕上顯示 ESP32 系統概述文字
    write_string((GFXfont *)&FiraSans, (char *)overview, &cursor_x, &cursor_y, framebuffer);

    // Draw Box
    //  繪製 "Prev" 按鈕框架
    epd_draw_rect(600, 450, 120, 60, 0, framebuffer);                         // 繪製矩形框（X=600, Y=450, 寬=120, 高=60）
    cursor_x = 615;                                                           // 設定按鈕文字 X 座標
    cursor_y = 490;                                                           // 設定按鈕文字 Y 座標
    writeln((GFXfont *)&FiraSans, "Prev", &cursor_x, &cursor_y, framebuffer); // 顯示 "Prev" 文字

    // 繪製 "Next" 按鈕框架
    epd_draw_rect(740, 450, 120, 60, 0, framebuffer);                         // 繪製矩形框（X=740, Y=450, 寬=120, 高=60）
    cursor_x = 755;                                                           // 設定按鈕文字 X 座標
    cursor_y = 490;                                                           // 設定按鈕文字 Y 座標
    writeln((GFXfont *)&FiraSans, "Next", &cursor_x, &cursor_y, framebuffer); // 顯示 "Next" 文字

    // 定義 LilyGo 標誌的顯示區域
    Rect_t area = {
        .x = 160,               // 標誌起始 X 座標
        .y = 420,               // 標誌起始 Y 座標
        .width = lilygo_width,  // 標誌寬度
        .height = lilygo_height // 標誌高度
    };

    // 將 LilyGo 標誌圖像複製到影像緩衝區
    epd_copy_to_framebuffer(area, (uint8_t *)lilygo_data, framebuffer);

    // 繪製主要內容區域的邊框
    epd_draw_rect(10, 20, EPD_WIDTH - 20, EPD_HEIGHT / 2 + 80, 0, framebuffer);

    // 將完整的影像緩衝區內容顯示到電子紙螢幕上
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);

    // 關閉電子紙電源以節省電力
    epd_poweroff();
}

int16_t x, y; // 觸控座標變數，用於儲存觸控點的 X 和 Y 座標

/**
 * 主迴圈函式：處理觸控事件和頁面切換
 *
 * 功能：
 * 1. 持續偵測觸控事件
 * 2. 處理 Prev/Next 按鈕點擊
 * 3. 根據狀態切換顯示內容
 * 4. 實現深度睡眠功能
 *
 * 觸控區域：
 * - Prev 按鈕：X(600-720), Y(450-510)
 * - Next 按鈕：X(740-860), Y(450-510)
 *
 * 顯示狀態：
 * 0: ESP32 系統概述
 * 1: 電子紙螢幕功能特色
 * 2: 微控制器功能特色
 * 3: 進入深度睡眠模式
 */
void loop()
{
    // 取得觸控點座標，如果有觸控事件則 touched 為 true
    uint8_t touched = touch.getPoint(&x, &y);
    if (touched)
    {
        // Serial.printf("X:%d Y:%d\n", x, y);  // 可用於除錯，顯示觸控座標

        // 檢查是否點擊 "Prev" 按鈕區域
        if ((x > 600 && x < 720) && (y > 450 && y < 510))
        {
            state--; // 狀態向前切換
        }
        // 檢查是否點擊 "Next" 按鈕區域
        else if ((x > 740 && x < 860) && (y > 450 && y < 510))
        {
            state++; // 狀態向後切換
        }
        // 如果點擊的是其他區域，不做任何處理
        else
        {
            return;
        }

        state %= 4; // 狀態循環：0, 1, 2, 3, 0, 1, 2, 3...

        // 輸出當前時間和狀態到串列埠
        Serial.print(millis());
        Serial.print(":");
        Serial.println(state);

        // 開啟電子紙電源準備更新顯示
        epd_poweron();

        // 重置文字游標位置
        cursor_x = 20;
        cursor_y = 60;

        // 根據當前狀態顯示不同內容
        switch (state)
        {
        case 0:
            // 狀態 0：顯示 ESP32 系統概述
            epd_clear_area(area1); // 清除顯示區域
            write_string((GFXfont *)&FiraSans, (char *)overview, &cursor_x, &cursor_y, NULL);
            break;
        case 1:
            // 狀態 1：顯示電子紙螢幕功能特色
            epd_clear_area(area1); // 清除顯示區域
            write_string((GFXfont *)&FiraSans, (char *)srceen_features, &cursor_x, &cursor_y, NULL);
            break;
        case 2:
            // 狀態 2：顯示微控制器功能特色
            epd_clear_area(area1); // 清除顯示區域
            write_string((GFXfont *)&FiraSans, (char *)mcu_features, &cursor_x, &cursor_y, NULL);
            break;
        case 3:
            // 狀態 3：進入深度睡眠模式
            delay(1000);           // 延遲 1 秒讓使用者看到狀態變化
            epd_clear_area(area1); // 清除顯示區域
            write_string((GFXfont *)&FiraSans, "DeepSleep", &cursor_x, &cursor_y, NULL);

            // The touch interrupt uses non-RTC-IO, so the touch wake-up function cannot be used to set the touch to sleep
            // 觸控中斷使用非 RTC-IO，因此無法使用觸控喚醒功能，需要將觸控設為睡眠模式
            touch.sleep();

            delay(5); // 短暫延遲確保觸控晶片進入睡眠

            Wire.end(); // 結束 I2C 通訊

            // 設定相關腳位為開漏輸出，降低深度睡眠時的功耗
            pinMode(BOARD_SDA, OPEN_DRAIN); // I2C 資料線設為開漏
            pinMode(BOARD_SCL, OPEN_DRAIN); // I2C 時鐘線設為開漏
            pinMode(TOUCH_INT, OPEN_DRAIN); // 觸控中斷線設為開漏

            // 關閉所有電子紙相關電源
            epd_poweroff_all();

#if defined(CONFIG_IDF_TARGET_ESP32)
            // Set to wake up by GPIO39
            // ESP32：設定 GPIO39 作為喚醒源（按鈕按下時為低電位）
            esp_sleep_enable_ext1_wakeup(GPIO_SEL_39, ESP_EXT1_WAKEUP_ANY_LOW);
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
            // ESP32S3：設定 GPIO21 作為喚醒源（按鈕按下時為低電位）
            esp_sleep_enable_ext1_wakeup(GPIO_SEL_21, ESP_EXT1_WAKEUP_ANY_LOW);
#endif

            // 進入深度睡眠模式，等待外部喚醒
            esp_deep_sleep_start();
            break;
        case 4:
            // 保留狀態，無特殊操作
            break;
        default:
            // 預設狀態，無特殊操作
            break;
        }

        // 關閉電子紙電源以節省電力
        epd_poweroff();
    }

    // 主迴圈延遲 10 毫秒，避免過度耗電和提供穩定的觸控偵測頻率
    delay(10);
}
/*
touch.ino 程式的完整中文註解。讓我總結一下為這個觸控介面程式添加的詳細說明：

touch.ino 程式註解總結
1. 程式概述和功能
    主要功能：ESP32 電子紙觸控介面示範程式
    技術整合：GT911 觸控晶片 + 4.7吋電子紙顯示器
    應用展示：多頁面資訊切換、觸控偵測、深度睡眠管理
2. 硬體架構說明
    觸控系統：GT911 電容式觸控晶片，支援多點觸控
    顯示系統：4.7吋 16級灰階電子紙
    通訊介面：I2C 控制觸控晶片
    電源管理：深度睡眠模式和喚醒機制
3. 程式資料結構
    顯示內容：三組技術規格文字（系統概述、螢幕特色、MCU特色）
    座標系統：觸控座標與電子紙像素對應
    狀態管理：4個狀態循環切換（0-3）
4. 初始化流程（setup）
    記憶體配置：PSRAM 影像緩衝區分配
    睡眠處理：深度睡眠喚醒後的穩定化延遲
    I2C 掃描：自動偵測 GT911 的實際位址
    觸控配置：座標範圍、軸向映射、鏡像設定
    介面建立：按鈕框架、標誌圖像、邊框繪製
5. 觸控事件處理（loop）
    座標偵測：即時獲取觸控點位置
    按鈕識別：
    Prev 按鈕：(600-720, 450-510)
    Next 按鈕：(740-860, 450-510)
    狀態切換：循環顯示不同技術資訊
    頁面更新：區域清除和文字重新渲染
6. 電源管理機制
    動態電源控制：按需開關電子紙電源
    深度睡眠準備：
    觸控晶片睡眠模式
    I2C 通訊結束
    GPIO 設為開漏降低功耗
    喚醒配置：
    ESP32：GPIO39 按鈕喚醒
    ESP32S3：GPIO21 按鈕喚醒
7. 顯示內容管理
    多語言支援：英文原文 + 中文詳細說明
    技術規格展示：
    ESP32 系統概述
    電子紙螢幕功能特色
    微控制器功能特色
    動態更新：區域清除確保顯示品質
8. 觸控技術特點
    多點觸控：GT911 支援最多5點同時偵測
    座標校正：軸向交換和鏡像適應不同安裝方向
    中斷機制：硬體中斷提高響應速度
    位址自適應：自動掃描 0x14 和 0x5D 兩個可能位址
9. 記憶體最佳化
    PSRAM 利用：大容量外部記憶體處理圖像資料
    緩衝區管理：統一的影像緩衝區處理所有圖形元素
    記憶體檢查：分配失敗時的安全處理
10. 使用者體驗設計
    直覺操作：簡單的 Prev/Next 按鈕導航
    視覺回饋：清晰的按鈕框架和文字顯示
    狀態指示：串列埠輸出當前狀態資訊
    節能設計：自動進入深度睡眠節省電力
這個程式現在包含了完整的中文技術文件，涵蓋了觸控電子紙系統的核心技術：

    觸控技術：電容式觸控的偵測和校正
    顯示技術：電子紙的區域更新和電源管理
    系統整合：多硬體模組的協調控制
    電源管理：深度睡眠和喚醒機制
    使用者介面：直覺的觸控操作體驗
*/