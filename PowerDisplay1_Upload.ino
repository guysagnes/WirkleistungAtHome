// Author: Guy Sagnes
// date: 07.02.2026

// Aim: collect current power consumption from Tasmota32 
// Basics
//   - information collected over RESTAPI fetchData()
//   - information reported on screen 
//     - current value each 30sec
//     - update curve each 3 min
//     - update current day value in web each 5 min
//   - A sunmmary file per day is generated at midnight
//   Remote files are
//     - url/power/power/today_power.csv (current day)
//     - url/power/power/2026-02-07_power.csv (day with date)
//   In case the value cannot be retrieven a restart is triggered
//
//  Remarks
//  - there are traces over UART
//  - in case of restart the current day will be backup
//  - Display
//    - Information + Duration + last restart time stamp
//    - Max Value in the duration reported range
//    - Current Value followed by the corresponding time stamp
//


#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include "FS.h"
#include <LittleFS.h> // Ersatz für SD.h
#include <time.h> // Für NTP Zeit
#include <WiFiClientSecure.h>

// --- Neue globale Variablen für LittleFS ---
const char* dataPath = "/power_log.csv";
// --- Configuration to be adapted ---
const char* ssid = "SSID_LOCAL";
const char* password = "PASSWORD_LOCAL";
const char* serverUrl = "http://tastoma.url/cm?cmnd=Status%208"; 

// --- Neue globale Variablen ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;    // Deutschland: UTC + 1h
const int   daylightOffset_sec = 3600; // Sommerzeit + 1h
int lastDay = -1; // Speichert den letzten Tag für den Mitternachts-Check
struct tm system_timeinfo;
char strSystemStart[20];
char strLastReport[20];
int lineColor = TFT_BLUE;
bool sysRestart = true;

// Graph settings
#define MAX_POINTS 120    // Number of points on X-axis
// Intervalle
#define FETCH_IST_INTERVAL 30000   // 30 Sekunden für Anzeige
#define GRAPH_UPDATE_COUNT 6       // Alle 6 Abfragen (6 * 30s = 3 Min) in Graph
#define WEB_SEND_INTERVAL 180000 // 3600000  // 1 Stunde

TFT_eSPI tft = TFT_eSPI();

float dataPoints[MAX_POINTS];
uint16_t pixel_x, pixel_y;
float currentP = 0;
int subSampleCount = 0;
unsigned long lastIstFetch = 0;
unsigned long lastWebTransfer = 0;

void setup() {
  uint16_t calibrationData[5];  
  // static values to be replaced by recalibration algorithm
  calibrationData[0] = 478;
  calibrationData[1] = 3388;
  calibrationData[2] = 438;
  calibrationData[3] = 3305;
  calibrationData[4] = 1;

  pinMode(15, OUTPUT);
  digitalWrite(15, LOW);
  Serial.begin(115200);
  Serial.println("Power Control by Guy Sagnes");
  Serial.println("For Az-Touch Mod 2.4-Display");
  randomSeed(analogRead(34));
  tft.init(); 
  tft.setRotation(3); 

  tft.fillScreen((0xFFFF));
  tft.setCursor(40, 20, 2);
  tft.setTextColor(TFT_RED, TFT_WHITE);
  tft.setTextSize(2);
  tft.println("Kalibrierung vom");
  tft.setCursor(40, 60, 2);
  tft.println("Display");
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setCursor(40, 100, 2);
  tft.println("Die angegebene Ecken");
  tft.setCursor(40, 140, 2);
  tft.println("zum kalibrieren");
  tft.setCursor(40, 180, 2);
  tft.println("beruehren");
  // activate the following line to allow manual calibration
  // tft.calibrateTouch(calibrationData, TFT_GREEN, TFT_RED, 15);
  Serial.printf("calibration 0: %u\n", calibrationData[0]);
  Serial.printf("calibration 1: %u\n", calibrationData[1]);
  Serial.printf("calibration 2: %u\n", calibrationData[2]);
  Serial.printf("calibration 3: %u\n", calibrationData[3]);
  Serial.printf("calibration 4: %u\n", calibrationData[4]);
  
  tft.fillScreen(TFT_BLACK);

  //Draw red frame
  drawFrame(5, TFT_RED);

  //Set first text
  tft.setCursor(70, 40);
  tft.setTextColor(TFT_BLUE);
  tft.setTextSize(3);
  tft.print("Wir");
  tft.setTextColor(TFT_GREEN);
  tft.print("kl");
  tft.setTextColor(TFT_WHITE);
  tft.print("ei");
  tft.setTextColor(TFT_GOLD);
  tft.print("stung");
  //Set second text
  tft.setCursor(100, 90);
  tft.setTextColor(TFT_RED);
  tft.setTextSize(2);
  tft.print("07.02.26");
  //Set last line
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(60, 130);
  tft.print("(c)United Disaster");  
  
  Serial.println("Next WIFI");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  
  for(int i=0; i<120; i++) dataPoints[i] = 0; 

  Serial.println("Next Time and date");
// Zeit über NTP konfigurieren
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Warten bis Zeit synchronisiert ist
  // struct tm timeinfo;
  if(!getLocalTime(&system_timeinfo)){
    Serial.println("Konnte Zeit nicht abrufen");
  } else {
    lastDay = system_timeinfo.tm_mday-1;
    // Format: Tag.Monat - Stunde:Minute
    strftime(strSystemStart, sizeof(strSystemStart), "%d.%m - %H:%M", &system_timeinfo);
    Serial.println(strSystemStart);
  }  

  Serial.println("Next step LittleFS");

  if(!LittleFS.begin(false)){ // Zuerst ohne automatisches Formatieren versuchen
      Serial.println("LittleFS Mount fehlgeschlagen, starte Formatierung...");
      if(LittleFS.format()){
          Serial.println("Formatierung erfolgreich.");
          if(LittleFS.begin()){
              Serial.println("LittleFS nach Formatierung bereit.");
          }
      } else {
          Serial.println("LittleFS Formatierung fehlgeschlagen!");
      }
  } else {
      Serial.println("LittleFS bereit.");
  }

}


