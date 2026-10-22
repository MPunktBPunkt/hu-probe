# Feldplan: Cache-Key / Identity (PiDrive F0)

**Stand:** 2026-10-22 · **Firmware-Ziel:** **0.3.18** (Presets + `seq_ident`) · **Basis-Audit:** technology-audit `hu-probe-media-ladder-2026-10-22` @ `4bb65b0` · Upstream-Pin `8234982` / FW 0.3.17  
**Regel:** Vorhersagen unten stehen **vor** der Messung fest. Nach der Fahrt nur abhaken.  
**Geltung:** nur BMW NBT Evo.

---

## Tagesablauf (kritisch)

| Phase | Was | FW |
|-------|-----|-----|
| **1 · Heimfahrt** | `autoCapture` + Medium `drive_mix` auf Auto-Probe `.89` — **kein** OTA vorher | **0.3.17** (aktuell) |
| **2 · Rückkehr daheim** | Trip(s) hochladen (`GET /api/trips` → `POST /api/trips/upload`), Integrität prüfen | 0.3.17 |
| **3 · Danach** | OTA **0.3.18** (`/ota-upload` oder Hub), dann Cache-Key-Läufe im Stand | **0.3.18** |

**Nicht:** 0.3.18 vor/während der Heimfahrt flashen — sonst bricht die laufende Drive-Konfiguration / Trip-Kette.

Heimfahrt laut Media-Leiter-Nachtrag bereits per Config gesetzt (wirkt ab Boot, LED Lila). Vor Abfahrt kurz bestätigen:

```bash
curl -sS http://192.168.178.89/api/config | jq '{fwVersion,autoCapture,autoMedia,autoFine}'
# erwartet: autoCapture=true, autoMedia=drive_mix (oder bewusst anders)
```

Falls `autoCapture=false`:

```bash
curl -sS -X POST http://192.168.178.89/api/config \
  -H 'Content-Type: application/json' \
  -d '{"autoCapture":true,"autoMedia":"drive_mix","autoFine":true}'
# danach Soft-Reset / Stromzyklus, LED Lila beim nächsten Boot mit HU
```

---

## Warum Remount ≠ Cache-Key

Bereits FACT:

| Fakt | Folge |
|------|--------|
| A→B→A **ohne** Remount → oft kein DATA-Read | Cache **innerhalb** einer Medieninstanz |
| Remount → voller Re-Read | Remount **löscht** den Hit — misst nicht „woran erkennt die HU den Track?“ |

Deshalb zwei **getrennte** Blöcke:

1. **In-Session (I0–I2):** gleiche Mount-Epoche, Inhalt/ID3 per `mutate` — das ist der eigentliche Cache-Key.  
2. **Identity-Remount (I4–I7):** Serial/Label/Volume — steuert Resume / Medieninstanz, nicht denselben Hit-Mechanismus.

---

## Vor der Messung (Labor, nach OTA 0.3.18)

| # | Prüfung | Abnahme |
|---|---------|---------|
| L-1 | Presets `cache_a_pin`, `ident_serial`, `ident_label`, `ident_vol`, `ident_b` in WebUI | `GET /api/media/presets` listet alle |
| L-2 | Sequenz `seq_ident` startet | WebUI zeigt Schritt-Prompts I0…I4 |
| L-3 | `mutate` auf Lab: `title` ändern, `notify:none` | `media_mutate`-Marker; Größe/LBA unverändert |
| L-4 | `analyze_session.py` auf einer bekannten Session | läuft ohne Crash |

**Lab `.88` 2026-10-22:** L-1/L-2/L-3 **PASS** (OTA 0.3.18; Presets Serial/Label/Vol wie spezifiziert; `seq_ident` 8 Schritte start/abort; mutate title+seed, Size 158473/LBA 289 stabil). Kurzer `free`-Offline-Lauf blieb in `uploading/replaying` hängen → bekanntes Lab-Replay **B50**, per Re-OTA bereinigt; `drive_mix` wieder aktiv, `autoCapture=false`.

---

## Block A — In-Session Cache-Key (`tcp_live` oder Offline+`delay_s`)

**Medium:** `cache_a` oder `cache_a_pin` · **Experiment:** `free` (5–10 min) oder Live-TCP · **eine Variable pro Session**

### I0 Kontrolle (Hit bestätigen)

1. TRACK1 wählen, T1→T2→T3 Autoplay (Cache aufbauen).  
2. Zurück zu TRACK1 (`replay` / manuell).  
3. **Erwartung:** kein DATA-Read auf TRACK1 (oder nur TUR) → Hit FACT bestätigt.

### I1 ID3-only

Nach I0-Cache, **ohne** Remount:

```bash
curl -sS -X POST http://192.168.178.89/api/media/mutate \
  -H 'Content-Type: application/json' \
  -d '{"file":"TRACK1.MP3","title":"CacheKey I1","notify":"none"}'
```

