# Hardware: VCC-GND Studio YD-ESP32-S3 (2022-V1.4)

**Stand:** 2026-10-19 · **Gerät:** hu-probe-Messsonde (Feld-ESP, BMW NBT Evo)

Belegklassen in diesem Dokument:

- **[Foto]** am eigenen Board abgelesen (Fotos vom 19.10.2026)
- **[Code]** aus Firmware/Build dieses Repos
- **[Quelle]** Hersteller-/Community-Angabe (VCC-GND GitHub, Mischianti, Espressif); nicht am eigenen Exemplar geprüft
- **[Grok]** Angabe aus einer Grok-Recherche, ungeprüft
- **[offen]** noch zu messen

---

## 1. Identifikation

| Merkmal | Wert | Beleg |
|---|---|---|
| Board | VCC-GND Studio (源地工作室) **YD-ESP32-S3**, Silkscreen „Made by YD“, **2022-V1.4**, www.vcc-gnd.com | [Foto] Rückseite |
| Bauform | Klon des Espressif ESP32-S3-DevKitC-1, aber 2 × USB-C | [Foto], [Quelle] |
| Modul | Espressif **ESP32-S3-WROOM-1** (PCB-Antenne) | [Foto] Blechdeckel |
| Modul-Code (Flash/PSRAM) | **unbekannt**: Zeile mit `N…R…` auf dem Foto nicht lesbar | [offen] |
| Herstellungscode | „2220“ auf dem Etikett (vermutlich Jahr 2022, KW 20) | [Foto] |

## 2. Speicher

### 2.1 Chip (fest)

| Ressource | Wert | Beleg |
|---|---|---|
| CPU | Xtensa LX7 Dual-Core, bis 240 MHz | [Quelle] |
| SRAM intern | 512 KB (+ 16 KB RTC-SRAM) | [Quelle] |
| ROM | 384 KB | [Quelle] |
| Nutzbarer Heap im Betrieb | grob 300 KB vor WLAN, deutlich weniger mit WLAN/TCP/Event-Ring | [Quelle], [offen] Messwert über `/api/status` → `hw` |

### 2.2 Modulvarianten (welche verbaut ist, ist offen)

Für YD-ESP32-S3 werden N8R2, N8R8 und N16R8 verkauft; N16R8 gilt als häufigste Variante [Grok]. Espressif-Varianten des WROOM-1 [Quelle]:

| Code | Flash | PSRAM | PSRAM-Bus | Folge für die Firmware |
|---|---|---|---|---|
| N8 | 8 MB Quad | — | — | nur internes RAM |
| N8R2 | 8 MB Quad | 2 MB | Quad (QSPI) | `board_build.arduino.memory_type = qio_qspi`, `-DBOARD_HAS_PSRAM` |
| N8R8 | 8 MB Quad | 8 MB | **Octal (OPI)** | `qio_opi`, `-DBOARD_HAS_PSRAM`; GPIO35–37 intern belegt |
| N16R8 | 16 MB Quad | 8 MB | **Octal (OPI)** | wie N8R8 |

Bei OPI-PSRAM sind **GPIO35, GPIO36, GPIO37** intern belegt und extern nicht nutzbar [Quelle].

### 2.3 Wie die Firmware den Speicher heute nutzt [Code]

- Build: `board = esp32-s3-devkitc-1`, `platform = espressif32@6.4.0` (Arduino-ESP32 2.0.x).
- **PSRAM ist nicht aktiviert** (kein `BOARD_HAS_PSRAM`, kein `memory_type`). Selbst ein verbautes PSRAM wird nicht genutzt; `ESP.getPsramSize()` meldet dann 0.
- Partitionstabelle `min_spiffs.csv` (für 4 MB Flash): 2 × App (OTA) ca. 1,9 MB + kleines SPIFFS. Flash über 4 MB bleibt ungenutzt.
- Offline-Capture: **nur RAM**, Puffer 64 KiB mit Fallback 48/32/24 KiB. **Im Feld bisher immer 32 KiB** (`capacity_bytes: 32768`) = 331 Records à 99 B. Grund: zu kleiner größter freier Block beim Arm. Mit dem Format `compact/1` (variable Länge, Ø ≈ 22 B) passen in 32 KiB rund 1500 Records (`docs/OFFLINE-CAPTURE.md`).
- Größter statischer RAM-Verbraucher: Event-Ring mit 2048 String-Slots (ca. 32 KiB).

