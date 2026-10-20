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
