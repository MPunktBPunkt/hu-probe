# hu-probe BMW S2 – korrigiertes Track-/CBW-Muster

**Übergabe nach Rohdaten-Korrektur, 2026-10-18**

| | |
|--|--|
| Repository | `MPunktBPunkt/hu-probe` |
| FW | 0.2.30 |
| BMW | NBT Evo |
| Status | Drei Offline-PASS bestätigt; Track-Geometrie anhand der Rohdaten korrigiert |
| Feld-Checkliste | [`docs/feld/2026-10-18-t2-hold-checklist.md`](../feld/2026-10-18-t2-hold-checklist.md) |
| Feld-Rohbericht | [`docs/feld/2026-10-18-bmw-s2-abend-offline.md`](../feld/2026-10-18-bmw-s2-abend-offline.md) |
| Multi-KI (vor Korrektur) | [`docs/feld/2026-10-18-bmw-s2-0.2.30-multi-ki-review.md`](../feld/2026-10-18-bmw-s2-0.2.30-multi-ki-review.md) |
| Audit (Meta ohne Roh-Git) | [`audits/hu-probe-bmw-s2-abend-offline-2026-10-18/`](../../audits/hu-probe-bmw-s2-abend-offline-2026-10-18/) · [`session-summaries.json`](../../audits/hu-probe-bmw-s2-abend-offline-2026-10-18/session-summaries.json) |

---

## 1. Wichtigste Korrektur

Die bisherige Interpretation

> 39 × 4096 B = 159744 B und 312 covered LBA

ist **REJECTED**.

Die Event-Anzahl 39 war korrekt, die Umrechnung auf übertragene Bytes war falsch.

Rohdaten:

```
38 × 4096 B = 155648 B
 1 × 2560 B =   2560 B
----------------------
               158208 B

158208 / 512 = 309 LBA
```

Track-Geometrie:

> **309 LBA = 158208 B pro Track-Burst.**

Der letzte Transfer ist kein vollständiger 4-KiB-Block, sondern **2560 B = 5 LBA**.

Damit verschwindet:

```
312 − 309 = 3 LBA
```

vollständig. Die Hypothese „309 LBA Nutzdaten + 3 LBA Metadaten“ ist **REJECTED** — Zählartefakt, weil der letzte Chunk der 69er-CBW nur 5 LBA groß ist.

---

## 2. Das echte Track-Muster

Jeder beobachtete Trackzugriff besteht aus **zwei benachbarten READ10-CBWs**:

| Track | erster CBW | zweiter CBW | Summe |
|-------|------------|-------------|------:|
| T1 | `(9, 240)` | `(249, 69)` | 309 LBA |
| T2 | `(318, 240)` | `(558, 69)` | 309 LBA |
| T3 | `(627, 240)` | `(867, 69)` | 309 LBA |

Bedeutung:

```
(head, 240)
(head + 240, 69)
→ 240 + 69 = 309 LBA
```

Heads: T1=9, T2=318, T3=627 → Abstände `318−9 = 627−318 = 309`.

```
Track 1:  9 ──────────── 248 | 249 ─────── 317
Track 2:  318 ────────── 557 | 558 ─────── 626
Track 3:  627 ────────── 866 | 867 ─────── 935
```

Die Trackbereiche schließen lückenlos an:

```
T1: 9…317 · T2: 318…626 · T3: 627…935
```

**Primäre HU-Eigenschaft (nicht „39er“):**

> Track-Atom = `(head,240) + (head+240,69)` = 309 LBA = 158208 B

---

## 3. Die 39 Events — erklärt, nicht mysteriös

```
240 LBA / 8 LBA = 30 Reads
69 LBA = 8×8 LBA + 5 LBA = 9 Reads
--------------------------------
                           39 Reads
```

Also: **30 + 9 = 39** = 38×4 KiB + 1×2560 B.

Damit stimmen gleichzeitig Event-Anzahl, Bytezahl, Covered Range und CBW-Geometrie.

---

## 4. Offline-PASS (Abend)

| Session | validate | Band | CBW-Atom (korrigiert) | Operator |
|---------|----------|------|----------------------|----------|
| `175203` | PASS | T3 | `(627,240)+(867,69)` · covered **627…935** | Autoplay T3 |
| `212136` | PASS | T1* | `(249,69)+(9,240)` **tail_then_head** · kinds file34/dir4/fat1 | Start T3 Cache → Hold 1 |
| `212635` | PASS | T3 | `(627,240)+(867,69)` | geplant T2 (1→AP2→Hold2); Buffer = T3 |

\* T1*: CBW-Mengen = T1-Atom, Reihenfolge `tail_then_head` — belegt in [`session-summaries.json`](../../audits/hu-probe-bmw-s2-abend-offline-2026-10-18/session-summaries.json).

