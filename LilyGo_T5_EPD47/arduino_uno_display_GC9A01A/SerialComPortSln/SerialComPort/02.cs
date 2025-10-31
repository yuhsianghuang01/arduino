using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.IO.Ports;
using System.Threading;

namespace ArduinoImageSender
{
    /// <summary>
    /// Arduino UNO GC9A01A 圖片傳送 Console 程式 (.NET Framework 4.6.0)
    /// 透過Serial Port將圖片傳送到Arduino UNO控制的GC9A01A圓形顯示器
    /// 
    /// 使用方式:
    /// 1. 編譯程式
    /// 2. 執行 ArduinoImageSender.exe
    /// 3. 輸入COM埠名稱 (例如: COM3)
    /// 4. 輸入圖片檔案路徑
    /// 5. 程式會自動調整圖片大小並傳送到Arduino
    /// 
    /// 需要參考的組件:
    /// - System.Drawing
    /// - System.IO.Ports
    /// </summary>
    class Program
    {
        // GC9A01A 顯示器規格
        private const int DISPLAY_WIDTH = 240;
        private const int DISPLAY_HEIGHT = 240;
        private const int BAUD_RATE = 115200;
        private const int SEND_DELAY_MS = 1; // 傳送延遲，避免Arduino緩衝區溢出

        static void Main(string[] args)
        {
            Console.WriteLine("===========================================");
            Console.WriteLine("Arduino UNO GC9A01A 圖片傳送器 v1.0");
            Console.WriteLine("===========================================\n");

            try
            {
                // 顯示可用串列埠
                ShowAvailablePorts();

                // 取得使用者輸入
                string portName = GetPortFromUser();
                string imagePath = GetImagePathFromUser();

                // 驗證檔案存在
                if (!File.Exists(imagePath))
                {
                    Console.WriteLine($"✗ 錯誤: 找不到圖片檔案 '{imagePath}'");
                    WaitForExit();
                    return;
                }

                // 開始傳送流程
                SendImageToArduino(portName, imagePath);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"✗ 程式執行錯誤: {ex.Message}");
            }

            WaitForExit();
        }

        /// <summary>
        /// 顯示可用的串列埠
        /// </summary>
        static void ShowAvailablePorts()
        {
            string[] ports = SerialPort.GetPortNames();
            Console.WriteLine("可用的串列埠:");

            if (ports.Length == 0)
            {
                Console.WriteLine("  (沒有找到可用的串列埠)");
            }
            else
            {
                foreach (string port in ports)
                {
                    Console.WriteLine($"  • {port}");
                }
            }
            Console.WriteLine();
        }

        /// <summary>
        /// 從使用者取得串列埠名稱
        /// </summary>
        static string GetPortFromUser()
        {
            while (true)
            {
                Console.Write("請輸入串列埠名稱 (例如: COM3): ");
                string input = Console.ReadLine();

                if (!string.IsNullOrEmpty(input))
                {
                    return input.Trim().ToUpper();
                }

                Console.WriteLine("✗ 請輸入有效的串列埠名稱\n");
            }
        }

        /// <summary>
        /// 從使用者取得圖片檔案路徑
        /// </summary>
        static string GetImagePathFromUser()
        {
            while (true)
            {
                Console.Write("請輸入圖片檔案路徑: ");
                string input = Console.ReadLine();

                if (!string.IsNullOrEmpty(input))
                {
                    // 移除路徑兩端的引號和空格
                    return input.Trim('"', ' ');
                }

                Console.WriteLine("✗ 請輸入有效的檔案路徑\n");
            }
        }

