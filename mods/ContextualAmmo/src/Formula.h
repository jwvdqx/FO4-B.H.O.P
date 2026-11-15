#pragma once
// Pure math, no game types: unit-tested on the build machine (tests/test_formula.cpp).
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Formula
{
	// How dangerous the victim was. Chicken-tier ~2, raider ~20, deathclaw ~120.
	inline double Threat(int a_level, double a_maxHealth)
	{
		return std::max(0, a_level) + std::max(0.0, a_maxHealth) / 10.0;
	}

	// Chance the kill gives nothing: 95% for trivial critters, decays with threat, floor 15%.
	inline double ChanceNothing(double a_threat)
	{
		return std::clamp(0.95 * std::exp(-a_threat / 40.0), 0.15, 0.95);
	}

	// Most rounds a kill can give before caliber scaling (chicken 2, raider 4, deathclaw 9).
	inline int MaxRounds(double a_threat)
	{
		return 1 + static_cast<int>(std::lround(std::sqrt(a_threat) * 0.75));
	}

	// r1, r2, r3 are uniform [0,1). a_ammoValue = the ammo's caps value (10mm 1, .50 4, fusion core ~200).
	// Expensive ammo is divided by sqrt(value), rounded randomly, so a mini nuke is rare but possible.
	inline int Rounds(double a_threat, int a_ammoValue, double r1, double r2, double r3)
	{
		if (r1 < ChanceNothing(a_threat)) {
			return 0;
		}
		const int    max = MaxRounds(a_threat);
		const int    raw = 1 + std::min(max - 1, static_cast<int>(r2 * max));
		const double scaled = raw / std::sqrt(static_cast<double>(std::max(1, a_ammoValue)));
		return static_cast<int>(std::floor(scaled + r3));
	}
}
