#pragma once
// Immersive Mode rules, kept free of Fallout code so they can be unit-tested.

#include <cmath>
#include <string>

namespace NightRule
{
	struct Settings
	{
		bool        enabled = true;
		int         nightStart = 21;  // 24-hour clock: fast travel is blocked from this hour...
		int         nightEnd = 6;     // ...until this hour
		bool        sunMessage = true;
		std::string darkText = "It's too dark! Fast Travel unavailable.";
		std::string sunText = "The sun shines! Fast travel available.";
	};

	inline int ClampHour(int a_hour) { return a_hour < 0 ? 0 : (a_hour > 23 ? 23 : a_hour); }

	// a_hour is the game's GameHour value (0.0 - 24.0, 13.5 = 1:30 PM).
	// The window may cross midnight (21 -> 6) or not (1 -> 5). Same start and end = never night.
	inline bool IsNight(float a_hour, int a_start, int a_end)
	{
		if (!std::isfinite(a_hour)) {
			return false;
		}
		a_start = ClampHour(a_start);
		a_end = ClampHour(a_end);
		if (a_start == a_end) {
			return false;
		}
		float h = std::fmod(a_hour, 24.0f);
		if (h < 0.0f) {
			h += 24.0f;
		}
		if (a_start < a_end) {
			return h >= a_start && h < a_end;
		}
		return h >= a_start || h < a_end;
	}

	// Watches the clock and says when the sun message is due (night -> day while playing).
	class SunWatch
	{
	public:
		void Reset() { known_ = false; }

		// Returns true once, on the step where night turns into day. The first step after a
		// load (or after Immersive Mode is switched on) only learns the current state.
		bool Step(bool a_night)
		{
			const bool due = known_ && night_ && !a_night;
			night_ = a_night;
			known_ = true;
			return due;
		}

	private:
		bool known_ = false;
		bool night_ = false;
	};

	// Reads one "key=value" line of the [Immersive] section into a_s. Returns false for unknown keys.
	inline bool ApplyKey(Settings& a_s, const std::string& a_key, const std::string& a_value)
	{
		auto toInt = [&](int& a_out) {
			try {
				a_out = std::stoi(a_value);
			} catch (...) {
			}
		};
		auto toBool = [&](bool& a_out) {
			if (!a_value.empty()) {
				a_out = a_value[0] != '0' && a_value[0] != 'f' && a_value[0] != 'F';
			}
		};
		if (a_key == "bImmersiveMode") {
			toBool(a_s.enabled);
		} else if (a_key == "iNightStart") {
			toInt(a_s.nightStart);
			a_s.nightStart = ClampHour(a_s.nightStart);
		} else if (a_key == "iNightEnd") {
			toInt(a_s.nightEnd);
			a_s.nightEnd = ClampHour(a_s.nightEnd);
		} else if (a_key == "bSunMessage") {
			toBool(a_s.sunMessage);
		} else if (a_key == "sTooDarkMessage") {
			if (!a_value.empty()) {
				a_s.darkText = a_value;
			}
		} else if (a_key == "sSunMessage") {
			if (!a_value.empty()) {
				a_s.sunText = a_value;
			}
		} else {
			return false;
		}
		return true;
	}
}
