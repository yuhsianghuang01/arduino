using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.IO.Ports;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace ImageSender
{
    /// <summary>
    /// Arduino GC9A01A 圖片傳送器
    /// 透過Serial Port將圖片傳送到Arduino UNO控制的GC9A01A圓形顯示器
    /// </summary>
    public class ArduinoImageSender
    {
        private SerialPort serialPort;
        private const int IMAGE_WIDTH = 240;
        private const int IMAGE_HEIGHT = 240;
        private const int BAUD_RATE = 115200;
        private const int BUFFER_SIZE = 512;

        /// <summary>
        /// 傳送進度事件
        /// </summary>
        public event EventHandler<ProgressEventArgs> ProgressChanged;

        /// <summary>
        /// 傳送完成事件
        /// </summary>
        public event EventHandler<bool> SendCompleted;

        /// <summary>
        /// 建構子
        /// </summary>
        /// <param name="portName">串列埠名稱 (例如: COM3)</param>
        public ArduinoImageSender(string portName)
        {
            serialPort = new SerialPort(portName, BAUD_RATE)
            {
                Timeout = 5000,
                ReadTimeout = 5000,
                WriteTimeout = 5000
            };
        }

        /// <summary>
        /// 開啟串列埠連接
        /// </summary>
        /// <returns>是否成功開啟</returns>
        public bool OpenConnection()
        {
            try
            {
                if (!serialPort.IsOpen)
                {
                    serialPort.Open();
                    Thread.Sleep(2000); // 等待Arduino重置

                    // 等待Arduino準備就緒訊息
                    string readyMessage = serialPort.ReadLine();
                    Console.WriteLine($"Arduino: {readyMessage}");

                    return true;
                }
                return serialPort.IsOpen;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"開啟串列埠失敗: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// 關閉串列埠連接
        /// </summary>
        public void CloseConnection()
        {
            try
            {
                if (serialPort?.IsOpen == true)
                {
                    serialPort.Close();
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"關閉串列埠失敗: {ex.Message}");
            }
        }

        /// <summary>
        /// 同步傳送圖片到Arduino
        /// </summary>
        /// <param name="imagePath">圖片檔案路徑</param>
        /// <returns>是否成功傳送</returns>
        public bool SendImage(string imagePath)
        {
            try
            {
                if (!File.Exists(imagePath))
                {
                    Console.WriteLine($"圖片檔案不存在: {imagePath}");
                    return false;
                }

                if (!serialPort.IsOpen)
                {
                    Console.WriteLine("串列埠未開啟");
                    return false;
                }

                // 載入並處理圖片
                using (Bitmap originalImage = new Bitmap(imagePath))
                {
                    using (Bitmap resizedImage = ResizeImage(originalImage, IMAGE_WIDTH, IMAGE_HEIGHT))
                    {
                        return SendBitmapData(resizedImage);
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"傳送圖片失敗: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// 非同步傳送圖片到Arduino
        /// </summary>
        /// <param name="imagePath">圖片檔案路徑</param>
        /// <param name="cancellationToken">取消權杖</param>
        /// <returns>Task</returns>
        public async Task<bool> SendImageAsync(string imagePath, CancellationToken cancellationToken = default)
        {
            return await Task.Run(() => SendImage(imagePath), cancellationToken);
        }

        /// <summary>
        /// 調整圖片尺寸
        /// </summary>
        /// <param name="originalImage">原始圖片</param>
        /// <param name="width">目標寬度</param>
        /// <param name="height">目標高度</param>
        /// <returns>調整後的圖片</returns>
        private Bitmap ResizeImage(Bitmap originalImage, int width, int height)
        {
            Bitmap resizedImage = new Bitmap(width, height);

            using (Graphics graphics = Graphics.FromImage(resizedImage))
            {
                graphics.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
                graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.HighQuality;
                graphics.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
                graphics.CompositingHint = System.Drawing.Drawing2D.CompositingHint.AssumeLinear;

                graphics.DrawImage(originalImage, 0, 0, width, height);
            }

            return resizedImage;
        }

        /// <summary>
        /// 傳送Bitmap資料到Arduino
        /// </summary>
        /// <param name="bitmap">要傳送的圖片</param>
        /// <returns>是否成功傳送</returns>
        private bool SendBitmapData(Bitmap bitmap)
        {
            try
            {
                int totalPixels = IMAGE_WIDTH * IMAGE_HEIGHT;
                int pixelsSent = 0;
                byte[] buffer = new byte[BUFFER_SIZE];
                int bufferIndex = 0;

                Console.WriteLine($"開始傳送圖片資料... ({IMAGE_WIDTH}x{IMAGE_HEIGHT})");

                // 逐像素處理並傳送
                for (int y = 0; y < IMAGE_HEIGHT; y++)
                {
                    for (int x = 0; x < IMAGE_WIDTH; x++)
                    {
                        Color pixel = bitmap.GetPixel(x, y);
                        ushort rgb565 = ConvertToRGB565(pixel);

                        // 將16位元RGB565分成2個byte (Big Endian)
                        buffer[bufferIndex++] = (byte)(rgb565 >> 8);
                        buffer[bufferIndex++] = (byte)(rgb565 & 0xFF);

                        pixelsSent++;

                        // 當緩衝區滿了或是最後一批資料時，傳送到Arduino
                        if (bufferIndex >= BUFFER_SIZE || pixelsSent >= totalPixels)
                        {
                            serialPort.Write(buffer, 0, bufferIndex);
                            bufferIndex = 0;

                            // 更新進度
                            int progress = (pixelsSent * 100) / totalPixels;
                            ProgressChanged?.Invoke(this, new ProgressEventArgs(progress, pixelsSent, totalPixels));

                            // 短暫延遲避免Arduino緩衝區溢出
                            Thread.Sleep(1);
                        }
                    }
                }

                Console.WriteLine("圖片傳送完成!");
                SendCompleted?.Invoke(this, true);
                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"傳送資料失敗: {ex.Message}");
                SendCompleted?.Invoke(this, false);
                return false;
            }
        }

        /// <summary>
        /// 將Color轉換為RGB565格式
        /// </summary>
        /// <param name="color">顏色</param>
        /// <returns>RGB565格式的16位元值</returns>
        private ushort ConvertToRGB565(Color color)
        {
            byte r = (byte)(color.R >> 3);  // 5位元紅色
            byte g = (byte)(color.G >> 2);  // 6位元綠色
            byte b = (byte)(color.B >> 3);  // 5位元藍色

            return (ushort)((r << 11) | (g << 5) | b);
        }

        /// <summary>
        /// 取得可用的串列埠清單
        /// </summary>
        /// <returns>串列埠名稱陣列</returns>
        public static string[] GetAvailablePorts()
        {
            return SerialPort.GetPortNames();
        }

        /// <summary>
        /// 釋放資源
        /// </summary>
        public void Dispose()
        {
            CloseConnection();
            serialPort?.Dispose();
        }
    }

    /// <summary>
    /// 進度事件參數
    /// </summary>
    public class ProgressEventArgs : EventArgs
    {
        public int ProgressPercentage { get; }
        public int PixelsSent { get; }
        public int TotalPixels { get; }

        public ProgressEventArgs(int progressPercentage, int pixelsSent, int totalPixels)
        {
            ProgressPercentage = progressPercentage;
            PixelsSent = pixelsSent;
            TotalPixels = totalPixels;
        }
    }

    /// <summary>
    /// 控制台應用程式範例
    /// </summary>
    class Program
    {
        static async Task Main(string[] args)
        {
            Console.WriteLine("Arduino GC9A01A 圖片傳送器 v1.0");
            Console.WriteLine("===================================");

            // 顯示可用的串列埠
            string[] ports = ArduinoImageSender.GetAvailablePorts();
            Console.WriteLine("可用的串列埠:");
            foreach (string port in ports)
            {
                Console.WriteLine($"  {port}");
            }

            // 取得使用者輸入
            Console.Write("\n請輸入串列埠名稱 (例如: COM3): ");
            string portName = Console.ReadLine();

            Console.Write("請輸入圖片檔案路徑: ");
            string imagePath = Console.ReadLine();

            // 移除路徑兩端的引號（如果有的話）
            imagePath = imagePath.Trim('"');

            // 建立傳送器
            ArduinoImageSender sender = new ArduinoImageSender(portName);

            // 註冊事件處理器
            sender.ProgressChanged += (s, e) =>
            {
                Console.Write($"\r傳送進度: {e.ProgressPercentage}% ({e.PixelsSent}/{e.TotalPixels})");
            };

            sender.SendCompleted += (s, success) =>
            {
                Console.WriteLine(success ? "\n✓ 傳送成功!" : "\n✗ 傳送失敗!");
            };

            try
            {
                // 開啟連接
                Console.WriteLine("\n正在連接Arduino...");
                if (sender.OpenConnection())
                {
                    Console.WriteLine("✓ 連接成功!");

                    // 傳送圖片
                    bool result = await sender.SendImageAsync(imagePath);

                    if (result)
                    {
                        Console.WriteLine("圖片已成功傳送到Arduino!");
                    }
                }
                else
                {
                    Console.WriteLine("✗ 連接失敗!");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"錯誤: {ex.Message}");
            }
            finally
            {
                // 關閉連接
                sender.CloseConnection();
                sender.Dispose();
            }

            Console.WriteLine("\n按任意鍵退出...");
            Console.ReadKey();
        }
    }
}