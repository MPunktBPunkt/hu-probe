# Feldplan P0 Größenleiter (PiDrive Q16 / B69)

**Stand:** 2026-10-27 · **Quelle:** PiDrive nächster Feldtest · **Messung:** hu-probe (kein Firmware-Feature)  
**Firmware-Ziel:** **0.3.21** — Presets `size_158` / `size_1m` / `size_4m` / `size_16m` + Sequenz `seq_size_ladder`  
**(0.3.20 Feld 26.10.:** sticky Magenta/`NEED_RESET` + HU kein Medium — Fix Soft-Cycle Latch; Retest GO [`2026-10-27-GO-groessenleiter.md`](2026-10-27-GO-groessenleiter.md))  
**OTA-Bin:** `firmware/dist/hu-probe.0.3.21.esp32s3.bin`  
**HU:** BMW NBT Evo · **CBR:** ~128 kbit/s · **Capture:** 180–300 s je Lauf (Sequenz: 4 min / 5 min @16 MiB)

**Frage:** Kann PiDrive lange/wachsende Bodies liefern, oder bleibt die HU bei kleinem Prefetch-Fenster?  
Steuert die **finale Play-Slot-MiB** im Zielbild — nicht Soft-Detach/Stall.  
Heute hängen starke Read-FACTs (R39 Fresh `32+128+6`) nur an ~158‑KiB-SynthFat; FULLMENU bleibt bewusst bei ~160 KiB bis die Leiter spricht.

**Regel:** Vorhersagen unten stehen **vor** der Fahrt fest. Nach der Fahrt nur abhaken.  
**Nicht** verwechseln mit der älteren Leiter `seq_size` / `size_1_4_8` (1/4/8 MiB + L3) — die bleibt optional/Lab.

---

## Stufen

| Größe | Preset | Zweck |
|------:|--------|--------|
| ~155 KiB (`size_kib:155`) | `size_158` | Kontrolle (bekannte Atom-/Fresh-Welt ≈158 KiB) |
| 1 MiB | `size_1m` | erstes Nachladen mitten in der Datei? |
| 4 MiB | `size_4m` | Plateau / Readahead-Grenze |
| 16 MiB | `size_16m` | realistischer Play-Slot + Zeit bis hörbarem Start |

Je Preset: **3 Tracks** gleicher Größe (seed 1/2/3), Serial gepinnt `HP0A01`, `spc` 8.  
`size_16m` setzt `volume_mib:64`.

Je Stufe zwei Läufe: **Fresh** und **Cycle+Resume** (ggf. 2. Cycle).

---

## Vor der Fahrt (Labor)

| # | Prüfung | Abnahme |
|---|---------|---------|
| L-1 | `GET /api/media/presets` listet `size_158`, `size_1m`, `size_4m`, `size_16m` | vorhanden |
| L-2 | Je Preset `POST /api/media/variant` → Lab-Host mount + `fsck.fat -n` | clean; Dateigrößen ≈ Spec |
| L-3 | Sequenz `seq_size_ladder` startet, 8 Schritte, Prompts lesbar | start/abort ok |
| L-4 | Optional parallel: `pidrive_l3` (L3-Geometrie) — Entscheidung kommt vom Feld, nicht vom Lab allein | mount ok |

---

## Ablauf: Sequenz `seq_size_ladder`

8 Schritte (pro Stufe Fresh → Cycle). Medium-Wechsel zwischen Stufen = Soft-Remount (Runner); deshalb **vor jedem Fresh-Arm Soft-Cycle** (WebUI oder BOOT‑Doppelklick in freier Session), sonst kein sauberer Fresh-Diskriminator.

