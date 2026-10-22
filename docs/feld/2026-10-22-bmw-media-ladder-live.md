# Feld: BMW NBT — Media-Leiter Live-TCP — 2026-10-22

**Probe:** Auto-ESP `.89` · Collector `192.168.178.187:9100`  
**FW:** **0.3.17** · `transport=tcp_live` · Szenario `free` · Profil `bmw_nbt_evo`  
**HU:** BMW NBT Evo · SynthFat-Medien nacheinander gewechselt  
**Status:** Live-Leiter **Integrity PASS** (alle BMW-Sessions). Lab-Offline `.88` separat **FAIL** (Replay).  
**Nachtrag:** Heimfahrt-Modus (`autoCapture`) danach per Config aktiviert — wirkt ab nächstem Boot (LED Lila).

Vergleich: [`artifacts/20261022_bmw_media_ladder_compare.md`](artifacts/20261022_bmw_media_ladder_compare.md) · Analysen unter [`artifacts/20261022-bmw-media-ladder/`](artifacts/20261022-bmw-media-ladder/).

---

## 1. Sessions (Beweismittel)

| Session | Medium | Fenster (ca.) | Events | Integrity | Operator |
|---|---|---|---:|---|---|
| `61434_bmw_free` | `cache_b` | 08:02–08:03 | 514 | PASS | Warm-up / Scan |
| `20261022-080353_bmw_free` | `cache_place` | 08:03–08:05 | 664 | PASS | Scan + Tracks |
| `20261022-080544_bmw_free` | `dur_mix` | 08:05–08:07 | 1251 | PASS | D010→D026→D060→D180 getippt |
| `20261022-080752_bmw_free` | `bitrate_mix` | 08:07–08:11 | 2362 | PASS | B032/B128/B320 Autoplay + manuell |
| `20261022-081152_bmw_free` | `id3_cover` | 08:11–08:14 | 817 | PASS | COVER1–3 + NOCOVER |
| `20261022-081410_bmw_free` | `drive_mix` | ab 08:14 (offen/lang) | ≥13k | PASS | Hörbuch K01–4, Jazz J01 |
| `20261022-065150_lab_debian_free` | Lab `.88` offline | 06:51 | 56 | **FAIL** | 30 s free, Soft-Detach — Replay leer |

Alle BMW: `parse_errors=47` (bekannt, B44; Capture PASS), `msc_sample_overflow=0`, TUR-Idle-Median **2,2388 s**.

Pfade: `sessions/<id>/` (`events.jsonl`, `session.json`). Zeitbasis Analyse: **`t_us` MEASURED**.

---

## 2. Operator-Beobachtungen ↔ Messung (FACT)

Mitprotokolliert während der Fahrt/Stand (LED Grün = HU liest):

| Beobachtung | Messung |
|---|---|
| Track 1 ohne Grün (nach Scan) | Datei nur im Mount-Fenster gelesen, danach Cache (`B032`, `COVER1`) |
| Grün **vor** hörbarem nächsten Track | Prefetch-Burst auf nächste Datei (`next_track_read` / Episoden vor Start) |
| Bei `B320` Grün vor 3 **und** nochmal wenn 3 läuft | Zwei große Read-Cluster (~116 s und ~123 s Sessionzeit); vorwärts, kein Rewind |
| `J01` „nochmal vorderes Viertel“ | Kein Seek zurück: sequentielle Cluster (früh → Mitte → EOF); zweites Grün = Weiterlesen |
| `J01` läuft weiter ohne Grün | Nach EOF-Read (~`file_blk` 14976) Rate 0 — Rest aus Cache |
| Autoplay zurück auf 1 ohne Grün | Re-Cache-Hit |

---

## 3. Muster je Medium

### A — `bitrate_mix` (B032 / B128 / B320, je ~60 s)

- Mount: alle drei Dateien kurz angefasst; **v1 hörbar ohne Nachlesen** (`B032` nur ~3,1–3,9 s).
- Prefetch (MEASURED, Analyzer):