### 2.4 Variante bestimmen [offen]

0. **Firmware (ab 0.2.31, `core/HwInfo.*`):** Die Sonde liest beim Boot dieselben eFuse-Felder wie esptool (BLK1: PSRAM-Größe, PSRAM-Hersteller, eingebetteter Flash, Gehäuse) und die JEDEC-ID des Flash-Chips. Das funktioniert auch bei abgeschaltetem PSRAM. Abrufbar:
   - WebUI: Zeile „Hardware: …“ unter der Überschrift
   - `GET /api/status` → `hw.module` (z. B. `N16R8`), `hw.flash` (`bytes_real`, `bytes_configured`, `bytes_unused`, `jedec_id`, `vendor`), `hw.psram` (`bytes_efuse`, `bus` quad/octal, `vendor`, `build_enabled`, `memory_type_needed`), `hw.efuse_raw`
   - Hub-Heartbeat `hwModule`, `capture_health.hw_module` (live und offline), Boot-Log `[HW] module=…`

   Grenzen: Der Modulname ist abgeleitet (N = echte Flash-Größe aus der JEDEC-ID, R = PSRAM laut eFuse). Ein eFuse-Wert 0 heißt „kein PSRAM im Chipgehäuse“; ein extern verdrahtetes PSRAM würde so nicht erkannt (bei WROOM-1 nicht vorgesehen). Die Quad/Octal-Zuordnung folgt den S3-Chipvarianten (R2 = quad, R8/R8V/R16V = octal). Noch nicht am Gerät geprüft.
