# Power Display for ESP32

Eine umfassende Lösung zur Erfassung und Visualisierung von Echtzeitstromverbrauchsdaten von Tasmota32-Geräten mit Display-Anzeige und Web-Integration.

## Überblick

Dieses Projekt monitoriert die Leistungsaufnahme eines Tasmota32-Geräts und präsentiert die Daten auf einem 2.4"-TFT-Display des Az-Touch-Moduls. Die Software erfasst Verbrauchswerte über die REST-API der Tasmota-Installation, visualisiert historische Daten mittels Grafik und speichert tägliche Verbrauchsmuster lokal.

## Features

- **Echtzeit-Leistungsanzeige**: Aktuellen Stromverbrauch alle 30 Sekunden
- **Grafische Darstellung**: Aktualisiert alle 3 Minuten mit einem gleitenden Fenster von bis zu 120 Datenpunkten
- **Tägliche Datenarchivierung**: Automatische Zusammenfassung der Verbrauchsdaten pro Tag um Mitternacht
- **Web-Integration**: Hochladen der aktuellen Tagesdaten alle 5 Minuten
- **Fehlerbehandlung**: Automatischer Neustart bei nicht verfügbarer REST-API
- **Persistente Speicherung**: CSV-Dateien im LittleFS-Dateisystem
- **Anzeige-Details**:
  - Aktuelle Leistung mit Zeitstempel
  - Maximalwert im angezeigten Zeitraum
  - Systemstart- und letzte Berichtszeit
  - Betriebsdauer

## Hardware-Anforderungen

- **Microcontroller**: ESP32
- **Display**: Az-Touch Mod 2.4" TFT (2.4" Touchscreen Display mit ILI9341 Treiber)
- **Verbindung**: WiFi-fähiger Tasmota32 Stromverbrauchssensor

## Software-Anforderungen

### Abhängigkeiten

- `WiFi.h` - WiFi-Konnektivität
- `HTTPClient.h` - REST-API-Zugriff
- `ArduinoJson.h` - JSON-Verarbeitung
- `TFT_eSPI.h` - Display-Steuerung
- `LittleFS.h` - Dateisystemzugriff (SD-Karten-Alternative)
- `time.h` - NTP-Zeitsynchronisation

### Arduino IDE Setup

1. Installiere die Arduino ESP32-Board-Definition
2. Installiere die erforderlichen Bibliotheken über Library Manager:
   - TFT_eSPI
   - ArduinoJson
3. Konfiguriere die Pin-Definitionen für dein Display

## Konfiguration

Im `PowerDisplay1_Upload.ino`-Sketch müssen folgende Parameter angepasst werden:

```cpp
// WiFi-Verbindung
const char* ssid = "SSID_LOCAL";
const char* password = "PASSWORD_LOCAL";

// Tasmota32 REST-API URL
const char* serverUrl = "http://tastoma.url/cm?cmnd=Status%208";

// NTP-Server (Standard: pool.ntp.org)
const char* ntpServer = "pool.ntp.org";

// Zeitzone (Standard: Deutschland UTC+1)
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
```

## Betriebsprinzipien

### Datenfluss

1. **REST-API-Abfrage** (30s Intervall):
   - Aktuelle Leistungswerte vom Tasmota32-Gerät abrufen
   - Wert auf Display anzeigen
   - Rohdaten sammeln

2. **Grafik-Update** (3 Minuten = 6 × 30s Abfragen):
   - Gleitender Durchschnitt berechnen
   - Grafik mit bis zu 120 Punkten aktualisieren
   - Max-Wert im Zeitraum ermitteln

3. **Web-Transfer** (5 Minuten):
   - Aktuelle Tageswerte an Remote-Server hochladen
   - Bei Fehler: kein Neustart (nur Log)

4. **Tägliche Archivierung** (Mitternacht):
   - Lokale CSV-Datei mit Tageswerten erstellen
   - Format: `YYYY-MM-DD_power.csv`
   - Backup der aktuellen Tageswerte

### Speicherte Remote-Dateien

- `url/power/power/today_power.csv` - Aktuelle Tageswerte
- `url/power/power/YYYY-MM-DD_power.csv` - Historische Tageswerte

## Display-Ausgabe

Das 2.4"-Display zeigt folgende Informationen:

- **Aktuelle Leistung** mit Zeitstempel
- **Maximale Leistung** im angezeigten Zeitraum
- **Betriebsdauer** seit letztem Start
- **Letzte Berichtszeit**
- **Grafische Darstellung** des Verbrauchsverlaufs
- **Systemstatus** (Farbanzeige: Blau = Normal, Rot = Fehler)

## Fehlerbehandlung

- **API nicht erreichbar**: Nach mehrmaligem Fehlschlag wird der ESP32 neu gestartet
- **Zeitproblem**: NTP-Synchronisation versucht alle 24 Stunden
- **Speicherproblem**: Alte Dateien können manuell gelöscht werden

## Dateistruktur

```
ESP_Wirkleistung/
├── PowerDisplay1_Upload.ino    # Hauptprogramm
├── README.md                   # Diese Datei
└── www/                        # (Optional) Web-Interface-Dateien
```

## Bemerkungen

- **Debug-Output**: Serial Monitor auf 115200 Baud zeigt Trace-Informationen
- **Startup-Backup**: Bei Neustart wird der aktuelle Tageswert gesichert
- **NTP-Synchronisation**: Deutschland nutzt UTC+1 (Standard) und Sommerzeit UTC+2
- **Display-Kalibrierung**: Beim ersten Start wird eine Touch-Kalibrierung durchgeführt

## Autor

Guy Sagnes - Datum: 07.02.2026

## Lizenz

Siehe Projektrepository für Lizenzinformationen.

## Troubleshooting

### Display zeigt nichts
- Überprüfe die Pin-Konfiguration für das Az-Touch Mod
- Stelle sicher, dass TFT_eSPI korrekt für dein Display konfiguriert ist

### Keine Datenempfänge von Tasmota
- Überprüfe WiFi-Verbindung
- Verifiziere die REST-API-URL und das Tasmota-Format
- Prüfe Firewall- und Sicherheitseinstellungen

### Speicher voll
- Überprüfe LittleFS-Belegung via Serial Monitor
- Lösche alte CSV-Dateien manuell
- Erhöhe ggf. die Partition-Größe in der Board-Konfiguration

## Support

Bei Fragen oder Problemen bitte Issue im Repository erstellen.
