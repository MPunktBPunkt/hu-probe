#include "WebUi.h"
#include "BuildFlags.h"
#include <WiFi.h>

static const char* PAGE =
    "<!DOCTYPE html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>hu-probe</title><style>"
    "body{font-family:system-ui,sans-serif;margin:1.5rem;background:#0f1419;color:#e7ecf1}"
    "h1{font-size:1.4rem;margin:0 0 .5rem} .muted{color:#8b9aab;font-size:.9rem}"
    "button,select,input{font:inherit;padding:.45rem .7rem;margin:.2rem 0;border-radius:6px;border:1px solid #334}"
    "button{background:#2a6;color:#fff;border:none;cursor:pointer} button.sec{background:#345}"
    "pre{background:#1a222c;padding:.8rem;border-radius:8px;overflow:auto;font-size:.8rem}"
    ".row{display:flex;flex-wrap:wrap;gap:.5rem;align-items:center;margin:.6rem 0}"
    "</style></head><body>"
    "<h1>hu-probe</h1><p class=muted>Head-Unit MSC Messsonde · " FW_VERSION "</p>"
    "<div class=row>"
    "<label>Vehicle <select id=v><option value=bmw>bmw</option><option value=mazda_cx30>mazda_cx30</option></select></label>"
    "<label>Scenario <select id=s>"
    "<option value=s0_plug_enum>s0_plug_enum</option>"
    "<option value=s1_eager_eof>s1_eager_eof</option>"
    "<option value=s2_prefetch>s2_prefetch</option>"
    "<option value=s3_cache_hit>s3_cache_hit</option>"
    "<option value=s4_remount>s4_remount</option>"
    "<option value=s5_busy_budget>s5_busy_budget</option>"
    "<option value=s6_identity>s6_identity</option>"
    "<option value=s7_lifecycle>s7_lifecycle</option>"
    "</select></label>"
    "</div>"
    "<div class=row>"
    "<button onclick=start()>Start Session</button>"
    "<button class=sec onclick=stop()>Stop</button>"
    "<button class=sec onclick=mark()>Marker</button>"
    "<button class=sec onclick=cycle()>USB Soft-Cycle</button>"
    "<button class=sec onclick=remount()>Media Remount</button>"
    "</div>"
    "<pre id=st>loading…</pre>"
    "<script>"
    "async function refresh(){const r=await fetch('/api/status');st.textContent=JSON.stringify(await r.json(),null,2)}"
    "async function start(){await fetch('/api/session/start',{method:'POST',headers:{'Content-Type':'application/json'},"
    "body:JSON.stringify({vehicle:v.value,scenario:s.value})});refresh()}"
    "async function stop(){await fetch('/api/session/stop',{method:'POST'});refresh()}"
    "async function mark(){await fetch('/api/marker',{method:'POST',headers:{'Content-Type':'application/json'},"
    "body:JSON.stringify({name:'operator'})});refresh()}"
    "async function cycle(){await fetch('/api/usb/cycle',{method:'POST',headers:{'Content-Type':'application/json'},"
    "body:JSON.stringify({holdMs:600})});refresh()}"
    "async function remount(){await fetch('/api/usb/remount',{method:'POST',headers:{'Content-Type':'application/json'},"
    "body:JSON.stringify({bumpSerial:true})});refresh()}"
    "setInterval(refresh,1000);refresh()"
    "</script></body></html>";

void WebUi::begin(ConfigStore* cfg, Session* session, ReadInstrument* inst,
                  MscGadget* msc, HubClient* hub, TcpStreamer* tcp) {
    cfg_ = cfg;
    session_ = session;
    inst_ = inst;
    msc_ = msc;
    hub_ = hub;
    tcp_ = tcp;

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    server_.on("/api/config", HTTP_GET, [this]() { handleConfigGet(); });
    server_.on("/api/config", HTTP_POST, [this]() { handleConfigPost(); });
    server_.on("/api/session/start", HTTP_POST, [this]() { handleSessionStart(); });
    server_.on("/api/session/stop", HTTP_POST, [this]() { handleSessionStop(); });
    server_.on("/api/marker", HTTP_POST, [this]() { handleMarker(); });
    server_.on("/api/debug/sim-read", HTTP_POST, [this]() { handleSimulateRead(); });
    server_.on("/api/usb/cycle", HTTP_POST, [this]() { handleUsbCycle(); });
    server_.on("/api/usb/remount", HTTP_POST, [this]() { handleUsbRemount(); });
    server_.begin();
    Serial.println("[WEB] :80");
}

void WebUi::loop() {
    server_.handleClient();
}

void WebUi::handleRoot() {
    server_.send(200, "text/html", PAGE);
}

