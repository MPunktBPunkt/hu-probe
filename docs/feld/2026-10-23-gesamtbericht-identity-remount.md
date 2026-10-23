# Gesamtbericht: Cache-Key Identity-Remount (Stand) 2026-10-23

**Probe:** Auto-ESP `.89` · FW **0.3.19** · BMW NBT Evo · Stand (kein Fahren)  
**Plan:** [`2026-10-22-feldplan-cache-key-identity.md`](2026-10-22-feldplan-cache-key-identity.md) · Presets ab 0.3.18  
**Collector:** `192.168.178.187:9100` (während Lauf kurz neu gestartet)  
**Artefakte:** `docs/feld/artifacts/20261023-identity-remount/`  
**Methode:** Operator-Beobachtung HU + API-gesteuerte Soft-Remount / USB-Cycle / Preset-Wechsel; `seq_ident` nur teilweise (Upload-Stall)

---

## 0. Ampel

| Bereich | Status | Beleg |
|---|---|---|
| Soft-Remount (NOT READY) unterbricht Autoplay | 🔴 FACT | 2× unsichtbar; T2→T3 lief durch |
| USB-Cycle (Re-Enum) unterbricht / Neu-Einlesen | 🟢 FACT | Stick weg, HU lädt neu |
| Cycle gleiche Identity → Resume Position | 🟢 STRONG | mehrfach (u. a. Track 3, Track 2) |
| Label allein bricht Resume | 🔴 verworfen | I6: Remount + Resume |
| Volume-ID allein bricht Resume | 🔴 verworfen | sauberer Retest: Resume bei 3 |
| Serial+Label+Vol alle neu bricht Resume | 🔴 verworfen | I4: Resume bei 2 |
| Resume an Inhalt/Struktur, nicht Identity | 🟢 STRONG | gleiche SynthFat-Tracks (seed 1/2/3) |
| Identity-Felder triggern Remount-Wahrnehmung | 🟢 FACT | Cycle bei Preset-Wechsel → weg/neu |
| Prefetch vor Track-Ende (LED grün) | 🟢 FACT | Ende 1/2 und mitten in 3 |
| In-Session Cache-Key (I1/I2 mutate) | ⚪ offen | nicht gefahren |
| `seq_ident` Ende-zu-Ende | 🟠 | I0 Capture ok, Upload hing → Sequenz abgebrochen |

---

## 1. Ablauf (Operator-Zeit ~07:24–07:50)

| Zeit (ca.) | Aktion | Medium / Serial | HU-Beobachtung |
|---|---|---|---|
| 07:26 | `seq_ident` Start, Schritt I0 | `cache_a_pin` / HP0A01 | Autoplay T1→T2→T3, Replay T1 |
| 07:29 | Measure-Ende `media_hide` | — | HU „hängt“ (Medium versteckt) — erwartet |
| 07:32+ | Soft-Remount (gleiche Serial) | HP0A01 | Autoplay wieder möglich |
| — | Soft-Remount ×2 | HP0A01 | **nicht** spürbar |
| — | USB-Cycle 800 ms | HP0A01 | weg/neu; **Resume** Track 1 Position → T2→T3 |
| — | weiterer Cycle (Upload-Unblock) | HP0A01 | weg/neu; Resume bei **3** → Autoplay 1 |
| 07:43 | RST (Upload-Stall), dann I5 | `ident_serial` / **HP0S01** | neu; Start bei **1** (RST vermischt) |
| 07:45 | I6 Cycle | `ident_label` / HP0A01 / **HUPROBE_L** | weg/neu; **Resume** Position 1 → 2→3 |
| 07:46 | I7 Cycle | `ident_vol` / Vol **0x48500701** | weg/neu; Operator: „vermutlich 1 neu“ |
| 07:47 | I7 Cycle gleiche Vol | gleich | weg/neu; **Resume bei 3** |
| 07:48 | I4 Cycle | `ident_b` / HP0B01+HUPROBE_B+0x48500B01 | weg/neu; **Resume bei 2** |
| 07:48 | Baseline Cycle | `cache_a_pin` | Resume bei **2** |
| 07:49 | I7 sauber: pin → vol | nur Vol-ID | weg/neu; **Resume bei 3** |

Sessions (Collector): siehe Artefakt-Ordner; Live-TCP `bmw_free` je Preset-Wechsel.

---

## 2. Muster & Verhalten

### 2.1 Remount-Stärke

1. **`/api/usb/remount` (NOT READY, ~600 ms)** reicht auf NBT Evo **nicht**, um Autoplay zu unterbrechen. Die HU spielt aus Puffer/Cache weiter.  
2. **`/api/usb/cycle` (Soft-Detach D+/Re-Enum)** ist die brauchbare Operator-Remount-Stimulation: Stick verschwindet, Neu-Einlesen, dann Wiedergabe.

Implikation Feld/PiDrive: Identity-Tests und Remount-Messungen mit **cycle**, nicht nur remount.

### 2.2 Resume ≠ Cold Start (verschärft)

