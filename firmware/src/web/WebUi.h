#pragma once

#include <WebServer.h>
#include "core/ConfigStore.h"
#include "session/Session.h"
#include "msc/ReadInstrument.h"
#include "msc/MscGadget.h"
#include "core/HubClient.h"
#include "net/TcpStreamer.h"

class WebUi {
public:
    void begin(ConfigStore* cfg, Session* session, ReadInstrument* inst,
               MscGadget* msc, HubClient* hub, TcpStreamer* tcp);
    void loop();

private:
    void handleRoot();
    void handleStatus();
    void handleConfigGet();
    void handleConfigPost();
    void handleSessionStart();
    void handleSessionStop();
    void handleMarker();
    void handleSimulateRead();
    void handleUsbCycle();
    void handleUsbRemount();
    void handleOtaUpload();
    void handleOtaDone();

    WebServer server_{80};
    ConfigStore* cfg_ = nullptr;
    Session* session_ = nullptr;
    ReadInstrument* inst_ = nullptr;
    MscGadget* msc_ = nullptr;
    HubClient* hub_ = nullptr;
    TcpStreamer* tcp_ = nullptr;
};
