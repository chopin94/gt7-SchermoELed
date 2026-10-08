	// ADERENZA: the four wheels at a glance. Each corner shows its tyre
	// temperature, how much the wheel slips against the ground (a lock-up
	// under braking turns it red, wheelspin under power orange) and the load on
	// its suspension. The car in the middle lights the wheel that slips, the
	// pedal bars on the sides show what the driver is doing, the strip at the
	// bottom counts the lock-ups and wheelspins of the lap.
	enum : int
	{
		GRIP_TILE_W = 96,
		GRIP_TILE_H = 70,
		GRIP_LEFT_X = 4,
		GRIP_RIGHT_X = 220,
		GRIP_FRONT_Y = 46,
		GRIP_REAR_Y = 122,
		GRIP_BAR_W = 86,
		GRIP_PEDAL_TOP = 54,
		GRIP_PEDAL_H = 134,
	};

	LGFX_Sprite gripTileSprite{&tft};
	bool gripTileCreated = false;
	uint32_t gripLastTiles = 0;

	void releaseGripSprites()
	{
		if (gripTileCreated) { gripTileSprite.deleteSprite(); gripTileCreated = false; }
	}

	// What the screen shows, taken from the live analysis or, in the theme
	// selector, from a fixed example.
	struct GripView
	{
		float slip[GripAnalysis::WHEELS];
		GripAnalysis::WheelState state[GripAnalysis::WHEELS];
		float temperature[GripAnalysis::WHEELS];
		float load[GripAnalysis::WHEELS];
		int locks, spins, lastLocks, lastSpins;
		float totalG, usage;
		bool hasWheels;
	};

	void fillGripView(GripView &view, const DashboardState &state)
	{
		if (settingsScreen == SettingsScreen::ThemeSelection)
		{
			// Fixed example for the selector: front left locking under braking.
			const float slip[4] = {-0.21f, -0.04f, -0.02f, 0.03f};
			const float load[4] = {0.8f, 0.55f, -0.3f, -0.2f};
			for (int i = 0; i < 4; ++i)
			{
				view.slip[i] = slip[i];
				view.state[i] = i == 0 ? GripAnalysis::WheelState::Locking : GripAnalysis::WheelState::Grip;
				view.temperature[i] = state.tyreTemperatures[i];
				view.load[i] = load[i];
			}
			view.locks = 1; view.spins = 0; view.lastLocks = 2; view.lastSpins = 1;
			view.totalG = 1.31f; view.usage = 0.93f; view.hasWheels = true;
			return;
		}
		view.hasWheels = gripAnalysis.wheelDataAvailable();
		for (int i = 0; i < 4; ++i)
		{
			view.slip[i] = gripAnalysis.slip(i);
			view.state[i] = gripAnalysis.state(i);
			view.temperature[i] = gripAnalysis.temperature(i);
			view.load[i] = gripAnalysis.suspensionLoad(i);
		}
		view.locks = gripAnalysis.lockEvents();
		view.spins = gripAnalysis.spinEvents();
		view.lastLocks = gripAnalysis.lastLapLockEvents();
		view.lastSpins = gripAnalysis.lastLapSpinEvents();
		view.totalG = gripAnalysis.totalG();
		view.usage = gripAnalysis.gripUsage();
	}

	uint16_t gripStateColor(GripAnalysis::WheelState wheelState)
	{
		switch (wheelState)
		{
		case GripAnalysis::WheelState::Locking: return analysisRed();
		case GripAnalysis::WheelState::Spinning: return tft.color565(255, 140, 0);
		default: return tft.color565(0, 170, 100);
		}
	}

	// Slip bar: zero in the middle, lock-ups to the left, wheelspin to the
	// right, the shaded ends start where a wheel counts as locked or spinning.
	void drawGripSlipBar(lgfx::LovyanGFX &canvas, int x, int y, int w, int h, float slip,
		GripAnalysis::WheelState wheelState)
	{
		const int half = w / 2;
		const int center = x + half;
		canvas.fillRect(x, y, w, h, tft.color565(26, 28, 32));
		const int lockEdge = static_cast<int>(lroundf(-GripAnalysis::LOCK_SLIP / GripAnalysis::DISPLAY_SLIP * half));
		const int spinEdge = static_cast<int>(lroundf(GripAnalysis::SPIN_SLIP / GripAnalysis::DISPLAY_SLIP * half));
		canvas.fillRect(x, y, half - lockEdge, h, tft.color565(58, 22, 24));
		canvas.fillRect(center + spinEdge, y, half - spinEdge, h, tft.color565(66, 40, 10));
		if (isfinite(slip))
		{
			float clipped = slip / GripAnalysis::DISPLAY_SLIP;
			if (clipped > 1.0f) clipped = 1.0f;
			if (clipped < -1.0f) clipped = -1.0f;
			const int length = static_cast<int>(lroundf(fabsf(clipped) * half));
			const uint16_t color = wheelState == GripAnalysis::WheelState::Grip
				|| wheelState == GripAnalysis::WheelState::Unknown
				? tft.color565(0, 170, 100) : gripStateColor(wheelState);
			if (length > 0)
			{
				if (clipped < 0) canvas.fillRect(center - length, y + 1, length, h - 2, color);
				else canvas.fillRect(center, y + 1, length, h - 2, color);
			}
		}
		canvas.drawFastVLine(center, y - 1, h + 2, TFT_WHITE);
	}

	// Load of a corner: a thin bar from the middle, to the right when loaded.
	void drawGripLoadBar(lgfx::LovyanGFX &canvas, int x, int y, int w, int h, float load)
	{
		const int half = w / 2;
		canvas.fillRect(x, y, w, h, tft.color565(26, 28, 32));
		const int length = static_cast<int>(lroundf(fabsf(load) * half));
		if (length > 0)
		{
			if (load > 0) canvas.fillRect(x + half, y, length, h, tft.color565(70, 140, 255));
			else canvas.fillRect(x + half - length, y, length, h, tft.color565(120, 100, 170));
		}
		canvas.drawFastVLine(x + half, y - 1, h + 2, tft.color565(150, 156, 166));
	}

	void drawGripTile(int wheel, const GripView &view, int x, int y, bool forceUpdate)
	{
		static const char *const names[4] = {"ANT SX", "ANT DX", "POST SX", "POST DX"};
		const GripAnalysis::WheelState wheelState = view.state[wheel];
		const bool active = wheelState == GripAnalysis::WheelState::Locking
			|| wheelState == GripAnalysis::WheelState::Spinning;
		const int slipPercent = isfinite(view.slip[wheel])
			? static_cast<int>(lroundf(view.slip[wheel] * 100.0f)) : 999;
		const int tempC = isfinite(view.temperature[wheel])
			? static_cast<int>(lroundf(view.temperature[wheel])) : -999;
		const int loadStep = static_cast<int>(lroundf(view.load[wheel] * 20.0f));
		// Position of the bar end in pixels: the bar is redrawn only when it moves.
		const int slipStep = isfinite(view.slip[wheel])
			? static_cast<int>(lroundf(constrain(view.slip[wheel] / GripAnalysis::DISPLAY_SLIP, -1.0f, 1.0f) * (GRIP_BAR_W / 2)))
			: 999;
		const String key = String(wheel) + "|" + String(static_cast<int>(wheelState)) + "|" +
			String(slipPercent) + "|" + String(slipStep) + "|" + String(tempC) + "|" + String(loadStep);
		const String cacheId = String("gripTile") + String(wheel);
		String &previous = prevData[cacheId];
		if (!forceUpdate && previous == key) return;
		previous = key;

		LGFX_Sprite &sprite = gripTileSprite;
		if (!gripTileCreated)
		{
			sprite.setColorDepth(8);
			gripTileCreated = sprite.createSprite(GRIP_TILE_W, GRIP_TILE_H) != nullptr;
		}
		lgfx::LovyanGFX &canvas = gripTileCreated ? static_cast<lgfx::LovyanGFX &>(sprite)
			: static_cast<lgfx::LovyanGFX &>(tft);
		const int ox = gripTileCreated ? 0 : x;
		const int oy = gripTileCreated ? 0 : y;

		const uint16_t panel = tft.color565(14, 16, 20);
		canvas.fillRect(ox, oy, GRIP_TILE_W, GRIP_TILE_H, TFT_BLACK);
		canvas.fillRoundRect(ox, oy, GRIP_TILE_W, GRIP_TILE_H, 5, panel);
		canvas.drawRoundRect(ox, oy, GRIP_TILE_W, GRIP_TILE_H, 5, active ? gripStateColor(wheelState) : analysisLine());
		if (active) canvas.drawRoundRect(ox + 1, oy + 1, GRIP_TILE_W - 2, GRIP_TILE_H - 2, 4, gripStateColor(wheelState));

		canvas.setFont(&fonts::Font0);
		canvas.setTextSize(1.0f);
		canvas.setTextDatum(TL_DATUM);
		canvas.setTextColor(analysisMuted(), panel);
		canvas.drawString(names[wheel], ox + 6, oy + 6);
		if (active)
		{
			canvas.setTextDatum(TR_DATUM);
			canvas.setTextColor(gripStateColor(wheelState), panel);
			canvas.drawString(wheelState == GripAnalysis::WheelState::Locking ? "BLOCCA" : "PATT.",
				ox + GRIP_TILE_W - 6, oy + 6);
		}

		// Tyre temperature.
		canvas.setFont(&fonts::Font4);
		canvas.setTextDatum(TL_DATUM);
		if (tempC > -999)
		{
			canvas.setTextColor(analysisTyreColor(view.temperature[wheel]), panel);
			canvas.drawString(String(tempC), ox + 6, oy + 17);
			canvas.setFont(&fonts::Font2);
			canvas.setTextColor(analysisMuted(), panel);
			canvas.drawString("C", ox + 6 + (tempC >= 100 ? 50 : 36), oy + 20);
		}
		else
		{
			canvas.setTextColor(analysisMuted(), panel);
			canvas.drawString("--", ox + 6, oy + 17);
		}
		// Slip in percent, signed.
		canvas.setFont(&fonts::Font2);
		canvas.setTextDatum(TR_DATUM);
		if (slipPercent != 999)
		{
			char buffer[10];
			snprintf(buffer, sizeof(buffer), "%+d%%", slipPercent);
			canvas.setTextColor(active ? gripStateColor(wheelState) : TFT_WHITE, panel);
			canvas.drawString(buffer, ox + GRIP_TILE_W - 6, oy + 22);
		}
		else
		{
			canvas.setTextColor(analysisMuted(), panel);
			canvas.drawString("--", ox + GRIP_TILE_W - 6, oy + 22);
		}

		drawGripSlipBar(canvas, ox + 5, oy + 47, GRIP_BAR_W, 9, view.slip[wheel], wheelState);
		drawGripLoadBar(canvas, ox + 5, oy + 61, GRIP_BAR_W, 4, view.load[wheel]);
		if (gripTileCreated) sprite.pushSprite(x, y);
	}

	// The car seen from above; the wheel that slips lights up.
	void drawGripCar(const GripView &view, bool forceUpdate)
	{
		String key = view.hasWheels ? "w" : "n";
		for (int i = 0; i < 4; ++i) key += String(static_cast<int>(view.state[i])) + ",";
		String &previous = prevData["gripCar"];
		if (!forceUpdate && previous == key) return;
		previous = key;

		const int cx = 160;
		tft.fillRect(122, 50, 76, 142, TFT_BLACK);
		// Body.
		tft.fillRoundRect(cx - 17, 56, 34, 130, 12, tft.color565(34, 38, 44));
		tft.drawRoundRect(cx - 17, 56, 34, 130, 12, tft.color565(84, 90, 100));
		tft.fillRoundRect(cx - 11, 86, 22, 40, 6, tft.color565(16, 18, 22));       // cabin
		tft.drawFastHLine(cx - 17, 74, 34, tft.color565(60, 64, 72));               // front axle
		tft.drawFastHLine(cx - 17, 166, 34, tft.color565(60, 64, 72));              // rear axle
		const int wheelX[4] = {cx - 33, cx + 21, cx - 33, cx + 21};
		const int wheelY[4] = {60, 60, 138, 138};
		for (int i = 0; i < 4; ++i)
		{
			uint16_t color = tft.color565(70, 74, 82);
			const GripAnalysis::WheelState wheelState = view.state[i];
			if (wheelState == GripAnalysis::WheelState::Unknown) color = tft.color565(40, 43, 48);
			else if (wheelState != GripAnalysis::WheelState::Grip) color = gripStateColor(wheelState);
			tft.fillRoundRect(wheelX[i], wheelY[i], 12, 34, 3, color);
		}
		const uint16_t axle = tft.color565(60, 64, 72);
		tft.drawFastHLine(cx - 21, 77, 4, axle);
		tft.drawFastHLine(cx + 17, 77, 4, axle);
		tft.drawFastHLine(cx - 21, 155, 4, axle);
		tft.drawFastHLine(cx + 17, 155, 4, axle);
		if (!view.hasWheels)
		{
			tft.setFont(&fonts::Font0);
			tft.setTextSize(1.0f);
			tft.setTextDatum(MC_DATUM);
			tft.setTextColor(analysisMuted(), tft.color565(16, 18, 22));
			tft.drawString("NO DATI", cx, 98);
			tft.drawString("RUOTE", cx, 110);
		}
	}

	// Vertical pedal bar, redrawn only when the value changes.
	void drawGripPedal(const char *id, int x, int percent, uint16_t color, const char *label, bool forceUpdate)
	{
		const String key = String(percent / 2) + "|" + String(color);
		String &previous = prevData[id];
		if (!forceUpdate && previous == key) return;
		previous = key;
		tft.fillRect(x, GRIP_PEDAL_TOP, 12, GRIP_PEDAL_H, tft.color565(26, 28, 32));
		const int filled = constrain(percent, 0, 100) * (GRIP_PEDAL_H - 2) / 100;
		if (filled > 0) tft.fillRect(x + 1, GRIP_PEDAL_TOP + GRIP_PEDAL_H - 1 - filled, 10, filled, color);
		tft.drawRect(x - 1, GRIP_PEDAL_TOP - 1, 14, GRIP_PEDAL_H + 2, analysisLine());
		tft.setFont(&fonts::Font0);
		tft.setTextSize(1.0f);
		tft.setTextDatum(TC_DATUM);
		tft.setTextColor(color, TFT_BLACK);
		tft.drawString(label, x + 6, GRIP_PEDAL_TOP - 9);
	}

	void drawGripStaticLayout()
	{
		tft.fillScreen(TFT_BLACK);
		tft.setFont(&fonts::Font0);
		tft.setTextSize(1.0f);
		tft.setTextDatum(TL_DATUM);
		tft.setTextColor(analysisMuted(), TFT_BLACK);
		const char *labels[4] = {"BLOCCHI (PR.)", "PATTIN. (PR.)", "G TOTALE", "USO GRIP"};
		for (int i = 0; i < 4; ++i) tft.drawString(labels[i], 6 + i * 80, 200);
		tft.drawString("KM/H", 92, 8);
		tft.drawFastHLine(0, 194, SCREEN_WIDTH, analysisLine());
		gripLastTiles = 0;
	}

	void drawGripDashboard(const DashboardState &state, bool forceUpdate)
	{
		if (forceUpdate) drawGripStaticLayout();

		drawAnalysisGear("gripGear", state.gear, 4, 2, 34, 38, TFT_WHITE, TFT_BLACK, forceUpdate);
		drawAnalysisText("gripSpeed", state.speed, 88, 22, 48, MR_DATUM, &fonts::Font4,
			TFT_WHITE, TFT_BLACK, forceUpdate);
		const uint16_t zones[3] = {tft.color565(0, 210, 70), tft.color565(255, 200, 0), tft.color565(255, 30, 30)};
		drawShiftLights(state, 12, 124, 6, 12, 12, 16, true, zones, tft.color565(255, 30, 30), forceUpdate);

		GripView view;
		fillGripView(view, state);

		const uint32_t now = millis();
		if (forceUpdate || now - gripLastTiles >= 50)
		{
			gripLastTiles = now;
			drawGripTile(0, view, GRIP_LEFT_X, GRIP_FRONT_Y, forceUpdate);
			drawGripTile(1, view, GRIP_RIGHT_X, GRIP_FRONT_Y, forceUpdate);
			drawGripTile(2, view, GRIP_LEFT_X, GRIP_REAR_Y, forceUpdate);
			drawGripTile(3, view, GRIP_RIGHT_X, GRIP_REAR_Y, forceUpdate);
		}
		releaseSpriteAfterThemePreview(gripTileSprite, gripTileCreated);
		drawGripCar(view, forceUpdate);
		drawGripPedal("gripBrake", 108, state.absLevel.toInt(), analysisRed(), "F", forceUpdate);
		drawGripPedal("gripThrottle", 200, state.tcLevel.toInt(), analysisGreen(), "G", forceUpdate);

		// Counters of the lap, with the previous lap in grey.
		const auto counter = [&](const char *id, int x, int value, int previousLap, uint16_t color)
		{
			drawAnalysisText(id, String(value), x, 222, 36, ML_DATUM, &fonts::Font4,
				value > 0 ? color : TFT_WHITE, TFT_BLACK, forceUpdate);
			drawAnalysisText((String(id) + "P").c_str(), String("(") + previousLap + ")", x + 38, 224, 34,
				ML_DATUM, &fonts::Font2, analysisMuted(), TFT_BLACK, forceUpdate);
		};
		counter("gripLocks", 6, view.locks, view.lastLocks, analysisRed());
		counter("gripSpins", 86, view.spins, view.lastSpins, tft.color565(255, 140, 0));
		char buffer[10];
		snprintf(buffer, sizeof(buffer), "%.2f", view.totalG);
		drawAnalysisText("gripG", String(buffer), 166, 222, 72, ML_DATUM, &fonts::Font4,
			TFT_WHITE, TFT_BLACK, forceUpdate);
		const int usage = static_cast<int>(lroundf(constrain(view.usage, 0.0f, 1.2f) * 100.0f));
		drawAnalysisText("gripUsage", String(usage) + "%", 246, 222, 72, ML_DATUM, &fonts::Font4,
			usage >= 95 ? analysisYellow() : TFT_WHITE, TFT_BLACK, forceUpdate);
		restoreDefaultText();
	}
