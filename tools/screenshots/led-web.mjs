// Screenshot della pagina web della striscia LED (firmware/led-strip/src/main.cpp)
// con /api/status simulato. Uso: node tools/screenshots/led-web.mjs [file.png]
import { createRequire } from 'node:module';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

// require() rispetta NODE_PATH, utile con Playwright installato globalmente
const { chromium } = createRequire(import.meta.url)('playwright');

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const out = process.argv[2] ?? path.join(root, 'docs/schermate/led-web.png');
const src = readFileSync(path.join(root, 'firmware/led-strip/src/main.cpp'), 'utf8');
let html = src.match(/R"rawliteral\(([\s\S]*?)\)rawliteral"/)[1];

// Stessi segnaposto sostituiti da handleRoot(), con i valori predefiniti
const values = {
  MAX_MA: '500', IDLE_COLOR: '#ffff00', BRIGHTNESS: '100', ACTIVE_LEDS: '143', GURGLE_VAL: '150',
  IDLE_2_SEL: 'selected', THEME_0_SEL: 'selected',
};
html = html.replace(/%([A-Z0-9_]+)%/g, (_, k) => values[k] ?? '');

const status = { wifi: true, ip: '192.168.1.57', gt7: true, rpm: 6150, maxRpm: 7400, speed: 38.9,
  gear: 4, throttle: 255, brake: 0, brightness: 100, theme: 0, idleMode: 2, gurgle: 150,
  leds: 143, maxMa: 500, idleColor: 0xffff00 };

const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 420, height: 900 }, deviceScaleFactor: 2 });
await page.route('**/api/**', (route) => route.fulfill({ json: status }));
await page.route('http://gt7-led.local/', (route) => route.fulfill({ contentType: 'text/html', body: html }));
await page.goto('http://gt7-led.local/');
await page.waitForTimeout(1500);
await page.screenshot({ path: out, fullPage: true });
await browser.close();
console.log(out);
