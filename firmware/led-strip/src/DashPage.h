#pragma once
#include <pgmspace.h>

// The phone dashboard (/cruscotto): six layouts, each in portrait and
// landscape, on a black screen. It reads /api/dash, one frame for every GT7
// packet (up to 60 a second), and draws at the screen's own rate with the
// values interpolated between frames. Fonts and icons come from the strip
// (DashAssets.h), so it works without internet. The drafts it comes from are
// in docs/bozze-cruscotto; tools/screenshots/cruscotto.mjs takes it from here
// for the screenshots in the README.
static const char DASH_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover,user-scalable=no">
<meta name="theme-color" content="#000000">
<meta name="color-scheme" content="dark">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black">
<meta name="apple-mobile-web-app-title" content="GT7 Cruscotto">
<link rel="manifest" href="/manifest.webmanifest">
<link rel="apple-touch-icon" href="/apple-touch-icon.png">
<link rel="icon" href="/icon-192.png">
<link rel="preload" href="/cruscotto/num-b.woff2" as="font" type="font/woff2" crossorigin>
<title>GT7 Cruscotto</title>
<style>
@font-face{font-family:Num;src:url(/cruscotto/num-b.woff2) format('woff2');font-weight:700 900;font-display:swap}
@font-face{font-family:Num;src:url(/cruscotto/num-s.woff2) format('woff2');font-weight:400 600;font-display:swap}
:root{--ink:#f1f2f5;--dim:#7c8191;--off:#121318;--red:#ff2b1c;--green:#22d66a;--yellow:#ffc21a;--purple:#b14dff;--blue:#2f8cff;--cyan:#3cc8ff;--amber:#ff8a00;
 --num:Num,'Roboto Condensed','Arial Narrow',sans-serif-condensed,system-ui,sans-serif;--ui:system-ui,-apple-system,'Segoe UI',Roboto,sans-serif}
