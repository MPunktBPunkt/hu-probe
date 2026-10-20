# Feld: BMW NBT — Offline `free` auf 0.2.40 — 2026-10-20

**Probe:** Auto-ESP `.89` (PiDrive-USB) · Collector `192.168.178.187:9100`  
**FW:** **0.2.40** (`b1b768fa7587-dirty-20261020T050224Z`) — WebUI-Default `bmw` + `free` · 60 s · `soft_usb=false` · `media_hide` · Unmount beendet nicht  
**HU:** BMW NBT Evo · SynthFat **`demo_v1`** (TRACK1/2/3 je 158.208 B / 309 LBA; Starts 9 / 318 / 627)  
**Status:** Drei Offline-Läufe **Integrity PASS** · Upload `delivered`.  
**Nachtrag:** Multi-Review 2026-10-20 (Mistral/GPT/Grok/Claude) — Korrekturen B34/B40–B43 eingearbeitet.

---

## 1. Sessions (Beweismittel)

| Session | Fenster | Records | USB im Fenster | File-Seq (Reads) | Marker |
|---|---|---|---|---|---|
| `20261020-075616_bmw_free` | 60 s · schon gesteckt | 69 | — | T2 → T3 | 2× `next_track_read` · `media_hide` |
| `20261020-075935_bmw_free` | 60 s · Plug nach Start | 207 | mount/enum | T1 → T2 → T3 | `mount_done` · `first_read` · 2× `next_track` · `media_hide` |
| `20261020-080508_bmw_free` | **90 s** · Replug mitte T3 | 231 | umount×2 + mount | T3 → T1 (tail_then_head) → T2 | dito · längeres Post-`next_track` bis Hide |

Alle: `transport=ram_offline` · `end_reason=auto_media_hide` · `dropped=0` · `offline_overflow=0` · `integrity_pass=true` · `variant=demo_v1`.  
`parse_errors=47` in allen drei (ungeklärt, B44; Capture selbst PASS).

Pfade: `sessions/<id>/` (`session.json`, `events.jsonl`). Zeitachse: **`capture_t_us` / `capture_seq`** (nicht UI-`t_us`).

---

## 2. Muster (FACT)

### A — Volles Datei-Atom bei Trackwechsel (nicht Head-only)

Bei jedem normalen Wechsel liest die HU das **komplette** Track-Atom in zwei READ10-CBWs:

| Datei | Head CBW | Tail CBW | Summe |
|---|---|---|---|
| TRACK1.MP3 | 9/240 (122.880 B) | 249/69 (35.328 B) | **158.208 B** |
| TRACK2.MP3 | 318/240 | 558/69 | **158.208 B** |
| TRACK3.MP3 | 627/240 | 867/69 | **158.208 B** |

14/14 Atome über drei Sessions: immer **beide** CBWs. Die Fenster „9…249“ usw. sind nur die Head-Hälfte; der Tail folgt unmittelbar.

**Atom-Timing (normale Atome, n≈6–7):**

| Größe | Wert |
|---|---|
| Head-CBW `dur_us` | ~113,6–114 ms |
| Continuation-Gap nach Head-Ende (`gap_us`) | ~4,0–4,1 ms |
| Head-Start → Tail-Start | ~117,7–118,1 ms |
| Tail-CBW `dur_us` | ~31,3–31,7 ms |
| Atom total | ~149 ms · ~1,04 MiB/s |

Die ~4 ms sind **Continuation-Gap / 4-KiB-Pace**, nicht die Head→Tail-CBW-Lücke. Split 240+69 = max. CBW (120 KiB) + Rest bis EOF (nicht „HU-Grammatik“ an sich).

### B — Track-Wechsel ≈ Track-Dauer dieses Mediums

Abstand `capture_t_us` zwischen aufeinanderfolgenden `next_track_read` (normale Wechsel):

| Session | Δ `next_track` → `next_track` |
|---|---|
| 075616 | **26,4746 s** |
| 075935 | **26,4747 s** |
| 080508 | **26,4717 s** |

≈ **26,473 s** — FACT für **dieses** `demo_v1`-Medium (beobachtete Wiedergabedauer), **kein** universeller NBT-Timer.

Zusätzlich: nach `mount_done` folgt `first_read` in **70–86 ms** (FACT, n=2); erster `next_track_read` oft ~1–1,6 s danach (Operator/Autoplay, kein sauberer HU-Timer).

