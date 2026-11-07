# Datenqualität, Proxmox-USB, Parameter (2026-10-14)

## Kurzurteil

| Frage | Antwort |
|-------|---------|
| Sind die alten Lab-Rohdaten auswertbar? | **Nein als Baseline** — falsches Profil, frühe FW, t_us-Bugs, dünne/incomplete Läufe |
| Ist das Instrument reif genug für Proxmox-USB? | **Ja für Lab-Referenz** (FW ≥0.2.7, Schema v2, Soft-USB + Remount + Host-Reads) |
| GATE-01 fertig? | **Nein** — Stufe A (Synth-BOT) ok; Stufe B (echter Bus-CBW) offen |
| BMW-Feld löschen? | **Nein** — einzige Fahrzeug-Evidenz; epistemisch getrennt von Lab |

## Qualitätsmatrix (vor Purge)

| Klasse | Sessions | Urteil |
|--------|----------|--------|
| JUNK | frühe Lab mit `profile=bmw_nbt_evo`, incomplete, 0.2.2-Lab | nicht baseline-fähig |
| ARCHIVE | 0.2.6 Lab (43 Reads, aber 43× t_us-Regression) | historisch, nicht mischen |
| THIN | 0.2.7 ohne Soft-USB / ohne Stop | verwerfen |
| OK_LAB | 0.2.7 Soft+Remount, t_us monoton | instrumentell ok, wenige Reads |
| KEEP_BMW | `20261014-081637_bmw_s0_plug_enum` | Feld-FACT; Validator FAIL (Seq 249–250, Drain-t_us) bewusst dokumentiert |

**Purge:** alle Lab-/Stub-Sessions lokal gelöscht; BMW-Session behalten. Git: vorläufige Lab-Sessions aus dem Index entfernt.

## Proxmox-USB vermessen?

**Ja — derselbe Pfad wie `lab_debian`:** ESP `.88` hängt am Proxmox-Host (Genesys-Hub `2-3.2`), ist in die Debian-LXC durchgereicht als `/dev/sda` (Model `HU_PROBE`, 512 KiB).

Messbar mit FW 0.2.7:

1. Soft-USB-Cycle + Media-Remount (Enum)  
2. Aktive Host-Reads (`dd if=/dev/sda …`) für Xfer/Gap unter Last  
3. Identity: `vehicle=lab_debian`, `usb_host_kind=linux_debian`, MAC `.88`

Nicht messbar ohne GATE-01B: Roh-CBW vom Bus, Control-Transfers als Host-Sniff.

**Blocker (Stand 13:55):** Lab `.88` nach Soft-Cycle/dd WiFi-tot (MSC sichtbar). Hub-`authorized` reicht nicht; kurz ~13:20 wieder online (unbrauchbare Session `2092` verworfen), danach erneut offline. Neuaufnahme Lab aufgeschoben — Auto-Feld hat Vorrang.

## Parameter (bis Power-Cycle / Neuaufnahme)

### Lab / Proxmox USB-Host (Instrument-Referenz)

Quellen: ausgewertete 0.2.6/0.2.7 Lab-Läufe + konsistente Gap-Cluster in Junk-Labs (~4 ms). Nach Neuaufnahme ersetzen.

| Parameter | Wert | Epistemik |
|-----------|------|-----------|
| Host | Proxmox → Genesys Hub → ESP MSC → LXC `/dev/sda` | FACT |
| `usb_host_kind` / role | `linux_debian` / `lab_reference` | FACT |
| Read-Größe Mode | **4096** (auch 512) | FACT |
| Gap-p50 (Pace &lt;10 s) | **~4,0–4,3 ms** (4136…4336 µs) | FACT Lab |
| Soft detach→attach | **~0,30 s** | FACT |
| BOT-Synth | CBW→DATA→read→CSW, `synthesized=true` | FACT ≥0.2.7 |
| Capture (reif) | dropped=0, seq lückenlos, t_us monoton | FACT ≥0.2.7 |
| Eager/Prefetch/TUR-Hz | nicht bestimmt | UNKNOWN |
| Echtes CBW | nicht | UNKNOWN (GATE-01B) |

### BMW NBT (Feld, getrennt — nicht mit Lab mischen)

Session `20261014-081637_bmw_s0_plug_enum`, FW 0.2.2:

| Parameter | Wert | Epistemik |
|-----------|------|-----------|
| Read-Mode | **4096** dominant | FACT |
| Gap-p50 | **3724 µs** | FACT |
| 22-Read-Grundmuster | 6/8 nicht-leere Fenster | FACT |
| Seq / Drain-t_us | Lücken 249–250; Drain-Artefakt Fenster 7 | FACT Limitation |
| Eager Body-Stream | nicht prüfbar (256 KiB Medium) | UNKNOWN |

## Nächster Schritt

1. Lab-ESP **stromlos** stecken (nicht nur Soft-USB)  
2. Drei Wiederholungen: Soft+Remount + `dd` 4k/512, FW 0.2.7  
3. Fingerprint → `profiles/lab_debian.json` expectations überschreiben  
4. BMW erst nach OTA ≥0.2.7 neu messen, wenn Feld-Baseline erneuert werden soll
