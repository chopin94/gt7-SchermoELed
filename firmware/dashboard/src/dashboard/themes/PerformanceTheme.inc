	// PRESTAZIONI: automatic acceleration timer. Stop for a second and the
	// drag-race tree lights up; the run is timed from the start: 0-100, 0-200,
	// 100-200 km/h, 400 m with the exit speed and top speed, last run and best.
	enum : int
	{
		PERF_TREE_X = 30,
		PERF_TABLE_X = 64,
		PERF_TABLE_Y = 128,
		PERF_ROW_H = 19,
	};

	void drawPerformanceTreeLight(int index, bool on)
	{
		// 0-1 stage (small white), 2-4 amber, 5 green, 6 red.
		static const int ys[7] = {24, 24, 54, 82, 110, 142, 174};
		const int x = index == 0 ? PERF_TREE_X - 9 : index == 1 ? PERF_TREE_X + 9 : PERF_TREE_X;
		const int r = index < 2 ? 6 : 11;
		uint16_t color;
		switch (index)
		{
		case 0: case 1: color = on ? TFT_WHITE : tft.color565(40, 40, 44); break;
		case 5: color = on ? tft.color565(30, 230, 70) : tft.color565(18, 52, 26); break;
		case 6: color = on ? tft.color565(255, 30, 30) : tft.color565(56, 18, 18); break;
		default: color = on ? tft.color565(255, 176, 0) : tft.color565(58, 42, 10); break;
		}
		tft.fillCircle(x, ys[index], r, color);
		tft.drawCircle(x, ys[index], r + 1, tft.color565(70, 72, 78));
	}

	void drawPerformanceTree(bool forceUpdate)
	{
		const LapAnalysis::TreeLight light = lapAnalysis.treeLight();
		bool lit[7] = {};
		switch (light)
		{
		case LapAnalysis::TreeLight::Staged: lit[0] = lit[1] = true; break;
		case LapAnalysis::TreeLight::Amber1: lit[0] = lit[1] = lit[2] = true; break;
		case LapAnalysis::TreeLight::Amber2: lit[0] = lit[1] = lit[2] = lit[3] = true; break;
		case LapAnalysis::TreeLight::Amber3: lit[0] = lit[1] = lit[2] = lit[3] = lit[4] = true; break;
		case LapAnalysis::TreeLight::Green: lit[5] = true; break;
		case LapAnalysis::TreeLight::FalseStart: lit[6] = true; break;
		default: break;
		}
		String cache;
		for (bool on : lit) cache += on ? '1' : '0';
		if (!forceUpdate && prevData["perfTree"] == cache) return;
		prevData["perfTree"] = cache;
		for (int i = 0; i < 7; ++i) drawPerformanceTreeLight(i, lit[i]);
	}

	static String performanceSeconds(float s)
	{
		if (!isfinite(s)) return "--";
		char buffer[12];
		snprintf(buffer, sizeof(buffer), "%.2f", s);
		return String(buffer);
	}

	void drawPerformanceStaticLayout()
	{
		tft.fillScreen(TFT_BLACK);
		tft.fillRoundRect(PERF_TREE_X - 22, 12, 44, 178, 10, analysisPanel());
		tft.drawFastVLine(PERF_TREE_X, 190, 50, analysisLine());
		tft.setFont(&fonts::Font0);
		tft.setTextSize(1.0f);
		tft.setTextColor(analysisMuted(), TFT_BLACK);
		tft.setTextDatum(TL_DATUM);
		tft.drawString("TEMPO", PERF_TABLE_X + 4, 16);
		tft.drawString("KM/H", 256, 16);
		tft.drawFastHLine(PERF_TABLE_X, PERF_TABLE_Y - 4, SCREEN_WIDTH - PERF_TABLE_X - 4, analysisLine());
		const int y = PERF_TABLE_Y + 2;
		tft.drawString("PROVA", PERF_TABLE_X + 4, y);
		tft.setTextDatum(TR_DATUM);
		tft.drawString("ULTIMA", 252, y);
		tft.drawString("MIGLIORE", 314, y);
		tft.setTextDatum(TL_DATUM);
		tft.setFont(&fonts::Font2);
		tft.setTextColor(TFT_WHITE, TFT_BLACK);
		const char *labels[5] = {"0-100", "0-200", "100-200", "400 m", "V MAX"};
		for (int i = 0; i < 5; ++i)
			tft.drawString(labels[i], PERF_TABLE_X + 4, PERF_TABLE_Y + 14 + i * PERF_ROW_H);
		for (int i = 0; i < 7; ++i) drawPerformanceTreeLight(i, false);
	}

	void drawPerformanceDashboard(const DashboardState &state, bool forceUpdate)
	{
		if (forceUpdate) drawPerformanceStaticLayout();
		drawPerformanceTree(forceUpdate);

		// Run timer (big) and status line.
		const LapAnalysis::LaunchPhase phase = lapAnalysis.launchPhase();
		const float seconds = lapAnalysis.runSeconds();
		String timer = performanceSeconds(seconds);
		if (!isfinite(seconds)) timer = "0.00";
		const LapAnalysis::TreeLight light = lapAnalysis.treeLight();
		const uint16_t timerColor = phase == LapAnalysis::LaunchPhase::Running ? TFT_WHITE
			: phase == LapAnalysis::LaunchPhase::Finished ? analysisYellow() : analysisMuted();
		drawAnalysisText("perfTimer", timer, PERF_TABLE_X + 2, 56, 128, ML_DATUM, &fonts::Font7,
			timerColor, TFT_BLACK, forceUpdate, 0.9f);
		drawAnalysisText("perfSpeed", state.speed, 314, 56, 104, MR_DATUM, &fonts::Font7,
			TFT_WHITE, TFT_BLACK, forceUpdate, 0.9f);

		String status;
		uint16_t statusColor = analysisMuted();
		switch (phase)
		{
		case LapAnalysis::LaunchPhase::Staged:
			status = light == LapAnalysis::TreeLight::Green ? "VIA!" : "PRONTO: PARTI AL VERDE";
			statusColor = light == LapAnalysis::TreeLight::Green ? analysisGreen() : analysisYellow();
			break;
		case LapAnalysis::LaunchPhase::Running:
			status = light == LapAnalysis::TreeLight::FalseStart ? "FALSA PARTENZA" : "PROVA IN CORSO";
			statusColor = light == LapAnalysis::TreeLight::FalseStart ? analysisRed() : TFT_WHITE;
			break;
		case LapAnalysis::LaunchPhase::Finished:
			status = "PROVA FINITA: FERMATI PER RIPARTIRE";
			break;
		default:
			status = "FERMATI 1 s PER INIZIARE";
			break;
		}
		drawAnalysisText("perfStatus", status, PERF_TABLE_X + 2, 92, 250, ML_DATUM, &fonts::Font2,
			statusColor, TFT_BLACK, forceUpdate);
		const LapAnalysis::LaunchRun &run = lapAnalysis.currentRun();
		const String reaction = isfinite(run.reaction)
			? String("REAZIONE ") + performanceSeconds(run.reaction) + " s" : String("");
		drawAnalysisText("perfReaction", reaction, PERF_TABLE_X + 2, 112, 130, ML_DATUM, &fonts::Font2,
			run.reaction < 0 ? analysisRed() : analysisMuted(), TFT_BLACK, forceUpdate);
		drawAnalysisText("perfGear", String("MARCIA ") + state.gear, 314, 112, 90, MR_DATUM, &fonts::Font2,
			analysisMuted(), TFT_BLACK, forceUpdate);

		// Results: last run and best of the session.
		const LapAnalysis::LaunchRun &best = lapAnalysis.bestRuns();
		const float lastValues[5] = {run.t100, run.t200, run.t100to200, run.t400, run.vmaxKmh};
		const float bestValues[5] = {best.t100, best.t200, best.t100to200, best.t400, best.vmaxKmh};
		for (int i = 0; i < 5; ++i)
		{
			const int y = PERF_TABLE_Y + 14 + i * PERF_ROW_H + 8;
			String last = i == 4 ? (isfinite(lastValues[i]) ? String(static_cast<int>(lroundf(lastValues[i]))) : "--")
				: performanceSeconds(lastValues[i]);
			String bestText = i == 4 ? (isfinite(bestValues[i]) ? String(static_cast<int>(lroundf(bestValues[i]))) : "--")
				: performanceSeconds(bestValues[i]);
			if (i == 3 && isfinite(run.v400Kmh)) last += String(" @") + static_cast<int>(lroundf(run.v400Kmh));
			const bool isBest = isfinite(lastValues[i]) && isfinite(bestValues[i]) &&
				fabsf(lastValues[i] - bestValues[i]) < 0.005f;
			drawAnalysisText((String("perfLast") + i).c_str(), last, 252, y, 120, MR_DATUM, &fonts::Font2,
				isBest ? analysisPurple() : TFT_WHITE, TFT_BLACK, forceUpdate);
			drawAnalysisText((String("perfBest") + i).c_str(), bestText, 314, y, 58, MR_DATUM, &fonts::Font2,
				analysisPurple(), TFT_BLACK, forceUpdate);
		}
		restoreDefaultText();
	}