### C — Kein Früh-Prefetch (Idle nur TUR)

Zwischen Atomen (Idle-Fenster 26–44 s): **0 File-READ10**, nur TUR mit Periode **2238–2239 ms** (p10=p50=p90, 3 Sessions).  
PiDrive-R21 „~15 s vor Trackende Prefetch“ ist für **158-KiB-demo_v1** **REJECTED**. Prefetch bei großen Dateien bleibt **UNKNOWN**.

GATE-01 schrumpft auf: Restlatenz `next_track_read` → hörbarer Wechsel (audible-Marker fehlt noch).

### D — Plug-nach-Start vs. schon gesteckt vs. Replug

- **Schon gesteckt** (075616): kein USB-Enum im Capture → weniger Records; Track-Wechsel T2→T3 trotzdem.  
- **Plug nach Start** (075935): volles Enum + FAT/DIR/BOOT + volle Atome T1→T2→T3; Enum-Sonde LBA 24/8 reproduziert.  
- **Replug mitten in T3** (080508): Remount → kurzes **T3-Atom** (~70 ms nach `mount_done`) → ~1,46 s → **T1 tail_then_head** (249/69 dann 9/240, Tail→Head **~34,3 ms**) → T2 im Normalrhythmus.  

**Nicht FACT:** „Remount startet Playlist pauschal bei T1“. FACT: T3 wird kurz adressiert, dann T1; Semantik (Resume vs. Restart vs. UI) offen.  
`tail_then_head` = starke Re-Entry-INFERENCE (n≥2 Kontexte); gezielte Pause→Play-Stichprobe offen.

### E — Timer schneidet Track 3

Bei 60 s endet Capture oft **vor Track-3-Ende**. 90 s gibt mehr Luft; für volle Ketten Default **90–120 s** empfohlen.

### F — LED nach Offline (Nebenbefund)

Nach `media_hide`: oft **Magenta** (TinyUSB `NEED_RESET`), dann **Gelb/Orange-Blink** trotz `tcp_ok`. Unkritisch für Integrity; RST räumt Latch/`plugged`. Soft-Serial kann pro Boot driften (HP0001→HP0002); Session-Verknüpfung über MAC.

---

## 3. Ablauf Feld (was funktioniert hat)

1. RST · dimmblau · WebUI `.89` · **bmw / free**.  
2. Optional OTG **raus** → Offline starten → OTG **rein** (Enum im Fenster).  
3. Tracks tippen oder Autoplay 1→2→3.  
4. Timer (`media_hide`) → WiFi zurück → Upload → Integrity PASS.

**0.2.40-Defaults:** ein Tipp „Offline starten“ reicht (kein Soft-USB, Unmount beendet nicht). Lab-Detach (`lab_debian`) gilt am Auto **nicht**.

---

## 4. Epistemik / offene Punkte

- Offline-Replay: UI-`t_ms` kollabiert oft; **Zeitachse = `capture_t_us` / `capture_seq`**.  
- GATE-01-Rest: `audible_change`-Marker (Operator-Tap).  
- Cache-Hit A→B→A ohne Remount / Remount-Reread: aus Serie FACT; offen ist der **Cache-Key** (Identity).  
- Größenleiter (>158 KiB) für Prefetch bei PiDrive-Medien: offen.  
- `parse_errors=47` (B44), dirty FW-Build (B45).

---

## 5. FW 0.2.40 (mitgeliefert)

- WebUI/API-Default: Experiment **`free`**, Fahrzeug **`bmw`**, Dauer **60 s**.  
- Profil `free`: `soft_usb=false`, `end_on_unmount=false`, `media_hide`.  
- Lab `.88` + Auto `.89` geflasht und für die obigen Läufe genutzt.  
- Traceability: Build dirty (`FW_GIT_DIRTY=1`) — für Profil-FACT später clean Build wünschenswert.

---

## 6. Fazit

**Auswertbar:** Ja — drei konsistente Offline-PASSes mit belastbarer Grammatik.  

**Kernmodell (`demo_v1`):** Trackwechsel → volles 158.208-B-Atom (~149 ms) → Idle nur TUR (~2239 ms) → nächster Wechsel (~Trackdauer). Kein beobachteter File-Prefetch während der Wiedergabe.  

