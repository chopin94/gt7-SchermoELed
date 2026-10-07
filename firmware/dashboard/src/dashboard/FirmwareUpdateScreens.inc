	// Firmware update over Wi-Fi: where to open the update page (settings >
	// DISPOSITIVO > AGGIORNAMENTO) and the full-screen progress of a transfer.

	static constexpr uint32_t FIRMWARE_UPDATE_ERROR_MS = 8000;
	// No news from a transfer for this long: it is over, whatever happened.
	static constexpr uint32_t FIRMWARE_UPDATE_STALL_MS = 20000;

	// The display fonts have no accented letters: "è" becomes "e'" as in the
	// other screens. The messages come from lib/OtaUpdate in UTF-8.
	static String screenText(const char *text)
	{
		String out;
		for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; ++p)
		{
			if (*p < 0x80) { out += char(*p); continue; }
			if (*p == 0xC3 && p[1])
			{
				const unsigned char c = *++p;
				const char base = c == 0xA0 ? 'a' : c == 0xA8 || c == 0xA9 ? 'e' : c == 0xAC ? 'i'
					: c == 0xB2 ? 'o' : c == 0xB9 ? 'u' : 0;
				if (base) { out += base; out += '\''; }
			}
		}
		return out;
	}

	// Up to `lines` centred lines of Font2 from a message, broken between words.
	void drawWrappedText(const String &text, int y, int lines, int lineHeight)
	{
		tft.setFont(&fonts::Font2);
		tft.setTextDatum(MC_DATUM);
		const int maxWidth = SCREEN_WIDTH - 24;
		int start = 0;
		for (int line = 0; line < lines && start < int(text.length()); ++line)
		{
			int end = text.length();
			while (tft.textWidth(text.substring(start, end)) > maxWidth)
			{
				const int space = text.lastIndexOf(' ', end - 1);
				end = space > start ? space : end - 1;
			}
			tft.drawString(text.substring(start, end), X_CENTER, y + line * lineHeight);
			start = end;
			while (start < int(text.length()) && text[start] == ' ') ++start;
		}
	}

	static String megabytes(uint32_t bytes)
	{
		char buffer[12];
		snprintf(buffer, sizeof(buffer), "%.1f MB", bytes / 1048576.0f);
		return String(buffer);
	}

	// Settings > DISPOSITIVO > AGGIORNAMENTO.
	void drawFirmwareUpdateInfo(int pressedButton)
	{
		tft.setTextColor(TFT_WHITE, TFT_BLACK);
		tft.drawString("AGGIORNAMENTO", X_CENTER, 18, 4);
		if (wifiConnected && updatePageReady)
		{
			drawQrCode((String("http://") + networkStatus + "/").c_str());
			tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
			tft.drawString("Inquadra il QR", 235, 53, 2);
			tft.drawString("o apri dal telefono", 235, 75, 1);
			tft.drawString("o dal PC l'indirizzo", 235, 91, 1);
			tft.setTextColor(TFT_CYAN, TFT_BLACK);
			tft.drawString(networkStatus, 235, 113, 2);
			tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
			tft.drawString("e carica firmware.bin", 235, 137, 1);
			tft.setTextColor(TFT_WHITE, TFT_BLACK);
			tft.drawString(String("Versione ") + GT7_DASH_VERSION, 235, 164, 2);
			tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
			tft.drawString(GT7_DASH_BOARD, 235, 184, 1);
		}
		else
		{
			tft.setTextColor(TFT_WHITE, TFT_BLACK);
			tft.drawString(String("Versione ") + GT7_DASH_VERSION, X_CENTER, 58, 2);
			tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
			tft.drawString(GT7_DASH_BOARD, X_CENTER, 78, 1);
			tft.setTextColor(analysisYellow(), TFT_BLACK);
			tft.drawString("Aggiornamento Wi-Fi non disponibile", X_CENTER, 112, 2);
			tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
			if (!telemetryModeUsesWifi(telemetry.mode))
			{
				tft.drawString("Con SimHub il Wi-Fi e' spento:", X_CENTER, 140, 2);
				tft.drawString("passa a GT7 o AC, o usa il cavo USB", X_CENTER, 160, 2);
			}
			else
			{
				tft.drawString("Lo schermo non e' collegato al Wi-Fi:", X_CENTER, 140, 2);
				tft.drawString("riprova quando e' collegato", X_CENTER, 160, 2);
			}
		}
		drawSettingsButton(105, 204, 110, 28, "INDIETRO", pressedButton == 0);
	}

	// Progress of a transfer, then the result. Only the bar and the numbers
	// change while the file arrives.
	void drawFirmwareUpdateScreen(bool full)
	{
		const OtaImage::Progress &update = firmwareUpdate;
		const bool done = update.phase == OtaImage::Phase::Done;
		const bool failed = update.phase == OtaImage::Phase::Failed;
		const uint16_t accent = failed ? analysisRed() : done ? analysisGreen() : tft.color565(0, 150, 255);
		const int percent = done ? 100 : update.total
			? int(min<uint64_t>(100, uint64_t(update.received) * 100 / update.total)) : -1;
		if (!full && percent == firmwareUpdateDrawnPercent) return;
		firmwareUpdateDrawnPercent = percent;
		firmwareUpdateDrawnPhase = update.phase;

		if (full)
		{
			tft.fillScreen(TFT_BLACK);
			tft.setTextDatum(MC_DATUM);
			tft.setTextColor(failed ? analysisRed() : done ? analysisGreen() : TFT_WHITE, TFT_BLACK);
			tft.drawString(failed ? "ERRORE" : done ? "AGGIORNATO" : "AGGIORNAMENTO", X_CENTER, 30, 4);
			tft.setTextColor(analysisMuted(), TFT_BLACK);
			tft.drawString(failed ? "Il firmware attuale non e' cambiato"
				: done ? "Riavvio con la versione nuova..." : "Firmware via Wi-Fi", X_CENTER, 58, 2);
			if (failed)
			{
				tft.setTextColor(TFT_WHITE, TFT_BLACK);
				drawWrappedText(screenText(update.message), 108, 3, 20);
				tft.setFont(&fonts::Font2);
				tft.setTextColor(analysisMuted(), TFT_BLACK);
				tft.drawString("Torna alla schermata di prima tra poco", X_CENTER, 212);
			}
			else
			{
				tft.drawRoundRect(20, 138, SCREEN_WIDTH - 40, 18, 6, analysisLine());
				tft.setTextColor(analysisYellow(), TFT_BLACK);
				tft.drawString("Non spegnere lo schermo", X_CENTER, 212, 2);
			}
		}
		if (failed)
		{
			restoreDefaultText();
			return;
		}

		// Percentage (KB when the size is unknown), bar and amount received.
		tft.setFont(&fonts::FreeSansBold24pt7b);
		tft.setTextDatum(MC_DATUM);
		tft.setTextPadding(200);
		tft.setTextColor(TFT_WHITE, TFT_BLACK);
		tft.drawString(percent >= 0 ? String(percent) + "%" : String(update.received / 1024) + " KB", X_CENTER, 104);
		const int inner = SCREEN_WIDTH - 44;
		const int filled = percent >= 0 ? inner * percent / 100 : 0;
		tft.fillRoundRect(22, 140, max(filled, 8), 14, 5, accent);
		if (filled < inner) tft.fillRect(22 + max(filled, 8), 140, inner - max(filled, 8), 14, TFT_BLACK);
		tft.setFont(&fonts::Font2);
		tft.setTextPadding(240);
		tft.setTextColor(analysisMuted(), TFT_BLACK);
		tft.drawString(update.total ? megabytes(update.received) + " di " + megabytes(update.total)
			: megabytes(update.received), X_CENTER, 176);
		restoreDefaultText();
	}

	// From loop() while the update screen is up. True when it has just been
	// closed and the normal loop can go on.
	bool serviceFirmwareUpdateScreen()
	{
		const uint32_t quiet = millis() - firmwareUpdateChangedAt;
		if (firmwareUpdate.phase == OtaImage::Phase::Receiving && quiet >= FIRMWARE_UPDATE_STALL_MS)
		{
			// The loop runs again, so no transfer is in progress any more.
			firmwareUpdate.phase = OtaImage::Phase::Failed;
			snprintf(firmwareUpdate.message, sizeof(firmwareUpdate.message), "Trasferimento interrotto");
			firmwareUpdateChangedAt = millis();
			drawFirmwareUpdateScreen(true);
			return false;
		}
		if (firmwareUpdate.phase != OtaImage::Phase::Failed || quiet < FIRMWARE_UPDATE_ERROR_MS) return false;
		firmwareUpdateActive = false;
		firmwareUpdateDrawnPercent = -1;
		if (settingsScreen != SettingsScreen::Closed)
		{
			settingsLastInteractionTime = millis();
			drawSettingsScreen();
		}
		else redrawAfterWifiResetDialog();
		return true;
	}
