# Kontrollierte Medienvariation

Ziel: Der HU genau **eine** Variable pro Vergleich präsentieren, damit Cache-/Identity-Hypothesen (HU-A2) trennbar werden.

## Dimensionen

| Dimension | Beispiel A | Beispiel B | Frage |
|-----------|------------|------------|-------|
| USB Serial | `PD0106` | `PD0107` | Identity-Cache? |
| Volume Label | `Radio BOB` | `Radio BOB` | Label irrelevant? |
| Dateiinhalt | Byte-identisch | Content geändert | Content-Cache? |
| FAT-Platzierung | LBA-Map A | gleiche Datei, andere Cluster | LBA-Cache? |
| Dateiname | `track.mp3` | `track.mp3` | Metadata-Cache? |

## Matrix (erste Kampagne)

1. **ID-only:** Serial ±1, Label/Datei/LBA gleich → S6 Identity  
2. **Label-only:** Label geändert, Serial/Datei gleich  
3. **Content-only:** gleiche Identity, Payload geändert  
4. **Placement-only:** gleiche Identity+Content, andere Cluster-Kette  
5. **Replug same:** A→detach→A (S4 Remount)  
6. **Replug ID-flip:** A→detach→B (Serial)

Jede Zelle = eigene Session mit `probe_identity` in `session.json` und Marker `media_variant=<id>`.

Firmware: `MscGadget::setIdentity` + geplante Slot/FAT-Builder (noch Stub). Analyse niemals NBT-Erwartungen auf Mazda übertragen.
