	// Ferrari themes for a sim rig: LED rev lights across the top (green,
	// yellow, red, all flashing red at the limiter), a large gear on a Giallo
	// Modena panel with the speed below, segmented bar gauges with icons and a
	// row of lamps. FERRARI shows brake, throttle, turbo and fuel with ABS, TCS,
	// ASM and handbrake lamps; FERRARI AC (Assetto Corsa) shows brake, throttle,
	// clutch and lap progress with ABS, TC and pit lamps.
	enum : int
	{
		FERRARI_LED_COUNT = 15,
		FERRARI_LED_Y = 13,
		FERRARI_LED_PITCH = 21,
		FERRARI_BAR_W = 28,
		FERRARI_BAR_TOP = 36,
		FERRARI_BAR_SEGMENTS = 17,
		FERRARI_BAR_PITCH = 8,
		FERRARI_BAR_SEGMENT_H = 6,
		FERRARI_ICON_Y = 188,
		FERRARI_BRAKE_X = 8,
		FERRARI_THROTTLE_X = 44,
		// Right side: a full-width bar next to the gear panel (turbo / clutch)
		// and a narrower secondary bar towards the screen edge (fuel / lap).
		FERRARI_RIGHT_BAR_X = 248,
		FERRARI_EDGE_BAR_X = 291,
		FERRARI_EDGE_BAR_W = 16,
		FERRARI_PANEL_X = 86,
		FERRARI_PANEL_Y = 34,
		FERRARI_PANEL_W = 148,
		FERRARI_PANEL_H = 114,
		FERRARI_GEAR_X = 115,
		FERRARI_GEAR_Y = 42,
		FERRARI_GEAR_W = 90,
		FERRARI_GEAR_H = 100,
		FERRARI_SPEED_X = 98,
		FERRARI_SPEED_Y = 154,
		FERRARI_SPEED_W = 96,
		FERRARI_SPEED_H = 44,
		FERRARI_LAMP_Y = 206,
		FERRARI_LAMP_H = 32,
		FERRARI_LAMP_ROW_W = 316,
	};

	uint16_t ferrariYellow() { return tft.color565(255, 204, 0); }
	uint16_t ferrariRed() { return tft.color565(220, 16, 16); }

	// ---- Racing glyphs ---------------------------------------------------------
	// Angular digits and N/R drawn as thick strokes on a unit grid (x and y
	// from 0 to 1, y downwards), so the gear stays sharp at any size.
	struct FerrariStroke
	{
		float x0, y0, x1, y1;
	};

	static const FerrariStroke *ferrariGlyph(char c, uint8_t &count)
	{
		static const FerrariStroke zero[] = {{0, 0, 1, 0}, {1, 0, 1, 1}, {1, 1, 0, 1}, {0, 1, 0, 0}};
		static const FerrariStroke one[] = {{0.6f, 0, 0.6f, 1}, {0.15f, 0.22f, 0.6f, 0}};
		static const FerrariStroke two[] = {{0, 0, 1, 0}, {1, 0, 1, 0.5f}, {1, 0.5f, 0, 0.5f},
			{0, 0.5f, 0, 1}, {0, 1, 1, 1}};
		static const FerrariStroke three[] = {{0, 0, 1, 0}, {1, 0, 1, 1}, {1, 1, 0, 1}, {0, 0.5f, 1, 0.5f}};
		static const FerrariStroke four[] = {{0, 0, 0, 0.5f}, {0, 0.5f, 1, 0.5f}, {1, 0, 1, 1}};
		static const FerrariStroke five[] = {{1, 0, 0, 0}, {0, 0, 0, 0.5f}, {0, 0.5f, 1, 0.5f},
			{1, 0.5f, 1, 1}, {1, 1, 0, 1}};
		static const FerrariStroke six[] = {{1, 0, 0, 0}, {0, 0, 0, 1}, {0, 1, 1, 1},
			{1, 1, 1, 0.5f}, {1, 0.5f, 0, 0.5f}};
		static const FerrariStroke seven[] = {{0, 0, 1, 0}, {1, 0, 1, 1}};
		static const FerrariStroke eight[] = {{0, 0, 1, 0}, {1, 0, 1, 1}, {1, 1, 0, 1},
			{0, 1, 0, 0}, {0, 0.5f, 1, 0.5f}};
		static const FerrariStroke nine[] = {{1, 0.5f, 0, 0.5f}, {0, 0.5f, 0, 0}, {0, 0, 1, 0},
			{1, 0, 1, 1}, {1, 1, 0, 1}};
		static const FerrariStroke letterN[] = {{0, 1, 0, 0}, {0, 0, 1, 1}, {1, 1, 1, 0}};
		static const FerrariStroke letterR[] = {{0, 1, 0, 0}, {0, 0, 1, 0}, {1, 0, 1, 0.5f},
			{1, 0.5f, 0, 0.5f}, {0.4f, 0.5f, 1, 1}};
		static const FerrariStroke dash[] = {{0.1f, 0.5f, 0.9f, 0.5f}};

#define FERRARI_GLYPH(table) \
	count = sizeof(table) / sizeof(table[0]); \
	return table
		switch (c)
		{
		case '0': FERRARI_GLYPH(zero);
		case '1': FERRARI_GLYPH(one);
		case '2': FERRARI_GLYPH(two);
		case '3': FERRARI_GLYPH(three);
		case '4': FERRARI_GLYPH(four);
		case '5': FERRARI_GLYPH(five);
		case '6': FERRARI_GLYPH(six);
		case '7': FERRARI_GLYPH(seven);
		case '8': FERRARI_GLYPH(eight);
		case '9': FERRARI_GLYPH(nine);
		case 'N': FERRARI_GLYPH(letterN);
		case 'R': FERRARI_GLYPH(letterR);
		case '-': FERRARI_GLYPH(dash);
		default:
			count = 0;
			return nullptr;
		}
#undef FERRARI_GLYPH
	}

	// Draws one glyph in the box (x, y, w, h), sheared to the right by
	// `slant` for a sporty italic.
	void drawFerrariGlyph(lgfx::LovyanGFX &canvas, char c, float x, float y,
		float w, float h, float stroke, float slant, uint16_t color)
	{
		uint8_t count = 0;
		const FerrariStroke *strokes = ferrariGlyph(c, count);
		const float half = stroke / 2.0f;
		const float bottom = y + h;
		auto corner = [&](float px, float py, int32_t &sx, int32_t &sy)
		{
			sx = lroundf(px + (bottom - py) * slant);
			sy = lroundf(py);
		};
		for (uint8_t i = 0; i < count; ++i)
		{
			const FerrariStroke &s = strokes[i];
			float ax = x + half + s.x0 * (w - stroke);
			float ay = y + half + s.y0 * (h - stroke);
			float bx = x + half + s.x1 * (w - stroke);
			float by = y + half + s.y1 * (h - stroke);
			float dx = bx - ax;
			float dy = by - ay;
			const float length = sqrtf(dx * dx + dy * dy);
			if (length < 0.01f) continue;
			dx /= length;
			dy /= length;
			// Square caps close the corners of straight strokes; diagonals end
			// flush inside the strokes they join.
			if (s.x0 == s.x1 || s.y0 == s.y1)
			{
				ax -= dx * half;
				ay -= dy * half;
				bx += dx * half;
				by += dy * half;
			}
			const float nx = -dy * half;
			const float ny = dx * half;
			int32_t x1, y1, x2, y2, x3, y3, x4, y4;
			corner(ax + nx, ay + ny, x1, y1);
			corner(ax - nx, ay - ny, x2, y2);
			corner(bx - nx, by - ny, x3, y3);
			corner(bx + nx, by + ny, x4, y4);
			canvas.fillTriangle(x1, y1, x2, y2, x3, y3, color);
			canvas.fillTriangle(x1, y1, x3, y3, x4, y4, color);
		}
	}

	// ---- Rev LEDs ----------------------------------------------------------------
	void drawFerrariRevLeds(const DashboardState &state, bool forceUpdate)
	{
		static uint16_t shown[FERRARI_LED_COUNT] = {};
		const uint16_t off = tft.color565(26, 26, 28);
		const uint16_t offRing = tft.color565(62, 62, 66);
		const uint16_t zoneColors[3] = {
			tft.color565(30, 220, 60), tft.color565(255, 200, 0), tft.color565(255, 28, 28)};

		// The LEDs cover the last part of the rev range up to GT7's max alert
		// RPM (the shift point): five green, five yellow, five red.
		const int start = state.rpmAlertRangeValid
			? constrain(state.rpmRedLineSetting - 25, 30, 90) : 65;
		int lit = 0;
		if (state.rpmPercent > start)
			lit = constrain(((state.rpmPercent - start) * FERRARI_LED_COUNT +
				(100 - start) / 2) / (100 - start), 0, FERRARI_LED_COUNT);
		const bool limiter = state.revLimitAlertActive ||
			(state.rpmAlertRangeValid && state.rpmPercent >= 100);
		const bool flashOn = (millis() / 70) % 2 == 0;

		for (int i = 0; i < FERRARI_LED_COUNT; ++i)
		{
			uint16_t color = off;
			if (limiter) color = flashOn ? zoneColors[2] : off;
			else if (i < lit) color = zoneColors[i / 5];
			if (!forceUpdate && shown[i] == color) continue;
			const int x = 13 + i * FERRARI_LED_PITCH;
			tft.fillCircle(x, FERRARI_LED_Y, 8, color);
			tft.drawCircle(x, FERRARI_LED_Y, 9,
				color == off ? offRing : blendRgb565(TFT_BLACK, color, 150));
			shown[i] = color;
		}
	}

	// ---- Bar gauges --------------------------------------------------------------
	// Segmented gauge filling from the bottom. Only segments whose colour
	// changed are redrawn, so fast inputs such as the pedals never flicker.
	void drawFerrariBar(int x, int width, int lit, uint16_t color,
		const String &id, bool forceUpdate)
	{
		lit = constrain(lit, 0, static_cast<int>(FERRARI_BAR_SEGMENTS));
		const String litKey = id + "N";
		const String colorKey = id + "C";
		const bool known = !forceUpdate && prevColor.count(litKey) && prevColor.count(colorKey);
		const int previousLit = known ? prevColor[litKey] : -1;
		const uint16_t previousColor = known ? prevColor[colorKey] : 0;
		if (known && previousLit == lit && previousColor == color) return;

		const uint16_t off = tft.color565(34, 34, 37);
		for (int i = 0; i < FERRARI_BAR_SEGMENTS; ++i)
		{
			const bool on = i < lit;
			if (known && on == (i < previousLit) && (!on || previousColor == color)) continue;
			const int y = FERRARI_BAR_TOP +
				(FERRARI_BAR_SEGMENTS - 1 - i) * FERRARI_BAR_PITCH;
			tft.fillRoundRect(x, y, width, FERRARI_BAR_SEGMENT_H, 2, on ? color : off);
		}
		prevColor[litKey] = lit;
		prevColor[colorKey] = color;
	}

	static int ferrariPercentSegments(int percent)
	{
		return (constrain(percent, 0, 100) * FERRARI_BAR_SEGMENTS + 50) / 100;
	}

	void drawFerrariPedalIcon(int cx, int cy, bool brake, uint16_t color)
	{
		if (brake)
		{
			// Wide brake pedal hanging from its arm.
			tft.fillRect(cx - 1, cy - 12, 3, 6, color);
			tft.fillRoundRect(cx - 9, cy - 6, 19, 14, 3, color);
			for (int y : {cy - 3, cy + 1, cy + 5})
				tft.drawFastHLine(cx - 6, y, 13, TFT_BLACK);
		}
		else
		{
			// Tall narrow throttle pedal.
			tft.fillRect(cx - 1, cy - 12, 3, 4, color);
			tft.fillRoundRect(cx - 6, cy - 8, 13, 20, 3, color);
			for (int y : {cy - 4, cy, cy + 4, cy + 8})
				tft.drawFastHLine(cx - 3, y, 7, TFT_BLACK);
		}
	}

	void drawFerrariTurboIcon(int cx, int cy, uint16_t color)
	{
		// Turbocharger: volute housing, compressor wheel and outlet pipe.
		const int hx = cx - 3;
		const int hy = cy + 2;
		tft.drawCircle(hx, hy, 9, color);
		tft.drawCircle(hx, hy, 8, color);
		tft.drawCircle(hx, hy, 4, color);
		tft.fillCircle(hx, hy, 1, color);
		tft.fillRect(hx, hy - 9, 12, 4, color);
		tft.fillRect(hx + 11, hy - 11, 2, 8, color);
	}

	// Redraws the icon under a bar when its colour changes (e.g. fuel reserve,
	// or turbo unavailable on a naturally aspirated car).
	template <typename Painter>
	void drawFerrariBarIcon(int barX, int barWidth, const char *id,
		uint16_t color, bool forceUpdate, Painter paint)
	{
		const String cacheState = String(color);
		if (!forceUpdate && prevData[id] == cacheState) return;
		const int cx = barX + barWidth / 2;
		tft.fillRect(cx - 17, FERRARI_ICON_Y - 13, 34, 26, TFT_BLACK);
		paint(cx, FERRARI_ICON_Y);
		prevData[id] = cacheState;
	}

	uint16_t ferrariUnavailable() { return tft.color565(70, 72, 76); }
	uint16_t ferrariBarRed() { return tft.color565(235, 30, 30); }
	uint16_t ferrariBarBlue() { return tft.color565(50, 160, 255); }

	void drawFerrariFlagIcon(int cx, int cy, uint16_t color)
	{
		// Chequered flag on a pole.
		tft.fillRect(cx - 9, cy - 11, 2, 23, color);
		tft.drawRect(cx - 7, cy - 11, 17, 12, color);
		for (int row = 0; row < 3; ++row)
			for (int col = 0; col < 4; ++col)
				if ((row + col) % 2 == 0)
					tft.fillRect(cx - 6 + col * 4, cy - 10 + row * 3, 4, 3, color);
	}

	// Brake and throttle bars on the left, shared by both Ferrari themes.
	void drawFerrariPedalBars(const DashboardState &state, bool forceUpdate)
	{
		const uint16_t unavailable = ferrariUnavailable();
		const uint16_t red = ferrariBarRed();
		const uint16_t green = tft.color565(40, 210, 70);

		// Pedals: SimHub may report "--" when a pedal is not available.
		const bool brakeKnown = state.absLevel != "--";
		const bool throttleKnown = state.tcLevel != "--";
		drawFerrariBar(FERRARI_BRAKE_X, FERRARI_BAR_W,
			brakeKnown ? ferrariPercentSegments(state.absLevel.toInt()) : 0,
			red, "ferBarBrake", forceUpdate);
		drawFerrariBar(FERRARI_THROTTLE_X, FERRARI_BAR_W,
			throttleKnown ? ferrariPercentSegments(state.tcLevel.toInt()) : 0,
			green, "ferBarThrottle", forceUpdate);
		const uint16_t brakeIcon = brakeKnown ? red : unavailable;
		drawFerrariBarIcon(FERRARI_BRAKE_X, FERRARI_BAR_W, "ferIconBrake", brakeIcon,
			forceUpdate, [&](int x, int y) { drawFerrariPedalIcon(x, y, true, brakeIcon); });
		const uint16_t throttleIcon = throttleKnown ? green : unavailable;
		drawFerrariBarIcon(FERRARI_THROTTLE_X, FERRARI_BAR_W, "ferIconThrottle", throttleIcon,
			forceUpdate, [&](int x, int y) { drawFerrariPedalIcon(x, y, false, throttleIcon); });
	}

	// GT7 right side: turbo next to the gear panel, fuel on the screen edge.
	void drawFerrariTurboFuelBars(const DashboardState &state, bool forceUpdate)
	{
		const uint16_t unavailable = ferrariUnavailable();
		const uint16_t red = ferrariBarRed();
		const uint16_t blue = ferrariBarBlue();

		// Fuel turns red in reserve (15% or less).
		const int fuelPercent = constrain(state.fuelProgressPercent, 0, 100);
		const bool fuelReserve = state.fuelValueValid && fuelPercent <= 15;
		const uint16_t fuelColor = fuelReserve ? red : ferrariYellow();
		drawFerrariBar(FERRARI_EDGE_BAR_X, FERRARI_EDGE_BAR_W,
			state.fuelValueValid ? ferrariPercentSegments(fuelPercent) : 0,
			fuelColor, "ferBarFuel", forceUpdate);
		const uint16_t fuelIcon = state.fuelValueValid ? fuelColor : unavailable;
		drawFerrariBarIcon(FERRARI_EDGE_BAR_X, FERRARI_EDGE_BAR_W, "ferIconFuel", fuelIcon,
			forceUpdate, [&](int x, int y) {
				drawDashboardFuelPumpIcon(x - 8, y - 12, 100, fuelIcon, fuelIcon, TFT_BLACK);
			});

		// Turbo boost on a 0-2 bar scale; empty on naturally aspirated cars.
		static constexpr float TURBO_FULL_SCALE_BAR = 2.0f;
		const bool turboKnown = state.hasTurbo && isfinite(state.turboBoostBar);
		const int turboSegments = turboKnown
			? lroundf(constrain(state.turboBoostBar / TURBO_FULL_SCALE_BAR, 0.0f, 1.0f) *
				FERRARI_BAR_SEGMENTS)
			: 0;
		drawFerrariBar(FERRARI_RIGHT_BAR_X, FERRARI_BAR_W, turboSegments, blue,
			"ferBarTurbo", forceUpdate);
		const uint16_t turboIcon = turboKnown ? blue : unavailable;
		drawFerrariBarIcon(FERRARI_RIGHT_BAR_X, FERRARI_BAR_W, "ferIconTurbo", turboIcon,
			forceUpdate, [&](int x, int y) { drawFerrariTurboIcon(x, y, turboIcon); });
	}

	// Assetto Corsa right side: clutch next to the gear panel, lap progress
	// on the screen edge (AC reports neither fuel nor turbo).
	void drawFerrariClutchLapBars(const DashboardState &state, bool forceUpdate)
	{
		const uint16_t unavailable = ferrariUnavailable();
		const uint16_t blue = ferrariBarBlue();

		const bool clutchKnown = state.clutchPercent >= 0;
		drawFerrariBar(FERRARI_RIGHT_BAR_X, FERRARI_BAR_W,
			clutchKnown ? ferrariPercentSegments(state.clutchPercent) : 0,
			blue, "ferBarClutch", forceUpdate);
		const uint16_t clutchIcon = clutchKnown ? blue : unavailable;
		drawFerrariBarIcon(FERRARI_RIGHT_BAR_X, FERRARI_BAR_W, "ferIconClutch", clutchIcon,
			forceUpdate, [&](int x, int y) { drawFerrariPedalIcon(x, y, true, clutchIcon); });

		const bool lapKnown = isfinite(state.lapProgress);
		drawFerrariBar(FERRARI_EDGE_BAR_X, FERRARI_EDGE_BAR_W,
			lapKnown ? ferrariPercentSegments(lroundf(state.lapProgress * 100.0f)) : 0,
			ferrariYellow(), "ferBarLap", forceUpdate);
		const uint16_t lapIcon = lapKnown ? ferrariYellow() : unavailable;
		drawFerrariBarIcon(FERRARI_EDGE_BAR_X, FERRARI_EDGE_BAR_W, "ferIconLap", lapIcon,
			forceUpdate, [&](int x, int y) { drawFerrariFlagIcon(x, y, lapIcon); });
	}

	// ---- Lamps -------------------------------------------------------------------
	// Ring with side arcs, as in the ISO parking-brake and ABS symbols.
	void drawFerrariRingLamp(int cx, int cy, const char *text, uint8_t font,
		uint16_t color)
	{
		tft.drawCircle(cx, cy, 11, color);
		tft.drawCircle(cx, cy, 10, color);
		for (int angle = -48; angle <= 48; angle += 3)
		{
			const float radians = angle * DEG_TO_RAD;
			const int dx = lroundf(cosf(radians) * 13.0f);
			const int dy = lroundf(sinf(radians) * 13.0f);
			tft.drawFastHLine(cx + dx, cy + dy, 2, color);
			tft.drawFastHLine(cx - dx - 1, cy + dy, 2, color);
		}
		tft.setTextDatum(MC_DATUM);
		tft.setTextColor(color);
		tft.drawString(text, cx + 1, cy + 1, font);
		tft.setTextDatum(TL_DATUM);
	}

	void drawFerrariBadgeLamp(int cx, int cy, const char *text, uint16_t color)
	{
		tft.drawRoundRect(cx - 13, cy - 8, 27, 17, 4, color);
		tft.drawRoundRect(cx - 12, cy - 7, 25, 15, 3, color);
		tft.setTextDatum(MC_DATUM);
		tft.setTextColor(color);
		tft.drawString(text, cx + 1, cy + 1, 1);
		tft.setTextDatum(TL_DATUM);
	}

	uint16_t ferrariLampIdle() { return tft.color565(72, 74, 78); }
	// Aid switched off in the car setup: dimmer than an idle lamp.
	uint16_t ferrariLampDisabled() { return tft.color565(36, 37, 40); }

	// One of `count` lamp cells in the bottom row: dark when off, tinted and
	// lit when on. `idleColor` draws the icon while the lamp is off.
	template <typename Painter>
	void drawFerrariLamp(int index, int count, const char *id, bool active,
		uint16_t color, uint16_t idleColor, bool forceUpdate, Painter paint)
	{
		const String cacheState = active ? "on" : String(idleColor);
		if (!forceUpdate && prevData[id] == cacheState) return;
		const int pitch = FERRARI_LAMP_ROW_W / count;
		const int x = 4 + index * pitch;
		const int width = pitch - 4;
		const uint16_t background = active ? blendRgb565(TFT_BLACK, color, 60) : TFT_BLACK;
		tft.fillRoundRect(x, FERRARI_LAMP_Y, width, FERRARI_LAMP_H, 6, background);
		tft.drawRoundRect(x, FERRARI_LAMP_Y, width, FERRARI_LAMP_H, 6,
			active ? color : tft.color565(50, 50, 54));
		paint(x + width / 2, FERRARI_LAMP_Y + FERRARI_LAMP_H / 2,
			active ? color : idleColor, background);
		prevData[id] = cacheState;
	}

	void drawFerrariLamps(const DashboardState &state, bool forceUpdate)
	{
		const uint16_t amber = tft.color565(255, 170, 0);
		const uint16_t red = ferrariBarRed();
		const uint16_t idle = ferrariLampIdle();
		drawFerrariLamp(0, 4, "ferLampAbs", isActiveValue(state.absActive), amber, idle, forceUpdate,
			[&](int x, int y, uint16_t c, uint16_t) { drawFerrariRingLamp(x, y, "ABS", 1, c); });
		drawFerrariLamp(1, 4, "ferLampTcs", isActiveValue(state.tcActive), amber, idle, forceUpdate,
			[&](int x, int y, uint16_t c, uint16_t bg) { drawDashboardTcsIcon(x - 11, y - 14, c, bg); });
		drawFerrariLamp(2, 4, "ferLampAsm", state.asmActive, amber, idle, forceUpdate,
			[&](int x, int y, uint16_t c, uint16_t) { drawFerrariBadgeLamp(x, y, "ASM", c); });
		drawFerrariLamp(3, 4, "ferLampHandbrake", state.handbrakeActive, red, idle, forceUpdate,
			[&](int x, int y, uint16_t c, uint16_t) { drawFerrariRingLamp(x, y, "P", 2, c); });
	}

	// Assetto Corsa lamps: ABS and TC (dimmer when switched off in the car
	// setup) and the pit lane.
	void drawFerrariACLamps(const DashboardState &state, bool forceUpdate)
	{
		const uint16_t amber = tft.color565(255, 170, 0);
		const uint16_t idle = ferrariLampIdle();
		const uint16_t disabled = ferrariLampDisabled();
		drawFerrariLamp(0, 3, "ferLampAbs", isActiveValue(state.absActive), amber,
			state.absEnabled ? idle : disabled, forceUpdate,
			[&](int x, int y, uint16_t c, uint16_t) { drawFerrariRingLamp(x, y, "ABS", 1, c); });
		drawFerrariLamp(1, 3, "ferLampTc", isActiveValue(state.tcActive), amber,
			state.tcEnabled ? idle : disabled, forceUpdate,
			[&](int x, int y, uint16_t c, uint16_t bg) { drawDashboardTcsIcon(x - 11, y - 14, c, bg); });
		drawFerrariLamp(2, 3, "ferLampPit", state.inPitLane, ferrariBarBlue(), idle, forceUpdate,
			[&](int x, int y, uint16_t c, uint16_t) { drawFerrariBadgeLamp(x, y, "PIT", c); });
	}

	// ---- Gear and speed ------------------------------------------------------------
	void drawFerrariGear(const String &gear, bool forceUpdate,
		LGFX_Sprite &sprite, bool &spriteCreated, int gearX = FERRARI_GEAR_X)
	{
		if (!forceUpdate && prevData["ferGear"] == gear) return;
		if (!spriteCreated)
		{
			sprite.setColorDepth(16);
			spriteCreated = sprite.createSprite(FERRARI_GEAR_W, FERRARI_GEAR_H) != nullptr;
		}
		// Without a sprite the glyph is drawn straight onto the panel.
		lgfx::LovyanGFX &canvas = spriteCreated
			? static_cast<lgfx::LovyanGFX &>(sprite) : static_cast<lgfx::LovyanGFX &>(tft);
		const int originX = spriteCreated ? 0 : gearX;
		const int originY = spriteCreated ? 0 : FERRARI_GEAR_Y;
		canvas.fillRect(originX, originY, FERRARI_GEAR_W, FERRARI_GEAR_H, ferrariYellow());

		const int chars = constrain(static_cast<int>(gear.length()), 1, 2);
		const float h = 94.0f;
		const float w = chars > 1 ? 32.0f : 54.0f;
		const float stroke = chars > 1 ? 10.0f : 16.0f;
		const float gap = 6.0f;
		const float slant = 0.14f;
		const float total = chars * w + (chars - 1) * gap + h * slant;
		const float x0 = originX + (FERRARI_GEAR_W - total) / 2.0f;
		const float y0 = originY + (FERRARI_GEAR_H - h) / 2.0f;
		for (int i = 0; i < chars && i < static_cast<int>(gear.length()); ++i)
			drawFerrariGlyph(canvas, gear[i], x0 + i * (w + gap), y0, w, h, stroke,
				slant, TFT_BLACK);

		if (spriteCreated)
		{
			sprite.pushSprite(gearX, FERRARI_GEAR_Y);
			releaseSpriteAfterThemePreview(sprite, spriteCreated);
		}
		prevData["ferGear"] = gear;
	}

	void drawFerrariSpeed(const String &speed, bool forceUpdate,
		LGFX_Sprite &sprite, bool &spriteCreated)
	{
		if (!forceUpdate && prevData["ferSpeed"] == speed) return;
		if (!spriteCreated)
		{
			sprite.setColorDepth(16);
			spriteCreated = sprite.createSprite(FERRARI_SPEED_W, FERRARI_SPEED_H) != nullptr;
		}
		lgfx::LovyanGFX &canvas = spriteCreated
			? static_cast<lgfx::LovyanGFX &>(sprite) : static_cast<lgfx::LovyanGFX &>(tft);
		const int originX = spriteCreated ? 0 : FERRARI_SPEED_X;
		const int originY = spriteCreated ? 0 : FERRARI_SPEED_Y;
		canvas.fillRect(originX, originY, FERRARI_SPEED_W, FERRARI_SPEED_H, TFT_BLACK);

		// Right-aligned so the digits do not shift as the speed changes.
		const float h = 40.0f, w = 22.0f, gap = 6.0f, stroke = 7.0f, slant = 0.14f;
		const int chars = min(static_cast<int>(speed.length()), 4);
		float x = originX + FERRARI_SPEED_W - 2 - h * slant - w;
		for (int i = chars - 1; i >= 0; --i)
		{
			drawFerrariGlyph(canvas, speed[i], x, originY + (FERRARI_SPEED_H - h) / 2.0f,
				w, h, stroke, slant, TFT_WHITE);
			x -= w + gap;
		}

		if (spriteCreated)
		{
			sprite.pushSprite(FERRARI_SPEED_X, FERRARI_SPEED_Y);
			releaseSpriteAfterThemePreview(sprite, spriteCreated);
		}
		prevData["ferSpeed"] = speed;
	}

	// ---- Layout ------------------------------------------------------------------
	void drawFerrariStaticLayout()
	{
		tft.fillScreen(TFT_BLACK);
		tft.setTextPadding(0);
		tft.setTextSize(1.0f);
		tft.setTextDatum(TL_DATUM);

		// Ferrari-red line under the rev LEDs.
		tft.fillRect(0, 27, 320, 2, ferrariRed());

		// Giallo Modena gear panel with a small tricolour at the top.
		tft.fillRoundRect(FERRARI_PANEL_X, FERRARI_PANEL_Y, FERRARI_PANEL_W,
			FERRARI_PANEL_H, 10, ferrariYellow());
		tft.fillRect(145, 37, 10, 3, tft.color565(0, 146, 70));
		tft.fillRect(155, 37, 10, 3, TFT_WHITE);
		tft.fillRect(165, 37, 10, 3, tft.color565(206, 43, 55));

		// Frames around the bar gauges.
		const uint16_t frame = tft.color565(48, 48, 52);
		static constexpr int bars[][2] = {
			{FERRARI_BRAKE_X, FERRARI_BAR_W}, {FERRARI_THROTTLE_X, FERRARI_BAR_W},
			{FERRARI_RIGHT_BAR_X, FERRARI_BAR_W}, {FERRARI_EDGE_BAR_X, FERRARI_EDGE_BAR_W}};
		for (const auto &bar : bars)
			tft.drawRoundRect(bar[0] - 3, FERRARI_BAR_TOP - 3, bar[1] + 6,
				FERRARI_BAR_SEGMENTS * FERRARI_BAR_PITCH + 4, 4, frame);

		tft.setTextColor(tft.color565(150, 154, 158), TFT_BLACK);
		tft.setTextDatum(ML_DATUM);
		tft.drawString("km/h", 197, 188, 2);
		tft.setTextDatum(TL_DATUM);
	}

	// Elements shared by both Ferrari themes.
	void drawFerrariCommon(const DashboardState &state, bool forceUpdate, int gearX = FERRARI_GEAR_X)
	{
		static LGFX_Sprite gearSprite(&tft);
		static LGFX_Sprite speedSprite(&tft);
		static bool gearSpriteCreated = false;
		static bool speedSpriteCreated = false;

		if (forceUpdate) drawFerrariStaticLayout();

		drawFerrariRevLeds(state, forceUpdate);
		drawFerrariGear(state.gear, forceUpdate, gearSprite, gearSpriteCreated, gearX);
		drawFerrariSpeed(state.speed, forceUpdate, speedSprite, speedSpriteCreated);
		drawFerrariPedalBars(state, forceUpdate);
	}

	void drawFerrariDashboard(const DashboardState &state, bool forceUpdate)
	{
		drawFerrariCommon(state, forceUpdate);
		drawFerrariTurboFuelBars(state, forceUpdate);
		drawFerrariLamps(state, forceUpdate);
	}

	// Assetto Corsa variant: clutch and lap progress instead of turbo and fuel,
	// ABS/TC/pit lamps instead of ABS/TCS/ASM/handbrake.
	void drawFerrariACDashboard(const DashboardState &state, bool forceUpdate)
	{
		drawFerrariCommon(state, forceUpdate);
		drawFerrariClutchLapBars(state, forceUpdate);
		drawFerrariACLamps(state, forceUpdate);
	}

	void drawFerrariGoldDashboard(const DashboardState &state, bool forceUpdate)
	{
		drawFerrariCommon(state, forceUpdate, FERRARI_GEAR_X - 12);
		drawFerrariTurboFuelBars(state, forceUpdate);
		drawFerrariLamps(state, forceUpdate);

        static String prevSuggested = "";
        if (forceUpdate) prevSuggested = "";

        if (state.suggestedGear != prevSuggested) {
            // Riquadro nell'angolo in basso a destra del pannello della marcia,
            // nello spazio libero a destra della marcia inserita (gear x FERRARI_GEAR_X - 12 + FERRARI_GEAR_W).
            const int rw = 32;
            const int rh = 44;
            const int cx = FERRARI_PANEL_X + FERRARI_PANEL_W - rw - 4;
            const int cy = FERRARI_PANEL_Y + FERRARI_PANEL_H - rh - 4;
            const int r = 6;

            // Cancella con il colore del pannello
            tft.fillRect(cx, cy, rw, rh, ferrariYellow());

            if (state.suggestedGear != "") {
                tft.fillRoundRect(cx, cy, rw, rh, r, ferrariRed());

                const float h = 30.0f;
                const float w = 17.0f;
                const float stroke = 6.0f;
                const float slant = 0.14f;
                const float x = cx + (rw - w) / 2.0f - (h * slant) / 2.0f + 1;
                const float y = cy + (rh - h) / 2.0f;

                drawFerrariGlyph(tft, state.suggestedGear[0], x, y, w, h, stroke, slant, TFT_BLACK);
            }
            prevSuggested = state.suggestedGear;
        }
	}
