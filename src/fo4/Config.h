#pragma once
// B.H.O.P. - Settings: defaults, reading the MCM ini files, checking values.
// Part of the plugin's single build unit: included in order by main.cpp (see main.cpp).

namespace
{
	// =====================================================================
	// settings
	// =====================================================================
	enum class Mode : int
	{
		kVanilla = 0,
		kHL1 = 1,
		kHL2 = 2
	};

	struct ModeDefaults
	{
		float friction;
		float accelerate;
		float stopSpeed;  // Half-Life units per second
		float tickRate;   // ticks per second
	};

	// Half-Life 1 (GoldSrc): sv_friction 4, sv_accelerate 10, sv_stopspeed 100, 100 fps standard
	constexpr ModeDefaults kHL1Defaults{ 4.0f, 10.0f, 100.0f, 100.0f };
	// Half-Life 2 (Source): sv_friction 4, sv_accelerate 10, sv_stopspeed 100, 66.67 tick
	constexpr ModeDefaults kHL2Defaults{ 4.0f, 10.0f, 100.0f, 1.0f / 0.015f };

	struct Config
	{
		Mode mode{ Mode::kHL2 };
		bool debugLog{ false };
		bool enableInPowerArmor{ true };
		bool wallClip{ true };
		bool customGround{ false };  // false = use the selected game's friction/accelerate/stop speed
		int  smoothing{ 2 };         // 0 = exact tick, 1 = exact tick + interpolation, 2 = every frame (most responsive)

		// ---- camera (Half-Life view effects)
		int   viewBob{ 1 };   // 0 off, 1 = like the game (HL1 on, HL2 off), 2 on
		float bobScale{ 1.0f };
		int   viewRoll{ 1 };  // 0 off, 1 = like the game (HL1 on, HL2 off), 2 on
		float rollAngle{ 2.0f };
		float rollSpeed{ 200.0f };
		bool  landingPunch{ true };
		float punchScale{ 1.0f };

		// negative = use the mode's default
		float friction{ -1.0f };
		float accelerate{ -1.0f };
		float stopSpeed{ -1.0f };
		float tickRate{ -1.0f };

		bool  carryWeight{ true };
		float fullLoadAccelMult{ 0.80f };
		float overencumberedAccelMult{ 0.60f };
		float fullLoadFrictionMult{ 1.0f };
		float powerArmorAccelMult{ 0.75f };
		float powerArmorFrictionMult{ 1.0f };
		float responsiveness{ 1.0f };  // multiplies friction and acceleration (1 = Half-Life)

		// ---- input + speeds
		bool  directInput{ true };   // read the stick directly (skips Fallout's own speed ramp)
		int   speedMode{ 0 };        // 0 = Fallout speeds, 1 = Half-Life speeds
		int   hl1Sprint{ 0 };        // HL1 speeds: 0 = sprint key does nothing, 1 = sprint is faster
		float hl1SprintMult{ 1.35f };
		float hlPowerArmorSpeedMult{ 0.85f };

		// ---- air + bunnyhop
		bool  airControl{ true };
		float airAccelerate{ 10.0f };  // sv_airaccelerate
		float airCap{ 30.0f };         // Half-Life caps air wish speed at 30
		float strafeAssist{ 0.5f };    // 0 = pure Half-Life, 1 = full help for sticks
		bool  bhop{ true };
		float bhopGrace{ 0.05f };      // seconds after landing without friction (original games: one frame)
		int   autoJump{ 1 };           // 0 off, 1 with Action Boy/Girl at max rank, 2 always
		bool  pushes{ true };          // hits/explosions add to your momentum (Half-Life style)
		bool  crouchJump{ true };      // crouch (sneak key) in the air lifts your feet like Half-Life's duck-jump
		int   crouchJumpHeight{ 0 };   // 0 = exact per game (HL1 18, HL2 36 units), 1 = scaled to Fallout's jump
		float crouchJumpWindow{ 0.15f };  // seconds after the top of the jump it still works
		bool  crouchLandSneak{ true };  // land sneaking after a crouch-jump (Half-Life keeps you ducked)
		int   edgeFriction{ 0 };       // 0 off, 1 HL1 mode only, 2 both modes
		float edgeFrictionMult{ 2.0f };  // sv_edgefriction
		float pushThreshold{ 60.0f };  // HL units/s of sudden extra speed that counts as a push
		bool  hl1BhopCap{ true };
		bool  hl2JumpBoost{ true };
		bool  hl2ABH{ true };
		int   hl2AirFriction{ 2 };     // HL2 rising air friction: 0 off, 1 exact (140 u/s), 2 scaled to Fallout's jump

