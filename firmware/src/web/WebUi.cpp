#include "WebUi.h"
#include "BuildFlags.h"
#include "app/DriveMode.h"
#include "capture/OfflineCapture.h"
#include "capture/ExperimentProfiles.h"
#include "capture/SequenceRunner.h"
#include "capture/TripStore.h"
#include "core/DiagLog.h"
#include "core/GithubOta.h"
#include "core/HwInfo.h"
#include "core/PanicTail.h"
#include "media/ProbeMedia.h"
#include "msc/MscBotTap.h"
#include "msc/MscGadget.h"
#include "msc/ReadStall.h"
#include <Update.h>
#include <WiFi.h>

static String profileForVehicle(const String& vehicle, const String& fallback);

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
    "<p class=muted id=hwNow>Hardware: …</p>"

    "<div class=card>"
    "<h2>Messlauf</h2>"
    "<div class=row>"
    "<label>Fahrzeug <select id=v>"
    "<option value=bmw selected>bmw (NBT Evo)</option>"
    "<option value=lab_debian>lab_debian</option>"
    "<option value=mazda_cx30>mazda_cx30</option>"
    "</select></label>"
    "<label>Experiment <select id=exp></select></label>"
    "<label>Dauer (s) <input id=dur type=number min=5 max=1800 value=60 style='width:5rem'></label>"
    "</div>"
    "<div class=row>"
    "<label><span>Ende</span>"
    "<select id=end><option value=media_hide>Media-Hide</option>"
    "<option value=soft_detach>Soft-Detach</option></select></label>"
    "<label style='flex-direction:row;align-items:center;gap:.4rem;margin-top:1.1rem'>"
    "<input id=soft type=checkbox> Soft-USB Re-Enum</label>"
    "<label style='flex-direction:row;align-items:center;gap:.4rem;margin-top:1.1rem'>"
    "<input id=endUm type=checkbox> Unmount/Remount der HU beendet Aufnahme</label>"
    "<label style='flex-direction:row;align-items:center;gap:.4rem;margin-top:1.1rem'>"
    "<input id=fine type=checkbox> DATA je 4 KiB (Fein-Modus)</label>"
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
    "<h2>Heimfahrt-Modus</h2>"
    "<p class=muted id=driveNow>…</p>"
    "<div class=row>"
    "<label style='flex-direction:row;align-items:center;gap:.4rem'>"
    "<input id=aCap type=checkbox> beim Start automatisch messen</label>"
    "<label style='flex-direction:row;align-items:center;gap:.4rem'>"
    "<input id=aFine type=checkbox> Fein-Modus</label>"
    "<label style='flex-direction:row;align-items:center;gap:.4rem'>"
    "<input id=aUpd type=checkbox> Auto-Update (GitHub)</label>"
    "</div>"
    "<div class=row>"
    "<label>Medium <select id=aMedia></select></label>"
    "<label>Hotspot-SSID <input id=w2s maxlength=32></label>"
    "<label>Hotspot-Passwort <input id=w2p type=password maxlength=64></label>"
    "</div>"
    "<div class=row><label style='flex:1'>Update-Manifest <input id=aUrl style='width:100%'></label></div>"
    "<div class=row><button onclick=saveDrive()>Speichern</button></div>"
    "<p class=muted>Gilt ab dem nächsten Start. Ablauf: WLAN (Heimnetz oder Hotspot, 15 s) → "
    "gespeicherte Fahrten hochladen → Update prüfen → Messung (bis 30 min) startet mit dem "
    "Einstecken. BOOT beim Einstecken gedrückt halten = WLAN-Portal.</p>"
    "<h2>Gespeicherte Fahrten (Flash)</h2>"
    "<p class=muted id=tripsNow></p>"
    "<div class=row>"
    "<button class=sec onclick=tripsUpload()>Ausstehende hochladen</button>"
    "<button class=sec onclick=tripsErase(false)>Hochgeladene löschen</button>"
    "<button class=sec onclick=tripsErase(true)>Alle löschen</button>"
    "</div>"
    "<pre id=tripList class=muted></pre>"
    "</div>"

    "<div class=card>"
    "<h2>Medium (SynthFat)</h2>"
    "<p class=muted id=mediaNow>aktiv: …</p>"
    "<div class=row>"
    "<label>Variante <select id=mpre></select></label>"
    "<label><span>Wechsel</span><select id=mmeth><option value=cycle>USB Re-Enum</option>"
    "<option value=remount>Remount (NOT READY)</option></select></label>"
    "<button class=sec onclick=setMedia()>Aktivieren</button>"
    "</div>"
    "<details><summary class=muted>Eigene Spezifikation (JSON)</summary>"
    "<textarea id=mspec rows=6 style='width:100%;font:inherit;font-size:.8rem'>"
    "{\"id\":\"custom\",\"spc\":8,\"files\":[{\"path\":\"A/T1.MP3\",\"dur_s\":30,\"seed\":1}]}"
    "</textarea>"
    "<button class=sec onclick=setSpec()>Spezifikation laden</button></details>"
    "<details><summary class=muted>Live-Änderung ohne Remount (ID3 / Cover / Seed)</summary>"
    "<div class=row>"
    "<label>Datei <select id=mfile onchange=fillMut()></select></label>"
    "<label>Seed <input id=mseed type=number min=0 max=255 style='width:4.5rem'></label>"
    "<label>Cover <select id=mcover><option value=''>–</option><option value=0>0 Platzhalter</option>"
    "<option value=1>1 rot</option><option value=2>2 grün</option><option value=3>3 blau</option>"
    "<option value=4>4 gelb</option></select></label>"
    "</div>"
    "<div class=row>"
    "<label>Titel <input id=mtitle maxlength=30></label>"
    "<label>Interpret <input id=martist maxlength=30></label>"
    "<label>Album <input id=malbum maxlength=30></label>"
    "</div>"
    "<div class=row>"
    "<label><span>Host informieren</span><select id=mnotify>"
    "<option value=none>nein (still)</option><option value=unit_attention>UNIT ATTENTION 06/28</option>"
    "<option value=remount>Remount (NOT READY)</option><option value=cycle>USB Re-Enum</option></select></label>"
    "<label>Verzögerung (s) <input id=mdelay type=number min=0 max=600 value=0 style='width:5rem'></label>"
    "<button class=sec onclick=mutate()>Ändern</button>"
    "<button class=sec onclick=notifyUa()>Nur UNIT ATTENTION</button>"
    "</div>"
    "<p class=muted>Größe/Cluster bleiben gleich. Leeres Textfeld = Standardtext. "
    "Verzögerung &gt; 0 = Änderung landet während der Offline-Messung (WLAN aus). "
    "Nicht persistent: Variantenwechsel stellt die Spezifikation wieder her.</p>"
    "</details>"
    "<pre id=mfiles class=muted></pre>"
    "</div>"

    "<div class=card>"
    "<h2>Sequenz</h2>"
    "<div class=row>"
    "<label>Sequenz <select id=seq></select></label>"
    "<label style='flex-direction:row;align-items:center;gap:.4rem;margin-top:1.1rem'>"
    "<input id=autoArm type=checkbox> ohne BOOT starten</label>"
    "<button onclick=seqStart()>▶ Sequenz starten</button>"
    "<button class=sec onclick=seqCmd('next')>Schritt starten</button>"
    "<button class=sec onclick=seqCmd('skip')>Überspringen</button>"
    "<button class=sec onclick=seqCmd('abort')>Abbrechen</button>"
    "</div>"
    "<p id=seqPrompt class=warn></p>"
    "<p class=muted>LED weiß blinkend = Fahrer ist dran: BOOT kurz = Schritt starten, "
    "BOOT lang = überspringen.</p>"
    "</div>"

    "<div class=card>"
    "<h2>Bremse (READ10 BUSY)</h2>"
    "<div class=row>"
    "<label style='flex-direction:row;align-items:center;gap:.3rem'><input id=srMeta type=checkbox> FAT/DIR</label>"
    "<label style='flex-direction:row;align-items:center;gap:.3rem'><input id=srHead type=checkbox> Dateikopf</label>"
    "<label style='flex-direction:row;align-items:center;gap:.3rem'><input id=srBody type=checkbox checked> Audio</label>"
    "<label><span>Tempo</span><select id=sPace><option value=fixed>fest</option>"
    "<option value=realtime>Live-Bitrate</option></select></label>"
    "<label>Verzögerung (ms) <input id=sDelay type=number min=1 max=120000 value=2000 style='width:6rem'></label>"
    "</div>"
    "<div class=row>"
    "<label>Anzahl (0 = alle) <input id=sCount type=number min=0 value=1 style='width:5rem'></label>"
    "<label>Start nach (s) <input id=sArm type=number min=0 max=600 value=0 style='width:5rem'></label>"
    "<label>Fenster (s, 0 = offen) <input id=sWin type=number min=0 max=3600 value=0 style='width:5rem'></label>"
    "<button class=sec onclick=stallOn()>Aktivieren</button>"
    "<button class=sec onclick=stallOff()>Aus</button>"
    "</div>"
    "<p class=muted id=stallNow></p>"
    "<p class=warn>Über ~20 s pro Read nur im Stand: Die HU kann das Medium abmelden.</p>"
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
    "    if(e.id==='free')o.selected=true;sel.appendChild(o)});"
    "  applyExp();"
    "}"
    "function curExp(){return (experiments.experiments||[]).find(e=>e.id===exp.value)}"
    "function applyExp(){const e=curExp();if(!e)return;"
    "  dur.value=Math.round((e.measure_ms||60000)/1000);"
    "  end.value=e.end_strategy||'media_hide';"
    "  soft.checked=!!e.soft_usb;"
    "  endUm.checked=e.end_on_unmount!==false;"
    "  fine.checked=!!e.fine_data;"
    "  markHint.textContent=e.free_markers?"
    "    'BOOT kurz = Marker mark_01, mark_02 … · BOOT 2× schnell = Soft-Steckzyklus (2 s, Strom bleibt) · BOOT lang (≥1,5 s) = Stop':"
    "    'Marker (BOOT kurz): '+(e.markers||[]).join(' → ')+' · BOOT lang (≥1,5 s) = Stop';"
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
    "    const mm=s=>Math.floor(s/60)+':'+String(s%60).padStart(2,'0');"
    "    budget.textContent='Puffer '+(b.capacity_bytes/1024).toFixed(0)+' KiB'+(b.capacity_allocated?'':' (erwartet)')+"
    "      ' · ≈'+b.est_bytes_per_s+' B/s '+(b.fine_data?'fein':'kompakt')+"
    "      ' · reicht ≈'+mm(b.est_duration_s||0)+' min (max '+mm(Math.round(b.max_duration_ms/1000))+')'"
    "      +(o.records!=null?(' · jetzt '+o.records+' Rec / '+((o.bytes||0)/1024).toFixed(1)+' KiB'):'');"
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
    "    const h=j.hw||{},hf=h.flash||{},hp=h.psram||{},MB=b=>Math.round((b||0)/1048576);"
    "    hwNow.textContent='Hardware: '+(h.chip||'?')+' rev '+(h.chip_rev??'?')+' · Modul '+(h.module||'?')+"
    "      ' · Flash '+MB(hf.bytes_real)+' MiB ('+(hf.vendor||hf.jedec_id||'?')+', genutzt '+MB(hf.bytes_configured)+')'+"
    "      ' · PSRAM '+(hp.bytes_efuse?(MB(hp.bytes_efuse)+' MiB '+hp.bus+' '+(hp.vendor||'')+"
    "      (hp.build_enabled?' (aktiv)':' (im Build aus)')):'keins')+"
    "      ' · Heap frei '+Math.round((h.heap_free||0)/1024)+' KiB, größter Block '+Math.round((h.heap_max_alloc||0)/1024)+' KiB';"
    "    const m=j.media||{};mediaNow.textContent='aktiv: '+m.variant+' · '+m.fs+' · '+"
    "      m.sector_count+' Sektoren · spc '+m.spc+' · hash '+m.hash;"
    "    const s=j.stall||{};stallNow.textContent=(s.enabled?(s.active?'AKTIV':'wartet auf Start'):'aus')+"
    "      ' · gehalten '+(s.held||0)+' · abgebrochen '+(s.abandoned||0)+' · max '+(s.max_wait_ms||0)+' ms'+"
    "      (s.last_off?(' · zuletzt aus: '+s.last_off):'');"
    "    const d=j.drive||{},g=j.ota||{};"
    "    driveNow.textContent='Zustand '+(d.state||'off')+(d.wlan?(' · WLAN '+d.wlan):'')+"
    "      ' · ausstehend '+(d.trips_pending||0)+(d.error?(' · '+d.error):'')+"
    "      ' · Update: '+(g.result||'–')+(g.rolled_back_from?(' · ROLLBACK von '+g.rolled_back_from):'');"
    "    const q=j.sequence||{};"
    "    seqPrompt.textContent=q.active?('['+q.state+'] Schritt '+q.step+'/'+q.steps+' · '+q.media+"
    "      ' · '+(q.prompt||'')):(q.state&&q.state!=='idle'?('Sequenz '+q.state+(q.error?(' · '+q.error):'')):'');"
    "  }catch(e){"
    "    if(localArm){setPhase('CAPTURING · WiFi OFF (UI getrennt)','warn');updateProgress({capturing:true,capture_budget:{measure_ms:localArm.ms,elapsed_ms:Date.now()-localArm.t0}})}"
    "    else setPhase('STATUS unreachable','fail');"
    "  }"
    "}"
    "async function runExperiment(){"
    "  const body={vehicle:v.value,experiment:exp.value,profile_id:PROFILE[v.value]||v.value,"
    "    measure_ms:Math.round(+dur.value*1000),end_strategy:end.value,soft_usb:!!soft.checked,end_on_unmount:!!endUm.checked,"
    "    fine_data:!!fine.checked};"
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
    "async function loadMedia(){"
    "  const j=await (await fetch('/api/media')).json();mpre.innerHTML='';"
    "  (j.presets||[]).forEach(p=>{const o=document.createElement('option');o.value=p.id;"
    "    o.textContent=p.id+' – '+p.label;if(j.active&&p.id===j.active.variant)o.selected=true;"
    "    mpre.appendChild(o)});"
    "  mediaFiles=((j.active||{}).files||[]).filter(f=>!f.dir);"
    "  const keep=mfile.value;mfile.innerHTML='';"
    "  mediaFiles.forEach(f=>{const o=document.createElement('option');o.value=f.path;o.textContent=f.path;"
    "    if(f.path===keep)o.selected=true;mfile.appendChild(o)});fillMut();"
    "  mfiles.textContent=((j.active||{}).files||[]).map(f=>f.path+(f.dir?'/':'  '+f.size+' B'+"
    "    (f.dur_ms?(' '+(f.dur_ms/1000).toFixed(1)+' s'):'')+(f.first_lba!=null?(' @LBA '+f.first_lba):'')+"
    "    (f.title?(' · '+f.title+' / '+f.artist+' / '+f.album):'')+(f.apic_bytes?(' · cover '+f.cover):''))).join('\\n')+"
    "    '\\nmutation_gen '+((j.active||{}).mutation_gen||0)+' · UA gesendet '+((j.active||{}).ua_sent||0);"
    "}"
    "let mediaFiles=[];"
    "function fillMut(){const f=mediaFiles.find(x=>x.path===mfile.value);if(!f)return;"
    "  mseed.value=f.seed||0;mtitle.value=f.title||'';martist.value=f.artist||'';malbum.value=f.album||'';"
    "  mcover.value=f.apic_bytes?String(f.cover||0):'';mcover.disabled=!f.apic_bytes}"
    "async function mutate(){const f=mediaFiles.find(x=>x.path===mfile.value)||{};"
    "  const b={file:mfile.value,notify:mnotify.value,delay_s:+mdelay.value||0};"
    "  if(+mseed.value!==(f.seed||0))b.seed=+mseed.value;"
    "  if(mtitle.value!==(f.title||''))b.title=mtitle.value;"
    "  if(martist.value!==(f.artist||''))b.artist=martist.value;"
    "  if(malbum.value!==(f.album||''))b.album=malbum.value;"
    "  if(mcover.value!==''&&!mcover.disabled&&+mcover.value!==(f.cover||0))b.cover=+mcover.value;"
    "  const r=await fetch('/api/media/mutate',{method:'POST',headers:{'Content-Type':'application/json'},"
    "    body:JSON.stringify(b)});st.textContent=await r.text();loadMedia()}"
    "async function notifyUa(){const r=await fetch('/api/media/notify',{method:'POST',"
    "  headers:{'Content-Type':'application/json'},body:JSON.stringify({kind:'unit_attention'})});"
    "  st.textContent=await r.text();loadMedia()}"
    "async function postMedia(body){"
    "  const r=await fetch('/api/media/variant',{method:'POST',headers:{'Content-Type':'application/json'},"
    "    body:JSON.stringify(body)});st.textContent=await r.text();loadMedia();refresh()}"
    "function setMedia(){postMedia({preset:mpre.value,method:mmeth.value})}"
    "function setSpec(){let s;try{s=JSON.parse(mspec.value)}catch(e){alert('JSON: '+e);return}"
    "  postMedia({spec:s,method:mmeth.value})}"
    "async function postStall(b){const r=await fetch('/api/stall',{method:'POST',"
    "  headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});st.textContent=await r.text();refresh()}"
    "function stallOn(){const reg=[];if(srMeta.checked)reg.push('meta');if(srHead.checked)reg.push('head');"
    "  if(srBody.checked)reg.push('body');"
    "  postStall({regions:reg,pace:sPace.value,delay_ms:+sDelay.value,count:+sCount.value,"
    "    arm_delay_s:+sArm.value,window_s:+sWin.value})}"
    "function stallOff(){postStall({enable:false})}"
    "async function loadSeq(){"
    "  const j=await (await fetch('/api/sequences')).json();seq.innerHTML='';"
    "  (j.sequences||[]).forEach(s=>{const o=document.createElement('option');o.value=s.id;"
    "    o.textContent=s.label+' ('+s.steps.length+' Schritte)';seq.appendChild(o)});"
    "}"
    "async function seqStart(){"
    "  const r=await fetch('/api/sequence/start',{method:'POST',headers:{'Content-Type':'application/json'},"
    "    body:JSON.stringify({id:seq.value,vehicle:v.value,profile_id:PROFILE[v.value]||v.value,"
    "    auto_arm:!!autoArm.checked})});st.textContent=await r.text();refresh()}"
    "async function seqCmd(c){const r=await fetch('/api/sequence/'+c,{method:'POST'});"
    "  st.textContent=await r.text();refresh()}"
    "async function loadDrive(){"
    "  const c=await (await fetch('/api/config')).json();"
    "  aCap.checked=!!c.autoCapture;aFine.checked=c.autoFine!==false;aUpd.checked=c.autoUpdate!==false;"
    "  w2s.value=c.wifi2Ssid||'';w2p.value='';w2p.placeholder=c.wifi2PassSet?'(gespeichert)':'';"
    "  aUrl.value=c.otaManifestUrl||'';aMedia.innerHTML=mpre.innerHTML;aMedia.value=c.autoMedia||'drive_mix';"
    "}"
    "async function saveDrive(){"
    "  const b={autoCapture:aCap.checked,autoFine:aFine.checked,autoUpdate:aUpd.checked,"
    "    autoMedia:aMedia.value,wifi2Ssid:w2s.value,otaManifestUrl:aUrl.value};"
    "  if(w2p.value)b.wifi2Pass=w2p.value;"
    "  const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},"
    "    body:JSON.stringify(b)});st.textContent=await r.text();loadDrive()}"
    "async function loadTrips(){"
    "  const j=await (await fetch('/api/trips')).json();"
    "  if(!j.available){tripsNow.textContent='keine trips-Partition (einmal per Kabel flashen)';tripList.textContent='';return}"
    "  tripsNow.textContent=(j.trips||[]).length+' Fahrten · ausstehend '+(j.pending||0)+' · frei '+"
    "    Math.round((j.free_bytes||0)/1024)+' KiB'+(j.upload_all?' · Upload läuft':'')+(j.upload_error?(' · '+j.upload_error):'');"
    "  tripList.textContent=(j.trips||[]).map(t=>'#'+t.trip_id+' '+(t.bytes/1024).toFixed(1)+' KiB · '+"
    "    t.commits+' Commits · '+t.end_reason+(t.uploaded?' · hochgeladen':' · AUSSTEHEND')+"
    "    (t.writing?' · schreibt':'')+' · '+t.media+' · '+t.wallclock+' · fw '+t.fw).join('\\n')||'keine';"
    "}"
    "async function tripsUpload(){const r=await fetch('/api/trips/upload',{method:'POST'});"
    "  st.textContent=await r.text();loadTrips()}"
    "async function tripsErase(all){if(all&&!confirm('Alle Fahrten löschen, auch nicht hochgeladene?'))return;"
    "  const r=await fetch('/api/trips/erase',{method:'POST',headers:{'Content-Type':'application/json'},"
    "    body:JSON.stringify({all:all})});st.textContent=await r.text();loadTrips()}"
    "loadExperiments().then(loadMedia).then(loadSeq).then(loadDrive).then(loadTrips).then(refresh);"
    "setInterval(refresh,1000);setInterval(loadTrips,5000);"
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
    server_.on("/api/media", HTTP_GET, [this]() { handleMediaGet(); });
    server_.on("/api/media/presets", HTTP_GET, [this]() { handleMediaPresets(); });
    server_.on("/api/media/variant", HTTP_POST, [this]() { handleMediaVariant(); });
    server_.on("/api/media/mutate", HTTP_POST, [this]() { handleMediaMutate(); });
    server_.on("/api/media/notify", HTTP_POST, [this]() { handleMediaNotify(); });
    server_.on("/api/stall", HTTP_GET, [this]() { handleStallGet(); });
    server_.on("/api/stall", HTTP_POST, [this]() { handleStallPost(); });
    server_.on("/api/sequences", HTTP_GET, [this]() { handleSequences(); });
    server_.on("/api/sequence/start", HTTP_POST, [this]() { handleSequenceStart(); });
    server_.on("/api/sequence/next", HTTP_POST, [this]() { handleSequenceCmd("next"); });
    server_.on("/api/sequence/skip", HTTP_POST, [this]() { handleSequenceCmd("skip"); });
    server_.on("/api/sequence/abort", HTTP_POST, [this]() { handleSequenceCmd("abort"); });
    server_.on("/api/diag/log", HTTP_GET, [this]() { handleDiagLog(); });
    server_.on("/api/trips", HTTP_GET, [this]() { handleTrips(); });
    server_.on("/api/trips/upload", HTTP_POST, [this]() { handleTripsUpload(); });
    server_.on("/api/trips/erase", HTTP_POST, [this]() { handleTripsErase(); });
    server_.on(
        "/ota-upload", HTTP_POST,
        [this]() { handleOtaDone(); },
        [this]() { handleOtaUpload(); });
    server_.begin();
    Serial.println("[WEB] :80");
}