void loop() {
  unsigned long now = millis();
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) 
  {
    Serial.println("getLocalTime Failed");
  }

  // 1. Alle 30 Sekunden IST-Wert abrufen
  if (now - lastIstFetch >= FETCH_IST_INTERVAL || lastIstFetch == 0) {
    fetchData(); 
    subSampleCount++;

    // 2. Alle 3 Minuten (nach 6 Samples) Graph aktualisieren
    if (subSampleCount >= GRAPH_UPDATE_COUNT) {
      updateGraphArray(currentP);
      subSampleCount = 0;
    }
    
    saveToCSV(currentP);
    drawUI(); 
    lastIstFetch = now;

    // 3. Einmal pro Stunde per WEB senden
    if (now - lastWebTransfer >= WEB_SEND_INTERVAL) {
      sendToWeb("today_power.csv");
      lastWebTransfer = now;
    }
  }
  // --- MITTERNACHTS-LOGIK ---
  if (timeinfo.tm_mday != lastDay) {
    Serial.println("Tageswechsel erkannt. Starte Datei-Archivierung...");

    // Dateiname generieren: YYYY-MM-DD_power.csv
    char fileName[30];
    strftime(fileName, sizeof(fileName), "%Y-%m-%d_power.csv", &timeinfo);
    
    // Aktuelle Datei unter dem Datums-Namen hochladen
    sendToWeb(fileName);
    
    // Lokale Datei löschen, um neu für den Tag zu starten
    if (sysRestart) {
      sysRestart = false;
    }
    else LittleFS.remove(dataPath);    
    lastDay = timeinfo.tm_mday;
  }
}

void fetchData() {
  struct tm timeinfo;
  HTTPClient http;
  http.begin(serverUrl);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      // Accessing the nested JSON path: StatusSNS -> MT175 -> P
      currentP = doc["StatusSNS"]["MT175"]["P"];      
      Serial.printf("Current Power: %.2f W\n", currentP);
      if(getLocalTime(&timeinfo))
      {
        strftime(strLastReport, sizeof(strSystemStart), "%d.%m - %H:%M:%S", &timeinfo);
        Serial.println(strLastReport);
      }
    } 
    else esp_restart();
  }
  else esp_restart();
  http.end();
}

void updateGraphArray(float val) {
  Serial.printf("Update Array: %.2f W\n", val); 
  for (int i = 0; i < MAX_POINTS - 1; i++) dataPoints[i] = dataPoints[i + 1]; 
  dataPoints[MAX_POINTS - 1] = val; 
}

// Speichern im internen Flash
void saveToCSV(float val) {
  struct tm timeinfo;
  Serial.printf("saveToCSV: %.2f W\n", val); 

  if(!getLocalTime(&timeinfo)) return;

  // FILE_APPEND fügt Daten am Ende hinzu
  fs::File file = LittleFS.open(dataPath, FILE_APPEND);
  if(file) {
    char timeStr[10];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    file.printf("%s,%.2f\n", timeStr, val);
    file.close();
    Serial.printf("saveToCSV success\n"); 
  }
}

