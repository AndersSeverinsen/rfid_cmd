#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "OpenSesame";
const char* password = "12345678";

LiquidCrystal_I2C lcd(0x3F, 16, 2);
#define SS_PIN 5    
#define RST_PIN 27  


#define YES_BUTTON_PIN 15  
#define NO_BUTTON_PIN 13

MFRC522 rfid(SS_PIN, RST_PIN);

const char* server = "http://192.168.27.229:8080";

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  lcd.init();       
  lcd.backlight();
  connectWiFi();

  pinMode(YES_BUTTON_PIN, INPUT_PULLUP);
  pinMode(NO_BUTTON_PIN, INPUT_PULLUP);
}

void loop() {

  displayMessage("To book locker","Place card here");
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {  
    String uid = getUID();
    
    if (WiFi.status() == WL_CONNECTED) {
      String response = sendBookingRequest(uid);
      if (!response.isEmpty()) {
        processBookingResponse(response, uid);
      } else {
        displayError("POST failed");
      }
    } else {
      displayError("WiFi lost!");
    }

    rfid.PICC_HaltA();       
    rfid.PCD_StopCrypto1();  
  }
    delay(1000);
}

// Connect to WiFi
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
}

// Extract UID from RFID card
String getUID() {
  String uid = "";
  for (int i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  Serial.print("UID: ");
  Serial.println(uid);
  return uid;
}

// Send booking request and get response
String sendBookingRequest(String uid) {
  WiFiClient client;
  HTTPClient http;
  String serverName = String(server) + "/book/" + uid;
  http.begin(client, serverName.c_str());
  
  int httpResponseCode = http.POST("");
  String payload = "";
  
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  } else {
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
  }

  http.end();
  return payload;
}

// Process booking response from the server
void processBookingResponse(String response, String uid) {
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    Serial.print("JSON Deserialization failed: ");
    Serial.println(error.f_str());
    displayError("Invalid response");
    return;
  }

  bool existingBooking = doc["existingBooking"];
  bool freeLocker = doc["freeLocker"];
  int lockerNum = doc["locker"];

  if (existingBooking) {
    handleExistingBooking(uid);
  } else if (freeLocker) {
    displayMessage("Locker booked:", "Locker " + String(lockerNum));
  } else {
    displayMessage("No lockers", "available");
  }
}

void handleExistingBooking(String userId) {
  displayMessage("Booking exists.", "Keep or release?");

  // Wait for button press
  bool userChoice = false;
  while (true) {
    if (digitalRead(YES_BUTTON_PIN) == LOW) {  // If yes button pressed
      userChoice = true;
      break;
    } else if (digitalRead(NO_BUTTON_PIN) == LOW) {  // If no button pressed
      userChoice = false;
      break;
    }
    delay(50); // Add small delay for debounce
  }

  displayMessage(userChoice ? "Booking kept." : "Booking released.", "");

  String endpoint = userChoice ? "/keepBooking/" : "/cancelBooking/";
  endpoint += userId;
  
  int responseCode = sendUserChoice(endpoint.c_str());  // Convert to const char*
}


// Send user choice to the server
int sendUserChoice(const char* endpoint) {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, String(server) + endpoint);
  int responseCode = http.POST("");
  http.end();
  return responseCode;
}

// Display error message on LCD
void displayError(String message) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message);
}

// Display message on LCD
void displayMessage(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}
