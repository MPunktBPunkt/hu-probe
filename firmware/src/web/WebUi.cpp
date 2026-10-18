#include "WebUi.h"
#include "BuildFlags.h"
#include "capture/OfflineCapture.h"
#include "capture/ExperimentProfiles.h"
#include <Update.h>
#include <WiFi.h>

static const char* PAGE =
    "<!DOCTYPE html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>hu-probe</title><style>"
    "body{font-family:system-ui,sans-serif;margin:1.2rem;background:#0f1419;color:#e7ecf1;max-width:52rem}"
    "h1{font-size:1.35rem;margin:0 0 .25rem} h2{font-size:1.05rem;margin:1.1rem 0 .4rem;color:#9ecbff}"
    ".muted{color:#8b9aab;font-size:.88rem} .ok{color:#6d6}.fail{color:#e66}.warn{color:#dc6}"
    "button,select,input{font:inherit;padding:.45rem .7rem;margin:.15rem 0;border-radius:6px;border:1px solid #334}"
    "button{background:#2a6;color:#fff;border:none;cursor:pointer} button.sec{background:#345}"
    "button:disabled{opacity:.45;cursor:not-allowed}"
    "pre{background:#1a222c;padding:.7rem;border-radius:8px;overflow:auto;font-size:.78rem}"
    ".row{display:flex;flex-wrap:wrap;gap:.5rem;align-items:center;margin:.45rem 0}"
    ".card{background:#161d26;border:1px solid #2a3542;border-radius:10px;padding:.85rem 1rem;margin:.7rem 0}"
    ".bar{height:8px;background:#243041;border-radius:4px;overflow:hidden;margin:.4rem 0}"
    ".bar>i{display:block;height:100%;background:#3a8;width:0;transition:width .3s}"
    "#phase{font-weight:600;letter-spacing:.02em} #integrity{font-size:1.1rem;font-weight:700}"
    "label{display:flex;flex-direction:column;gap:.15rem;font-size:.82rem;color:#9aa}"
    "</style></head><body>"
    "<h1>hu-probe</h1>"
    "<p class=muted>Head-Unit MSC Messsonde · " FW_VERSION " · Offline-First Experiment</p>"

    "<div class=card>"
    "<h2>Messlauf</h2>"
    "<div class=row>"
    "<label>Fahrzeug <select id=v>"
    "<option value=lab_debian>lab_debian</option>"
    "<option value=bmw>bmw (NBT Evo)</option>"
    "<option value=mazda_cx30>mazda_cx30</option>"
    "</select></label>"
    "<label>Experiment <select id=exp></select></label>"
    "<label>Dauer (s) <input id=dur type=number min=5 max=180 value=60 style='width:5rem'></label>"
    "</div>"
    "<div class=row>"
    "<label><span>Ende</span>"
    "<select id=end><option value=media_hide>Media-Hide</option>"
    "<option value=soft_detach>Soft-Detach</option></select></label>"
    "<label style='flex-direction:row;align-items:center;gap:.4rem;margin-top:1.1rem'>"
    "<input id=soft type=checkbox checked> Soft-USB Re-Enum</label>"
    "</div>"
    "<p class=muted id=markHint>BOOT kurz = nächster Marker · BOOT lang (≥1,5 s) = Stop</p>"
    "<div class=row>"
    "<button id=go onclick=runExperiment()>▶ Offline-Messung starten</button>"
    "<button class=sec onclick=stopOffline()>Stop anfordern</button>"
    "</div>"
    "<p id=phase class=muted>IDLE</p>"
    "<div class=bar><i id=pbar></i></div>"
    "<p class=muted id=budget></p>"
    "<p id=integrity></p>"
    "<pre id=lastRun class=muted>noch kein Lauf</pre>"
    "</div>"

    "<div class=card>"
    "<h2>Live / Debug</h2>"
    "<div class=row>"
    "<button class=sec onclick=startLive()>Start Session (TCP live)</button>"
    "<button class=sec onclick=stopLive()>Stop</button>"
    "<button class=sec onclick=mark('operator')>Marker</button>"
    "<button class=sec onclick=cycle()>USB Soft-Cycle</button>"
    "<button class=sec onclick=remount()>Media Remount</button>"
    "</div>"
    "<div class=row>"
    "<button class=sec onclick=mark('play_start')>play_start</button>"
    "<button class=sec onclick=mark('track_next')>track_next</button>"
    "<button class=sec onclick=mark('audible_change')>audible_change</button>"
    "<button class=sec onclick=mark('replay')>replay</button>"
    "<button class=sec onclick=mark('stop')>stop</button>"
    "</div>"
    "<p class=muted>Live-Marker brauchen WLAN. Während Offline-Capture nur BOOT.</p>"
    "</div>"

    "<pre id=st>loading…</pre>"
    "<script>"
    "const PROFILE={lab_debian:'lab_debian',bmw:'bmw_nbt_evo',mazda_cx30:'mazda_cx30'};"
    "let experiments=[];"
    "let localArm=null;"
    "async function loadExperiments(){"
    "  const r=await fetch('/api/experiments');experiments=await r.json();"
    "  const sel=exp;sel.innerHTML='';"
    "  (experiments.experiments||[]).forEach(e=>{"
    "    const o=document.createElement('option');o.value=e.id;o.textContent=e.label+' ('+e.id+')';"
    "    if(e.id==='s2_prefetch')o.selected=true;sel.appendChild(o)});"
    "  applyExp();"
    "}"
    "function curExp(){return (experiments.experiments||[]).find(e=>e.id===exp.value)}"
    "function applyExp(){const e=curExp();if(!e)return;"
    "  dur.value=Math.round((e.measure_ms||60000)/1000);"
    "  end.value=e.end_strategy||'media_hide';"
    "  soft.checked=!!e.soft_usb;"
    "  markHint.textContent='Marker (BOOT kurz): '+(e.markers||[]).join(' → ')+"
    "    ' · BOOT lang (≥1,5 s) = Stop';"
    "}"
    "exp.onchange=applyExp;"
    "function setPhase(t,cls){phase.textContent=t;phase.className=cls||'';}"
    "function showIntegrity(lr){"
    "  if(!lr){integrity.textContent='';lastRun.textContent='noch kein Lauf';return}"
    "  const pass=!!lr.integrity_pass;"
    "  integrity.textContent='Integrity: '+(pass?'PASS':'FAIL');"
    "  integrity.className=pass?'ok':'fail';"
    "  lastRun.textContent=JSON.stringify(lr,null,2);"
    "}"
    "function updateProgress(o){"
    "  const b=o&&o.capture_budget||{};"
    "  const meas=b.measure_ms||0;const el=b.elapsed_ms||0;"
    "  if(meas>0&&(o.capturing||o.state==='capturing')){"
    "    const pct=Math.min(100,Math.round(100*el/meas));pbar.style.width=pct+'%';"
    "  } else if(localArm&&Date.now()<localArm.until){"
    "    const pct=Math.min(100,Math.round(100*(Date.now()-localArm.t0)/localArm.ms));"
    "    pbar.style.width=pct+'%';"
    "  } else if(!o||!o.active){pbar.style.width='0%'}"
    "  if(b.record_size){"
    "    budget.textContent='Budget: '+b.estimated_max_records+' Rec × '+b.record_size+"
    "      ' B ≤ '+(b.capacity_bytes/1024).toFixed(0)+' KiB · max '+(b.max_duration_ms/1000)+'s';"
    "      +(o.records!=null?(' · now '+o.records+' Rec / '+((o.bytes||0)/1024).toFixed(1)+' KiB'):'');"
    "  }"
    "}"
    "async function refresh(){"
    "  try{"
    "    const r=await fetch('/api/status');const j=await r.json();"
    "    st.textContent=JSON.stringify(j,null,2);"
    "    const o=j.offline||{};"
    "    const stt=(o.state||'idle').toUpperCase();"
    "    if(o.capturing)setPhase('CAPTURING · WiFi OFF · BOOT=Marker','warn');"
    "    else if(o.state==='arming')setPhase('ARMING','warn');"
    "    else if(o.state==='finalizing')setPhase('FINALIZING','warn');"
    "    else if(o.state==='uploading')setPhase('UPLOADING · '+ (o.upload_status||''),'warn');"
    "    else setPhase(stt==='IDLE'?'IDLE':'STATE '+stt);"
    "    updateProgress(o);"
    "    if(o.last_run){showIntegrity(o.last_run);if(!o.active)localArm=null}"
    "    go.disabled=!!o.active;"
    "  }catch(e){"
    "    if(localArm){setPhase('CAPTURING · WiFi OFF (UI getrennt)','warn');updateProgress({capturing:true,capture_budget:{measure_ms:localArm.ms,elapsed_ms:Date.now()-localArm.t0}})}"
    "    else setPhase('STATUS unreachable','fail');"
    "  }"
    "}"
    "async function runExperiment(){"
    "  const body={vehicle:v.value,experiment:exp.value,profile_id:PROFILE[v.value]||v.value,"
    "    measure_ms:Math.round(+dur.value*1000),end_strategy:end.value,soft_usb:!!soft.checked};"
    "  setPhase('ARMING…','warn');go.disabled=true;"
    "  try{"
    "    const r=await fetch('/api/experiment/start',{method:'POST',headers:{'Content-Type':'application/json'},"
    "      body:JSON.stringify(body)});"
    "    const t=await r.text();st.textContent=t;"
    "    let j={};try{j=JSON.parse(t)}catch(_){}"
    "    if(j.ok){"
    "      localArm={t0:Date.now(),ms:body.measure_ms||60000,until:Date.now()+(body.measure_ms||60000)+120000};"
    "      if(j.capture_budget)updateProgress({capturing:true,capture_budget:j.capture_budget,records:0,bytes:0});"
    "      setPhase('CAPTURING · WiFi OFF','warn');"
    "    } else {setPhase('ARM FAILED','fail');go.disabled=false}"
    "  }catch(e){setPhase('ARM ERROR (Probe geht offline?)','warn');"
    "    localArm={t0:Date.now(),ms:Math.round(+dur.value*1000),until:Date.now()+300000}}"
    "}"
    "async function stopOffline(){await fetch('/api/offline/stop',{method:'POST'});refresh()}"
    "async function startLive(){await fetch('/api/session/start',{method:'POST',headers:{'Content-Type':'application/json'},"
    "body:JSON.stringify({vehicle:v.value,scenario:exp.value,profile_id:PROFILE[v.value]||v.value})});refresh()}"
    "async function stopLive(){await fetch('/api/session/stop',{method:'POST'});refresh()}"
    "async function mark(n){await fetch('/api/marker',{method:'POST',headers:{'Content-Type':'application/json'},"
    "body:JSON.stringify({name:n||'operator'})});refresh()}"
    "async function cycle(){await fetch('/api/usb/cycle',{method:'POST',headers:{'Content-Type':'application/json'},"
    "body:JSON.stringify({holdMs:600})});refresh()}"
    "async function remount(){await fetch('/api/usb/remount',{method:'POST',headers:{'Content-Type':'application/json'},"
    "body:JSON.stringify({bumpSerial:true})});refresh()}"
    "loadExperiments().then(refresh);setInterval(refresh,1000);"
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
    server_.on("/api/offline/start", HTTP_POST, [this]() { handleOfflineStart(); });
    server_.on("/api/offline/stop", HTTP_POST, [this]() { handleOfflineStop(); });
    server_.on("/api/experiments", HTTP_GET, [this]() { handleExperiments(); });
    server_.on("/api/experiment/start", HTTP_POST, [this]() { handleExperimentStart(); });
    server_.on(
        "/ota-upload", HTTP_POST,
        [this]() { handleOtaDone(); },
        [this]() { handleOtaUpload(); });
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
    {
        JsonObject oc = doc["offline"].to<JsonObject>();
        OfflineCapture::instance().toStatusJson(oc);
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

static String profileForVehicle(const String& vehicle, const String& fallback) {
    if (vehicle == "lab_debian") return "lab_debian";
    if (vehicle == "bmw") return "bmw_nbt_evo";
    if (vehicle == "mazda_cx30") return "mazda_cx30";
    return fallback;
}

void WebUi::handleSessionStart() {
    String vehicle = cfg_ ? cfg_->vehicleId : "lab_debian";
    String scenario = cfg_ ? cfg_->scenarioId : "s0_plug_enum";
    String profile = cfg_ ? cfg_->profileId : "lab_debian";
    bool profileExplicit = false;
    if (server_.hasArg("plain")) {
        JsonDocument doc;
        if (!deserializeJson(doc, server_.arg("plain"))) {
            if (doc["vehicle"].is<const char*>()) vehicle = doc["vehicle"].as<String>();
            if (doc["scenario"].is<const char*>()) scenario = doc["scenario"].as<String>();
            if (doc["profile_id"].is<const char*>()) {
                profile = doc["profile_id"].as<String>();
                profileExplicit = true;
            }
        }
    }
    if (!profileExplicit) {
        profile = profileForVehicle(vehicle, profile);
    }
    if (cfg_) {
        cfg_->vehicleId = vehicle;
        cfg_->scenarioId = scenario;
        cfg_->profileId = profile;
        cfg_->save();
    }
    if (session_) {
        const char* ser = msc_ ? msc_->identitySerial() : nullptr;
        const char* lab = msc_ ? msc_->volumeLabel() : nullptr;
        session_->start(vehicle, scenario, profile, ser, lab);
    }
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
    OfflineCapture& oc = OfflineCapture::instance();
    if (oc.capturing()) {
        oc.queueMarker(name.c_str());
        server_.send(200, "application/json", "{\"ok\":true,\"via\":\"offline_ram\"}");
        return;
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

void WebUi::handleExperiments() {
    JsonDocument doc;
    JsonArray arr = doc["experiments"].to<JsonArray>();
    for (size_t i = 0; i < kExperimentCount; i++) {
        const ExperimentProfile& p = kExperiments[i];
        JsonObject o = arr.add<JsonObject>();
        o["id"] = p.id;
        o["label"] = p.label;
        o["scenario"] = p.scenario;
        o["measure_ms"] = p.measureMs;
        o["soft_usb"] = p.softUsb;
        o["end_strategy"] = p.endStrategy;
        JsonArray m = o["markers"].to<JsonArray>();
        for (uint8_t j = 0; j < p.markerCount; j++) m.add(p.markers[j]);
    }
    {
        JsonObject budget = doc["capture_budget"].to<JsonObject>();
        OfflineCapture::instance().toBudgetJson(budget);
    }
    String body;
    serializeJson(doc, body);
    server_.send(200, "application/json", body);
}

void WebUi::armOfflineCapture(const String& vehicle, const String& scenario,
                              const String& profile, bool softUsb, uint32_t measureMs,
                              const String& endStrategy, const char* const* markers,
                              uint8_t markerCount, const char* experimentId) {
    OfflineCapture& oc = OfflineCapture::instance();
    if (oc.active()) {
        server_.send(409, "application/json", "{\"ok\":false,\"err\":\"already_active\"}");
        return;
    }
    if (cfg_) {
        cfg_->vehicleId = vehicle;
        cfg_->scenarioId = scenario;
        cfg_->profileId = profile;
        cfg_->offlineEndStrategy = endStrategy;
        cfg_->save();
    }

    JsonDocument out;
    out["ok"] = true;
    out["action"] = "offline_arm";
    if (experimentId) out["experiment"] = experimentId;
    out["vehicle"] = vehicle;
    out["scenario"] = scenario;
    out["profile_id"] = profile;
    out["soft_usb"] = softUsb;
    out["auto_detach_ms"] = measureMs;
    out["measure_ms"] = measureMs;
    out["end_strategy"] = endStrategy;
    out["note"] = "wifi_off_then_capture";
    {
        JsonArray m = out["markers"].to<JsonArray>();
        if (markers && markerCount) {
            for (uint8_t i = 0; i < markerCount; i++) m.add(markers[i]);
        }
    }
    {
        JsonObject budget = out["capture_budget"].to<JsonObject>();
        // Theoretical budget before alloc (actual appears in /api/status after arm).
        budget["capacity_bytes"] = (uint32_t)OfflineCapture::defaultCapBytes();
        budget["capacity_target_bytes"] = (uint32_t)OfflineCapture::defaultCapBytes();
        budget["capacity_fallback_bytes"] = (uint32_t)OfflineCapture::fallbackCapBytes();
        budget["record_size"] = (uint32_t)OfflineCapture::recordSize();
        budget["estimated_max_records"] =
            (uint32_t)OfflineCapture::maxRecordsForCap(OfflineCapture::defaultCapBytes());
        budget["max_duration_ms"] = OfflineCapture::maxCaptureMs();
        budget["measure_ms"] = measureMs;
    }
    String body;
    serializeJson(out, body);
    server_.send(200, "application/json", body);
    delay(50);
    server_.client().flush();
    delay(50);

    if (!oc.arm(session_, msc_, inst_, tcp_)) {
        Serial.println("[OFFLINE] arm failed");
        return;
    }
    if (!oc.beginCapture(vehicle, scenario, profile, softUsb, measureMs, endStrategy, markers,
                         markerCount)) {
        Serial.println("[OFFLINE] beginCapture failed");
        oc.requestStop("begin_failed");
    }
}

void WebUi::handleOfflineStart() {
    OfflineCapture& oc = OfflineCapture::instance();
    if (oc.active()) {
        server_.send(409, "application/json", "{\"ok\":false,\"err\":\"already_active\"}");
        return;
    }
    String vehicle = cfg_ ? cfg_->vehicleId : "lab_debian";
    String scenario = cfg_ ? cfg_->scenarioId : "s0_plug_enum";
    String profile = cfg_ ? cfg_->profileId : "lab_debian";
    // Align with beginCapture / docs / experiment profiles (was false → silent no re-enum).
    bool softUsb = true;
    uint32_t autoDetachMs = 0;
    String endStrategy = cfg_ ? cfg_->offlineEndStrategy : "media_hide";
    bool softExplicit = false;
    bool measureExplicit = false;
    bool endExplicit = false;
    if (server_.hasArg("plain")) {
        JsonDocument doc;
        if (!deserializeJson(doc, server_.arg("plain"))) {
            if (doc["vehicle"].is<const char*>()) vehicle = doc["vehicle"].as<String>();
            if (doc["scenario"].is<const char*>()) scenario = doc["scenario"].as<String>();
            if (doc["profile_id"].is<const char*>()) profile = doc["profile_id"].as<String>();
            if (!doc["soft_usb"].isNull()) {
                softUsb = doc["soft_usb"].as<bool>();
                softExplicit = true;
            }
            if (!doc["auto_detach_ms"].isNull()) {
                autoDetachMs = doc["auto_detach_ms"].as<uint32_t>();
                measureExplicit = true;
            }
            if (!doc["measure_ms"].isNull() && !measureExplicit) {
                autoDetachMs = doc["measure_ms"].as<uint32_t>();
                measureExplicit = true;
            }
            if (doc["end_strategy"].is<const char*>()) {
                String requested = doc["end_strategy"].as<String>();
                if (requested == "media_hide" || requested == "soft_detach") {
                    endStrategy = requested;
                    endExplicit = true;
                }
            }
        }
    }
    const ExperimentProfile* exp = findExperimentByScenario(scenario);
    const char* const* markers = exp ? exp->markers : nullptr;
    uint8_t markerCount = exp ? exp->markerCount : 0;
    if (exp) {
        if (!softExplicit) softUsb = exp->softUsb;
        if (!endExplicit) endStrategy = exp->endStrategy;
        if (!measureExplicit) autoDetachMs = exp->measureMs;
    }
    armOfflineCapture(vehicle, scenario, profile, softUsb, autoDetachMs, endStrategy, markers,
                      markerCount, exp ? exp->id : nullptr);
}

void WebUi::handleExperimentStart() {
    OfflineCapture& oc = OfflineCapture::instance();
    if (oc.active()) {
        server_.send(409, "application/json", "{\"ok\":false,\"err\":\"already_active\"}");
        return;
    }
    String vehicle = cfg_ ? cfg_->vehicleId : "lab_debian";
    String experimentId = "s2_prefetch";
    String profile = cfg_ ? cfg_->profileId : "lab_debian";
    bool softUsb = true;
    uint32_t measureMs = 0;
    String endStrategy = "media_hide";
    bool softExplicit = false;
    bool endExplicit = false;
    bool measureExplicit = false;

    if (server_.hasArg("plain")) {
        JsonDocument doc;
        if (!deserializeJson(doc, server_.arg("plain"))) {
            if (doc["vehicle"].is<const char*>()) vehicle = doc["vehicle"].as<String>();
            if (doc["experiment"].is<const char*>()) experimentId = doc["experiment"].as<String>();
            else if (doc["scenario"].is<const char*>()) experimentId = doc["scenario"].as<String>();
            if (doc["profile_id"].is<const char*>()) profile = doc["profile_id"].as<String>();
            else profile = profileForVehicle(vehicle, profile);
            if (!doc["soft_usb"].isNull()) {
                softUsb = doc["soft_usb"].as<bool>();
                softExplicit = true;
            }
            if (!doc["measure_ms"].isNull()) {
                measureMs = doc["measure_ms"].as<uint32_t>();
                measureExplicit = true;
            } else if (!doc["auto_detach_ms"].isNull()) {
                measureMs = doc["auto_detach_ms"].as<uint32_t>();
                measureExplicit = true;
            }
            if (doc["end_strategy"].is<const char*>()) {
                String requested = doc["end_strategy"].as<String>();
                if (requested == "media_hide" || requested == "soft_detach") {
                    endStrategy = requested;
                    endExplicit = true;
                }
            }
        }
    } else {
        profile = profileForVehicle(vehicle, profile);
    }

    const ExperimentProfile* exp = findExperiment(experimentId.c_str());
    if (!exp) {
        server_.send(400, "application/json", "{\"ok\":false,\"err\":\"unknown_experiment\"}");
        return;
    }
    if (!measureExplicit) measureMs = exp->measureMs;
    if (!softExplicit) softUsb = exp->softUsb;
    if (!endExplicit) endStrategy = exp->endStrategy;
    if (measureMs > OfflineCapture::maxCaptureMs()) {
        measureMs = OfflineCapture::maxCaptureMs();
    }

    armOfflineCapture(vehicle, exp->scenario, profile, softUsb, measureMs, endStrategy,
                      exp->markers, exp->markerCount, exp->id);
}

void WebUi::handleOfflineStop() {
    OfflineCapture& oc = OfflineCapture::instance();
    if (!oc.capturing()) {
        server_.send(409, "application/json", "{\"ok\":false,\"err\":\"not_capturing\"}");
        return;
    }
    // WiFi is off — this only works if stop is triggered while somehow still connected
    // (e.g. race) or after reconnect. Prefer Unplug/BOOT. Still accept the request.
    oc.requestStop("api_stop");
    server_.send(200, "application/json", "{\"ok\":true,\"action\":\"offline_stop_requested\"}");
}

void WebUi::handleOtaUpload() {
    HTTPUpload& upload = server_.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("[OTA] upload start %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (!Update.end(true)) {
            Update.printError(Serial);
        } else {
            Serial.printf("[OTA] %u bytes\n", upload.totalSize);
        }
    }
}

void WebUi::handleOtaDone() {
    server_.sendHeader(F("Connection"), F("close"));
    if (Update.hasError()) {
        server_.send(500, F("text/plain"), F("OTA fail"));
        return;
    }
    server_.send(200, F("text/plain"), F("OK"));
    delay(400);
    ESP.restart();
}
