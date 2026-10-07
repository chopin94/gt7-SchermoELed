	// Session summary (waiting screen "RIEPILOGO") and the full-screen
	// notifications (best lap, last lap, fuel reserve).

	static String analysisGap(int32_t ms)
	{
		char buffer[12];
		snprintf(buffer, sizeof(buffer), "%c%d.%03d", ms < 0 ? '-' : '+', abs(ms) / 1000, abs(ms) % 1000);
		return String(buffer);
	}

	// Times of the session on the waiting screen: best, ideal (sum of the best
	// sectors), average, top speed and the last laps.
	void drawSessionSummary()
	{
		const uint16_t muted = analysisMuted();
		tft.setTextSize(1.0f);
		tft.setFont(&fonts::FreeSansBold9pt7b);
		tft.setTextDatum(MC_DATUM);
		tft.setTextColor(TFT_WHITE, TFT_BLACK);
		tft.drawString("RIEPILOGO SESSIONE", X_CENTER, 13);

		const int laps = lapAnalysis.lapsCompleted();
		if (laps == 0)
		{
			drawFerrariFlagIcon(X_CENTER, 92, muted);
			tft.setFont(&fonts::FreeSansBold9pt7b);
			tft.setTextColor(TFT_WHITE, TFT_BLACK);
			tft.drawString("Nessun giro completato", X_CENTER, 128);
			tft.setFont(&fonts::Font2);
			tft.setTextColor(muted, TFT_BLACK);
			tft.drawString("I tempi della sessione compaiono qui", X_CENTER, 154);
			tft.drawString("quando il gioco e' in pausa o chiuso", X_CENTER, 172);
			restoreDefaultText();
			return;
		}

		tft.drawFastHLine(8, 26, SCREEN_WIDTH - 16, analysisLine());
		tft.drawFastVLine(166, 32, 186, analysisLine());

		// Left column: the session at a glance.
		struct Line { const char *label; String value; uint16_t color; };
		int32_t ideal = 0;
		for (int i = 0; i < LapAnalysis::SECTORS; ++i)
		{
			const int32_t best = lapAnalysis.bestSectorMs(i);
			if (best <= 0) { ideal = -1; break; }
			ideal += best;
		}
		const Line lines[4] = {
			{"MIGLIORE", analysisLapTime(lapAnalysis.bestLapMs()), analysisPurple()},
			{"GIRO IDEALE", ideal > 0 ? analysisLapTime(ideal) : String("--"), TFT_WHITE},
			{"MEDIA", analysisLapTime(lapAnalysis.averageLapMs()), TFT_WHITE},
			{"V MAX", String(static_cast<int>(lroundf(lapAnalysis.topSpeedKmh()))) + " km/h", TFT_WHITE},
		};
		for (int i = 0; i < 4; ++i)
		{
			const int y = 36 + i * 46;
			tft.setFont(&fonts::Font0);
			tft.setTextDatum(TL_DATUM);
			tft.setTextColor(muted, TFT_BLACK);
			tft.drawString(lines[i].label, 12, y);
			tft.setFont(&fonts::FreeSansBold12pt7b);
			tft.setTextDatum(ML_DATUM);
			tft.setTextColor(lines[i].color, TFT_BLACK);
			tft.drawString(lines[i].value, 12, y + 24);
		}

		// Right column: the last laps, best in purple, gap to the best.
		tft.setFont(&fonts::Font0);
		tft.setTextDatum(TL_DATUM);
		tft.setTextColor(muted, TFT_BLACK);
		tft.drawString(String("ULTIMI GIRI (") + laps + ")", 176, 36);
		const int32_t best = lapAnalysis.bestLapMs();
		const int shown = min(lapAnalysis.storedLaps(), 7);
		for (int age = 0; age < shown; ++age)
		{
			const LapAnalysis::LapRecord &lap = lapAnalysis.lapRecord(age);
			const int y = 58 + age * 23;
			const bool isBest = lap.lapMs == best;
			tft.setFont(&fonts::Font2);
			tft.setTextDatum(ML_DATUM);
			tft.setTextColor(muted, TFT_BLACK);
			tft.drawString(String(laps - age), 176, y);
			tft.setTextColor(isBest ? analysisPurple() : TFT_WHITE, TFT_BLACK);
			tft.drawString(analysisLapTime(lap.lapMs), 200, y);
			if (!isBest && best > 0)
			{
				tft.setTextDatum(MR_DATUM);
				tft.setTextColor(analysisYellow(), TFT_BLACK);
				tft.drawString(analysisGap(lap.lapMs - best), 312, y);
			}
		}
		restoreDefaultText();
	}

	// ---- Notifications ------------------------------------------------------------
	// Shows the queued notifications one after the other. True while one is
	// on screen; the theme is drawn again from scratch afterwards.
	bool serviceNotification(const DashboardState &state)
	{
		const uint32_t now = millis();
		if (notificationActive && now - notificationStart >= NOTIFICATION_MS)
		{
			notificationActive = false;
			invalidateDashboardRenderer();
		}
		if (!notificationActive)
		{
			// Events that happened while the menus were open are stale.
			while (notificationCount > 0 && now - notificationQueue[0].queuedAt > 8000) dropNotification();
			if (notificationCount == 0) return false;
			shownNotification = notificationQueue[0];
			dropNotification();
			notificationActive = true;
			notificationStart = now;
			drawNotification(state, true);
			return true;
		}
		drawNotification(state, false);
		return true;
	}

	void dropNotification()
	{
		for (int i = 1; i < notificationCount; ++i) notificationQueue[i - 1] = notificationQueue[i];
		if (notificationCount > 0) notificationCount--;
	}

	void drawNotification(const DashboardState &state, bool full)
	{
		const DashboardNotification &n = shownNotification;
		if (full)
		{
			tft.fillScreen(TFT_BLACK);
			uint16_t band = analysisPurple(), ink = TFT_WHITE;
			const char *title = "GIRO MIGLIORE";
			String value, detail;
			uint16_t detailColor = analysisGreen();
			switch (n.type)
			{
			case LapAnalysis::EventType::BestLap:
				value = analysisLapTime(n.lapMs);
				detail = n.gainMs > 0 ? analysisGap(-n.gainMs) + " s" : String("PRIMO GIRO CRONOMETRATO");
				if (n.gainMs <= 0) detailColor = analysisMuted();
				break;
			case LapAnalysis::EventType::FinalLap:
				band = TFT_WHITE;
				ink = TFT_BLACK;
				title = "ULTIMO GIRO";
				value = n.detail;
				detail = "BANDIERA A SCACCHI AL PROSSIMO PASSAGGIO";
				detailColor = analysisMuted();
				break;
			case LapAnalysis::EventType::LowFuel:
				band = tft.color565(255, 170, 0);
				ink = TFT_BLACK;
				title = "RISERVA";
				value = n.detail;
				detail = "DI CARBURANTE: PROGRAMMA LA SOSTA";
				detailColor = analysisMuted();
				break;
			}
			tft.fillRect(0, 26, SCREEN_WIDTH, 50, band);
			if (n.type == LapAnalysis::EventType::FinalLap)
			{
				// Chequered borders on the white band.
				for (int x = 0; x < SCREEN_WIDTH; x += 8)
				{
					tft.fillRect(x, (x / 8) % 2 ? 26 : 30, 8, 4, TFT_BLACK);
					tft.fillRect(x, (x / 8) % 2 ? 68 : 72, 8, 4, TFT_BLACK);
				}
			}
			tft.setTextSize(1.0f);
			tft.setFont(&fonts::FreeSansBold18pt7b);
			tft.setTextDatum(MC_DATUM);
			tft.setTextColor(ink, band);
			tft.drawString(title, X_CENTER, 52);
			const bool time = n.type == LapAnalysis::EventType::BestLap;
			tft.setFont(&fonts::FreeSansBold24pt7b);
			// Long texts step down to a smaller font instead of leaving the screen.
			if (tft.textWidth(value) > SCREEN_WIDTH - 16) tft.setFont(&fonts::FreeSansBold12pt7b);
			tft.setTextColor(time ? analysisPurple() : TFT_WHITE, TFT_BLACK);
			tft.drawString(value, X_CENTER, 116);
			tft.setFont(&fonts::Font2);
			tft.setTextColor(detailColor, TFT_BLACK);
			if (n.type == LapAnalysis::EventType::BestLap && n.gainMs > 0)
				tft.setFont(&fonts::FreeSansBold12pt7b);
			tft.drawString(detail, X_CENTER, 160);
			tft.drawFastHLine(0, 180, SCREEN_WIDTH, analysisLine());
			tft.setFont(&fonts::Font0);
			tft.setTextColor(analysisMuted(), TFT_BLACK);
			tft.setTextDatum(TR_DATUM);
			tft.drawString("KM/H", 312, 186);
		}
		// Live: revs, gear and speed keep updating under the message.
		const uint16_t zones[3] = {tft.color565(0, 210, 70), tft.color565(255, 200, 0), tft.color565(255, 30, 30)};
		drawShiftLights(state, 15, 4, 4, 17, 14, 21, false, zones, tft.color565(255, 30, 30), full);
		drawAnalysisGear("notifGear", state.gear, 8, 186, 46, 50, TFT_WHITE, TFT_BLACK, full);
		drawAnalysisText("notifSpeed", state.speed, 312, 214, 120, MR_DATUM, &fonts::FreeSansBold24pt7b,
			TFT_WHITE, TFT_BLACK, full);
		drawAnalysisText("notifLap", compactLapTime(state.currentLapTime), 70, 212, 110, ML_DATUM,
			&fonts::FreeSansBold12pt7b, analysisMuted(), TFT_BLACK, full);
		restoreDefaultText();
	}
