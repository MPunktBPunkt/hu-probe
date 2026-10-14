#pragma once

#include <Arduino.h>

/**
 * TinyUSB MSC Gadget Stub.
 * v0.1: noch kein reales Medium — nur State-Hooks für Session/Instrument.
 * Nächster Schritt: virtuelles FAT + Callbacks an ReadInstrument.
 */
class MscGadget {
public:
    void begin();
    void loop();
    bool mounted() const { return mounted_; }
    void setMounted(bool m) { mounted_ = m; }
    const char* identitySerial() const { return serial_; }
    const char* volumeLabel() const { return label_; }
    void setIdentity(const char* serial, const char* label);

private:
    bool mounted_ = false;
    char serial_[21] = "HUPROBE0001";
    char label_[12] = "HUPROBE";
};