**Nächster sinnvoller Lauf:** Größenleiter (1/4/16 MiB) + audible als Beifang; Cache-Key-Diskriminierung; nicht weitere reine Prefetch-Geometrie auf 158 KiB.

---

## 7. Nachauswertung (Rohdaten nachgerechnet, 2026-10-20 nachmittags)

Grundlage: die drei `events.jsonl` (Zeitachse `capture_t_us`), `demo_fat.bin` (MP3-Frames gezählt), die S2-Sessions vom 15.10. und 18./19.10. und die Lab-Sessions `lab_debian`. Labels wie in der Serie: FACT = direkt gemessen, INFERENCE = abgeleitet, HYPOTHESIS = ungeprüft.

### 7.1 Auflösung offener Review-Punkte

| Punkt | Ergebnis |
|---|---|
| B41/B42 | Beide Zahlen stimmen. `gap_us` (Ende CBW n → Beginn CBW n+1) = 4,0–4,1 ms normal, 2,7 ms bei tail_then_head. CBW-Start → CBW-Start = 117,7–118,1 ms bzw. 34,3 ms. Nur die Benennung muss sauber sein. |
| B43 | Operator: Nach dem Replug lief T3 **an der Aussteckstelle weiter**, danach kam per Autoplay T1. Rechnung: Atom-Ende T3 → nächster Read 1,427 s statt 26,325 s → Resume-Position ≈ 24,9 s (falls während des Aussteckens pausiert). |
| B44 `parse_errors=47` | **Ursache gefunden:** Der Collector zählte global über die ganze Prozesslaufzeit und schrieb diesen Wert in jede Session. Die drei Sessions haben 0 nicht parsebare Zeilen. Fix: Zähler pro Session, Gesamtwert getrennt als `collector_parse_errors_total`. |
| Profil `next_track_read_s` (9–16 s nach Start / 11–18 s vor Ende, S2 15.10.) | **Kein Prefetch, sondern Resume.** In allen S2-Sessions ist der erste Read nach dem Mount der zuletzt gespielte Track (Resume mitten im Track); der nächste Track folgt nach der Restlaufzeit (9–21 s). Die Vorlauf-Rechnung hatte Wiedergabe ab Trackanfang angenommen. |

### 7.2 Neue Muster

1. **tail_then_head = Rückwärtssprung in den LBAs (5/5).** Alle bekannten Fälle (`195417`, `200319`, `2276`, `212136`, `080508`) sind T3 → T1. Bei jedem anderen Wechsel (T1→T2, T2→T3, erster Read nach Mount) liest die HU head-first.
   - Erklärung (INFERENCE): Die HU stellt beide CBWs der Datei gleichzeitig in eine Warteschlange und bedient zuerst den, der näher an der zuletzt gelesenen LBA liegt (Elevator). Nach T3 (Ende LBA 935) liegt der Tail (249) näher als der Head (9). Der kürzere `gap_us` (2,7 ms statt 4,1 ms) passt zu einer schon wartenden Anforderung.
   - Damit ist die Deutung "Re-Entry-Grammatik" überholt.
   - Vorhersage: Jeder manuelle Rückwärtssprung (T2→T1, T3→T2) liest tail-first; jeder Vorwärtssprung head-first.
2. **256-KiB-Lesefenster (HYPOTHESIS).** PiDrive A7 (512-KiB-Slot = `240,240,32,240,240,32`) sind 2 × 512 Blöcke; `demo_v1` mit 309 Blöcken ist ein am Dateiende abgeschnittenes Fenster (`240,69`). Modell: Die HU fordert Fenster bis 512 Blöcke an und teilt sie in CBWs bis 240 Blöcke. "Volles Atom" und "kein Prefetch" auf `demo_v1` folgen daraus, weil die Datei kleiner als ein Fenster ist. Bei 128 kbit/s entsprechen 256 KiB 16,4 s Audio; R21 ("~15 s") könnte das Nachladen des nächsten Fensters sein.
3. **Wiedergabetakt.**
   - `demo_v1`-Tracks: MPEG-1 Layer 3, 48 kbit/s, 44,1 kHz, joint stereo, ohne Xing/Info-Tag. Sie haben 1014 Frames, der letzte ist um 20 B abgeschnitten; 1013 vollständige Frames ergeben 26,462 s.
   - Read-zu-Read (Start→Start) = 26,4746 / 26,4747 / 26,4717 s, also Audiodauer + ~12 ms (FACT für dieses Medium).
   - Atom-Ende → nächster Read = 26,32526 / 26,32510 / 26,32355 s.
   - Beim tail_then_head-Wechsel passt der Takt zum Atom-Ende (1,6 ms Abweichung), nicht zum Head-Start (37 ms). Vermutung (INFERENCE, n=1): Die Wiedergabe startet erst, wenn die ganze Datei gelesen ist.
