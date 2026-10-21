# Firmware-Ablauf (0.3.1)

Stand: aus dem Code abgeleitet (`App::begin`/`loop`, `DriveMode`, `OfflineCapture`, TinyUSB-Pfad).
Nicht auf Hardware nachgezeichnet.

Das README zeigt nur die **Systemübersicht** (HU ↔ Sonde ↔ Collector/Hub). Hier steht der
**interne Ablauf** der Firmware: Start, parallele Tasks, USB-Messpfad und die drei
Zustandsmaschinen.

## Tasks zur Laufzeit

```mermaid
flowchart TB
    setup["Arduino setup<br/>App::begin"] --> loopTask
    setup --> usbTask
    setup --> coexTask

    subgraph loopTask["Arduino-Loop Task"]
        L["App::loop<br/>jede Iteration"]
        L --> MSC["MscGadget::loop"]
        L --> OC["OfflineCapture::poll"]
        L --> DM["DriveMode::poll"]
        L --> SEQ["SequenceRunner::poll"]
        L --> TCP["TcpStreamer::loop"]
        L --> WEB["WebUi::loop"]
        L --> HUB["HubClient::loop<br/>ohne USB-Host immer,<br/>mit Host höchstens 1/min"]
        L --> LED["StatusLed::tick"]
        L --> PUB["status / capture_health<br/>wenn kein Coex-Hold"]
    end

    subgraph usbTask["TinyUSB usbd Task prio 24"]
        U["tud_msc_* Callbacks"]
        U --> FAT["SynthFat / Mp3Gen<br/>Sektoren on-the-fly"]
        U --> TAP["MscBotTap + ReadInstrument"]
    end

    subgraph coexTask["wifiCoex Task prio 25 core 0"]
        C["alle 500 ms"]
        C --> SUP{"OfflineCapture oder DriveMode<br/>besitzt WLAN?"}
        SUP -->|nein| W["App::superviseWifi"]
        SUP -->|ja| SKIP["WLAN nicht anfassen"]
        C --> FLUSH["bei Coex-Hold: TCP leeren"]
    end
```

`wifiCoexTask` liegt über TinyUSB (prio 25 vs. 24), weil der Arduino-Loop unter USB-Last
sonst die WLAN-Überwachung nicht mehr erreicht.

## Start (`App::begin`)

```mermaid
flowchart TD
    A["setup: Serial, PanicTail, DiagLog"] --> B["GithubOta::bootCheck<br/>Rollback nach 3 fehlgeschlagenen Starts"]
    B --> C["HwInfo, LED, ConfigStore NVS"]
    C --> D["EventBus, Session, ReadInstrument, AutoMarker"]
    D --> E["TripStore::begin"]
    E --> F{"config.autoCapture<br/>Heimfahrt-Modus?"}

    F -->|nein| G["MSC attached"]
    G --> H["WiFiManager autoConnect<br/>Portal-Timeout → Restart"]
    H --> I{"vehicle/profile<br/>lab_debian?"}
    I -->|ja| J["MSC soft-detach<br/>WLAN zuerst"]
    I -->|nein| K["MSC bleibt attached"]
    J --> L
    K --> L["NTP, Hub, TCP, WebUI, SequenceRunner"]
    L --> M["GithubOta::markHealthy"]
    M --> N["Live-Session auto-start"]
    N --> Z["wifiCoexTask starten"]

    F -->|ja| P["MSC startet detached"]
    P --> Q{"BOOT beim Start gedrückt?"}
    Q -->|ja| R["WLAN-Portal"]
    Q -->|nein| S{"Heimnetz oder Hotspot bekannt?"}
    R --> T
    S -->|ja| T["WiFiMulti 15 s<br/>kein Portal"]
    S -->|nein| R
    T --> L2["NTP, Hub, TCP, WebUI, SequenceRunner"]
    L2 --> U["DriveMode::bootPhase"]
    U --> Z
```

`DriveMode::bootPhase` (Heimfahrt, nach der übrigen Initialisierung):

