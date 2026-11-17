# GO — Größenleiter Heimkehr 2026-11-17

**Plan:** `2026-11-17a` (Zähler bleibt; **keine** neue Plan-ID)  
**FW:** 0.3.41 · **Probe:** `.89` · **Collector:** `192.168.178.187:9100` (läuft)

## Operator-Meldung (unterwegs)

| Wann | Was |
|------|-----|
| Morgen Lauf 1 | komplett durch |
| Morgen Lauf 2 | Abbruch mit BOOT lang |
| Heimfahrt | Messung läuft · HU Autoplay `HP Track1/2/3` |

**Zähler-Annahme** (Advance bei Host-Mount): `s158` → `s1m` (abgebrochen, aber gezählt) → aktuell **`s4m`** → als Nächstes **`s16m`**.  
Nach Upload an `/api/status` → `drive.field_plan` prüfen (`runs_done` / `next_job`).

## Ankunft (Upload zuerst)

1. Heimnetz gewinnen lassen (**kein Hotspot**).
2. Upload idle abwarten (LED blau blink → idle). **Kein RST** während Upload.
3. Erwartete Sessions u. a.:
   - `fp_2026-11-17a_s158_r1` · media `size_158`
   - ggf. kurze/abgebrochene `…_s1m_r1`
   - Heimfahrt `…_s4m_r1` · media `size_4m`
4. Lab: `integrity PASS`, `replay_records≈offline_records`.

`arm_at_home=false` im Repo → nach Upload **kein** Sofort-Arm im Heimnetz (Absicht).

## Nächste Messung: `s16m` (10 min, fine)

Sobald Upload idle und `next_job=s16m` (sonst Lab melden):

1. Soft-Cycle (BOOT doppelt), Medium sichtbar.
2. **TRACK1** wählen, Autoplay T1→T2→T3 (nicht Podcast).
3. Messung scharf:
   - **Variante A (Parkplatz/Heim):** Lab setzt kurz `arm_at_home=true`, dann RST bei steckender HU · **oder**
   - **Variante B (kurz fahren):** Zündung/RST **außerhalb** Heimnetz → Plan arm’t `s16m`.
4. LED **lila** ≈ 10 min · danach Soft-Detach ok.
5. Magenta → Soft-Cycle. **Kein RST** mitten in lila Phase.

## Lab nach Upload (Copy-Paste)

```bash
# neue Sessions seit Snapshot
comm -13 <(sort docs/feld/artifacts/20261117-groessenleiter/pre_sessions.txt) \
         <(ls -1 sessions | sort)

# Status Sonde
curl -sS http://192.168.178.89/api/status | python3 -c \
  'import json,sys; d=json.load(sys.stdin); print(d.get("drive",{}).get("field_plan")); print("media",(d.get("media") or {}).get("variant"))'

# Analyse einer neuen Session
python3 tools/analyze_session.py sessions/<NEU>
```

## LED

| LED | Bedeutung |
|-----|-----------|
| lila fest | Messung läuft |
| grün fest | HU liest |
| blau blink | Upload — nicht RST |
| magenta blink | Soft-Cycle |
| gelb/orange blink | Trip im Flash, RST ok |
