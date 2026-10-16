	// ===== Telecomando della striscia LED (menu IMPOSTAZIONI LED) =====
	// Tutte le impostazioni della pagina web della striscia, divise in
	// sottomenu. Tema in gara, animazione di riposo e avvisi si scelgono
	// guardando una demo animata: la striscia disegnata intorno a una TV con lo
	// stesso codice degli effetti della striscia (firmware/led-strip/src/LedEffects.h)
	// e una guida simulata, come l'anteprima dei temi dello schermo.

	// Impostazioni della versione 2 della striscia (prima non esistevano).
	bool ledHasV2 = false;
	int ledAlerts = LedFx::ALERTS_DEFAULT;
	int ledRight = 38, ledTop = 67, ledLeft = 38;
	bool ledStartLeft = false;
	int ledDeltaScale = 10; // decimi di secondo
	int ledIdleOffMin = 0;  // 0 = mai
	String ledStatusText = "";
	bool ledResetArmed = false;

	// Sottomenu: quale tema, animazione o avviso mostra la demo.
	int ledPreview = 0;
	String ledPreviewColorHex = "ffff00";
	uint32_t ledPressStartMs = 0, ledLastRepeatMs = 0;
	bool ledRepeatFired = false;

	static constexpr int LED_ALERT_COUNT = 8;
	static constexpr unsigned long LED_SETTINGS_TIMEOUT_MS = 60000UL;

	struct LedControl
	{
		int id, x, y, w, h;
	};

	struct LedDemo
	{
		LedFx::Renderer fx;
		LedFx::Rgb frame[LedFx::MAX_LEDS];
		uint16_t shown[LedFx::MAX_LEDS];
		uint8_t edge[LedFx::MAX_LEDS]; // 0 destra, 1 alto, 2 sinistra
		int16_t from[LedFx::MAX_LEDS], to[LedFx::MAX_LEDS];
		uint32_t lastFrameMs = 0, lastEventMs = 0;
		uint8_t eventStep = 0;
	};
	LedDemo *ledDemo = nullptr;

	// La TV della demo e la striscia appena fuori dai suoi bordi.
	static constexpr int LED_TV_X = 44, LED_TV_Y = 56, LED_TV_W = 233, LED_TV_H = 119;
	static constexpr int LED_BAND = 4; // spessore di ogni fascia di luce

	static bool isLedScreen(SettingsScreen s)
	{
		return s == SettingsScreen::LedSettings || s == SettingsScreen::LedThemePicker ||
			s == SettingsScreen::LedIdlePicker || s == SettingsScreen::LedAlerts ||
			s == SettingsScreen::LedLight || s == SettingsScreen::LedLayout ||
			s == SettingsScreen::LedAdvanced;
	}

	static bool isLedPicker(SettingsScreen s)
	{
		return s == SettingsScreen::LedThemePicker || s == SettingsScreen::LedIdlePicker ||
			s == SettingsScreen::LedAlerts;
	}

	unsigned long settingsTimeoutMs() const
	{
		return isLedScreen(settingsScreen) ? LED_SETTINGS_TIMEOUT_MS : SETTINGS_TIMEOUT_MS;
	}

	static const char *ledAlertName(int i)
	{
		static const char *const names[LED_ALERT_COUNT] = {"LIMITATORE", "GIRO MIGLIORE", "SETTORI",
			"PUNTI DI FRENATA", "RISERVA", "BLOCCAGGI", "ULTIMO GIRO E ARRIVO", "MARCIA CONSIGLIATA"};
		return i >= 0 && i < LED_ALERT_COUNT ? names[i] : "";
	}

	// Le stesse spiegazioni della pagina web della striscia.
	static const char *ledPreviewDescription(SettingsScreen s, int i)
	{
		static const char *const themes[] = {
			"Si riempie lungo la striscia: verde, giallo, rosso",
			"Dal centro della striscia verso i due capi",
			"Tutta blu, solo sopra il 70% dei giri",
			"Tutta la striscia da azzurro a rosso",
			"Dai due capi in basso fino al centro in alto",
			"Come Default, dall'altro capo",
			"Lati verdi se sei in anticipo sul giro migliore, rossi se in ritardo; giri in alto",
			"Gas sul lato destro, freno sul sinistro, giri in alto",
			"Scie di luce che escono dal centro, piu' veloci con la velocita'",
			"La luce va dove la forza spinge il corpo: a sinistra in curva a destra, in alto in frenata",
			"Ogni angolo con il colore della temperatura della sua gomma",
			"Il colore del cielo all'ora del giorno della gara",
			"Luce fissa tenue del colore di riposo: si muovono solo gli avvisi"};
		static const char *const idles[] = {
			"La striscia resta spenta a gioco fermo",
			"Tutta la striscia del colore scelto qui sotto",
			"Il colore scelto si accende e si spegne piano",
			"Tutti i colori che scorrono lungo la striscia",
			"Due comete del colore scelto salgono dai lati e si incontrano in alto",
			"Fiamme che salgono dai due lati verso il centro in alto",
			"Onde lente di verde, azzurro e viola",
			"Luce bianca neutra dietro la TV, per guardare film e serie"};
		static const char *const alerts[LED_ALERT_COUNT] = {
			"Tutta la striscia lampeggia rossa al limitatore",
			"Tre impulsi viola a ogni nuovo giro migliore",
			"Il lato alto prende il colore del settore appena chiuso",
			"Da 150 m i lati si accendono dall'alto: bianco, arancione, rosso",
			"Impulsi arancioni quando il carburante sta finendo",
			"Rosso (bloccaggio) o arancione (pattinamento) sul lato della ruota",
			"Lampi bianchi all'ultimo giro, bandiera a scacchi all'arrivo",
			"Angoli azzurri se il gioco consiglia una marcia piu' bassa"};
		if (s == SettingsScreen::LedThemePicker && i >= 0 && i < 13) return themes[i];
		if (s == SettingsScreen::LedIdlePicker && i >= 0 && i < 8) return idles[i];
		if (s == SettingsScreen::LedAlerts && i >= 0 && i < LED_ALERT_COUNT) return alerts[i];
		return "";
	}

	int ledPreviewCount() const
	{
		if (settingsScreen == SettingsScreen::LedThemePicker) return ledThemeCount;
		if (settingsScreen == SettingsScreen::LedIdlePicker) return ledIdleModeCount;
		return LED_ALERT_COUNT;
	}

	// Animazioni che usano il colore di riposo: il selettore del colore compare solo per loro.
	bool ledPreviewUsesColor() const
	{
		return settingsScreen == SettingsScreen::LedIdlePicker &&
			(ledPreview == LedFx::IDLE_SOLID || ledPreview == LedFx::IDLE_BREATHING ||
				ledPreview == LedFx::IDLE_WAVE);
	}

	static LedFx::Rgb ledColorFromHex(const String &hex)
	{
		const long v = strtol(hex.c_str(), nullptr, 16);
		return LedFx::Rgb((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
	}

	static String ledHueHex(int x)
	{
		const float hue = map(constrain(x, 20, 300) - 20, 0, 280, 0, 359);
		const float c = 1.0f, hx = c * (1 - fabsf(fmodf(hue / 60.0f, 2) - 1));
		float r = 0, g = 0, b = 0;
		if (hue < 60) { r = c; g = hx; } else if (hue < 120) { r = hx; g = c; }
		else if (hue < 180) { g = c; b = hx; } else if (hue < 240) { g = hx; b = c; }
		else if (hue < 300) { r = hx; b = c; } else { r = c; b = hx; }
		char hex[7];
		snprintf(hex, sizeof(hex), "%02x%02x%02x", (int)(r * 255), (int)(g * 255), (int)(b * 255));
		return String(hex);
	}

	LedFx::Layout ledLayout() const
	{
		if (!ledHasV2) return LedFx::Layout::forCount(ledActiveCount);
		LedFx::Layout l;
		l.right = ledRight;
		l.top = ledTop;
		l.left = ledLeft;
		l.startLeft = ledStartLeft;
		return l;
	}

	// ---- Comandi alla striscia ----

	bool ledGet(const String &path)
	{
		if (!ledStripFound || ledStripIP == "") return false;
		HTTPClient http;
		http.begin("http://" + ledStripIP + path);
		http.setTimeout(1000);
		const int code = http.GET();
		http.end();
		return code == 200;
	}

	// ---- Tabelle dei comandi: disegno, stato premuto e tocco dalle stesse coordinate ----

	int ledControls(const LedControl *&out) const
	{
		static const LedControl hub[] = {
			{1, 10, 30, 146, 50}, {2, 164, 30, 146, 50},
			{11, 10, 86, 146, 50}, {4, 164, 86, 146, 50},
			{5, 10, 142, 146, 50}, {6, 164, 142, 146, 50},
			{0, 10, 198, 146, 34}, {10, 164, 198, 146, 34}};
		static const LedControl missing[] = {{10, 20, 204, 280, 28}};
		static const LedControl picker[] = {
			{20, 0, 0, 106, 40}, {21, 214, 0, 106, 40}, {22, 20, 180, 280, 16},
			{23, 0, 198, 160, 42}, {24, 160, 198, 160, 42}};
		static const LedControl light[] = {
			{3, 20, 36, 280, 24}, {7, 20, 82, 280, 24},
			{30, 100, 116, 50, 32}, {31, 250, 116, 50, 32},
			{8, 20, 158, 135, 34}, {9, 165, 158, 135, 34}, {10, 20, 204, 280, 28}};
		static const LedControl layout[] = {
			{32, 100, 26, 50, 30}, {33, 250, 26, 50, 30},
			{34, 100, 60, 50, 30}, {35, 250, 60, 50, 30},
			{36, 100, 94, 50, 30}, {37, 250, 94, 50, 30},
			{38, 20, 156, 280, 32}, {39, 20, 204, 135, 28}, {10, 165, 204, 135, 28}};
		static const LedControl layoutV1[] = {{32, 100, 60, 50, 30}, {33, 250, 60, 50, 30}, {10, 20, 204, 280, 28}};
		static const LedControl advanced[] = {
			{40, 100, 30, 50, 32}, {41, 250, 30, 50, 32},
			{42, 100, 90, 50, 32}, {43, 250, 90, 50, 32},
			{44, 20, 148, 135, 32}, {45, 165, 148, 135, 32}, {10, 20, 204, 280, 28}};
		static const LedControl advancedV1[] = {{44, 20, 148, 135, 32}, {10, 20, 204, 280, 28}};
#define LED_TABLE(t) (out = t, static_cast<int>(sizeof(t) / sizeof(t[0])))
		if (!ledStripFound) return LED_TABLE(missing);
		switch (settingsScreen)
		{
		case SettingsScreen::LedThemePicker:
		case SettingsScreen::LedIdlePicker:
		case SettingsScreen::LedAlerts: return LED_TABLE(picker);
		case SettingsScreen::LedLight: return LED_TABLE(light);
		case SettingsScreen::LedLayout: return ledHasV2 ? LED_TABLE(layout) : LED_TABLE(layoutV1);
		case SettingsScreen::LedAdvanced: return ledHasV2 ? LED_TABLE(advanced) : LED_TABLE(advancedV1);
		default: return LED_TABLE(hub);
		}
#undef LED_TABLE
	}

	bool ledControlEnabled(int id) const
	{
		if (id == 22) return ledPreviewUsesColor();
		if (id == 4) return ledHasV2; // gli avvisi esistono dalla versione 2
		return true;
	}

	int ledButtonAtTouch() const
	{
		const LedControl *c;
		const int n = ledControls(c);
		for (int i = 0; i < n; ++i)
			if (ledControlEnabled(c[i].id) && touchInside(c[i].x, c[i].y, c[i].w, c[i].h)) return c[i].id;
		return -1;
	}

	static bool isLedSlider(int id) { return id == 3 || id == 7 || id == 22; }
	static bool isLedStepper(int id) { return id >= 30 && id <= 43 && id != 38 && id != 39; }

	// ---- Disegno ----

	void drawLedTitle(const char *title)
	{
		tft.setTextDatum(MC_DATUM);
		tft.setTextColor(TFT_WHITE, TFT_BLACK);
		tft.drawString(title, X_CENTER, 12, 2);
	}

	void drawLedTile(const LedControl &c, const char *title, const String &value, bool pressed, bool enabled = true)
	{
		const uint16_t fill = pressed ? tft.color565(38, 126, 150) : tft.color565(42, 46, 52);
		tft.fillRoundRect(c.x, c.y, c.w, c.h, 7, fill);
		tft.drawRoundRect(c.x, c.y, c.w, c.h, 7, enabled ? TFT_LIGHTGREY : TFT_DARKGREY);
		tft.setTextDatum(MC_DATUM);
		tft.setTextColor(enabled ? TFT_WHITE : TFT_DARKGREY, fill);
		tft.drawString(title, c.x + c.w / 2, c.y + 15, 2);
		tft.setTextColor(enabled ? tft.color565(110, 200, 235) : TFT_DARKGREY, fill);
		tft.drawString(value, c.x + c.w / 2, c.y + 35, 2);
	}

	void drawLedSlider(int id)
	{
		const LedControl *c;
		const int n = ledControls(c);
		for (int i = 0; i < n; ++i)
		{
			if (c[i].id != id) continue;
			const int x = c[i].x, y = c[i].y, w = c[i].w, h = c[i].h;
			if (id == 22)
			{
				for (int k = 0; k < w - 2; ++k)
				{
					const LedFx::Rgb rgb = ledColorFromHex(ledHueHex(20 + k * 280 / (w - 2)));
					tft.drawFastVLine(x + 1 + k, y + 1, h - 2, tft.color565(rgb.r, rgb.g, rgb.b));
				}
				tft.drawRect(x, y, w, h, TFT_WHITE);
				return;
			}
			const int value = id == 3 ? ledBrightness : ledGurgle;
			const int fillW = map(value, 0, 255, 0, w - 2);
			const uint16_t bar = id == 3 ? TFT_LIGHTGREY : tft.color565(255, 140, 40);
			tft.drawRect(x, y, w, h, TFT_WHITE);
			tft.fillRect(x + 1, y + 1, fillW, h - 2, bar);
			tft.fillRect(x + 1 + fillW, y + 1, w - 2 - fillW, h - 2, TFT_BLACK);
			tft.setTextDatum(MC_DATUM);
			const String label = id == 3 ? String(value * 100 / 255) + "%" : (value ? String(value) : String("SPENTO"));
			tft.setTextColor(fillW > w / 2 ? TFT_BLACK : TFT_WHITE);
			tft.drawString(label, x + w / 2, y + h / 2, 2);
			return;
		}
	}

	String ledStepperValue(int id) const
	{
		switch (id)
		{
		case 30: case 31: return String(ledMaxCurrentMA) + " mA";
		case 32: case 33: return String(ledHasV2 ? ledRight : ledActiveCount);
		case 34: case 35: return String(ledTop);
		case 36: case 37: return String(ledLeft);
		case 40: case 41: return String(ledDeltaScale / 10) + "." + String(ledDeltaScale % 10) + " s";
		case 42: case 43: return ledIdleOffMin ? String(ledIdleOffMin) + " min" : String("MAI");
		default: return "";
		}
	}

	// Il valore di una riga "- valore +": fra i due pulsanti della riga.
	void drawLedStepperValue(int minusId)
	{
		const LedControl *c;
		const int n = ledControls(c);
		for (int i = 0; i < n; ++i)
		{
			if (c[i].id != minusId) continue;
			const int x = c[i].x + c[i].w + 6, w = 250 - 6 - x;
			const uint16_t fill = tft.color565(18, 74, 104);
			tft.fillRoundRect(x, c[i].y, w, c[i].h, 7, fill);
			tft.setTextDatum(MC_DATUM);
			tft.setTextColor(TFT_WHITE, fill);
			tft.drawString(ledStepperValue(minusId), x + w / 2, c[i].y + c[i].h / 2, 2);
			return;
		}
	}

	void drawLedRowLabel(const char *label, int y, int h)
	{
		tft.setTextDatum(ML_DATUM);
		tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
		tft.drawString(label, 20, y + h / 2, 2);
	}

	void drawLedHint(const char *text, int y)
	{
		tft.setTextDatum(MC_DATUM);
		tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
		tft.drawString(text, X_CENTER, y, 1);
	}

	void drawLedLayoutTotal()
	{
		tft.fillRect(0, 128, SCREEN_WIDTH, 24, TFT_BLACK);
		tft.setTextDatum(MC_DATUM);
		const int total = ledRight + ledTop + ledLeft;
		tft.setTextColor(total > 200 ? TFT_RED : TFT_WHITE, TFT_BLACK);
		tft.drawString("Totale " + String(total) + " LED (massimo 200)", X_CENTER, 140, 2);
	}

	void drawLedAdvancedStatus()
	{
		tft.fillRect(0, 184, SCREEN_WIDTH, 18, TFT_BLACK);
		tft.setTextDatum(MC_DATUM);
		tft.setTextColor(ledResetArmed ? TFT_ORANGE : TFT_GREEN, TFT_BLACK);
		tft.drawString(ledStatusText, X_CENTER, 192, 2);
	}

	void drawLedControl(const LedControl &c, bool pressed)
	{
		const SettingsScreen s = settingsScreen;
		const int id = c.id;
		if (s == SettingsScreen::LedSettings && ledStripFound)
		{
			switch (id)
			{
			case 1: drawLedTile(c, "TEMA IN GARA", getThemeName(ledTheme), pressed); return;
			case 2: drawLedTile(c, "GIOCO FERMO", getIdleName(ledIdleMode), pressed); return;
			case 4:
			{
				int on = 0;
				for (int i = 0; i < LED_ALERT_COUNT; ++i) on += (ledAlerts >> i) & 1;
				drawLedTile(c, "AVVISI", ledHasV2 ? String(on) + " di 8 attivi" : String("non disponibili"), pressed, ledHasV2);
				return;
			}
			case 5: drawLedTile(c, "MONTAGGIO", String(ledHasV2 ? ledRight + ledTop + ledLeft : ledActiveCount) + " LED", pressed); return;
			case 6: drawLedTile(c, "AVANZATE", ledHasV2 ? "delta, auto-off" : "riavvio", pressed); return;
			case 0: drawSettingsButton(c.x, c.y, c.w, c.h, ledBrightness > 0 ? "LED ACCESI" : "LED SPENTI", pressed, ledBrightness > 0); return;
			}
		}
		if (s == SettingsScreen::LedSettings && ledStripFound && id == 11)
		{
			drawLedTile(c, "LUCE", String(ledBrightness * 100 / 255) + "%  " + String(ledMaxCurrentMA) + " mA", pressed);
			return;
		}
		if (isLedSlider(id)) { drawLedSlider(id); return; }
		if (isLedPicker(s))
		{
			const bool alerts = s == SettingsScreen::LedAlerts;
			const bool on = alerts && ((ledAlerts >> ledPreview) & 1);
			if (id == 23) drawSettingsButton(c.x, c.y, c.w, c.h, alerts ? "INDIETRO" : "ANNULLA", pressed);
			else if (id == 24) drawSettingsButton(c.x, c.y, c.w, c.h, alerts ? (on ? "ATTIVO" : "DISATTIVO") : "APPLICA", pressed, !alerts || on);
			return; // frecce: nessuno stato premuto, come nella scelta del tema
		}
		if (isLedStepper(id))
		{
			drawSettingsButton(c.x, c.y, c.w, c.h, (id % 2) == 0 ? "-" : "+", pressed);
			if ((id % 2) == 0) drawLedStepperValue(id);
			return;
		}
		switch (id)
		{
		case 8: drawSettingsButton(c.x, c.y, c.w, c.h, "ACCESA", pressed, ledBrightness > 0); return;
		case 9: drawSettingsButton(c.x, c.y, c.w, c.h, "SPENTA", pressed, ledBrightness == 0); return;
		case 10: drawSettingsButton(c.x, c.y, c.w, c.h, "INDIETRO", pressed); return;
		case 38: drawSettingsButton(c.x, c.y, c.w, c.h, ledStartLeft ? "PARTE IN BASSO A SINISTRA" : "PARTE IN BASSO A DESTRA", pressed); return;
		case 39: drawSettingsButton(c.x, c.y, c.w, c.h, "PROVA 15 s", pressed); return;
		case 44: drawSettingsButton(c.x, c.y, c.w, c.h, "RIAVVIA", pressed); return;
		case 45: drawSettingsButton(c.x, c.y, c.w, c.h, ledResetArmed ? "CONFERMA" : "AZZERA GIRI", pressed, false, true); return;
		}
	}

	void drawLedPickerHeader()
	{
		const uint16_t overlay = tft.color565(31, 34, 38);
		const uint16_t divider = tft.color565(82, 86, 92);
		tft.fillRect(0, 0, SCREEN_WIDTH, 40, overlay);
		tft.drawFastHLine(0, 39, SCREEN_WIDTH, divider);
		for (int offset = -1; offset <= 1; ++offset)
		{
			tft.drawLine(42 + offset, 10, 32 + offset, 20, TFT_WHITE);
			tft.drawLine(32 + offset, 20, 42 + offset, 30, TFT_WHITE);
			tft.drawLine(SCREEN_WIDTH - 42 + offset, 10, SCREEN_WIDTH - 32 + offset, 20, TFT_WHITE);
			tft.drawLine(SCREEN_WIDTH - 32 + offset, 20, SCREEN_WIDTH - 42 + offset, 30, TFT_WHITE);
		}
		const SettingsScreen s = settingsScreen;
		const String name = s == SettingsScreen::LedThemePicker ? getThemeName(ledPreview)
			: s == SettingsScreen::LedIdlePicker ? getIdleName(ledPreview) : String(ledAlertName(ledPreview));
		tft.setTextDatum(MC_DATUM);
		tft.setTextColor(TFT_WHITE, overlay);
		tft.drawString(name, X_CENTER, 17, 2);

		// Un trattino per voce: verde quella in uso (per gli avvisi: quelli attivi).
		const int count = ledPreviewCount();
		const int total = count * 6 + (count - 1) * 3;
		const int x0 = (SCREEN_WIDTH - total) / 2;
		const uint16_t applied = tft.color565(92, 176, 112);
		for (int i = 0; i < count; ++i)
		{
			const bool inUse = s == SettingsScreen::LedThemePicker ? i == ledTheme
				: s == SettingsScreen::LedIdlePicker ? i == ledIdleMode : ((ledAlerts >> i) & 1) != 0;
			const uint16_t color = i == ledPreview ? TFT_WHITE : inUse ? applied : divider;
			tft.fillRect(x0 + i * 9, 31, 6, 3, color);
		}
	}

	// Testo a capo su più righe, centrato nella TV.
	void drawLedTvText(const char *text)
	{
		static constexpr int MAX_LINES = 4;
		String lines[MAX_LINES];
		int count = 0;
		String line = "", word = "";
		const int maxW = LED_TV_W - 20;
		tft.setTextFont(2);
		for (const char *p = text;; ++p)
		{
			if (*p == ' ' || *p == 0)
			{
				const String test = line.length() ? line + " " + word : word;
				if (tft.textWidth(test) > maxW && line.length() && count < MAX_LINES)
				{
					lines[count++] = line;
					line = word;
				}
				else line = test;
				word = "";
				if (*p == 0) break;
			}
			else word += *p;
		}
		if (line.length() && count < MAX_LINES) lines[count++] = line;
		const uint16_t screen = tft.color565(6, 7, 10);
		tft.setTextDatum(MC_DATUM);
		tft.setTextColor(tft.color565(170, 175, 185), screen);
		const int y0 = LED_TV_Y + LED_TV_H / 2 - (count - 1) * 9;
		for (int i = 0; i < count; ++i) tft.drawString(lines[i], X_CENTER, y0 + i * 18, 2);
	}

	void drawLedPicker()
	{
		drawLedPickerHeader();
		// La parete dietro la TV e la TV spenta con la spiegazione sullo schermo.
		tft.fillRect(0, 40, SCREEN_WIDTH, 158, TFT_BLACK);
		const uint16_t bezel = tft.color565(30, 32, 38);
		const uint16_t screen = tft.color565(6, 7, 10);
		tft.fillRect(LED_TV_X, LED_TV_Y, LED_TV_W, LED_TV_H, bezel);
		tft.fillRect(LED_TV_X + 3, LED_TV_Y + 3, LED_TV_W - 6, LED_TV_H - 6, screen);
		drawLedTvText(ledPreviewDescription(settingsScreen, ledPreview));
		if (settingsScreen == SettingsScreen::LedAlerts)
		{
			const bool on = (ledAlerts >> ledPreview) & 1;
			tft.setTextColor(on ? TFT_GREEN : TFT_DARKGREY, screen);
			tft.drawString(on ? "ATTIVO" : "DISATTIVATO", X_CENTER, LED_TV_Y + LED_TV_H - 14, 1);
		}
		else if (settingsScreen == SettingsScreen::LedIdlePicker && ledPreviewUsesColor())
		{
			drawLedSlider(22);
		}
		else
		{
			drawLedHint("DEMO", 188);
		}
		const LedControl *c;
		const int n = ledControls(c);
		for (int i = 0; i < n; ++i)
			if (c[i].id == 23 || c[i].id == 24) drawLedControl(c[i], false);
		if (ledDemo)
			for (int i = 0; i < LedFx::MAX_LEDS; ++i) ledDemo->shown[i] = 0x0001; // ridisegna tutti i LED
	}

	void drawLedScreen(int pressedButton)
	{
		const SettingsScreen s = settingsScreen;
		if (!ledStripFound)
		{
			drawLedTitle("IMPOSTAZIONI LED");
			tft.setTextDatum(MC_DATUM);
			tft.setTextColor(TFT_RED, TFT_BLACK);
			tft.drawString("Striscia LED non trovata", X_CENTER, 100, 2);
			tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
			tft.drawString("Deve essere accesa e sulla stessa rete Wi-Fi", X_CENTER, 128, 1);
		}
		else if (isLedPicker(s))
		{
			drawLedPicker();
			return;
		}
		else if (s == SettingsScreen::LedSettings)
		{
			drawLedTitle("IMPOSTAZIONI LED");
			tft.setTextDatum(MR_DATUM);
			tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
			tft.drawString(ledStripIP, SCREEN_WIDTH - 8, 12, 1);
		}
		else if (s == SettingsScreen::LedLight)
		{
			drawLedTitle("LUCE");
			drawLedHint("LUMINOSITA'", 28);
			drawLedHint("GORGOGLIO: TREMOLIO ALLA PUNTA DELLE BARRE", 74);
			drawLedRowLabel("CORRENTE", 116, 32);
		}
		else if (s == SettingsScreen::LedLayout)
		{
			drawLedTitle("MONTAGGIO DIETRO LA TV");
			if (ledHasV2)
			{
				drawLedRowLabel("DESTRO", 26, 30);
				drawLedRowLabel("ALTO", 60, 30);
				drawLedRowLabel("SINISTRO", 94, 30);
				drawLedLayoutTotal();
				drawLedHint("Prova: destra rosso, alto verde, sinistra blu", 196);
			}
			else
			{
				drawLedRowLabel("LED", 60, 30);
				drawLedHint("Aggiorna la striscia per i LED di ogni lato", 120);
			}
		}
		else if (s == SettingsScreen::LedAdvanced)
		{
			drawLedTitle("AVANZATE");
			if (ledHasV2)
			{
				drawLedRowLabel("DELTA", 30, 32);
				drawLedHint("Ritardo che accende tutti i lati nel tema DELTA", 72);
				drawLedRowLabel("SPEGNI", 90, 32);
				drawLedHint("Dopo quanti minuti senza GT7 i LED si spengono", 132);
			}
			else
			{
				drawLedHint("Aggiorna la striscia per scala del delta,", 60);
				drawLedHint("avvisi e spegnimento automatico", 74);
			}
			drawLedAdvancedStatus();
		}
		const LedControl *c;
		const int n = ledControls(c);
		for (int i = 0; i < n; ++i) drawLedControl(c[i], pressedButton == c[i].id);
	}

	void redrawLedButton(int button, bool pressed)
	{
		const LedControl *c;
		const int n = ledControls(c);
		for (int i = 0; i < n; ++i)
			if (c[i].id == button && !isLedSlider(button) &&
				!(isLedPicker(settingsScreen) && (button == 20 || button == 21)))
				drawLedControl(c[i], pressed);
	}

	// ---- Demo animata ----

	void releaseLedDemo()
	{
		delete ledDemo;
		ledDemo = nullptr;
	}

	void createLedDemo()
	{
		if (!ledDemo) ledDemo = new (std::nothrow) LedDemo();
		if (!ledDemo) return;
		ledDemo->fx.setLayout(ledLayout());
		const LedFx::Layout &l = ledDemo->fx.layout();
		const int n = ledDemo->fx.count();
		const int left = LED_TV_X, right = LED_TV_X + LED_TV_W, top = LED_TV_Y, bottom = LED_TV_Y + LED_TV_H;
		int first = l.startLeft ? l.left : l.right, last = l.startLeft ? l.right : l.left, across = l.top;
		if (first + last == 0) across = n; // striscia dritta: tutta sul lato alto
		for (int i = 0; i < n; ++i)
		{
			LedDemo &d = *ledDemo;
			if (i < first)
			{
				d.edge[i] = l.startLeft ? 2 : 0;
				d.from[i] = bottom - (i + 1) * LED_TV_H / first;
				d.to[i] = bottom - i * LED_TV_H / first;
			}
			else if (i < first + across)
			{
				const int j = i - first;
				d.edge[i] = 1;
				if (l.startLeft) { d.from[i] = left + j * LED_TV_W / across; d.to[i] = left + (j + 1) * LED_TV_W / across; }
				else { d.from[i] = right - (j + 1) * LED_TV_W / across; d.to[i] = right - j * LED_TV_W / across; }
			}
			else
			{
				const int j = i - first - across;
				d.edge[i] = l.startLeft ? 0 : 2;
				d.from[i] = top + j * LED_TV_H / last;
				d.to[i] = top + (j + 1) * LED_TV_H / last;
			}
			d.shown[i] = 0x0001;
		}
		ledDemo->lastFrameMs = 0;
	}

	// Un LED: la striscia appena fuori dal bordo della TV e tre fasce di luce
	// sempre più tenui sulla parete. Spento resta grigio scuro per vedere dov'è.
	void drawLedDemoLed(int i, LedFx::Rgb c)
	{
		static const uint8_t bands[4] = {255, 150, 80, 34};
		const LedDemo &d = *ledDemo;
		const int len = d.to[i] - d.from[i] > 0 ? d.to[i] - d.from[i] : 1;
		const bool off = !c.r && !c.g && !c.b;
		for (int b = 0; b < 4; ++b)
		{
			uint16_t color = off ? (b == 0 ? tft.color565(34, 34, 40) : TFT_BLACK)
				: tft.color565(LedFx::scale8(c.r, bands[b]), LedFx::scale8(c.g, bands[b]), LedFx::scale8(c.b, bands[b]));
			const int k = b * LED_BAND;
			if (d.edge[i] == 0) tft.fillRect(LED_TV_X + LED_TV_W + k, d.from[i], LED_BAND, len, color);
			else if (d.edge[i] == 2) tft.fillRect(LED_TV_X - LED_BAND - k, d.from[i], LED_BAND, len, color);
			else tft.fillRect(d.from[i], LED_TV_Y - LED_BAND - k, len, LED_BAND, color);
		}
	}

	// Una guida simulata in 8 secondi: tre marce in pieno, frenata, curva.
	LedFx::Input ledDemoInput(uint32_t now) const
	{
		LedFx::Input in;
		in.racing = settingsScreen != SettingsScreen::LedIdlePicker;
		in.maxRpm = 8000;
		in.alertRpm = 7000;
		const uint32_t t = now % 8000;
		if (t < 4800)
		{
			const float k = (t % 1600) / 1600.0f;
			in.gear = 3 + static_cast<int>(t / 1600);
			in.rpm = 4200 + k * 3800;
			in.throttle = 1.0f;
			in.speedKmh = 120 + t * 0.03f;
			in.longitudinalG = 0.5f - k * 0.2f;
			in.lateralG = 0.15f * sinf(now * 0.004f);
			in.limiter = k > 0.94f;
		}
		else if (t < 6400)
		{
			const float k = (t - 4800) / 1600.0f;
			in.gear = 5 - static_cast<int>(k * 2.99f);
			in.rpm = 7400 - k * 2600;
			in.brake = 0.95f - k * 0.5f;
			in.speedKmh = 264 - k * 160;
			in.longitudinalG = -1.5f + k * 0.6f;
			in.lateralG = 0.4f * k;
			in.suggestedGear = 2;
			if (k < 0.25f) in.wheel[0] = in.wheel[1] = LedFx::WHEEL_LOCKING;
		}
		else
		{
			const float k = (t - 6400) / 1600.0f;
			in.gear = 3;
			in.rpm = 4400 + k * 900;
			in.throttle = 0.3f + 0.7f * k;
			in.speedKmh = 104 + k * 16;
			in.lateralG = 1.4f * sinf(k * 3.1416f);
			in.longitudinalG = 0.2f * k;
			if (k > 0.65f) in.wheel[2] = in.wheel[3] = LedFx::WHEEL_SPINNING;
		}
		for (int w = 0; w < 4; ++w) in.wheel[w] = in.wheel[w] ? in.wheel[w] : LedFx::WHEEL_GRIP;
		in.hasDelta = true;
		in.deltaS = 0.6f * sinf(now * 6.2832f / 9000.0f);
		for (int w = 0; w < 4; ++w) in.tyreTemp[w] = 82 + 26 * sinf(now / 2300.0f + w * 1.7f);
		in.timeOfDayH = fmodf(now / 500.0f, 24.0f); // un giorno in 12 secondi
		if (t >= 3800 && t < 4800) { in.hasBrakePoint = true; in.brakePointM = 150.0f * (4800 - t) / 1000.0f; }

		if (settingsScreen == SettingsScreen::LedAlerts)
		{
			// Ogni avviso si accende spesso, così la demo lo mostra subito.
			const uint32_t a = now % 3000;
			switch (1 << ledPreview)
			{
			case LedFx::ALERT_LIMITER: in.limiter = a < 1200; if (in.limiter) in.rpm = in.maxRpm; break;
			case LedFx::ALERT_BRAKING: in.hasBrakePoint = a < 2200; in.brakePointM = 150.0f * (2200 - a) / 2200.0f; break;
			case LedFx::ALERT_GRIP:
				for (int w = 0; w < 4; ++w) in.wheel[w] = LedFx::WHEEL_GRIP;
				if (a < 1000) in.wheel[0] = in.wheel[1] = LedFx::WHEEL_LOCKING;
				else if (a >= 1500 && a < 2500) in.wheel[2] = in.wheel[3] = LedFx::WHEEL_SPINNING;
				break;
			case LedFx::ALERT_GEAR: in.suggestedGear = a < 1800 ? in.gear - 1 : -1; break;
			}
		}
		return in;
	}

	void triggerLedDemoEvents(uint32_t now)
	{
		LedDemo &d = *ledDemo;
		const SettingsScreen s = settingsScreen;
		int alert = -1;
		if (s == SettingsScreen::LedAlerts) alert = 1 << ledPreview;
		else if (s == SettingsScreen::LedThemePicker && ledPreview == LedFx::THEME_ALERTS_ONLY) alert = LedFx::ALERT_SECTORS;
		const uint32_t period = alert == LedFx::ALERT_SECTORS ? 1600 : alert == LedFx::ALERT_FLAGS ? 4000 : 3000;
		if (alert < 0 || (d.lastEventMs && now - d.lastEventMs < period)) return;
		d.lastEventMs = now;
		++d.eventStep;
		switch (alert)
		{
		case LedFx::ALERT_BEST_LAP: d.fx.trigger(LedFx::Event::BestLap, now); break;
		case LedFx::ALERT_SECTORS:
		{
			static const LedFx::Event sectors[3] = {LedFx::Event::SectorPurple, LedFx::Event::SectorGreen, LedFx::Event::SectorYellow};
			d.fx.trigger(sectors[d.eventStep % 3], now);
			break;
		}
		case LedFx::ALERT_FUEL: d.fx.trigger(LedFx::Event::LowFuel, now); break;
		case LedFx::ALERT_FLAGS: d.fx.trigger(d.eventStep % 2 ? LedFx::Event::FinalLap : LedFx::Event::Finish, now); break;
		}
	}

	void updateLedDemo()
	{
		if (!ledDemo || !isLedPicker(settingsScreen) || screenSleeping || !ledStripFound) return;
		const uint32_t now = millis();
		if (now - ledDemo->lastFrameMs < 33) return;
		ledDemo->lastFrameMs = now;
		LedFx::Settings &fx = ledDemo->fx.settings;
		const SettingsScreen s = settingsScreen;
		fx.theme = s == SettingsScreen::LedThemePicker ? ledPreview : ledTheme;
		fx.idleMode = s == SettingsScreen::LedIdlePicker ? ledPreview : ledIdleMode;
		fx.idleColor = ledColorFromHex(s == SettingsScreen::LedIdlePicker ? ledPreviewColorHex : ledIdleColorHex);
		fx.gurgle = ledGurgle;
		fx.alerts = s == SettingsScreen::LedAlerts ? (1 << ledPreview) : (ledHasV2 ? ledAlerts : 0);
		fx.deltaScaleS = ledDeltaScale / 10.0f;
		triggerLedDemoEvents(now);
		ledDemo->fx.render(ledDemoInput(now), now, ledDemo->frame);
		tft.startWrite();
		for (int i = 0; i < ledDemo->fx.count(); ++i)
		{
			const LedFx::Rgb c = ledDemo->frame[i];
			const uint16_t key = tft.color565(c.r, c.g, c.b);
			if (key == ledDemo->shown[i]) continue;
			ledDemo->shown[i] = key;
			drawLedDemoLed(i, c);
		}
		tft.endWrite();
	}

	// ---- Tocchi ----

	void showLedPicker(SettingsScreen screen)
	{
		if (screen == SettingsScreen::LedThemePicker) ledPreview = ledTheme;
		else if (screen == SettingsScreen::LedIdlePicker) ledPreview = ledIdleMode;
		else ledPreview = 0;
		ledPreview = constrain(ledPreview, 0, 31);
		ledPreviewColorHex = ledIdleColorHex;
		createLedDemo();
		showSettingsScreen(screen);
	}

	// Aggiorna lo stato locale dalla posizione X del dito; l'invio alla
	// striscia avviene al rilascio.
	void updateLedSliderFromTouch(int button)
	{
		const int x = constrain(lastSettingsTouchX, 20, 300);
		if (button == 3) ledBrightness = map(x, 20, 300, 0, 255);
		else if (button == 7) ledGurgle = map(x, 20, 300, 0, 255);
		else if (button == 22) ledPreviewColorHex = ledHueHex(x);
	}

	void ledStep(int id)
	{
		const int dir = (id % 2) == 0 ? -1 : 1;
		switch (id)
		{
		case 30: case 31: ledMaxCurrentMA = constrain(ledMaxCurrentMA + dir * 100, 100, 10000); break;
		case 32: case 33:
			if (ledHasV2) ledRight = constrain(ledRight + dir, 0, 200 - ledTop - ledLeft);
			else ledActiveCount = constrain(ledActiveCount + dir, 1, 200);
			break;
		case 34: case 35: ledTop = constrain(ledTop + dir, 0, 200 - ledRight - ledLeft); break;
		case 36: case 37: ledLeft = constrain(ledLeft + dir, 0, 200 - ledRight - ledTop); break;
		case 40: case 41: ledDeltaScale = constrain(ledDeltaScale + dir, 1, 50); break;
		case 42: case 43: ledIdleOffMin = constrain(ledIdleOffMin + dir * 5, 0, 240); break;
		}
		if (ledHasV2)
		{
			if (ledRight + ledTop + ledLeft < 1) ledTop = 1;
			ledActiveCount = ledRight + ledTop + ledLeft;
		}
		drawLedStepperValue(id & ~1);
		if (settingsScreen == SettingsScreen::LedLayout && ledHasV2) drawLedLayoutTotal();
	}

	// Dito tenuto su "-" o "+": il valore continua a cambiare.
	void ledTouchHeld(int button)
	{
		const uint32_t now = millis();
		if (isLedSlider(button))
		{
			updateLedSliderFromTouch(button);
			if (now - lastLedSliderDraw >= 60)
			{
				lastLedSliderDraw = now;
				if (button != 22) drawLedSlider(button);
			}
		}
		else if (isLedStepper(button) && now - ledPressStartMs > 450 && now - ledLastRepeatMs > 70)
		{
			ledLastRepeatMs = now;
			ledRepeatFired = true;
			ledStep(button);
		}
	}

	void ledTouchStarted()
	{
		ledPressStartMs = millis();
		ledRepeatFired = false;
	}

	// Dito rilasciato fuori dal comando premuto: invia comunque l'ultimo valore.
	void ledTouchReleasedOutside(int button)
	{
		if (!ledStripFound) return;
		if (button == 3 || button == 7 || (isLedStepper(button) && ledRepeatFired))
		{
			sendLedSettings();
			if (button == 3 || button == 7) drawLedSlider(button);
		}
		ledRepeatFired = false;
	}

	void activateLedButton(int button)
	{
		const SettingsScreen s = settingsScreen;
		if (button == 10)
		{
			showSettingsScreen(s == SettingsScreen::LedSettings || !ledStripFound ? SettingsScreen::Main : SettingsScreen::LedSettings);
			return;
		}
		if (!ledStripFound) return;
		if (s == SettingsScreen::LedSettings)
		{
			switch (button)
			{
			case 0:
				if (ledBrightness > 0) { lastLedBrightness = ledBrightness; ledBrightness = 0; }
				else ledBrightness = lastLedBrightness > 0 ? lastLedBrightness : 100;
				sendLedSettings();
				redrawLedButton(0, false);
				break;
			case 1: showLedPicker(SettingsScreen::LedThemePicker); break;
			case 2: showLedPicker(SettingsScreen::LedIdlePicker); break;
			case 11: showSettingsScreen(SettingsScreen::LedLight); break;
			case 4: showLedPicker(SettingsScreen::LedAlerts); break;
			case 5: showSettingsScreen(SettingsScreen::LedLayout); break;
			case 6:
				ledStatusText = "";
				ledResetArmed = false;
				showSettingsScreen(SettingsScreen::LedAdvanced);
				break;
			}
			return;
		}
		if (isLedPicker(s))
		{
			const int count = ledPreviewCount();
			if (button == 20 || button == 21)
			{
				ledPreview = (ledPreview + (button == 20 ? count - 1 : 1)) % count;
				if (ledDemo) ledDemo->lastEventMs = 0;
				drawLedPicker();
			}
			else if (button == 22)
			{
				updateLedSliderFromTouch(button);
			}
			else if (button == 23)
			{
				showSettingsScreen(SettingsScreen::LedSettings);
			}
			else if (button == 24 && s == SettingsScreen::LedAlerts)
			{
				ledAlerts ^= 1 << ledPreview;
				sendLedSettings();
				drawLedPicker();
			}
			else if (button == 24)
			{
				if (s == SettingsScreen::LedThemePicker) ledTheme = ledPreview;
				else
				{
					ledIdleMode = ledPreview;
					if (ledPreviewUsesColor()) ledIdleColorHex = ledPreviewColorHex;
				}
				sendLedSettings();
				showSettingsScreen(SettingsScreen::LedSettings);
			}
			return;
		}
		if (isLedSlider(button))
		{
			updateLedSliderFromTouch(button);
			sendLedSettings();
			drawLedSlider(button);
			redrawLedButton(8, false);
			redrawLedButton(9, false);
			return;
		}
		if (isLedStepper(button))
		{
			if (!ledRepeatFired) ledStep(button);
			ledRepeatFired = false;
			sendLedSettings();
			return;
		}
		switch (button)
		{
		case 8:
		case 9:
			if (button == 9 && ledBrightness > 0) { lastLedBrightness = ledBrightness; ledBrightness = 0; }
			else if (button == 8 && ledBrightness == 0) ledBrightness = lastLedBrightness > 0 ? lastLedBrightness : 100;
			sendLedSettings();
			drawLedSlider(3);
			redrawLedButton(8, false);
			redrawLedButton(9, false);
			break;
		case 38:
			ledStartLeft = !ledStartLeft;
			sendLedSettings();
			redrawLedButton(38, false);
			break;
		case 39:
			ledGet("/api/test?layout=1");
			break;
		case 44:
			ledResetArmed = false;
			ledStatusText = ledGet("/restart") ? "Riavvio della striscia..." : "La striscia non risponde";
			drawLedAdvancedStatus();
			break;
		case 45:
			if (!ledResetArmed)
			{
				ledResetArmed = true;
				ledStatusText = "Tocca CONFERMA per azzerare giri e circuito";
			}
			else
			{
				ledResetArmed = false;
				ledStatusText = ledGet("/api/reset") ? "Giri e circuito azzerati" : "La striscia non risponde";
			}
			drawLedAdvancedStatus();
			redrawLedButton(45, false);
			break;
		}
	}
