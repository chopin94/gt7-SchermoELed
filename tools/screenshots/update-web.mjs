// Screenshot della pagina /update (firmware/dashboard/lib/OtaUpdate/OtaWebUpdate.h)
// e verifica dei controlli che fa nel browser prima di inviare il file.
// Uso: node tools/screenshots/update-web.mjs [cartella_output]
import { createRequire } from 'node:module';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import assert from 'node:assert/strict';
import path from 'node:path';

// require() rispetta NODE_PATH, utile con Playwright installato globalmente
const { chromium } = createRequire(import.meta.url)('playwright');

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const outDir = process.argv[2] ?? path.join(root, 'docs/schermate');
const src = readFileSync(path.join(root, 'firmware/dashboard/lib/OtaUpdate/OtaWebUpdate.h'), 'utf8');
let html = src.match(/R"html\(([\s\S]*?)\)html"/)[1];

// Stessi segnaposto sostituiti da sendPage() sullo schermo ESP32-2432S024C
const slot = 0x1F0000;
const values = { DEVICE: 'GT7 Schermo', VERSION: '2.1.0', BOARD: 'schermo-esp32-2432s024c', CHIP: '0', MAX: String(slot) };
html = html.replace(/%([A-Z]+)%/g, (all, k) => values[k] ?? all);

// Un firmware finto con intestazione ESP-IDF e nome della scheda.
function firmware({ size = 1900000, chip = 0, app = true, tag = 'GT7-FW:schermo-esp32-2432s024c' } = {}) {
  const data = Buffer.alloc(size, 0x5a);
  data[0] = 0xe9;
  data.writeUInt16LE(chip, 12);
  if (app) data.writeUInt32LE(0xabcd5432, 32);
  if (tag) data.write(tag + '\0', Math.floor(size / 2), 'latin1');
  return data;
}

const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 420, height: 640 }, deviceScaleFactor: 2 });
let posted = 0;
await page.route('http://gt7-schermo.local/update', async (route) => {
  if (route.request().method() === 'POST') {
    posted = route.request().postDataBuffer().length;
    return route.fulfill({ contentType: 'text/plain', body: 'Aggiornato: riavvio con la versione nuova' });
  }
  return route.fulfill({ contentType: 'text/html', body: html });
});
await page.goto('http://gt7-schermo.local/update');

const msg = () => page.textContent('#msg');
async function choose(name, buffer) {
  await page.setInputFiles('#file', { name, mimeType: 'application/octet-stream', buffer });
  await page.waitForFunction(() => document.querySelector('#msg').textContent !== '');
  return msg();
}

// File rifiutati nel browser: il pulsante resta disattivato.
const refused = [
  ['firmware.bin', firmware({ tag: 'GT7-FW:schermo-esp32-st7789' }), "Firmware per un'altra scheda: schermo-esp32-st7789"],
  ['firmware.bin', firmware({ chip: 2, tag: 'GT7-FW:led-lolin-s2-mini' }), 'altro chip'],
  ['bootloader.bin', firmware({ app: false }), 'non bootloader.bin'],
  ['firmware.bin', firmware({ tag: '' }), 'senza aggiornamento Wi-Fi'],
  ['firmware.bin', firmware({ size: slot + 4096 }), 'troppo grande'],
  ['foto.jpg', Buffer.from('not a firmware at all, just some text '.repeat(10)), 'Non è un firmware'],
];
for (const [name, data, reason] of refused) {
  const text = await choose(name, data);
  assert.ok(text.includes(reason), `${name}: "${text}" non contiene "${reason}"`);
  assert.ok(await page.isDisabled('#send'), `${name}: il pulsante deve restare disattivato`);
}
await page.screenshot({ path: path.join(outDir, 'web-aggiornamento-rifiutato.png') });

// Il firmware giusto passa e viene inviato per intero.
const good = firmware();
assert.match(await choose('firmware.bin', good), /File valido/);
assert.ok(!(await page.isDisabled('#send')));
await page.click('#send');
await page.waitForFunction(() => document.querySelector('#msg').className === 'ok');
assert.equal(posted, good.length);
assert.match(await msg(), /^Aggiornato: riavvio con la versione nuova\./);
await page.waitForTimeout(400); // fine dell'animazione della barra
await page.screenshot({ path: path.join(outDir, 'web-aggiornamento.png') });
await browser.close();
console.log('Pagina /update: controlli verificati, screenshot in', outDir);