void WebUi::loop() {
    server_.handleClient();
    if (tripUploadAll_) pumpTripUpload();
}

void WebUi::pumpTripUpload() {
    OfflineCapture& oc = OfflineCapture::instance();
    if (oc.active()) return;
    const TripStore::Trip* t = TripStore::instance().firstPending();
    bool done = !t;
    if (t && t->meta.tripId == tripUploadLastId_) {
        tripUploadErr_ = "trip_upload_incomplete";
        done = true;
    }
    if (!done) {
        tripUploadLastId_ = t->meta.tripId;
        if (!oc.replayStored(session_, msc_, inst_, tcp_)) {
            tripUploadErr_ = "replay_failed";
            done = true;
        }
    }
    if (!done) return;
    tripUploadAll_ = false;
    if (tripUploadRestoreLive_ && session_ && !session_->active() && cfg_) {
        session_->start(cfg_->vehicleId, cfg_->scenarioId, cfg_->profileId,
                        msc_ ? msc_->identitySerial() : nullptr,
                        msc_ ? msc_->volumeLabel() : nullptr);
    }
}

void WebUi::handleTrips() {
    JsonDocument doc;
    TripStore::instance().toJson(doc.to<JsonObject>());
    doc["upload_all"] = tripUploadAll_;
    if (tripUploadErr_.length()) doc["upload_error"] = tripUploadErr_;
    sendJson(200, doc);
}

