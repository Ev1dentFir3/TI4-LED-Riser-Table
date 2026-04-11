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
  var maxH = 9;
  var vbW  = (columns.length - 1) * columnSpacing + 2 * hexRadius + 20;
  var vbH  = maxH * hexHeight + 10 + 20;
  svg.setAttribute('viewBox', '0 0 ' + vbW + ' ' + vbH);
  svg.setAttribute('style', 'width:100%;height:100%;');
  var firstCX = hexRadius + 10;
  var gridCY  = 10 + (maxH * hexHeight) / 2;

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

  function buildGrid(sideGap) {
    // Clear any previous build
    while (svg.lastChild) svg.removeChild(svg.lastChild);
    columns.forEach(function (col, ci) {
      var cx  = firstCX + ci * columnSpacing;
      var cy0 = gridCY - (col.count * hexHeight) / 2 + hexHeight / 2;
      for (var ri = 0; ri < col.count; ri++) {
        var idx = col.topStart ? col.start + ri : col.start + (col.count - 1 - ri);
        var cy  = cy0 + ri * hexHeight;
        var corners = getCorners(cx, cy, hexRadius - sideGap);
        var g = document.createElementNS(NS, 'g');
        g.setAttribute('id', 'hex-' + idx);
        g.setAttribute('data-index', idx);
        g.addEventListener('click', (function (i) { return function () { selectHex(i); }; })(idx));
        var poly = document.createElementNS(NS, 'polygon');
        poly.setAttribute('points', pointsStr(cx, cy));
        poly.setAttribute('class', 'hex');
        poly.setAttribute('id', 'hex-poly-' + idx);
        g.appendChild(poly);
        for (var si = 0; si < 6; si++) {
          var p1 = corners[si], p2 = corners[(si + 1) % 6];
          var line = document.createElementNS(NS, 'line');
          line.setAttribute('id', 'hex-side-' + idx + '-' + si);
          line.setAttribute('class', 'hex-side');
          line.setAttribute('x1', p1.x.toFixed(2)); line.setAttribute('y1', p1.y.toFixed(2));
          line.setAttribute('x2', p2.x.toFixed(2)); line.setAttribute('y2', p2.y.toFixed(2));
          g.appendChild(line);
        }
        var lbl = document.createElementNS(NS, 'text');
        lbl.setAttribute('x', cx.toFixed(2)); lbl.setAttribute('y', cy.toFixed(2));
        lbl.textContent = idx;
        g.appendChild(lbl);
        svg.appendChild(g);
      }
    });
  }

  // Expose so a live preview button could call this in future
  window.buildHexGrid = buildGrid;

  // Fetch current sideGap from settings, then draw
  fetch('/getsettings', {cache: 'no-store'})
    .then(function (r) { return r.json(); })
    .then(function (cfg) { buildGrid(cfg.sideGap != null ? +cfg.sideGap : 4); })
    .catch(function ()  { buildGrid(4); });

  // Bridge functions (match 61_hex_grid.html API exactly)
  window.setHexSide = function (i, s, r, g, b) {
    var el = document.getElementById('hex-side-' + i + '-' + s);
    if (el) el.style.stroke = 'rgb(' + r + ',' + g + ',' + b + ')';
  };
  window.setHexAllSides = function (i, r, g, b) {
    for (var s = 0; s < 6; s++) {
      var el = document.getElementById('hex-side-' + i + '-' + s);
      if (el) el.style.stroke = 'rgb(' + r + ',' + g + ',' + b + ')';
    }
  };
  window.clearHexSides = function (i) {
    for (var s = 0; s < 6; s++) {
      var el = document.getElementById('hex-side-' + i + '-' + s);
      if (el) el.style.stroke = 'transparent';
    }
  };
  window.setAllSides = function (r, g, b) {
    document.querySelectorAll('.hex-side').forEach(function (el) {
      el.style.stroke = 'rgb(' + r + ',' + g + ',' + b + ')';
    });
  };
  window.clearAllSides = function () {
    document.querySelectorAll('.hex-side').forEach(function (el) {
      el.style.stroke = 'transparent';
    });
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
var bSlider = document.getElementById('brightness');
var bVal    = document.getElementById('bright-val');
bSlider.addEventListener('input', function () {
  bVal.textContent = bSlider.value;
  send('BRIGHTNESS:' + bSlider.value);
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
// HTTP polling (server -> browser) — replaces SSE
// Short-lived GET /poll every 100 ms; no persistent connection.
// ============================================================
var _pollBusy = false;
var _online   = false;

function setOnline(on) {
  if (on === _online) return;
  _online = on;
  document.getElementById('ws-dot').classList.toggle('on', on);
  document.getElementById('ws-label').textContent = on ? 'Hardware online' : 'Hardware offline';
}

function pollHex() {
  if (_pollBusy) return;          // skip if previous poll still in-flight
  _pollBusy = true;
  fetch('/poll', {cache: 'no-store'})
    .then(function (r) { return r.text(); })
    .then(function (data) {
      _pollBusy = false;
      setOnline(true);
      for (var i = 0; i < 61 && (i + 1) * 6 <= data.length; i++) {
        var o = i * 6;
        setHexAllSides(i,
          parseInt(data.slice(o,     o + 2), 16),
          parseInt(data.slice(o + 2, o + 4), 16),
          parseInt(data.slice(o + 4, o + 6), 16));
      }
    })
    .catch(function () {
      _pollBusy = false;
      setOnline(false);
    });
}

function send(msg) {
  fetch('/cmd?' + msg).catch(function () {});  // fire-and-forget; silent if offline
}

setInterval(pollHex, 100);
pollHex();  // immediate first poll
</script>
</body>
</html>
)=====";
