// Screenshot della pagina web della striscia LED (firmware/led-strip/src/WebPage.h)
// con /api/status simulato. Uso: node tools/screenshots/led-web.mjs [file.png]
import { createRequire } from 'node:module';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

// require() rispetta NODE_PATH, utile con Playwright installato globalmente
const { chromium } = createRequire(import.meta.url)('playwright');

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const out = process.argv[2] ?? path.join(root, 'docs/schermate/led-web.png');
const src = readFileSync(path.join(root, 'firmware/led-strip/src/WebPage.h'), 'utf8');
const html = src.match(/R"rawliteral\(([\s\S]*?)\)rawliteral"/)[1];

const status = { wifi: 1, ip: '192.168.1.57', gt7: true, rpm: 6150, maxRpm: 8000, speed: 38.9, gear: 4,
  throttle: 255, brake: 0, brightness: 160, theme: 6, idleMode: 5, gurgle: 150, leds: 143, maxMa: 2500,
  idleColor: 0xffc800, themes: 13, idleModes: 8, alerts: 0x57, right: 38, top: 67, left: 38, startLeft: 0,
  deltaScale: 10, idleOff: 30, fw: '2.0.0', rssi: -58, uptime: 5400, heap: 151000, tracks: 3 };

const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 1000, height: 900 }, deviceScaleFactor: 1 });
await page.route('**/api/**', (r) => r.fulfill({ json: status }));
await page.route('http://gt7-led.local/', (r) => r.fulfill({ contentType: 'text/html', body: html }));
await page.goto('http://gt7-led.local/');
await page.waitForTimeout(800);
await page.screenshot({ path: out, fullPage: true });
await browser.close();
console.log(out);
