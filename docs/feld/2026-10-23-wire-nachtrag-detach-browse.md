# Wire-/Feld-Nachtrag: Detach-Dauer, RST/OTG, folders_30-Browse

**Stand:** 2026-10-23 · Auto-ESP `.89` · FW **0.3.19** · Operator + Status-API  
**Bezug:** [`2026-10-23-wire-nachtrag-074319-074455.md`](2026-10-23-wire-nachtrag-074319-074455.md) · Multi-Review  
**Medium Detach-Leiter:** `cache_a_pin` · **Browse:** `folders_30`

Keine vollständige READ10-Nachrechnung aller Live-TCP-Sessions dieses Nachmittags; Aussagen zu Detach/Browse sind **Operator-FACTs** + API-Timing, konsistent mit dem Zwei-Pfad-/Zwei-Schichten-Modell.

---

## 1. Ampel

| Aussage | Status |
|---|---|
| Soft-Cycle `holdMs` 0,2 / 1 / 3 / 10 / **30 s** → jeweils **Resume** (Track-Position) | 🟢 FACT |
| ESP-RST (kurz), gleiches Medium → **Resume** (bei T3), kein Fresh-Start bei 1 | 🟢 FACT |
| Physisch OTG ~30 s ab → Resume | 🟢 FACT |
| OTG **~99 s** ab + Zündung kurz aus/ein → weiterhin **Resume bei T3** | 🟢 FACT |
| Fresh `32+128+6` / Start bei 1 **nicht** durch Soft-Detach ≤30 s, ESP-RST oder ~99 s OTG+Zündung reproduziert | 🟢 STRONG |
| folders_30: In-Session Browse-Cache (Album einmal Miss, Rückkehr Hit) | 🟢 FACT |
| Nach kurzem Cycle: Browse/Titel-Cache **leer** (jeder Titel neu), Navigator **merkt** Album 3 / Titel | 🟢 FACT |
| Resume-Position ≠ Browse-/Datei-Cache (Zwei Schichten) | 🟢 STRONG |

---

## 2. Detach-Dauer-Leiter (`cache_a_pin`, Soft-Cycle)

Stimulus: `POST /api/usb/cycle` mit `holdMs`, Serial gepinnt `HP0A01`.

| holdMs | Operator |
|-------:|----------|
| 200 | neu eingelesen, **v3 läuft** (Resume) |
| 1000 | Autoplay→1, Remount, **bei 1 weiter** |
| 3000 | Remount, **bei 1 weiter**, Autoplay 2 |
| 10000 | Remount, **bei 2 weiter** |
| 30000 | Remount, **Ende 2 weiter**, Autoplay 3 |

**Schluss:** Im getesteten Fenster ist Soft-Detach-Dauer **kein** Schalter Cycle-Resume → Fresh-Scan.

---

## 3. RST und physischer OTG-Verlust

| Stimulus | Dauer / Kontext | Operator |
|---|---|---|
| ESP-RST | bei Track 3, Medium unverändert `cache_a_pin` | Remount, **weiter bei 3** (nicht Start bei 1) |
| OTG physisch ab/an | ~30 s (Schätzung), bei 3 | **weiter bei 3** |
| OTG physisch ab | **99 s** gemessen (`plug=false` 16:49:33–16:51:13) + Zündung kurz aus/ein, dann OTG ein | **weiter bei 3** |

**Korrektur am Arbeitsmodell:** Fresh-Scan (`074319`: `32+128+6`) ist **nicht** gleichbedeutend mit „ESP-RST“ oder „USB länger weg“ in diesem Bereich. `074319` bleibt Fresh-Wire-Beleg, aber kausal **RST+Serial/Kontext vermischt** und nicht durch reine Abwesenheitsdauer ≤~100 s reproduziert.

Offen: was Fresh wirklich auslöst (Minuten, Kaltstart HU, anderer Host-Kontext, …).

---

## 4. folders_30 — Browse-Cache vs. Navigator

Medium gewechselt auf `folders_30` (Serial policy → u. a. `HP0002`, nach Cycle `HP0003`).

### 4.1 In-Session (kein Remount)

| Aktion | Lesen? |
|---|---|
| Einstieg / Weiter in Album 3, alle Titel | (Session-Start) |
| SUB gewählt, Titel angetippt; eine Ebene hoch | **kein** erneutes Einlesen (Hit) |
| Album 2 erstmals | **gelesen** (Miss); Titel getippt |
| zurück Album 3 / SUB | **Hit** |
| Album 1 erstmals | **gelesen** (Miss); alle Titel getippt |
| danach Album 2 / 3 / SUB | **kein** Neu-Einlesen |

**FACT:** Browse-/Titel-Cache füllt sich pro Ordner in der Mount-Epoche; Rückkehr = Hit.

### 4.2 Nach kurzem Cycle (800 ms)

- Serial neu (`HP0003`)
- Operator: **liest bei jedem Titel neu** → Browse-Cache **flush**
- Gleichzeitig: **bei Album 3 und Titel weitergemacht** (Navigator/Resume gemerkt)
- Album 2 und Album 1 danach gewählt → jeweils neu gelesen (Miss; Cache nach Cycle leer)

**STRONG:** Zwei Schichten

```text
Navigator / Playlist- / Ordner-Position   ← überlebt kurzen Cycle
Browse- / Datei-Titel-Cache               ← wird durch kurzen Cycle geleert
```

Konsistent mit Identity-Nachmittag: Cycle → oft Resume-Position + trotzdem Datei-Reads (`tail_then_head` / Vollatom), nicht Fresh-`32+128+6`.

---

## 5. Aktualisiertes Modell (kurz)

```text
USB weg / Cycle / kurzer RST / OTG ≤~100s
        │
        ├─ Navigator-Position: oft erhalten (Track/Album/Titel)
        ├─ Browse-/Body-Cache: oft weg → Neu-Lesen der Titel
        └─ Fresh-Bibliotheks-Scan 32+128+6: selten;
             in dieser Session nicht durch Dauer/RST/OTG+Zündung erzwungen
```

---

## 6. Nächste Tests

1. Fresh gezielt provozieren (z. B. HU länger ohne Medium, Kaltstart) — nicht weitere Soft-Holds ≤30 s  
2. Größenleiter (ob Browse-Hit/Resume-Atom bei großen Dateien anders)  
3. B59 Upload-Stall (Infrastruktur)  
4. GATE-01 Audible  

Identity Serial/Label/Vol und Soft-Detach-Leiter ≤30 s: **abgeschlossen genug**.
