// Il cruscotto del telefono (firmware/led-strip/src/DashPage.h) servito con
// una gara simulata, come farebbe la striscia: /cruscotto, /api/dash (flusso
// a 60 Hz), /api/session (mappa e frenate), icone, font e video.
//   node tools/screenshots/cruscotto.mjs            screenshot in docs/schermate
//   node tools/screenshots/cruscotto.mjs --serve    solo il server, su http://localhost:8080/cruscotto
// La simulazione è la stessa delle bozze (docs/bozze-cruscotto/index.html).
import { createRequire } from 'node:module';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import http from 'node:http';
import path from 'node:path';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const serveOnly = process.argv.includes('--serve');
const outDir = process.argv.find((a, i) => i > 1 && !a.startsWith('--')) ?? path.join(root, 'docs/schermate');

const page = readFileSync(path.join(root, 'firmware/led-strip/src/DashPage.h'), 'utf8');
const html = page.match(/DASH_PAGE\[\] PROGMEM = R"rawliteral\(([\s\S]*?)\)rawliteral"/)[1];
const manifest = page.match(/DASH_MANIFEST\[\] PROGMEM = R"rawliteral\(([\s\S]*?)\)rawliteral"/)[1];
const drafts = readFileSync(path.join(root, 'docs/bozze-cruscotto/index.html'), 'utf8');
const SIM = new Function(drafts.match(/(const SIM=\(\(\)=>\{[\s\S]*?\n\}\)\(\);)/)[1] + '\nreturn SIM;')();

const assets = path.join(root, 'tools/images/cruscotto');
const files = {
  '/cruscotto/num-b.woff2': ['font/woff2', path.join(assets, 'BarlowCondensed-Bold-sub.woff2')],
  '/cruscotto/num-s.woff2': ['font/woff2', path.join(assets, 'BarlowCondensed-SemiBold-sub.woff2')],
  '/cruscotto/sveglia.mp4': ['video/mp4', path.join(assets, 'sveglia.mp4')],
  '/cruscotto/sveglia.webm': ['video/webm', path.join(assets, 'sveglia.webm')],
};

// Mappa e frenate come le dà /api/session, dalla pista della simulazione
const track = SIM.track;
const xs = track.map(p => p[0]), zs = track.map(p => p[1]);
const session = {
  map: { state: 2, key: 0x5a17, b: [Math.min(...xs), Math.min(...zs), Math.max(...xs), Math.max(...zs)],
    p: track.filter((_, i) => i % 4 === 0).map(p => [Math.round(p[0]), Math.round(p[1])]) },
  brakes: SIM.brakes.map(b => [Math.round(track[b.i][0]), Math.round(track[b.i][1]), Math.round(b.kmh), null, null]),
};
const frame = () => ({ ...SIM.frame, rev: [1, 1, 1, 0, 0, 0x5a17] });

const clients = new Set();
setInterval(() => { SIM.step(1 / 60); const data = 'event: t\ndata: ' + JSON.stringify(frame()) + '\n\n'; for (const c of clients) c.write(data); }, 1000 / 60);

const server = http.createServer((req, res) => {
  const url = req.url.split('?')[0];
  if (url === '/cruscotto' || url === '/') return res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' }).end(html);
  if (url === '/manifest.webmanifest') return res.writeHead(200, { 'Content-Type': 'application/manifest+json' }).end(manifest);
  if (url === '/api/session') return res.writeHead(200, { 'Content-Type': 'application/json' }).end(JSON.stringify(session));
  if (url === '/api/dash') {
    res.writeHead(200, { 'Content-Type': 'text/event-stream', 'Cache-Control': 'no-store' });
    res.write('retry: 1500\n\n');
    clients.add(res);
    req.on('close', () => clients.delete(res));
    return;
  }
  if (files[url]) return res.writeHead(200, { 'Content-Type': files[url][0] }).end(readFileSync(files[url][1]));
  res.writeHead(404).end();
});
await new Promise(ok => server.listen(serveOnly ? 8080 : 0, ok));
const base = `http://localhost:${server.address().port}`;
if (serveOnly) {
  console.log(`Cruscotto simulato: ${base}/cruscotto`);
} else {
  // require() rispetta NODE_PATH, utile con Playwright installato globalmente
  const { chromium } = createRequire(import.meta.url)('playwright');
  const browser = await chromium.launch();
  const names = ['gara', 'delta', 'analogico', 'ingegnere', 'circuito', 'essenziale'];
  const errors = [];
  for (const [orient, viewport] of [['por', { width: 390, height: 800 }], ['lan', { width: 844, height: 370 }]]) {
    const ctx = await browser.newContext({ viewport, deviceScaleFactor: 2, hasTouch: true });
    const tab = await ctx.newPage();
    tab.on('pageerror', e => errors.push(e.message));
    for (let i = 0; i < names.length; i++) {
      await tab.addInitScript(([o, n]) => localStorage.setItem('cruscotto-' + o, n), [orient, i]);
      await tab.goto(base + '/cruscotto');
      await tab.waitForTimeout(4200); // dopo che il suggerimento in alto è sparito
      const file = path.join(outDir, `cruscotto-${names[i]}-${orient === 'por' ? 'verticale' : 'orizzontale'}.png`);
      await tab.screenshot({ path: file });
      console.log(file);
    }
    await ctx.close();
  }
  await browser.close();
  server.close();
  for (const c of clients) c.end();
  if (errors.length) { console.error(errors.join('\n')); process.exit(1); }
  process.exit(0);
}
