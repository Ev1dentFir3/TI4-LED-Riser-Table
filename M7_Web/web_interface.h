#pragma once

// =============================================================================
// TI4 Hex Riser - Web Interface
// =============================================================================
// Hex grid rendering taken from user-provided 61_hex_grid.html reference.
// Flat-top hexes, columns 5-6-7-8-9-8-7-6-5, per-side coloring supported.
//
// Animations run client-side in the browser (no Arduino needed to preview).
// When Arduino IS connected the EFFECT command is also sent to hardware.
//
// WebSocket protocol:
//   Arduino → Browser:  HEX:nn:RRGGBB        hex fill color
//                        HEXSIDE:nn:s:RRGGBB  one side color
//                        ALL:RRGGBB            all hexes
//                        BRIGHTNESS:nnn
//   Browser → Arduino:  SELECT:nn
//                        SETHEX:nn:RRGGBB
//                        SETHEXSIDE:nn:s:RRGGBB
//                        BRIGHTNESS:nnn
//                        EFFECT:name  (RAINBOW|PULSE|SPIRAL|SPARKLE|WAVE|NONE)
// =============================================================================

const char WEB_PAGE[] = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>TI4 Hex Riser</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  display: flex;
  height: 100vh;
  overflow: hidden;
  background: #1a2a3a;
  font-family: sans-serif;
  color: #ccd6e0;
}
#sidebar {
  width: 210px;
  min-width: 180px;
  background: #0f1820;
  border-right: 1px solid #223344;
  padding: 12px 10px;
  display: flex;
  flex-direction: column;
  gap: 12px;
  overflow-y: auto;
  flex-shrink: 0;
}
h1 { font-size: 0.85rem; letter-spacing: 0.12em; text-transform: uppercase; color: #4a9acc; }
.lbl { font-size: 0.68rem; text-transform: uppercase; letter-spacing: 0.09em; color: #4a6070; margin-bottom: 2px; }
.sublbl { font-size: 0.72rem; color: #556677; }
hr { border: none; border-top: 1px solid #223344; }
#status-row { display: flex; align-items: center; gap: 6px; font-size: 0.78rem; }
.dot { width: 8px; height: 8px; border-radius: 50%; background: #664444; flex-shrink: 0; transition: background 0.3s; }
.dot.on { background: #33bb66; }
#ws-label { color: #667788; }
.bright-row { display: flex; align-items: center; gap: 8px; }
input[type=range] { flex: 1; accent-color: #4a9acc; }
.bright-row span { font-size: 0.78rem; color: #8899aa; min-width: 24px; text-align: right; }
.fx-grid { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 3px; }
button {
  padding: 5px 3px;
  border: 1px solid #2a4a6a;
  background: #152535;
  color: #99bbcc;
  border-radius: 4px;
  font-size: 0.72rem;
  cursor: pointer;
  transition: background 0.1s;
}
button:hover { background: #1e3a55; color: #ddeeff; }
button.active-fx { background: #1a4a6a; border-color: #4a9acc; color: #ffffff; }
button.stop { border-color: #6a2a2a; background: #2a1010; color: #ee8888; }
button.stop:hover { background: #3a1515; }
button.dim { border-color: #223344; background: #0f1820; color: #445566; }
button.dim:hover { color: #8899aa; }
.two-col { display: grid; grid-template-columns: 1fr 1fr; gap: 3px; }
#hex-info {
  background: #0a1520;
  border: 1px solid #223344;
  border-radius: 5px;
  padding: 8px;
  font-size: 0.78rem;
  color: #667788;
  min-height: 2em;
}
#hex-info strong { color: #99ccee; font-size: 0.95rem; }
input[type=color] {
  width: 100%;
  height: 30px;
  border: 1px solid #2a4a6a;
  border-radius: 4px;
  background: #0f1820;
  cursor: pointer;
  padding: 2px;
}
.side-grid { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 3px; }
.side-grid .gap { }
.side-btn { font-size: 0.7rem; padding: 5px 2px; }
.side-btn.active { background: #1a5a8a; border-color: #4a9acc; color: #ffffff; }
#main { flex: 1; display: flex; align-items: center; justify-content: center; overflow: hidden; padding: 10px; }
#hex-wrap { flex: 1; width: 100%; height: 100%; display: flex; align-items: center; justify-content: center; }
.hex {
  fill: #2a3a4a;
  stroke: #8899aa;
  stroke-width: 1.5;
  transition: fill 0.05s;
  cursor: pointer;
}
.hex:hover { stroke: #ccddee; stroke-width: 2; }
.hex.selected { fill: #2d4a5e; }
.hex-side {
  stroke: transparent;
  stroke-width: 5;
  stroke-linecap: round;
  fill: none;
  transition: stroke 0.05s;
  pointer-events: none;
}
#hex-svg text {
  font-family: sans-serif;
  font-size: 13px;
  font-weight: 500;
  fill: #ffffff;
  text-anchor: middle;
  dominant-baseline: central;
  pointer-events: none;
}
</style>
</head>
<body>

<div id="sidebar">
  <h1>TI4 Hex Riser</h1>

  <div id="status-row">
    <div class="dot" id="ws-dot"></div>
    <span id="ws-label">Hardware offline</span>
  </div>

  <hr>

  <div>
    <div class="lbl">Brightness</div>
    <div class="bright-row">
      <input type="range" id="brightness" min="0" max="200" value="128">
      <span id="bright-val">128</span>
    </div>
  </div>

  <div>
    <div class="lbl">Effects</div>
    <div class="fx-grid">
      <button id="fx-RAINBOW" onclick="fx('RAINBOW')">Rainbow</button>
      <button id="fx-PULSE"   onclick="fx('PULSE')">Pulse</button>
      <button id="fx-SPIRAL"  onclick="fx('SPIRAL')">Spiral</button>
      <button id="fx-SPARKLE" onclick="fx('SPARKLE')">Sparkle</button>
      <button id="fx-WAVE"    onclick="fx('WAVE')">Wave</button>
      <button class="stop"    onclick="fx('NONE')">&#9632; Stop</button>
    </div>
  </div>

  <hr>

  <div class="lbl">Selected Hex</div>
  <div id="hex-info">Click any hex</div>

  <div>
    <div class="lbl">Side Color</div>
    <div class="side-grid">
      <div class="gap"></div>
      <button class="side-btn" data-side="0">Top</button>
      <div class="gap"></div>
      <button class="side-btn" data-side="5">TL</button>
      <div class="gap"></div>
      <button class="side-btn" data-side="1">TR</button>
      <button class="side-btn" data-side="4">BL</button>
      <div class="gap"></div>
      <button class="side-btn" data-side="2">BR</button>
      <div class="gap"></div>
      <button class="side-btn" data-side="3">Bot</button>
      <div class="gap"></div>
    </div>
    <div class="sublbl" style="margin-top:3px">Side: <span id="sel-side-name">Top (0)</span></div>
    <input type="color" id="side-color" value="#0088ff" style="margin-top:4px">
    <div class="two-col" style="margin-top:3px">
      <button onclick="applySide()">This Side</button>
      <button onclick="applyAllSides()">All Sides</button>
    </div>
    <button class="dim" style="margin-top:3px;width:100%" onclick="clearSides()">Clear Sides</button>
  </div>

  <hr>
  <button class="dim" style="width:100%" onclick="clearAllHexes()">Clear All</button>
  <a href="/settings" style="display:block;margin-top:10px;text-align:center;color:#888;font-size:.8em;text-decoration:none">&#9881; Settings</a>
</div>

<div id="main">
  <div id="hex-wrap"><svg id="hex-svg" width="100%"></svg></div>
</div>

<script>
// ============================================================
// Hex Grid — fetches sideGap from /getsettings then builds SVG
// ============================================================
(function () {
  var svg = document.getElementById('hex-svg');
  var NS  = 'http://www.w3.org/2000/svg';
  var hexRadius     = 36;
  var hexHeight     = hexRadius * Math.sqrt(3);
  var columnSpacing = 1.5 * hexRadius;
  var columns = [
    { count:5, start: 0, topStart:true  },
    { count:6, start: 5, topStart:false },
    { count:7, start:11, topStart:true  },
    { count:8, start:18, topStart:false },
    { count:9, start:26, topStart:true  },
    { count:8, start:35, topStart:false },
    { count:7, start:43, topStart:true  },
    { count:6, start:50, topStart:false },
    { count:5, start:56, topStart:true  }
  ];
  var maxColHeight = 9;
  var viewboxW  = (columns.length - 1) * columnSpacing + 2 * hexRadius + 20;
  var viewboxH  = maxColHeight * hexHeight + 10 + 20;
  svg.setAttribute('viewBox', '0 0 ' + viewboxW + ' ' + viewboxH);
  svg.setAttribute('style', 'width:100%;height:100%;');
  var firstColCX  = hexRadius + 10;
  var gridCenterY = 10 + (maxColHeight * hexHeight) / 2;

  function getCorners(cx, cy, r) {
    var pts = [];
    for (var i = 0; i < 6; i++) {
      var a = Math.PI / 180 * (60 * i + 240);
      pts.push({ x: cx + r * Math.cos(a), y: cy + r * Math.sin(a) });
    }
    return pts;
  }
  function pointsStr(cx, cy) {
    return getCorners(cx, cy, hexRadius - 1)
      .map(function (p) { return p.x.toFixed(2) + ',' + p.y.toFixed(2); }).join(' ');
  }

  // Flat cache: _sideEls[hexIdx * 6 + sideIdx] → SVG line element reference.
  // Eliminates getElementById on every poll frame (was 366 lookups per 100ms).
  var _sideEls = new Array(61 * 6).fill(null);

  // Packed RGB per hex for diffing: skip DOM writes when color hasn't changed.
  // -1 = unknown (forces a write on first poll after build).
  var _prevColors = new Int32Array(61).fill(-1);

  function buildGrid(sideGap) {
    console.log('[Grid] buildGrid called, sideGap=' + sideGap);
    // Clear any previous build and reset caches
    while (svg.lastChild) svg.removeChild(svg.lastChild);
    _sideEls.fill(null);
    _prevColors.fill(-1);

    columns.forEach(function (col, colIdx) {
      var cx       = firstColCX + colIdx * columnSpacing;
      var colTopCY = gridCenterY - (col.count * hexHeight) / 2 + hexHeight / 2;
      for (var rowIdx = 0; rowIdx < col.count; rowIdx++) {
        var hexIdx  = col.topStart ? col.start + rowIdx : col.start + (col.count - 1 - rowIdx);
        var cy      = colTopCY + rowIdx * hexHeight;
        var corners = getCorners(cx, cy, hexRadius - sideGap);
        var hexGroup = document.createElementNS(NS, 'g');
        hexGroup.setAttribute('id', 'hex-' + hexIdx);
        hexGroup.setAttribute('data-index', hexIdx);
        hexGroup.addEventListener('click', (function (i) { return function () { selectHex(i); }; })(hexIdx));
        var poly = document.createElementNS(NS, 'polygon');
        poly.setAttribute('points', pointsStr(cx, cy));
        poly.setAttribute('class', 'hex');
        poly.setAttribute('id', 'hex-poly-' + hexIdx);
        hexGroup.appendChild(poly);
        for (var sideIdx = 0; sideIdx < 6; sideIdx++) {
          var cornerA = corners[sideIdx], cornerB = corners[(sideIdx + 1) % 6];
          var line = document.createElementNS(NS, 'line');
          line.setAttribute('class', 'hex-side');
          line.setAttribute('x1', cornerA.x.toFixed(2)); line.setAttribute('y1', cornerA.y.toFixed(2));
          line.setAttribute('x2', cornerB.x.toFixed(2)); line.setAttribute('y2', cornerB.y.toFixed(2));
          _sideEls[hexIdx * 6 + sideIdx] = line;  // cache the reference
          hexGroup.appendChild(line);
        }
        var hexLabel = document.createElementNS(NS, 'text');
        hexLabel.setAttribute('x', cx.toFixed(2)); hexLabel.setAttribute('y', cy.toFixed(2));
        hexLabel.textContent = hexIdx;
        hexGroup.appendChild(hexLabel);
        svg.appendChild(hexGroup);
      }
    });
  }

  // Expose so a live preview button could call this in future
  window.buildHexGrid = buildGrid;

  // Fetch current sideGap from settings, then draw
  console.log('[Grid] fetching /getsettings...');
  fetch('/getsettings', {cache: 'no-store'})
    .then(function (r) {
      console.log('[Grid] /getsettings status=' + r.status);
      return r.json();
    })
    .then(function (cfg) {
      console.log('[Grid] settings JSON ok, sideGap=' + cfg.sideGap);
      buildGrid(cfg.sideGap != null ? +cfg.sideGap : 4);
    })
    .catch(function (e) {
      console.warn('[Grid] /getsettings failed:', e, '— using default sideGap=4');
      buildGrid(4);
    });

  // Bridge functions — use cached element refs, skip DOM write when color unchanged.
  window.setHexAllSides = function (i, r, g, b) {
    var packed = (r << 16) | (g << 8) | b;
    if (_prevColors[i] === packed) return;  // nothing changed, skip
    _prevColors[i] = packed;
    var stroke = 'rgb(' + r + ',' + g + ',' + b + ')';
    var base = i * 6;
    for (var s = 0; s < 6; s++) {
      var el = _sideEls[base + s];
      if (el) el.style.stroke = stroke;
    }
  };
  window.setHexSide = function (i, s, r, g, b) {
    _prevColors[i] = -1;  // invalidate so next setHexAllSides rewrites all sides
    var el = _sideEls[i * 6 + s];
    if (el) el.style.stroke = 'rgb(' + r + ',' + g + ',' + b + ')';
  };
  window.clearHexSides = function (i) {
    _prevColors[i] = -1;
    var base = i * 6;
    for (var s = 0; s < 6; s++) {
      var el = _sideEls[base + s];
      if (el) el.style.stroke = 'transparent';
    }
  };
  window.setAllSides = function (r, g, b) {
    var packed = (r << 16) | (g << 8) | b;
    var stroke = 'rgb(' + r + ',' + g + ',' + b + ')';
    for (var i = 0; i < 61; i++) {
      if (_prevColors[i] === packed) continue;
      _prevColors[i] = packed;
      var base = i * 6;
      for (var s = 0; s < 6; s++) {
        var el = _sideEls[base + s];
        if (el) el.style.stroke = stroke;
      }
    }
  };
  window.clearAllSides = function () {
    _prevColors.fill(-1);
    for (var i = 0; i < 61 * 6; i++) {
      if (_sideEls[i]) _sideEls[i].style.stroke = 'transparent';
    }
  };
})();

// ============================================================
// State
// ============================================================
var selectedHex  = -1;
var selectedSide = 0;
var prevSelPoly  = null;
var SIDE_NAMES   = ['Top (0)','TR (1)','BR (2)','Bot (3)','BL (4)','TL (5)'];

// ============================================================
// Hex selection
// ============================================================
function selectHex(idx) {
  if (prevSelPoly) prevSelPoly.classList.remove('selected');
  selectedHex = idx;
  var poly = document.getElementById('hex-poly-' + idx);
  if (poly) { poly.classList.add('selected'); prevSelPoly = poly; }
  document.getElementById('hex-info').innerHTML = '<strong>Hex ' + idx + '</strong>';
}

// ============================================================
// Side buttons
// ============================================================
var sideBtns = document.querySelectorAll('.side-btn');
sideBtns.forEach(function (btn) {
  btn.addEventListener('click', function () {
    sideBtns.forEach(function (b) { b.classList.remove('active'); });
    btn.classList.add('active');
    selectedSide = parseInt(btn.getAttribute('data-side'));
    document.getElementById('sel-side-name').textContent = SIDE_NAMES[selectedSide];
  });
});
document.querySelector('.side-btn[data-side="0"]').classList.add('active');

// ============================================================
// Fill / side color actions
// ============================================================
function applySide() {
  if (selectedHex < 0) return;
  var hex = document.getElementById('side-color').value.slice(1).toUpperCase();
  send('SETHEXSIDE:' + selectedHex + ':' + selectedSide + ':' + hex);
  setHexSide(selectedHex, selectedSide, parseInt(hex.slice(0,2),16), parseInt(hex.slice(2,4),16), parseInt(hex.slice(4,6),16));
}
function applyAllSides() {
  if (selectedHex < 0) return;
  var hex = document.getElementById('side-color').value.slice(1).toUpperCase();
  var r = parseInt(hex.slice(0,2),16), g = parseInt(hex.slice(2,4),16), b = parseInt(hex.slice(4,6),16);
  for (var s = 0; s < 6; s++) send('SETHEXSIDE:' + selectedHex + ':' + s + ':' + hex);
  setHexAllSides(selectedHex, r, g, b);
}
function clearSides() {
  if (selectedHex < 0) return;
  for (var s = 0; s < 6; s++) send('SETHEXSIDE:' + selectedHex + ':' + s + ':000000');
  clearHexSides(selectedHex);
}
function clearAllHexes() {
  send('ALL:000000');
  setActiveFxBtn('');
}

// ============================================================
// Brightness
// ============================================================
var brightnessSlider   = document.getElementById('brightness');
var brightnessDisplay  = document.getElementById('bright-val');
brightnessSlider.addEventListener('input', function () {
  brightnessDisplay.textContent = brightnessSlider.value;
  send('BRIGHTNESS:' + brightnessSlider.value);
});

// ============================================================
// Effect button highlight — purely cosmetic, no local animation
// ============================================================
function setActiveFxBtn(name) {
  ['RAINBOW','PULSE','SPIRAL','SPARKLE','WAVE'].forEach(function (n) {
    var el = document.getElementById('fx-' + n);
    if (el) el.classList.toggle('active-fx', n === name);
  });
}

// Called by sidebar buttons — sends command to Arduino, browser updates via SSE
function fx(name) {
  send('EFFECT:' + name);
  setActiveFxBtn(name);
}

// ============================================================
// SSE (server → browser state push via /events)
// Commands sent browser → server via fetch('/cmd?msg=...')
// EventSource auto-reconnects on drop — no manual retry needed.
// ============================================================
console.log('[M7] script reached SSE section');
var _online = false;

function setOnline(on) {
  if (on === _online) return;
  _online = on;
  document.getElementById('ws-dot').classList.toggle('on', on);
  document.getElementById('ws-label').textContent = on ? 'Hardware online' : 'Hardware offline';
}

function send(msg) {
  fetch('/cmd?msg=' + encodeURIComponent(msg)).catch(function () {});
}

function handleMessage(data) {
  if (data.startsWith('ALLHEX:')) {
    var hex = data.slice(7);
    for (var i = 0; i < 61 && (i + 1) * 6 <= hex.length; i++) {
      var o = i * 6;
      setHexAllSides(i,
        parseInt(hex.slice(o,     o + 2), 16),
        parseInt(hex.slice(o + 2, o + 4), 16),
        parseInt(hex.slice(o + 4, o + 6), 16));
    }

  } else if (data.startsWith('HEX:')) {
    var parts = data.split(':');
    if (parts.length >= 3) {
      var rgb = parts[2];
      setHexAllSides(parseInt(parts[1]),
        parseInt(rgb.slice(0, 2), 16),
        parseInt(rgb.slice(2, 4), 16),
        parseInt(rgb.slice(4, 6), 16));
    }

  } else if (data.startsWith('BRIGHTNESS:')) {
    var b = parseInt(data.slice(11));
    var sl = document.getElementById('brightness');
    if (sl) { sl.value = b; document.getElementById('bright-val').textContent = b; }

  } else if (data.startsWith('GAMESTATE:')) {
    try { var gs = JSON.parse(data.slice(10)); } catch(e) {}

  }
  // PLAYER: reserved for future game UI
}

var _evtSrc = null;
var _lastMsgMs = 0;

function connectSSE() {
  console.log('[SSE] connecting to /events...');
  if (_evtSrc) _evtSrc.close();
  _evtSrc = new EventSource('/events');
  _evtSrc.onopen = function () {
    console.log('[SSE] connected');
    _lastMsgMs = Date.now();
    setOnline(true);
  };
  _evtSrc.onerror = function (e) {
    console.warn('[SSE] error/closed, readyState=' + _evtSrc.readyState, e);
    setOnline(false);
  };
  _evtSrc.onmessage = function (evt) {
    _lastMsgMs = Date.now();
    handleMessage(evt.data);
  };
}

// Heartbeat watchdog: server sends SSE keepalive every 3s.
// If nothing arrives for 6s the board is offline (unplugged / lost).
setInterval(function () {
  if (!_online) return;
  if (_lastMsgMs > 0 && Date.now() - _lastMsgMs > 6000) {
    console.warn('[SSE] heartbeat timeout — board offline');
    setOnline(false);
  }
}, 1000);

// Delay SSE connect so page + /getsettings finish before /events opens.
// Nina's TCP backlog is 1 — simultaneous connections drop silently.
setTimeout(connectSSE, 800);
</script>
</body>
</html>
)=====";