void WebUi::handleStatus() {
    JsonDocument doc;
    doc["fw"] = FW_VERSION;
    doc["fwType"] = FW_TYPE;
    doc["board"] = HUPROBE_BOARD_LABEL;
    doc["ip"] = WiFi.localIP().toString();
    doc["hub_ok"] = hub_ ? hub_->lastOk() : false;
    doc["tcp_ok"] = tcp_ ? tcp_->connected() : false;
    if (msc_) {
        doc["msc_ready"] = msc_->ready();
        doc["msc_plugged"] = msc_->plugged();
        doc["msc_media"] = msc_->mediaPresented();
        doc["msc_soft_attached"] = msc_->usbSoftAttached();
        doc["msc_serial"] = msc_->identitySerial();
        doc["msc_reads"] = msc_->readCount();
        doc["msc_sample_overflow"] = msc_->sampleOverflow();
        doc["plug_count"] = msc_->plugCount();
        doc["unplug_count"] = msc_->unplugCount();
    }
    doc["read_rate_kb_s"] = inst_ ? inst_->readRateKbS() : 0;
    doc["tur_hz"] = inst_ ? inst_->turHz() : 0;
    doc["read_count"] = inst_ ? inst_->readCount() : 0;
    if (session_) {
        JsonObject s = doc["session"].to<JsonObject>();
        session_->toStatusJson(s);
    }
    if (cfg_) {
        doc["vehicle"] = cfg_->vehicleId;
        doc["scenario"] = cfg_->scenarioId;
        doc["collector"] = cfg_->collectorHost + ":" + String(cfg_->collectorPort);
    }
    String out;
    serializeJson(doc, out);
    server_.send(200, "application/json", out);
}

void WebUi::handleConfigGet() {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    if (cfg_) cfg_->toJson(obj);
    String out;
    serializeJson(doc, out);
    server_.send(200, "application/json", out);
}

void WebUi::handleConfigPost() {
    if (!cfg_ || !server_.hasArg("plain")) {
        server_.send(400, "text/plain", "bad request");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain"))) {
        server_.send(400, "text/plain", "json");
        return;
    }
    cfg_->fromJson(doc.as<JsonVariantConst>());
    cfg_->save();
    server_.send(200, "application/json", "{\"ok\":true}");
}

void WebUi::handleSessionStart() {
    String vehicle = cfg_ ? cfg_->vehicleId : "bmw";
    String scenario = cfg_ ? cfg_->scenarioId : "s0_plug_enum";
    String profile = cfg_ ? cfg_->profileId : "bmw_nbt_evo";
    if (server_.hasArg("plain")) {
        JsonDocument doc;
        if (!deserializeJson(doc, server_.arg("plain"))) {
            if (doc["vehicle"].is<const char*>()) vehicle = doc["vehicle"].as<String>();
            if (doc["scenario"].is<const char*>()) scenario = doc["scenario"].as<String>();
            if (doc["profile_id"].is<const char*>()) profile = doc["profile_id"].as<String>();
        }
    }
    if (cfg_) {
        cfg_->vehicleId = vehicle;
        cfg_->scenarioId = scenario;
        cfg_->profileId = profile;
        cfg_->save();
    }
    if (session_) session_->start(vehicle, scenario, profile);
    server_.send(200, "application/json", "{\"ok\":true}");
}

void WebUi::handleSessionStop() {
    if (session_) session_->stop();
    server_.send(200, "application/json", "{\"ok\":true}");
}

void WebUi::handleMarker() {
    String name = "operator";
    String note;
    if (server_.hasArg("plain")) {
        JsonDocument doc;
        if (!deserializeJson(doc, server_.arg("plain"))) {
            if (doc["name"].is<const char*>()) name = doc["name"].as<String>();
            if (doc["note"].is<const char*>()) note = doc["note"].as<String>();
        }
    }
    if (session_) session_->marker(name, note);
    server_.send(200, "application/json", "{\"ok\":true}");
}

void WebUi::handleSimulateRead() {
    if (!inst_) {
        server_.send(200, "application/json", "{\"ok\":false}");
        return;
    }
    static uint32_t simTag = 1;
    uint32_t tag = simTag++;
    inst_->onUsb("attach", 0, "out", 50, "sim");
    inst_->onBotCbw(tag, "2800000007a10000000800", "read10", 1953, 8, 4096, 0x80);
    inst_->onBotData(tag, "in", 4096, 4096, 3800, 1953, "sim", "file");
    inst_->onBotCsw(tag, 0, 0);
    server_.send(200, "application/json", "{\"ok\":true,\"schema\":2}");
}

void WebUi::handleUsbCycle() {
    uint16_t holdMs = 600;
    if (server_.hasArg("plain")) {
        JsonDocument doc;
        if (!deserializeJson(doc, server_.arg("plain"))) {
            if (!doc["holdMs"].isNull()) holdMs = doc["holdMs"].as<uint16_t>();
        }
    }
    bool ok = msc_ && msc_->softCycle(holdMs);
    JsonDocument out;
    out["ok"] = ok;
    out["action"] = "usb_cycle";
    out["holdMs"] = holdMs;
    out["note"] = "soft_usb=D+_pullup_not_VBUS";
    String body;
    serializeJson(out, body);
    server_.send(ok ? 200 : 500, "application/json", body);
}

void WebUi::handleUsbRemount() {
    bool bump = true;
    if (server_.hasArg("plain")) {
        JsonDocument doc;
        if (!deserializeJson(doc, server_.arg("plain"))) {
            if (!doc["bumpSerial"].isNull()) bump = doc["bumpSerial"].as<bool>();
        }
    }
    if (msc_) msc_->remountMedia(bump);
    JsonDocument out;
    out["ok"] = true;
    out["action"] = "media_remount";
    out["serial"] = msc_ ? msc_->identitySerial() : "";
    String body;
    serializeJson(out, body);
    server_.send(200, "application/json", body);
}
