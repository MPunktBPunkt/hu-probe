# BMW S2 — T2 Offline PASS (2026-10-19, FW 0.2.30)

**Fahrzeug:** BMW NBT Evo · Szenario `s2_prefetch` · Profil `bmw_nbt_evo`  
**Probe:** `.89` · Collector lokal `sessions/` · `soft_usb=false` · `media_hide` · `measure_ms=90000`  
**Checkliste:** [`2026-10-18-t2-hold-checklist.md`](2026-10-18-t2-hold-checklist.md)  
**Übergabe:** [`docs/reports/2026-10-19-hu-probe-bmw-s2-t2-offline.md`](../reports/2026-10-19-hu-probe-bmw-s2-t2-offline.md)

## Kurzfazit

1. **T2 offline = FACT** — Session `20261019-075344` Integrity **PASS**, CBW `(318,240)+(558,69)`, covered **318…626**, 158208 B.
2. Fall **A** der T2-Hold-Checkliste erfüllt (P0 geschlossen).
3. Demo-Medium: `TRACK1/2/3.MP3` je **158208 B / 309 LBA**, byte-identisch, Starts LBA 9 / 318 / 627.
4. Operativ: erster Arm `buffer_alloc_failed` → RST nötig; Magenta während Finalize trotzdem Upload **PASS** möglich (ohne RST).
5. OTG-Aus/Ein-Läufe danach kurz (`umount`, 3 bzw. 15 Rec) — kein Atom, kein Gegenclaim.

## Offline-Sessions (heute)

| Session | validate | offline_rec | Atom | Notiz |
|---------|----------|------------:|------|-------|
| (Arm 1) | — | 0 | — | `buffer_alloc_failed` vor RST |
| (Arm 2) | — | 0 | — | Magenta / NEED_RESET, verworfen |
| **`20261019-075344`** | **PASS** | 123 | **T2** `(318,240)+(558,69)` | Magenta gesehen, Finalize ohne RST, `delivered` |
| `20261019-075646` | PASS* | 3 | keines | `umount` (OTG), diagnostisch |
| `20261019-075714` | PASS* | 15 | keines | `umount` (OTG), diagnostisch |

\* Integrity-Flag der Probe bei kurzem Fenster; kein Track-Atom.

## Epistemik

| Claim | Klasse |
|-------|--------|
| T2-Atom offline `(318,240)+(558,69)` / covered 318…626 | **FACT** (`075344`) |
| T1/T2/T3 Dateigröße = 309 LBA auf Demo-FAT | **FACT** (`demo_fat.bin`) |
| Prefetch-Lead / GATE-01 quantitativ | **OPEN** (keine Marker in `075344`; B7 fehlt) |
| Magenta ⇒ immer FAIL | **REJECTED** (dieser Lauf) |

## Nächste Schritte (nicht Feld heute)

1. P0b B7: `capture_t_us` ≠ `replay_t_us`
2. S3 Cache/Replay
3. Prefetch-Lead mit Markern + Timing
