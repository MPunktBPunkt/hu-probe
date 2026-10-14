#include "MscGadget.h"
#include <cstring>

void MscGadget::begin() {
    mounted_ = false;
    Serial.println("[MSC] gadget stub ready (no TinyUSB yet)");
}

void MscGadget::loop() {
    // TinyUSB task loop kommt mit ReadInstrument.
}

void MscGadget::setIdentity(const char* serial, const char* label) {
    if (serial) {
        strncpy(serial_, serial, sizeof(serial_) - 1);
        serial_[sizeof(serial_) - 1] = '\0';
    }
    if (label) {
        strncpy(label_, label, sizeof(label_) - 1);
        label_[sizeof(label_) - 1] = '\0';
    }
}
