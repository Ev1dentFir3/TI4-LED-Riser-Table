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
.player-swatch {
  display: flex; align-items: center; gap: 7px;
  padding: 5px 6px; margin-bottom: 3px;
  border: 2px solid transparent; border-radius: 4px;
  cursor: pointer; font-size: 0.75rem; background: #0f1820;
  transition: border-color 0.12s;
}
.player-swatch:hover { border-color: #4a9acc; }
.player-swatch.selected { border-color: #ffffff; font-weight: bold; }
.swatch-dot { width: 12px; height: 12px; border-radius: 50%; flex-shrink: 0; }
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
      <button id="fx-LIFE"    onclick="fx('LIFE')">Life</button>
      <button id="fx-RIPPLE"  onclick="fx('RIPPLE')">Ripple</button>
      <button id="fx-SPARKLE" onclick="fx('SPARKLE')">Sparkle</button>
      <button id="fx-WAVE"    onclick="fx('WAVE')">Wave</button>
      <button class="stop"    onclick="fx('NONE')">&#9632; Stop</button>
    </div>
  </div>

  <hr>

  <div class="lbl">Selected Hex</div>
  <div id="hex-info">Click any hex</div>

  <div>
    <div class="lbl">Players</div>
    <div id="player-swatches"><span class="sublbl">Waiting for hardware&hellip;</span></div>
  </div>

  <button id="btn-all-sides" onclick="applyAllSides()" style="width:100%;margin-top:4px">Claim Hex (All Sides)</button>
  <div class="two-col" style="margin-top:3px">
    <button onclick="applySide()">This Side</button>
    <button class="dim" onclick="clearSides()">Clear Sides</button>
  </div>

  <hr>
  <div class="lbl">Battle</div>
  <div class="two-col">
    <select id="battle-atk" style="background:#0f1820;color:#99bbcc;border:1px solid #2a4a6a;border-radius:4px;font-size:0.72rem;padding:3px"></select>
    <select id="battle-def" style="background:#0f1820;color:#99bbcc;border:1px solid #2a4a6a;border-radius:4px;font-size:0.72rem;padding:3px"></select>
  </div>
  <div class="two-col" style="margin-top:3px">
    <button onclick="startBattleWeb()" style="border-color:#6a3a1a;color:#ffcc88">&#9876; Battle</button>
    <button class="stop" onclick="send('ENDBATTLE')">End Battle</button>
  </div>

  <hr>
  <button id="btn-clear-all" class="dim" style="width:100%" onclick="onClearAllClick()">Clear All</button>
  <a href="/settings" style="display:block;margin-top:10px;text-align:center;color:#888;font-size:.8em;text-decoration:none">&#9881; Settings</a>

  <hr>
  <div class="lbl">Side</div>
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
  fetch('/getsettings', {cache: 'no-store'})
    .then(function (r) { return r.json(); })
    .then(function (cfg) { buildGrid(cfg.sideGap != null ? +cfg.sideGap : 4); })
    .catch(function ()  { buildGrid(4); });

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
var selectedHex        = -1;
var selectedSide       = 0;
var prevSelPoly        = null;
var SIDE_NAMES         = ['Top (0)','TR (1)','BR (2)','Bot (3)','BL (4)','TL (5)'];
var _selectedPlayerIdx = -1;
var _selectedPlayerRGB = [255, 255, 255];
var _clearStep         = 0;
var _clearTimer        = null;
var _clearLabels       = ['Clear All','Are you sure?','Are you really sure?','This CANNOT be undone!'];
var _clearBgs          = ['','#2a1a10','#3a1510','#4a1010'];
var _clearBorders      = ['','#553333','#883333','#aa3333'];

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
// Player swatches
// ============================================================
function hexToRgb(h) {
  var n = parseInt(h, 16);
  return [(n >> 16) & 0xFF, (n >> 8) & 0xFF, n & 0xFF];
}
function selectPlayer(idx, r, g, b) {
  _selectedPlayerIdx = idx;
  _selectedPlayerRGB = [r, g, b];
  document.querySelectorAll('.player-swatch').forEach(function (el) {
    el.classList.toggle('selected', parseInt(el.getAttribute('data-idx')) === idx);
  });
}
function fetchAndRenderPlayers() {
  fetch('/players', {cache: 'no-store'})
    .then(function (r) { return r.json(); })
    .then(function (data) {
      var c = document.getElementById('player-swatches');
      if (!c) return;
      c.innerHTML = '';
      if (!data.players || !data.players.length) {
        c.innerHTML = '<span class="sublbl">No active players</span>';
        return;
      }
      data.players.forEach(function (p) {
        var rgb = hexToRgb(p.color);
        var el = document.createElement('div');
        el.className = 'player-swatch' + (_selectedPlayerIdx === p.idx ? ' selected' : '');
        el.setAttribute('data-idx', p.idx);
        el.onclick = (function (i, r, g, b) { return function () { selectPlayer(i, r, g, b); }; })(p.idx, rgb[0], rgb[1], rgb[2]);
        var dot = document.createElement('div');
        dot.className = 'swatch-dot';
        dot.style.background = '#' + p.color;
        var lbl = document.createElement('span');
        lbl.textContent = 'Player ' + (p.idx + 1) + (p.locked ? ' \u2713' : '');
        el.appendChild(dot);
        el.appendChild(lbl);
        c.appendChild(el);
      });

      // Populate battle dropdowns
      ['battle-atk','battle-def'].forEach(function (id) {
        var sel = document.getElementById(id);
        if (!sel) return;
        var prev = sel.value;
        sel.innerHTML = '';
        data.players.forEach(function (p) {
          var opt = document.createElement('option');
          opt.value = p.idx;
          opt.textContent = 'P' + (p.idx + 1);
          sel.appendChild(opt);
        });
        if (prev !== '') sel.value = prev;
        // Default defender to second player
        if (id === 'battle-def' && data.players.length > 1) {
          if (!prev) sel.value = data.players[1].idx;
        }
      });
    })
    .catch(function () {});
}

function startBattleWeb() {
  var atk = document.getElementById('battle-atk').value;
  var def = document.getElementById('battle-def').value;
  if (atk === def) return;
  send('BATTLE:' + atk + ':' + def);
}

// ============================================================
// Hex color actions
// ============================================================
function applySide() {
  if (selectedHex < 0 || _selectedPlayerIdx < 0) return;
  var r = _selectedPlayerRGB[0], g = _selectedPlayerRGB[1], b = _selectedPlayerRGB[2];
  var hex = ('00' + r.toString(16)).slice(-2) + ('00' + g.toString(16)).slice(-2) + ('00' + b.toString(16)).slice(-2);
  send('SETHEXSIDE:' + selectedHex + ':' + selectedSide + ':' + hex.toUpperCase());
  setHexSide(selectedHex, selectedSide, r, g, b);
}
function applyAllSides() {
  if (selectedHex < 0 || _selectedPlayerIdx < 0) return;
  send('CLAIMHEX:' + selectedHex + ':' + _selectedPlayerIdx);
  setHexAllSides(selectedHex, _selectedPlayerRGB[0], _selectedPlayerRGB[1], _selectedPlayerRGB[2]);
}
function clearSides() {
  if (selectedHex < 0) return;
  send('CLAIMHEX:' + selectedHex + ':255');
  clearHexSides(selectedHex);
}

// ============================================================
// Clear All — 4-step escalating confirmation
// ============================================================
function _resetClearBtn() {
  _clearStep = 0;
  var btn = document.getElementById('btn-clear-all');
  btn.textContent = _clearLabels[0];
  btn.style.background = '';
  btn.style.borderColor = '';
  btn.style.color = '';
}
function onClearAllClick() {
  if (_clearTimer) { clearTimeout(_clearTimer); _clearTimer = null; }
  _clearStep++;
  if (_clearStep >= _clearLabels.length) {
    send('ALL:000000');
    setActiveFxBtn('');
    _resetClearBtn();
    return;
  }
  var btn = document.getElementById('btn-clear-all');
  btn.textContent = _clearLabels[_clearStep];
  btn.style.background  = _clearBgs[_clearStep];
  btn.style.borderColor = _clearBorders[_clearStep];
  btn.style.color = _clearStep >= 2 ? '#ffaaaa' : '#cc8888';
  _clearTimer = setTimeout(_resetClearBtn, 3000);
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
  ['RAINBOW','PULSE','LIFE','RIPPLE','SPARKLE','WAVE'].forEach(function (n) {
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
        var byteOffset = i * 6;
        setHexAllSides(i,
          parseInt(data.slice(byteOffset,     byteOffset + 2), 16),
          parseInt(data.slice(byteOffset + 2, byteOffset + 4), 16),
          parseInt(data.slice(byteOffset + 4, byteOffset + 6), 16));
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

setInterval(fetchAndRenderPlayers, 5000);
fetchAndRenderPlayers();
</script>
</body>
</html>
)=====";