		// ---- surfaces
		bool  surfaceFriction{ true };
		float surfaceStrength{ 1.0f };

		// ---- perks: 0 off, 1 bonus, 2 unlock
		int   perkMode{ 1 };
		// perk names to look for (lower case, '|' between alternatives) - change for non-English games
		std::string perkNames[6]{ "moving target", "strong back", "adamantium skeleton", "aquaboy|aquagirl", "sneak", "action boy|action girl" };

		// 1 Half-Life unit (1 inch) in Havok units (metres)
		float unitScale{ 0.0254f };

		// ---- sounds
		bool  sounds{ true };
		int   soundSet{ 0 };  // 0 = follow iMode, 1 = Half-Life 1, 2 = Half-Life 2
		bool  muteVanillaFootsteps{ true };
		bool  jumpLandSounds{ true };
		bool  waterSounds{ true };
		bool  swimSounds{ false };
		bool  powerArmorHardboot{ true };
		bool  hl1FallPain{ true };
		bool  useVanillaOutput{ true };  // reuse the vanilla footstep output model (reverb, distance)
		float volume{ 1.0f };
		float runVolume{ 0.5f };
		float walkVolume{ 0.2f };
		float sneakVolumeMult{ 1.0f };  // extra; Half-Life already makes ducking steps quieter
		float runStepSpeed{ 0.0f };  // HL units/s where steps switch to running; 0 = automatic (scaled from Half-Life)
		float minStepSpeed{ 10.0f };
		float runInterval{ 0.3f };
		float walkInterval{ 0.4f };
		float sneakExtraInterval{ 0.1f };
		float waterInterval{ 0.6f };
		float landSoftSpeed{ 150.0f };
		float landHardSpeed{ 350.0f };
		float fallPainSpeed{ 580.0f };
		std::uint32_t fileSoundFlags{ 0x1A };
		std::uint32_t filePriority{ 1 };
		std::vector<std::pair<std::string, std::string>> materialOverrides;
	} g_cfg;

	struct Active
	{
		float friction;
		float accelerate;
		float stopSpeed;
		float tick;  // seconds per tick
	} g_active{};

	std::string Trim(std::string s)
	{
		const auto b = s.find_first_not_of(" \t\r\n");
		if (b == std::string::npos) {
			return {};
		}
		const auto e = s.find_last_not_of(" \t\r\n");
		return s.substr(b, e - b + 1);
	}

	std::string Lower(std::string s)
	{
		for (auto& c : s) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return s;
	}

	bool ToBool(const std::string& v)
	{
		return v == "1" || v == "true" || v == "True" || v == "TRUE";
	}