void drawUI() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  float Dauer = (FETCH_IST_INTERVAL* 120.0)*GRAPH_UPDATE_COUNT/3600000 ;
  tft.setTextSize(1);
  tft.drawString("Power Usage - Hours:", 10, 5);
  tft.drawFloat(Dauer, 1, tft.width()/2, 5); // duration
  tft.drawString(strSystemStart, 3*tft.height()/4 + 15, 5);
  tft.drawString(strLastReport, 3*tft.height()/4 + 15, 45);

  tft.drawLine(tft.width()/2, tft.height()/2, tft.width()/2, tft.height(), lineColor);
  tft.drawLine(tft.width()/4, 3*tft.height()/4, tft.width()/4, tft.height(), lineColor);
  tft.drawLine(3*tft.width()/4, 3*tft.height()/4, 3*tft.width()/4, tft.height(), lineColor);

  // Simple Auto-scaling
  float maxVal = 100; // Minimum scale ceiling
  for(int i=0; i<MAX_POINTS; i++) if(dataPoints[i] > maxVal) maxVal = dataPoints[i];

  // Draw Graph Lines
  for (int i = 0; i < MAX_POINTS - 1; i++) {
    int x1 = map(i, 0, MAX_POINTS - 1, 10, tft.width() - 10);
    int x2 = map(i + 1, 0, MAX_POINTS - 1, 10, tft.width() - 10);
    
    // Y-axis is inverted in TFT coordinates (0 is top)
    int y1 = map(dataPoints[i], 0, maxVal, tft.height() - 20, 40);
    int y2 = map(dataPoints[i+1], 0, maxVal, tft.height() - 20, 40);

    tft.drawLine(x1, y1, x2, y2, TFT_GREEN);
  }
  
  tft.drawNumber((int)maxVal, 10, 25); // Show max scale
  tft.drawString("W (max)", 50, 25);
  tft.drawNumber(currentP, 10, 45); // Show Last
  tft.drawString("W (ist)", 50, 45);
}

/*
* =================================================================
* Function:     drawFrame   
* Returns:      void
* INPUT iSize:  Size of the frame
* INPUT color:  Color of the frame
* Description:  Draw frame with given size and color
* =================================================================
*/
void drawFrame(int iSize, uint16_t color)
{
  int iCnt;
  for (iCnt = 0; iCnt <= iSize; iCnt++)
    tft.drawRect(0 + iCnt, 0 + iCnt, 320 - (iCnt * 2), 240 - (iCnt * 2), color);
}


void sendToWeb(const char* remoteName) {
    WiFiClientSecure client;
    Serial.printf("SendToWeb: %s\n", remoteName);     
    client.setInsecure(); // Überspringt die SSL-Zertifikatsprüfung für Strato

    HTTPClient http;
    // Ersetzen Sie dies durch Ihre tatsächliche URL zum PHP-Script
    String url = "https://united4u.eu/power/upload.php"; 

    if (http.begin(client, url)) {
        fs::File file = LittleFS.open("/power_log.csv", FILE_READ);
        if (file) {
            String boundary = "---------------------------123456789";
            http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

            // Passwort und Metadaten für das PHP-Formular
            String head = "--" + boundary + "\r\n" +
                          "Content-Disposition: form-data; name=\"key\"\r\n\r\n" + "United4U@Strato!" + "\r\n" +
                          "--" + boundary + "\r\n" +
                          "Content-Disposition: form-data; name=\"fileToUpload\"; filename=\"" + String(remoteName) + "\"\r\n" +
                          "Content-Type: text/csv\r\n\r\n";
            String tail = "\r\n--" + boundary + "--\r\n";

            // Wir berechnen die Gesamtlänge für den HTTP-Header
            size_t totalLen = head.length() + file.size() + tail.length();
            
            // Den gesamten Inhalt senden
            // Hinweis: Bei Dateien > 32KB empfiehlt sich http.writeToStream, 
            // für Tages-Logs (ca. 70KB) bauen wir es hier einfach zusammen:
            uint8_t *buffer = (uint8_t *)malloc(file.size());
            if (buffer) {
                file.read(buffer, file.size());
                int response = http.POST(head + String((char*)buffer) + tail);
                
                if (response > 0) {
                    Serial.printf("Server Antwort: %s\n", http.getString().c_str());
                } else {
                    Serial.printf("HTTP Fehler: %s\n", http.errorToString(response).c_str());
                }
                free(buffer);
            }
            file.close();
            http.end();
        }
        else {
          Serial.println("Fehler: power_log.csv nicht gefunden.");
          lineColor = TFT_RED;
        }
    }
}
