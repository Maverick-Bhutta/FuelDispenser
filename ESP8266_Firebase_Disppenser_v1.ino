//Firebase Dispenser Ver 1 gets frame of  raw string from Dispenser on serial, decode string and creat firbaseJSON object and send to cloud 
//Here WIFI8266 moduel is connected to internet router and received serial Data from Dispensor and send to my cloud server Firebase
//WIFI is not webserer her, Simple to use firebase realtime database for display /control fron anyware your sensors / things over internet (IOT)
// Backend is google firebase Realime Databas and front end is web page hosted in my gethub account
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>    // Firebase ESP Client (recommended)
#include <EEPROM.h>
/* 1. Define the WiFi credentials */
#define API_KEY "*******************"
#define DATABASE_URL "*********************************" 
#define USER_EMAIL "***********"
#define USER_PASSWORD "************"

#define UNITID_ADDR 0
#define COMPANY_ADDR 4
#define TOTAL_ADDR 16
#define SSID_ADDR 24
#define PASSWD_ADDR 36

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseJson json;
// Timing and state
unsigned long sendDataPrevMillis = 0;
const unsigned long UPDATE_INTERVAL = 1000; // ms

// WiFi credentials read from Serial in setup (single-line "SSID,PASSWORD")
String wifiSSID = "";
String wifiPASS = "";
int Mode;
String MachineID= "1"; //Ensure Unique ID No of each Unit
String VendorName ="Seven_Star";
String sValue;
// Parsed values
float fPrice = 0.0;
float fQty   = 0.0;
float fRate  = 0.0;
float fTotalM  = 0.0;

void setup() {
  Serial.begin(9600);
  if(downloadSetting()){ // get save data first and check if SSID and Password ok
    connectToWiFi(wifiSSID, wifiPASS);
  if (WiFi.status() == WL_CONNECTED) 
    connectFirbase();
  else 
    Serial.println(F("WIFI not connected, Saved credentials are nort correct:"));
  } 
  else{
    getWIFICredential();
    getMachineID();
  }
}