4. **TUR-Timer läuft frei (FACT).**
   - Periode 2238,79 ms ± 0,01 ms.
   - Weder Atome noch der Media-Loss-Burst verschieben die Phase: TURs nach einem Atom liegen auf dem vor dem Atom extrapolierten Raster (z. B. 169,8155 s in 075616). Einzelne TURs weichen um ~10 ms ab, ohne das Raster zu verschieben.
5. **Media-Loss-Erkennung nur am TUR-Tick (FACT).**
   - Vom `media_hide` bis zur ersten fehlschlagenden TUR vergehen 1,239 / 0,394 / 2,182 s; die TUR liegt jeweils auf dem alten Raster.
   - Daraus folgt ein Blindfenster bis 2,24 s, in dem die HU eine Medienänderung nicht bemerkt, solange sie nicht liest.
6. **Media-Loss-Burst deterministisch (FACT, n=3).**
   - Ablauf: 11–12 TUR/REQUEST_SENSE-Paare plus 1× PREVENT_ALLOW in ~180–220 ms, Abstände ≈ 9 / 1 / 8 / 32 / 12–17 ms.
   - Danach wieder TUR im alten 2,24-s-Raster: Die erste TUR nach dem Burst liegt auf dem Raster der ersten Burst-TUR.
7. **Plug-Zeitkette (FACT, n=2):**

| Schritt | 075935 | 080508 |
|---|---:|---:|
| set_address → SET_CONFIG | 22,5 ms | 23,4 ms |
| SET_CONFIG → GET_MAX_LUN | 996,9 ms | 996,0 ms |
| GET_MAX_LUN → mount_done | 407,9 ms | 429,0 ms |
| mount_done → erster Datei-Read | 85,7 ms | 70,1 ms |
| **set_address → erster Datei-Read** | **1,513 s** | **1,518 s** |

8. **Resume nach Remount (FACT Operator + Rechnung, n=1 heute; S2: erster Read nach Mount = zuletzt gespielter Track in allen Sessions).** Beim Resume liest die HU das volle Atom ab Block 0; die Resume-Position ist bei Dateien unter einem Fenster in den LBAs nicht sichtbar. Serial vor und nach dem Replug gleich (HP0002).
9. **LBA 24/8 ist sehr wahrscheinlich ein Dateisystem-Probe (INFERENCE).** Der Linux-Labhost (`lab_debian`) liest beim Mount 0, 24, 8, 32, 64, 128, 256, 512 (libblkid-Muster). LBA 24 = Byte 12288 = 4-KiB-Seite 3, absolut und nicht TRACK1-bezogen. Probe: Preset `tail_probe` (40 reservierte Sektoren, LBA 24 liegt dann im Reserved-Bereich).
10. **Lesetempo ist Bus-Grenze (INFERENCE).** 3,79 ms je 4 KiB ≈ 1,08 MB/s ≈ 89 % des theoretischen Full-Speed-Bulk-Maximums (~1,2 MB/s). Kein HU-Parameter, aber Obergrenze für PiDrive-Bursts.

### 7.3 Folgen für das Modell

- Die NBT-Lesegeometrie ist für Dateien unter 256 KiB erklärt (ein Fenster, Elevator-Reihenfolge, Bus-Tempo). Offen ist, **wann** bei größeren Dateien das nächste Fenster kommt. Das ist die eigentliche PiDrive-Frage.
- tail_then_head ist kein Zustandsmerkmal und taugt nicht als Re-Entry-Detektor.
- Das TUR-Blindfenster ist ein Hebel für PiDrive (Medienwechsel ohne Neu-Mount) und zugleich ein Risiko (veralteter HU-Cache).

Feldplan für die Prüfung: [`2026-10-21-feldplan-groessenleiter.md`](2026-10-21-feldplan-groessenleiter.md). Analyzer: `tools/analyze_session.py` gibt diese Kennzahlen jetzt als `tur_phase`, `media_loss`, `plug_timeline`, `atoms` und `read_windows` aus.
