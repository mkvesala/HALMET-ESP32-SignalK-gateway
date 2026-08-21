#include "WebUIManager.h"
#include "helpers.h"
#include "version.h"

// Both senders hang off the one ADS1115, so one pair of scaling constants describes
// both. If that ever stops being true, fillTank() must take them as parameters.
static_assert(WaterSensor::CCS_CURRENT_A == VDOSensor::CCS_CURRENT_A &&
              WaterSensor::ADS_LSB_V     == VDOSensor::ADS_LSB_V,
              "WaterSensor and VDOSensor CCS/LSB constants disagree — see fillTank()");

// === S T A T I C ===

// Calibration page. Static apart from the numbers, so it lives in flash and is sent
// verbatim; every live value arrives from /status instead.
static const char PAGE_HTML[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>HALMET calibration</title>
<style>
body{background:#101314;color:#d6dbdd;font:13px/1.5 ui-monospace,Menlo,Consolas,monospace;margin:14px}
pre{white-space:pre;overflow-x:auto;margin:0}
</style></head><body><pre id="d">Loading...</pre>
<script>
var CAL=[],STEP=0,CAP=0;
function z(n){return (n<10?'0':'')+n;}
function hms(s){return z(Math.floor(s/3600))+':'+z(Math.floor(s/60)%60)+':'+z(s%60);}
function f(v,d){return (v===null||v===undefined)?'--':Number(v).toFixed(d);}
function p(s,n){s=String(s);while(s.length<n)s=' '+s;return s;}
function pe(s,n){s=String(s);while(s.length<n)s=s+' ';return s;}
function row(label,t){
  return pe(label,11)+p(f(t.ohms,1),8)+p(f(t.filt,1),8)+p(f(t.mv,1),8)
        +p(t.adc===null?'--':t.adc,7)+p(f(t.ratio,3),8)+'\n';
}
function note(label,t){
  var s=pe(label,11)+t.n+'/'+t.win+' samples, updated '+f(t.age_ms/1000,1)+' s ago';
  if(!t.ok)s+='   NO SENSOR';
  else if(t.age_ms>6000)s+='   STALE - open circuit?';
  else if(t.n<t.win)s+='   (unfiltered, filling window)';
  return s+'\n';
}
function calBlock(v){
  if(!CAL.length)return '';
  var s='\nWaterCal::OHMS   edit the first column in WaterProcessor.h\n';
  for(var i=0;i<CAL.length;i++){
    var last=(i===CAL.length-1);
    var hit=(v!==null&&v!==undefined)&&v>=CAL[i]&&(last||v<CAL[i+1]);
    s+=(hit?'> ':'  ')+p((i*STEP).toFixed(1),6)+' L   '+p(CAL[i].toFixed(1),7)+'\n';
  }
  return s;
}
function vol(t){
  if(t.litres===null||t.litres===undefined)return '';
  var s=pe('water vol:',11)+f(t.litres,1)+' L';
  if(CAP)s+=' of '+f(CAP,0)+' L';
  if(t.sat)s+='   SENDER CEILING - 57.5..80 L all read alike';
  return s+'\n';
}
function upd(){
  fetch('/status').then(function(r){return r.json();}).then(function(j){
    var s='HALMET calibration       fw '+j.version+'   up '+hms(j.uptime_s)+'\n\n';
    s+=pe('',11)+p('ohms',8)+p('filt',8)+p('mV',8)+p('adc',7)+p('ratio',8)+'\n';
    s+=row('Water  A2',j.water);
    s+=row('Fuel   A1',j.fuel);
    s+='\n'+note('water:',j.water)+note('fuel:',j.fuel)+vol(j.water);
    s+='\nSignalK '+(j.sk_open?'connected':'disconnected')
      +'      heap '+Math.round(j.heap/1024)+' kB'
      +'      water open-circuit limit '+f(j.water_max_ohms,0)+' ohm\n';
    s+=calBlock(j.water.filt===null?j.water.ohms:j.water.filt);
    document.getElementById('d').textContent=s;
  }).catch(function(){
    document.getElementById('d').textContent='fetch failed - device unreachable?';
  });
}
fetch('/cal').then(function(r){return r.json();}).then(function(a){CAL=a.ohms;STEP=a.step_l;CAP=a.cap_l;}).catch(function(){});
setInterval(upd,1009);upd();
</script></body></html>)HTML";

// === P U B L I C ===

// Constructor
WebUIManager::WebUIManager(DS18B20Processor  &ds18b20_proc,
                           VDOProcessor      &vdo_proc,
                           WaterProcessor    &water_proc,
                           HALMETPreferences &prefs,
                           SignalKBroker     &signalk)
    : _ds18b20_proc(ds18b20_proc)
    , _vdo_proc(vdo_proc)
    , _water_proc(water_proc)
    , _prefs(prefs)
    , _signalk(signalk)
{}

// Register routes and start the HTTP server — called once from initWifiServices()
void WebUIManager::begin() {
    _server.on("/",       [this]() { this->handlePage();   });
    _server.on("/status", [this]() { this->handleStatus(); });
    _server.on("/cal",    [this]() { this->handleCal();    });
    _server.begin();
}

// Dispatch pending HTTP requests — call from loop()
void WebUIManager::handleRequest() {
    _server.handleClient();
}

// === P R I V A T E ===

// Serve the static page straight from flash
void WebUIManager::handlePage() {
    _server.send_P(200, "text/html; charset=utf-8", PAGE_HTML);
}

// Live values for the page poller
void WebUIManager::handleStatus() {
    const unsigned long now = millis();

    _status_doc.clear();
    _status_doc["version"]        = FIRMWARE_VERSION;
    _status_doc["uptime_s"]       = now / 1000UL;
    _status_doc["heap"]           = ESP.getFreeHeap();
    _status_doc["sk_open"]        = _signalk.isOpen();
    _status_doc["water_max_ohms"] = (float)WaterSensor::MAX_OHMS;  // cast: avoids odr-using the constexpr member

    JsonObject w = _status_doc.createNestedObject("water");
    fillTank(w,
             _water_proc.getLastOhms(),
             _water_proc.getFilteredOhms(),
             _water_proc.getWaterLevelDelta().water_level_ratio,
             _water_proc.getSampleCount(),
             _water_proc.getWindowSize(),
             now - _water_proc.getLastUpdateMs(),
             _water_proc.available());

    // Water-only fields — fillTank() is shared with the fuel tank, which has no volume readout
    float water_l = _water_proc.getVolumeLitres();
    if (validf(water_l)) w["litres"] = water_l; else w["litres"] = nullptr;
    w["sat"] = _water_proc.isAboveFloatCeiling();

    JsonObject f = _status_doc.createNestedObject("fuel");
    fillTank(f,
             _vdo_proc.getLastOhms(),
             _vdo_proc.getFilteredOhms(),
             _vdo_proc.getFuelLevelDelta().fuel_level_ratio,
             _vdo_proc.getSampleCount(),
             _vdo_proc.getWindowSize(),
             now - _vdo_proc.getLastUpdateMs(),
             _vdo_proc.available());

    char out[1024];
    size_t n = serializeJson(_status_doc, out, sizeof(out));
    if (n >= sizeof(out) - 1) {
        _server.send(500, "text/plain", "status buffer too small");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json; charset=utf-8", out);
}

// Calibration table — static data, fetched once per page load
void WebUIManager::handleCal() {
    StaticJsonDocument<512> doc;
    doc["step_l"] = WaterCal::CAL_STEP_L;
    doc["cap_l"]  = WaterCal::TANK_CAPACITY_L;
    JsonArray arr = doc.createNestedArray("ohms");
    for (int i = 0; i < WaterCal::CAL_POINTS; i++) arr.add(WaterCal::OHMS[i]);

    char out[768];
    size_t n = serializeJson(doc, out, sizeof(out));
    if (n >= sizeof(out) - 1) {
        _server.send(500, "text/plain", "cal buffer too small");
        return;
    }
    _server.send(200, "application/json; charset=utf-8", out);
}

// One tank object. Volts and ADC counts are RECONSTRUCTED from ohms, not resampled —
// the sensor's raw-to-ohms map is linear and lossless, but note this bypasses the
// negative-voltage clamp in readResistance(), so a noisy zero reads back as 0 here.
void WebUIManager::fillTank(JsonObject o, float ohms, float filt, float ratio,
                            int samples, int window, unsigned long age_ms, bool ok) {
    if (validf(ohms)) {
        o["ohms"] = ohms;
        o["mv"]   = ohms * WaterSensor::CCS_CURRENT_A * 1000.0f;
        o["adc"]  = (int)lroundf(ohms / (WaterSensor::ADS_LSB_V / WaterSensor::CCS_CURRENT_A));
    } else {
        o["ohms"] = nullptr;
        o["mv"]   = nullptr;
        o["adc"]  = nullptr;
    }
    if (validf(filt))  o["filt"]  = filt;  else o["filt"]  = nullptr;
    if (validf(ratio)) o["ratio"] = ratio; else o["ratio"] = nullptr;
    o["n"]      = samples;
    o["win"]    = window;
    o["age_ms"] = age_ms;
    o["ok"]     = ok;
}
