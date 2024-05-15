#include <Wire.h>  // Thư viện cho giao tiếp I2C
#include "MAX30100_PulseOximeter.h"  // Thư viện cho cảm biến nhịp tim và SpO2 MAX30100
#include "LogisticRegressionCoefficients.h"  // Thư viện chứa hệ số cho mô hình hồi quy logistic
#include <cmath>  // Thư viện cho các hàm toán học, ví dụ như exp()

#include <ESP8266WiFi.h>  // Thư viện cho WiFi ESP8266
#include <BlynkSimpleEsp8266.h>  // Thư viện Blynk cho ESP8266
#include <ThingSpeak.h> // Thư viện ThingSpeak

// Thông tin template và token cho Blynk
#define BLYNK_TEMPLATE_ID "TMPL6E6qQGaNk"
#define BLYNK_TEMPLATE_NAME "GIAM SAT NHIP TIM 1"
#define BLYNK_AUTH_TOKEN "G51SpEg5NHnPtEWCSFHWCaHIiXTso0rX"
#define BLYNK_PRINT Serial  // Định nghĩa xuất dữ liệu qua Serial cho Blynk
// Thông tin mạng WiFi
char ssid[] = "UTE WIFI";
char pass[] = "";
WiFiClient  client;
unsigned long myChannelNumber = 2547584;  // Thay thế bằng Channel ID của bạn
const char * myWriteAPIKey = "C88H2AAIK8WQ3CWM";  // Thay thế YOUR_WRITE_API_KEY bằng Write API Key của bạn

#include <Adafruit_GFX.h>  // Thư viện đồ họa cơ bản cho màn hình
#include <Adafruit_SSD1306.h>  // Thư viện cho màn hình OLED SSD1306
#include <Fonts/FreeSerif9pt7b.h>  // Font chữ cho màn hình
Adafruit_SSD1306 display(128, 32, &Wire, -1);
#include <Ticker.h>  // Thư viện Ticker cho phép gọi hàm theo định kỳ
Ticker timer;
PulseOximeter pox;  // Tạo đối tượng cảm biến
unsigned long timeS = millis();  // Lấy thời gian hiện tại

WidgetLED LEDCONNECT(V0);  // Widget LED trên Blynk
#define LED_PIN V0  // Định nghĩa Virtual Pin cho đèn LED
WidgetLED ledConnection(LED_PIN);  // Tạo widget LED

// Hàm hồi quy logistic để tính xác suất
float logistic_regression(float x) {
  float z = coefficient * x + intercept;
  return 1 / (1 + exp(-z));
}

void setup() {
  Serial.begin(115200);  // Khởi động giao tiếp Serial với boud rate 115200
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);  // Khởi tạo kết nối Blynk
  Serial.begin(115200);  // Khởi động lại Serial để đảm bảo
  ledConnection.on();  // Bật đèn LED khi kết nối

  ThingSpeak.begin(client);

  // Khởi tạo màn hình OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  
    Serial.println("SSD1306 allocation failed");
    for (;;);  // Dừng chương trình nếu màn hình không khởi động được
  }

  display.setFont(&FreeSerif9pt7b);  // Đặt font chữ
  display.clearDisplay();  // Xóa màn hình
  display.setTextSize(1);             
  display.setTextColor(WHITE);        
  display.setCursor(30,20);             
  display.println("NHOM 4");          
  display.display();  // Hiển thị thông tin lên màn hình
  delay(2000);  // Đợi 2 giây

  // Khởi tạo cảm biến MAX30100
  if (!pox.begin()) {
    Serial.println("FAILED");
    for (;;);
  } else {
    Serial.println("SUCCESS");
  }
  pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);  // Đặt dòng điện cho đèn LED hồng ngoại
  pox.setOnBeatDetectedCallback(onBeatDetected);  // Đặt callback khi có nhịp tim được phát hiện
  timer.attach_ms(100, update);  // Cài đặt timer gọi hàm update mỗi 100ms
}

void loop() {
  Blynk.run();  // Chạy thư viện Blynk trong vòng lặp
  if (Blynk.connected()) {
    ledConnection.on();  // Bật đèn LED khi có kết nối
  } else {
    ledConnection.off();  // Tắt đèn LED khi mất kết nối
  }
  
  // Kiểm tra nếu quá 1000ms (1 giây) thì cập nhật lại dữ liệu từ cảm biến
  if (millis() - timeS > 1000) {
    float heartRate = pox.getHeartRate();  // Lấy nhịp tim
    float SpO2 = pox.getSpO2();  // Lấy nồng độ oxy

    float prediction = logistic_regression(heartRate);  // Dự đoán dựa trên nhịp tim
    int classification = prediction > 0.5 ? 1 : 0;  // Phân loại kết quả

    Serial.print("Predicted target: ");
    Serial.println(classification);

    Blynk.virtualWrite(V1, heartRate);  // Gửi nhịp tim lên Blynk
    //Blynk.virtualWrite(V2, SpO2);  // Gửi SpO2 lên Blynk
    Blynk.virtualWrite(V3, classification);  // Gửi kết quả phân loại lên Blynk
    Blynk.virtualWrite(V4, SpO2);

    // Cập nhật kênh ThingSpeak với dữ liệu nhịp tim và SpO2
    ThingSpeak.setField(1, heartRate);
    ThingSpeak.setField(2, SpO2);
  
    // Ghi dữ liệu lên ThingSpeak
    ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

    Serial.print("Heart rate:");
    Serial.print(heartRate);
    Serial.print("bpm / SpO2:");
    Serial.print(SpO2);
    Serial.println("%");
    display.clearDisplay();  // Xóa màn hình OLED
    display.setCursor(0,12);  
    display.print("HRate:");
    display.print(heartRate,0);  // Hiển thị nhịp tim
    display.print("-BB:");
    display.println(classification);  // Hiển thị kết quả phân loại
    display.setCursor(0,29);
    display.print("SpO2  : ");
    display.print(SpO2);  // Hiển thị SpO2
    display.println(" %");
    display.display();  // Cập nhật màn hình OLED

    timeS = millis();  // Cập nhật thời gian
  }
}

// Hàm được gọi khi có nhịp tim được phát hiện
void update() {
  pox.update();  // Cập nhật trạng thái cảm biến
}

// Callback khi có nhịp tim
void onBeatDetected() {
    Serial.println("Beat!");  // In ra "Beat!" khi có nhịp tim
}
