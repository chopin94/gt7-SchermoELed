	// FRENATA: coaching of the braking points. A countdown to the next point
	// where the best lap started to brake, with a bar that fills as it gets
	// closer, how the last braking compared (metres earlier or later, entry
	// speed) and one chip per braking point of the lap with its result.
	enum : int
	{
		BRK_CHIP_W = 24,
		BRK_CHIP_H = 21,
		BRK_CHIP_PITCH = 25,
		BRK_CHIPS_PER_ROW = 12,
		BRK_ROW1_Y = 192,
		BRK_ROW2_Y = 216,
		BRK_BAR_X = 8,
		BRK_BAR_Y = 116,
		BRK_BAR_W = 304,
		BRK_BAR_H = 10,
	};
	static constexpr float BRK_BAR_RANGE_M = 300.0f; // the bar starts filling this far from the point
	int brakeChipsDrawn = 0;

	struct BrakeView
	{
		int zones;                        // braking points of the reference lap
		float delta[LapAnalysis::MAX_BRAKE_ZONES]; // result this lap, NAN: not braked
		int next;                         // next point, -1 none
		bool hasNext;
		float meters, referenceKmh;       // to the next point
		bool hasResult;                   // a braking has been measured
		float resultDelta, entryKmh, resultReferenceKmh;
	};

	void fillBrakeView(BrakeView &view)
	{
		if (settingsScreen == SettingsScreen::ThemeSelection)
		{
			// Fixed example for the selector.
			const float example[8] = {3.0f, -9.0f, 14.0f, 0.5f, -22.0f, NAN, NAN, NAN};
			view.zones = 8;
			for (int i = 0; i < LapAnalysis::MAX_BRAKE_ZONES; ++i) view.delta[i] = i < 8 ? example[i] : NAN;
			view.next = 5;
			view.hasNext = true;
			view.meters = 142.0f; view.referenceKmh = 246.0f;
			view.hasResult = true;
			view.resultDelta = -22.0f; view.entryKmh = 251.0f; view.resultReferenceKmh = 246.0f;
			return;
		}
		view.zones = lapAnalysis.referenceBrakeZoneCount();
		for (int i = 0; i < LapAnalysis::MAX_BRAKE_ZONES; ++i) view.delta[i] = lapAnalysis.brakeDelta(i);
		view.next = lapAnalysis.nextBrakeIndex();
		view.hasNext = lapAnalysis.nextBrakePoint(view.meters, view.referenceKmh);
		view.hasResult = lapAnalysis.lastBrakeResult(view.resultDelta, view.entryKmh, view.resultReferenceKmh);
	}

	// Green within 5 m of the reference, yellow within 15 m, red beyond.
	uint16_t brakeDeltaColor(float metres)
	{
		const float distance = fabsf(metres);
		if (distance <= 5.0f) return tft.color565(0, 150, 64);
		if (distance <= 15.0f) return tft.color565(224, 172, 0);
		return tft.color565(196, 28, 34);
	}

	// "+12" earlier or "-8" later, "0" when the same.
	static String brakeDeltaShort(float metres)
	{
		const int m = static_cast<int>(lroundf(metres));
		if (m == 0) return "0";
		return String(m > 0 ? "+" : "") + String(m);
	}

	void drawBrakeChip(int index, const BrakeView &view, bool forceUpdate)
	{
		const int row = index / BRK_CHIPS_PER_ROW, column = index % BRK_CHIPS_PER_ROW;
		const int x = 8 + column * BRK_CHIP_PITCH, y = row == 0 ? BRK_ROW1_Y : BRK_ROW2_Y;
		// States: braked (coloured by the result), upcoming, current, passed.
		const bool braked = isfinite(view.delta[index]);
		const bool upcoming = view.next >= 0 && index >= view.next;
		const bool current = view.next == index;
		const String key = String(braked ? static_cast<int>(lroundf(view.delta[index])) : 9999) + "|" +
			String(upcoming) + String(current) + "|" + String(view.zones);
		const String id = String("brkChip") + String(index);
		String &previous = prevData[id];
		if (!forceUpdate && previous == key) return;
		previous = key;

		uint16_t fill = tft.color565(18, 20, 24), ink = analysisMuted(), line = tft.color565(44, 48, 56);
		String text = "--";
		if (braked)
		{
			fill = brakeDeltaColor(view.delta[index]);
			line = fill;
			ink = fill == tft.color565(224, 172, 0) ? TFT_BLACK : TFT_WHITE;
			text = brakeDeltaShort(view.delta[index]);
		}
		else if (upcoming)
		{
			text = String(index + 1);
			line = current ? TFT_WHITE : tft.color565(84, 90, 100);
			ink = current ? TFT_WHITE : analysisMuted();
		}
		tft.fillRoundRect(x, y, BRK_CHIP_W, BRK_CHIP_H, 3, fill);
		tft.drawRoundRect(x, y, BRK_CHIP_W, BRK_CHIP_H, 3, line);
		if (current) tft.drawRoundRect(x + 1, y + 1, BRK_CHIP_W - 2, BRK_CHIP_H - 2, 2, line);
		tft.setFont(&fonts::Font0);
		tft.setTextSize(1.0f);
		tft.setTextDatum(MC_DATUM);
		tft.setTextColor(ink, fill);
		tft.drawString(text, x + BRK_CHIP_W / 2, y + BRK_CHIP_H / 2);
	}

	// The bar fills as the car closes on the braking point.
	void drawBrakeApproach(const BrakeView &view, bool forceUpdate)
	{
		int filled = 0;
		uint16_t color = tft.color565(0, 170, 90);
		if (view.hasNext)
		{
			const float closeness = 1.0f - constrain(view.meters / BRK_BAR_RANGE_M, 0.0f, 1.0f);
			filled = static_cast<int>(lroundf(closeness * BRK_BAR_W));
			if (view.meters < 50.0f) color = tft.color565(226, 30, 36);
			else if (view.meters < 150.0f) color = tft.color565(255, 190, 0);
		}
		const String key = String(filled) + "|" + String(color);
		String &previous = prevData["brkBar"];
		if (!forceUpdate && previous == key) return;
		previous = key;
		tft.fillRect(BRK_BAR_X, BRK_BAR_Y, BRK_BAR_W, BRK_BAR_H, tft.color565(26, 28, 32));
		if (filled > 0) tft.fillRect(BRK_BAR_X, BRK_BAR_Y, filled, BRK_BAR_H, color);
		tft.drawRect(BRK_BAR_X - 1, BRK_BAR_Y - 1, BRK_BAR_W + 2, BRK_BAR_H + 2, analysisLine());
	}

	void drawBrakeStaticLayout()
	{
		tft.fillScreen(TFT_BLACK);
		tft.setFont(&fonts::Font0);
		tft.setTextSize(1.0f);
		tft.setTextDatum(TL_DATUM);
		tft.setTextColor(analysisMuted(), TFT_BLACK);
		tft.drawString("KM/H", 92, 8);
		tft.drawString("PROSSIMA FRENATA", 8, 50);
		tft.drawString("ULTIMA FRENATA", 8, 134);
		tft.drawString("PUNTI: + METRI PRIMA / - DOPO IL RIFERIMENTO", 8, 180);
		brakeChipsDrawn = 0;
		tft.drawFastHLine(0, 130, SCREEN_WIDTH, analysisLine());
		tft.drawFastHLine(0, 176, SCREEN_WIDTH, analysisLine());
	}

	void drawBrakeDashboard(const DashboardState &state, bool forceUpdate)
	{
		if (forceUpdate) drawBrakeStaticLayout();

		drawAnalysisGear("brkGear", state.gear, 4, 2, 34, 38, TFT_WHITE, TFT_BLACK, forceUpdate);
		drawAnalysisText("brkSpeed", state.speed, 88, 22, 48, MR_DATUM, &fonts::Font4,
			TFT_WHITE, TFT_BLACK, forceUpdate);
		const uint16_t zones[3] = {tft.color565(0, 210, 70), tft.color565(255, 200, 0), tft.color565(255, 30, 30)};
		drawShiftLights(state, 12, 124, 6, 12, 12, 16, true, zones, tft.color565(255, 30, 30), forceUpdate);

		BrakeView view;
		fillBrakeView(view);

		// Countdown: every metre close to the point, every 5 m further away.
		if (view.hasNext)
		{
			const int rounded = view.meters < 100.0f ? static_cast<int>(lroundf(view.meters))
				: static_cast<int>(lroundf(view.meters / 5.0f)) * 5;
			const uint16_t color = view.meters < 50.0f ? analysisRed()
				: view.meters < 150.0f ? analysisYellow() : TFT_WHITE;
			drawAnalysisText("brkMeters", String(rounded), 8, 62, 118, TL_DATUM, &fonts::Font7, color,
				TFT_BLACK, forceUpdate);
			drawAnalysisText("brkMetersUnit", "m", 132, 90, 20, TL_DATUM, &fonts::Font4, analysisMuted(),
				TFT_BLACK, forceUpdate);
			drawAnalysisText("brkRefSpeed", String(static_cast<int>(lroundf(view.referenceKmh))) + " km/h",
				312, 76, 120, TR_DATUM, &fonts::Font4, TFT_WHITE, TFT_BLACK, forceUpdate);
			drawAnalysisText("brkRefLabel", "VELOCITA' DI RIFERIMENTO", 312, 62, 150, TR_DATUM, &fonts::Font0,
				analysisMuted(), TFT_BLACK, forceUpdate);
		}
		else
		{
			drawAnalysisText("brkMeters", "--", 8, 62, 118, TL_DATUM, &fonts::Font7, analysisMuted(),
				TFT_BLACK, forceUpdate);
			drawAnalysisText("brkMetersUnit", "", 132, 90, 20, TL_DATUM, &fonts::Font4, analysisMuted(),
				TFT_BLACK, forceUpdate);
			drawAnalysisText("brkRefSpeed", view.zones > 0 ? "" : "SERVE UN GIRO", 312, 76, 160, TR_DATUM,
				&fonts::Font2, analysisMuted(), TFT_BLACK, forceUpdate);
			drawAnalysisText("brkRefLabel", view.zones > 0 ? "" : "DI RIFERIMENTO", 312, 94, 160, TR_DATUM,
				&fonts::Font2, analysisMuted(), TFT_BLACK, forceUpdate);
		}
		drawBrakeApproach(view, forceUpdate);

		// Last braking.
		if (view.hasResult && isfinite(view.resultDelta))
		{
			const int metres = static_cast<int>(lroundf(fabsf(view.resultDelta)));
			const String text = metres == 0 ? String("UGUALE")
				: String(metres) + " m " + (view.resultDelta > 0 ? "PRIMA" : "DOPO");
			drawAnalysisText("brkResult", text, 8, 156, 190, ML_DATUM, &fonts::Font4,
				brakeDeltaColor(view.resultDelta), TFT_BLACK, forceUpdate);
			drawAnalysisText("brkSpeeds",
				String("TU ") + static_cast<int>(lroundf(view.entryKmh)) + "  RIF " +
					static_cast<int>(lroundf(view.resultReferenceKmh)) + " km/h",
				312, 156, 120, MR_DATUM, &fonts::Font2, TFT_WHITE, TFT_BLACK, forceUpdate);
		}
		else if (view.hasResult)
		{
			drawAnalysisText("brkResult", "NESSUN RIFERIMENTO", 8, 156, 190, ML_DATUM, &fonts::Font2,
				analysisMuted(), TFT_BLACK, forceUpdate);
			drawAnalysisText("brkSpeeds", String("INGRESSO ") + static_cast<int>(lroundf(view.entryKmh)) + " km/h",
				312, 156, 120, MR_DATUM, &fonts::Font2, TFT_WHITE, TFT_BLACK, forceUpdate);
		}
		else
		{
			drawAnalysisText("brkResult", "--", 8, 156, 190, ML_DATUM, &fonts::Font4, analysisMuted(),
				TFT_BLACK, forceUpdate);
			drawAnalysisText("brkSpeeds", "", 312, 156, 120, MR_DATUM, &fonts::Font2, TFT_WHITE, TFT_BLACK,
				forceUpdate);
		}

		// A new reference with fewer points: wipe the chips that are no longer used.
		for (int i = view.zones; i < brakeChipsDrawn && i < LapAnalysis::MAX_BRAKE_ZONES; ++i)
		{
			const int row = i / BRK_CHIPS_PER_ROW, column = i % BRK_CHIPS_PER_ROW;
			tft.fillRect(8 + column * BRK_CHIP_PITCH, row == 0 ? BRK_ROW1_Y : BRK_ROW2_Y, BRK_CHIP_W, BRK_CHIP_H, TFT_BLACK);
			prevData[String("brkChip") + String(i)] = "";
		}
		brakeChipsDrawn = view.zones;
		for (int i = 0; i < view.zones && i < LapAnalysis::MAX_BRAKE_ZONES; ++i) drawBrakeChip(i, view, forceUpdate);
		restoreDefaultText();
	}
