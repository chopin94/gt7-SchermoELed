<div align="center">

# 🏁 GT7 Schermo e LED

**Cruscotto da 320×240 e striscia LED shift light per Gran Turismo 7 e Assetto Corsa**<br>
Due ESP32 che leggono la telemetria del gioco in Wi-Fi, senza PC.

![ESP32](https://img.shields.io/badge/ESP32-2432S024C%20%2F%202432S028-E7352C?logo=espressif&logoColor=white)
![ESP32-S2](https://img.shields.io/badge/ESP32--S2-mini%20%2B%20WS2812B-E7352C?logo=espressif&logoColor=white)
![PlatformIO](https://img.shields.io/badge/PlatformIO-Arduino-F5822A?logo=platformio&logoColor=white)
![Temi](https://img.shields.io/badge/temi%20schermo-15-FFCC00)
![Temi LED](https://img.shields.io/badge/temi%20LED-6-22C55E)
![Delta live](https://img.shields.io/badge/delta-live-B030FF)

<img src="docs/schermate/mosaico-temi.png" alt="I 15 temi del cruscotto e il riepilogo della sessione" width="100%">

<sub>Tutte le immagini sono generate dal vero codice grafico del firmware: sono pixel per pixel quello che compare sullo schermo. I dati di gara vengono da una simulazione di guida su un circuito sintetico, elaborata dallo stesso motore di analisi del firmware.</sub>

</div>

---

## Indice

- [Come funziona](#come-funziona)
- [Hardware](#hardware)
- [Funzioni](#funzioni)
- [Analisi del giro: delta live, settori, mappa](#analisi-del-giro-delta-live-settori-mappa)
- [Temi del cruscotto (15)](#temi-del-cruscotto-15)
- [Notifiche](#notifiche)
- [Schermata di attesa](#schermata-di-attesa)
- [Menu e impostazioni](#menu-e-impostazioni)
- [Striscia LED](#striscia-led)
- [Comunicazione tra schermo e LED](#comunicazione-tra-schermo-e-led)
- [Compilare e caricare i firmware](#compilare-e-caricare-i-firmware)
- [Struttura del repository](#struttura-del-repository)
- [Rigenerare gli screenshot](#rigenerare-gli-screenshot)
- [Prossimi passi](#prossimi-passi)
- [Crediti e licenze](#crediti-e-licenze)

---

## Come funziona

```mermaid
flowchart LR
    PS5["🎮 PS5 / PS4<br/>Gran Turismo 7"]
    AC["🏎️ Assetto Corsa<br/>(PC o console)"]
    PC["💻 PC + SimHub"]
    DASH["📟 Schermo 320×240<br/>ESP32-2432S024C"]
    LED["🚥 Striscia LED WS2812B<br/>ESP32-S2 mini"]

    PS5 -- "UDP 33739/33740<br/>telemetria cifrata Salsa20" --> DASH
    PS5 -- "UDP 33739/33740" --> LED
    AC -- "UDP 9996<br/>Remote Telemetry" --> DASH
    PC -- "USB seriale<br/>Custom Protocol DSH1" --> DASH
    LED -- "UDP 33741 broadcast<br/>annuncio GT7LED" --> DASH
    DASH -- "HTTP /api/settings<br/>/api/theme" --> LED
```

- Lo **schermo** e la **striscia LED** si collegano alla stessa rete Wi-Fi della console e ricevono la telemetria di GT7 direttamente, ognuno per conto suo.
- La striscia si annuncia in rete ogni 2 secondi. Lo schermo la trova da solo e diventa il suo **telecomando**: tema, luminosità, colore e animazione di riposo si cambiano dal touch, senza aprire la pagina web.
- In alternativa a GT7 lo schermo legge **Assetto Corsa** in Wi-Fi, oppure qualsiasi gioco PC tramite **SimHub** via USB.
- Dai dati grezzi del gioco lo schermo calcola da solo **delta live, settori, mappa del circuito, forze G e tempi di accelerazione** ([come](#analisi-del-giro-delta-live-settori-mappa)).

## Hardware

| Componente | Modello | Note |
| --- | --- | --- |
| Schermo | **ESP32-2432S024C** (2.4″ ILI9341, touch capacitivo CST820) | scheda in uso, ambiente `esp32-2432s024c` |
| Schermo, alternativa | ESP32-2432S028 (2.8″ ILI9341 o ST7789, touch resistivo) | ambienti `esp32` e `esp32-st7789` |
| Controller LED | **LOLIN / Wemos ESP32-S2 mini** | dati LED sul **GPIO 7** |
| LED | Striscia **WS2812B** (fino a 200 LED, 143 di default) | alimentazione 5 V, limite corrente regolabile |

## Funzioni

**Schermo**

- 🚗 Telemetria **GT7 diretta in Wi-Fi**, con rilevamento automatico della PS5 (nessun IP da inserire)
- 🏎️ Telemetria **Assetto Corsa** in Wi-Fi (PC e console): il limite dei giri viene imparato per ogni auto
- 🔌 **SimHub USB** per i giochi PC, con [un'unica formula valida per tutti i temi](firmware/dashboard/simhub/custom-protocol.txt)
- 🎨 **15 temi** selezionabili con anteprima dal touch
- ⏱️ **Delta live** metro per metro rispetto al giro migliore, in tutti i temi (oppure differenza ultimo giro - migliore, a scelta)
- 🟪 **3 settori** con i colori della F1: viola miglior tempo, verde meglio del giro migliore, giallo più lento
- 🗺️ **Mappa del circuito** disegnata da sola durante il primo giro, con la posizione dell'auto e i settori colorati
- 📈 **Forze G** con scia e **tracce di gas e freno** in stile MoTeC
- 🚦 **Cronometro accelerazioni** automatico: 0-100, 0-200, 100-200 km/h e 400 m con velocità d'uscita, con l'albero di Natale da drag race e il tempo di reazione
- 🔔 **Notifiche a schermo**: GIRO MIGLIORE, ULTIMO GIRO, RISERVA
- 📋 **Riepilogo della sessione** sulla schermata di attesa: migliore, giro ideale, media, velocità massima, ultimi giri
- ⛽ Consumo e autonomia stimata, rilevamento automatico delle auto elettriche
- 🚨 Spie ABS, TCS, ASM, freno a mano, box; marcia consigliata (Ferrari Gold, BMW M)
- 🕐 Orologio con ora legale automatica (tema BMW M)
- 🖼️ Tre sfondi per la schermata di attesa (Minimale, Racing, Riepilogo)
- 🚥 Controllo completo della striscia LED dal touch
- 📶 Configurazione Wi-Fi con QR code; se la rete salvata non si trova, il portale si riapre da solo
- 🌙 Spegnimento manuale e sospensione/risveglio automatici
- 🇮🇹 Interfaccia in italiano

**Striscia LED**

- 6 temi shift light, effetto "gorgoglio" alla punta, lampeggio rosso al limitatore (flag ufficiale di GT7)
- 4 animazioni a gioco fermo o in pausa: spento, colore fisso, respiro, arcobaleno
- Pagina web di configurazione con telemetria live
- Impostazioni salvate in memoria, configurazione Wi-Fi con WiFiManager

---

## Analisi del giro: delta live, settori, mappa

GT7 e Assetto Corsa trasmettono solo dati grezzi: posizione, velocità, contagiri, tempo del giro in corso. Tutto il resto lo calcola lo schermo, in [`src/LapAnalysis.h`](firmware/dashboard/src/LapAnalysis.h). È C++ puro, verificato da [test automatici](firmware/dashboard/tests/lap_analysis.cpp) su un circuito simulato.

| Cosa | Come viene calcolato |
| --- | --- |
| **Delta live** | Durante ogni giro lo schermo salva il tempo ogni 12,5 m di strada percorsa (GT7) o ogni 1/2048 di giro (AC, che fornisce la frazione del giro). Il delta confronta il tempo attuale con quello del giro migliore **nello stesso punto della pista**: verde se sei in anticipo, rosso se sei in ritardo. |
| **Settori** | La pista è divisa in tre parti uguali. Ogni settore diventa **viola** se è il migliore della sessione, **verde** se è più veloce dello stesso settore del giro migliore, **giallo** se è più lento. Il **giro ideale** è la somma dei tre settori migliori. |
| **Mappa** | Al primo passaggio sul traguardo lo schermo comincia a registrare le coordinate dell'auto. Il tracciato compare mentre guidi e si chiude al passaggio successivo. Le coordinate X e Z dei giochi danno una mappa con lo stesso orientamento di quella del gioco. |
| **Forze G** | Laterale: velocità × rapidità di sterzata della traiettoria. Longitudinale: variazione di velocità. Sul cerchio il punto si muove come la forza che senti: in alto in frenata, a sinistra in una curva a destra. |
| **Accelerazioni** | Fermati per 1 secondo: si accende l'albero di Natale (2 luci bianche, 3 gialle ogni 0,5 s, poi verde). Il tempo parte quando l'auto si muove e la prova finisce quando freni o rilasci. Il 100-200 viene misurato anche in movimento. |

Per evitare dati falsati:

- Il **giro di uscita dai box** non viene contato: il suo inizio non passa dal traguardo.
- Il **primo giro di gara** parte dalla griglia, dietro al traguardo. Viene cronometrato, ma non diventa il riferimento del delta e non disegna la mappa: la mappa parte dal primo giro lanciato.
- Un **salto di posizione** (riavvio, rewind, cambio sessione) annulla il giro in corso.
- **Cambiando auto** i tempi ripartono da zero e la mappa resta. **Cambiando pista** riparte tutto: Assetto Corsa comunica il nome del circuito, per GT7 lo schermo se ne accorge quando l'auto resta a più di 200 m dal tracciato registrato.
- Dal menu **FUNZIONI** puoi azzerare mappa e tempi in ogni momento.

> Con **SimHub** il delta resta quello calcolato da SimHub. Mappa, settori, forze G e accelerazioni servono i dati diretti di GT7 o Assetto Corsa.

---

## Temi del cruscotto (15)

Tutte le schermate dei temi, nello stesso momento di gara simulato:

- **In curva**: quarto giro, in anticipo sul giro migliore.
- **In frenata**: quinto giro, ABS attivo, in ritardo.
- **Al limitatore**: sesta marcia, luci che lampeggiano.

### I 4 nuovi temi con l'analisi del giro

#### FORMULA
Volante da monoposto. Shift light verdi, rossi e viola, tutti viola lampeggianti al limitatore. Delta live su un riquadro verde o rosso, marcia al centro, velocità, giro in corso e ultimo giro. Sotto, i tre settori con colori e tempi, le quattro gomme agli angoli (colore in base alla temperatura), il giro migliore, il giro, la posizione e il carburante.

| In curva | In frenata | Al limitatore |
| :-: | :-: | :-: |
| <img src="docs/schermate/theme-formula.png" width="260"> | <img src="docs/schermate/theme-formula-frenata.png" width="260"> | <img src="docs/schermate/theme-formula-limitatore.png" width="260"> |

#### MAPPA PISTA
Prima del traguardo la mappa è vuota. Durante il primo giro lanciato il tracciato si disegna da solo e la scala si adatta man mano. Poi compaiono la posizione dell'auto (pallino rosso), la linea del traguardo e i tre settori colorati: quelli del giro in corso già completati, gli altri del giro precedente. A destra: marcia, velocità, delta, giro in corso, ultimo, migliore, settori, giro e posizione.

| Prima del traguardo | Mappa in costruzione | In curva |
| :-: | :-: | :-: |
| <img src="docs/schermate/theme-mappa-pista-attesa.png" width="260"> | <img src="docs/schermate/theme-mappa-pista-costruzione.png" width="260"> | <img src="docs/schermate/theme-mappa-pista.png" width="260"> |
| **In frenata** | **Al limitatore** | |
| <img src="docs/schermate/theme-mappa-pista-frenata.png" width="260"> | <img src="docs/schermate/theme-mappa-pista-limitatore.png" width="260"> | |

#### TELEMETRIA
Cerchio delle forze G con la scia degli ultimi 3 secondi: in frenata il punto sale, in una curva a destra va a sinistra. Accanto, le tracce di gas (verde) e freno (rosso) degli ultimi 7 secondi, il delta e, in basso, G laterale, G longitudinale e i picchi della sessione.

| In curva | In frenata | Al limitatore |
| :-: | :-: | :-: |
| <img src="docs/schermate/theme-telemetria.png" width="260"> | <img src="docs/schermate/theme-telemetria-frenata.png" width="260"> | <img src="docs/schermate/theme-telemetria-limitatore.png" width="260"> |

#### PRESTAZIONI
Cronometro automatico. Fermati 1 secondo e l'albero di Natale si accende: 2 luci bianche, poi 3 gialle ogni mezzo secondo, poi la verde. Se parti prima della verde si accende la rossa (falsa partenza). Misura 0-100, 0-200 e 100-200 km/h, i 400 m con la velocità d'uscita e la velocità massima, sia per l'ultima prova sia per la migliore della sessione (in viola).

| Partenza (luci gialle) | Prova in corso | Prova finita |
| :-: | :-: | :-: |
| <img src="docs/schermate/theme-prestazioni-partenza.png" width="260"> | <img src="docs/schermate/theme-prestazioni-in-corsa.png" width="260"> | <img src="docs/schermate/theme-prestazioni.png" width="260"> |
| **Falsa partenza** | **Al limitatore** | |
| <img src="docs/schermate/theme-prestazioni-falsa-partenza.png" width="260"> | <img src="docs/schermate/theme-prestazioni-limitatore.png" width="260"> | |

### Gli altri 11 temi

| # | Tema | In curva | In frenata | Al limitatore |
| :-: | --- | :-: | :-: | :-: |
| 0 | **Classic**<br><sub>Layout originale a 5 colonne, tempi grandi, delta colorato</sub> | <img src="docs/schermate/theme-classic.png" width="230"> | <img src="docs/schermate/theme-classic-frenata.png" width="230"> | <img src="docs/schermate/theme-classic-limitatore.png" width="230"> |
| 1 | **GT3**<br><sub>Tema scuro motorsport (predefinito), contagiri ad arco</sub> | <img src="docs/schermate/theme-gt3.png" width="230"> | <img src="docs/schermate/theme-gt3-frenata.png" width="230"> | <img src="docs/schermate/theme-gt3-limitatore.png" width="230"> |
| 2 | **Retro**<br><sub>Strumentazione carta e inchiostro</sub> | <img src="docs/schermate/theme-retro.png" width="230"> | <img src="docs/schermate/theme-retro-frenata.png" width="230"> | <img src="docs/schermate/theme-retro-limitatore.png" width="230"> |
| 3 | **Radar**<br><sub>Contagiri circolare al centro</sub> | <img src="docs/schermate/theme-radar.png" width="230"> | <img src="docs/schermate/theme-radar-frenata.png" width="230"> | <img src="docs/schermate/theme-radar-limitatore.png" width="230"> |
| 4 | **Mono**<br><sub>Digitale monocromatico</sub> | <img src="docs/schermate/theme-mono.png" width="230"> | <img src="docs/schermate/theme-mono-frenata.png" width="230"> | <img src="docs/schermate/theme-mono-limitatore.png" width="230"> |
| 5 | **Pocket**<br><sub>LCD da console portatile a 4 toni</sub> | <img src="docs/schermate/theme-pocket.png" width="230"> | <img src="docs/schermate/theme-pocket-frenata.png" width="230"> | <img src="docs/schermate/theme-pocket-limitatore.png" width="230"> |
| 6 | **Endurance**<br><sub>Gare di durata: gomme, carburante, giri</sub> | <img src="docs/schermate/theme-endurance.png" width="230"> | <img src="docs/schermate/theme-endurance-frenata.png" width="230"> | <img src="docs/schermate/theme-endurance-limitatore.png" width="230"> |
| 7 | **Ferrari**<br><sub>Marcia su pannello giallo, 15 LED, freno/gas, turbo e benzina</sub> | <img src="docs/schermate/theme-ferrari.png" width="230"> | <img src="docs/schermate/theme-ferrari-frenata.png" width="230"> | <img src="docs/schermate/theme-ferrari-limitatore.png" width="230"> |
| 8 | **Ferrari AC**<br><sub>Per Assetto Corsa: frizione, avanzamento giro, spia PIT</sub> | <img src="docs/schermate/theme-ferrari-ac.png" width="230"> | <img src="docs/schermate/theme-ferrari-ac-frenata.png" width="230"> | <img src="docs/schermate/theme-ferrari-ac-limitatore.png" width="230"> |
| 9 | **Ferrari Gold**<br><sub>Come Ferrari, con la marcia consigliata nell'angolo del pannello</sub> | <img src="docs/schermate/theme-ferrari-gold.png" width="230"> | <img src="docs/schermate/theme-ferrari-gold-frenata.png" width="230"> | <img src="docs/schermate/theme-ferrari-gold-limitatore.png" width="230"> |
| 10 | **BMW M**<br><sub>Pannello blu con le strisce M, orologio, marcia consigliata</sub> | <img src="docs/schermate/theme-bmw-m.png" width="230"> | <img src="docs/schermate/theme-bmw-m-frenata.png" width="230"> | <img src="docs/schermate/theme-bmw-m-limitatore.png" width="230"> |

<sub>Numeri dei temi come nel firmware: 11 Formula, 12 Mappa pista, 13 Telemetria, 14 Prestazioni. I temi 7–14 sono stati creati in questo progetto; i temi 0–6 vengono da <a href="https://github.com/caa1211/esp32-gt7-dashboard">esp32-gt7-dashboard</a>.</sub>

## Notifiche

Per 3 secondi occupano lo schermo. Giri, marcia, tempo del giro e velocità restano aggiornati in alto e in basso. Si possono disattivare dal menu **FUNZIONI**.

| Giro migliore | Primo giro | Ultimo giro | Riserva |
| :-: | :-: | :-: | :-: |
| <img src="docs/schermate/notifica-giro-migliore.png" width="200"> | <img src="docs/schermate/notifica-primo-giro.png" width="200"> | <img src="docs/schermate/notifica-ultimo-giro.png" width="200"> | <img src="docs/schermate/notifica-riserva.png" width="200"> |
| Nuovo record, con il miglioramento | Il primo giro cronometrato della sessione | All'inizio dell'ultimo giro di gara (GT7) | Carburante per meno di 1,5 giri o sotto l'8% |

## Schermata di attesa

Compare quando il gioco non trasmette. In alto ci sono lo stato del Wi-Fi e della striscia LED: il pallino è verde se è collegata o trovata, e toccando "LED" se ne aprono i controlli. In basso c'è l'indirizzo IP dello schermo.

| Minimale | Racing | Riepilogo |
| :-: | :-: | :-: |
| <img src="docs/schermate/waiting-minimale.png" width="260"> | <img src="docs/schermate/waiting-racing.png" width="260"> | <img src="docs/schermate/waiting-riepilogo.png" width="260"> |
| **Riepilogo senza giri** | **Wi-Fi scollegato** | |
| <img src="docs/schermate/waiting-riepilogo-vuoto.png" width="260"> | <img src="docs/schermate/waiting-wifi-disconnesso.png" width="260"> | |

Lo sfondo **Riepilogo** mostra i tempi della sessione appena guidata: giro migliore, giro ideale (somma dei settori migliori), media, velocità massima e gli ultimi giri con il distacco dal migliore.

## Menu e impostazioni

Tocca lo schermo in qualsiasi momento per aprire il menu. Senza tocchi si chiude da solo dopo 15 secondi.

| Impostazioni | Funzioni | Funzioni dopo l'azzeramento |
| :-: | :-: | :-: |
| <img src="docs/schermate/menu-impostazioni.png" width="260"> | <img src="docs/schermate/menu-funzioni.png" width="260"> | <img src="docs/schermate/menu-funzioni-azzerato.png" width="260"> |
| **Selezione tema** | **Anteprima a tutto schermo** | **Dispositivo** |
| <img src="docs/schermate/menu-selezione-tema.png" width="260"> | <img src="docs/schermate/menu-anteprima-tema.png" width="260"> | <img src="docs/schermate/menu-dispositivo.png" width="260"> |
| **Sfondo: Minimale** | **Sfondo: Racing** | **Sfondo: Riepilogo** |
| <img src="docs/schermate/menu-sfondo-minimale.png" width="260"> | <img src="docs/schermate/menu-sfondo-racing.png" width="260"> | <img src="docs/schermate/menu-sfondo-riepilogo.png" width="260"> |
| **Controllo LED** | **LED non trovata** | **Scelta connessione** |
| <img src="docs/schermate/menu-led.png" width="260"> | <img src="docs/schermate/menu-led-non-trovata.png" width="260"> | <img src="docs/schermate/menu-scelta-connessione.png" width="260"> |
| **Configurazione Wi-Fi** | **Ripristino totale** | **Calibrazione touch** |
| <img src="docs/schermate/menu-wifi.png" width="260"> | <img src="docs/schermate/menu-ripristino.png" width="260"> | <img src="docs/schermate/menu-calibrazione-touch.png" width="260"> |
| **Verifica calibrazione** | | |
| <img src="docs/schermate/menu-calibrazione-verifica.png" width="260"> | | |

| Schermata | Cosa fa |
| --- | --- |
| **Impostazioni** | Accesso a tema, sfondo di attesa, funzioni, LED e dispositivo |
| **Funzioni** | Notifiche sì/no, delta *live* (metro per metro) o *ultimo giro* (differenza ultimo giro - migliore), **azzera mappa e tempi** |
| **Selezione tema** | Anteprima dal vivo, frecce per scorrere i 15 temi, *Applica* per salvarlo; toccando l'anteprima si vede a tutto schermo |
| **Sfondo attesa** | Sceglie tra *Minimale* (logo Sparco su blu), *Racing* (sfondo a tutto schermo) e *Riepilogo* (tempi della sessione); la scelta resta dopo il riavvio |
| **Controllo LED** | Tema e animazione di riposo, slider di luminosità e colore (si trascina col dito), ON/OFF |
| **Dispositivo** | Luminosità dello schermo, sorgente dati (GT7, AC, SimHub), ripristino di fabbrica |
| **Scelta connessione** | Al primo avvio: GT7 diretto, Assetto Corsa diretto o SimHub USB |
| **Configurazione Wi-Fi** | QR code per collegarsi alla rete `GT7-DASH-SETUP` e aprire `192.168.4.1` |
| **Calibrazione touch** | Solo al primo avvio: si tocca il bersaglio, poi lo si verifica e si salva |

---

## Striscia LED

### Temi

| # | Tema | Effetto |
| :-: | --- | --- |
| 0 | **Default** | Si riempie da sinistra: verde fino al 60%, giallo fino all'85%, poi rosso |
| 5 | **Default Reverse** | Come Default, da destra a sinistra |
| 1 | **F1 Center** | Dal centro verso i bordi, simmetrico, verde/giallo/rosso |
| 4 | **F1 Reverse** | Dai bordi verso il centro |
| 2 | **Supercar** | Si accende solo sopra il 70% dei giri, tutta blu |
| 3 | **Smooth Fade** | Tutta la striscia sfuma da azzurro ghiaccio a rosso |

Con il **limitatore** attivo (flag di GT7) tutti i temi lampeggiano in rosso. Il **gorgoglio** fa tremolare gli ultimi LED accesi, con intensità regolabile (0 = spento).

### Animazioni a gioco fermo

| Modo | Effetto |
| --- | --- |
| Spento | Nessuna luce |
| Colore fisso | Colore scelto, a luminosità piena |
| Respiro | Il colore scelto pulsa lentamente |
| Arcobaleno | Arcobaleno che scorre |

### Pagina web

All'indirizzo IP della striscia (mostrato sul monitor seriale) c'è la pagina di configurazione con telemetria in tempo reale. Al primo avvio la striscia crea la rete Wi-Fi **`GT7_LED_Config`** per inserire la password di casa.

<img src="docs/schermate/led-web.png" alt="Pagina web della striscia LED" width="360">

---

## Comunicazione tra schermo e LED

| Direzione | Canale | Contenuto |
| --- | --- | --- |
| LED → rete | UDP broadcast, porta **33741**, ogni 2 s | `GT7LED\|<ip della striscia>` |
| Schermo → LED | `GET http://<ip>/api/status` | Stato JSON: `rpm`, `gear`, `brightness`, `theme`, `idleMode`, `gurgle`, `leds`, `maxMa`, `idleColor`, … |
| Schermo → LED | `GET http://<ip>/api/settings?b=&l=&g=&ma=&im=&ic=&save=1` | Luminosità, numero LED, gorgoglio, limite mA, animazione di riposo, colore |
| Schermo → LED | `GET http://<ip>/api/theme?v=<0-5>` | Tema LED |
| Browser → LED | `GET http://<ip>/restart` | Riavvio |

Gli indici di tema (0–5) e di animazione di riposo (0–3) sono gli stessi nei due firmware: `enum Theme` e `enum IdleMode` in [`firmware/led-strip/src/main.cpp`](firmware/led-strip/src/main.cpp).

---

## Compilare e caricare i firmware

Serve **[PlatformIO](https://platformio.org/)**: l'estensione per VS Code oppure `pip install platformio`. Le librerie e il compilatore ESP32 si scaricano da soli alla prima compilazione (la vecchia cartella `core` non va copiata).

**Schermo**

```bash
cd firmware/dashboard
pio run -e esp32-2432s024c -t upload     # ESP32-2432S024C (touch capacitivo)
# pio run -e esp32 -t upload             # ESP32-2432S028 ILI9341
# pio run -e esp32-st7789 -t upload      # ESP32-2432S028 ST7789
pio device monitor
```

**Striscia LED**

```bash
cd firmware/led-strip
pio run -t upload
pio device monitor
```

La porta COM viene trovata in automatico. Se non la trova, togli il `;` davanti a `upload_port` in `platformio.ini` e scrivi la tua porta (per esempio `COM9` per lo schermo, `COM8` per i LED).

> GitHub Actions compila entrambi i firmware a ogni push ed esegue i test. I file `.bin` pronti si scaricano dagli *Artifacts* della scheda **Actions**.

**Primo avvio dello schermo:** calibrazione touch → scelta connessione → configurazione Wi-Fi tramite QR. Poi avvia GT7 sulla stessa rete: lo schermo trova la PS5 da solo.

---

## Struttura del repository

```
├── firmware/
│   ├── dashboard/                   Schermo 320×240 (PlatformIO, ESP32)
│   │   ├── platformio.ini           3 ambienti: 2432S024C, 2432S028, 2432S028-ST7789
│   │   ├── src/
│   │   │   ├── main.cpp             Avvio, rete, sorgenti di telemetria
│   │   │   ├── SHCustomProtocol.h   Logica del cruscotto, menu, notifiche, controllo LED
│   │   │   ├── LapAnalysis.h        Delta live, settori, mappa, forze G, accelerazioni, sessione
│   │   │   ├── dashboard/themes/    I 15 temi (*.inc)
│   │   │   ├── dashboard/*.inc      Widget condivisi, riepilogo e notifiche
│   │   │   ├── board/               Configurazione display e touch della scheda
│   │   │   ├── ACUdpTelemetry.h     Telemetria Assetto Corsa
│   │   │   └── sparco_*.h           Immagini della schermata di attesa
│   │   ├── lib/                     GT7 UDP, metriche derivate, librerie SimHub
│   │   ├── simhub/                  Formula Custom Protocol per SimHub
│   │   ├── docs/                    Protocollo di telemetria, sviluppo temi, EV
│   │   └── tests/                   Test del protocollo SimHub e dell'analisi del giro
│   └── led-strip/                   Striscia LED WS2812B (PlatformIO, ESP32-S2)
│       ├── platformio.ini
│       └── src/main.cpp             Temi LED, pagina web, API, discovery
├── docs/
│   ├── schermate/                   Tutte le schermate 320×240 (generate)
│   ├── SIMHUB.md                    Guida SimHub USB
│   └── img/
├── tools/
│   ├── screenshots/                 Simulatore su PC che genera gli screenshot
│   ├── images/image_to_header.py    Converte un'immagine in header RGB565 per lo schermo
│   └── serial_log.py                Riavvia la scheda e salva il log seriale
└── .github/workflows/build.yml      Compilazione automatica e test
```

## Rigenerare gli screenshot

Gli screenshot si ottengono compilando sul PC il vero codice di disegno del firmware (`SHCustomProtocol.h` e i temi) insieme a LovyanGFX 1.1.12. Il display è simulato con una sprite in memoria, che poi viene salvata come PNG 320×240.

Per mappa, delta, settori e forze G, [`render.cpp`](tools/screenshots/render.cpp) fa guidare alcuni giri a un pilota simulato su un circuito di 3,3 km ([`track_sim.h`](tools/screenshots/track_sim.h)). Il pilota ha aderenza, frenata e motore realistici e invia i dati a 60 Hz come GT7, al vero motore di analisi del firmware. Su Linux o WSL:

```bash
tools/screenshots/build.sh                 # tutte le schermate in docs/schermate/
python3 tools/screenshots/mosaic.py        # immagine di apertura del README
node tools/screenshots/led-web.mjs         # pagina web LED (richiede Playwright)
```

## Prossimi passi

Idee già valutate, non ancora fatte:

- **Aggiornamento del firmware via Wi-Fi (OTA)**: richiede una nuova tabella delle partizioni, da caricare un'ultima volta via USB.
- **Striscia LED**: modalità delta (verde in anticipo, rosso in ritardo), avviso riserva, lampeggio sulla marcia consigliata.
- **Pagina web dello schermo** per cambiare tema dal telefono.
- **Mappe salvate per circuito**, per non doverle ridisegnare a ogni sessione.

## Crediti e licenze

- Cruscotto basato su **[caa1211/esp32-gt7-dashboard](https://github.com/caa1211/esp32-gt7-dashboard)** (temi 0–6, protocollo SimHub, rete) e sull'ecosistema **ESP-SimHub**.
- Decodifica GT7 con **[MacManley/gt7-udp](https://github.com/MacManley/gt7-udp)** (MIT), copia condivisa dai due firmware in `firmware/dashboard/lib/GT7Udp`.
- Grafica con **[LovyanGFX](https://github.com/lovyan03/LovyanGFX)**, LED con **[FastLED](https://github.com/FastLED/FastLED)**, Wi-Fi con **[WiFiManager](https://github.com/tzapu/WiFiManager)**.
- *Gran Turismo* è un marchio di Sony Interactive Entertainment. *Ferrari*, *BMW M*, *Sparco* e *Assetto Corsa* appartengono ai rispettivi proprietari: qui sono usati solo come ispirazione grafica per un progetto personale, senza alcuna affiliazione.