	// Reads one ini file; later files override earlier ones.
	bool ReadIni(const char* a_path)
	{
		std::ifstream f(a_path);
		if (!f) {
			return false;
		} else {
			std::string line;
			while (std::getline(f, line)) {
				line = Trim(line);
				if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') {
					continue;
				}
				const auto eq = line.find('=');
				if (eq == std::string::npos) {
					continue;
				}
				const auto key = Trim(line.substr(0, eq));
				auto       val = Trim(line.substr(eq + 1));
				if (const auto c = val.find(';'); c != std::string::npos) {
					val = Trim(val.substr(0, c));
				}
				try {
					auto F = [&](const char* k, float& out) {
						if (key == k) {
							out = std::stof(val);
						}
					};
					auto B = [&](const char* k, bool& out) {
						if (key == k) {
							out = ToBool(val);
						}
					};
					if (key == "iMode") {
						g_cfg.mode = static_cast<Mode>(std::clamp(std::stoi(val), 0, 2));
					} else if (key == "iSoundSet") {
						g_cfg.soundSet = std::clamp(std::stoi(val), 0, 2);
					} else if (key == "iFileSoundFlags") {
						g_cfg.fileSoundFlags = static_cast<std::uint32_t>(std::stoul(val, nullptr, 0));
					} else if (key == "iFilePriority") {
						g_cfg.filePriority = static_cast<std::uint32_t>(std::stoul(val, nullptr, 0));
					} else if (key.rfind("Material.", 0) == 0) {
						g_cfg.materialOverrides.emplace_back(Lower(key.substr(9)), Lower(val));
					}
					B("bDebugLog", g_cfg.debugLog);
					B("bEnableInPowerArmor", g_cfg.enableInPowerArmor);
					B("bWallClip", g_cfg.wallClip);
					B("bCustomGround", g_cfg.customGround);
					if (key == "iSmoothing") {
						g_cfg.smoothing = std::clamp(std::stoi(val), 0, 2);
					} else if (key == "iViewBob") {
						g_cfg.viewBob = std::clamp(std::stoi(val), 0, 2);
					} else if (key == "iViewRoll") {
						g_cfg.viewRoll = std::clamp(std::stoi(val), 0, 2);
					}
					F("fResponsiveness", g_cfg.responsiveness);
					B("bDirectInput", g_cfg.directInput);
					if (key == "iSpeedMode") {
						g_cfg.speedMode = std::clamp(std::stoi(val), 0, 1);
					} else if (key == "iHL1Sprint") {
						g_cfg.hl1Sprint = std::clamp(std::stoi(val), 0, 1);
					} else if (key == "iPerkMode") {
						g_cfg.perkMode = std::clamp(std::stoi(val), 0, 2);
					}
					F("fHL1SprintMult", g_cfg.hl1SprintMult);
					F("fHLPowerArmorSpeedMult", g_cfg.hlPowerArmorSpeedMult);
					B("bAirControl", g_cfg.airControl);
					F("fAirAccelerate", g_cfg.airAccelerate);
					F("fAirSpeedCap", g_cfg.airCap);
					F("fStrafeAssist", g_cfg.strafeAssist);
					B("bBunnyhop", g_cfg.bhop);
					F("fBhopGrace", g_cfg.bhopGrace);
					if (key == "iAutoJump") {
						g_cfg.autoJump = std::clamp(std::stoi(val), 0, 2);
					}
					B("bPushes", g_cfg.pushes);
					B("bCrouchJump", g_cfg.crouchJump);
					B("bCrouchLandSneak", g_cfg.crouchLandSneak);
					F("fCrouchJumpWindow", g_cfg.crouchJumpWindow);
					F("fEdgeFrictionMult", g_cfg.edgeFrictionMult);
					if (key == "iCrouchJumpHeight") {
						g_cfg.crouchJumpHeight = std::clamp(std::stoi(val), 0, 1);
					} else if (key == "iEdgeFriction") {
						g_cfg.edgeFriction = std::clamp(std::stoi(val), 0, 2);
					}
					F("fPushThreshold", g_cfg.pushThreshold);
					B("bHL1BhopCap", g_cfg.hl1BhopCap);
					B("bHL2JumpBoost", g_cfg.hl2JumpBoost);
					B("bHL2ABH", g_cfg.hl2ABH);
					if (key == "iHL2AirFriction") {
						g_cfg.hl2AirFriction = std::clamp(std::stoi(val), 0, 2);
					}
					{
						static constexpr const char* kPerkKeys[6] = { "sPerkMovingTarget", "sPerkStrongBack", "sPerkAdamantiumSkeleton", "sPerkAquaboy", "sPerkSneak", "sPerkActionBoy" };
						for (int i = 0; i < 6; ++i) {
							if (key == kPerkKeys[i] && !val.empty()) {
								g_cfg.perkNames[i] = Lower(val);
							}
						}
					}
					B("bSurfaceFriction", g_cfg.surfaceFriction);
					F("fSurfaceFrictionStrength", g_cfg.surfaceStrength);
					F("fBobScale", g_cfg.bobScale);
					F("fRollAngle", g_cfg.rollAngle);
					F("fRollSpeed", g_cfg.rollSpeed);
					B("bLandingPunch", g_cfg.landingPunch);
					F("fPunchScale", g_cfg.punchScale);
					F("fFriction", g_cfg.friction);
					F("fAccelerate", g_cfg.accelerate);
					F("fStopSpeed", g_cfg.stopSpeed);
					F("fTickRate", g_cfg.tickRate);
					B("bCarryWeightAffectsMovement", g_cfg.carryWeight);
					F("fFullLoadAccelMult", g_cfg.fullLoadAccelMult);
					F("fOverencumberedAccelMult", g_cfg.overencumberedAccelMult);
					F("fFullLoadFrictionMult", g_cfg.fullLoadFrictionMult);
					F("fPowerArmorAccelMult", g_cfg.powerArmorAccelMult);
					F("fPowerArmorFrictionMult", g_cfg.powerArmorFrictionMult);
					F("fUnitScale", g_cfg.unitScale);
					B("bHalfLifeSounds", g_cfg.sounds);
					B("bMuteVanillaPlayerFootsteps", g_cfg.muteVanillaFootsteps);
					B("bJumpLandSounds", g_cfg.jumpLandSounds);
					B("bWaterSounds", g_cfg.waterSounds);
					B("bSwimSounds", g_cfg.swimSounds);
					B("bPowerArmorHardboot", g_cfg.powerArmorHardboot);
					B("bHL1FallPainSound", g_cfg.hl1FallPain);
					B("bUseVanillaReverb", g_cfg.useVanillaOutput);
					F("fVolume", g_cfg.volume);
					F("fRunVolume", g_cfg.runVolume);
					F("fWalkVolume", g_cfg.walkVolume);
					F("fSneakVolumeMult", g_cfg.sneakVolumeMult);
					F("fRunStepSpeed", g_cfg.runStepSpeed);
					F("fMinStepSpeed", g_cfg.minStepSpeed);
					F("fRunStepInterval", g_cfg.runInterval);
					F("fWalkStepInterval", g_cfg.walkInterval);
					F("fSneakExtraInterval", g_cfg.sneakExtraInterval);
					F("fWaterStepInterval", g_cfg.waterInterval);
					F("fLandSoftSpeed", g_cfg.landSoftSpeed);
					F("fLandHardSpeed", g_cfg.landHardSpeed);
					F("fFallPainSpeed", g_cfg.fallPainSpeed);
				} catch (...) {
					REX::WARN("Bad ini line: {}", line);
				}
			}
		}
		return true;
	}

	// Defaults ship in MCM\Config\BHOP\settings.ini; MCM saves changes to MCM\Settings\BHOP.ini.
	// Keeps every setting inside a sensible range, whatever the ini says.
	void ValidateConfig()
	{
		auto& c = g_cfg;
		auto  F = [](float& v, float lo, float hi, float def) {
			if (!std::isfinite(v)) {
				v = def;
			}
			v = std::clamp(v, lo, hi);
		};
		F(c.friction, 0.0f, 20.0f, 4.0f);
		F(c.accelerate, 0.0f, 50.0f, 10.0f);
		F(c.stopSpeed, 0.0f, 500.0f, 100.0f);
		if (c.tickRate > 0.0f) {
			F(c.tickRate, 20.0f, 500.0f, 66.67f);
		}
		F(c.responsiveness, 0.25f, 4.0f, 1.3f);
		F(c.fullLoadAccelMult, 0.05f, 1.0f, 0.8f);
		F(c.overencumberedAccelMult, 0.05f, 1.0f, 0.6f);
		F(c.fullLoadFrictionMult, 0.05f, 2.0f, 1.0f);
		F(c.powerArmorAccelMult, 0.05f, 2.0f, 0.75f);
		F(c.powerArmorFrictionMult, 0.05f, 2.0f, 1.0f);
		F(c.unitScale, 0.001f, 1.0f, 0.0254f);
		F(c.hl1SprintMult, 1.0f, 3.0f, 1.35f);
		F(c.hlPowerArmorSpeedMult, 0.1f, 2.0f, 0.85f);
		F(c.airAccelerate, 0.0f, 100.0f, 10.0f);
		F(c.airCap, 1.0f, 500.0f, 30.0f);
		F(c.strafeAssist, 0.0f, 1.0f, 0.5f);
		F(c.bhopGrace, 0.0f, 0.5f, 0.05f);
		F(c.pushThreshold, 10.0f, 1000.0f, 60.0f);
		F(c.crouchJumpWindow, 0.0f, 1.0f, 0.15f);
		F(c.edgeFrictionMult, 1.0f, 10.0f, 2.0f);
		F(c.surfaceStrength, 0.0f, 1.0f, 1.0f);
		F(c.bobScale, 0.0f, 5.0f, 1.0f);
		F(c.rollAngle, -15.0f, 15.0f, 2.0f);
		F(c.rollSpeed, 1.0f, 1000.0f, 200.0f);
		F(c.punchScale, 0.0f, 5.0f, 1.0f);
		F(c.volume, 0.0f, 4.0f, 1.0f);
		F(c.runVolume, 0.0f, 1.0f, 0.5f);
		F(c.walkVolume, 0.0f, 1.0f, 0.2f);
		F(c.sneakVolumeMult, 0.0f, 1.0f, 1.0f);
		F(c.runStepSpeed, 0.0f, 1000.0f, 0.0f);
		F(c.minStepSpeed, 0.0f, 200.0f, 10.0f);
		F(c.runInterval, 0.05f, 2.0f, 0.3f);
		F(c.walkInterval, 0.05f, 2.0f, 0.4f);
		F(c.sneakExtraInterval, 0.0f, 1.0f, 0.1f);
		F(c.waterInterval, 0.05f, 2.0f, 0.6f);
		F(c.landSoftSpeed, 0.0f, 2000.0f, 150.0f);
		F(c.landHardSpeed, 0.0f, 2000.0f, 350.0f);
		F(c.fallPainSpeed, 0.0f, 5000.0f, 580.0f);
		c.filePriority = std::min<std::uint32_t>(c.filePriority, 255);
	}

	void LoadConfig()
	{
		g_cfg = Config{};  // start from clean defaults, so a removed line doesn't keep an old value
		const bool defaults = ReadIni("Data/MCM/Config/BHOP/settings.ini");
		const bool user = ReadIni("Data/MCM/Settings/BHOP.ini");
		ValidateConfig();
		REX::INFO("Settings: defaults {}, MCM changes {}", defaults ? "found" : "MISSING", user ? "found" : "none yet");

		const auto& d = g_cfg.mode == Mode::kHL1 ? kHL1Defaults : kHL2Defaults;
		const bool  custom = g_cfg.customGround;
		g_active.friction = custom && g_cfg.friction >= 0.0f ? g_cfg.friction : d.friction;
		g_active.accelerate = custom && g_cfg.accelerate >= 0.0f ? g_cfg.accelerate : d.accelerate;
		g_active.stopSpeed = custom && g_cfg.stopSpeed >= 0.0f ? g_cfg.stopSpeed : d.stopSpeed;
		const float resp = std::clamp(g_cfg.responsiveness, 0.25f, 4.0f);
		g_active.friction *= resp;
		g_active.accelerate *= resp;
		const float rate = g_cfg.tickRate > 0.0f ? g_cfg.tickRate : d.tickRate;
		g_active.tick = 1.0f / std::clamp(rate, 20.0f, 500.0f);
		if (g_cfg.unitScale <= 0.0f) {
			g_cfg.unitScale = 0.0254f;
		}

		static constexpr const char* kNames[] = { "Vanilla", "Half-Life 1", "Half-Life 2" };
		REX::INFO("Mode: {} | friction {} accelerate {} stopspeed {} tick {:.2f}/s | unit scale {}",
			kNames[static_cast<int>(g_cfg.mode)], g_active.friction, g_active.accelerate, g_active.stopSpeed,
			1.0f / g_active.tick, g_cfg.unitScale);
	}

	// Which sound set is in use: 0 = none, 1 = HL1, 2 = HL2
	int SoundSet()
	{
		if (!g_cfg.sounds) {
			return 0;
		}
		if (g_cfg.soundSet != 0) {
			return g_cfg.soundSet;
		}
		return static_cast<int>(g_cfg.mode);
	}

	long long NowMs()
	{
		using namespace std::chrono;
		return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
	}
}
