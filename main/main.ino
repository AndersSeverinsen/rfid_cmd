#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "OpenSesame";
const char* password = "12345678";

LiquidCrystal_I2C lcd(0x27, 16, 2);
#define SS_PIN 5    
#define RST_PIN 27  

MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  lcd.init();       
  lcd.backlight();
  lcd.setCursor(3,0);
  lcd.print("Hello, world!");
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
}

void loop() {
  if (rfid.PICC_IsNewCardPresent()) {  
    if (rfid.PICC_ReadCardSerial()) {  
      String uid = "";
      for (int i = 0; i < rfid.uid.size; i++) {
        uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
        uid += String(rfid.uid.uidByte[i], HEX);
      }
      
      if(WiFi.status() == WL_CONNECTED){
        WiFiClient client;
        HTTPClient http;
        String serverName = "http://192.168.27.229:8080/book/" + uid;
        http.begin(client, serverName.c_str());
        
        int httpResponseCode = http.POST("");
        
        if (httpResponseCode > 0) {
          Serial.print("HTTP Response code: ");
          Serial.println(httpResponseCode);

          String payload = http.getString();
          Serial.println("Response payload: ");
          Serial.println(payload);

          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Server says:");
          lcd.setCursor(0, 1);
          lcd.print(payload); 
          
        } else {
          Serial.print("Error on sending POST: ");
          Serial.println(httpResponseCode);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("POST failed");
        }

        http.end();
      } else {
        Serial.println("WiFi Disconnected");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WiFi lost!");
      }

      rfid.PICC_HaltA();       
      rfid.PCD_StopCrypto1();  
    }
  }
}