```mermaid
flowchart TD
    A["GithubOta::markHealthy<br/>Bild gilt als gut, bevor der Upload lange dauert"] --> B{"WLAN verbunden?"}
    B -->|ja| C["NTP bis 3 s warten"]
    C --> D{"Collector erreichbar?"}
    D -->|ja| E["ausstehende Fahrten hochladen<br/>Journal leeren wenn fertig"]
    D -->|nein| F
    E --> F{"autoUpdate?"}
    B -->|nein| G["preErase 256 KiB"]
    F -->|ja| H["GithubOta::check Manifest<br/>bei neuer Version: Download, sha256, Restart"]
    F -->|nein| G
    H --> G
    G --> I["armCapture: Medium autoMedia,<br/>OfflineCapture persist + beginCapture"]
    I --> J{"Journal voll?"}
    J -->|ja| K["LED rot/lila, MSC attach,<br/>Normal oder CarIdle"]
    J -->|nein| L["WaitHost 20 s"]
```

## USB-Messpfad

Zwei Ziele, je nachdem ob gerade offline aufgenommen wird:

```mermaid
flowchart LR
    HU["Head-Unit USB-Host"] --> TUD["TinyUSB MSC"]
    TUD --> MED["ProbeMedia<br/>SynthFat + Mp3Gen"]
    TUD --> BOT["MscBotTap<br/>CBW / DATA / CSW"]
    BOT --> RI["ReadInstrument<br/>SCSI, LBA, Timing"]
    BOT --> ST["ReadStall optional<br/>BUSY halten"]
    RI --> SW{"OfflineCapture<br/>acceptsPending?"}
    SW -->|ja| RAM["kompakte Records im PSRAM"]
    RAM --> CKPT["Checkpoints ins Flash<br/>nur Heimfahrt, USB-Pause"]
    SW -->|nein| EB["EventBus Ring"]
    EB --> TCP["TcpStreamer JSONL<br/>Collector :9100"]
```

Live-Sessions schreiben jedes Ereignis sofort in den EventBus. Offline-Läufe puffern
zuerst (WLAN aus); nach Finalize kommt derselbe TCP-Pfad als Replay.

## Offline-Messung (`OfflineCapture`)

Auslöser: WebUI `/api/experiment/start`, Sequenzschritt oder Heimfahrt `armCapture`.

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Arming: arm<br/>Live-Session stop, WLAN aus, Puffer
    Arming --> Capturing: beginCapture<br/>optional Soft-Re-Enum
    Capturing --> Finalizing: Timer / BOOT lang / Unmount / API
    Capturing --> Idle: abortNoHost<br/>kein Host in 20 s
    Finalizing --> Uploading: WLAN an, Session, Replay
    Uploading --> Idle: fertig oder stored_in_flash
```

Während `Capturing`:

- BOOT kurz = nächster Marker (Profil) oder `mark_NN` (freie Marker)
- BOOT doppelt = Soft-Steckzyklus (nur freie Marker)
- BOOT lang ≥ 1,5 s = Stop
- Heimfahrt: in USB-Pausen Checkpoints (`ckpt:<ms>:<KiB>`); Stromverlust sichert bis zum letzten Checkpoint

## Heimfahrt (`DriveMode`)

`autoCapture` in der WebUI. `ownsWifi()` ist in Boot, WaitHost, Capturing und CarIdle wahr —
dann darf `superviseWifi` nicht neu verbinden oder neu starten.

```mermaid
stateDiagram-v2
    [*] --> Off
    Off --> Boot: autoCapture + begin
    Boot --> WaitHost: Capture scharf, Soft-Re-Enum
    Boot --> Normal: Arm fehlgeschlagen oder Journal voll mit WLAN
    Boot --> CarIdle: Journal voll ohne WLAN
    WaitHost --> Capturing: Host gemountet
    WaitHost --> Normal: 20 s kein Host, Lauf verworfen
    Capturing --> Normal: Aufnahme vorbei, WLAN da
    Capturing --> CarIdle: Aufnahme vorbei, kein WLAN<br/>Funkmodul bleibt aus
