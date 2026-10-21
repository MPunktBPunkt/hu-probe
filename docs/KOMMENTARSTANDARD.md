# Kommentarstandard Firmware (Doxygen)

Gilt für alle `.h`/`.cpp` unter `firmware/src/` und `firmware/include/`. Kommentare sind
**englisch** (wie Bezeichner und bestehende Code-Kommentare), Format ist **Doxygen im
Javadoc-Stil**. Die Doku wird mit `doxygen firmware/Doxyfile` erzeugt; fehlende Kommentare
erscheinen als Warnung (vergleichbar StyleCop SA1600).

## Wo steht der Kommentar?

| Fall | Ort |
|---|---|
| Klasse, Struct, Enum, Namespace | direkt über der Deklaration im Header |
| Methode / freie Funktion mit Deklaration im Header | über der Deklaration im Header (nicht zusätzlich im `.cpp`) |
| Funktion nur im `.cpp` (`static`, anonymer Namespace, ISR-/Task-Callback) | über der Definition im `.cpp` |
| Inline-Funktion im Header (Definition = Deklaration) | über der Funktion |
| Überschriebene virtuelle Methode ohne neues Verhalten | `/** @copydoc Base::method */` oder kurzer `@brief` |

## Datei-Kopf

Jede Datei beginnt (bei Headern direkt nach `#pragma once`) mit:

```cpp
/**
 * @file OfflineCapture.h
 * @brief RAM-buffered offline capture of USB/SCSI activity while WiFi is off.
 */
```

## Funktions-Kopf (Standard)

```cpp
/**
 * @brief Arm the offline capture: stop the live session, switch WiFi off, allocate the buffer.
 *
 * Optional details: preconditions, side effects, constraints that the code cannot show
 * (timing, ISR/task context, flash cache, ownership).
 *
 * @param session  Live session to stop; must outlive the capture.
 * @param msc      MSC gadget used for soft re-enumeration.
 * @return true if the buffer was allocated and the capture is armed.
 * @note  Called from the Arduino loop task only.
 */
bool arm(Session* session, MscGadget* msc);
```

Regeln:

1. `@brief` ist Pflicht: ein Satz, Verb im Imperativ oder Präsens („Return …“, „Start …“), endet mit Punkt.
2. `@param` für **jeden** Parameter, in Signatur-Reihenfolge, Name exakt wie im Code.
   Ausgabeparameter: `@param[out] err`, Ein-/Ausgabe: `@param[in,out]`.
3. `@return` bei jedem Rückgabewert außer `void`; bei `bool` beide Fälle nennen, wenn nicht trivial.
4. Optional: `@note` (Kontext/Threading), `@warning` (Gefahr, z. B. Flash-Schreiben sperrt Cache),
   `@see` (verwandte Funktion), `@pre`/`@post`.
5. Bestehende Erklärungen (Feldtest-Bezüge wie „B7:“, „0.2.31“, Messregeln) **bleiben erhalten** und
   wandern in den Detailteil.
6. Triviale Getter/Setter ohne Parameter dürfen einzeilig sein:
   `/** @brief Number of READ10 commands since boot. */`
7. Mitglieder von Enums/Structs bei Bedarf mit `///<` am Zeilenende.
8. Vor jedem `/**`-Block steht eine Leerzeile, damit Kommentar und Deklaration als Einheit
   erkennbar sind. Ausnahme: direkt nach `{` oder nach `public:` / `private:` / `protected:`.
9. Kein Kommentar wiederholt nur den Namen („Gets the count.“ für `count()`): Einheit, Bereich oder
   Bedeutung nennen.

## Doku erzeugen

```bash
cd firmware
doxygen Doxyfile          # HTML nach firmware/build/docs/html/index.html
```

`WARN_IF_UNDOCUMENTED` und `WARN_NO_PARAMDOC` sind an; die Warnungen landen in
`firmware/build/docs/doxygen-warnings.log`.
