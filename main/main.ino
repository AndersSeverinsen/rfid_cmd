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
#define BUZZER_PIN 2
#define YES_BUTTON_PIN 15
#define NO_BUTTON_PIN 13

MFRC522 rfid(SS_PIN, RST_PIN);

const char* server = "http://192.168.27.229:8080";

unsigned long lastActivityTime = 0;
const unsigned long timeoutPeriod = 10000;

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  lcd.init();
  lcd.backlight();
  connectWiFi();

  pinMode(YES_BUTTON_PIN, INPUT_PULLUP);
  pinMode(NO_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  lastActivityTime = millis(); // Initialize last activity time
}

void loop() {

  if (millis() - lastActivityTime >= timeoutPeriod) {
    lcd.noBacklight();  // Dim the LCD after 10 seconds of inactivity
  }
  
  displayMessage("To book a locker", "Place card here");
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    lastActivityTime = millis();  // Reset last activity time
    lcd.backlight();  // Restore LCD backlight
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);

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
  if (digitalRead(YES_BUTTON_PIN) == LOW || digitalRead(NO_BUTTON_PIN) == LOW) {
    lastActivityTime = millis();  // Reset last activity time when a button is pressed
    lcd.backlight();  // Restore LCD backlight
  }
  delay(1000);
}

// Connect to WiFi
void connectWiFi() {
  WiFi.begin(ssid, password);
  
  Serial.println("Connecting to WiFi");
  displayMessage("Connecting to", "WiFi: " + String(ssid));
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  displayMessage("\nConnected", "to WiFi!");
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

// Send booking request and get response with retries
String sendBookingRequest(String uid) {
  WiFiClient client;
  HTTPClient http;
  String serverName = String(server) + "/book/" + uid;
  int maxRetries = 3;  // Number of retries
  int retryDelay = 2000;  // Delay between retries in milliseconds

  String payload = "";
  for (int attempt = 0; attempt < maxRetries; ++attempt) {
    http.begin(client, serverName.c_str());
    http.setTimeout(5000);  // Set 5-second timeout for HTTP connection

    int httpResponseCode = http.POST("");
    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      payload = http.getString();
      http.end();
      return payload;  // Exit loop on success
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }
    http.end();
    delay(retryDelay);  // Wait before retrying
  }
  return "";  // Return empty if all attempts fail
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
    delay(50);  // Add small delay for debounce
  }

  displayMessage(userChoice ? "Booking kept." : "Booking released.", "");

  String endpoint = userChoice ? "/keepBooking/" : "/cancelBooking/";
  endpoint += userId;

  int responseCode = sendUserChoice(endpoint.c_str());  // Convert to const char*
}



// Send user choice to the server with retries
int sendUserChoice(const char* endpoint) {
  WiFiClient client;
  HTTPClient http;
  String serverName = String(server) + endpoint;
  int maxRetries = 3;  // Number of retries
  int retryDelay = 2000;  // Delay between retries in milliseconds
  int responseCode = -1;

  for (int attempt = 0; attempt < maxRetries; ++attempt) {
    http.begin(client, serverName);
    http.setTimeout(5000);  // Set 5-second timeout for HTTP connection

    responseCode = http.POST("");
    if (responseCode > 0) {
      http.end();
      return responseCode;  // Exit loop on success
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(responseCode);
    }
    http.end();
    delay(retryDelay);  // Wait before retrying
  }
  return responseCode;  // Return last response code if all attempts fail
}

// Display error message on LCD
void displayError(String message) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Error:");
  lcd.setCursor(0, 1);
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