| Schritt | Medium | Fenster | Operator |
|--------:|--------|--------:|----------|
| 1 | `size_158` | 4 min | Soft-Cycle → TRACK1, Autoplay; **kein** weiterer Cycle |
| 2 | `size_158` | 4 min | TRACK1; nach ~30 s Soft-Cycle; Resume; ggf. 2. Cycle |
| 3 | `size_1m` | 4 min | Soft-Cycle → TRACK1; Mitten-Nachladen? |
| 4 | `size_1m` | 4 min | Cycle+Resume wie oben |
| 5 | `size_4m` | 4 min | Soft-Cycle → TRACK1; Plateau? |
| 6 | `size_4m` | 4 min | Cycle+Resume |
| 7 | `size_16m` | 5 min | Soft-Cycle → TRACK1; **BOOT bei Wahl + hörbarem Start** (Startlatenz) |
| 8 | `size_16m` | 5 min | Cycle nach ~60 s; Resume + Nachladen |

Marker (`free`): BOOT kurz = Marke; BOOT doppelt = Soft-Cycle; BOOT lang = Schritt überspringen.

**Zeitbudget:** ≈ 8 × 4–5 min Capture + Wechsel ≈ **40–50 min** im Stand.

Alternativ ohne Sequenz: Experiment `free` + Preset manuell, gleiche Matrix.

---

## Messgrößen (Auswertung)

```bash
python tools/analyze_session.py sessions/<id> --kbps 128
```

Pro Lauf notieren / aus Analysis ziehen:

| Größe | Was |
|-------|-----|
| READ10-Offsets / Burst | `atoms`, CBW-Folgen, `read_windows` |
| EOF-Probe | Fresh: Länge der Tail-Probe (bei ~158 KiB historisch **6** in `32+128+6`) |
| body_rest | Lead vor hörbarem Start / vor Trackende |
| Cache-Hit/Miss | nach Cycle: Vollatom vs. kein DATA / nur TUR |
| @16 MiB zusätzlich | Startlatenz: Marker-Abstand Wahl → hörbar |

**Fresh-Diskriminator (Referenz R39):** flächendeckend Head+EOF (`32+128+6` o. Ä.), Start bei Track 1.  
**Cycle-Diskriminator:** oft Resume-Position + `tail_then_head` / Vollatom, **kein** Fresh-Bibliotheks-Scan.

---

## Vorhersagen (festgelegt 2026-10-24)

| Hypothese | Erwartung | Folge PiDrive |
|-----------|-----------|---------------|
| **H-Fenster** | Ab 1 MiB mehrere Read-Episoden / Fenster-Refill; nicht Eager-EOF der ganzen Datei in einem Rutsch | Play-Slot braucht Live-Nachschub; Slot-MiB an Fenster koppeln |
| **H-Eager** | Auch 4/16 MiB werden weitgehend vor/bei Start durchgezogen | großer Prefetch; Slot darf groß sein, Startlatenz steigt |
| **H-Plateau** | Ab fester Byte-/Zeitgrenze kein weiteres Vorlesen bis Bedarf | Plateau = HU-Puffer; Slot ≤ Plateau oder Nachladen Pflicht |
| **H-Fresh-skaliert** | EOF-Probe / Head-Muster skaliert mit Dateigröße; bei 16 MiB anderes Scan-Budget | Scan-Zeit + Startlatenz in UX einplanen |

Sonderfälle notieren: kein Nachladen trotz 4 min @1 MiB; Cycle löscht Body trotzdem; 16 MiB Start ohne hörbaren Ton trotz Reads (Audible-GATE bleibt parallel).

---

## Was das Ergebnis steuert

- finale **Play-Slot-MiB** im PiDrive-Zielbild  
- **nicht** Soft-Detach/Stall-Politik  
- FULLMENU-Slots bleiben klein (~160 KiB), bis die Leiter spricht  

Optional parallel Lab-Geometrien L0/L3 (`pidrive_l3`); Feld entscheidet.

---

## Nach der Fahrt

1. Sessions sichern; `analyze_session.py --kbps 128` je Lauf.  
2. Je Stufe Fresh vs Cycle in einer Tabelle: Offsets, Burst, EOF-Probe, body_rest, Hit/Miss, @16 MiB Startlatenz.  
3. Vorhersagen als eingetroffen / nicht / nicht messbar markieren → Feldbericht unter `docs/feld/`.  
4. Ergebnis an PiDrive (Q16/B69, Play-Slot-MiB); `profiles/bmw_nbt_evo.json` `read_window_blocks` nachziehen wenn belastbar.
