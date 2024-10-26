#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>

#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "OpenSesame";
const char* password = "12345678";

// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;

// set LCD address, number of columns and rows
// if you don't know your display address, run an I2C scanner sketch
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  

#define SS_PIN 5    // ESP32 pin GPIO5
#define RST_PIN 27  // ESP32 pin GPIO27

MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  SPI.begin();      // init SPI bus
  rfid.PCD_Init();  // init MFRC522
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
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Tap an RFID/NFC tag on the RFID-RC522 reader");
}

void loop() {
  if (rfid.PICC_IsNewCardPresent()) {  // new tag is available
    if (rfid.PICC_ReadCardSerial()) {  // NUID has been read
      MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
      Serial.print("RFID/NFC Tag Type: ");
      Serial.println(rfid.PICC_GetTypeName(piccType));

      // Initialize an empty string for UID
      String uid = "";
      Serial.print("UID: ");
      
      // Append each byte of UID to the string and print to Serial Monitor
      for (int i = 0; i < rfid.uid.size; i++) {
        uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
        uid += String(rfid.uid.uidByte[i], HEX);
      }

      Serial.println(uid);
      
      if(WiFi.status() == WL_CONNECTED){
        WiFiClient client;
        HTTPClient http;

        // Create server URL dynamically with the UID
        String endpoint = "http://192.168.27.229:8080/book/" + uid;

        Serial.print("Sending POST request to: " + endpoint);
        
        
        // Begin the HTTP POST request
        http.begin(client, endpoint.c_str());
        int httpResponseCode = http.POST("");

        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);

        // Free resources
        http.end();
      } else {
        Serial.println("WiFi Disconnected");
      }

      // Display UID on LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("UID:");
      lcd.setCursor(0, 1);
      lcd.print(uid);

      rfid.PICC_HaltA();       // halt PICC
      rfid.PCD_StopCrypto1();  // stop encryption on PCD
    }
  }
}