Dann zurück zu TRACK1. Offline: vor Capture `delay_s` so setzen, dass Mutate nach dem ersten Durchlauf greift (WLAN aus).

| Hypothese | Erwartung | Folge PiDrive |
|-----------|-----------|---------------|
| **H-ID3-out** (Titel nicht im Key) | Hit (kein Head-Read); UI evtl. alter Titel | Menütitel live änderbar ohne Cache-Bruch |
| **H-ID3-in** | Miss (Head/Atom erneut); UI neuer Titel | ID3-Änderung = neue Datei-Identität |

### I2 Content-only (`seed`)

Wie I1, aber `{"file":"TRACK1.MP3","seed":11,"notify":"none"}` (byte-verschieden, Size/LBA gleich).

| Hypothese | Erwartung |
|-----------|-----------|
| **H-Content-out** | Hit trotz neuem Stempel |
| **H-Content-in** | Miss |

**Nicht mischen:** I1 und I2 in derselben Session nur nacheinander, wenn I0 dazwischen den Hit erneut bestätigt — sonst Auswertung unklar.

---

## Block B — Identity-Remount (`seq_ident`, FW ≥ 0.3.18)

**Sequenz:** `seq_ident` · Stand (kein Fahren) · BOOT bei Prompt / hörbarem Wechsel.

| Schritt | Medium | Serial-Policy | Frage |
|---------|--------|---------------|-------|
| I0 | `cache_a_pin` (HP0A01 / HUPROBE / `0x48500131`) | Policy | Baseline + Replay-Hit? |
| I0b | `cache_a_pin` | Keep | Remount gleiche ID → full reread? (Kontrolle FACT) |
| I5 | `ident_serial` (nur Serial `HP0S01`) | Policy | Nur Serial bricht Resume/Instanz? |
| — | `cache_a_pin` | Policy | Baseline zurück |
| I6 | `ident_label` (nur Label `HUPROBE_L`, Serial pinned HP0A01) | Keep | Nur Label? |
| — | `cache_a_pin` | Policy | Baseline zurück |
| I7 | `ident_vol` (nur Vol `0x48500701`) | Keep | Nur Volume-ID? |
| I4 | `ident_b` (Serial+Label+Vol) | Policy | Alle drei zusammen (Regression) |

**Vorhersagen (festgelegt 2026-10-22):**

| ID | Erwartung (offen → nach Messung abhaken) |
|----|------------------------------------------|
| I0b | Full Re-Read trotz gleichem Inhalt (bestätigt `remount_full_reread`) |
| I5 | **offen:** Resume bricht **oder** bleibt — entscheidet Serial im Medien-Key |
| I6 | **offen:** Label allein |
| I7 | **offen:** Volume-ID allein |
| I4 | wie bisher `ident_b`: mind. Mount/Scan neu; Resume-Semantik notieren |

Messkriterium pro Schritt (binär):

- **Hit-artig:** nach Rückkehr zum Track kaum/keine DATA-Reads  
- **Miss/Remount:** Head/Atom/Bulk erneut; erster Read nach Mount = welcher Track?

Operator notiert UI: alter/neuer Titel, Playlist-Position, Cover.

---

## Block C — bewusst später (nicht dieser Termin)

| Thema | Warum später |
|-------|----------------|
| I3 Bitrate/Größe | vermischt Extent + Key; erst nach I1/I2/I5–I7 |
| Audible / GATE-01 | eigener Lauf; Beifang ok, kein Blocker für Identity |
| B47 Analyzer | Schreibtisch vor Prefetch-Lead-Auswertung |
| Profil `bmw_nbt_evo.json` | einmal nach Tap+Identity nachziehen |
| Elevator-Drittstichprobe | P2, nicht Live-Audio-Blocker |

---

## Auswertung

```bash
python3 tools/analyze_session.py sessions/<id> --kbps 48
# Abschnitte: episodes / atoms / mount_signatures; media_mutate → mutation_reactions
```

Pro Session in Feldbericht: Hypothese eingetroffen / nicht / nicht messbar. Evidenz später in technology-audit.

---

## Artefakte FW 0.3.18

| Neu | Ort |
|-----|-----|
| Presets `cache_a_pin`, `ident_serial`, `ident_label`, `ident_vol` | `firmware/src/media/MediaPresets.h` |
| Sequenz `seq_ident` | `firmware/src/capture/ExperimentProfiles.h` |
| Build | `firmware/.pio/build/huprobe-s3/firmware.bin` → nach Build auch `ota/hu-probe-0.3.18.bin` |

---

## Kill / nicht tun

- Lead ≈ 8 s als Profil-Konstante setzen  
- „Nur Ordnerstruktur“ wiederbeleben  
- A8/`seq_size` mit dieser Matrix schließen  
- Mehrere Identity-Dimensionen in **einem** Schritt ändern (außer I4 bewusst alle)
