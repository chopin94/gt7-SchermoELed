	// TELEMETRIA: MoTeC-style screen. G-force circle with the trail of the last
	// three seconds, throttle and brake traces scrolling from the right, gear,
	// speed and the peaks of the session.
	enum : int
	{
		TEL_CIRCLE_X = 6,
		TEL_CIRCLE_Y = 46,
		TEL_CIRCLE_SIZE = 144,
		TEL_TRACE_X = 160,
		TEL_TRACE_Y = 46,
		TEL_TRACE_W = 156,
		TEL_TRACE_H = 104,
	};
	static constexpr float TEL_G_FULL_SCALE = 2.0f; // g at the edge of the circle

	LGFX_Sprite telemetryCircleSprite{&tft};
	LGFX_Sprite telemetryTraceSprite{&tft};
	bool telemetryCircleCreated = false;
	bool telemetryTraceCreated = false;
	uint32_t telemetryLastCircle = 0, telemetryLastTrace = 0;

	void releaseTelemetrySprites()
	{
		if (telemetryCircleCreated) { telemetryCircleSprite.deleteSprite(); telemetryCircleCreated = false; }
		if (telemetryTraceCreated) { telemetryTraceSprite.deleteSprite(); telemetryTraceCreated = false; }
	}

	// Felt force, as on a car's G meter: braking pushes the dot up (forwards),
	// accelerating down, a right turn pushes it to the left.
	void telemetryDotPosition(float lat, float lon, int center, int radius, int &x, int &y)
	{
		float dx = -lat / TEL_G_FULL_SCALE, dy = lon / TEL_G_FULL_SCALE;
		const float length = sqrtf(dx * dx + dy * dy);
		if (length > 1.0f) { dx /= length; dy /= length; }
		x = center + static_cast<int>(lroundf(dx * radius));
		y = center + static_cast<int>(lroundf(dy * radius));
	}

	void drawTelemetryCircle()
	{
		LGFX_Sprite &s = telemetryCircleSprite;
		if (!telemetryCircleCreated)
		{
			s.setColorDepth(8);
			telemetryCircleCreated = s.createSprite(TEL_CIRCLE_SIZE, TEL_CIRCLE_SIZE) != nullptr;
		}
		lgfx::LovyanGFX &canvas = telemetryCircleCreated ? static_cast<lgfx::LovyanGFX &>(s)
			: static_cast<lgfx::LovyanGFX &>(tft);
		const int ox = telemetryCircleCreated ? 0 : TEL_CIRCLE_X;
		const int oy = telemetryCircleCreated ? 0 : TEL_CIRCLE_Y;
		const int c = TEL_CIRCLE_SIZE / 2;
		const int r = c - 3;
		canvas.fillRect(ox, oy, TEL_CIRCLE_SIZE, TEL_CIRCLE_SIZE, TFT_BLACK);
		const uint16_t grid = tft.color565(52, 56, 64);
		canvas.drawCircle(ox + c, oy + c, r, tft.color565(96, 102, 112));
		canvas.drawCircle(ox + c, oy + c, r * 3 / 4, grid);
		canvas.drawCircle(ox + c, oy + c, r / 2, grid);
		canvas.drawCircle(ox + c, oy + c, r / 4, grid);
		canvas.drawFastHLine(ox + c - r, oy + c, 2 * r + 1, grid);
		canvas.drawFastVLine(ox + c, oy + c - r, 2 * r + 1, grid);
		canvas.setFont(&fonts::Font0);
		canvas.setTextColor(tft.color565(110, 116, 126));
		canvas.setTextDatum(TL_DATUM);
		canvas.drawString("1g", ox + c + r / 2 + 2, oy + c + 2);
		canvas.drawString("2g", ox + c + r - 12, oy + c + 2);
		canvas.setTextDatum(TC_DATUM);
		canvas.drawString("FRENATA", ox + c, oy + 2);
		canvas.setTextDatum(BC_DATUM);
		canvas.drawString("ACCELERAZIONE", ox + c, oy + TEL_CIRCLE_SIZE - 1);

		// Trail: older points darker and smaller.
		const int points = lapAnalysis.gTrailCount();
		int px = 0, py = 0;
		for (int age = points - 1; age >= 0; --age)
		{
			float lat, lon;
			lapAnalysis.gTrail(age, lat, lon);
			int x, y;
			telemetryDotPosition(lat, lon, c, r, x, y);
			const uint8_t shade = static_cast<uint8_t>(60 + 150 * (points - age) / max(points, 1));
			const uint16_t color = tft.color565(0, shade, shade / 2 + 30);
			if (age < points - 1) canvas.drawLine(ox + px, oy + py, ox + x, oy + y, color);
			px = x; py = y;
		}
		int x, y;
		telemetryDotPosition(lapAnalysis.lateralG(), lapAnalysis.longitudinalG(), c, r, x, y);
		canvas.fillCircle(ox + x, oy + y, 6, TFT_WHITE);
		canvas.fillCircle(ox + x, oy + y, 4, analysisRed());
		if (telemetryCircleCreated) s.pushSprite(TEL_CIRCLE_X, TEL_CIRCLE_Y);
	}

	void drawTelemetryTraces()
	{
		LGFX_Sprite &s = telemetryTraceSprite;
		if (!telemetryTraceCreated)
		{
			s.setColorDepth(8);
			telemetryTraceCreated = s.createSprite(TEL_TRACE_W, TEL_TRACE_H) != nullptr;
		}
		lgfx::LovyanGFX &canvas = telemetryTraceCreated ? static_cast<lgfx::LovyanGFX &>(s)
			: static_cast<lgfx::LovyanGFX &>(tft);
		const int ox = telemetryTraceCreated ? 0 : TEL_TRACE_X;
		const int oy = telemetryTraceCreated ? 0 : TEL_TRACE_Y;
		canvas.fillRect(ox, oy, TEL_TRACE_W, TEL_TRACE_H, TFT_BLACK);
		const uint16_t grid = tft.color565(40, 44, 50);
		for (int q = 0; q <= 4; ++q)
			canvas.drawFastHLine(ox, oy + 2 + (TEL_TRACE_H - 5) * q / 4, TEL_TRACE_W, grid);
		for (int sec = 1; sec <= 7; ++sec)
			canvas.drawFastVLine(ox + TEL_TRACE_W - 1 - sec * 20, oy, TEL_TRACE_H, grid);

		const int samples = min(lapAnalysis.traceCount(), static_cast<int>(TEL_TRACE_W));
		const uint16_t green = analysisGreen(), red = analysisRed();
		int lastThrottle = -1, lastBrake = -1;
		for (int age = samples - 1; age >= 0; --age)
		{
			const int x = ox + TEL_TRACE_W - 1 - age;
			const int throttle = oy + 2 + (TEL_TRACE_H - 5) * (100 - lapAnalysis.traceThrottle(age)) / 100;
			const int brake = oy + 2 + (TEL_TRACE_H - 5) * (100 - lapAnalysis.traceBrake(age)) / 100;
			if (lastThrottle >= 0)
			{
				canvas.drawLine(x - 1, lastThrottle, x, throttle, green);
				canvas.drawLine(x - 1, lastBrake, x, brake, red);
				canvas.drawLine(x - 1, lastThrottle + 1, x, throttle + 1, green);
				canvas.drawLine(x - 1, lastBrake + 1, x, brake + 1, red);
			}
			lastThrottle = throttle;
			lastBrake = brake;
		}
		if (telemetryTraceCreated) s.pushSprite(TEL_TRACE_X, TEL_TRACE_Y);
	}

	void drawTelemetryStaticLayout()
	{
		tft.fillScreen(TFT_BLACK);
		tft.setFont(&fonts::Font0);
		tft.setTextSize(1.0f);
		tft.setTextDatum(TL_DATUM);
		tft.setTextColor(analysisMuted(), TFT_BLACK);
		tft.setTextDatum(TR_DATUM);
		tft.drawString("7 s", TEL_TRACE_X + TEL_TRACE_W, TEL_TRACE_Y - 10);
		tft.setTextDatum(TL_DATUM);
		const char *labels[4] = {"G LATERALE", "G LONGIT.", "MAX LAT.", "MAX FRENATA"};
		for (int i = 0; i < 4; ++i) tft.drawString(labels[i], 6 + i * 80, 200);
		tft.drawString("KM/H", 92, 8);
		tft.drawFastHLine(0, 194, SCREEN_WIDTH, analysisLine());
		telemetryLastCircle = telemetryLastTrace = 0;
	}

	static String telemetryG(float g)
	{
		char buffer[10];
		snprintf(buffer, sizeof(buffer), "%.2f", fabsf(g) < 0.005f ? 0.0f : g);
		return String(buffer);
	}

	void drawTelemetryDashboard(const DashboardState &state, bool forceUpdate)
	{
		if (forceUpdate) drawTelemetryStaticLayout();

		drawAnalysisGear("telGear", state.gear, 4, 2, 34, 38, TFT_WHITE, TFT_BLACK, forceUpdate);
		drawAnalysisText("telSpeed", state.speed, 88, 22, 48, MR_DATUM, &fonts::Font4,
			TFT_WHITE, TFT_BLACK, forceUpdate);
		const uint16_t zones[3] = {tft.color565(0, 210, 70), tft.color565(255, 200, 0), tft.color565(255, 30, 30)};
		drawShiftLights(state, 12, 124, 6, 12, 12, 16, true, zones, tft.color565(255, 30, 30), forceUpdate);
		drawDeltaBox("telDelta", 160, 158, 156, 32, state.sessionBestLiveDeltaSeconds,
			&fonts::FreeSansBold12pt7b, forceUpdate);

		const uint32_t now = millis();
		if (forceUpdate || now - telemetryLastCircle >= 66)
		{
			telemetryLastCircle = now;
			drawTelemetryCircle();
		}
		if (forceUpdate || now - telemetryLastTrace >= 100)
		{
			telemetryLastTrace = now;
			drawTelemetryTraces();
		}
		releaseSpriteAfterThemePreview(telemetryCircleSprite, telemetryCircleCreated);
		releaseSpriteAfterThemePreview(telemetryTraceSprite, telemetryTraceCreated);

		const float lat = lapAnalysis.lateralG(), lon = lapAnalysis.longitudinalG();
		drawAnalysisText("telLat", telemetryG(fabsf(lat)), 6, 222, 72, ML_DATUM, &fonts::Font4,
			TFT_WHITE, TFT_BLACK, forceUpdate);
		drawAnalysisText("telLong", telemetryG(lon), 86, 222, 72, ML_DATUM, &fonts::Font4,
			lon < -0.05f ? analysisRed() : lon > 0.05f ? analysisGreen() : TFT_WHITE, TFT_BLACK, forceUpdate);
		drawAnalysisText("telPeakLat", telemetryG(lapAnalysis.peakLateralG()), 166, 222, 72, ML_DATUM,
			&fonts::Font4, analysisYellow(), TFT_BLACK, forceUpdate);
		drawAnalysisText("telPeakBrake", telemetryG(lapAnalysis.peakBrakingG()), 246, 222, 72, ML_DATUM,
			&fonts::Font4, analysisYellow(), TFT_BLACK, forceUpdate);
		// Pedals now, as a legend of the traces.
		drawAnalysisText("telThrottle", String("GAS ") + state.tcLevel + "%", TEL_TRACE_X, TEL_TRACE_Y - 6,
			54, ML_DATUM, &fonts::Font0, analysisGreen(), TFT_BLACK, forceUpdate);
		drawAnalysisText("telBrake", String("FRENO ") + state.absLevel + "%", TEL_TRACE_X + 60, TEL_TRACE_Y - 6,
			66, ML_DATUM, &fonts::Font0, analysisRed(), TFT_BLACK, forceUpdate);
		restoreDefaultText();
	}
