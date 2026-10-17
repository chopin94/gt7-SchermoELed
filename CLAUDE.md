# Note per Claude

## Come l'utente carica i firmware (Windows, dal PC)

Prima aggiorna il repository con `git pull`, poi carica via USB con PlatformIO
lanciato da Python. Quando gli dai istruzioni per caricare un firmware usa
questi comandi, non `pio`.

Striscia LED (ESP32-S2 mini, porta COM8), dalla cartella `firmware/led-strip`
(oppure aggiungendo `-d firmware/led-strip` dalla cartella principale):

```
python -m platformio run -t upload --upload-port COM8
```

Schermo (ESP32-2432S024C, porta COM4), dalla cartella `firmware/dashboard`
(oppure aggiungendo `-d firmware/dashboard`):

```
python -m platformio run -e esp32-2432s024c -t upload --upload-port COM4
```

Il codice nuovo arriva con `git pull` solo dopo essere stato unito in `main`:
se il lavoro è su un altro branch, ricordalo all'utente.

## Compilare in una sessione cloud

PlatformIO qui non raggiunge il suo registro e non scarica la toolchain: la
compilazione dei firmware si verifica con la CI di GitHub
(`.github/workflows/build.yml`) dopo il push.
