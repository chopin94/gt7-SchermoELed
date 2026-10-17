#pragma once
#include <pgmspace.h>

// The web page of the strip: live telemetry, laps, LED settings, system.
// Static (no placeholders): it reads the settings from /api/status and the
// telemetry from the event stream /api/events. No external files, so it works
// on a network without internet. tools/screenshots/led-web.mjs takes it from
// here for the screenshot in the README.
static const char WEB_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0b0c10">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black">
<meta name="apple-mobile-web-app-title" content="GT7 LED">
<link rel="manifest" href="/manifest.webmanifest">
<link rel="apple-touch-icon" href="/apple-touch-icon.png">
<link rel="icon" href="/icon-192.png">
<title>GT7 LED Telemetria</title>
<style>
:root{--bg:#0b0c10;--card:#14161c;--line:#252832;--text:#e8e9ee;--dim:#8b8f9c;--red:#e10600;--green:#16c45a;--yellow:#ffc400;--purple:#b04dff;--blue:#2f8cff;--orange:#ff7a00}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font:14px/1.35 system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;min-height:100vh}
header{position:sticky;top:0;z-index:5;background:#0b0c10ee;backdrop-filter:blur(6px);border-bottom:1px solid var(--line)}
.bar{display:flex;align-items:center;gap:10px;padding:10px 16px;max-width:1280px;margin:0 auto}
.logo{font-weight:800;letter-spacing:.5px}.logo b{color:var(--red)}
.pill{white-space:nowrap;padding:3px 10px;border-radius:99px;font-size:12px;font-weight:600;background:#23262f;color:var(--dim)}
.pill.on{background:#0f3d22;color:#5ff59a}.pill.pause{background:#3d330f;color:#ffd75a}
.sp{flex:1}.small{font-size:12px;color:var(--dim)}
.dashbtn{white-space:nowrap;padding:6px 12px;border-radius:8px;background:var(--red);color:#fff;font-weight:700;font-size:13px;text-decoration:none}
@media(max-width:600px){#rate{display:none}.logo{white-space:nowrap}}
nav{display:flex;gap:4px;padding:0 12px;max-width:1280px;margin:0 auto;overflow-x:auto}
nav button{background:none;border:0;color:var(--dim);padding:9px 12px;font:inherit;font-weight:600;border-bottom:2px solid transparent;cursor:pointer;white-space:nowrap}
nav button.sel{color:var(--text);border-color:var(--red)}
main{max-width:1280px;margin:0 auto;padding:14px 16px 40px}
.tab{display:none}.tab.sel{display:block}
.grid{display:grid;gap:12px;grid-template-columns:repeat(12,1fr)}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:12px 14px;min-width:0}
.card h2{font-size:12px;text-transform:uppercase;letter-spacing:1px;color:var(--dim);margin-bottom:10px;display:flex;justify-content:space-between;gap:8px}
.c12{grid-column:span 12}.c8{grid-column:span 8}.c6{grid-column:span 6}.c4{grid-column:span 4}.c3{grid-column:span 3}
@media(max-width:900px){.c8,.c6,.c4{grid-column:span 12}.c3{grid-column:span 6}}
.hero{display:grid;grid-template-columns:auto 1fr auto;gap:14px;align-items:center}
.gear{font-size:84px;font-weight:800;line-height:1;width:84px;text-align:center;font-variant-numeric:tabular-nums}
.gear small{display:block;font-size:12px;color:var(--cyan,#3cc8ff);font-weight:600;height:14px}
.speed{font-size:40px;font-weight:700;font-variant-numeric:tabular-nums}.speed span{font-size:13px;color:var(--dim);margin-left:4px}
.rpmtxt{color:var(--dim);font-variant-numeric:tabular-nums}
.shift{display:flex;gap:3px;height:20px;margin:8px 0 4px}.shift i{flex:1;border-radius:3px;background:#22252d}
.pedals{display:flex;gap:8px;height:110px}
.ped{width:22px;background:#22252d;border-radius:6px;position:relative;overflow:hidden}
.ped i{position:absolute;left:0;right:0;bottom:0;border-radius:6px}.ped b{position:absolute;left:0;right:0;bottom:0;background:#ffffff40}
.delta{font-size:44px;font-weight:800;font-variant-numeric:tabular-nums;text-align:center}
.dbar{height:8px;background:#22252d;border-radius:4px;position:relative;margin-top:6px}.dbar i{position:absolute;top:0;bottom:0;border-radius:4px}
.kv{display:grid;grid-template-columns:repeat(auto-fit,minmax(92px,1fr));gap:8px}
.kv div{background:#1b1e26;border-radius:8px;padding:7px 9px}.kv b{display:block;font-size:17px;font-variant-numeric:tabular-nums;white-space:nowrap}.kv span{font-size:11px;color:var(--dim);text-transform:uppercase}
.sec{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.sec div{border-radius:8px;padding:8px;background:#1b1e26;text-align:center;font-variant-numeric:tabular-nums}
.sec div b{display:block;font-size:18px}.sec div span{font-size:11px;color:var(--dim)}
.s2{background:#4a1f73!important}.s3{background:#0f4d2a!important}.s4{background:#5a4a08!important}.s1{outline:1px solid #fff4}
canvas{width:100%;display:block;border-radius:8px;background:#0f1116}
.tyres{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.tyre{border-radius:10px;padding:10px;text-align:center;background:#1b1e26;border:2px solid transparent}
.tyre b{font-size:22px;display:block;font-variant-numeric:tabular-nums}.tyre span{font-size:12px;color:#000a}
.tyre.lock{border-color:var(--red)}.tyre.spin{border-color:var(--orange)}
.susp{height:5px;background:#0006;border-radius:3px;margin-top:5px;position:relative}.susp i{position:absolute;top:0;bottom:0;left:50%;background:#fff}
.lights{display:flex;flex-wrap:wrap;gap:6px}.lights span{padding:3px 8px;border-radius:6px;background:#22252d;color:#666;font-size:12px;font-weight:700}
.lights span.on{background:var(--yellow);color:#000}.lights span.red.on{background:var(--red);color:#fff}
.fuelbar{height:10px;background:#22252d;border-radius:5px;overflow:hidden;margin:6px 0}.fuelbar i{display:block;height:100%;background:var(--green)}
table{width:100%;border-collapse:collapse;font-variant-numeric:tabular-nums}
th,td{padding:7px 6px;text-align:right;border-bottom:1px solid var(--line);white-space:nowrap}th{color:var(--dim);font-weight:600;font-size:12px}
td:first-child,th:first-child{text-align:left}
.best{color:var(--purple);font-weight:700}.pos{color:var(--green)}.neg{color:#ff5a52}
.tw{overflow-x:auto}
.btn{background:var(--red);color:#fff;border:0;border-radius:9px;padding:10px 14px;font:inherit;font-weight:600;cursor:pointer;text-decoration:none;display:inline-block;text-align:center}
.btn.sec2{background:#2a2e3a}.btn:disabled{opacity:.5}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
.themes{display:grid;grid-template-columns:repeat(auto-fill,minmax(170px,1fr));gap:8px}
.th{background:#1b1e26;border:2px solid transparent;border-radius:10px;padding:9px 10px;cursor:pointer;text-align:left;color:inherit;font:inherit}
.th b{display:block;margin-bottom:3px}.th span{font-size:12px;color:var(--dim)}.th.sel{border-color:var(--red);background:#2a1416}
.th em{font-style:normal;font-size:10px;color:var(--yellow);margin-left:6px}
label.chk{display:flex;gap:8px;align-items:flex-start;padding:7px 0;border-bottom:1px solid var(--line);cursor:pointer}
label.chk input{margin-top:3px;accent-color:var(--red);width:16px;height:16px}label.chk span{font-size:12px;color:var(--dim);display:block}
.field{margin:10px 0}.field>span{display:flex;justify-content:space-between;font-size:12px;color:var(--dim);margin-bottom:4px}
input[type=range]{width:100%;accent-color:var(--red)}
input[type=number],input[type=text],select{width:100%;padding:9px;background:#1b1e26;color:var(--text);border:1px solid var(--line);border-radius:8px;font:inherit}
input[type=color]{width:100%;height:38px;border:0;background:none;padding:0}
.three{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
#preview{width:100%;height:auto;max-height:340px;display:block}
.note{font-size:12px;color:var(--dim);margin-top:8px;line-height:1.5}
.big{font-size:26px;font-weight:700;font-variant-numeric:tabular-nums}
.toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:#2a2e3a;padding:9px 16px;border-radius:9px;opacity:0;transition:opacity .3s;pointer-events:none}
.toast.show{opacity:1}
</style>
</head>
<body>
<header>
 <div class="bar">
  <div class="logo">GT7 <b>LED</b> &middot; Telemetria</div>
  <span class="pill" id="state">In attesa</span>
  <span class="sp"></span>
  <span class="small" id="rate"></span>
  <a class="dashbtn" href="/cruscotto">Cruscotto</a>
 </div>
 <nav>
  <button data-tab="tel" class="sel">Telemetria</button>
  <button data-tab="laps">Giri</button>
  <button data-tab="led">Striscia LED</button>
  <button data-tab="sys">Sistema</button>
 </nav>
</header>
<main>

<section class="tab sel" id="tab-tel">
 <div class="grid">
  <div class="card c8">
   <div class="hero">
    <div class="gear" id="gear">N<small id="sgear"></small></div>
    <div>
     <div class="speed"><b id="kmh">0</b><span>km/h</span></div>
     <div class="rpmtxt"><b id="rpm">0</b> giri/min <span id="rpmmax"></span></div>
     <div class="shift" id="shift"></div>
    </div>
    <div class="pedals" title="frizione, freno, gas (in bianco quello applicato da ABS e TCS)">
     <div class="ped"><i id="pclu" style="background:var(--blue)"></i></div>
     <div class="ped"><i id="pbrk" style="background:var(--red)"></i><b id="pbrko"></b></div>
     <div class="ped"><i id="pthr" style="background:var(--green)"></i><b id="pthro"></b></div>
    </div>
   </div>
   <div class="kv" style="margin-top:12px">
    <div><span>Giro</span><b id="lap">-</b></div>
    <div><span>Tempo</span><b id="cur">-</b></div>
    <div><span>Ultimo</span><b id="last">-</b></div>
    <div><span>Migliore</span><b id="best" class="best">-</b></div>
    <div><span>Ideale</span><b id="ideal">-</b></div>
    <div><span>Posizione</span><b id="pos">-</b></div>
   </div>
  </div>
  <div class="card c4">
   <h2>Delta live <span id="dref"></span></h2>
   <div class="delta" id="delta">--</div>
   <div class="dbar"><i id="dbar"></i></div>
   <div class="sec" style="margin-top:12px">
    <div id="s0"><span>S1</span><b>-</b></div><div id="s1"><span>S2</span><b>-</b></div><div id="s2"><span>S3</span><b>-</b></div>
   </div>
   <div class="kv" style="margin-top:10px">
    <div><span>Prossima frenata</span><b id="nb">-</b></div>
    <div><span>Ultima frenata</span><b id="lb">-</b></div>
   </div>
  </div>
  <div class="card c6">
   <h2>Circuito <span id="mapinfo"></span></h2>
   <canvas id="map" height="320"></canvas>
  </div>
  <div class="card c6">
   <h2>Forze G <span id="gpeak"></span></h2>
   <canvas id="gg" height="320"></canvas>
  </div>
  <div class="card c12">
   <h2>Tracce (ultimi 20 s) <span>velocit&agrave; &middot; <span style="color:var(--green)">gas</span> &middot; <span style="color:var(--red)">freno</span> &middot; <span style="color:var(--yellow)">giri</span></span></h2>
   <canvas id="trace" height="190"></canvas>
  </div>
  <div class="card c12">
   <h2>Confronto con il giro migliore <span>velocit&agrave; lungo il giro: <span style="color:#fff">giro in corso</span> &middot; <span style="color:var(--purple)">migliore</span> &middot; <span style="color:var(--yellow)">delta</span></span></h2>
   <canvas id="cmp" height="200"></canvas>
   <p class="note">Registrato dal browser mentre la pagina &egrave; aperta: servono un giro completo e uno in corso.</p>
  </div>
  <div class="card c4">
   <h2>Gomme e aderenza <span id="locks"></span></h2>
   <div class="tyres" id="tyres"></div>
   <div class="kv" style="margin-top:10px"><div><span>Uso del grip</span><b id="grip">-</b></div><div><span>G totale</span><b id="gtot">-</b></div></div>
  </div>
  <div class="card c4">
   <h2>Carburante</h2>
   <div class="big" id="fuel">-</div>
   <div class="fuelbar"><i id="fuelbar" style="width:0"></i></div>
   <div class="kv"><div><span>Giri rimasti</span><b id="flaps">-</b></div><div><span>Per giro</span><b id="fpl">-</b></div></div>
  </div>
  <div class="card c4">
   <h2>Motore e spie</h2>
   <div class="kv" style="margin-bottom:10px">
    <div><span>Turbo</span><b id="boost">-</b></div><div><span>Olio</span><b id="oil">-</b></div>
    <div><span>Acqua</span><b id="wt">-</b></div><div><span>T olio</span><b id="ot">-</b></div>
   </div>
   <div class="lights">
    <span id="lABS">ABS</span><span id="lTCS">TCS</span><span id="lASM">ASM</span><span id="lHB" class="red">FRENO A MANO</span><span id="lLIM" class="red">LIMITATORE</span><span id="lLGT">LUCI</span>
   </div>
  </div>
 </div>
</section>

<section class="tab" id="tab-laps">
 <div class="grid">
  <div class="card c12">
   <h2>Sessione <span id="trackkey"></span></h2>
   <div class="kv">
    <div><span>Giri</span><b id="sCount">-</b></div><div><span>Migliore</span><b id="sBest" class="best">-</b></div>
    <div><span>Ideale</span><b id="sIdeal">-</b></div><div><span>Media</span><b id="sAvg">-</b></div>
    <div><span>Velocit&agrave; max</span><b id="sTop">-</b></div><div><span>Picchi G</span><b id="sPeaks">-</b></div>
   </div>
  </div>
  <div class="card c12">
   <h2>Giri <span>il colore dei settori come in F1</span></h2>
   <div class="tw"><table><thead><tr><th>Giro</th><th>Tempo</th><th>Distacco</th><th>S1</th><th>S2</th><th>S3</th><th>Vel. max</th><th>Carb.</th></tr></thead><tbody id="laprows"></tbody></table></div>
   <div class="row" style="margin-top:12px">
    <button class="btn sec2" id="csvLaps">Esporta giri (CSV)</button>
    <button class="btn sec2" id="csvTel">Esporta telemetria (CSV)</button>
    <button class="btn" id="reset">Azzera sessione e circuito</button>
   </div>
   <p class="note">La striscia ricorda gli ultimi 10 giri; il browser registra tutti i giri e la telemetria (20 campioni al secondo) da quando la pagina &egrave; aperta.</p>
  </div>
  <div class="card c12">
   <h2>Accelerazioni <span>fermati 1 secondo, poi parti</span></h2>
   <div class="tw"><table><thead><tr><th></th><th>Reazione</th><th>0-100</th><th>0-200</th><th>100-200</th><th>400 m</th><th>V. 400 m</th><th>V. max</th></tr></thead><tbody id="runs"></tbody></table></div>
  </div>
 </div>
</section>

<section class="tab" id="tab-led">
 <div class="grid">
  <div class="card c12">
   <h2>Anteprima dal vivo <span id="ledinfo"></span></h2>
   <svg id="preview" viewBox="0 0 400 240"></svg>
  </div>
  <div class="card c8">
   <h2>Tema in gara</h2>
   <div class="themes" id="themes"></div>
  </div>
  <div class="card c4">
   <h2>Avvisi <span>sopra ogni tema</span></h2>
   <div id="alerts"></div>
  </div>
  <div class="card c6">
   <h2>Gioco fermo</h2>
   <div class="field"><span>Animazione</span><select id="idle"></select></div>
   <div class="field"><span>Colore</span><input type="color" id="icolor"></div>
   <div class="field"><span>Spegni dopo minuti senza GT7 <b id="itv"></b></span><input type="range" id="itime" min="0" max="240" step="5"></div>
  </div>
  <div class="card c6">
   <h2>Luce</h2>
   <div class="field"><span>Luminosit&agrave; <b id="bv"></b></span><input type="range" id="bright" min="0" max="255"></div>
   <div class="field"><span>Gorgoglio alla punta <b id="gv"></b></span><input type="range" id="gurgle" min="0" max="255"></div>
   <div class="field"><span>Scala del delta (lati pieni) <b id="dsv"></b></span><input type="range" id="dscale" min="1" max="50"></div>
   <div class="field"><span>Limite di corrente (mA)</span><input type="number" id="maxma" min="100" max="10000" step="100"></div>
  </div>
  <div class="card c12">
   <h2>Montaggio dietro la TV <span id="total"></span></h2>
   <div class="three">
    <div class="field"><span>Lato destro</span><input type="number" id="zr" min="0" max="200"></div>
    <div class="field"><span>Lato alto</span><input type="number" id="zt" min="0" max="200"></div>
    <div class="field"><span>Lato sinistro</span><input type="number" id="zl" min="0" max="200"></div>
   </div>
   <label class="chk"><input type="checkbox" id="sleft"><div>La striscia parte in basso a sinistra<span>Senza spunta: parte in basso a destra, sale, va a sinistra lungo il lato alto e scende. Nessun LED sul lato basso.</span></div></label>
   <div class="row" style="margin-top:12px">
    <button class="btn sec2" id="test">Prova montaggio (15 s)</button>
    <button class="btn" id="save">Salva come impostazioni di avvio</button>
   </div>
   <p class="note">Nella prova il lato destro &egrave; rosso, quello alto verde, il sinistro blu e il primo LED lampeggia bianco: se un colore sconfina sull'altro lato, correggi i numeri. Con 0 LED su entrambi i lati la striscia &egrave; una barra dritta. Ogni modifica vale subito; <b>Salva</b> la mantiene dopo il riavvio.</p>
  </div>
 </div>
</section>

<section class="tab" id="tab-sys">
 <div class="grid">
  <div class="card c6">
   <h2>Dispositivo</h2>
   <div class="kv" id="sysinfo"></div>
  </div>
  <div class="card c6">
   <h2>Indirizzo fisso</h2>
   <p class="note" id="netinfo" style="margin-top:0"></p>
   <div class="row" style="margin-top:10px">
    <input type="text" id="fixip" inputmode="decimal" autocomplete="off" spellcheck="false" style="flex:1 1 150px;width:auto" aria-label="Indirizzo fisso">
    <button class="btn" id="fixset">Usa sempre questo</button>
    <button class="btn sec2" id="fixauto">Torna automatico</button>
   </div>
   <p class="note">Con l'indirizzo fisso il cruscotto aggiunto alla schermata Home si apre sempre, anche dove <b>gt7led.local</b> non funziona (l'hotspot dell'iPhone). Vale solo per questa rete (stesso nome e stesso router, fino a 4 reti ricordate): su un'altra, o su un hotspot che cambia indirizzi a ogni accensione, la striscia usa quello dato dal router. Sull'hotspot dell'iPhone la striscia usa già da sola 172.20.10.14, l'ultimo che l'iPhone assegna (<i>Torna automatico</i> lo spegne). Su un router di casa scegline uno fuori dagli indirizzi che il router distribuisce, oppure riservalo nel router.</p>
  </div>
  <div class="card c6">
   <h2>Manutenzione</h2>
   <div class="row">
    <a class="btn sec2" href="/update">Aggiorna firmware via Wi-Fi</a>
    <button class="btn" id="restart">Riavvia</button>
   </div>
   <p class="note">Interfaccia per programmi esterni: <code>/api/status</code>, <code>/api/session</code>, flusso <code>/api/events</code> (eventi <code>t</code> telemetria a 20 Hz e <code>leds</code> colori della striscia).</p>
  </div>
 </div>
</section>
</main>
<div class="toast" id="toast"></div>

<script>
'use strict';
const $=id=>document.getElementById(id);
const THEMES=[
 ['Default','Si riempie lungo la striscia: verde, giallo, rosso'],
 ['F1 Center','Dal centro della striscia verso i due capi'],
 ['Supercar','Tutta blu, solo sopra il 70% dei giri'],
 ['Smooth Fade','Tutta la striscia da azzurro a rosso'],
 ['F1 Reverse','Dai due capi in basso fino al centro in alto'],
 ['Default Rev','Come Default, dall\'altro capo'],
 ['Delta','Lati verdi se sei in anticipo sul giro migliore, rossi se in ritardo; giri in alto',1],
 ['Pedali','Gas sul lato destro, freno sul sinistro, giri in alto',1],
 ['Velocità','Scie di luce che escono dal centro, più veloci con la velocità',1],
 ['Forze G','La luce va dove la forza spinge il corpo: a sinistra in curva a destra, in alto in frenata',1],
 ['Gomme','Ogni angolo con il colore della temperatura della sua gomma',1],
 ['Ambiente','Il colore del cielo all\'ora del giorno della gara',1],
 ['Solo avvisi','Luce fissa tenue del colore di riposo: si muovono solo gli avvisi',1]];
const IDLES=['Spento','Colore fisso','Respiro','Arcobaleno','Onda','Fuoco','Aurora','Luce TV (bianco)'];
const ALERTS=[
 ['Limitatore','Tutta la striscia lampeggia rossa al limitatore'],
 ['Giro migliore','Tre impulsi viola a ogni nuovo giro migliore'],
 ['Settori','Il lato alto prende il colore del settore appena chiuso'],
 ['Punti di frenata','Da 150 m i lati si accendono dall\'alto: bianco, arancione, rosso'],
 ['Riserva','Impulsi arancioni quando il carburante sta finendo'],
 ['Bloccaggi e pattinamenti','Rosso o arancione sul lato della ruota'],
 ['Ultimo giro e arrivo','Lampi bianchi all\'ultimo giro, bandiera a scacchi all\'arrivo'],
 ['Marcia consigliata','Angoli azzurri se il gioco consiglia una marcia più bassa']];
let S={},F=null,sess=null,rev='',ledColors='',lastFrameAt=0;
const fmt=ms=>{if(ms==null||ms<=0)return'-';const m=Math.floor(ms/60000),s=(ms%60000)/1000;return m+':'+(s<10?'0':'')+s.toFixed(3)};
const fmtS=ms=>ms==null||ms<=0?'-':(ms/1000).toFixed(3);
const fmtD=ms=>ms==null?'--':(ms<0?'-':'+')+(Math.abs(ms)/1000).toFixed(3);
const num=(v,d=0,u='')=>v==null||!isFinite(v)?'-':v.toFixed(d)+u;
function toast(t){const e=$('toast');e.textContent=t;e.classList.add('show');clearTimeout(toast.t);toast.t=setTimeout(()=>e.classList.remove('show'),1800)}
function api(p){return fetch(p).then(r=>{if(!r.ok)throw r;return r})}
// ---------- tabs
document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{
 document.querySelectorAll('nav button').forEach(x=>x.classList.toggle('sel',x==b));
 document.querySelectorAll('.tab').forEach(x=>x.classList.toggle('sel',x.id=='tab-'+b.dataset.tab));
 try{localStorage.setItem('tab',b.dataset.tab)}catch(e){}
 resize()});
try{const t=localStorage.getItem('tab');if(t){const b=document.querySelector('nav button[data-tab="'+t+'"]');if(b)b.click()}}catch(e){}
// ---------- canvases
function prep(c){const r=window.devicePixelRatio||1,w=c.clientWidth,h=+c.getAttribute('height');if(c.width!=w*r){c.width=w*r;c.height=h*r;c.style.height=h+'px'}const x=c.getContext('2d');x.setTransform(r,0,0,r,0,0);x.clearRect(0,0,w,h);return[x,w,h]}
function resize(){drawMap();drawGG();drawTrace();drawCmp()}
window.addEventListener('resize',resize);
// ---------- shift lights
const SHIFT=16;for(let i=0;i<SHIFT;i++)$('shift').appendChild(document.createElement('i'));
function shiftLights(f){const pct=f.mx>0?f.rpm/f.mx*100:0,ok=f.al>0&&f.al<f.mx,start=ok?Math.min(90,Math.max(30,f.al/f.mx*100-25)):65;
 const lit=pct>start?Math.round((pct-start)/(100-start)*SHIFT):0,lim=(f.f&32)&&(Date.now()/70|0)%2==0;
 [...$('shift').children].forEach((e,i)=>{e.style.background=lim?'var(--blue)':i<lit?(i<SHIFT*.4?'var(--green)':i<SHIFT*.7?'var(--yellow)':'var(--red)'):''})}
// ---------- telemetry
const ST=['','run','s2','s3','s4'];
function setText(id,t){const e=$(id);if(e.textContent!==t)e.textContent=t}
function onFrame(f){
 F=f;lastFrameAt=Date.now();record(f);
 const st=$('state');
 st.textContent=!f.live?'In attesa di GT7':f.race?'In pista':(f.f&2)?'Pausa':(f.f&4)?'Caricamento':'Menu';
 st.className='pill'+(f.race?' on':f.live?' pause':'');
 setText('rate',f.live?f.hz.toFixed(0)+' pacchetti/s'+(f.car?' · auto '+f.car:''):'');
 const g=f.g==15?'N':f.g==0?'R':''+f.g;$('gear').firstChild.nodeValue=g;
 setText('sgear',f.sg>0&&f.sg<15&&f.sg!=f.g?'consigliata '+f.sg:'');
 setText('kmh',num(f.kmh));setText('rpm',num(f.rpm));setText('rpmmax',f.mx?'/ '+f.mx:'');
 shiftLights(f);
 $('pthr').style.height=f.thr+'%';$('pbrk').style.height=f.brk+'%';$('pclu').style.height=(f.clu||0)+'%';
 $('pthro').style.height=f.thro+'%';$('pbrko').style.height=f.brko+'%';
 setText('lap',f.lap>0?(f.tl>0?f.lap+'/'+f.tl:''+f.lap):'-');
 setText('cur',fmt(f.cur));setText('last',fmt(f.last));setText('best',fmt(f.best));
 setText('pos',f.pos>0?(f.cars>0?f.pos+'/'+f.cars:''+f.pos):'-');
 setText('ideal',sess&&sess.ideal>0?fmt(sess.ideal):'-');
 const d=$('delta');d.textContent=fmtD(f.d);d.style.color=f.d==null?'var(--dim)':f.d<=0?'var(--green)':'#ff5a52';
 const k=f.d==null?0:Math.min(1,Math.abs(f.d)/((S.deltaScale||10)*100));const db=$('dbar');
 db.style.background=f.d<=0?'var(--green)':'var(--red)';db.style.width=k*50+'%';db.style.left=f.d<=0?(50-k*50)+'%':'50%';
 setText('dref',f.ref>0?'su '+fmt(f.ref):'serve un giro completo');
 for(let i=0;i<3;i++){const e=$('s'+i),s=f.ss[i],ms=f.sm[i];e.className=ST[s]||'';e.lastChild.textContent=ms>0?fmtS(ms):s==1?'in corso':f.ls[i]>1?'prec.':'-'}
 setText('nb',f.nb!=null?f.nb+' m · '+f.nbk+' km/h':'-');
 if(f.lb){const e=$('lb');e.textContent=(f.lb[0]==null?'':(f.lb[0]>=0?'+':'')+f.lb[0].toFixed(0)+' m · ')+f.lb[1]+' km/h';e.className=f.lb[0]==null?'':Math.abs(f.lb[0])<=5?'pos':Math.abs(f.lb[0])<=15?'':'neg'}
 // tyres
 const names=['AS','AD','PS','PD'];let h='';
 for(let w=0;w<4;w++){const t=f.tt[w],s=f.sl[w],c=tyreColor(t),cl=f.ws[w]==2?' lock':f.ws[w]==3?' spin':'',l=Math.max(-1,Math.min(1,f.sus[w]||0));
  h+='<div class="tyre'+cl+'" style="background:'+c+'"><span>'+names[w]+'</span><b>'+num(t,0,'°')+'</b><span>'+(s==null?'slitt. -':'slitt. '+s.toFixed(1)+'%')+'</span><div class="susp"><i style="'+(l>=0?'left:50%;width:'+l*50+'%':'left:'+(50+l*50)+'%;width:'+(-l*50)+'%')+'"></i></div></div>'}
 $('tyres').innerHTML=h;
 setText('locks','bloccaggi '+f.lk[0]+' ('+f.lk[2]+') · pattinamenti '+f.lk[1]+' ('+f.lk[3]+')');
 setText('grip',num(f.gu*100,0,'%'));setText('gtot',num(Math.hypot(f.glat,f.glon),2,' g'));
 const ev=f.cap>=0&&f.cap<0.1;
 setText('fuel',ev?num(f.fuel,0,' kWh'):f.cap>0?num(f.fuel,1,' L')+' · '+num(f.fuel/f.cap*100,0,'%'):'-');
 $('fuelbar').style.width=(f.cap>0?Math.min(100,f.fuel/f.cap*100):0)+'%';$('fuelbar').style.background=f.fl!=null&&f.fl<1.5?'var(--orange)':'';
 setText('flaps',num(f.fl,1));setText('fpl',num(f.fpl,2,' L'));
 setText('boost',num(f.boost,2,' bar'));setText('oil',num(f.oil,1,' bar'));setText('wt',num(f.wt,0,'°'));setText('ot',num(f.ot,0,'°'));
 const L=(id,on)=>$(id).classList.toggle('on',!!on);
 L('lABS',f.abs);L('lTCS',f.f&2048);L('lASM',f.f&1024);L('lHB',f.f&64);L('lLIM',f.f&32);L('lLGT',f.f&128);
 const r=f.rev.join(',');if(r!==rev){rev=r;loadSession()}
}
function tyreColor(t){if(t==null)return'#30333b';const s=[[50,[30,60,255]],[62,[40,110,230]],[72,[40,180,210]],[82,[0,190,80]],[95,[0,190,80]],[102,[255,170,0]],[110,[226,30,36]]];
 if(t<=s[0][0])return'rgb('+s[0][1]+')';for(let i=1;i<s.length;i++)if(t<s[i][0]){const k=(t-s[i-1][0])/(s[i][0]-s[i-1][0]),a=s[i-1][1],b=s[i][1];return'rgb('+a.map((v,j)=>Math.round(v+(b[j]-v)*k))+')'}return'rgb(226,30,36)'}
// ---------- browser-side recording: traces, laps, CSV
const TR=[],LAPS={},REC=[];let curLap=-1,lapStartFuel=null;
function record(f){
 const now=Date.now();
 if(f.race){TR.push([now,f.kmh,f.thr,f.brk,f.mx>0?f.rpm/f.mx:0]);
  REC.push([new Date(now).toISOString(),f.lap,f.cur,f.prog,f.kmh,f.rpm,f.g,f.thr,f.brk,f.glat,f.glon,f.x,f.z,f.d,f.fuel,f.tt.join('|')].join(','));if(REC.length>200000)REC.splice(0,20000)}
 while(TR.length&&TR[0][0]<now-20000)TR.shift();
 if(f.lap!==curLap){if(LAPS[curLap]&&f.last>0)LAPS[curLap].time=f.last;if(LAPS[curLap]&&lapStartFuel!=null)LAPS[curLap].fuel=lapStartFuel-f.fuel;curLap=f.lap;lapStartFuel=f.fuel}
 if(f.race&&f.prog!=null){const L=LAPS[f.lap]||(LAPS[f.lap]={pts:[],time:0,fuel:null});const p=L.pts;if(!p.length||f.prog>p[p.length-1][0])p.push([f.prog,f.kmh,f.d])}
}
function bestRecorded(){let b=null;for(const k in LAPS){const L=LAPS[k];if(+k===curLap||!L.time||L.pts.length<50||L.pts[0][0]>0.05||L.pts[L.pts.length-1][0]<0.95)continue;if(!b||L.time<b.time)b=L}return b}
function dl(name,text){const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([text],{type:'text/csv'}));a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(a.href),2000)}
$('csvLaps').onclick=()=>{let t='giro;tempo;s1;s2;s3;vel_max;carburante\n';const seen={};
 (sess?sess.laps:[]).slice().reverse().forEach(l=>{seen[l.n]=1;t+=[l.n,fmt(l.t),fmtS(l.s[0]),fmtS(l.s[1]),fmtS(l.s[2]),num(l.v,1),''].join(';')+'\n'});
 dl('giri-gt7.csv',t)};
$('csvTel').onclick=()=>{if(!REC.length)return toast('Nessun dato registrato');dl('telemetria-gt7.csv','ora,giro,tempo_giro_ms,progresso,kmh,rpm,marcia,gas,freno,g_lat,g_long,x,z,delta_ms,carburante,gomme\n'+REC.join('\n'))};
$('reset').onclick=()=>{if(confirm('Azzerare tempi, mappa e i dati salvati di questo circuito?'))api('/api/reset').then(()=>{toast('Sessione azzerata');for(const k in LAPS)delete LAPS[k]})};
// ---------- session
function loadSession(){api('/api/session').then(r=>r.json()).then(s=>{sess=s;drawSession();drawMap()}).catch(()=>{})}
function drawSession(){const s=sess;if(!s)return;
 setText('sCount',''+s.completed);setText('ideal',s.ideal>0?fmt(s.ideal):'-');setText('sBest',fmt(s.best));setText('sIdeal',fmt(s.ideal));setText('sAvg',fmt(s.avg));setText('sTop',num(s.top,0,' km/h'));
 setText('sPeaks',s.peaks.map(v=>num(v,2)).join(' / '));setText('trackkey',s.map.key?'circuito '+(s.map.key>>>0).toString(16):'');
 let h='';s.laps.forEach(l=>{const gap=s.best>0&&l.t>0?l.t-s.best:null,L=LAPS[l.n];
  h+='<tr><td>'+l.n+'</td><td class="'+(l.t==s.best?'best':'')+'">'+fmt(l.t)+'</td><td class="'+(gap>0?'neg':'')+'">'+(gap==null?'-':gap==0?'':'+'+(gap/1000).toFixed(3))+'</td>'+
  l.s.map((v,i)=>'<td class="'+(v>0&&v==s.bs[i]?'best':'')+'">'+fmtS(v)+'</td>').join('')+'<td>'+num(l.v,0)+'</td><td>'+(L&&L.fuel!=null?L.fuel.toFixed(2)+' L':'-')+'</td></tr>'});
 $('laprows').innerHTML=h||'<tr><td colspan="8" style="text-align:center;color:var(--dim)">Nessun giro completo</td></tr>';
 const R=(n,r)=>'<tr><td>'+n+'</td>'+[num(r[0],3,' s'),num(r[1],2,' s'),num(r[2],2,' s'),num(r[3],2,' s'),num(r[4],2,' s'),num(r[5],0,' km/h'),num(r[6],0,' km/h')].map(v=>'<td>'+v+'</td>').join('')+'</tr>';
 $('runs').innerHTML=R('Ultima',s.runs.last)+R('<span class="best">Migliore</span>',s.runs.best)}
// ---------- drawings
function drawMap(){const c=$('map');if(!c.clientWidth)return;const[x,w,h]=prep(c);const s=sess;
 if(!s||!s.map.p.length){x.fillStyle='#8b8f9c';x.font='13px system-ui';x.textAlign='center';x.fillText(s&&s.map.state==0?'La mappa si disegna dal primo giro lanciato':'In attesa di dati',w/2,h/2);setText('mapinfo','');return}
 let[a,b,c2,d]=s.map.b||[0,0,1,1];const sc=Math.min((w-30)/Math.max(1,c2-a),(h-30)/Math.max(1,d-b)),ox=(w-(c2-a)*sc)/2,oy=(h-(d-b)*sc)/2;
 const P=p=>[ox+(p[0]-a)*sc,oy+(p[1]-b)*sc],pts=s.map.p,n=pts.length,complete=s.map.state==2;
 x.lineWidth=7;x.lineCap='round';x.lineJoin='round';
 const secCol=i=>{if(!F)return'#3a3e4a';const st=F.ss[i]>1?F.ss[i]:F.ls[i];return F.sec==i?'#ffffff':st==2?'#b04dff':st==3?'#16c45a':st==4?'#ffc400':'#3a3e4a'};
 for(let i=0;i<n-1+(complete?1:0);i++){x.strokeStyle=complete?secCol(Math.floor(i*3/n)):'#5a5e6a';x.beginPath();x.moveTo(...P(pts[i]));x.lineTo(...P(pts[(i+1)%n]));x.stroke()}
 if(complete){const[p,q]=[P(pts[0]),P(pts[2])];x.strokeStyle='#fff';x.lineWidth=3;const dx=q[0]-p[0],dy=q[1]-p[1],l=Math.hypot(dx,dy)||1;x.beginPath();x.moveTo(p[0]-dy/l*10,p[1]+dx/l*10);x.lineTo(p[0]+dy/l*10,p[1]-dx/l*10);x.stroke()}
 s.brakes.forEach((z,i)=>{const p=P(z);const r=z[3];x.fillStyle=r==null?'#ff7a00':Math.abs(r)<=5?'#16c45a':Math.abs(r)<=15?'#ffc400':'#ff3b30';x.fillRect(p[0]-4,p[1]-4,8,8);
  if(F&&F.nbi==i){x.strokeStyle='#fff';x.lineWidth=2;x.strokeRect(p[0]-7,p[1]-7,14,14)}});
 if(F&&F.x!=null){const p=P([F.x,F.z]);x.fillStyle='#e10600';x.strokeStyle='#fff';x.lineWidth=2;x.beginPath();x.arc(p[0],p[1],7,0,7);x.fill();x.stroke()}
 setText('mapinfo',complete?(s.brakes.length+' punti di frenata'):'in costruzione')}
const GT=[];
function drawGG(){const c=$('gg');if(!c.clientWidth)return;const[x,w,h]=prep(c),r=Math.min(w,h)/2-16,cx=w/2,cy=h/2,max=Math.max(1.5,...GT.map(p=>Math.hypot(p[0],p[1])));
 x.strokeStyle='#2a2e3a';x.lineWidth=1;for(let k=1;k<=3;k++){x.beginPath();x.arc(cx,cy,r*k/3,0,7);x.stroke()}
 x.beginPath();x.moveTo(cx-r,cy);x.lineTo(cx+r,cy);x.moveTo(cx,cy-r);x.lineTo(cx,cy+r);x.stroke();
 x.fillStyle='#8b8f9c';x.font='11px system-ui';x.fillText(max.toFixed(1)+' g',cx+r*0.72,cy-r*0.72);x.fillText('FRENATA',cx-24,cy-r-4);x.fillText('ACCELERAZIONE',cx-38,cy+r+12);
 // the point moves like the load: up braking, left in a right turn
 GT.forEach((p,i)=>{x.fillStyle='rgba(225,6,0,'+(i/GT.length*0.8)+')';x.beginPath();x.arc(cx-p[0]/max*r,cy+p[1]/max*r,3,0,7);x.fill()});
 const p=GT[GT.length-1];if(p){x.fillStyle='#fff';x.beginPath();x.arc(cx-p[0]/max*r,cy+p[1]/max*r,7,0,7);x.fill();setText('gpeak','lat '+Math.abs(p[0]).toFixed(2)+' · long '+Math.abs(p[1]).toFixed(2)+' g')}}
function drawTrace(){const c=$('trace');if(!c.clientWidth)return;const[x,w,h]=prep(c),now=Date.now();
 x.strokeStyle='#22252d';for(let k=1;k<4;k++){x.beginPath();x.moveTo(0,h*k/4);x.lineTo(w,h*k/4);x.stroke()}
 const line=(i,col,sc)=>{x.strokeStyle=col;x.lineWidth=1.6;x.beginPath();TR.forEach((p,j)=>{const X=w-(now-p[0])/20000*w,Y=h-3-p[i]*sc*(h-6);j?x.lineTo(X,Y):x.moveTo(X,Y)});x.stroke()};
 const vmax=Math.max(100,...TR.map(p=>p[1]));line(4,'#ffc40088',1);line(2,'#16c45a',0.01);line(3,'#e10600',0.01);line(1,'#ffffff',1/vmax);
 x.fillStyle='#8b8f9c';x.font='11px system-ui';x.fillText(vmax.toFixed(0)+' km/h',4,12)}
function drawCmp(){const c=$('cmp');if(!c.clientWidth)return;const[x,w,h]=prep(c);const b=bestRecorded(),cur=LAPS[curLap];
 if(!b||!cur){x.fillStyle='#8b8f9c';x.font='13px system-ui';x.textAlign='center';x.fillText('In attesa di un giro completo registrato',w/2,h/2);return}
 const vmax=Math.max(...b.pts.map(p=>p[1]),...cur.pts.map(p=>p[1]),100);
 const line=(pts,col,i,sc,off)=>{x.strokeStyle=col;x.lineWidth=1.6;x.beginPath();let st=0;pts.forEach(p=>{if(p[i]==null)return;const X=p[0]*w,Y=off-p[i]*sc;st++?x.lineTo(X,Y):x.moveTo(X,Y)});x.stroke()};
 x.strokeStyle='#22252d';x.beginPath();x.moveTo(0,h*0.75);x.lineTo(w,h*0.75);x.stroke();
 line(b.pts,'#b04dff',1,(h*0.62)/vmax,h*0.66);line(cur.pts,'#ffffff',1,(h*0.62)/vmax,h*0.66);
 const dm=Math.max(500,...cur.pts.map(p=>Math.abs(p[2]||0)));line(cur.pts,'#ffc400',2,-(h*0.2)/dm,h*0.75+h*0.0);
 x.fillStyle='#8b8f9c';x.font='11px system-ui';x.fillText('migliore '+fmt(b.time)+' · delta ±'+(dm/1000).toFixed(1)+' s',4,12)}
// ---------- live stream
let es=null;
function connect(){if(es)es.close();es=new EventSource('/api/events');
 es.addEventListener('t',e=>{try{const f=JSON.parse(e.data);onFrame(f);GT.push([f.glat,f.glon]);while(GT.length>40)GT.shift()}catch(err){}});
 es.addEventListener('leds',e=>{ledColors=e.data;drawPreview()});
 es.onerror=()=>{$('state').textContent='Connessione persa';$('state').className='pill'}}
connect();
setInterval(()=>{if(Date.now()-lastFrameAt>4000&&es&&es.readyState==2)connect()},3000);
let drawTick=0;(function loop(){drawTick++;if(F){if(document.querySelector('#tab-tel.sel')){drawGG();drawTrace();if(drawTick%3==0){drawMap();drawCmp()}}}requestAnimationFrame(loop)})();
// ---------- LED settings
function buildLed(){
 $('themes').innerHTML=THEMES.map((t,i)=>'<button class="th" data-i="'+i+'"><b>'+t[0]+(t[2]?'<em>NUOVO</em>':'')+'</b><span>'+t[1]+'</span></button>').join('');
 $('themes').querySelectorAll('.th').forEach(b=>b.onclick=()=>{S.theme=+b.dataset.i;markTheme();api('/api/theme?v='+S.theme).then(()=>toast('Tema: '+THEMES[S.theme][0]))});
 $('alerts').innerHTML=ALERTS.map((a,i)=>'<label class="chk"><input type="checkbox" data-bit="'+i+'"><div>'+a[0]+'<span>'+a[1]+'</span></div></label>').join('');
 $('alerts').querySelectorAll('input').forEach(c=>c.onchange=()=>{S.alerts=c.checked?S.alerts|(1<<c.dataset.bit):S.alerts&~(1<<c.dataset.bit);send(false)});
 $('idle').innerHTML=IDLES.map((n,i)=>'<option value="'+i+'">'+n+'</option>').join('')}
function markTheme(){$('themes').querySelectorAll('.th').forEach(b=>b.classList.toggle('sel',+b.dataset.i==S.theme))}
function showSettings(){markTheme();
 $('alerts').querySelectorAll('input').forEach(c=>c.checked=!!(S.alerts&(1<<c.dataset.bit)));
 $('idle').value=S.idleMode;$('icolor').value='#'+(S.idleColor>>>0).toString(16).padStart(6,'0');
 $('bright').value=S.brightness;$('gurgle').value=S.gurgle;$('dscale').value=S.deltaScale;$('maxma').value=S.maxMa;$('itime').value=S.idleOff;
 $('zr').value=S.right;$('zt').value=S.top;$('zl').value=S.left;$('sleft').checked=!!S.startLeft;labels();drawPreview()}
function labels(){setText('bv',Math.round($('bright').value/2.55)+'%');setText('gv',$('gurgle').value==0?'spento':$('gurgle').value);
 setText('dsv',($('dscale').value/10).toFixed(1)+' s');setText('itv',$('itime').value==0?'mai':$('itime').value+' min');
 setText('total',(+$('zr').value + +$('zt').value + +$('zl').value)+' LED in tutto')}
function read(){S.idleMode=+$('idle').value;S.idleColor=parseInt($('icolor').value.slice(1),16);S.brightness=+$('bright').value;S.gurgle=+$('gurgle').value;
 S.deltaScale=+$('dscale').value;S.maxMa=+$('maxma').value;S.idleOff=+$('itime').value;S.right=+$('zr').value;S.top=+$('zt').value;S.left=+$('zl').value;S.startLeft=$('sleft').checked?1:0}
function send(save){read();labels();const q='b='+S.brightness+'&l='+(S.right+S.top+S.left)+'&g='+S.gurgle+'&ma='+S.maxMa+'&im='+S.idleMode+'&ic='+(S.idleColor>>>0).toString(16).padStart(6,'0')+
 '&al='+S.alerts+'&zr='+S.right+'&zt='+S.top+'&zl='+S.left+'&sl='+S.startLeft+'&ds='+S.deltaScale+'&it='+S.idleOff+'&save='+(save?1:0);
 return api('/api/settings?'+q).then(()=>{if(save)toast('Salvato: la striscia ripartirà così')}).catch(()=>toast('Valori non accettati'))}
['idle','icolor','bright','gurgle','dscale','maxma','itime','zr','zt','zl','sleft'].forEach(id=>{$(id).onchange=()=>{send(false);drawPreview()};$(id).oninput=labels});
$('save').onclick=()=>send(true);
$('test').onclick=()=>send(false).then(()=>api('/api/test?layout=1')).then(()=>toast('Prova montaggio: 15 secondi'));
$('restart').onclick=()=>{if(confirm('Riavviare la striscia?'))api('/restart').then(()=>toast('Riavvio...'))};
// The strip drawn around a TV, in the colours it shows now.
function drawPreview(){const sv=$('preview');if(!S.right&&S.right!==0)return;
 const R=+S.right,T=+S.top,Lf=+S.left,n=R+T+Lf,left=!!S.startLeft;if(!n)return;
 const X0=60,X1=340,Y0=30,Y1=200;let h='<rect x="'+(X0+6)+'" y="'+(Y0+6)+'" width="'+(X1-X0-12)+'" height="'+(Y1-Y0-12)+'" rx="6" fill="#050608" stroke="#2a2e3a"/><rect x="185" y="'+Y1+'" width="30" height="16" fill="#1b1e26"/><rect x="150" y="'+(Y1+16)+'" width="100" height="6" rx="3" fill="#1b1e26"/>';
 const first=left?Lf:R,last=left?R:Lf,top=T,pos=[];
 for(let i=0;i<n;i++){let x,y;if(i<first){const k=(i+.5)/first;x=left?X0:X1;y=Y1-k*(Y1-Y0)}else if(i<first+top){const k=(i-first+.5)/top;x=left?X0+k*(X1-X0):X1-k*(X1-X0);y=Y0}else{const k=(i-first-top+.5)/last;x=left?X1:X0;y=Y0+k*(Y1-Y0)}pos.push([x,y])}
 const r=Math.max(1.6,Math.min(4,560/n));
 pos.forEach((p,i)=>{const c=ledColors.length>=6*(i+1)?'#'+ledColors.substr(6*i,6):'#2a2e3a';h+='<circle cx="'+p[0].toFixed(1)+'" cy="'+p[1].toFixed(1)+'" r="'+r+'" fill="'+c+'"/>'});
 if(pos.length)h+='<text x="'+pos[0][0]+'" y="'+(pos[0][1]+16)+'" fill="#8b8f9c" font-size="10" text-anchor="middle">inizio</text>';
 sv.innerHTML=h;setText('ledinfo',THEMES[S.theme]?'tema '+THEMES[S.theme][0]+' · '+n+' LED':'')}
function loadStatus(){api('/api/status').then(r=>r.json()).then(s=>{S=s;showSettings();showNet(s);
 const up=s.uptime|0;$('sysinfo').innerHTML=[['Firmware',s.fw],['Indirizzo',s.ip],['Nome in rete',s.host?'<a href="http://'+s.host+'" style="color:inherit">'+s.host+'</a>':'-'],['Segnale Wi-Fi',s.rssi+' dBm'],['Acceso da',Math.floor(up/3600)+' h '+Math.floor(up%3600/60)+' min'],['Memoria libera',Math.round(s.heap/1024)+' KB'],['PSRAM libera',Math.round((s.psram||0)/1024)+' KB'],['Circuiti salvati',s.tracks<0?'memoria non disponibile':s.tracks],['Pagine collegate',s.clients]].map(v=>'<div><span>'+v[0]+'</span><b>'+v[1]+'</b></div>').join('')}).catch(()=>{})}
// ---------- fixed address
function showNet(s){
 setText('netinfo',s.fixed?'La striscia usa sempre '+s.ip+' sulla rete '+s.ssid+'.':
  'Indirizzo dato dal router: '+s.ip+' sulla rete '+s.ssid+'. Può cambiare a ogni accensione.'+
  (s.fixedNets?' Indirizzi fissi salvati per altre reti: '+s.fixedNets+'.':''));
 const f=$('fixip');if(!f.dataset.edited&&document.activeElement!==f)f.value=s.fixed?s.ip:s.suggest}
$('fixip').oninput=()=>{$('fixip').dataset.edited=1};
function moveTo(q,url,msg){api('/api/network?ip='+q).then(()=>{toast(msg);setTimeout(()=>{if(url)location.href=url},7000)})
 .catch(r=>r&&r.text?r.text().then(toast):toast('Striscia non raggiungibile'))}
$('fixset').onclick=()=>{const ip=$('fixip').value.trim();if(!/^\d{1,3}(\.\d{1,3}){3}$/.test(ip))return toast('Scrivi un indirizzo come 172.20.10.14');
 moveTo(ip,'http://'+ip+'/','Riavvio su '+ip+': la pagina si riapre lì')};
$('fixauto').onclick=()=>moveTo('auto',null,'Riavvio con l\'indirizzo del router: lo vedi sullo schermo, nel menu LED');
buildLed();loadStatus();setInterval(()=>{if(document.querySelector('#tab-sys.sel'))loadStatus()},5000);
</script>
</body>
</html>
)rawliteral";