void WebUi::handleTripsUpload() {
    JsonDocument out;
    TripStore& ts = TripStore::instance();
    const char* err = nullptr;
    if (!ts.available()) err = "no_trips_partition";
    else if (OfflineCapture::instance().active() || tripUploadAll_) err = "busy";
    else if (WiFi.status() != WL_CONNECTED) err = "no_wifi";
    else if (!cfg_ || !cfg_->enableCollector) err = "collector_disabled";
    out["ok"] = err == nullptr;
    if (err) {
        out["err"] = err;
        sendJson(409, out);
        return;
    }
    out["pending"] = ts.pendingCount();
    tripUploadErr_ = "";
    tripUploadLastId_ = 0;
    tripUploadRestoreLive_ = session_ && session_->active();
    tripUploadAll_ = ts.pendingCount() > 0;
    sendJson(200, out);
}

void WebUi::handleTripsErase() {
    JsonDocument in;
    if (server_.hasArg("plain")) deserializeJson(in, server_.arg("plain"));
    const bool all = in["all"] | false;
    TripStore& ts = TripStore::instance();
    JsonDocument out;
    if (!ts.available() || ts.writing() || OfflineCapture::instance().active() || tripUploadAll_) {
        out["ok"] = false;
        out["err"] = ts.available() ? "busy" : "no_trips_partition";
        sendJson(409, out);
        return;
    }
    const uint8_t pendingBefore = ts.pendingCount();
    // The journal is append-only: "uploaded only" can erase only when nothing is pending.
    const bool ok = all ? ts.eraseAll() : ts.resetIfDone();
    out["ok"] = ok;
    if (!ok && !all && pendingBefore) out["err"] = "pending_trips_left";
    out["trips"] = ts.tripCount();
    sendJson(ok ? 200 : 409, out);
}