void loop() {
  // Read serial when available
  if (Serial.available()) {
    getSerialdata();
  }
  // Only attempt Firebase update when ready and interval elapsed
  if (Firebase.ready() && (millis() - sendDataPrevMillis > UPDATE_INTERVAL || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    sendJsonToFirebase();
  }
  // Optional: small yield to keep WiFi stack happy
  yield();
}

void getSerialdata() {
  // Read one line (until newline)
  sValue = Serial.readStringUntil('\n');
  sValue.trim(); // remove whitespace and CR/LF
  // expected format
  // <!00-00000048-46000000-1912537-0439820!>
  // or "<!Mode-price-fuel-rate-Total!>" depending on your device

  if (sValue.length() == 0) return;

  if (sValue.startsWith("<") && sValue.endsWith(">")) {
    sValue = sValue.substring(2, sValue.length() - 2);
  }
  // If your payload uses '-' as separators, split by '-'
  // We'll split into tokens by '-' and also handle comma or space separators
  String tokens[8];
  int tokenCount = 0;
  int start = 0;
  for (int i = 0; i <= sValue.length(); ++i) {
    if (i == sValue.length() || sValue.charAt(i) == '-' || sValue.charAt(i) == ',' || sValue.charAt(i) == ' ') {
      if (i - start > 0 && tokenCount < 8) {
        tokens[tokenCount++] = sValue.substring(start, i);
      }
      start = i + 1;
    }
  }

  if (tokenCount >= 5) {
    // tokens[0] -> price, tokens[1] -> qty/fuel, tokens[2] -> rate
    Mode = safeToFloat(tokens[0]);
    fPrice = safeToFloat(tokens[1]);
    fQty   = safeToFloat(tokens[2]);
    fRate  = safeToFloat(tokens[3]);
    fTotalM = safeToFloat(tokens[4]);
  } 
  else {
    // Could not parse; keep previous values and log
    Serial.print("Unrecognized serial format: ");
    Serial.println(sValue);
    return;
  }
  //Debug print parsed values
  Serial.print("Parsed -> price: ");
  Serial.print(fPrice, 5);
  Serial.print("  qty: ");
  Serial.print(fQty, 5);
  Serial.print("  rate: ");
  Serial.println(fRate, 5);
  Serial.print("  TotalM: ");
  Serial.println(fTotalM, 5);
}

float safeToFloat(const String &s) {
  String t = s;
  t.trim();
  // Remove any non-numeric characters except '.' and '-'
  String clean = "";
  for (unsigned int i = 0; i < t.length(); ++i) {
    char c = t.charAt(i);
    if ((c >= '0' && c <= '9') || c == '.') clean += c;
  }
  if (clean.length() == 0) return 0.0;
  return clean.toFloat();
}

void sendJsonToFirebase() {
  // Build JSON object
  json.clear();
  json.set("price", fPrice);
  json.set("qty", fQty);
  json.set("rate", fRate);
  json.set("totalM", fTotalM);

  // Add timestamp (epoch seconds) and sequence counter
  // uint32_t ts = (uint32_t)(millis() / 1000); // local uptime seconds; replace with RTC if available
  // json.set("timestamp", ts);
  // json.set("seq", ++seqCounter);
  // // Optionally add raw string for debugging
  // json.set("raw", sValue);
  String DatabaseAdd = "/Dispenser/" +Company +"/" + UnitID;
  // Send JSON atomically to /Dispenser
  if (Firebase.RTDB.setJSONAsync(&fbdo, DatabaseAdd, &json)) {
    // Async call queued; you can check fbdo for result later if needed
    Serial.print("JSON update queued");
  } else {
    // If immediate failure, print reason
    Serial.print("setJSONAsync failed: ");
    Serial.println(fbdo.errorReason());
  }

  // Optionally: check last operation result (non-blocking)
  // If you prefer blocking write, use setJSON(&fbdo, "/Dispenser", &json) and check return.
}
/* ---------- WiFi connect helper ---------- */
void connectFirbase(){
  Serial.println(F("Initializing Cloud Database website..."));
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;

  fbdo.setBSSLBufferSize(4096, 4096);
  fbdo.setResponseSize(2048);

  Firebase.reconnectNetwork(true);
  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(5);

  config.timeout.serverResponse = 10 * 1000;
  Serial.println(F("Firebase initialized."));
}
void getWIFICredential(){
  Serial.println();
  Serial.println(F("=== Enter WiFi credentials : SSID,PASSWORD ==="));
  Serial.println(F("Type the line now and press Enter. Waiting up to 30 seconds..."));
  // Read a single line from Serial with a timeout
  unsigned long start = millis();
  const unsigned long credentialTimeout = 30000; // 30 seconds
  String line = "";
  while ((millis() - start) < credentialTimeout) {
    if (Serial.available()) {
      line = Serial.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) break;
    }
    delay(10);
  }
  if (line.length() == 0) {
    Serial.println(F("No credentials received. running without WiFi."));
  } 
  else {
    // Expect format: SSID,PASSWORD
    int commaIndex = line.indexOf(',');
    if (commaIndex <= 0) {
      Serial.println(F("Invalid format. Expected: SSID,PASSWORD"));
    } 
    else {
      wifiSSID = line.substring(0, commaIndex);
      wifiPASS = line.substring(commaIndex + 1);
      wifiSSID.trim();
      wifiPASS.trim();
      Serial.print(F("Received SSID: "));
      Serial.println(wifiSSID);
      Serial.println("Try to connecting WIFI......");
      connectToWiFi(wifiSSID, wifiPASS);
      if(WiFi.status() == WL_CONNECTED) {
        Serial.println("connecting to Firebase cloud server......");
        uploadSetting();
        connectFirbase();
      }   
      else 
        Serial.println(F("WIFI not connected, Saved credentials are nort correct:"));
    }
  }
}
void getMachineID(){
  Serial.println();
  Serial.println(F("=== Enter Machine Details : CompanyName,MachineNo(1-9) ==="));
  Serial.println(F("Type the line now and press Enter. Waiting up to 30 seconds..."));
  // Read a single line from Serial with a timeout
  unsigned long start = millis();
  const unsigned long credentialTimeout = 30000; // 30 seconds
  String line = "";
  while ((millis() - start) < credentialTimeout) {
    if (Serial.available()) {
      line = Serial.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) break;
    }
    delay(10);
  }

  if (line.length() == 0) {
    Serial.println(F("No Company/Machine Detail received. Must Provide!!"));
  } else {
    // Expect format: SSID,PASSWORD
    int commaIndex = line.indexOf(',');
    if (commaIndex <= 0) {
      Serial.println(F("Invalid format. Expected: SSID,PASSWORD"));
    } else {
      Vendor = line.substring(0, commaIndex);
      MachineID = line.substring(commaIndex + 1);
      Vendor.trim();
      MachineID.trim();
    }
  }
}
void connectToWiFi(const String &ssid, const String &pass) {
  Serial.print(F("Connecting to WiFi: "));
  Serial.println(ssid);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long start = millis();
  const unsigned long timeout = 20000; // 20 seconds
  Serial.print(F("Connecting"));
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout) {
    Serial.print(F("."));
    delay(500);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("WiFi connected."));
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("Failed to connect to WiFi (timeout)."));
  }
}
void uploadSetting(){
  EEPROM.put(UNITID_ADDR, MachineID);
  EEPROM.put(COMPANY_ADDR, Vendor);
  EEPROM.put(SSID_ADDR, wifiSSID);
  EEPROM.put(PASSWD_ADDR, wifiPASS);
  EEPROM.put(TOTAL_ADDR, fTotalM);
  delay(200);
}
bool downloadSetting(){
  // Get system setting from eeprom
  EEPROM.put(UNITID_ADDR, MachineID);
  EEPROM.put(COMPANY_ADDR, Vendor);
  EEPROM.put(SSID_ADDR, wifiSSID);
  EEPROM.put(PASSWD_ADDR, wifiPASS);
  EEPROM.put(TOTAL_ADDR, fTotalM);
  delay(50);
  if(wifiSSID.length() > 1) 
    return true;
  else 
    return false;
}