Heimfahrt 2026-10-22: Remount ≠ Cold Start (positionsbezogene Probe, ROCK/R04-Kontext).

Heute, kontrolliert mit **byte-gleichem** SynthFat (`cache_a` / seeds 1–2–3):

- Nach Cycle mit **unverändertem** Inhalt setzt die HU die **Playlist-Position** fort (Track + Stelle).  
- Das gilt auch, wenn USB-Serial, FS-Label und/oder Volume-ID wechseln (I4, I6, sauberer I7).

**Arbeitshypothese (STRONG):** Die HU keyed Resume/Playlist an **Medieninhalt bzw. Datei-Identität** (Pfad/Größe/Inhalt), nicht an USB-Serial / Volume-Label / Volume-ID. Die Identity-Felder beeinflussen, *dass* neu gemountet/gescannt wird, nicht *ob* die Position verworfen wird.

### 2.3 Vorhersagen aus dem Feldplan — Abgleich

| ID | Vorhersage (2026-10-22) | Ergebnis 2026-10-23 |
|---|---|---|
| I0b | Full Re-Read trotz gleicher ID | Cycle: Neu-Einlesen **ja**, aber Resume — kein Cold Start |
| I5 Serial | offen | RST+Serial vermischt → Start bei 1; **nicht** isoliert |
| I6 Label | offen | Remount + **Resume** → Label allein bricht Instanz nicht |
| I7 Vol | offen | Erstbeobachtung unklar; **Retest Resume** → Vol allein bricht nicht |
| I4 alle | Mount/Scan neu; Resume notieren | Mount neu + **Resume bei 2** |

### 2.4 Prefetch (Beifang)

LED grün (= Host liest) mehrfach kurz vor Track-Ende und einmal mitten in Track 3. Passt zum bekannten Prefetch/Eager-Muster; kein neuer Quant-Befund ohne Analyzer-Korrelation.

### 2.5 Betriebsstörungen (Meta)

| Problem | Wirkung | Mitigation heute |
|---|---|---|
| Offline-Ende `media_hide` | HU tot bis Upload/`restoreMedia` | Soft-Remount half der HU; Sequenz blieb im Upload |
| Offline-Upload `replaying` hing | `seq_ident` kam nicht über Schritt 1; Medienwechsel 409 | Collector-Restart; danach weiter RST |
| Collector `:9100` | viele ESTAB, HTTP auf 9100 ist ohnehin kein HTTP (JSONL) | Prozess neu gestartet; Upload auf ESP blieb trotzdem stecken |
| Kein API-Abbruch für Upload-Phase | `POST /api/offline/stop` nur während Capture | RST als Notausgang |

**FW-Follow-ups (P1):** Upload-Stall trotz `tcp_ok` härter abbrechen; nach `media_hide` Medium wieder präsentieren sobald Measure vorbei (oder Operator-API `restoreMedia`); Sequenz darf Identity-Schritte nicht hinter totem Replay blockieren.

---

## 3. Was das für PiDrive / Cache-Key heißt

| Frage | Antwort jetzt |
|---|---|
| Ist Remount = Cache-Flush der Track-Position? | **Nein** — Position überlebt Cycle bei gleichem Inhalt |
| Steuert Serial/Label/Vol die Playlist-Instanz? | **Eher nein** (unter diesen Presets) |
| Was bleibt für echten Cache-Key? | **In-Session** I1/I2 (`mutate` ID3/seed **ohne** Remount) — heute nicht gefahren |
| Nächster sinnvoller Sprung | Audible/Play-Intent (Heimfahrt-Konsens) **oder** I1/I2 Stand ohne Remount |

Identity-Remount-Block des Feldplans ist für NBT Evo **operativ ausgewertet**: Remount-Wahrnehmung ≠ Resume-Verlust; Identity-Dimensionen allein erklären Resume nicht.

---

## 4. Sessions / Artefakte

| Session | Medium | Rolle |
|---|---|---|
| `20261023-072929_bmw_seq_ident_1r1` | `cache_a_pin` (I0 offline) | einziger Offline-Schritt; 434 Records, Upload problematisch |
| `…074319…` | `ident_serial` | I5 nach RST |
| `…074455…` | `ident_label` | I6 |
| `…074610…` / `…074705…` | `ident_vol` | I7 + Repeat |
| `…074749…` | `ident_b` | I4 |
| `…074838…` | `cache_a_pin` | Baseline vor Retest |
| `…074908…` | `ident_vol` | sauberer I7 |

Pfad: `docs/feld/artifacts/20261023-identity-remount/`.

---

## 5. Nächste Schritte

1. **I1/I2** In-Session mutate (ID3 / seed, `notify:none`) — eigentlicher Cache-Key.  
2. FW: Offline-Upload Deadlock + `media_hide` UX beheben, bevor wieder `seq_*` im Feld.  
3. Optional: I5 Serial **ohne** RST (von `cache_a_pin` → `ident_serial` per Cycle), um Serial isoliert zu bestätigen (Erwartung nach I4: ebenfalls Resume).  
4. Audible GATE unverändert P0 für Play-Intent.