void WebUi::handleRoot() {
    server_.send(200, "text/html", PAGE);
}

void WebUi::handleDiagLog() {
    server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_.send(200, "text/plain; charset=utf-8", "");
    char hdr[96];
    snprintf(hdr, sizeof(hdr), "# reset=%s restart_cause=%s boot_generation=%u uptime_ms=%lu\n",
             PanicTail::resetReason(), PanicTail::restartCause(),
             (unsigned)PanicTail::bootGeneration(), (unsigned long)millis());
    server_.sendContent(hdr);
    const char* prev = DiagLog::previousTail();
    if (prev[0]) {
        server_.sendContent("# --- previous boot (RTC tail) ---\n");
        server_.sendContent(prev);
    }
    server_.sendContent("# --- this boot ---\n");
    char buf[512];
    const size_t total = DiagLog::size();
    for (size_t off = 0; off < total;) {
        const size_t n = DiagLog::copyOut(off, buf, sizeof(buf));
        if (n == 0) break;
        server_.sendContent(buf, n);
        off += n;
        yield();
    }
    server_.sendContent("");
}

void WebUi::handleStatus() {
    JsonDocument doc;
    doc["fw"] = FW_VERSION;
    doc["fwType"] = FW_TYPE;
    doc["board"] = HUPROBE_BOARD_LABEL;
    {
        JsonObject hw = doc["hw"].to<JsonObject>();
        hw["chip"] = ESP.getChipModel();
        hw["chip_rev"] = ESP.getChipRevision();
        hw["cpu_mhz"] = ESP.getCpuFreqMHz();
        hw["flash_bytes"] = ESP.getFlashChipSize();
        hw["psram_bytes"] = ESP.getPsramSize();
        hw["heap_total"] = ESP.getHeapSize();
        hw["heap_free"] = ESP.getFreeHeap();
        hw["heap_min_free"] = ESP.getMinFreeHeap();
        hw["heap_max_alloc"] = ESP.getMaxAllocHeap();
        hw["sketch_bytes"] = ESP.getSketchSize();
        hw["sketch_free"] = ESP.getFreeSketchSpace();
        HwInfo::toJson(hw);
    }
    PanicTail::appendPrevious(doc.as<JsonObject>());
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
    ProbeMedia::instance().toJson(doc["media"].to<JsonObject>(), false);
    SequenceRunner::instance().toJson(doc["sequence"].to<JsonObject>());
    ReadStall::instance().toJson(doc["stall"].to<JsonObject>());
    DriveMode::instance().toJson(doc["drive"].to<JsonObject>());
    GithubOta::toJson(doc["ota"].to<JsonObject>());
    if (inst_) inst_->turStatsJson(doc["tur"].to<JsonObject>());
    String out;
    serializeJson(doc, out);
    server_.send(200, "application/json", out);
}

