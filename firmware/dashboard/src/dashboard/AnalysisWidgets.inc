	// Widgets shared by the themes built on the lap analysis (FORMULA, MAPPA
	// PISTA, TELEMETRIA, PRESTAZIONI), the notifications and the session summary.

	uint16_t analysisPurple() { return tft.color565(176, 48, 255); }
	uint16_t analysisGreen() { return tft.color565(0, 200, 80); }
	uint16_t analysisYellow() { return tft.color565(255, 210, 0); }
	uint16_t analysisRed() { return tft.color565(226, 30, 36); }
	uint16_t analysisPanel() { return tft.color565(22, 24, 28); }
	uint16_t analysisLine() { return tft.color565(58, 62, 70); }
	uint16_t analysisMuted() { return tft.color565(128, 134, 142); }

	// The other themes may rely on LovyanGFX's default text settings: put them
	// back after drawing a screen of these themes.
	void restoreDefaultText()
	{
		tft.setFont(&fonts::Font0);
		tft.setTextSize(1.0f);
		tft.setTextDatum(TL_DATUM);
		tft.setTextPadding(0);
	}

	uint16_t sectorColor(LapAnalysis::SectorState state)
	{
		switch (state)
		{
		case LapAnalysis::SectorState::Purple: return analysisPurple();
		case LapAnalysis::SectorState::Green: return analysisGreen();
		case LapAnalysis::SectorState::Yellow: return analysisYellow();
		case LapAnalysis::SectorState::Running: return tft.color565(232, 234, 238);
		default: return tft.color565(48, 52, 58);
		}
	}

	// Sector shown on the map and in the sector bars: the current lap where it
	// is already timed, otherwise the previous lap.
	LapAnalysis::SectorState displaySectorState(int i)
	{
		const LapAnalysis::SectorState current = lapAnalysis.sectorState(i);
		if (current != LapAnalysis::SectorState::None) return current;
		return lapAnalysis.lastLapSectorState(i);
	}

	static String analysisLapTime(int32_t ms, bool tenths = false)
	{
		char buffer[12];
		LapAnalysis::formatLapTime(ms, buffer, sizeof(buffer), tenths);
		return String(buffer);
	}

	// "01:24.631" (protocol format) -> "1:24.631"
	static String compactLapTime(const String &time)
	{
		if (time.length() >= 9 && time[0] == '0' && time[2] == ':') return time.substring(1);
		return time;
	}

	uint16_t deltaFillColor(const String &delta)
	{
		const int sign = deltaSign(delta);
		return sign < 0 ? tft.color565(0, 150, 64) : sign > 0 ? tft.color565(196, 28, 34)
			: tft.color565(54, 58, 66);
	}

	// Text drawn over its own background, padded to `width`, only when it
	// changed: no flicker and no leftover pixels from longer values.
	void drawAnalysisText(const char *id, const String &text, int x, int y, int width,
		uint8_t datum, const lgfx::IFont *font, uint16_t color, uint16_t background,
		bool forceUpdate, float scale = 1.0f)
	{
		const String cache = text + "|" + String(color) + "|" + String(background);
		String &previous = prevData[id];
		if (!forceUpdate && previous == cache) return;
		previous = cache;
		tft.setFont(font);
		tft.setTextSize(scale);
		tft.setTextDatum(datum);
		tft.setTextColor(color, background);
		tft.setTextPadding(width);
		tft.drawString(text, x, y);
		tft.setTextPadding(0);
		tft.setTextSize(1.0f);
	}

	// Coloured box with the live delta: green ahead, red behind, grey unknown.
	void drawDeltaBox(const char *id, int x, int y, int w, int h, const String &delta,
		const lgfx::IFont *font, bool forceUpdate, float scale = 1.0f)
	{
		const uint16_t fill = deltaFillColor(delta);
		const String fillKey = String(id) + "F";
		const bool refill = forceUpdate || prevColor.count(fillKey) == 0 ||
			prevColor[fillKey] != fill;
		if (refill)
		{
			tft.fillRoundRect(x, y, w, h, 6, fill);
			prevColor[fillKey] = fill;
		}
		drawAnalysisText(id, delta, x + w / 2, y + h / 2, w - 12, MC_DATUM, font,
			TFT_WHITE, fill, refill || forceUpdate, scale);
	}

	// Three sector segments; the running one is white, the others coloured by
	// their time (purple best, green better than the best lap, yellow slower).
	void drawSectorBar(const char *id, int x, int y, int w, int h, int gap, bool withTimes,
		bool forceUpdate)
	{
		const int segment = (w - 2 * gap) / LapAnalysis::SECTORS;
		for (int i = 0; i < LapAnalysis::SECTORS; ++i)
		{
			const LapAnalysis::SectorState state = displaySectorState(i);
			const int32_t current = lapAnalysis.sectorMs(i);
			const int32_t shown = current > 0 ? current : (state != LapAnalysis::SectorState::Running
				? lapAnalysis.lastLapSectorState(i) != LapAnalysis::SectorState::None
					? lastLapSectorMs(i) : -1
				: -1);
			const uint16_t fill = sectorColor(state);
			const String cache = String(static_cast<int>(state)) + "|" + String(shown);
			const String key = String(id) + String(i);
			if (!forceUpdate && prevData[key] == cache) continue;
			prevData[key] = cache;
			const int sx = x + i * (segment + gap);
			tft.fillRoundRect(sx, y, segment, h, 3, fill);
			if (!withTimes) continue;
			const bool dark = state == LapAnalysis::SectorState::Yellow ||
				state == LapAnalysis::SectorState::Running;
			const uint16_t ink = dark ? TFT_BLACK : TFT_WHITE;
			tft.setFont(&fonts::Font2);
			tft.setTextSize(1.0f);
			tft.setTextColor(ink, fill);
			tft.setTextDatum(ML_DATUM);
			tft.drawString(String("S") + (i + 1), sx + 6, y + h / 2);
			if (shown > 0)
			{
				tft.setTextDatum(MR_DATUM);
				tft.drawString(String(shown / 1000) + "." + String((shown % 1000) / 100), sx + segment - 6, y + h / 2);
			}
		}
	}

	int32_t lastLapSectorMs(int i)
	{
		return lapAnalysis.storedLaps() > 0 ? lapAnalysis.lapRecord(0).sectorMs[i] : -1;
	}

	// Tyre temperature colour: cold blue, working green, hot orange/red.
	uint16_t analysisTyreColor(float celsius)
	{
		if (!isfinite(celsius)) return tft.color565(48, 52, 58);
		if (celsius < 60) return tft.color565(40, 110, 230);
		if (celsius < 75) return tft.color565(40, 180, 210);
		if (celsius < 95) return tft.color565(0, 190, 80);
		if (celsius < 105) return tft.color565(255, 170, 0);
		return tft.color565(226, 30, 36);
	}

	// Shift lights: `count` LEDs in three colour zones over the last part of
	// the rev range, all flashing at the limiter. Rectangular or round.
	void drawShiftLights(const DashboardState &state, int count, int x, int y, int w, int h,
		int pitch, bool round, const uint16_t zones[3], uint16_t limiterColor, bool forceUpdate)
	{
		static uint16_t shown[24] = {};
		static int shownCount = 0;
		if (shownCount != count) { forceUpdate = true; shownCount = count; }
		const uint16_t off = tft.color565(28, 30, 34);
		const int start = state.rpmAlertRangeValid
			? constrain(state.rpmRedLineSetting - 25, 30, 90) : 65;
		int lit = 0;
		if (state.rpmPercent > start)
			lit = constrain(((state.rpmPercent - start) * count + (100 - start) / 2) / (100 - start), 0, count);
		const bool limiter = state.revLimitAlertActive ||
			(state.rpmAlertRangeValid && state.rpmPercent >= 100);
		const bool flashOn = (millis() / 70) % 2 == 0;
		const int zone = (count + 2) / 3;
		for (int i = 0; i < count && i < 24; ++i)
		{
			uint16_t color = off;
			if (limiter) color = flashOn ? limiterColor : off;
			else if (i < lit) color = zones[min(i / zone, 2)];
			if (!forceUpdate && shown[i] == color) continue;
			shown[i] = color;
			const int lx = x + i * pitch;
			if (round)
				tft.fillCircle(lx + w / 2, y + h / 2, w / 2, color);
			else
				tft.fillRoundRect(lx, y, w, h, 2, color);
		}
	}

	// Gear drawn with the angular racing glyphs of the Ferrari themes.
	void drawAnalysisGear(const char *id, const String &gear, int x, int y, int w, int h,
		uint16_t color, uint16_t background, bool forceUpdate)
	{
		String &previous = prevData[id];
		if (!forceUpdate && previous == gear) return;
		previous = gear;
		tft.fillRect(x, y, w, h, background);
		const int chars = constrain(static_cast<int>(gear.length()), 1, 2);
		const float gh = h * 0.92f;
		const float gw = chars > 1 ? w * 0.36f : w * 0.6f;
		const float stroke = chars > 1 ? gw * 0.3f : gw * 0.3f;
		const float slant = 0.12f;
		const float gap = w * 0.06f;
		const float total = chars * gw + (chars - 1) * gap + gh * slant;
		float gx = x + (w - total) / 2.0f;
		for (int i = 0; i < chars && i < static_cast<int>(gear.length()); ++i)
		{
			drawFerrariGlyph(tft, gear[i], gx, y + (h - gh) / 2.0f, gw, gh, stroke, slant, color);
			gx += gw + gap;
		}
	}
