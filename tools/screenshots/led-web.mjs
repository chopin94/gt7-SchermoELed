// Screenshot della pagina web della striscia LED (firmware/led-strip/src/WebPage.h)
// con le API simulate. I dati di gara vengono dalla gara simulata dei test:
//   g++ -std=c++11 -O2 tools/screenshots/led-themes.cpp -lz -o /tmp/led-themes
//   /tmp/led-themes docs/schermate /tmp/led-web.json
//   node tools/screenshots/led-web.mjs /tmp/led-web.json [cartella]
// Produce led-web.png (telemetria), led-web-giri.png e led-web-led.png.
import { createRequire } from 'node:module';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

// require() rispetta NODE_PATH, utile con Playwright installato globalmente
const { chromium } = createRequire(import.meta.url)('playwright');

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const data = JSON.parse(readFileSync(process.argv[2] ?? '/tmp/led-web.json', 'utf8'));
const outDir = process.argv[3] ?? path.join(root, 'docs/schermate');
const src = readFileSync(path.join(root, 'firmware/led-strip/src/WebPage.h'), 'utf8');
const html = src.match(/R"rawliteral\(([\s\S]*?)\)rawliteral"/)[1];

const status = { wifi: 1, ip: '192.168.1.57', gt7: true, rpm: 6150, maxRpm: 8000, speed: 38.9, gear: 4,
  throttle: 255, brake: 0, brightness: 160, theme: 6, idleMode: 5, gurgle: 150, leds: 143, maxMa: 2500,
  idleColor: 0xffc800, themes: 13, idleModes: 8, alerts: 0x5f, right: 38, top: 67, left: 38, startLeft: 0,
  deltaScale: 10, idleOff: 30, fw: '2.0.0', rssi: -58, uptime: 5400, heap: 151000, psram: 1980000,
  tracks: 3, clients: 1 };

const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 1280, height: 900 }, deviceScaleFactor: 1 });
// Un orologio controllato: ogni fotogramma arriva 50 ms dopo il precedente.
await page.addInitScript(() => { window.__t = 1700000000000; Date.now = () => window.__t; });
await page.route('**/api/status', (r) => r.fulfill({ json: status }));
await page.route('**/api/session', (r) => r.fulfill({ json: data.session }));
await page.route('**/api/events', (r) => r.fulfill({ status: 204, body: '' }));
await page.route('http://gt7-led.local/', (r) => r.fulfill({ contentType: 'text/html', body: html }));
await page.goto('http://gt7-led.local/');
await page.waitForTimeout(500);
await page.evaluate((frames) => {
  for (const f of frames) { window.__t += 50; onFrame(f); GT.push([f.glat, f.glon]); while (GT.length > 40) GT.shift(); }
  sess = null; loadSession();
}, data.frames);
await page.evaluate((leds) => { ledColors = leds; drawPreview(); }, data.leds);
await page.waitForTimeout(800);
await page.evaluate(() => { drawMap(); drawGG(); drawTrace(); drawCmp(); });
await page.screenshot({ path: path.join(outDir, 'led-web.png'), fullPage: true });
await page.click('nav button[data-tab="laps"]');
await page.waitForTimeout(300);
await page.screenshot({ path: path.join(outDir, 'led-web-giri.png'), fullPage: true });
await page.click('nav button[data-tab="led"]');
await page.evaluate((leds) => { ledColors = leds; drawPreview(); }, data.leds);
await page.waitForTimeout(300);
await page.screenshot({ path: path.join(outDir, 'led-web-led.png'), fullPage: true });
// Telefono
await page.setViewportSize({ width: 400, height: 860 });
await page.click('nav button[data-tab="tel"]');
await page.evaluate(() => resize());
await page.waitForTimeout(300);
await page.screenshot({ path: path.join(outDir, 'led-web-telefono.png') });
await browser.close();
console.log(outDir);
