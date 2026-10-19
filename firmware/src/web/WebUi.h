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
    void handleOfflineStart();
    void handleOfflineStop();
    void handleExperiments();
    void handleExperimentStart();
    void handleMediaGet();
    void handleMediaPresets();
    void handleMediaVariant();
    void handleMediaMutate();
    void handleMediaNotify();
    void handleStallGet();
    void handleStallPost();
    void handleSequences();
    void handleSequenceStart();
    void handleSequenceCmd(const char* cmd);
    void sendJson(int code, JsonDocument& doc);
    void handleOtaUpload();
    void handleOtaDone();

    /** Shared arm path for /api/offline/start and /api/experiment/start. */
    void armOfflineCapture(const String& vehicle, const String& scenario,
                           const String& profile, bool softUsb, uint32_t measureMs,
                           const String& endStrategy, const char* const* markers,
                           uint8_t markerCount, const char* experimentId,
                           bool endOnUnmount = true, bool freeMarkers = false);

    WebServer server_{80};
    ConfigStore* cfg_ = nullptr;
    Session* session_ = nullptr;
    ReadInstrument* inst_ = nullptr;
    MscGadget* msc_ = nullptr;
    HubClient* hub_ = nullptr;
    TcpStreamer* tcp_ = nullptr;
};
