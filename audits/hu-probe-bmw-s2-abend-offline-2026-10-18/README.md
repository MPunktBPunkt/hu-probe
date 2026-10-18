# Audit — BMW S2 Abend Offline (2026-10-18)

**Zweck:** Meta-Summen der drei Offline-PASS-Sessions im Repo, damit Claims zu
kind-Counts, Start-Listen und CBW-Atomen **ohne** vollständige `events.jsonl`
(Collector-lokal, typisch gitignored) extern prüfbar sind.

| Artefakt | Inhalt |
|----------|--------|
| [`session-summaries.json`](session-summaries.json) | kind-Counts, `read_lba_starts`, `read10_cbws`, Track-Atome, Covered |
| [`summarize_sessions.py`](summarize_sessions.py) | Replikation aus lokalem `sessions/` |

## Sessions

| short | session_id | Atom | kinds | Covered |
|-------|------------|------|-------|---------|
| `175203` | `20261018-175203_bmw_s2_prefetch` | `(627,240)+(867,69)` head_then_tail | file 39 | 627…935 |
| `212136` | `20261018-212136_bmw_s2_prefetch` | `(249,69)+(9,240)` **tail_then_head** | file 34 · dir 4 · fat 1 | 9…317 |
| `212635` | `20261018-212635_bmw_s2_prefetch` | `(627,240)+(867,69)` head_then_tail | file 39 | 627…935 |

Alle drei: `read_bytes_sum = 158208` (= 309 LBA).

## Replikation

```bash
# Rohdaten müssen lokal liegen (Collector-Sessions):
ls sessions/20261018-212136_bmw_s2_prefetch/events.jsonl

python3 audits/hu-probe-bmw-s2-abend-offline-2026-10-18/summarize_sessions.py
# → schreibt session-summaries.json neu
```

Ohne Roh-Git: `session-summaries.json` allein reicht für M2/B10-Nachrechnung
(34/4/1-Split, Start-Sequenzen, CBW-Paare, Covered 627…935).

## Verweise

- Übergabe: [`docs/reports/2026-10-18-hu-probe-bmw-s2-abend-offline.md`](../../docs/reports/2026-10-18-hu-probe-bmw-s2-abend-offline.md)
- Feld: [`docs/feld/2026-10-18-bmw-s2-abend-offline.md`](../../docs/feld/2026-10-18-bmw-s2-abend-offline.md)
