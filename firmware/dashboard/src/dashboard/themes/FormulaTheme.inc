	// FORMULA: steering-wheel display of a single-seater. Shift lights green,
	// red and purple (all flashing at the limiter), live delta on a coloured
	// box, the gear in the middle, three sectors coloured like F1 timing and the
	// four tyres at the corners.
	enum : int
	{
		FORMULA_LED_Y = 3,
		FORMULA_LED_W = 17,
		FORMULA_LED_H = 12,
		FORMULA_LED_PITCH = 21,
		FORMULA_SIDE_W = 100,
		FORMULA_LEFT_X = 4,
		FORMULA_RIGHT_X = 216,
		FORMULA_GEAR_X = 110,
		FORMULA_GEAR_Y = 22,
		FORMULA_GEAR_W = 100,
		FORMULA_GEAR_H = 100,
		FORMULA_SECTOR_Y = 128,
		FORMULA_SECTOR_H = 22,
		FORMULA_TYRE_Y = 156,
		FORMULA_TYRE_W = 54,
		FORMULA_TYRE_H = 38,
	};

	void drawFormulaLabel(const char *text, int x, int y, uint8_t datum = TL_DATUM)
	{
		tft.setFont(&fonts::Font0);
		tft.setTextSize(1.0f);
		tft.setTextDatum(datum);
		tft.setTextColor(analysisMuted(), TFT_BLACK);
		tft.drawString(text, x, y);
	}

	void drawFormulaStaticLayout()
	{
		tft.fillScreen(TFT_BLACK);
		tft.fillRoundRect(FORMULA_GEAR_X, FORMULA_GEAR_Y, FORMULA_GEAR_W, FORMULA_GEAR_H, 10,
			analysisPanel());
		tft.drawRoundRect(FORMULA_GEAR_X, FORMULA_GEAR_Y, FORMULA_GEAR_W, FORMULA_GEAR_H, 10,
			analysisLine());
		drawFormulaLabel("DELTA", FORMULA_LEFT_X + 2, 22);
		drawFormulaLabel("GIRO", FORMULA_LEFT_X + 2, 82);
		drawFormulaLabel("KM/H", FORMULA_RIGHT_X + FORMULA_SIDE_W - 2, 22, TR_DATUM);
		drawFormulaLabel("ULTIMO", FORMULA_RIGHT_X + 2, 82);
		// Tyres: front at the top, rear at the bottom, as on the car.
		for (int i = 0; i < 4; ++i)
		{
			const int x = i % 2 == 0 ? FORMULA_LEFT_X : SCREEN_WIDTH - FORMULA_LEFT_X - FORMULA_TYRE_W;
			const int y = FORMULA_TYRE_Y + (i / 2) * (FORMULA_TYRE_H + 4);
			tft.drawRoundRect(x, y, FORMULA_TYRE_W, FORMULA_TYRE_H, 6, analysisLine());
		}
		// Centre bottom: best lap, lap, position and fuel.
		const int cx = FORMULA_LEFT_X + FORMULA_TYRE_W + 6;
		const int cw = SCREEN_WIDTH - 2 * cx;
		tft.drawRoundRect(cx, FORMULA_TYRE_Y, cw, 38, 6, analysisLine());
		drawFormulaLabel("MIGLIORE", cx + 6, FORMULA_TYRE_Y + 4);
		drawFormulaLabel("GIRO", cx + 6, FORMULA_TYRE_Y + 46);
		drawFormulaLabel("POS", cx + cw / 2 - 18, FORMULA_TYRE_Y + 46);
		drawFormulaLabel("CARB.", cx + cw - 6, FORMULA_TYRE_Y + 46, TR_DATUM);
	}

	void drawFormulaTyres(const DashboardState &state, bool forceUpdate)
	{
		for (int i = 0; i < 4; ++i)
		{
			const float t = state.tyreTemperatures[i];
			const uint16_t color = analysisTyreColor(t);
			const String text = isfinite(t) ? String(static_cast<int>(lroundf(t))) : "--";
			const String key = String("forTyre") + i;
			const String cache = text + "|" + String(color);
			if (!forceUpdate && prevData[key] == cache) continue;
			prevData[key] = cache;
			const int x = i % 2 == 0 ? FORMULA_LEFT_X : SCREEN_WIDTH - FORMULA_LEFT_X - FORMULA_TYRE_W;
			const int y = FORMULA_TYRE_Y + (i / 2) * (FORMULA_TYRE_H + 4);
			tft.fillRoundRect(x + 2, y + 2, FORMULA_TYRE_W - 4, FORMULA_TYRE_H - 4, 5, color);
			tft.setFont(&fonts::FreeSansBold12pt7b);
			tft.setTextSize(1.0f);
			tft.setTextDatum(MC_DATUM);
			const bool darkInk = color == analysisYellow() || (isfinite(t) && t >= 95 && t < 105);
			tft.setTextColor(darkInk ? TFT_BLACK : TFT_WHITE, color);
			tft.drawString(text, x + FORMULA_TYRE_W / 2, y + FORMULA_TYRE_H / 2 + 1);
		}
	}

	void drawFormulaDashboard(const DashboardState &state, bool forceUpdate)
	{
		if (forceUpdate) drawFormulaStaticLayout();

		const uint16_t zones[3] = {tft.color565(0, 210, 70), tft.color565(255, 30, 30), analysisPurple()};
		drawShiftLights(state, 15, 4, FORMULA_LED_Y, FORMULA_LED_W, FORMULA_LED_H,
			FORMULA_LED_PITCH, false, zones, analysisPurple(), forceUpdate);

		// Left: live delta and current lap.
		drawDeltaBox("forDelta", FORMULA_LEFT_X, 32, FORMULA_SIDE_W, 44,
			state.sessionBestLiveDeltaSeconds, &fonts::FreeSansBold12pt7b, forceUpdate, 1.15f);
		drawAnalysisText("forLap", compactLapTime(state.currentLapTime), FORMULA_LEFT_X + FORMULA_SIDE_W / 2, 106,
			FORMULA_SIDE_W, MC_DATUM, &fonts::FreeSansBold12pt7b, TFT_WHITE, TFT_BLACK, forceUpdate);

		// Centre: gear, with the suggested gear in a corner when GT7 reports it.
		drawAnalysisGear("forGear", state.gear, FORMULA_GEAR_X + 8, FORMULA_GEAR_Y + 8,
			FORMULA_GEAR_W - 16, FORMULA_GEAR_H - 16, TFT_WHITE, analysisPanel(), forceUpdate);

		// Right: speed and last lap.
		drawAnalysisText("forSpeed", state.speed, FORMULA_RIGHT_X + FORMULA_SIDE_W - 2, 56,
			FORMULA_SIDE_W, MR_DATUM, &fonts::FreeSansBold24pt7b, TFT_WHITE, TFT_BLACK, forceUpdate);
		drawAnalysisText("forLast", compactLapTime(state.lastLapTime), FORMULA_RIGHT_X + FORMULA_SIDE_W / 2, 106,
			FORMULA_SIDE_W, MC_DATUM, &fonts::FreeSansBold12pt7b, analysisMuted(), TFT_BLACK, forceUpdate);

		drawSectorBar("forSector", FORMULA_LEFT_X, FORMULA_SECTOR_Y, SCREEN_WIDTH - 2 * FORMULA_LEFT_X,
			FORMULA_SECTOR_H, 4, true, forceUpdate);

		drawFormulaTyres(state, forceUpdate);

		// Centre bottom.
		const int cx = FORMULA_LEFT_X + FORMULA_TYRE_W + 6;
		const int cw = SCREEN_WIDTH - 2 * cx;
		const int32_t best = lapAnalysis.bestLapMs();
		const String bestText = best > 0 ? analysisLapTime(best) : compactLapTime(state.bestLapTime);
		drawAnalysisText("forBest", bestText, cx + cw / 2, FORMULA_TYRE_Y + 24, cw - 8, MC_DATUM,
			&fonts::FreeSansBold12pt7b, analysisPurple(), TFT_BLACK, forceUpdate);
		const int rowY = FORMULA_TYRE_Y + 66;
		drawAnalysisText("forLapNo", state.tyrePressureRearLeft, cx + 6, rowY, 50, ML_DATUM,
			&fonts::Font4, TFT_WHITE, TFT_BLACK, forceUpdate, 0.85f);
		drawAnalysisText("forPos", state.tyrePressureFrontRight, cx + cw / 2 - 18, rowY, 34, ML_DATUM,
			&fonts::Font4, TFT_WHITE, TFT_BLACK, forceUpdate, 0.85f);
		const bool reserve = state.fuelValueValid && state.fuelProgressPercent <= 15;
		drawAnalysisText("forFuel", state.fuelDisplayValue + (state.fuelIsEV ? "" : "%"), cx + cw - 6, rowY, 52,
			MR_DATUM, &fonts::Font4, reserve ? analysisRed() : TFT_WHITE, TFT_BLACK, forceUpdate, 0.85f);
		restoreDefaultText();
	}
