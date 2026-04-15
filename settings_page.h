#pragma once

// =============================================================================
// TI4 Hex Riser - Settings Page HTML
// =============================================================================
// Served at GET /settings.  Uses the same dark theme as web_interface.h.
// Reads current values from GET /getsettings (JSON).
// Saves via GET /savesettings?key=val&...
// Reboots via GET /reboot.
// =============================================================================

const char SETTINGS_PAGE[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TI4 Riser - Settings</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:#111;color:#ccc;font-family:sans-serif;font-size:14px;padding:20px}
  h1{color:#e8c77a;font-size:1.3em;margin-bottom:18px;letter-spacing:1px}
  h2{color:#aaa;font-size:.9em;text-transform:uppercase;letter-spacing:1px;
     margin:22px 0 10px;border-bottom:1px solid #333;padding-bottom:5px}
  .row{display:flex;align-items:center;margin-bottom:8px;gap:10px}
  label{width:200px;color:#999;flex-shrink:0}
  input[type=text],input[type=password],input[type=number]{
    background:#1e1e1e;border:1px solid #444;color:#eee;
    padding:5px 8px;border-radius:4px;width:240px}
  input[type=checkbox]{width:18px;height:18px;cursor:pointer}
  .note{font-size:.75em;color:#666;margin-left:4px}
  .btn{padding:8px 20px;border:none;border-radius:4px;cursor:pointer;font-size:.9em;margin-right:8px}
  .btn-save{background:#2a5fa5;color:#fff}
  .btn-save:hover{background:#3a6fb5}
  .btn-reboot{background:#8b2020;color:#fff}
  .btn-reboot:hover{background:#a03030}
  .btn-back{background:#333;color:#ccc}
  .btn-back:hover{background:#444}
  #status{margin-top:14px;font-size:.85em;color:#6c6}
  #rebooting{display:none;color:#e8c77a;margin-top:14px}
</style>
</head>
<body>
<h1>&#9881; Settings</h1>

<h2>WiFi</h2>
<div class="row"><label>Home SSID</label><input type="text" id="homeSSID"><span class="note">leave blank to always use AP mode</span></div>
<div class="row"><label>Home Password</label><input type="password" id="homePass"></div>
<div class="row"><label>AP SSID</label><input type="text" id="apSSID"></div>
<div class="row"><label>AP Password</label><input type="password" id="apPass"></div>
<div class="row"><label>Home Timeout (ms)</label><input type="number" id="homeTimeoutMs" min="1000" max="30000" step="1000"></div>

<h2>LED</h2>
<div class="row"><label>Default Brightness</label><input type="number" id="defaultBrightness" min="0" max="255"></div>
<div class="row"><label>Max Brightness</label><input type="number" id="maxBrightness" min="0" max="255"></div>
<div class="row"><label>LED Update Rate (ms)</label><input type="number" id="ledUpdateMs" min="1" max="100"></div>
<div class="row"><label>Broadcast Rate (ms)</label><input type="number" id="broadcastMs" min="50" max="1000"></div>
<div class="row"><label>Side Gap</label><input type="number" id="sideGap" min="0" max="20"><span class="note">px inset on LED side lines (0 = touching, higher = more gap)</span></div>
<div class="row"><label>Simulate Hardware</label><input type="checkbox" id="simulateHardware"><span class="note">skip FastLED.show() calls</span></div>

<h2>Debug</h2>
<div class="row"><label>Debug Serial</label><input type="checkbox" id="debugSerial"></div>
<div class="row"><label>Debug Web</label><input type="checkbox" id="debugWeb"></div>
<div class="row"><label>Debug LED</label><input type="checkbox" id="debugLed"></div>
<div class="row"><label>Debug Keyboard</label><input type="checkbox" id="debugKeyboard"></div>

<br>
<button class="btn btn-save" onclick="saveSettings()">Save Settings</button>
<button class="btn btn-reboot" onclick="doReboot()">Reboot Board</button>
<button class="btn btn-back" onclick="location.href='/'">Back to Main</button>
<div id="status"></div>
<div id="rebooting">Rebooting... reconnecting in 8 seconds.</div>

<script>
function setStatus(msg, ok) {
  var el = document.getElementById('status');
  el.textContent = msg;
  el.style.color = ok ? '#6c6' : '#c66';
}

function loadSettings() {
  fetch('/getsettings').then(function(r){return r.json();}).then(function(d){
    document.getElementById('homeSSID').value        = d.homeSSID        || '';
    document.getElementById('homePass').value        = d.homePass        || '';
    document.getElementById('apSSID').value          = d.apSSID          || '';
    document.getElementById('apPass').value          = d.apPass          || '';
    document.getElementById('homeTimeoutMs').value   = d.homeTimeoutMs   || 10000;
    document.getElementById('defaultBrightness').value = d.defaultBrightness || 128;
    document.getElementById('maxBrightness').value   = d.maxBrightness   || 200;
    document.getElementById('ledUpdateMs').value     = d.ledUpdateMs     || 16;
    document.getElementById('broadcastMs').value     = d.broadcastMs     || 100;
    document.getElementById('sideGap').value         = d.sideGap         != null ? d.sideGap : 4;
    document.getElementById('simulateHardware').checked = d.simulateHardware || false;
    document.getElementById('debugSerial').checked   = d.debugSerial     || false;
    document.getElementById('debugWeb').checked      = d.debugWeb        || false;
    document.getElementById('debugLed').checked      = d.debugLed        || false;
    document.getElementById('debugKeyboard').checked = d.debugKeyboard   || false;
  }).catch(function(){setStatus('Could not load settings from board.', false);});
}

function encodeField(key, val) {
  return key + '=' + encodeURIComponent(val);
}

function saveSettings() {
  var params = [
    encodeField('homeSSID',          document.getElementById('homeSSID').value),
    encodeField('homePass',          document.getElementById('homePass').value),
    encodeField('apSSID',            document.getElementById('apSSID').value),
    encodeField('apPass',            document.getElementById('apPass').value),
    encodeField('homeTimeoutMs',     document.getElementById('homeTimeoutMs').value),
    encodeField('defaultBrightness', document.getElementById('defaultBrightness').value),
    encodeField('maxBrightness',     document.getElementById('maxBrightness').value),
    encodeField('ledUpdateMs',       document.getElementById('ledUpdateMs').value),
    encodeField('broadcastMs',       document.getElementById('broadcastMs').value),
    encodeField('sideGap',           document.getElementById('sideGap').value),
    encodeField('simulateHardware',  document.getElementById('simulateHardware').checked ? '1' : '0'),
    encodeField('debugSerial',       document.getElementById('debugSerial').checked ? '1' : '0'),
    encodeField('debugWeb',          document.getElementById('debugWeb').checked ? '1' : '0'),
    encodeField('debugLed',          document.getElementById('debugLed').checked ? '1' : '0'),
    encodeField('debugKeyboard',     document.getElementById('debugKeyboard').checked ? '1' : '0'),
  ].join('&');
  fetch('/savesettings?' + params)
    .then(function(){setStatus('Saved. Reboot the board for WiFi changes to take effect.', true);})
    .catch(function(){setStatus('Save failed.', false);});
}

function doReboot() {
  if (!confirm('Reboot the board now?')) return;
  fetch('/reboot').catch(function(){});
  document.getElementById('rebooting').style.display = 'block';
  setTimeout(function(){location.href='/';}, 8000);
}

loadSettings();
</script>
</body>
</html>
)rawhtml";
