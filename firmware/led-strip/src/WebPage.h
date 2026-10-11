#pragma once
#include <pgmspace.h>

// The web page of the strip: light settings only (theme, alerts, idle
// animation, brightness, layout) and system. Static: it reads the settings
// from /api/status. No external files, so it works without internet.
// tools/screenshots/led-web.mjs takes it from here for the README.
static const char WEB_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0b0c10">
<title>GT7 LED</title>
<style>
:root{--bg:#0b0c10;--card:#14161c;--line:#252832;--text:#e8e9ee;--dim:#8b8f9c;--red:#e10600}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font:14px/1.4 system-ui,-apple-system,'Segoe UI',Roboto,sans-serif}
header{padding:12px 16px;border-bottom:1px solid var(--line);display:flex;gap:10px;align-items:center}
.logo{font-weight:800}.logo b{color:var(--red)}.sp{flex:1}
.pill{white-space:nowrap;padding:3px 10px;border-radius:99px;font-size:12px;font-weight:600;background:#23262f;color:var(--dim)}.pill.on{background:#0f3d22;color:#5ff59a}
main{max-width:1000px;margin:0 auto;padding:14px 16px 40px;display:grid;gap:12px;grid-template-columns:repeat(2,1fr)}
@media(max-width:760px){main{grid-template-columns:1fr}}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:12px 14px;min-width:0}
.wide{grid-column:1/-1}
.card h2{font-size:12px;text-transform:uppercase;letter-spacing:1px;color:var(--dim);margin-bottom:10px;display:flex;justify-content:space-between}
.themes{display:grid;grid-template-columns:repeat(auto-fill,minmax(170px,1fr));gap:8px}
.th{background:#1b1e26;border:2px solid transparent;border-radius:10px;padding:9px 10px;cursor:pointer;text-align:left;color:inherit;font:inherit}
.th b{display:block;margin-bottom:3px}.th span{font-size:12px;color:var(--dim)}.th.sel{border-color:var(--red);background:#2a1416}
label.chk{display:flex;gap:8px;align-items:flex-start;padding:7px 0;border-bottom:1px solid var(--line);cursor:pointer}
label.chk input{margin-top:3px;accent-color:var(--red);width:16px;height:16px}label.chk span{font-size:12px;color:var(--dim);display:block}
.field{margin:10px 0}.field>span{display:flex;justify-content:space-between;font-size:12px;color:var(--dim);margin-bottom:4px}
input[type=range]{width:100%;accent-color:var(--red)}
input[type=number],select{width:100%;padding:9px;background:#1b1e26;color:var(--text);border:1px solid var(--line);border-radius:8px;font:inherit}
input[type=color]{width:100%;height:38px;border:0;background:none;padding:0}
.three{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.row{display:flex;flex-wrap:wrap;gap:8px;margin-top:12px}
.btn{background:var(--red);color:#fff;border:0;border-radius:9px;padding:10px 14px;font:inherit;font-weight:600;cursor:pointer;text-decoration:none;display:inline-block}
.btn.sec{background:#2a2e3a}
.note{font-size:12px;color:var(--dim);margin-top:8px;line-height:1.5}
#preview{width:100%;max-height:260px;display:block}
.kv{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:8px}
.kv div{background:#1b1e26;border-radius:8px;padding:7px 9px}.kv b{display:block}.kv span{font-size:11px;color:var(--dim);text-transform:uppercase}
.toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:#2a2e3a;padding:9px 16px;border-radius:9px;opacity:0;transition:opacity .3s;pointer-events:none}.toast.show{opacity:1}
</style>
</head>
<body>
<header><div class="logo">GT7 <b>LED</b></div><span class="pill" id="state">-</span><span class="sp"></span></header>
<main>
 <div class="card wide"><h2>Tema in gara</h2><div class="themes" id="themes"></div></div>
 <div class="card"><h2>Avvisi <span>sopra ogni tema</span></h2><div id="alerts"></div></div>
 <div class="card">
  <h2>Luce</h2>
  <div class="field"><span>Luminosit&agrave; <b id="bv"></b></span><input type="range" id="bright" min="0" max="255"></div>
  <div class="field"><span>Gorgoglio alla punta <b id="gv"></b></span><input type="range" id="gurgle" min="0" max="255"></div>
  <div class="field"><span>Scala del delta (lati pieni) <b id="dsv"></b></span><input type="range" id="dscale" min="1" max="50"></div>
  <div class="field"><span>Limite di corrente (mA)</span><input type="number" id="maxma" min="100" max="10000" step="100"></div>
  <h2 style="margin-top:16px">Gioco fermo</h2>
  <div class="field"><span>Animazione</span><select id="idle"></select></div>
  <div class="field"><span>Colore</span><input type="color" id="icolor"></div>
  <div class="field"><span>Spegni dopo minuti senza GT7 <b id="itv"></b></span><input type="range" id="itime" min="0" max="240" step="5"></div>
 </div>
 <div class="card wide">
  <h2>Montaggio dietro la TV <span id="total"></span></h2>
  <svg id="preview" viewBox="0 0 400 240"></svg>
  <div class="three">
   <div class="field"><span>Lato destro</span><input type="number" id="zr" min="0" max="200"></div>
   <div class="field"><span>Lato alto</span><input type="number" id="zt" min="0" max="200"></div>
   <div class="field"><span>Lato sinistro</span><input type="number" id="zl" min="0" max="200"></div>
  </div>
  <label class="chk"><input type="checkbox" id="sleft"><div>La striscia parte in basso a sinistra<span>Senza spunta: parte in basso a destra, sale, va a sinistra lungo il lato alto e scende. Nessun LED sul lato basso.</span></div></label>
  <div class="row"><button class="btn sec" id="test">Prova montaggio (15 s)</button><button class="btn" id="save">Salva come impostazioni di avvio</button></div>
  <p class="note">Nella prova il lato destro &egrave; rosso, quello alto verde, il sinistro blu e il primo LED lampeggia bianco. Ogni modifica vale subito; <b>Salva</b> la mantiene dopo il riavvio (il tema si salva da solo).</p>
 </div>
 <div class="card wide">
  <h2>Sistema</h2>
  <div class="kv" id="sysinfo"></div>
  <div class="row"><a class="btn sec" href="/update">Aggiorna firmware via Wi-Fi</a><button class="btn sec" id="reset">Azzera tempi e circuito</button><button class="btn" id="restart">Riavvia</button></div>
 </div>
</main>
<div class="toast" id="toast"></div>
<script>
'use strict';
const $=id=>document.getElementById(id);
const THEMES=[['Default','Si riempie lungo la striscia: verde, giallo, rosso'],['F1 Center','Dal centro della striscia verso i due capi'],['Supercar','Tutta blu, solo sopra il 70% dei giri'],['Smooth Fade','Tutta la striscia da azzurro a rosso'],['F1 Reverse','Dai due capi in basso fino al centro in alto'],['Default Rev','Come Default, dall\'altro capo'],
['Delta','Lati verdi in anticipo sul giro migliore, rossi in ritardo; giri in alto'],['Pedali','Gas a destra, freno a sinistra, giri in alto'],['Velocità','Scie di luce dal centro, più veloci con la velocità'],['Forze G','La luce va dove la forza spinge il corpo'],['Gomme','Ogni angolo con il colore della temperatura della gomma'],['Ambiente','Il colore del cielo all\'ora del giorno della gara'],['Solo avvisi','Luce fissa tenue: si muovono solo gli avvisi']];
const IDLES=['Spento','Colore fisso','Respiro','Arcobaleno','Onda','Fuoco','Aurora','Luce TV (bianco)'];
const ALERTS=[['Limitatore','Tutta la striscia lampeggia rossa'],['Giro migliore','Tre impulsi viola'],['Settori','Il lato alto prende il colore del settore chiuso'],['Punti di frenata','Da 150 m i lati si accendono dall\'alto'],['Riserva','Impulsi arancioni a carburante basso'],['Bloccaggi e pattinamenti','Rosso o arancione sul lato della ruota'],['Ultimo giro e arrivo','Lampi bianchi, bandiera a scacchi'],['Marcia consigliata','Angoli azzurri se serve scalare']];
let S={};
function toast(t){const e=$('toast');e.textContent=t;e.classList.add('show');clearTimeout(toast.t);toast.t=setTimeout(()=>e.classList.remove('show'),1800)}
function api(p){return fetch(p).then(r=>{if(!r.ok)throw r;return r})}
$('themes').innerHTML=THEMES.map((t,i)=>'<button class="th" data-i="'+i+'"><b>'+t[0]+'</b><span>'+t[1]+'</span></button>').join('');
$('themes').querySelectorAll('.th').forEach(b=>b.onclick=()=>{S.theme=+b.dataset.i;mark();api('/api/theme?v='+S.theme).then(()=>toast('Tema: '+THEMES[S.theme][0]))});
$('alerts').innerHTML=ALERTS.map((a,i)=>'<label class="chk"><input type="checkbox" data-bit="'+i+'"><div>'+a[0]+'<span>'+a[1]+'</span></div></label>').join('');
$('alerts').querySelectorAll('input').forEach(c=>c.onchange=()=>{S.alerts=c.checked?S.alerts|(1<<c.dataset.bit):S.alerts&~(1<<c.dataset.bit);send(false)});
$('idle').innerHTML=IDLES.map((n,i)=>'<option value="'+i+'">'+n+'</option>').join('');
function mark(){$('themes').querySelectorAll('.th').forEach(b=>b.classList.toggle('sel',+b.dataset.i==S.theme))}
function show(){mark();$('alerts').querySelectorAll('input').forEach(c=>c.checked=!!(S.alerts&(1<<c.dataset.bit)));
 $('idle').value=S.idleMode;$('icolor').value='#'+(S.idleColor>>>0).toString(16).padStart(6,'0');$('bright').value=S.brightness;$('gurgle').value=S.gurgle;
 $('dscale').value=S.deltaScale;$('maxma').value=S.maxMa;$('itime').value=S.idleOff;$('zr').value=S.right;$('zt').value=S.top;$('zl').value=S.left;$('sleft').checked=!!S.startLeft;labels()}
function labels(){$('bv').textContent=Math.round($('bright').value/2.55)+'%';$('gv').textContent=$('gurgle').value==0?'spento':$('gurgle').value;
 $('dsv').textContent=($('dscale').value/10).toFixed(1)+' s';$('itv').textContent=$('itime').value==0?'mai':$('itime').value+' min';
 $('total').textContent=(+$('zr').value + +$('zt').value + +$('zl').value)+' LED in tutto';preview()}
function read(){S.idleMode=+$('idle').value;S.idleColor=parseInt($('icolor').value.slice(1),16);S.brightness=+$('bright').value;S.gurgle=+$('gurgle').value;S.deltaScale=+$('dscale').value;
 S.maxMa=+$('maxma').value;S.idleOff=+$('itime').value;S.right=+$('zr').value;S.top=+$('zt').value;S.left=+$('zl').value;S.startLeft=$('sleft').checked?1:0}
function send(save){read();labels();const q='b='+S.brightness+'&l='+(S.right+S.top+S.left)+'&g='+S.gurgle+'&ma='+S.maxMa+'&im='+S.idleMode+'&ic='+(S.idleColor>>>0).toString(16).padStart(6,'0')+
 '&al='+S.alerts+'&zr='+S.right+'&zt='+S.top+'&zl='+S.left+'&sl='+S.startLeft+'&ds='+S.deltaScale+'&it='+S.idleOff+'&save='+(save?1:0);
 return api('/api/settings?'+q).then(()=>{if(save)toast('Salvato')}).catch(()=>toast('Valori non accettati'))}
['idle','icolor','bright','gurgle','dscale','maxma','itime','zr','zt','zl','sleft'].forEach(id=>{$(id).onchange=()=>send(false);$(id).oninput=labels});
$('save').onclick=()=>send(true);
$('test').onclick=()=>send(false).then(()=>api('/api/test?layout=1')).then(()=>toast('Prova montaggio: 15 secondi'));
$('reset').onclick=()=>{if(confirm('Azzerare tempi, mappa e dati salvati di questo circuito?'))api('/api/reset').then(()=>toast('Azzerato'))};
$('restart').onclick=()=>{if(confirm('Riavviare la striscia?'))api('/restart').then(()=>toast('Riavvio...'))};
// The strip around the TV, each side in the colour of the layout test.
function preview(){const R=+$('zr').value,T=+$('zt').value,L=+$('zl').value,n=R+T+L,left=$('sleft').checked;if(!n)return;
 const X0=60,X1=340,Y0=30,Y1=200;let h='<rect x="66" y="36" width="268" height="158" rx="6" fill="#050608" stroke="#2a2e3a"/><rect x="185" y="200" width="30" height="16" fill="#1b1e26"/>';
 const first=left?L:R,last=left?R:L,r=Math.max(1.6,Math.min(4,560/n));
 for(let i=0;i<n;i++){let x,y,c;if(i<first){const k=(i+.5)/first;x=left?X0:X1;y=Y1-k*(Y1-Y0);c=left?'#2f6bff':'#ff3b30'}else if(i<first+T){const k=(i-first+.5)/T;x=left?X0+k*(X1-X0):X1-k*(X1-X0);y=Y0;c='#16c45a'}else{const k=(i-first-T+.5)/last;x=left?X1:X0;y=Y0+k*(Y1-Y0);c=left?'#ff3b30':'#2f6bff'}
  h+='<circle cx="'+x.toFixed(1)+'" cy="'+y.toFixed(1)+'" r="'+r+'" fill="'+(i==0?'#fff':c)+'"/>'}
 $('preview').innerHTML=h}
function load(){api('/api/status').then(r=>r.json()).then(s=>{const first=!S.theme&&S.theme!==0;S=s;if(first)show();
 $('state').textContent=s.gt7?'In pista':'GT7 non in gara';$('state').className='pill'+(s.gt7?' on':'');
 const up=s.uptime|0;$('sysinfo').innerHTML=[['Firmware',s.fw],['Indirizzo',s.ip],['Segnale Wi-Fi',s.rssi+' dBm'],['Acceso da',Math.floor(up/3600)+' h '+Math.floor(up%3600/60)+' min'],['Memoria libera',Math.round(s.heap/1024)+' KB'],['Circuiti salvati',s.tracks<0?'-':s.tracks]].map(v=>'<div><span>'+v[0]+'</span><b>'+v[1]+'</b></div>').join('')}).catch(()=>{})}
load();setInterval(load,5000);
</script>
</body>
</html>
)rawliteral";
