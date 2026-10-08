	// MAPPA PISTA: the circuit drawn by the dashboard itself during the first
	// lap from the car positions (GT7 and Assetto Corsa), with the car, the
	// start line and the sectors coloured like F1 timing; delta, gear, speed and
	// lap times on the right.
	enum : int
	{
		MAP_AREA_X = 2,
		MAP_AREA_Y = 13,
		MAP_AREA_W = 206,
		MAP_AREA_H = 225,
		MAP_PANEL_X = 212,
		MAP_PANEL_W = 104,
		MAP_CAR_R = 5,
	};

	struct TrackMapView
	{
		float scale = 1, offsetX = 0, offsetZ = 0;
		float minX = 0, minZ = 0, maxX = 0, maxZ = 0; // area covered by the view
		bool valid = false;
	};
	TrackMapView trackMapView;
	uint32_t trackMapDrawnRevision = 0;
	int trackMapDrawnPoints = 0;
	LapAnalysis::MapState trackMapDrawnState = LapAnalysis::MapState::Waiting;
	int trackMapCarX = -1000, trackMapCarY = -1000;
	String trackMapSectorCache;
	String trackMapMarkCache;

	void setTrackMapView(float minX, float minZ, float maxX, float maxZ)
	{
		const float w = max(maxX - minX, 60.0f), h = max(maxZ - minZ, 60.0f);
		const float cx = (minX + maxX) / 2, cz = (minZ + maxZ) / 2;
		TrackMapView &v = trackMapView;
		v.scale = min((MAP_AREA_W - 2.0f * (MAP_CAR_R + 4)) / w, (MAP_AREA_H - 2.0f * (MAP_CAR_R + 4)) / h);
		v.offsetX = MAP_AREA_X + MAP_AREA_W / 2.0f - cx * v.scale;
		v.offsetZ = MAP_AREA_Y + MAP_AREA_H / 2.0f - cz * v.scale;
		v.minX = cx - (MAP_AREA_W / 2.0f) / v.scale;
		v.maxX = cx + (MAP_AREA_W / 2.0f) / v.scale;
		v.minZ = cz - (MAP_AREA_H / 2.0f) / v.scale;
		v.maxZ = cz + (MAP_AREA_H / 2.0f) / v.scale;
		v.valid = true;
	}

	void trackMapToScreen(float x, float z, int &sx, int &sy) const
	{
		sx = static_cast<int>(lroundf(x * trackMapView.scale + trackMapView.offsetX));
		sy = static_cast<int>(lroundf(z * trackMapView.scale + trackMapView.offsetZ));
	}

	// Braking points of the reference lap, as small orange squares on the track.
	void drawTrackMapBrakeMarks(int bx, int by, int bw, int bh)
	{
		const int zones = lapAnalysis.referenceBrakeZoneCount();
		for (int i = 0; i < zones; ++i)
		{
			LapAnalysis::BrakeZone zone;
			if (!lapAnalysis.referenceBrakeZone(i, zone)) continue;
			int sx, sy;
			trackMapToScreen(zone.x, zone.z, sx, sy);
			if (sx < bx - 4 || sx > bx + bw + 4 || sy < by - 4 || sy > by + bh + 4) continue;
			tft.fillRect(sx - 3, sy - 3, 7, 7, TFT_BLACK);
			tft.fillRect(sx - 2, sy - 2, 5, 5, tft.color565(255, 120, 0));
		}
	}

	void drawThickLine(int x0, int y0, int x1, int y1, uint16_t color)
	{
		tft.drawLine(x0, y0, x1, y1, color);
		tft.drawLine(x0 + 1, y0, x1 + 1, y1, color);
		tft.drawLine(x0 - 1, y0, x1 - 1, y1, color);
		tft.drawLine(x0, y0 + 1, x1, y1 + 1, color);
		tft.drawLine(x0, y0 - 1, x1, y1 - 1, color);
	}

	uint16_t trackMapSegmentColor(int i)
	{
		if (lapAnalysis.mapState() != LapAnalysis::MapState::Complete) return tft.color565(150, 156, 166);
		const LapAnalysis::SectorState state = displaySectorState(LapAnalysis::Analyzer::mapPointSector(i));
		if (state == LapAnalysis::SectorState::None || state == LapAnalysis::SectorState::Running)
			return tft.color565(150, 156, 166);
		return sectorColor(state);
	}

	// Draws the segments of the track that touch the box (all with a full box).
	void drawTrackMapSegments(int bx, int by, int bw, int bh)
	{
		const int count = lapAnalysis.mapPointCount();
		const bool closed = lapAnalysis.mapState() == LapAnalysis::MapState::Complete;
		const int segments = closed ? count : count - 1;
		for (int i = 0; i < segments; ++i)
		{
			float x0, z0, x1, z1;
			lapAnalysis.mapPoint(i, x0, z0);
			lapAnalysis.mapPoint((i + 1) % count, x1, z1);
			int sx0, sy0, sx1, sy1;
			trackMapToScreen(x0, z0, sx0, sy0);
			trackMapToScreen(x1, z1, sx1, sy1);
			if (max(sx0, sx1) + 2 < bx || min(sx0, sx1) - 2 > bx + bw ||
				max(sy0, sy1) + 2 < by || min(sy0, sy1) - 2 > by + bh)
				continue;
			drawThickLine(sx0, sy0, sx1, sy1, trackMapSegmentColor(i));
		}
		if (closed && count > 2)
		{
			// Start/finish line across the track at the first point.
			float x0, z0, x1, z1;
			lapAnalysis.mapPoint(0, x0, z0);
			lapAnalysis.mapPoint(1, x1, z1);
			int sx, sy, tx, ty;
			trackMapToScreen(x0, z0, sx, sy);
			trackMapToScreen(x1, z1, tx, ty);
			float nx = -(ty - sy), ny = static_cast<float>(tx - sx);
			const float len = sqrtf(nx * nx + ny * ny);
			if (len > 0.1f)
			{
				nx = nx / len * 7; ny = ny / len * 7;
				for (int o = -1; o <= 1; ++o)
					tft.drawLine(sx - nx + o, sy - ny, sx + nx + o, sy + ny, TFT_WHITE);
			}
		}
		if (closed) drawTrackMapBrakeMarks(bx, by, bw, bh);
	}

	void drawTrackMapMessage(const char *title, const String &line1, const String &line2)
	{
		tft.fillRect(MAP_AREA_X, MAP_AREA_Y, MAP_AREA_W, MAP_AREA_H, TFT_BLACK);
		const int cx = MAP_AREA_X + MAP_AREA_W / 2;
		drawFerrariFlagIcon(cx, 92, analysisMuted());
		tft.setFont(&fonts::FreeSansBold9pt7b);
		tft.setTextSize(1.0f);
		tft.setTextDatum(MC_DATUM);
		tft.setTextColor(TFT_WHITE, TFT_BLACK);
		tft.drawString(title, cx, 124);
		tft.setFont(&fonts::Font2);
		tft.setTextColor(analysisMuted(), TFT_BLACK);
		tft.drawString(line1, cx, 148);
		tft.drawString(line2, cx, 166);
	}

	void drawTrackMap(bool forceUpdate)
	{
		const LapAnalysis::MapState state = lapAnalysis.mapState();
		const int count = lapAnalysis.mapPointCount();
		String sectors;
		for (int i = 0; i < LapAnalysis::SECTORS; ++i) sectors += static_cast<int>(displaySectorState(i));

		bool redraw = forceUpdate || state != trackMapDrawnState;
		if (state == LapAnalysis::MapState::Waiting)
		{
			if (redraw)
			{
				drawTrackMapMessage("MAPPA PISTA", "Si disegna da sola dal", "prossimo passaggio sul traguardo");
				trackMapDrawnState = state;
				trackMapCarX = trackMapCarY = -1000;
			}
			return;
		}

		float minX, minZ, maxX, maxZ;
		lapAnalysis.mapBounds(minX, minZ, maxX, maxZ);
		if (state == LapAnalysis::MapState::Recording)
		{
			// Grows with the track: rescale when the line leaves the view.
			const float marginX = max((maxX - minX) * 0.3f, 120.0f);
			const float marginZ = max((maxZ - minZ) * 0.3f, 120.0f);
			if (!trackMapView.valid || redraw || minX < trackMapView.minX || maxX > trackMapView.maxX ||
				minZ < trackMapView.minZ || maxZ > trackMapView.maxZ)
			{
				setTrackMapView(minX - marginX, minZ - marginZ, maxX + marginX, maxZ + marginZ);
				redraw = true;
			}
		}
		else if (redraw)
			setTrackMapView(minX, minZ, maxX, maxZ);

		// New colours of the sectors or new braking points of the reference lap.
		const String marks = String(lapAnalysis.referenceBrakeRevision());
		if (state == LapAnalysis::MapState::Complete && (sectors != trackMapSectorCache || marks != trackMapMarkCache))
			redraw = true;

		if (redraw)
		{
			tft.fillRect(MAP_AREA_X, MAP_AREA_Y, MAP_AREA_W, MAP_AREA_H, TFT_BLACK);
			drawTrackMapSegments(MAP_AREA_X, MAP_AREA_Y, MAP_AREA_W, MAP_AREA_H);
			trackMapDrawnPoints = count;
			trackMapCarX = trackMapCarY = -1000;
			trackMapSectorCache = sectors;
			trackMapMarkCache = marks;
			trackMapDrawnState = state;
		}
		else if (state == LapAnalysis::MapState::Recording && count > trackMapDrawnPoints)
		{
			// Only the new part of the line.
			for (int i = max(trackMapDrawnPoints - 1, 0); i + 1 < count; ++i)
			{
				float x0, z0, x1, z1;
				lapAnalysis.mapPoint(i, x0, z0);
				lapAnalysis.mapPoint(i + 1, x1, z1);
				int sx0, sy0, sx1, sy1;
				trackMapToScreen(x0, z0, sx0, sy0);
				trackMapToScreen(x1, z1, sx1, sy1);
				drawThickLine(sx0, sy0, sx1, sy1, trackMapSegmentColor(i));
			}
			trackMapDrawnPoints = count;
		}
		if (state == LapAnalysis::MapState::Recording)
		{
			const String recorded = String("MAPPA ") + String(lapAnalysis.mapRecordedMeters() / 1000.0f, 1) + " km";
			drawAnalysisText("mapRec", recorded, MAP_AREA_X + 2, MAP_AREA_Y + 6, 110, ML_DATUM,
				&fonts::Font0, analysisMuted(), TFT_BLACK, redraw);
		}

		// Car: erase the old dot (redrawing the track under it), draw the new one.
		float carX, carZ;
		if (!lapAnalysis.carPosition(carX, carZ)) return;
		int sx, sy;
		trackMapToScreen(carX, carZ, sx, sy);
		sx = constrain(sx, MAP_AREA_X + MAP_CAR_R, MAP_AREA_X + MAP_AREA_W - MAP_CAR_R - 1);
		sy = constrain(sy, MAP_AREA_Y + MAP_CAR_R, MAP_AREA_Y + MAP_AREA_H - MAP_CAR_R - 1);
		if (sx == trackMapCarX && sy == trackMapCarY) return;
		if (trackMapCarX > -1000)
		{
			const int ex = trackMapCarX - MAP_CAR_R - 1, ey = trackMapCarY - MAP_CAR_R - 1;
			const int size = 2 * MAP_CAR_R + 3;
			tft.setClipRect(ex, ey, size, size);
			tft.fillRect(ex, ey, size, size, TFT_BLACK);
			drawTrackMapSegments(ex, ey, size, size);
			tft.clearClipRect();
		}
		tft.fillCircle(sx, sy, MAP_CAR_R, TFT_WHITE);
		tft.fillCircle(sx, sy, MAP_CAR_R - 2, analysisRed());
		trackMapCarX = sx;
		trackMapCarY = sy;
	}

	void drawTrackMapStaticLayout()
	{
		tft.fillScreen(TFT_BLACK);
		tft.drawFastVLine(MAP_PANEL_X - 3, MAP_AREA_Y, MAP_AREA_H, analysisLine());
		tft.setFont(&fonts::Font0);
		tft.setTextSize(1.0f);
		tft.setTextColor(analysisMuted(), TFT_BLACK);
		tft.setTextDatum(TL_DATUM);
		tft.drawString("GIRO", MAP_PANEL_X, 114);
		tft.drawString("ULTIMO", MAP_PANEL_X, 150);
		tft.drawString("MIGLIORE", MAP_PANEL_X, 182);
		tft.drawString("KM/H", MAP_PANEL_X + MAP_PANEL_W - 24, 50);
		trackMapView.valid = false;
		trackMapCarX = trackMapCarY = -1000;
	}

	void drawTrackMapDashboard(const DashboardState &state, bool forceUpdate)
	{
		if (forceUpdate) drawTrackMapStaticLayout();

		// Slim shift bar across the top.
		const uint16_t zones[3] = {tft.color565(0, 210, 70), tft.color565(255, 200, 0), tft.color565(255, 30, 30)};
		drawShiftLights(state, 20, 2, 2, 14, 7, 16, false, zones, tft.color565(255, 30, 30), forceUpdate);

		drawTrackMap(forceUpdate);

		const int px = MAP_PANEL_X, pw = MAP_PANEL_W;
		drawAnalysisGear("mapGear", state.gear, px, 14, 44, 52, TFT_WHITE, TFT_BLACK, forceUpdate);
		drawAnalysisText("mapSpeed", state.speed, px + pw - 2, 34, 56, MR_DATUM, &fonts::Font4,
			TFT_WHITE, TFT_BLACK, forceUpdate);
		drawDeltaBox("mapDelta", px, 72, pw, 36, state.sessionBestLiveDeltaSeconds,
			&fonts::FreeSansBold12pt7b, forceUpdate);
		drawAnalysisText("mapLap", compactLapTime(state.currentLapTime), px, 134, pw, ML_DATUM,
			&fonts::FreeSansBold12pt7b, TFT_WHITE, TFT_BLACK, forceUpdate);
		drawAnalysisText("mapLast", compactLapTime(state.lastLapTime), px, 166, pw, ML_DATUM,
			&fonts::FreeSansBold9pt7b, TFT_WHITE, TFT_BLACK, forceUpdate);
		const int32_t best = lapAnalysis.bestLapMs();
		drawAnalysisText("mapBest", best > 0 ? analysisLapTime(best) : compactLapTime(state.bestLapTime),
			px, 198, pw, ML_DATUM, &fonts::FreeSansBold9pt7b, analysisPurple(), TFT_BLACK, forceUpdate);
		drawSectorBar("mapSector", px, 212, pw, 9, 3, false, forceUpdate);
		drawAnalysisText("mapLapNo", String("GIRO ") + state.tyrePressureRearLeft, px, 232, 60, ML_DATUM,
			&fonts::Font2, TFT_WHITE, TFT_BLACK, forceUpdate);
		drawAnalysisText("mapPos", String("P") + state.tyrePressureFrontRight, px + pw, 232, 40, MR_DATUM,
			&fonts::Font2, TFT_WHITE, TFT_BLACK, forceUpdate);
		restoreDefaultText();
	}