void WebUi::sendJson(int code, JsonDocument& doc) {
    String body;
    serializeJson(doc, body);
    server_.send(code, "application/json", body);
}

void WebUi::handleMediaGet() {
    JsonDocument doc;
    ProbeMedia::instance().toJson(doc["active"].to<JsonObject>(), true);
    ProbeMedia::presetsJson(doc["presets"].to<JsonArray>());
    sendJson(200, doc);
}

void WebUi::handleMediaPresets() {
    JsonDocument doc;
    ProbeMedia::presetsJson(doc["presets"].to<JsonArray>());
    sendJson(200, doc);
}

void WebUi::handleMediaVariant() {
    JsonDocument out;
    if (OfflineCapture::instance().active() || SequenceRunner::instance().active()) {
        out["ok"] = false;
        out["err"] = "capture_or_sequence_active";
        sendJson(409, out);
        return;
    }
    JsonDocument in;
    if (!server_.hasArg("plain") || deserializeJson(in, server_.arg("plain"))) {
        out["ok"] = false;
        out["err"] = "json";
        sendJson(400, out);
        return;
    }
    // method: cycle (default, host re-enumerates) | remount (NOT READY window only)
    const char* method = in["method"] | "cycle";
    const bool cycle = strcmp(method, "remount") != 0;
    String err;
    bool ok = false;
    if (in["spec"].is<JsonObjectConst>()) {
        String spec;
        serializeJson(in["spec"], spec);
        ok = msc_ && msc_->switchMedia(nullptr, &spec, cycle, &err);
    } else if (in["preset"].is<const char*>()) {
        ok = msc_ && msc_->switchMedia(in["preset"].as<const char*>(), nullptr, cycle, &err);
    } else {
        err = "preset_or_spec_required";
    }
    out["ok"] = ok;
    out["method"] = cycle ? "cycle" : "remount";
    if (!ok) out["err"] = err.length() ? err : String("msc_not_ready");
    ProbeMedia::instance().toJson(out["active"].to<JsonObject>(), true);
    if (msc_) out["usb_serial"] = msc_->identitySerial();
    sendJson(ok ? 200 : 400, out);
}

