#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <DFPlayerMini_Fast.h>

// 🔗 Thông tin WiFi & API
const char* ssid = "Le Tan";
const char* password = "12345678";
String api_token = "FOE8NU31EPPH0KIGPSBV92XOJ0GJY46OHRI9K2VNKUTMZALAJ1ERDRF4ZTMFVDFX";
String accountNumber = "LOCSPAY000308452";

// 🛠 Cấu hình phần cứng
#define DFPLAYER_RX 12  // D6 Chân RX của DFPlayer
#define DFPLAYER_TX 14  // D5 Chân TX của DFPlayer
#define LCD_SDA 4      // D2 Chân SDA của LCD
#define LCD_SCL 5      // D1 Chân SCL của LCD
#define VOLUME 30      // Âm lượng của DFPlayer (0-30)

// Định nghĩa thời gian delay chi tiết cho từng thành phần
#define DELAY_AFTER_GREETING 1000    // Delay sau câu chào đầu tiên
#define DELAY_AFTER_NUMBER 400       // Delay sau khi đọc số (1, 2, 3...)
#define DELAY_AFTER_MILLION 400      // Delay sau khi đọc "triệu"
#define DELAY_AFTER_HUNDRED 400      // Delay sau khi đọc "trăm"
#define DELAY_AFTER_THOUSAND 400     // Delay sau khi đọc "nghìn"
#define DELAY_AFTER_TEN 400          // Delay sau khi đọc "mươi"
#define DELAY_AFTER_UNIT 400         // Delay sau khi đọc đơn vị cuối (triệu, nghìn...)
#define DELAY_BEFORE_CURRENCY 400    // Delay trước khi đọc "đồng"
#define DELAY_AFTER_CURRENCY 400     // Delay sau khi đọc "đồng"
#define DELAY_BETWEEN_GROUPS 400     // Delay giữa các nhóm số (triệu, nghìn...)

// ⚡ Biến toàn cục
SoftwareSerial dfPlayerSerial(DFPLAYER_RX, DFPLAYER_TX);
DFPlayerMini_Fast dfPlayer;
LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long timeGetData = 0;
String transactionDate = "";
long amountIn = 0;
bool isSpeaking = false;

// 📌 Thông tin tài khoản
String accountName = "PHAM QUANG VINH";
String bankName = "ACB";

// 🎵 Audio file mapping
#define AUDIO_GREETING 1        // Chào mừng
#define AUDIO_DONG 2            // Đồng
#define AUDIO_NUMBER_OFFSET 3   // Numbers 1-9 are files 4-12
#define AUDIO_MUOI 13           // Mười
#define AUDIO_MUOI_OFFSET 21    // "Hai mươi" to "Chín mươi" are files 21-29
#define AUDIO_TRAM 31           // Trăm
#define AUDIO_NGHIN 32          // Nghìn
#define AUDIO_TRIEU 33          // Triệu
#define AUDIO_NOTIFICATION 34   // Thông báo đã nhận tiền (nếu có)

// Biến để lưu thời gian phát âm ước tính cho file hiện tại
unsigned long currentAudioDuration = 0;

// 🔢 Định dạng số tiền
String formatNumber(long number) {
  String numStr = String(number);
  String formatted = "";
  int len = numStr.length();

  for (int i = 0; i < len; i++) {
    formatted += numStr[i];
    if ((len - i - 1) % 3 == 0 && i < len - 1) {
      formatted += ".";
    }
  }
  return formatted;
}

// 📺 Hiển thị thông tin tài khoản trên LCD
void displayAccountInfo() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(accountName);
  lcd.setCursor(0, 1);
  lcd.print(bankName);
}

// 📺 Hiển thị số tiền trên LCD
void displayAmount(long amount) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ban da nhan duoc:");
  
  // Format with thousand separators
  String formattedAmount = formatNumber(amount);
  
  lcd.setCursor(0, 1);
  lcd.print(formattedAmount + " d");
}

// Play audio with appropriate delay
void playAudio(int fileNumber) {
  if (fileNumber <= 0) return;
  
  dfPlayer.play(fileNumber);
  
  // Dynamic delay based on file type
  if (fileNumber == AUDIO_GREETING) {
    currentAudioDuration = 3000;   // Khoảng 3 giây
  } else if (fileNumber == AUDIO_DONG) {
    currentAudioDuration = 800;    // Khoảng 0.8 giây
  } else if (fileNumber >= 4 && fileNumber <= 12) {  // Các số từ 1-9
    currentAudioDuration = 600;    // Khoảng 0.6 giây  
  } else if (fileNumber == AUDIO_MUOI) {
    currentAudioDuration = 600;
  } else if (fileNumber >= 21 && fileNumber <= 29) { // "Hai mươi" đến "Chín mươi"
    currentAudioDuration = 700;
  } else if (fileNumber == AUDIO_TRIEU) {
    currentAudioDuration = 600;
  } else if (fileNumber == AUDIO_TRAM) {
    currentAudioDuration = 500;
  } else if (fileNumber == AUDIO_NGHIN) {
    currentAudioDuration = 600;
  } else {
    currentAudioDuration = 700;    // Giá trị mặc định
  }
  
  delay(currentAudioDuration);
}