| playing → next | Lag nach Start | Vorlauf vor Ende |
|---|---:|---:|
| B032 → B128 (früh) | 0,11 s | 59,9 s |
| B128 → B320 (früh) | 0,16 s | 59,8 s |
| B032 → B128 (später) | 52,5 s | 7,5 s |
| B128 → B320 (später) | 59,7 s | 0,3 s |

- `B320`: starke Read-Cluster vor hörbarem Start **und** erneut nahe Start — großer Track, Prefetch + Nachziehen.
- `tail_then_head`: 0/7.

### B — `id3_cover` (COVER1–3 + NOCOVER, je ~26,5 s)

- Gleiches Schema: Track 1 Cache nach Mount; Grün vor 2/3/4 = Prefetch (~22,6 / 49,1 / 75,5 s).
- Beim hörbaren Start von 2/3/4 oft **kein** zweiter Burst (kleiner als `B320`) — Prefetch reicht.
- Cover vs. `NOCOVER`: gleiches Read-Muster; Cover-Größe ändert den Prefetch-Takt hier nicht sichtbar.
- `tail_then_head`: 0/8.

### C — `dur_mix` (D010 / D026 / D060 / D180)

- Read-Volumen skaliert mit Dauer (`D180` ≫ `D010`).
- Prefetch-Vorlauf streut stark mit Trackdauer (Analyzer: Vorlauf bis ~178 s bei langem Track) — Regel eher „weit vor Ende“ als feste Sekunden.

### D — `drive_mix` (Hörbuch + Jazz)

- `K01`–`K04`: jeweils schwere Read-Episoden (hunderte bis ~970 Reads/Datei) — Kapitel werden wirklich vom Stick gezogen, nicht nur Metadaten.
- `J01` („Blaue Stunde“, ~320 s / ~7,6 MiB): ~1425 Reads, drei Zeit-Cluster, Datei bis EOF vorgezogen, danach stundenlanges Hören ohne USB möglich.
- Scan von `drive_mix` (52 Nodes) erzeugt viele kurze Episoden über den Baum; Operator-Fokus war HOERBUCH + JAZZ.

### E — `cache_place` / `cache_b`

- Kurze Live-Sessions mit TRACK1–3; `tail_then_head` bei `cache_place` 2/3 (Platzierung/+Lücke beeinflusst Lesereihenfolge — wie Mediumname nahelegt).

---

## 4. Lab `.88` Offline (Gegenbeispiel)

`20261022-065150_lab_debian_free`: 199 Offline-Records, `upload_status=delivered`, aber `replay_records=0` / `complete=false` → Collector nur Session + `media_layout` + Health, **keine** `bot_*`.  
Ursache: Replay-Pfad, nicht „keine Datei gewählt“ (TUR 110 am Host). Separates Thema zu Live-BMW.

---

## 5. Schlussfolgerungen

1. **NBT hält nach erstem Atom stark im Cache** — hörbarer Track 1 oft ohne Grün.
2. **Prefetch des Nachfolgers ist der Normalfall** — Grün vor Trackwechsel ist erwartbar und in Prefetch-Leads quantifiziert.
3. **Große/hohe Bitrate-Dateien** (`B320`, lange `drive_mix`-Tracks) erzeugen zusätzliche Read-Wellen (Nachziehen / EOF-Vorlauf), ohne Seek zurück an den Dateianfang.
4. **ID3-Cover** ändert in dieser Leiter nicht das Cache/Prefetch-Grundmuster gegenüber `NOCOVER`.
5. Live-TCP auf 0.3.17 trägt die Leiter; Heimfahrt-Flash war in dieser Session **aus** (`autoCapture` erst danach gesetzt).

---

## 6. Nächste Schritte

- [ ] Heimfahrt-Boot: Lila bestätigen, eine echte Fahrt mit `drive_mix` + Checkpoints.
- [ ] Lab-Offline-Replay (`.88`) separat debuggen (`replay_records=0` trotz Records).
- [ ] Optional: `B320`-Doppelburst vs. `COVER*`-Einmal-Prefetch als feste Hypothese (Schwelle Dateigröße/Bitrate).