void WebUi::handleMediaMutate() {
    JsonDocument out;
    JsonDocument in;
    if (!server_.hasArg("plain") || deserializeJson(in, server_.arg("plain"))) {
        out["ok"] = false;
        out["err"] = "json";
        sendJson(400, out);
        return;
    }
    MscGadget::MediaNotify notify;
    if (!MscGadget::parseNotify(in["notify"] | "none", notify)) {
        out["ok"] = false;
        out["err"] = "notify_none_unit_attention_remount_cycle";
        sendJson(400, out);
        return;
    }
    // Offline capture turns WiFi off: arm with delay_s so the change lands mid-capture.
    const float delayS = in["delay_s"] | 0.0f;
    ProbeMedia& media = ProbeMedia::instance();
    ProbeMedia::Mutation m;
    String err;
    bool ok = media.parseMutation(in.as<JsonVariantConst>(), m, &err) && msc_ &&
              msc_->mutateMedia(m, notify, delayS > 0 ? (uint32_t)(delayS * 1000.0f) : 0u, &err);
    if (!ok && !err.length()) err = "msc_not_ready";
    out["ok"] = ok;
    if (!ok) out["err"] = err;
    out["notify"] = MscGadget::notifyName(notify);
    out["scheduled"] = msc_ ? msc_->mutationPending() : false;
    out["due_in_ms"] = msc_ ? msc_->mutationDueInMs() : 0;
    media.toJson(out["active"].to<JsonObject>(), true);
    sendJson(ok ? 200 : 400, out);
}