// Main function to announce amount
void announceAmount(long amount) {
  displayAmount(amount);
  isSpeaking = true;
  
  // Thông báo bắt đầu
  playAudio(AUDIO_GREETING);
  delay(DELAY_AFTER_GREETING);
  
  bool hasSpokenAny = false; // Đánh dấu đã đọc phần nào chưa
  
  if (amount >= 1000000) {  // Hàng triệu
    int millions = amount / 1000000;
    playAudio(millions + AUDIO_NUMBER_OFFSET);
    delay(DELAY_AFTER_NUMBER);
    
    playAudio(AUDIO_TRIEU);
    delay(DELAY_AFTER_MILLION);
    
    amount %= 1000000;
    hasSpokenAny = true;
    
    // Nếu còn số phía sau, delay thêm
    if (amount > 0) {
      delay(DELAY_BETWEEN_GROUPS);
    }
  }
  
  if (amount >= 100000) {  // Hàng trăm nghìn
    int hundredThousands = amount / 100000;
    playAudio(hundredThousands + AUDIO_NUMBER_OFFSET);
    delay(DELAY_AFTER_NUMBER);
    
    playAudio(AUDIO_TRAM);
    delay(DELAY_AFTER_HUNDRED);
    
    amount %= 100000;
    
    if (amount == 0 && hasSpokenAny) {
      // Nếu số còn lại là 0 và đã đọc phần triệu, thêm "nghìn"
      playAudio(AUDIO_NGHIN);
      delay(DELAY_AFTER_THOUSAND);
    }
    
    hasSpokenAny = true;
  }
  
  if (amount >= 10000 && amount < 100000) {  // Hàng chục nghìn (không đọc lại nếu đã đọc hàng trăm nghìn)
    int tenThousands = amount / 10000;
    
    if (tenThousands >= 2) {
      // "Hai mươi", "Ba mươi",...
      playAudio(AUDIO_MUOI_OFFSET + tenThousands);
      delay(DELAY_AFTER_TEN);
    } else {
      playAudio(AUDIO_MUOI);
      delay(DELAY_AFTER_TEN);
    }
    
    amount %= 10000;
    
    if (amount >= 1000) {
      int thousands = amount / 1000;
      if (tenThousands >= 1 && thousands > 0) {
        playAudio(thousands + AUDIO_NUMBER_OFFSET);
        delay(DELAY_AFTER_NUMBER);
      }
      amount %= 1000;
    }
    
    playAudio(AUDIO_NGHIN);
    delay(DELAY_AFTER_THOUSAND);
    
    hasSpokenAny = true;
  }
  else if (amount >= 1000) {  // Hàng nghìn (< 10.000)
    int thousands = amount / 1000;
    playAudio(thousands + AUDIO_NUMBER_OFFSET);
    delay(DELAY_AFTER_NUMBER);
    
    playAudio(AUDIO_NGHIN);
    delay(DELAY_AFTER_THOUSAND);
    
    amount %= 1000;
    
    hasSpokenAny = true;
  }
  
  if (amount >= 100) {  // Hàng trăm
    int hundreds = amount / 100;
    playAudio(hundreds + AUDIO_NUMBER_OFFSET);
    delay(DELAY_AFTER_NUMBER);
    
    playAudio(AUDIO_TRAM);
    delay(DELAY_AFTER_HUNDRED);
    
    amount %= 100;
    
    hasSpokenAny = true;
  }
  
  if (amount > 0) {
    if (amount >= 20) {
      int tens = amount / 10;
      playAudio(AUDIO_MUOI_OFFSET + tens);
      delay(DELAY_AFTER_TEN);
      
      int ones = amount % 10;
      if (ones > 0) {
        playAudio(ones + AUDIO_NUMBER_OFFSET);
        delay(DELAY_AFTER_NUMBER);
      }
    } else if (amount >= 10) {
      playAudio(AUDIO_MUOI);
      delay(DELAY_AFTER_TEN);
      
      int ones = amount % 10;
      if (ones > 0) {
        playAudio(ones + AUDIO_NUMBER_OFFSET);
        delay(DELAY_AFTER_NUMBER);
      }
    } else {
      playAudio(amount + AUDIO_NUMBER_OFFSET);
      delay(DELAY_AFTER_NUMBER);
    }
    hasSpokenAny = true;
  }
  
  if (hasSpokenAny) {
    delay(DELAY_BEFORE_CURRENCY);
  }
  
  playAudio(AUDIO_DONG);
  delay(DELAY_AFTER_CURRENCY);
  
  isSpeaking = false;
}

