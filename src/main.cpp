#include <SPI.h>
#include <MFRC522.h>
#include <Arduino.h>
#include <liquidCrystal_I2C.h>
#define RST_PIN         9  // RC522 RST pini
#define SS_PIN          10 // RC522 SDA (SS) pini
//v1.0.0

MFRC522 mfrc522(SS_PIN, RST_PIN); // RC522 nesnesi oluştur
LiquidCrystal_I2C lcd(0x27,20,4);
void readUID();
void readSector12();
void writeSector12(String data);
void updateLCD(const char* incoming, const char* outgoing);

bool lastSerialState = false;  // Seri port durumunu takip etmek için

void checkSerialConnection() {
    bool currentSerialState = Serial;
    if (currentSerialState != lastSerialState) {
        if (currentSerialState) {
            updateLCD("Port", "ACILDI");
            Serial.println("Port Acildi");
        } else {
            updateLCD("Port", "KAPANDI");
            // Port kapandığında belleği temizle
            while(Serial.available()) { Serial.read(); }
            mfrc522.PCD_Init();
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Port Kapandi!");
            delay(2000);
            // Ana ekrana dön
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("RFID Sistem Hazir");
            lcd.setCursor(0,1);
            lcd.print("-----------------");
            lcd.setCursor(0,2);
            lcd.print("Gelen:");
            lcd.setCursor(0,3);
            lcd.print("Giden:");
        }
        lastSerialState = currentSerialState;
    }
}

void setup() {
    Serial.begin(9600); // Seri haberleşmeyi başlat
    while (!Serial) { ; }  // Seri port hazır olana kadar bekle
    Serial.setTimeout(50); // Seri port timeout süresini azalt
    SPI.begin();        // SPI haberleşmesini başlat
    mfrc522.PCD_Init(); // RC522'yi başlat
    Serial.println("RC522 hazır.");
  
    lcd.init();
    lcd.backlight();
    lcd.clear();
    // LCD başlangıç ekranı
    lcd.setCursor(0,0);
    lcd.print("RFID Sistem Hazir");
    lcd.setCursor(0,1);
    lcd.print("-----------------");
    lcd.setCursor(0,2);
    lcd.print("Gelen:");
    lcd.setCursor(0,3);
    lcd.print("Giden:");
}

void loop() {
    checkSerialConnection();  // Her döngüde port durumunu kontrol et
    
    if (Serial.available() > 0) {
        String receivedData = Serial.readStringUntil('\n'); // Seri porttan gelen veriyi oku
        receivedData.trim();
        
        // Seri port tamponunu temizle
        while(Serial.available()) {
            Serial.read();
        }

        if (receivedData == "R") {
            updateLCD("ID", "..");
            readUID(); // Kartın UID'sini oku
            
        } else if (receivedData == "D") {
            updateLCD("OKU", "--");
            readSector12(); // Sektör 12'yi oku
            
        } else if (receivedData.startsWith("W")) {
            updateLCD("..", "YAZ");
            String dataToWrite = receivedData.substring(1);
            writeSector12(dataToWrite); // Sektör 12'ye veri yaz
            
        }
    }
    
    // Her döngüde kartı resetle
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(10); // Kısa bekleme ekle
}

void readUID() {
    // Kart işlemleri öncesi reset
    mfrc522.PCD_Init();
    delay(50);
    
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        updateLCD("Durum", "Kart Yok!");
        Serial.println("Kart algılanamadı.");
        return;
    }
    updateLCD("Durum", "Kart Okundu");
    Serial.print("UID:");
    String uidString = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
        Serial.print(mfrc522.uid.uidByte[i], HEX);
        uidString += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "") + String(mfrc522.uid.uidByte[i], HEX);
    }
    Serial.println();
    updateLCD("Kart ID", uidString.c_str());
    mfrc522.PICC_HaltA();
    
    Serial.flush();  // Yanıt gönderilmeden önce
    delay(50);      // Yanıt için kısa bekleme
}

void readSector12() {
    // Kart işlemleri öncesi reset
    mfrc522.PCD_Init();
    delay(50);
    
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        updateLCD("Durum", "Kart Yok!");
        Serial.println("Kart algılanamadı.");
        return;
    }
    updateLCD("Durum", "Kart Hazir");
    Serial.println("Kart algılandı.");

    MFRC522::MIFARE_Key key;
    for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF; // Varsayılan anahtar

    byte block = 12 * 4; // Blok numarasını hesapla
    byte buffer[18];
    byte size = sizeof(buffer);

    if (mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid)) != MFRC522::STATUS_OK) {
        Serial.println("Auth Hatası");
        return;
    }
    if (mfrc522.MIFARE_Read(block, buffer, &size) != MFRC522::STATUS_OK) {
        Serial.println("Okuma Hatası");
        updateLCD("Hata!", "Okuma Hatasi");
        return;
    }
    Serial.print("Sector12Data:");
    for (byte i = 0; i < 16; i++) {
        Serial.print((char)buffer[i]); // Veriyi string olarak yazdır
    }
    Serial.println();
    updateLCD("Veri Okundu", (char*)buffer);
    
    Serial.flush();
    delay(50);
}

void writeSector12(String data) {
    // Kart işlemleri öncesi reset
    mfrc522.PCD_Init();
    delay(50);
    
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        updateLCD("Durum", "Kart Yok!");
        Serial.println("Kart algılanamadı.");
        return;
    }
    updateLCD("Durum", "Kart Hazir");
    Serial.println("Kart algılandı.");

    MFRC522::MIFARE_Key key;
    for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF; // Varsayılan anahtar

    byte block = 12 * 4;
    byte buffer[16] = {0};
    data.getBytes(buffer, 16);

    if (mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid)) != MFRC522::STATUS_OK) {
        Serial.println("Auth Hatası");
        updateLCD("Hata!", "Yazma Hatasi");
        return;
    }
    if (mfrc522.MIFARE_Write(block, buffer, 16) != MFRC522::STATUS_OK) {
        Serial.println("Yazma Hatası");
        updateLCD("Hata!", "Yazma Hatasi");
        return;
    }
    Serial.println("Veri yazıldı");
    updateLCD("Yazildi", "OK");
    
    Serial.flush();
    delay(50);
}

void updateLCD(const char* incoming, const char* outgoing) {
    // 3. satırı güncelle
    lcd.setCursor(7,2);
    lcd.print("             "); // Temizle
    lcd.setCursor(7,2);
    lcd.print(incoming);
    
    // 4. satırı güncelle
    lcd.setCursor(7,3);
    lcd.print("             "); // Temizle
    lcd.setCursor(7,3);
    lcd.print(outgoing);
}