```

Stromverlust (Zündung aus) ist das normale Ende einer Fahrt: das Image startet neu, lädt
beim nächsten Heimnetz die Flash-Fahrt hoch und arm't erneut.

## Sequenz (`SequenceRunner`)

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Prepare: /api/sequence/start
    Prepare --> WaitDriver: Medium gewechselt
    WaitDriver --> Running: BOOT kurz oder auto_arm
    WaitDriver --> Prepare: BOOT lang = Schritt überspringen
    Running --> Prepare: nächster Schritt
    Running --> WaitDriver: Puffer-Overflow, Schritt halbieren einmal
    Running --> Done: letzter Schritt fertig
    Prepare --> Failed: Abbruch
    WaitDriver --> Failed: Abbruch
    Running --> Failed: Abbruch
```

Jeder Schritt ruft `OfflineCapture` auf. Szenario-Name:
`<seq>_<step>r<rep>[p<part>]`.

## WLAN/USB-Coex (`superviseWifi`)

Läuft nur, wenn weder Offline-Capture noch DriveMode das WLAN besitzen.

```mermaid
flowchart TD
    A["alle 500 ms"] --> B{"TCP-Stall?<br/>Queue voll, kein tcp_tx_ok"}
    B -->|ja| C["enterCoexHold<br/>USB soft-detach, WLAN stop/start"]
    C --> D["USB aus, bis Ring leer<br/>und Daten wieder fließen"]
    D --> E["reattach außer lab_debian"]
    B -->|nein| F{"WLAN verloren?"}
    F -->|ja| G["Reconnect alle 10 s"]
    G --> H{"nach 25 s und ≥ 3 Versuchen?"}
    H -->|ja| I["ESP.restart<br/>restart_cause=wifi_unrecoverable"]
    F -->|nein| J["weiter"]
```

## Diagnose-Log (was gerade passiert)

USB-Events stehen **nicht** im Diagnose-Log (die gehören in die Messung). `DiagLog` sammelt
nur den Lebenszyklus: WLAN, Capture-Zustände, DriveMode, OTA, Coex, Restarts.

| Speicher | Überlebt | Größe | Lesen |
|---|---|---|---|
| PSRAM-Ring | bis Reset | 64 KiB | WebUI Diagnose, `GET /api/diag/log` |
| RTC | Software-Reset, Panic, Watchdog — **nicht** Zündung aus | 2 KiB + 32 Breadcrumbs | Kopf von `/api/diag/log`, `panic_tail` / `panic_now` in `/api/status` |
| Flash-Snapshots | Stromverlust | 8 × 4 KiB (Anfang der `spiffs`-Partition, kein Dateisystem) | WebUI Flash-Snapshots, `GET /api/diag/saved` |

Flash wird nur an sicheren Stellen beschrieben (kein USB-Bulk):

```mermaid
flowchart LR
    B["Boot nach Panic/WDT/Software"] --> F["DiagLog::persist"]
    R["geplanter ESP.restart"] --> F
    C["Coex-Hold USB bereits detached"] --> F
    D["Heimfahrt bootPhase MSC detached"] --> F
    K["Checkpoint USB-Pause, höchstens 1/min"] --> F
    E["Capture-Ende vor USB-Restore"] --> F
    W["WebUI Jetzt sichern"] --> F
```

Ein Freeze ohne Watchdog hinterlässt nichts — das kann kein Log nachholen. Danach: Reset-Grund
`task_watchdog` / `panic` in der WebUI, letzter Flash-Snapshot, vorheriger RTC-Tail.

## Wo steuern?

| Absicht | Einstieg |
|---|---|
| Live messen (Labor) | WebUI Live-Session; Lab `lab_debian` muss MSC erst anhängen |
| Ein Experiment | WebUI Messlauf oder `POST /api/experiment/start` |
| Sequenz | WebUI Sequenz oder `POST /api/sequence/start` |
| Tägliche Fahrt | WebUI Heimfahrt-Modus `autoCapture` |
| Firmware holen | Hub-OTA, WebUI-Upload oder GitHub-Manifest (`enabled: true`) |
| Nachschauen was die Sonde tat | WebUI Karte Diagnose, `/api/diag/log`, `/api/diag/saved` |

Siehe [Offline-Capture](OFFLINE-CAPTURE.md), [Heimfahrt-Modus](feld/heimfahrt-modus.md),
[Szenarien](SZENARIEN.md).
