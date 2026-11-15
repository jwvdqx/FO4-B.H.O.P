#pragma once
// Pure math, no game types: unit-tested on the build machine (tests/test_formula.cpp).
#include <algorithm>
#include <cmath>

namespace Formula
{
	// Explosions at or above this damage (nuke mines, Nuka grenades, mini nukes...) are never paid out.
	inline constexpr float kHeavyExplosionDamage = 400.0F;
	// Grenades must deal this share of max health (cumulative) or land the kill.
	inline constexpr double kGrenadeShare = 0.20;
	// Chance a grenade kill sets off the victim's own grenades.
	inline constexpr double kChainChance = 0.30;

	// How dangerous the victim was. Chicken-tier ~2, raider ~20, deathclaw ~120.
	inline double Threat(int a_level, double a_maxHealth)
	{
		return std::max(0, a_level) + std::max(0.0, a_maxHealth) / 10.0;
	}

	// Chance the kill gives nothing: 95% for trivial critters, decays with threat, floor 15%.
	// Scrounger (rank 0-4) cuts it by 10% per rank.
	inline double ChanceNothing(double a_threat, int a_scrounger = 0)
	{
		const double base = std::clamp(0.95 * std::exp(-a_threat / 40.0), 0.15, 0.95);
		return base * (1.0 - 0.10 * std::clamp(a_scrounger, 0, 4));
	}

	// Most rounds a kill can give before caliber scaling (chicken 2, raider 4, deathclaw 9).
	inline int MaxRounds(double a_threat)
	{
		return 1 + static_cast<int>(std::lround(std::sqrt(a_threat) * 0.75));
	}

	// r1, r2, r3 uniform [0,1). a_ammoValue = caps value (10mm 1, .50 4, fusion core ~200).
	// Expensive ammo is divided by sqrt(value), rounded randomly. Scrounger: +25% rounds per rank.
	inline int Rounds(double a_threat, int a_ammoValue, double r1, double r2, double r3, int a_scrounger = 0)
	{
		if (r1 < ChanceNothing(a_threat, a_scrounger)) {
			return 0;
		}
		const int    max = MaxRounds(a_threat);
		const int    raw = 1 + std::min(max - 1, static_cast<int>(r2 * max));
		const double scaled = raw * (1.0 + 0.25 * std::clamp(a_scrounger, 0, 4)) /
		                      std::sqrt(static_cast<double>(std::max(1, a_ammoValue)));
		return static_cast<int>(std::floor(scaled + r3));
	}

	// Grenades back: 0..max, max 1 (raider) -> 2 (gunner) -> 3 (overlord-tier).
	inline int Grenades(double a_threat, double r)
	{
		const int max = std::clamp(1 + static_cast<int>(a_threat / 50.0), 1, 3);
		return std::min(max, static_cast<int>(r * (max + 1)));
	}

	inline bool GrenadeQualifies(double a_grenadeDamage, double a_maxHealth, bool a_grenadeKill)
	{
		return a_grenadeKill || (a_maxHealth > 0.0 && a_grenadeDamage >= kGrenadeShare * a_maxHealth);
	}
}