void WebUi::handleMediaNotify() {
    JsonDocument out;
    JsonDocument in;
    const char* kind = "unit_attention";
    if (server_.hasArg("plain") && !deserializeJson(in, server_.arg("plain"))) {
        kind = in["kind"] | "unit_attention";
    }
    MscGadget::MediaNotify notify;
    if (!MscGadget::parseNotify(kind, notify) || notify == MscGadget::MediaNotify::None) {
        out["ok"] = false;
        out["err"] = "kind_unit_attention_remount_cycle";
        sendJson(400, out);
        return;
    }
    const bool ok = msc_ && msc_->notifyMedia(notify);
    out["ok"] = ok;
    out["kind"] = MscGadget::notifyName(notify);
    out["ua_pending"] = MscBotTap::unitAttentionPending();
    out["ua_sent"] = MscBotTap::unitAttentionSent();
    sendJson(ok ? 200 : 500, out);
}

void WebUi::handleStallGet() {
    JsonDocument doc;
    ReadStall::instance().toJson(doc["stall"].to<JsonObject>());
    sendJson(200, doc);
}

void WebUi::handleStallPost() {
    JsonDocument out;
    JsonDocument in;
    if (!server_.hasArg("plain") || deserializeJson(in, server_.arg("plain"))) {
        out["ok"] = false;
        out["err"] = "json";
        sendJson(400, out);
        return;
    }
    ReadStall& rs = ReadStall::instance();
    bool ok = true;
    String err;
    if (!(in["enable"] | true)) {
        rs.disable("api");
    } else if (SequenceRunner::instance().active()) {
        ok = false;
        err = "sequence_active";
    } else {
        ok = rs.configure(in.as<JsonVariantConst>(), &err);
    }
    out["ok"] = ok;
    if (!ok) out["err"] = err;
    rs.toJson(out["stall"].to<JsonObject>());
    sendJson(ok ? 200 : 400, out);
}

void WebUi::handleSequences() {
    JsonDocument doc;
    SequenceRunner::sequencesJson(doc["sequences"].to<JsonArray>());
    SequenceRunner::instance().toJson(doc["runner"].to<JsonObject>());
    sendJson(200, doc);
}

