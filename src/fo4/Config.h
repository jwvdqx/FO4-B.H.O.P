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
		bool enabled{ true };         // master switch: off = plain Fallout, nothing of the mod runs
		int  profile{ 0 };            // 0 = your own settings, 1 = full Half-Life 1, 2 = full Half-Life 2, 3 = Claude's choice
		Mode mode{ Mode::kHL2 };
		bool debugLog{ false };
		bool enableInPowerArmor{ true };
		bool wallClip{ true };
		float slopeAngle{ 45.0f };    // Half-Life slopes: up to this steep you keep your heading and speed (degrees, 0 = off)
		bool wallProbe{ true };       // check the real wall surface to fine-tune the wall direction (experiment)
		int  swimMode{ 1 };           // 0 = Fallout swimming, 1 = Half-Life swimming (flat; Fallout keeps floating and diving)
		bool waterSurf{ true };       // hold jump at the surface while fast: skim along the water like Half-Life
		bool sprintSound{ true };     // HL2 suit sound when you start sprinting
		int  jumpPhysics{ 0 };        // 0 = Fallout jump and gravity, 1 = HL gravity with Fallout's jump height, 2 = exact HL
		bool record{ false };         // write a movement recording after closing the pause menu
		float recordSeconds{ 15.0f };
		bool customGround{ false };  // false = use the selected game's friction/accelerate/stop speed
		int  smoothing{ 2 };         // 0 = exact tick (delayed), 1 = exact tick + blending, 2 = every frame in Half-Life-sized steps (no delay)

		// ---- camera (Half-Life view effects)
		int   viewBob{ 1 };   // 0 off, 1 = like the game (HL1 on, HL2 off), 2 on
		float bobScale{ 1.0f };
		int   viewRoll{ 1 };  // 0 off, 1 = like the game (HL1 on, HL2 off), 2 on
		float rollAngle{ 2.0f };
		float rollSpeed{ 200.0f };
		bool  landingPunch{ true };
		int   falloutCamera{ 1 };  // Fallout's own first person camera movement: 0 = keep, 1 = off while jumping and landing, 2 = always off
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
		float baseSpeedCap{ 0.0f };  // HL units/s: your base speed never goes above this (0 = no cap); buffs and perks apply after

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
		int   crouchJumpKey{ 1 };      // 0 = sneak key only, 1 = sneak key, or jump pressed again on the way up
		float jumpBuffer{ 0.1f };      // seconds: jump pressed this long before landing still jumps (0 = off)
		float ledgeJump{ 0.1f };       // seconds after running off a ledge you can still jump (0 = off)
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
		int   perkMessages{ 2 };  // on-screen notes: 0 off, 1 when you gain a rank, 2 also one line after loading
		// perk names to look for (lower case, '|' between alternatives) - change for non-English games
		std::string perkNames[6]{ "moving target", "strong back", "adamantium skeleton", "aquaboy|aquagirl", "sneak", "action boy|action girl" };

		// 1 Half-Life unit (1 inch) in Havok units (metres)
		float unitScale{ 0.0254f };

		// ---- sounds
		bool  sounds{ true };
		int   soundSet{ 0 };  // 0 = follow iMode, 1 = Half-Life 1, 2 = Half-Life 2
		int   surfaceSet[17]{};  // per surface (kSurfaces): 0 = the sound set above, 1 = Half-Life 1, 2 = Half-Life 2
		bool  muteVanillaFootsteps{ true };
		bool  animSpeed{ false };     // third person: walk/run animation speed follows your real speed (experiment)
		int   thirdPersonSteps{ 1 };  // third person: 0 = Half-Life's step timing, 1 = steps follow the walking animation
		bool  jumpLandSounds{ true };
		bool  waterSounds{ true };
		bool  swimSounds{ true };
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

	// Footstep surfaces you can give their own game (MCM -> Sounds -> Sounds by surface).
	// Each lists the surface categories it covers (see Sounds::CategoryFromName).
	struct SurfaceKey
	{
		const char* key;
		const char* cats[3];
	};
	inline constexpr SurfaceKey kSurfaces[17]{
		{ "iSurfaceConcrete", { "concrete" } },
		{ "iSurfaceMetal", { "metal" } },
		{ "iSurfaceGrate", { "grate", "chainlink" } },
		{ "iSurfaceDuct", { "duct" } },
		{ "iSurfaceWood", { "wood" } },
		{ "iSurfaceWoodPanel", { "woodpanel" } },
		{ "iSurfaceTile", { "tile", "glass", "ice" } },
		{ "iSurfaceDirt", { "dirt" } },
		{ "iSurfaceGrass", { "grass" } },
		{ "iSurfaceGravel", { "gravel" } },
		{ "iSurfaceMud", { "mud" } },
		{ "iSurfaceSand", { "sand" } },
		{ "iSurfaceSnow", { "snow" } },
		{ "iSurfaceCarpet", { "carpet" } },
		{ "iSurfaceWater", { "slosh", "wade" } },
		{ "iSurfaceSwim", { "swim" } },
		{ "iSurfacePowerArmor", { "powerarmor" } },
	};

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
					B("bEnabled", g_cfg.enabled);
					if (key == "iProfile") {
						g_cfg.profile = std::clamp(std::stoi(val), 0, 3);
					}
					B("bDebugLog", g_cfg.debugLog);
					B("bEnableInPowerArmor", g_cfg.enableInPowerArmor);
					B("bWallClip", g_cfg.wallClip);
					B("bWallProbe", g_cfg.wallProbe);
					B("bThirdPersonAnimSpeed", g_cfg.animSpeed);
					F("fSlopeAngle", g_cfg.slopeAngle);
					B("bWaterSurf", g_cfg.waterSurf);
					B("bSprintSound", g_cfg.sprintSound);
					if (key == "iSwimMode") {
						g_cfg.swimMode = std::clamp(std::stoi(val), 0, 1);
					}
					B("bRecord", g_cfg.record);
					F("fRecordSeconds", g_cfg.recordSeconds);
					for (int i = 0; i < 17; ++i) {
						if (key == kSurfaces[i].key) {
							g_cfg.surfaceSet[i] = std::clamp(std::stoi(val), 0, 2);
						}
					}
					if (key == "iJumpPhysics") {
						g_cfg.jumpPhysics = std::clamp(std::stoi(val), 0, 2);
					}
					B("bCustomGround", g_cfg.customGround);
					if (key == "iSmoothing") {
						g_cfg.smoothing = std::clamp(std::stoi(val), 0, 2);
					} else if (key == "iViewBob") {
						g_cfg.viewBob = std::clamp(std::stoi(val), 0, 2);
					} else if (key == "iFalloutCameraMotion") {
						g_cfg.falloutCamera = std::clamp(std::stoi(val), 0, 2);
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
					F("fBaseSpeedCap", g_cfg.baseSpeedCap);
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
					F("fJumpBuffer", g_cfg.jumpBuffer);
					F("fLedgeJump", g_cfg.ledgeJump);
					if (key == "iCrouchJumpKey") {
						g_cfg.crouchJumpKey = std::clamp(std::stoi(val), 0, 1);
					} else if (key == "iPerkMessages") {
						g_cfg.perkMessages = std::clamp(std::stoi(val), 0, 2);
					} else if (key == "iThirdPersonSteps") {
						g_cfg.thirdPersonSteps = std::clamp(std::stoi(val), 0, 1);
					}
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
		F(c.responsiveness, 0.25f, 4.0f, 1.0f);
		F(c.fullLoadAccelMult, 0.05f, 1.0f, 0.8f);
		F(c.overencumberedAccelMult, 0.05f, 1.0f, 0.6f);
		F(c.fullLoadFrictionMult, 0.05f, 2.0f, 1.0f);
		F(c.powerArmorAccelMult, 0.05f, 2.0f, 0.75f);
		F(c.powerArmorFrictionMult, 0.05f, 2.0f, 1.0f);
		F(c.unitScale, 0.001f, 1.0f, 0.0254f);
		F(c.hl1SprintMult, 1.0f, 3.0f, 1.35f);
		F(c.baseSpeedCap, 0.0f, 1000.0f, 0.0f);
		F(c.slopeAngle, 0.0f, 60.0f, 45.0f);
		F(c.hlPowerArmorSpeedMult, 0.1f, 2.0f, 0.85f);
		F(c.airAccelerate, 0.0f, 100.0f, 10.0f);
		F(c.airCap, 1.0f, 500.0f, 30.0f);
		F(c.strafeAssist, 0.0f, 1.0f, 0.5f);
		F(c.bhopGrace, 0.0f, 0.5f, 0.05f);
		F(c.pushThreshold, 10.0f, 1000.0f, 60.0f);
		F(c.recordSeconds, 1.0f, 120.0f, 15.0f);
		F(c.crouchJumpWindow, 0.0f, 1.0f, 0.15f);
		F(c.jumpBuffer, 0.0f, 0.3f, 0.1f);
		F(c.ledgeJump, 0.0f, 0.3f, 0.1f);
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

	// Profiles. Your own settings are whatever MCM saved (MCM\Settings\BHOP.ini): that file is only ever
	// read here, never written, so picking another profile can't change it. A ready-made profile is laid
	// over the settings in memory; going back to "My settings" brings yours back exactly as you left them.
	void ApplyProfile()
	{
		const int profile = g_cfg.profile;
		if (profile == 0) {
			return;
		}
		Config c{};  // the mod's own defaults: nothing of your settings leaks into a ready-made profile...
		// ...except what isn't about how it plays: the switches themselves, your volume, language and debug options
		c.enabled = g_cfg.enabled;
		c.profile = profile;
		c.debugLog = g_cfg.debugLog;
		c.record = g_cfg.record;
		c.recordSeconds = g_cfg.recordSeconds;
		c.volume = g_cfg.volume;
		c.muteVanillaFootsteps = g_cfg.muteVanillaFootsteps;
		c.useVanillaOutput = g_cfg.useVanillaOutput;
		c.fileSoundFlags = g_cfg.fileSoundFlags;
		c.filePriority = g_cfg.filePriority;
		c.materialOverrides = g_cfg.materialOverrides;
		c.unitScale = g_cfg.unitScale;
		c.perkMessages = g_cfg.perkMessages;
		c.enableInPowerArmor = g_cfg.enableInPowerArmor;
		c.animSpeed = g_cfg.animSpeed;
		for (int i = 0; i < 6; ++i) {
			c.perkNames[i] = g_cfg.perkNames[i];
		}
		if (profile == 1 || profile == 2) {
			// the original game, nothing added: its speeds, its jump, its timing, no helpers, no Fallout extras
			c.mode = profile == 1 ? Mode::kHL1 : Mode::kHL2;
			c.soundSet = 0;
			c.speedMode = 1;
			c.hl1Sprint = 0;
			c.jumpPhysics = 2;
			c.hl2AirFriction = 1;
			c.strafeAssist = 0.0f;
			c.bhopGrace = 0.02f;  // the originals give you one frame
			c.autoJump = 0;
			c.jumpBuffer = 0.0f;
			c.ledgeJump = 0.0f;
			c.crouchJumpKey = 0;
			c.crouchJumpHeight = 0;
			c.edgeFriction = 1;  // Half-Life 1 only
			c.perkMode = 0;
			c.carryWeight = false;
			c.surfaceFriction = false;
			c.powerArmorAccelMult = 1.0f;
			c.powerArmorFrictionMult = 1.0f;
			c.hlPowerArmorSpeedMult = 1.0f;
			c.thirdPersonSteps = 0;
			c.slopeAngle = 45.57f;  // floor normal z 0.7, as in both games
			c.falloutCamera = 2;    // no camera movement that isn't Half-Life's
			c.viewBob = 1;
			c.viewRoll = 1;
		} else {
			// Claude's choice: Half-Life 2's feel, tuned to sit well in Fallout and on a gamepad
			c.mode = Mode::kHL2;
			c.soundSet = 0;
			c.speedMode = 0;         // Fallout's speeds, so fights and perks stay balanced
			c.jumpPhysics = 0;       // Fallout's jump and fall damage
			c.hl2AirFriction = 2;
			c.strafeAssist = 0.6f;
			c.bhopGrace = 0.08f;
			c.autoJump = 1;
			c.jumpBuffer = 0.12f;
			c.ledgeJump = 0.1f;
			c.crouchJumpKey = 1;
			c.crouchJumpHeight = 1;
			c.perkMode = 1;
			c.thirdPersonSteps = 1;
			c.viewBob = 0;
			c.viewRoll = 2;
			c.rollAngle = 1.5f;
		}
		g_cfg = std::move(c);
	}

	void LoadConfig()
	{
		g_cfg = Config{};  // start from clean defaults, so a removed line doesn't keep an old value
		const bool defaults = ReadIni("Data/MCM/Config/BHOP/settings.ini");
		const bool user = ReadIni("Data/MCM/Settings/BHOP.ini");
		ApplyProfile();
		ValidateConfig();
		static constexpr const char* kProfiles[] = { "My settings", "Full Half-Life 1", "Full Half-Life 2", "Claude's choice" };
		REX::INFO("Mod {} | profile: {}", g_cfg.enabled ? "ON" : "OFF (master switch)", kProfiles[g_cfg.profile]);
		if (!g_cfg.enabled) {
			g_cfg.mode = Mode::kVanilla;  // master switch off: plain Fallout movement, camera and sounds
			g_cfg.sounds = false;
		}
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