        /// <summary>
        /// 傳送圖片到Arduino
        /// </summary>
        static void SendImageToArduino(string portName, string imagePath)
        {
            SerialPort serialPort = null;

            try
            {
                Console.WriteLine($"\n正在連接到 {portName}...");

                // 建立串列埠連接
                serialPort = new SerialPort(portName, BAUD_RATE)
                {
                    Timeout = 5000,
                    ReadTimeout = 2000,
                    WriteTimeout = 5000
                };

                serialPort.Open();
                Console.WriteLine("✓ 串列埠連接成功");

                // 等待Arduino重置和初始化
                Console.WriteLine("等待Arduino準備就緒...");
                Thread.Sleep(3000);

                // 嘗試讀取Arduino的準備訊息
                try
                {
                    if (serialPort.BytesToRead > 0)
                    {
                        string arduinoMessage = serialPort.ReadLine();
                        Console.WriteLine($"Arduino: {arduinoMessage}");
                    }
                }
                catch
                {
                    Console.WriteLine("Arduino: (準備就緒)");
                }

                // 載入和處理圖片
                Console.WriteLine($"\n正在載入圖片: {Path.GetFileName(imagePath)}");
                using (Bitmap originalImage = new Bitmap(imagePath))
                {
                    Console.WriteLine($"原始圖片尺寸: {originalImage.Width} x {originalImage.Height}");

                    // 調整圖片尺寸為240x240
                    using (Bitmap resizedImage = ResizeImage(originalImage, DISPLAY_WIDTH, DISPLAY_HEIGHT))
                    {
                        Console.WriteLine($"調整後尺寸: {DISPLAY_WIDTH} x {DISPLAY_HEIGHT}");
                        Console.WriteLine("\n開始傳送圖片資料到Arduino...");

                        // 傳送像素資料
                        SendPixelData(serialPort, resizedImage);
                    }
                }

                Console.WriteLine("\n✓ 圖片傳送完成!");
            }
            catch (UnauthorizedAccessException)
            {
                Console.WriteLine($"✗ 錯誤: 無法存取串列埠 {portName}，可能被其他程式使用中");
            }
            catch (ArgumentException)
            {
                Console.WriteLine($"✗ 錯誤: 串列埠名稱 {portName} 無效");
            }
            catch (IOException ex)
            {
                Console.WriteLine($"✗ 錯誤: 串列埠通訊失敗 - {ex.Message}");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"✗ 錯誤: {ex.Message}");
            }
            finally
            {
                // 關閉串列埠
                if (serialPort != null && serialPort.IsOpen)
                {
                    serialPort.Close();
                    Console.WriteLine("串列埠已關閉");
                }
                if (serialPort != null)
                {
                    serialPort.Dispose();
                }
            }
        }

        /// <summary>
        /// 調整圖片尺寸 (.NET Framework 4.6.0 相容版本)
        /// </summary>
        static Bitmap ResizeImage(Bitmap originalImage, int targetWidth, int targetHeight)
        {
            Bitmap resizedImage = new Bitmap(targetWidth, targetHeight);

            using (Graphics graphics = Graphics.FromImage(resizedImage))
            {
                // 設定高品質縮放參數
                graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
                graphics.SmoothingMode = SmoothingMode.HighQuality;
                graphics.PixelOffsetMode = PixelOffsetMode.HighQuality;
                graphics.CompositingHint = CompositingHint.AssumeLinear;

                // 繪製調整後的圖片
                graphics.DrawImage(originalImage, 0, 0, targetWidth, targetHeight);
            }

            return resizedImage;
        }

        /// <summary>
        /// 傳送像素資料到Arduino
        /// </summary>
        static void SendPixelData(SerialPort serialPort, Bitmap bitmap)
        {
            int totalPixels = DISPLAY_WIDTH * DISPLAY_HEIGHT;
            int pixelsSent = 0;
            int lastProgressPercentage = -1;

            // 逐行掃描並傳送像素
            for (int y = 0; y < DISPLAY_HEIGHT; y++)
            {
                for (int x = 0; x < DISPLAY_WIDTH; x++)
                {
                    // 取得像素顏色
                    Color pixel = bitmap.GetPixel(x, y);

                    // 轉換為RGB565格式
                    ushort rgb565 = ConvertToRGB565(pixel);

                    // 分割為2個bytes並傳送 (Big Endian)
                    byte highByte = (byte)(rgb565 >> 8);
                    byte lowByte = (byte)(rgb565 & 0xFF);

                    serialPort.Write(new byte[] { highByte, lowByte }, 0, 2);

                    pixelsSent++;

                    // 顯示進度
                    int progressPercentage = (pixelsSent * 100) / totalPixels;
                    if (progressPercentage != lastProgressPercentage)
                    {
                        Console.Write($"\r進度: {progressPercentage}% ({pixelsSent}/{totalPixels} 像素)");
                        lastProgressPercentage = progressPercentage;
                    }

                    // 小延遲避免Arduino緩衝區溢出
                    if (pixelsSent % 100 == 0)
                    {
                        Thread.Sleep(SEND_DELAY_MS);
                    }
                }
            }

            Console.WriteLine(); // 換行
        }

        /// <summary>
        /// 將Color轉換為RGB565格式
        /// RGB565: 5位元紅色 + 6位元綠色 + 5位元藍色 = 16位元
        /// </summary>
        static ushort ConvertToRGB565(Color color)
        {
            // 將8位元顏色值轉換為RGB565格式
            byte r5 = (byte)(color.R >> 3);  // 8位元 -> 5位元 (捨棄低3位元)
            byte g6 = (byte)(color.G >> 2);  // 8位元 -> 6位元 (捨棄低2位元)
            byte b5 = (byte)(color.B >> 3);  // 8位元 -> 5位元 (捨棄低3位元)

            // 組合成16位元RGB565格式
            // RRRRRGGGGGGBBBBB
            return (ushort)((r5 << 11) | (g6 << 5) | b5);
        }

        /// <summary>
        /// 等待使用者按鍵退出
        /// </summary>
        static void WaitForExit()
        {
            Console.WriteLine("\n按任意鍵退出程式...");
            Console.ReadKey();
        }
    }
}