**REJECTED gegenüber älteren Berichten:** T3 covered 627…938 · Burst 312 LBA · „3 Meta-LBA“ · „39 × 4 KiB“.

**Replikation ohne Roh-Git:** kind-Counts, Start-Listen und CBW-Paare liegen im Audit (Antwort auf Mistral M2/B10).

---

## 5. Hold-Burst `212136`

Vorher: rätselhafte Nichtmonotonie ~249…241 mit 34 file / 4 dir / 1 fat.

Rohdaten-Abgleich (Audit):

| Feld | Wert |
|------|------|
| `kind_counts` | `{file: 34, dir: 4, fat: 1}` |
| `read10_cbws` | `(249, 69)` dann `(9, 240)` |
| form | **`tail_then_head`** |
| covered | 9…317 (309 LBA) |
| bytes | 158208 |
| starts | 249…313 dann Sprung zu 9…241 (nicht-monoton) |

| | |
|--|--|
| T1a (normal) | head → 240, dann head+240 → 69 |
| T1b (Hold) | **dieselbe Geometrie**, Reihenfolge tail→head + dir/fat |

Klasse: CBW-Geometrie + Split **FACT**; Ursache der Reihenfolge weiter **INFERENCE**.

---

## 6. Was `212635` bedeutet

| | |
|--|--|
| **FACT** | Während eines als T2-Hold protokollierten Laufs wurde ein vollständiges **T3-Atom** aufgezeichnet |
| **Nicht bewiesen** | Prefetch von T3 während Hold T2 |

Offene Erklärungen: Autoplay vor Hold · Prefetch · früherer Cache-Miss · Timing Operator≠USB.

---

## 7. Epistemik nach Korrektur

| Aussage | Status |
|---------|--------|
| Drei Offline-PASS | **FACT** |
| T1/T2/T3 Heads = 9/318/627 | **FACT** |
| Track-Abstand = 309 LBA | **FACT** |
| Track-Atom = 240 + 69 LBA | **FACT** (T1 Live, T3 Live+Offline; T2 Live, Offline **OPEN**) |
| Trackgröße = 309 LBA / 158208 B | **FACT** |
| 39 Events = 30 + 9 Reads | **FACT** |
| letzter Transfer = 2560 B / 5 LBA | **FACT** |
| T3 covered = 627…935 | **FACT** |
| T3 covered = 627…938 | **REJECTED** |
| 312 LBA Burst / „3 Meta“ / 39×4 KiB | **REJECTED** |
| T1-Hold = gleiche T1-CBW-Geometrie `(9,240)+(249,69)` | **FACT** (Audit `212136`) |
| `212136` CBW-Reihenfolge = `tail_then_head` | **FACT** |
| Ursache der Hold-Reihenfolge / dir·fat am Kopf | **INFERENCE** |
| `212635` = T3 während Hold-2-Protokoll | **FACT** |
| `212635` = Prefetch | **UNKNOWN** / Kandidat |
| T2 offline | **OPEN** |
| Prefetch-Lead / GATE-01 | **OPEN** |

---

## 8. Offene wissenschaftliche Frage

Nicht mehr: „Warum liest die HU 39 Reads?“

Sondern:

> Warum und wann fordert die HU das **309-LBA-Track-Atom** an — insbesondere kann sie das Atom des **nächsten** Tracks bereits während eines gehaltenen aktuellen Tracks anfordern?

Das entscheidet Prefetch / Play-Intent / Cache und den PiDrive-F0-Pfad.

---

## 9. Auftrag an die nächste KI / den Operator

**P0:** Direkt Track 2 starten und halten — **keine** Navigation T1→T2.

Siehe Feld-Checkliste: [`docs/feld/2026-10-18-t2-hold-checklist.md`](../feld/2026-10-18-t2-hold-checklist.md)

| Ziel | Signal |
|------|--------|
| 1 | T2-CBW `(318,240)+(558,69)` → T2 offline **FACT** |
| 2 | Stattdessen `(627,240)+(867,69)` nach `hold_2` → Prefetch-Kandidat massiv gestärkt |
| 3 | FW: `capture_t_us` ≠ `replay_t_us` → GATE-01 quantitativ |

---

## 10. Endstatus (Skizze)

```
                 309 LBA
        <------------------------>

T1     |========================|
T2                              |========================|
T3                                                        |========================|
       9                       318                       627
```

Drei Tracks = regelmäßige, lückenlose 309-LBA-Struktur. Das erklärt das frühere 39er-Muster und beseitigt die falsche 312-LBA-/„3 Meta“-Hypothese.