void WebUi::handleSequenceStart() {
    String vehicle = cfg_ ? cfg_->vehicleId : "lab_debian";
    String profile = cfg_ ? cfg_->profileId : "lab_debian";
    String id;
    bool autoArm = false;
    bool profileExplicit = false;
    if (server_.hasArg("plain")) {
        JsonDocument doc;
        if (!deserializeJson(doc, server_.arg("plain"))) {
            if (doc["id"].is<const char*>()) id = doc["id"].as<String>();
            if (doc["vehicle"].is<const char*>()) vehicle = doc["vehicle"].as<String>();
            if (doc["profile_id"].is<const char*>()) {
                profile = doc["profile_id"].as<String>();
                profileExplicit = true;
            }
            if (!doc["auto_arm"].isNull()) autoArm = doc["auto_arm"].as<bool>();
        }
    }
    if (!profileExplicit) profile = profileForVehicle(vehicle, profile);
    JsonDocument out;
    String err;
    const bool ok = SequenceRunner::instance().start(id.c_str(), vehicle, profile, autoArm, &err);
    out["ok"] = ok;
    if (!ok) out["err"] = err;
    SequenceRunner::instance().toJson(out["runner"].to<JsonObject>());
    sendJson(ok ? 200 : 409, out);
}

void WebUi::handleSequenceCmd(const char* cmd) {
    SequenceRunner& r = SequenceRunner::instance();
    if (strcmp(cmd, "next") == 0) {
        r.requestNext();
    } else if (strcmp(cmd, "skip") == 0) {
        r.skip();
    } else {
        r.abort("api_abort");
    }
    JsonDocument out;
    out["ok"] = true;
    out["cmd"] = cmd;
    r.toJson(out["runner"].to<JsonObject>());
    sendJson(200, out);
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
        o["end_on_unmount"] = !p.keepOnUnmount;
        o["free_markers"] = p.freeMarkers;
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
                              uint8_t markerCount, const char* experimentId,
                              bool endOnUnmount, bool freeMarkers, bool fineData) {
    OfflineCapture& oc = OfflineCapture::instance();
    if (oc.active()) {
        server_.send(409, "application/json", "{\"ok\":false,\"err\":\"already_active\"}");
        return;
    }
    if (SequenceRunner::instance().active()) {
        server_.send(409, "application/json", "{\"ok\":false,\"err\":\"sequence_active\"}");
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
    out["end_on_unmount"] = endOnUnmount;
    out["free_markers"] = freeMarkers;
    out["fine_data"] = fineData;
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
        const size_t cap = OfflineCapture::expectedCapBytes();
        budget["capacity_bytes"] = (uint32_t)cap;
        budget["capacity_target_bytes"] = (uint32_t)cap;
        budget["capacity_fallback_bytes"] = (uint32_t)OfflineCapture::fallbackCapBytes();
        budget["capacity_allocated"] = false;
        budget["record_size"] = (uint32_t)OfflineCapture::recordSize();
        budget["estimated_max_records"] = (uint32_t)OfflineCapture::maxRecordsForCap(cap);
        budget["fine_data"] = fineData;
        budget["est_bytes_per_s"] = OfflineCapture::estBytesPerS(fineData);
        budget["est_duration_s"] = OfflineCapture::estDurationS(cap, fineData);
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
                         markerCount, endOnUnmount, freeMarkers, fineData)) {
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
    bool fineData = false;
    if (server_.hasArg("plain")) {
        JsonDocument doc;
        if (!deserializeJson(doc, server_.arg("plain"))) {
            if (doc["vehicle"].is<const char*>()) vehicle = doc["vehicle"].as<String>();
            if (doc["scenario"].is<const char*>()) scenario = doc["scenario"].as<String>();
            if (!doc["fine_data"].isNull()) fineData = doc["fine_data"].as<bool>();
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
                      markerCount, exp ? exp->id : nullptr, exp ? !exp->keepOnUnmount : true,
                      exp ? exp->freeMarkers : false, fineData);
}

void WebUi::handleExperimentStart() {
    OfflineCapture& oc = OfflineCapture::instance();
    if (oc.active()) {
        server_.send(409, "application/json", "{\"ok\":false,\"err\":\"already_active\"}");
        return;
    }
    String vehicle = cfg_ ? cfg_->vehicleId : "bmw";
    String experimentId = "free";
    String profile = cfg_ ? cfg_->profileId : "bmw_nbt_evo";
    bool softUsb = false;
    uint32_t measureMs = 0;
    String endStrategy = "media_hide";
    bool softExplicit = false;
    bool endExplicit = false;
    bool measureExplicit = false;
    bool endOnUnmount = true;
    bool unmountExplicit = false;
    bool fineData = false;

    if (server_.hasArg("plain")) {
        JsonDocument doc;
        if (!deserializeJson(doc, server_.arg("plain"))) {
            if (doc["vehicle"].is<const char*>()) vehicle = doc["vehicle"].as<String>();
            if (!doc["fine_data"].isNull()) fineData = doc["fine_data"].as<bool>();
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
            if (!doc["end_on_unmount"].isNull()) {
                endOnUnmount = doc["end_on_unmount"].as<bool>();
                unmountExplicit = true;
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
    if (!unmountExplicit) endOnUnmount = !exp->keepOnUnmount;
    if (measureMs > OfflineCapture::maxCaptureMs()) {
        measureMs = OfflineCapture::maxCaptureMs();
    }

    armOfflineCapture(vehicle, exp->scenario, profile, softUsb, measureMs, endStrategy,
                      exp->markers, exp->markerCount, exp->id, endOnUnmount, exp->freeMarkers,
                      fineData);
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
    PanicTail::noteRestart(PanicTail::RestartWebUi);
    delay(400);
    ESP.restart();
}