// 🔗 Lấy dữ liệu từ API sePay
void getTransactionData() {
  if (WiFi.status() == WL_CONNECTED) {
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setInsecure();

    HTTPClient http;
    String apiUrl = "https://my.sepay.vn/userapi/transactions/list?account_number=" + accountNumber + "&limit=5";

    Serial.println("🔗 Đang gửi yêu cầu HTTPS...");
    http.begin(*client, apiUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + String(api_token));

    int httpCode = http.GET();
    Serial.println("📡 HTTP CODE: " + String(httpCode));

    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("📥 JSON nhận được:");
      Serial.println(payload);

      // 🎯 Giải mã JSON
      StaticJsonDocument<2048> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print("❌ Lỗi JSON: ");
        Serial.println(error.f_str());
        return;
      }

      JsonArray transactions = doc["transactions"].as<JsonArray>();
      if (transactions.size() > 0) {
        JsonObject transaction = transactions[0];

        String newTransactionDate = transaction["transaction_date"].as<String>();
        long newAmountIn = transaction["amount_in"].as<String>().toFloat();

        Serial.print("💰 Số tiền nhận được: ");
        Serial.println(newAmountIn);

        // Nếu có giao dịch mới và số tiền > 0
        if (newTransactionDate != transactionDate && newAmountIn > 0) {
          Serial.println("💡 Phát hiện giao dịch mới!");
          displayAmount(newAmountIn);
          
          // Lưu thông tin giao dịch mới
          amountIn = newAmountIn;
          transactionDate = newTransactionDate;
          
          // Chỉ phát âm thanh nếu không phải lần đầu tiên khởi động
          if (!isSpeaking) {
            announceAmount(newAmountIn);
          }
        }
      }
    } else {
      Serial.println("❌ Lỗi khi gửi yêu cầu HTTPS");
    }
    http.end();
  }
}

// 🚀 Khởi tạo hệ thống
void setup() {
  Serial.begin(115200);
  delay(1000);

  // 🌐 Kết nối WiFi
  WiFi.begin(ssid, password);
  Serial.println("🔄 Đang kết nối WiFi...");
  
  // Khởi tạo Wire cho I2C với chân tùy chỉnh
  Wire.begin(LCD_SDA, LCD_SCL);
  
  // Hiển thị thông báo khởi động
  lcd.init();
  lcd.backlight();
  lcd.print("Dang khoi dong...");
  
  // Kết nối WiFi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Đã kết nối WiFi");
  
  // 🔊 Khởi tạo DFPlayer
  dfPlayerSerial.begin(115200);
  delay(1000);
  
  if (!dfPlayer.begin(dfPlayerSerial)) {
    Serial.println("❌ Lỗi DFPlayer!");
    lcd.clear();
    lcd.print("Loi DFPlayer!");
    while (true);
  }
  
  dfPlayer.volume(VOLUME);
  
  // Hiển thị thông tin tài khoản
  displayAccountInfo();
  delay(2000);
  
  lcd.clear();
  lcd.print("San sang!");
  delay(1000);
  
  // Đặt thời gian kiểm tra dữ liệu
  timeGetData = millis();
}

// 🔄 Chạy vòng lặp chính
void loop() {
  // Kiểm tra nhập liệu từ Serial (để test)
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    long amount = input.toInt();
    
    if (amount > 0) {
      Serial.println("📢 Đang thông báo: " + String(amount));
      announceAmount(amount);
    } else {
      Serial.println("❌ Lỗi: Số tiền không hợp lệ!");
      lcd.clear();
      lcd.print("Loi: So tien");
      lcd.setCursor(0, 1);
      lcd.print("khong hop le!");
      delay(2000);
      lcd.clear();
    }
  }
  
  // Kiểm tra giao dịch mới mỗi 6 giây nếu không đang phát âm thanh
  if (!isSpeaking && millis() - timeGetData > 3000) {
    getTransactionData();
    timeGetData = millis();
  }
  
  // Kiểm tra trạng thái phát âm thanh
  if (!dfPlayer.isPlaying() && isSpeaking) {
    isSpeaking = false;
  }
}
