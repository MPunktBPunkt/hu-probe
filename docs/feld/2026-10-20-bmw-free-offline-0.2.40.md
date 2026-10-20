# Feld: BMW NBT — Offline `free` auf 0.2.40 — 2026-10-20

**Probe:** Auto-ESP `.89` (PiDrive-USB) · Collector `192.168.178.187:9100`  
**FW:** **0.2.40** (`b1b768fa7587-dirty-20261020T050224Z`) — WebUI-Default `bmw` + `free` · 60 s · `soft_usb=false` · `media_hide` · Unmount beendet nicht  
**HU:** BMW NBT Evo · SynthFat `cache_b` (TRACK1/2/3)  
**Status:** Drei Offline-Läufe **Integrity PASS** · Upload `delivered`. Prefetch-/Track-Muster reproduzierbar.

---

## 1. Sessions (Beweismittel)

| Session | Fenster | Records | USB im Fenster | File-Seq (Reads) | Marker |
|---|---|---|---|---|---|
| `20261020-075616_bmw_free` | 60 s · schon gesteckt | 69 | — | T2 → T3 | 2× `next_track_read` · `media_hide` |
| `20261020-075935_bmw_free` | 60 s · Plug nach Start | 207 | mount/enum | T1 → T2 → T3 | `mount_done` · `first_read` · 2× `next_track` · `media_hide` |
| `20261020-080508_bmw_free` | **90 s** · Replug mitte T3 | 231 | umount×2 + mount | T1 → T3 → T1 → T2 | dito · längeres Post-`next_track` bis Hide |

Alle: `transport=ram_offline` · `end_reason=auto_media_hide` · `dropped=0` · `offline_overflow=0` · `integrity_pass=true`.

Pfade: `sessions/<id>/` (`session.json`, `events.jsonl`).

---

## 2. Muster (FACT)

### A — Prefetch-Geometrie (stabil)

Datei-Reads sind **dünn** (wenige große Chunks), nicht Streaming-Byte-für-Byte:

| Datei | typ. LBA-Fenster | typ. `n` |
|---|---|---|
| TRACK1.MP3 | 9…249 | 122880 (240×512) und/oder 35328 (69×512) |
| TRACK2.MP3 | 318…558 | dito |
| TRACK3.MP3 | 627…867 | dito |

Das ist das bekannte **Head/Tail-Prefetch**-Bild (S2): HU holt Dateikopf + weiteren Block, nicht den ganzen Track kontinuierlich.

### B — Track-Wechsel ≈ Track-Dauer

Abstand `capture_t_us` zwischen aufeinanderfolgenden `next_track_read`:

| Session | Δ `next_track` → `next_track` |
|---|---|
| 075616 | **26,475 s** |
| 075935 | **26,475 s** |
| 080508 | **26,472 s** |

Entspricht der SynthFat-Tracklänge (`dur_ms≈26514` bei `cache_b`). Autoplay/Tap erzeugt also einen **festen ~26,5 s-Rhythmus** im Offline-Marker — unabhängig vom 60/90 s-Fenster.

Zusätzlich: nach `mount_done` folgt `first_read` in **~70–85 ms**; erster `next_track_read` oft **~1–1,6 s** danach (früher Tap/Autoplay-Start).

### C — Plug-nach-Start vs. schon gesteckt

- **Schon gesteckt** (075616): kein USB-Enum im Capture → weniger Records, aber Track-Wechsel trotzdem (`next_track`×2, T2/T3).  
- **Plug nach Start** (075935): volles Enum (`umount`/`set_address`/`mount`/Descriptoren) + FAT/DIR/BOOT-Reads + Prefetch T1→T2→T3.  
- **Replug mitten in T3** (080508): zweites `umount` + Remount; File-Seq springt **T1→T3→T1→T2** — nach Remount erneut Prefetch, nicht monoton T1→T2→T3.

### D — Timer schneidet Track 3

Bei 60 s endet Capture oft **vor Track-3-Ende** (LED/Web wieder da, während T3 noch läuft). 90 s gibt mehr Luft nach dem zweiten `next_track` (~39 s bis `media_hide` in 080508), aber EOF von T3 war nicht das Ziel dieses Laufs.

### E — LED nach Offline (Nebenbefund)

Nach `media_hide`: oft **Magenta** (TinyUSB `NEED_RESET`), dann **Gelb/Orange-Blink** trotz `tcp_ok` (LED wertet „kein frischer `tcp_tx_ok`“ bei gestecktem USB). Unkritisch für Capture-Integrity; RST räumt Latch/`plugged`.

---

## 3. Ablauf Feld (was funktioniert hat)

1. RST · dimmblau · WebUI `.89` · **bmw / free**.  
2. Optional OTG **raus** → Offline starten → OTG **rein** (Enum im Fenster).  
3. Tracks tippen oder Autoplay 1→2→3.  
4. Timer (`media_hide`) → WiFi zurück → Upload → Integrity PASS.

**0.2.40-Defaults:** ein Tipp „Offline starten“ reicht (kein Soft-USB, Unmount beendet nicht). Lab-Detach (`lab_debian`) gilt am Auto **nicht**.

---

## 4. Epistemik / offene Punkte

- Offline-Replay setzt UI-`t_ms` oft gleich; **Zeitachse = `capture_t_us` / `capture_seq`**.  
- S2-UNKNOWN-Regel (play_start + audible) gilt für Operator-Marker; hier tragen vor allem **`next_track_read` + Datei-LBA** die Epistemik.  
- Noch nicht systematisch: bewusstes Warten bis T3-EOF; Soft-USB-Re-Enum am NBT; Coex-Tod unter langem OTG ohne Offline (Silizium-Risiko bleibt).

---

## 5. FW 0.2.40 (mitgeliefert)

- WebUI/API-Default: Experiment **`free`**, Fahrzeug **`bmw`**, Dauer **60 s**.  
- Profil `free`: `soft_usb=false`, `end_on_unmount=false`, `media_hide`.  
- Lab `.88` + Auto `.89` geflasht und für die obigen Läufe genutzt.

---

## 6. Fazit

**Auswertbar:** Ja — drei konsistente Offline-PASSes.  
**Kernmuster:** Prefetch-LBAs pro Track + **~26,5 s** zwischen `next_track_read`; Remount löst erneutes Prefetch aus; 60 s-Fenster schneidet T3.  
**Nächster sinnvoller Lauf:** ≥90–120 s, Start auf T1 nach frischem Mount, Autoplay bis nach T3-Ende (oder BOOT-lang Stop).