1. **esptool-js im Browser** (ohne Installation): [espressif.github.io/esptool-js](https://espressif.github.io/esptool-js/) → Port **COM** wählen → Connect → Ausgabe `Features: … Embedded PSRAM 8MB/2MB` und `Detected flash size: …`. Nichts flashen, danach Disconnect und RST.
2. Alternativ: `pio pkg exec -p tool-esptoolpy -- esptool.py --chip esp32s3 flash_id`.
3. Oder den Modul-Code mit Lupe/Makro vom Blechdeckel ablesen.

## 3. USB-Ports, Stromversorgung, Lötbrücken

### 3.1 Ports

| Port (Silkscreen) | Lage | Anbindung | Nutzung im Projekt |
|---|---|---|---|
| **USB** | Oberseite **links**, Rückseite rechts | nativer USB des ESP32-S3: GPIO19 = D−, GPIO20 = D+, interner Full-Speed-PHY | **HU-Anschluss** (TinyUSB-MSC, `ARDUINO_USB_MODE=0`) |
| **COM** | Oberseite **rechts**, Rückseite links | **CH343P** USB-UART-Bridge an UART0 (GPIO43 TX / GPIO44 RX) | Flashen, Serial-Log, Stromversorgung |

[Foto] Silkscreen „COM“/„USB“ auf der Rückseite; der CH343P (QFN) sitzt auf der Oberseite neben der rechten Buchse.

- Getrennte PHYs: CH343P hat einen eigenen; der interne PHY gehört nur dem nativen Port. Beide Ports sind parallel nutzbar [Quelle/Grok].
- Intern teilen sich **USB-OTG** und **USB-Serial/JTAG** den einen PHY; aktiv ist immer nur einer. Die Pins GPIO19/20 sind fest und nicht über die GPIO-Matrix umlegbar [Quelle].
- Windows-Treiber für COM: WCH CH343SER ([wch-ic.com](https://www.wch-ic.com/products/CH343.html)) [Grok].

### 3.2 Lötbrücken [Quelle: Mischianti-Kommentare / Reddit r/esp32]

| Brücke | Lage | Offen (Auslieferung) | Geschlossen |
|---|---|---|---|
| **USB-OTG** | Rückseite, nahe „USB“ | VBUS der beiden USB-C-Buchsen getrennt (je über Diode auf 5 V) | VBUS **beider** Buchsen direkt verbunden |
| **IN-OUT** | Oberseite links („IN-OUT“) | 5Vin und USB-VBUS über Diode getrennt | eine Diode überbrückt; USB-VBUS liegt auf 5Vin (Rückspeisung möglich) |
| **RGB** | Oberseite neben RGB-LED | RGB-LED evtl. ohne Versorgung | RGB-LED versorgt |

**Projektfolge:** Im Feld hängt COM am Netzteil und USB an der HU. **USB-OTG muss offen bleiben**, sonst verbindet die Brücke Netzteil-5 V und HU-VBUS direkt (Rückspeisung in die HU, Ausgleichsströme beim Stecken). Ob sie an diesem Exemplar offen ist, ist auf dem Foto nicht sicher erkennbar → [offen], mit Lupe prüfen.

Die PiDrive-Reset-Analyse (HU-Facts Q11) beruht auf dem **Espressif**-DevKitC-1-Schaltplan (Schottky 1N5819 je Buchse, LDO SGM2212). Für dieses YD-Board gilt der YD-Schaltplan V1.4; die Diodenführung ist ähnlich, die Brücken sind zusätzlich. Q11-Hypothesen also mit YD-Schaltplan gegenprüfen.

### 3.3 Versorgung

- 5 V → 3,3 V LDO, laut Grok 1 A [Grok]; genauer Typ aus YD-Schaltplan V1.4 [offen].
- Versorgung über COM, USB, 5V-Pin oder 3V3-Pin möglich [Quelle].
- **Feld (BMW NBT Evo, mehrfach, Betreiberbeobachtung):** Nur am HU-Port (Buchse USB, ohne Netzteil an COM) leuchtet die PWR-LED und der ESP bootet. Die HU kann die Sonde also allein versorgen. **Nicht gemessen:** Strombudget des HU-Ports, Spannungseinbruch bei WLAN-TX, Verhalten bei Zündung aus/HU-Standby.
- Feld-Standardaufbau: COM am USB-Netzteil (Zigarettenanzünder), USB an der HU. Gründe, das Netzteil für Messungen zu behalten: RAM-Capture überlebt keinen Spannungsverlust vor dem Upload; physisches Abstecken (E8, S4, `unplug_idle`) ist nur mit Fremdversorgung messbar; Plug→INQUIRY bliebe sonst nicht vergleichbar (ESP-Bootzeit käme hinzu).
- Diagnose Q11 (Resets beim Stecken): A/B mit und ohne Netzteil. Ohne Netzteil gibt es weder Speisungsumschaltung noch Masseversatz zwischen Netzteil und HU.

## 4. Bedienelemente und LEDs

| Element | GPIO | Projekt [Code] |
|---|---|---|
| RGB-LED WS2812 | **GPIO48** | Status-LED über `neopixelWrite(RGB_BUILTIN, …)`, passt zu `esp32-s3-devkitc-1` |
| PWR-LED | — | nicht steuerbar |
| TX-/RX-LED | GPIO43 / GPIO44 (UART0) | blinken bei Serial-Log |
| BOOT-Taste | **GPIO0** | `RESET_BUTTON_PIN 0`: Marker (kurz), Stop/Skip (≥ 1,5 s) |
| RST-Taste | EN | Hardware-Reset |

## 5. Pinout (Header, Oberseite)

[Foto] Silkscreen stimmt mit der Tabelle überein. [Quelle] VCC-GND README / Schaltplan V1.4.

| Pin | Linke Leiste (P1) | Rechte Leiste (P2) |
|---|---|---|
| 1 | 3V3 | GND |
| 2 | 3V3 | TX (GPIO43) |
| 3 | RST (EN) | RX (GPIO44) |
| 4 | GPIO4 | GPIO1 |
| 5 | GPIO5 | GPIO2 |
| 6 | GPIO6 | GPIO42 |
| 7 | GPIO7 | GPIO41 |
| 8 | GPIO15 | GPIO40 |
| 9 | GPIO16 | GPIO39 |
| 10 | GPIO17 | GPIO38 |
| 11 | GPIO18 | GPIO37 (bei OPI belegt) |
| 12 | GPIO8 | GPIO36 (bei OPI belegt) |
| 13 | GPIO3 | GPIO35 (bei OPI belegt) |
| 14 | GPIO46 | GPIO0 (BOOT) |
| 15 | GPIO9 | GPIO45 |
| 16 | GPIO10 | GPIO48 (RGB-LED) |
| 17 | GPIO11 | GPIO47 |
| 18 | GPIO12 | GPIO21 |
| 19 | GPIO13 | GPIO20 (**USB D+**) |
| 20 | GPIO14 | GPIO19 (**USB D−**) |
| 21 | 5V (5Vin) | GND |
| 22 | GND | GND |

**Projektfolge:** GPIO19/20 liegen **parallel** zur USB-Buchse auf dem Header. Dort nichts anschließen: Jede Last oder jedes lange Kabel verschlechtert das USB-Signal zur HU.

## 6. WLAN bei USB-Last

Bekanntes S3-Thema, nicht YD-spezifisch [Grok/Quelle, ungeprüft]:

1. Der interne USB-PHY kann die WLAN-RF-Leistung beeinträchtigen.
2. Gleichzeitige USB- und WLAN-TX-Last erzeugt Stromspitzen (Brownout-Risiko bei schwacher Versorgung).
3. TinyUSB und der WLAN-Treiber konkurrieren um CPU und Bus.

Das passt zu den Projektbeobachtungen „WLAN stirbt unter OTG-Last“ und ist ein Grund für das Offline-Konzept (WLAN aus während der Messung) [Code: `OfflineCapture`, `wifiCoexTask`]. Abhilfen aus der Recherche: stabile 5-V-Versorgung (kurzes Kabel, ≥ 1 A), aktuelle Arduino-ESP32/IDF-Versionen. Das Abschalten des USB-PHY bei WLAN kommt für die hu-probe nicht in Frage, weil der native Port die HU bedient.

## 7. Offene Punkte

| # | Punkt | Wie |
|---|---|---|
| H1 | Modul-Code (Flash/PSRAM) | esptool-js (§2.4) |
| H2 | Lötbrücke USB-OTG offen? | Lupe/Foto Rückseite |
| H3 | LDO-Typ, Diodenführung | YD-Schaltplan V1.4 |
| H4 | Heap beim Arm im Feld | ab FW 0.2.31: `capture_health.arm_heap_free/arm_heap_max_alloc`, `/api/status` → `hw` |
| H5 | PSRAM nutzen (Aufnahmepuffer) | nach H1: Build-Flags je Variante, Puffer per `ps_malloc` |
| H6 | Strombudget HU-Port (Min/Max, Einbruch bei WLAN-TX) | USB-Messadapter zwischen HU und ESP; Betrieb nur über HU ist bestätigt |
| H7 | Resets beim Stecken mit/ohne Netzteil | je 5–10 Steckvorgänge A/B, `reset_reason` bzw. Neustart beim Stecken notieren |

## 8. Quellen

- VCC-GND Studio: [github.com/vcc-gnd/YD-ESP32-S3](https://github.com/vcc-gnd/YD-ESP32-S3) (README, Schaltplan `YD-ESP32-S3-SCH-V1.4.pdf`)
- Mischianti: [YD-ESP32-S3 pinout and specs](https://mischianti.org/vcc-gnd-studio-yd-esp32-s3-devkitc-1-clone-high-resolution-pinout-and-specs/) (inkl. Kommentare zu IN-OUT/USB-OTG/RGB)
- Espressif: ESP32-S3 Datasheet, ESP32-S3-DevKitC-1 Varianten-Tabelle
- Grok-Recherche vom 19.10.2026 (ungeprüft; nur wo markiert)