*{box-sizing:border-box;margin:0;padding:0}
[hidden]{display:none!important}
html,body{height:100%;background:#000;color:var(--ink);overflow:hidden;overscroll-behavior:none}
body{font:14px var(--ui);-webkit-tap-highlight-color:transparent;-webkit-text-size-adjust:100%}
#app{position:fixed;inset:0;display:flex;padding:env(safe-area-inset-top,0px) env(safe-area-inset-right,0px) env(safe-area-inset-bottom,0px) env(safe-area-inset-left,0px)}
.dash{flex:1;min-width:0;min-height:0;position:relative;background:#000;font-family:var(--num);font-variant-numeric:tabular-nums;
 overflow:hidden;user-select:none;-webkit-user-select:none;touch-action:none;container-type:size}
.dash>div{height:100%}
.dash canvas{display:block;width:100%;height:100%;position:absolute;inset:0}
.lbl{font:600 max(10px,2.2cqmin) var(--ui);letter-spacing:1.4px;text-transform:uppercase;color:var(--dim)}
.v{font-weight:700;line-height:.95;white-space:nowrap}
.cell{display:flex;flex-direction:column;justify-content:center;min-width:0;min-height:0}
.r{text-align:right;align-items:flex-end}
.shift{display:grid;grid-template-columns:repeat(16,1fr);gap:.8cqmin;height:100%}
.shift i{border-radius:.8cqmin;background:var(--off)}
.shift i.g{background:var(--green)}.shift i.y{background:var(--yellow)}.shift i.r{background:var(--red)}.shift i.b,.shift.flash i{background:var(--blue)}
.shift.flash.blink i{background:var(--off)}
.secs{display:grid;grid-template-columns:repeat(3,1fr);gap:1cqmin}
.secs i{min-height:6px;border-radius:.8cqmin;background:#191b21;font-style:normal;display:flex;align-items:center;justify-content:center;font-weight:700;color:#000}
.secs i.s1{background:#3a3e4a;color:var(--ink)}.secs i.s2{background:var(--purple)}.secs i.s3{background:var(--green)}.secs i.s4{background:var(--yellow)}
.pos{color:var(--green)}.neg{color:var(--red)}.nil{color:var(--dim)}
.chip{display:inline-block;font:700 max(10px,2cqmin) var(--ui);letter-spacing:1px;padding:.4cqmin 1.2cqmin;border-radius:.8cqmin;background:#16181e;color:#3a3e4a}
.chip.on{background:var(--amber);color:#000}
.ped{display:flex;gap:1cqmin;height:100%}
.ped div{flex:1;background:var(--off);border-radius:.8cqmin;position:relative;overflow:hidden}
.ped b{position:absolute;inset:0;transform-origin:50% 100%;transform:scaleY(0);will-change:transform}
.ped .t b{background:var(--green)}.ped .k b{background:var(--red)}
.ped i{position:absolute;inset:0;border-top:2px solid #fff;transform:translateY(100%);will-change:transform}
.hb{background:var(--off);border-radius:.8cqmin;overflow:hidden;position:relative}
.hb b{position:absolute;inset:0;transform-origin:0 50%;transform:scaleX(0);background:var(--cyan);will-change:transform}
.hb.low b{background:var(--red)}
.tyre{border-radius:2cqmin;display:flex;flex-direction:column;align-items:center;justify-content:center;border:3px solid transparent;color:#000;font-weight:700;background:#191b21}
.tyre.lock{border-color:#fff}.tyre.spin{border-color:var(--cyan)}
.tyre.k0{background:#191b21;color:var(--dim)}.tyre.k1{background:var(--blue)}.tyre.k2{background:var(--green)}.tyre.k3{background:var(--yellow)}.tyre.k4{background:var(--red)}

/* 1 Gara */
.d-gara{display:grid;padding:2cqmin 3cqmin 3cqmin;gap:2cqmin}
.por .d-gara{grid-template:"sh sh" 4cqh "gr gr" minmax(0,1fr) "sp dl" auto "sc sc" 1.6cqh "t1 t2" auto "t3 t4" auto/1fr 1fr}
.lan .d-gara{grid-template:"sh sh sh" 7cqh "sp gr dl" minmax(0,1fr) "sc sc sc" 3cqh "t1 t2 t3" auto/1fr 1.1fr 1fr;column-gap:4cqmin}
.lan .d-gara .t4{display:none}.lan .d-gara .t2{align-items:center;text-align:center}.lan .d-gara .t3{align-items:flex-end;text-align:right}
.d-gara .gear{grid-area:gr;display:flex;flex-direction:column;align-items:center;justify-content:center;min-height:0}
.d-gara .gear .v{font-size:min(62cqh,52cqw);font-weight:800}
.lan .d-gara .gear .v{font-size:56cqh}
.d-gara .sg{font:600 3.2cqmin var(--ui);color:var(--cyan);height:4cqmin}
.d-gara .rpmtxt{font-size:4cqmin;color:var(--dim)}
.d-gara .big{font-size:13cqmin}.lan .d-gara .big{font-size:20cqh}
.d-gara .tm{font-size:7cqmin}.lan .d-gara .tm{font-size:10cqh}

/* 2 Delta */
.d-delta{display:grid;padding:2cqmin 3cqmin 3cqmin;gap:2cqmin}
.por .d-delta{grid-template:"sh sh" 2.4cqh "hd hd" minmax(0,1fr) "bar bar" 3cqh "sc sc" 7cqh "pr pr" auto "a b" auto/1fr 1fr}
.lan .d-delta{grid-template:"sh sh sh" 4cqh "hd hd pr" minmax(0,1fr) "bar bar a" 5cqh "sc sc b" 12cqh/1fr 1fr .9fr;column-gap:4cqmin}
.d-delta .hd{grid-area:hd;border-radius:3cqmin;display:flex;flex-direction:column;align-items:center;justify-content:center;background:#101114;transition:background-color .3s}
.d-delta .hd.up1{background:#06200f}.d-delta .hd.up2{background:#0a3519}.d-delta .hd.up3{background:#0f4d24}
.d-delta .hd.dn1{background:#240806}.d-delta .hd.dn2{background:#3d0d09}.d-delta .hd.dn3{background:#58120c}
.d-delta .hd .v{font-size:min(30cqh,32cqw);font-weight:800}
.lan .d-delta .hd .v{font-size:min(46cqh,24cqw)}
.d-delta .mini{display:flex;gap:4cqmin;font-size:6cqmin;color:var(--dim);margin-top:1cqmin}
.d-delta .mini b{color:var(--ink)}
.d-delta .bar{grid-area:bar;position:relative;background:var(--off);border-radius:1cqmin;overflow:hidden}
.d-delta .bar b{position:absolute;top:0;bottom:0;left:50%;width:50%;transform-origin:0 50%;transform:scaleX(0);background:var(--red);will-change:transform}
.d-delta .bar b.up{background:var(--green)}
.d-delta .bar::after{content:"";position:absolute;left:50%;top:0;bottom:0;width:2px;background:#fff}
.d-delta .secs i{font-size:4.6cqmin}
.d-delta .x{font-size:8cqmin}

/* 3 Analogico */
.d-ana{display:grid;padding:2cqmin 3cqmin 3cqmin;gap:2.4cqmin}
.por .d-ana{grid-template:"tc tc tc" minmax(0,1fr) "a b c" auto "d e f" auto "fu fu fu" 2cqh/1fr 1fr 1fr}
.lan .d-ana{grid-template:"a tc d" 1fr "b tc e" 1fr "c tc f" 1fr "fu fu fu" 3cqh/1fr 1.7fr 1fr;column-gap:4cqmin}
.d-ana .tc{grid-area:tc;position:relative;min-height:0}
.d-ana .tc .in{position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center;pointer-events:none}
.d-ana .tc .gv{font-size:min(26cqh,30cqw);font-weight:800}
.lan .d-ana .tc .gv{font-size:40cqh}
.d-ana .tc .kv{font-size:min(8cqh,9cqw);font-weight:600;margin-top:1cqmin}
.d-ana .x{font-size:9cqmin}.lan .d-ana .x{font-size:13cqh}
.d-ana small{font:600 3cqmin var(--ui);color:var(--dim);margin-left:.6cqmin}

/* 4 Ingegnere */
.d-ing{display:grid;padding:2cqmin 3cqmin 3cqmin;gap:2.4cqmin}
.por .d-ing{grid-template:"hd hd" auto "gc gc" minmax(0,1fr) "ty pd" 30cqh "ev ev" auto/1fr 24cqw}
.lan .d-ing{grid-template:"hd hd hd" auto "ty gc pd" minmax(0,1fr) "ev ev ev" auto/1.1fr 1.2fr .7fr;column-gap:3cqmin}
.d-ing .hd{grid-area:hd;display:flex;justify-content:space-between;align-items:baseline}
.d-ing .hd .v{font-size:12cqmin}.lan .d-ing .hd .v{font-size:14cqh}
.d-ing .tys{grid-area:ty;display:grid;grid-template-columns:1fr 1fr;grid-template-rows:1fr 1fr;gap:2cqmin;min-height:0}
.d-ing .tyre .v{font-size:8cqmin}
.d-ing .tyre small{font:600 2.6cqmin var(--ui);opacity:.75}
.d-ing .gc{grid-area:gc;min-height:0;position:relative}
.d-ing .pd{grid-area:pd;display:grid;grid-template-rows:auto 1fr;gap:1cqmin;min-height:0}
.d-ing .ev{grid-area:ev;display:flex;gap:3cqmin;justify-content:space-between;align-items:center;font-size:5cqmin;flex-wrap:wrap}
.d-ing .ev span.lbl{margin-right:.8cqmin}

/* 5 Circuito */
.d-map{display:grid;padding:2cqmin 3cqmin 3cqmin;gap:2cqmin}
.por .d-map{grid-template:"sh sh sh" 2.4cqh "mp mp mp" minmax(0,1fr) "bk bk bk" auto "g k d" auto/1fr 1fr 1fr}
.lan .d-map{grid-template:"sh sh" 4cqh "mp bk" 1fr "mp g" auto "mp k" auto "mp d" auto/1.5fr 1fr;column-gap:4cqmin}
.d-map .mp{grid-area:mp;min-height:0;position:relative}
.d-map .bk{grid-area:bk;border-radius:2.4cqmin;background:var(--off);padding:2cqmin 3cqmin;display:flex;justify-content:space-between;align-items:center;gap:2cqmin}
.d-map .bk.near{background:var(--red)}.d-map .bk.near .lbl{color:#ffd3cf}
.d-map .bk .v{font-size:11cqmin}.lan .d-map .bk .v{font-size:14cqh}
.d-map .x{font-size:9cqmin}.lan .d-map .x{font-size:11cqh}

/* 6 Essenziale */
.d-min{display:grid;padding:3cqmin;gap:3cqmin}
.por .d-min{grid-template:"sh" 9cqh "gr" minmax(0,1fr) "dl" auto/1fr}
.lan .d-min{grid-template:"sh sh" 14cqh "gr dl" 1fr/1fr 1fr;column-gap:4cqmin}
.d-min .shift{grid-template-columns:repeat(10,1fr);gap:1.2cqmin;align-items:center}
.d-min .shift i{border-radius:50%;aspect-ratio:1;width:100%;max-width:8cqh;justify-self:center}
.d-min .gear{grid-area:gr;display:flex;align-items:center;justify-content:center;min-height:0}
.d-min .gear .v{font-size:min(60cqh,70cqw);font-weight:800}
.lan .d-min .gear .v{font-size:80cqh}
.d-min .dl{grid-area:dl;display:flex;flex-direction:column;align-items:center;justify-content:center}
.d-min .dl .v{font-size:18cqmin;font-weight:800}
.lan .d-min .dl .v{font-size:30cqh}

/* stato, menu */
#note{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;pointer-events:none;z-index:2}
#note span{font:700 max(16px,5cqmin) var(--num);letter-spacing:3px;color:var(--yellow);background:#000c;padding:1.5cqmin 4cqmin;border-radius:2cqmin;text-transform:uppercase}
#note.off span{color:var(--dim);font-size:max(14px,3.6cqmin)}
#menu{position:fixed;left:0;right:0;bottom:0;z-index:5;background:#0b0c10f2;border-top:1px solid #1d2029;
 padding:14px max(16px,env(safe-area-inset-right,0px)) calc(14px + env(safe-area-inset-bottom,0px)) max(16px,env(safe-area-inset-left,0px));display:grid;gap:12px}
#menu .row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
#menu button,#menu a{font:600 14px var(--ui);color:var(--dim);background:#16181e;border:1px solid #23262f;border-radius:10px;padding:9px 13px;text-decoration:none;cursor:pointer}
#menu button.sel{color:#fff;border-color:var(--red);background:#1e1012}
#menu button:focus-visible,#menu a:focus-visible{outline:2px solid var(--cyan);outline-offset:2px}
#menu p{font-size:12.5px;color:var(--dim);line-height:1.4}
#menu p b{color:var(--ink);font-weight:600}
#menu .sp{flex:1}
#hint{position:absolute;left:0;right:0;top:6px;text-align:center;font:600 11px var(--ui);color:#4b4f5c;z-index:3;pointer-events:none;transition:opacity .8s}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
</style>
</head>
<body>
<div id="app"><div class="dash por" id="dash"></div><div id="note" hidden><span></span></div></div>
<div id="hint">tocca per il menu &middot; scorri per cambiare cruscotto</div>
<div id="menu" hidden>
 <div class="row" id="pick"></div>
 <div class="row"><button id="fs">Nascondi anche la barra di stato</button><a href="/">Telemetria completa</a><span class="sp"></span><button id="close">Chiudi</button></div>
 <p id="info"></p>
</div>
<script>
'use strict';
const $=(r,s)=>r.querySelector(s);
const DPR=()=>Math.min(3,window.devicePixelRatio||1);
// ---------- formats
const lt=ms=>{if(!(ms>0))return'-:--.---';const m=Math.floor(ms/60000),s=(ms%60000)/1000;return m+':'+s.toFixed(3).padStart(6,'0')};
const dlt=ms=>{if(ms==null)return'--.--';const c=Math.round(ms/10);return(c<0?'−':c>0?'+':'±')+(Math.abs(c)/100).toFixed(2)};
const gr=g=>g==15?'N':g==0?'R':g==null?'-':''+g;
const n1=(v,d)=>v==null||isNaN(v)?'--':(+v).toFixed(d);
const tyreClass=t=>!(t>1)?'k0':t<65?'k1':t<85?'k2':t<100?'k3':'k4';

// ---------- frames: interpolated between the last two, at the screen rate
let prev=null,cur=null,curAt=0,lastAt=0,gap=33,dirty=true,ses=null;
const SMOOTH=['rpm','kmh','glat','glon','x','z','prog','d','thr','brk','thro','brko','nb','gu','boost','fuel'];
function onFrame(f){
 const now=performance.now();
 if(cur){const g=now-curAt;if(g>4&&g<250)gap+=(g-gap)*.1}
 prev=cur;cur=f;curAt=now;lastAt=now;dirty=true;
 TRAIL.push([f.glat||0,f.glon||0]);if(TRAIL.length>40)TRAIL.shift();
 checkSession(f);
}
function view(now){
 if(!cur)return null;if(!prev)return cur;
 const a=Math.min(1,(now-curAt)/gap);if(a>=1)return cur;
 const o=Object.assign({},cur);
 for(const k of SMOOTH){const p=prev[k],c=cur[k];if(typeof p!=='number'||typeof c!=='number')continue;
  if(k==='prog'&&Math.abs(c-p)>.5)continue;if(k==='d'&&Math.abs(c-p)>1500)continue;if(k==='nb'&&c>p+20)continue;
  if((k==='x'||k==='z')&&Math.abs(c-p)>60)continue;
  o[k]=p+(c-p)*a}
 return o;
}

// ---------- canvases: sized by ResizeObserver, static layers drawn once
const RO=new ResizeObserver(es=>{for(const e of es){const c=e.target;c._w=e.contentRect.width;c._h=e.contentRect.height;c._sized=false;dirty=true}});
function ctx(c){if(!c._w)return null;const d=DPR();
 if(!c._sized){c.width=Math.round(c._w*d);c.height=Math.round(c._h*d);c._sized=true;c._key=null}
 const x=c.getContext('2d');x.setTransform(d,0,0,d,0,0);return x}
function layer(c,key,draw){if(c._key===key&&c._layer)return c._layer;
 const l=c._layer||document.createElement('canvas');l.width=c.width;l.height=c.height;const x=l.getContext('2d');const d=DPR();x.setTransform(d,0,0,d,0,0);x.clearRect(0,0,c._w,c._h);
 draw(x,c._w,c._h);c._layer=l;c._key=key;return l}
function blit(x,c,l){x.setTransform(1,0,0,1,0,0);x.clearRect(0,0,c.width,c.height);x.drawImage(l,0,0);const d=DPR();x.setTransform(d,0,0,d,0,0)}

// ---------- widgets
function shiftLights(el,o,n){
 if(!el._b){el.innerHTML='<i></i>'.repeat(n);el._b=[...el.children];el._s=[]}
 const mx=o.mx>0?o.mx:8000,al=o.al>0&&o.al<mx?o.al:mx*.85,lo=al-(mx-al)*.6;
 const f=Math.max(0,Math.min(1,(o.rpm-lo)/(mx-lo))),on=o.live?Math.round(f*n):0,flash=o.live&&o.rpm>=mx-30;
 el.classList.toggle('flash',flash);el.classList.toggle('blink',flash&&performance.now()%140<70);
 for(let i=0;i<n;i++){const c=i<on?(i<n*.4?'g':i<n*.75?'y':i<n*.94?'r':'b'):'';if(el._s[i]!==c){el._b[i].className=c;el._s[i]=c}}
 return flash;
}
function sectors(el,o,times){
 if(!el._b){el.innerHTML='<i></i><i></i><i></i>';el._b=[...el.children];el._s=[]}
 for(let i=0;i<3;i++){const st=o.ss?o.ss[i]|0:0,t=times?(st>=2&&o.sm[i]>0?(o.sm[i]/1000).toFixed(2):st==1?'S'+(i+1):''):'',k=st+'|'+t;
  if(el._s[i]!==k){el._b[i].className='s'+st;el._b[i].textContent=t;el._s[i]=k}}
}
function setCls(el,c){if(el._c!==c){el.className=c;el._c=c}}
function setTf(el,t){if(el._t!==t){el.style.transform=t;el._t=t}}
const TRAIL=[];
function drawG(c,o){const x=ctx(c);if(!x)return;const w=c._w,h=c._h,r=Math.min(w,h)/2-10,cx=w/2,cy=h/2,G=1.8;
 blit(x,c,layer(c,'g'+w+'x'+h,(y)=>{y.strokeStyle='#1d2029';y.lineWidth=1;for(const g of[.5,1,1.5]){y.beginPath();y.arc(cx,cy,r*g/G,0,7);y.stroke()}
  y.beginPath();y.moveTo(cx-r,cy);y.lineTo(cx+r,cy);y.moveTo(cx,cy-r);y.lineTo(cx,cy+r);y.stroke();
  y.fillStyle='#3a3e4a';y.font='600 11px system-ui';y.textAlign='center';y.fillText('1 G',cx+r/G+12,cy-4);y.fillText('FRENATA',cx,cy-r+12);y.fillText('ACCELERAZIONE',cx,cy+r-4)}));
 // the dot moves like the load: up braking, left in a right turn (as on the main page)
 const P=p=>[cx-p[0]/G*r,cy+p[1]/G*r];
 x.lineWidth=3;for(let i=1;i<TRAIL.length;i++){const a=P(TRAIL[i-1]),b=P(TRAIL[i]);x.strokeStyle='rgba(60,200,255,'+(i/TRAIL.length*.8).toFixed(2)+')';x.beginPath();x.moveTo(a[0],a[1]);x.lineTo(b[0],b[1]);x.stroke()}
 const p=P([o.glat||0,o.glon||0]);x.fillStyle='#fff';x.beginPath();x.arc(p[0],p[1],7,0,7);x.fill()}
function drawTach(c,o){const x=ctx(c);if(!x)return;const w=c._w,h=c._h,r=Math.min(w,h)/2-8,cx=w/2,cy=h/2;
 const mx=o.mx>0?o.mx:8000,al=o.al>0&&o.al<mx?o.al:mx*.85,top=Math.ceil(mx/1000+.3)*1000,a0=Math.PI*.75,a1=Math.PI*2.25,A=v=>a0+(a1-a0)*Math.max(0,Math.min(1,v/top));
 blit(x,c,layer(c,'t'+w+'x'+h+'|'+mx+'|'+top,(y)=>{y.lineWidth=r*.09;y.strokeStyle='#121318';y.beginPath();y.arc(cx,cy,r,a0,a1);y.stroke();
  y.strokeStyle='#4a0d09';y.beginPath();y.arc(cx,cy,r,A(mx),a1);y.stroke();
  y.fillStyle='#7c8191';y.font='600 '+Math.round(r*.11)+'px Num,system-ui';y.textAlign='center';y.textBaseline='middle';
  for(let k=0;k<=top/1000;k++){const a=A(k*1000);y.strokeStyle=k*1000>=mx?'#ff2b1c':'#3a3e4a';y.lineWidth=2;y.beginPath();
   y.moveTo(cx+Math.cos(a)*r*.92,cy+Math.sin(a)*r*.92);y.lineTo(cx+Math.cos(a)*r*.82,cy+Math.sin(a)*r*.82);y.stroke();y.fillText(k,cx+Math.cos(a)*r*.71,cy+Math.sin(a)*r*.71)}
  y.font='600 '+Math.round(r*.075)+'px system-ui';y.fillText('×1000 giri/min',cx,cy-r*.5)}));
 const rpm=o.live?o.rpm:0;x.lineWidth=r*.09;x.strokeStyle=rpm>=mx-30?'#2f8cff':rpm>=al?'#ff2b1c':'#f1f2f5';x.beginPath();x.arc(cx,cy,r,a0,A(rpm));x.stroke();
 const a=A(rpm);x.strokeStyle='#ff2b1c';x.lineWidth=Math.max(3,r*.025);x.lineCap='round';x.beginPath();
 x.moveTo(cx+Math.cos(a)*r*.5,cy+Math.sin(a)*r*.5);x.lineTo(cx+Math.cos(a)*r*1.02,cy+Math.sin(a)*r*1.02);x.stroke();x.lineCap='butt'}
const SECCOL=['#2a2d36','#2a2d36','#b14dff','#22d66a','#ffc21a'];
function drawMap(c,o){const x=ctx(c);if(!x)return;const w=c._w,h=c._h,m=ses&&ses.map,pts=m&&m.p||[];
 if(pts.length<3){x.clearRect(0,0,w,h);x.fillStyle='#7c8191';x.font='600 14px system-ui';x.textAlign='center';
  x.fillText(m&&m.state==1?'Mappa in costruzione: completa il giro':'La mappa si disegna dal primo giro lanciato',w/2,h/2);return}
 const b=m.b||[0,0,1,1],pad=16,sc=Math.min((w-2*pad)/Math.max(1,b[2]-b[0]),(h-2*pad)/Math.max(1,b[3]-b[1])),ox=(w-(b[2]-b[0])*sc)/2,oy=(h-(b[3]-b[1])*sc)/2;
 const P=p=>[ox+(p[0]-b[0])*sc,oy+(p[1]-b[1])*sc],th=Math.max(4,Math.min(w,h)/45),n=pts.length,done=m.state==2;
 const col=i=>{const st=o.ss&&o.ss[i]>1?o.ss[i]:(o.ls?o.ls[i]:0);return o.sec==i?'#5d6170':SECCOL[st|0]||'#2a2d36'};
 const key='m'+w+'x'+h+'|'+ses._rev+'|'+(done?[0,1,2].map(col).join():'')+'|'+o.nbi;
 blit(x,c,layer(c,key,(y)=>{y.lineCap='round';y.lineJoin='round';y.lineWidth=th;
  for(let s=0;s<3;s++){const i0=Math.floor(s*n/3),i1=Math.min(n-1+(done?1:0),Math.floor((s+1)*n/3));y.strokeStyle=done?col(s):'#4a4e5a';y.beginPath();
   for(let i=i0;i<=i1;i++){const p=P(pts[i%n]);i==i0?y.moveTo(p[0],p[1]):y.lineTo(p[0],p[1])}y.stroke()}
  (ses.brakes||[]).forEach((z,i)=>{const p=P(z);y.fillStyle='#ff2b1c';y.beginPath();y.arc(p[0],p[1],i===o.nbi?th*.95:th*.55,0,7);y.fill();
   if(i===o.nbi){y.strokeStyle='#fff';y.lineWidth=2;y.stroke();y.lineWidth=th}})}));
 if(o.x!=null&&o.z!=null){const p=P([o.x,o.z]);x.fillStyle='#fff';x.strokeStyle='#000';x.lineWidth=2;x.beginPath();x.arc(p[0],p[1],th*1.2,0,7);x.fill();x.stroke()}}

// ---------- the six dashboards
const DRAFTS=[
 {id:'gara',name:'Gara',html:'<div class="d-gara"><div class="shift" style="grid-area:sh"></div>'+
  '<div class="gear"><div class="sg"></div><div class="v" data-k="g"></div><div class="rpmtxt"><span data-k="rpm"></span> giri</div></div>'+
  '<div class="cell" style="grid-area:sp"><span class="lbl">km/h</span><span class="v big" data-k="kmh"></span></div>'+
  '<div class="cell r" style="grid-area:dl"><span class="lbl">delta</span><span class="v big" data-k="d"></span></div>'+
  '<div class="secs" style="grid-area:sc"></div>'+
  '<div class="cell" style="grid-area:t1"><span class="lbl">giro <span data-k="lapn"></span></span><span class="v tm" data-k="cur"></span></div>'+
  '<div class="cell r t2" style="grid-area:t2"><span class="lbl">migliore</span><span class="v tm" data-k="best"></span></div>'+
  '<div class="cell t3" style="grid-area:t3"><span class="lbl">ultimo</span><span class="v tm" data-k="last"></span></div>'+
  '<div class="cell r t4" style="grid-area:t4"><span class="lbl">giri carburante</span><span class="v tm" data-k="fl"></span></div></div>',
  update(r,o){const f=shiftLights(r._q('.shift'),o,16);sectors(r._q('.secs'),o,false);
   const sg=o.sg>0&&o.sg<15&&o.sg!=o.g?'▲ '+o.sg:'',e=r._q('.sg');if(e.textContent!==sg)e.textContent=sg;return f}},
 {id:'delta',name:'Delta',html:'<div class="d-delta"><div class="shift" style="grid-area:sh"></div>'+
  '<div class="hd"><span class="lbl">delta sul riferimento</span><span class="v" data-k="d"></span><div class="mini"><span><b data-k="g"></b> marcia</span><span><b data-k="kmh"></b> km/h</span></div></div>'+
  '<div class="bar"><b></b></div><div class="secs" style="grid-area:sc"></div>'+
  '<div class="cell" style="grid-area:pr"><span class="lbl">giro previsto</span><span class="v x" data-k="pred"></span></div>'+
  '<div class="cell" style="grid-area:a"><span class="lbl">migliore</span><span class="v x" data-k="best"></span></div>'+
  '<div class="cell" style="grid-area:b"><span class="lbl">ultimo</span><span class="v x" data-k="last"></span></div></div>',
  update(r,o){const f=shiftLights(r._q('.shift'),o,16);sectors(r._q('.secs'),o,true);
   const d=o.d,k=d==null?0:Math.max(-1,Math.min(1,d/1000)),lvl=Math.min(3,Math.ceil(Math.abs(k)*3));
   setCls(r._q('.hd'),'hd'+(d==null||!lvl?'':(d<0?' up':' dn')+lvl));
   const b=r._q('.bar b');setCls(b,d<0?'up':'');setTf(b,'scaleX('+k.toFixed(3)+')');return f}},
 {id:'ana',name:'Analogico',html:'<div class="d-ana"><div class="tc"><canvas></canvas><div class="in"><span class="v gv" data-k="g"></span><span class="v kv"><span data-k="kmh"></span><small>km/h</small></span></div></div>'+
  '<div class="cell" style="grid-area:a"><span class="lbl">carburante</span><span class="v x"><span data-k="fuel"></span><small>L</small></span></div>'+
  '<div class="cell" style="grid-area:b"><span class="lbl">giri rimasti</span><span class="v x" data-k="fl"></span></div>'+
  '<div class="cell" style="grid-area:c"><span class="lbl">turbo</span><span class="v x"><span data-k="boost"></span><small>bar</small></span></div>'+
  '<div class="cell r" style="grid-area:d"><span class="lbl">acqua</span><span class="v x"><span data-k="wt"></span><small>°C</small></span></div>'+
  '<div class="cell r" style="grid-area:e"><span class="lbl">olio</span><span class="v x"><span data-k="ot"></span><small>°C</small></span></div>'+
  '<div class="cell r" style="grid-area:f"><span class="lbl">pressione olio</span><span class="v x"><span data-k="oil"></span><small>bar</small></span></div>'+
  '<div class="hb" style="grid-area:fu"><b></b></div></div>',
  update(r,o){drawTach(r._q('canvas'),o);const hb=r._q('.hb');setCls(hb,'hb'+(o.fl!=null&&o.fl<2?' low':''));
   setTf(hb.firstChild,'scaleX('+(o.cap>0?Math.max(0,Math.min(1,o.fuel/o.cap)):0).toFixed(3)+')');return false}},
 {id:'ing',name:'Ingegnere',html:'<div class="d-ing"><div class="hd"><span><span class="v" data-k="g"></span> <span class="lbl">marcia</span></span><span><span class="v" data-k="kmh"></span> <span class="lbl">km/h</span></span><span class="v" data-k="d"></span></div>'+
  '<div class="tys">'+'<div class="tyre"><small></small><span class="v"></span><small></small></div>'.repeat(4)+'</div>'+
  '<div class="gc"><canvas></canvas></div>'+
  '<div class="pd"><span class="lbl">pedali</span><div class="ped"><div class="t"><b></b><i></i></div><div class="k"><b></b><i></i></div></div></div>'+
  '<div class="ev"><span><span class="lbl">uso grip</span><span data-k="gu"></span></span><span><span class="lbl">bloccaggi</span><span data-k="lock"></span></span><span><span class="lbl">pattinamenti</span><span data-k="spin"></span></span>'+
  '<span><span class="chip" data-c="abs">ABS</span> <span class="chip" data-c="tcs">TCS</span> <span class="chip" data-c="asm">ASM</span></span></div></div>',
  update(r,o){drawG(r._q('canvas'),o);
   if(!r._ty)r._ty=[...r.querySelectorAll('.tyre')].map(t=>({t,s:t.children}));
   const N=['ANT SX','ANT DX','POST SX','POST DX'];
   r._ty.forEach((y,w)=>{const tt=o.tt?o.tt[w]:0,ws=o.ws?o.ws[w]:0;setCls(y.t,'tyre '+tyreClass(tt)+(ws==2?' lock':ws==3?' spin':''));
    const a=N[w],b=tt>1?Math.round(tt)+'°':'--',c='slitt. '+(o.sl?Math.round(o.sl[w]):0)+'%';
    if(y.s[0].textContent!==a)y.s[0].textContent=a;if(y.s[1].textContent!==b)y.s[1].textContent=b;if(y.s[2].textContent!==c)y.s[2].textContent=c});
   if(!r._pd)r._pd=[...r.querySelectorAll('.ped>div')].map(d=>[d.firstChild,d.lastChild]);
   // bar: the pedal after ABS/TCS; white line: the driver's foot
   const ped=(k,out,inp)=>{setTf(r._pd[k][0],'scaleY('+(Math.max(0,Math.min(100,out||0))/100).toFixed(3)+')');setTf(r._pd[k][1],'translateY('+(100-Math.max(0,Math.min(100,inp||0))).toFixed(1)+'%)')};
   ped(0,o.thro,o.thr);ped(1,o.brko,o.brk);
   if(!r._ch)r._ch=[...r.querySelectorAll('[data-c]')];
   const on=[!!o.abs,!!(o.f&2048),!!(o.f&1024)];r._ch.forEach((c,i)=>setCls(c,'chip'+(on[i]?' on':'')));return false}},
 {id:'map',name:'Circuito',html:'<div class="d-map"><div class="secs" style="grid-area:sh"></div><div class="mp"><canvas></canvas></div>'+
  '<div class="bk"><div class="cell"><span class="lbl">prossima frenata</span><span class="v" data-k="nb"></span></div><div class="cell r"><span class="lbl">velocità di ingresso</span><span class="v" data-k="nbk"></span></div></div>'+
  '<div class="cell" style="grid-area:g"><span class="lbl">marcia</span><span class="v x" data-k="g"></span></div>'+
  '<div class="cell" style="grid-area:k"><span class="lbl">km/h</span><span class="v x" data-k="kmh"></span></div>'+
  '<div class="cell" style="grid-area:d"><span class="lbl">delta</span><span class="v x" data-k="d"></span></div></div>',
  map:true,
  update(r,o){sectors(r._q('.secs'),o,false);drawMap(r._q('canvas'),o);setCls(r._q('.bk'),'bk'+(o.nb!=null&&o.nb<120?' near':''));return false}},
 {id:'min',name:'Essenziale',html:'<div class="d-min"><div class="shift" style="grid-area:sh"></div><div class="gear"><span class="v" data-k="g"></span></div>'+
  '<div class="dl"><span class="v" data-k="d"></span><span class="lbl">delta &middot; giro <span data-k="lapn"></span></span></div></div>',
  update(r,o){return shiftLights(r._q('.shift'),o,10)}}
];
const FMT={g:o=>gr(o.g),kmh:o=>Math.round(o.kmh||0),rpm:o=>Math.round((o.rpm||0)/10)*10,d:o=>dlt(o.d),cur:o=>lt(o.cur),best:o=>lt(o.best),last:o=>lt(o.last),
 lapn:o=>o.tl>0?o.lap+'/'+o.tl:o.lap|0,fl:o=>n1(o.fl,1),fuel:o=>n1(o.fuel,1),boost:o=>o.boost==null?'--':(o.boost>=0?'+':'')+o.boost.toFixed(2),
 wt:o=>n1(o.wt,0),ot:o=>n1(o.ot,0),oil:o=>n1(o.oil,1),gu:o=>Math.round((o.gu||0)*100)+'%',lock:o=>o.lk?o.lk[0]:0,spin:o=>o.lk?o.lk[1]:0,
 nb:o=>o.nb==null?'--':Math.round(o.nb)+' m',nbk:o=>o.nbk==null?'--':Math.round(o.nbk),pred:o=>o.ref>0&&o.d!=null?lt(o.ref+o.d):'-:--.---'};

// ---------- mount and paint
const dash=document.getElementById('dash');
let orient='por',sel=0;
const store={get(k){try{return localStorage.getItem(k)}catch(e){return null}},set(k,v){try{localStorage.setItem(k,v)}catch(e){}}};
function chosen(o){const v=parseInt(store.get('cruscotto-'+o),10);return v>=0&&v<DRAFTS.length?v:(o==='por'?0:0)}
function mount(i){
 sel=i;const D=DRAFTS[i];RO.disconnect();dash.innerHTML=D.html;const r=dash.firstChild;
 const q={};r._q=s=>q[s]||(q[s]=r.querySelector(s));
 dash._b=[...r.querySelectorAll('[data-k]')].map(n=>({n,k:n.dataset.k,last:null,d:n.dataset.k==='d'}));
 dash._d=D;dash._r=r;r.querySelectorAll('canvas').forEach(c=>RO.observe(c));
 dirty=true;if(D.map)loadSession(true);renderMenu();
}
function paint(o){
 for(const b of dash._b){const t=''+FMT[b.k](o);if(t!==b.last){b.n.textContent=t;b.last=t}
  if(b.d){const c=o.d==null?'nil':o.d<0?'pos':'neg';if(b.c!==c){b.n.classList.remove('pos','neg','nil');b.n.classList.add(c);b.c=c}}}
 return dash._d.update(dash._r,o);
}
const note=document.getElementById('note');
let noteText=null;
function setNote(t,cls){if(t!==noteText){note.hidden=!t;note.firstChild.textContent=t||'';note.className=cls||'';noteText=t}}
const IDLE={live:0,rpm:0,mx:0,al:0,kmh:0,g:15,d:null,ss:[0,0,0],sm:[0,0,0],tt:[0,0,0,0],sl:[0,0,0,0],ws:[0,0,0,0],lk:[0,0,0,0],f:0};
function loop(now){
 const stale=!cur||now-lastAt>2000;
 setNote(stale?(cur?'Nessun dato dalla striscia':'Collegamento alla striscia…'):!cur.live?'In attesa di GT7':(cur.f&2)?'In pausa':'',stale||!cur.live?'off':'');
 if(dirty||(prev&&now-curAt<gap)||anim){const o=view(now)||IDLE;anim=!!paint(o);dirty=false}
 requestAnimationFrame(loop);
}
let anim=false;

// ---------- orientation: each one remembers its dashboard
function orientNow(){return innerWidth>innerHeight?'lan':'por'}
function applyOrient(){const o=orientNow();if(o===orient&&dash._d)return;orient=o;dash.className='dash '+o;mount(chosen(o))}
addEventListener('resize',applyOrient);

// ---------- stream
let es=null;
function connect(){if(es)es.close();es=new EventSource('/api/dash');
 es.addEventListener('t',e=>{try{onFrame(JSON.parse(e.data))}catch(err){}})}
// the strip sends a frame at least 4 times a second: silence means a lost connection
setInterval(()=>{if(!document.hidden&&performance.now()-lastAt>3000)connect()},3000);
document.addEventListener('visibilitychange',()=>{if(document.hidden){if(es){es.close();es=null}}else{connect();wake()}});

// ---------- session (map and braking points), only for the map dashboard
let revKey='',sesBusy=false,sesAt=0;
function checkSession(f){if(!f.rev)return;const k=f.rev[1]+','+f.rev[2]+','+f.rev[3]+','+f.rev[5];if(k!==revKey){revKey=k;if(dash._d&&dash._d.map)loadSession(false)}}
function loadSession(force){const now=performance.now();if(sesBusy||(!force&&now-sesAt<3000)){if(!force)setTimeout(()=>loadSession(false),3000);return}
 sesBusy=true;sesAt=now;fetch('/api/session').then(r=>r.json()).then(s=>{s._rev=revKey+'/'+now;ses=s;dirty=true}).catch(()=>{}).finally(()=>{sesBusy=false})}

// ---------- the screen stays on: Wake Lock where the browser allows it
// (HTTPS or a page added to the home screen), else a muted video in a loop
let lock=null,video=null;
async function wake(){
 if(document.hidden)return;
 if('wakeLock' in navigator&&window.isSecureContext){try{if(!lock){lock=await navigator.wakeLock.request('screen');lock.addEventListener('release',()=>{lock=null})}return}catch(e){}}
 try{if(!video){video=document.createElement('video');video.muted=true;video.loop=true;video.playsInline=true;video.setAttribute('playsinline','');video.setAttribute('muted','');
   video.style.cssText='position:fixed;width:1px;height:1px;opacity:0;pointer-events:none;left:0;top:0';document.body.appendChild(video);
   const mp4=video.canPlayType('video/mp4')!=='';const b=await (await fetch(mp4?'/cruscotto/sveglia.mp4':'/cruscotto/sveglia.webm')).blob();video.src=URL.createObjectURL(b)}
  await video.play()}catch(e){}
}

// ---------- touch: swipe to change, tap for the menu
const menu=document.getElementById('menu'),hint=document.getElementById('hint');
let sx=0,sy=0,st=0,touching=false;
dash.addEventListener('pointerdown',e=>{sx=e.clientX;sy=e.clientY;st=performance.now();touching=true;wake()});
dash.addEventListener('pointerup',e=>{if(!touching)return;touching=false;const dx=e.clientX-sx,dy=e.clientY-sy;
 if(Math.abs(dx)>50&&Math.abs(dx)>Math.abs(dy)*1.3)choose((sel+(dx<0?1:DRAFTS.length-1))%DRAFTS.length);
 else if(Math.abs(dx)<12&&Math.abs(dy)<12&&performance.now()-st<500)toggleMenu()});
dash.addEventListener('pointercancel',()=>{touching=false});
document.addEventListener('keydown',e=>{if(e.key==='ArrowRight')choose((sel+1)%DRAFTS.length);else if(e.key==='ArrowLeft')choose((sel+DRAFTS.length-1)%DRAFTS.length);else if(e.key==='Escape')menu.hidden=true});
function choose(i){store.set('cruscotto-'+orient,i);mount(i)}
let menuTimer=0;
function toggleMenu(show){menu.hidden=show===undefined?!menu.hidden:!show;clearTimeout(menuTimer);if(!menu.hidden)menuTimer=setTimeout(()=>menu.hidden=true,6000)}
function renderMenu(){const p=document.getElementById('pick');
 p.innerHTML=DRAFTS.map((d,i)=>'<button data-i="'+i+'" class="'+(i===sel?'sel':'')+'">'+(i+1)+' '+d.name+'</button>').join('');
 p.querySelectorAll('button').forEach(b=>b.onclick=()=>{choose(+b.dataset.i);toggleMenu(true)});
 const app=matchMedia('(display-mode: standalone)').matches||matchMedia('(display-mode: fullscreen)').matches||navigator.standalone;
 document.getElementById('info').innerHTML='In <b>'+(orient==='por'?'verticale':'orizzontale')+'</b> si apre <b>'+DRAFTS[sel].name+'</b>: verticale e orizzontale ricordano ognuno il proprio cruscotto.'+
  (app?'':' Per avere solo la barra di stato nera sopra al cruscotto, senza le barre del browser: menu del browser → <b>Aggiungi a schermata Home</b>.')+
  (fixedNote?' '+fixedNote:'')}
// Without a fixed address the icon on the home screen stops working when the
// address changes: say so in the menu (once per page load).
let fixedNote='';
fetch('/api/status').then(r=>r.json()).then(s=>{if(!s.fixed){fixedNote='L\'indirizzo della striscia ('+s.ip+') può cambiare: prima di aggiungere alla Home fissalo in <b>Telemetria completa → Sistema → Indirizzo fisso</b>.';renderMenu()}}).catch(()=>{});
document.getElementById('close').onclick=()=>toggleMenu(false);
document.getElementById('fs').onclick=()=>{const d=document.documentElement;
 if(document.fullscreenElement){document.exitFullscreen().catch(()=>{})}
 else if(d.requestFullscreen){d.requestFullscreen({navigationUI:'hide'}).catch(()=>{})}toggleMenu(false)};
setTimeout(()=>hint.style.opacity=0,3500);

applyOrient();connect();requestAnimationFrame(loop);
</script>
</body>
</html>
)rawliteral";

// The manifest for "Add to home screen": opens without the browser bars,
// with the phone's status bar (clock, notifications, battery) kept on black.
static const char DASH_MANIFEST[] PROGMEM = R"rawliteral({
"name":"GT7 Cruscotto","short_name":"Cruscotto","start_url":"/cruscotto","scope":"/","display":"standalone",
"orientation":"any","background_color":"#000000","theme_color":"#000000",
"icons":[{"src":"/icon-192.png","sizes":"192x192","type":"image/png","purpose":"any"},
{"src":"/icon-512.png","sizes":"512x512","type":"image/png","purpose":"any maskable"}]
})rawliteral";
