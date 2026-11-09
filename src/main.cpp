// B.H.O.P. - Boston Hopping Overhaul Project
//
// Stage 1: Half-Life ground acceleration, friction and weight.
// Sounds:  Half-Life 1 / 2 player footsteps, jump and landing sounds, picked by the
//          surface you stand on, timed the way Half-Life times them. The player's
//          vanilla footsteps are muted; NPC footsteps are left alone.
//
// Movement: the player's character controller is given its new velocity every
// physics step through SetLinearVelocityImpl. Vanilla Fallout passes the speed it
// wants you to move at, applied instantly. We treat that as Half-Life's "wish
// velocity" and run Half-Life's friction + acceleration on our own velocity.

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

	// =====================================================================
	// memory + RTTI helpers (used to find things without hard-coded addresses)
	// =====================================================================
	bool Readable(const void* a_ptr, std::size_t a_size)
	{
		if (!a_ptr) {
			return false;
		}
		REX::W32::MEMORY_BASIC_INFORMATION mbi{};
		if (!REX::W32::VirtualQuery(a_ptr, &mbi, sizeof(mbi))) {
			return false;
		}
		if (mbi.state != REX::W32::MEM_COMMIT || (mbi.protect & 0x101u) != 0 || mbi.protect == 0) {  // NOACCESS | GUARD
			return false;
		}
		const auto end = reinterpret_cast<std::uintptr_t>(mbi.baseAddress) + mbi.regionSize;
		return reinterpret_cast<std::uintptr_t>(a_ptr) + a_size <= end;
	}

	struct Section
	{
		std::uintptr_t start;
		std::size_t    size;
		bool           exec;
	};

	struct GameModule
	{
		std::uintptr_t       base{ 0 };
		std::uintptr_t       end{ 0 };
		std::vector<Section> data;  // non-executable sections (.rdata, .data)

		bool Contains(std::uintptr_t a_addr, std::size_t a_size = 1) const { return a_addr >= base && a_addr + a_size <= end; }
	};

	const GameModule& Game()
	{
		static GameModule mod = []() {
			GameModule m;
			m.base = reinterpret_cast<std::uintptr_t>(REX::W32::GetModuleHandleW(nullptr));
			if (!m.base) {
				return m;
			}
			const auto nt = m.base + *reinterpret_cast<const std::uint32_t*>(m.base + 0x3C);
			const auto numSections = *reinterpret_cast<const std::uint16_t*>(nt + 6);
			const auto optSize = *reinterpret_cast<const std::uint16_t*>(nt + 20);
			const auto imageSize = *reinterpret_cast<const std::uint32_t*>(nt + 24 + 56);
			m.end = m.base + imageSize;
			auto sec = nt + 24 + optSize;
			for (std::uint16_t i = 0; i < numSections; ++i, sec += 40) {
				const auto vsize = *reinterpret_cast<const std::uint32_t*>(sec + 8);
				const auto rva = *reinterpret_cast<const std::uint32_t*>(sec + 12);
				const auto chars = *reinterpret_cast<const std::uint32_t*>(sec + 36);
				const bool exec = (chars & 0x20000000u) != 0;
				if (!exec) {
					m.data.push_back({ m.base + rva, vsize, exec });
				}
			}
			return m;
		}();
		return mod;
	}

	// ".?AVBGSSoundOutput@@" etc, or nullptr
	const char* RttiName(const void* a_obj)
	{
		const auto& g = Game();
		if (!g.base || !Readable(a_obj, 8)) {
			return nullptr;
		}
		const auto vtbl = *reinterpret_cast<const std::uintptr_t*>(a_obj);
		if (!g.Contains(vtbl - 8, 8)) {
			return nullptr;
		}
		const auto col = *reinterpret_cast<const std::uintptr_t*>(vtbl - 8);
		if (!g.Contains(col, 24) || *reinterpret_cast<const std::uint32_t*>(col) != 1) {
			return nullptr;
		}
		const auto td = g.base + *reinterpret_cast<const std::uint32_t*>(col + 12);
		if (!g.Contains(td + 16, 4)) {
			return nullptr;
		}
		return reinterpret_cast<const char*>(td + 16);
	}

	// Offset of the part of an object that this pointer points at (from the RTTI), or -1.
	std::int32_t SubobjectOffset(const void* a_obj)
	{
		const auto& g = Game();
		if (!g.base || !Readable(a_obj, 8)) {
			return -1;
		}
		const auto vtbl = *reinterpret_cast<const std::uintptr_t*>(a_obj);
		if (!g.Contains(vtbl - 8, 8)) {
			return -1;
		}
		const auto col = *reinterpret_cast<const std::uintptr_t*>(vtbl - 8);
		if (!g.Contains(col, 24) || *reinterpret_cast<const std::uint32_t*>(col) != 1) {
			return -1;
		}
		return static_cast<std::int32_t>(reinterpret_cast<const std::uint32_t*>(col)[1]);
	}

	std::uint32_t ClassDescRva(const void* a_obj)
	{
		const auto vtbl = *reinterpret_cast<const std::uintptr_t*>(a_obj);
		const auto col = *reinterpret_cast<const std::uintptr_t*>(vtbl - 8);
		return reinterpret_cast<const std::uint32_t*>(col)[4];
	}

	std::int32_t BaseOffset(std::uint32_t a_classDescRva, std::string_view a_baseSubstr);

	// Turns a BGSSoundOutput pointer (any part of it) into its verified BSISoundOutputModel part.
	const RE::BSISoundOutputModel* SoundOutputInterface(const void* a_ptr)
	{
		const auto cur = SubobjectOffset(a_ptr);
		if (cur < 0) {
			return nullptr;
		}
		const auto complete = reinterpret_cast<std::uintptr_t>(a_ptr) - static_cast<std::uintptr_t>(cur);
		const auto ifaceOff = BaseOffset(ClassDescRva(a_ptr), "BSISoundOutputModel");
		if (ifaceOff < 0) {
			return nullptr;
		}
		const auto iface = reinterpret_cast<const void*>(complete + static_cast<std::uintptr_t>(ifaceOff));
		const char* n = RttiName(iface);
		if (SubobjectOffset(iface) != ifaceOff || !n || std::string_view(n).find("BGSSoundOutput") == std::string_view::npos) {
			return nullptr;
		}
		return static_cast<const RE::BSISoundOutputModel*>(iface);
	}

	struct FoundVtable
	{
		std::uintptr_t vtbl;
		std::uint32_t  subobjectOffset;
		std::uint32_t  classDescRva;
	};

	// Finds every vtable of a class by its RTTI name, e.g. "BGSImpactManager".
	std::vector<FoundVtable> FindVtables(std::string_view a_class)
	{
		std::vector<FoundVtable> out;
		const auto&              g = Game();
		if (!g.base) {
			return out;
		}
		const std::string needle = ".?AV" + std::string(a_class) + "@@";

		std::uint32_t tdRva = 0;
		for (auto& s : g.data) {
			const char* p = reinterpret_cast<const char*>(s.start);
			const auto  n = s.size;
			for (std::size_t i = 16; i + needle.size() + 1 < n; ++i) {
				if (p[i] == '.' && std::memcmp(p + i, needle.data(), needle.size()) == 0 && p[i + needle.size()] == '\0') {
					tdRva = static_cast<std::uint32_t>(s.start + i - 16 - g.base);
					break;
				}
			}
			if (tdRva) {
				break;
			}
		}
		if (!tdRva) {
			return out;
		}

		std::vector<std::uintptr_t> cols;
		for (auto& s : g.data) {
			for (std::size_t i = 0; i + 24 <= s.size; i += 4) {
				const auto a = s.start + i;
				const auto* u = reinterpret_cast<const std::uint32_t*>(a);
				if (u[0] == 1 && u[3] == tdRva && u[5] == static_cast<std::uint32_t>(a - g.base)) {
					cols.push_back(a);
				}
			}
		}
		for (auto col : cols) {
			for (auto& s : g.data) {
				for (std::size_t i = 0; i + 16 <= s.size; i += 8) {
					const auto a = s.start + i;
					if (*reinterpret_cast<const std::uintptr_t*>(a) == col) {
						const auto* u = reinterpret_cast<const std::uint32_t*>(col);
						out.push_back({ a + 8, u[1], u[4] });
					}
				}
			}
		}
		return out;
	}

	// Offset of a base class (whose RTTI name contains a_baseSubstr) inside the class described by a_classDescRva.
	std::int32_t BaseOffset(std::uint32_t a_classDescRva, std::string_view a_baseSubstr)
	{
		const auto& g = Game();
		const auto  chd = g.base + a_classDescRva;
		if (!g.Contains(chd, 16)) {
			return -1;
		}
		const auto num = reinterpret_cast<const std::uint32_t*>(chd)[2];
		const auto arr = g.base + reinterpret_cast<const std::uint32_t*>(chd)[3];
		for (std::uint32_t i = 0; i < num && i < 256; ++i) {
			if (!g.Contains(arr + i * 4, 4)) {
				break;
			}
			const auto bcd = g.base + reinterpret_cast<const std::uint32_t*>(arr)[i];
			if (!g.Contains(bcd, 28)) {
				continue;
			}
			const auto* b = reinterpret_cast<const std::uint32_t*>(bcd);
			const auto  td = g.base + b[0];
			if (!g.Contains(td + 16, 4)) {
				continue;
			}
			const std::string_view name(reinterpret_cast<const char*>(td + 16));
			// a_baseSubstr may hold several parts separated by '|'; all must be in the name
			bool        all = true;
			std::size_t start = 0;
			while (start <= a_baseSubstr.size()) {
				const auto end = a_baseSubstr.find('|', start);
				const auto part = a_baseSubstr.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
				if (!part.empty() && name.find(part) == std::string_view::npos) {
					all = false;
					break;
				}
				if (end == std::string_view::npos) {
					break;
				}
				start = end + 1;
			}
			if (all) {
				return static_cast<std::int32_t>(b[2]);  // mdisp
			}
		}
		return -1;
	}

	// Writes every base class of a class to the log (for finding things on new game versions).
	void LogBases(std::uint32_t a_classDescRva)
	{
		const auto& g = Game();
		const auto  chd = g.base + a_classDescRva;
		if (!g.Contains(chd, 16)) {
			return;
		}
		const auto num = reinterpret_cast<const std::uint32_t*>(chd)[2];
		const auto arr = g.base + reinterpret_cast<const std::uint32_t*>(chd)[3];
		for (std::uint32_t i = 0; i < num && i < 64; ++i) {
			if (!g.Contains(arr + i * 4, 4)) {
				break;
			}
			const auto bcd = g.base + reinterpret_cast<const std::uint32_t*>(arr)[i];
			if (!g.Contains(bcd, 28)) {
				continue;
			}
			const auto* b = reinterpret_cast<const std::uint32_t*>(bcd);
			const auto  td = g.base + b[0];
			if (g.Contains(td + 16, 4)) {
				REX::INFO("  base {} at +{:X}", reinterpret_cast<const char*>(td + 16), static_cast<std::int32_t>(b[2]));
			}
		}
	}

	// =====================================================================
	// values read on the main thread
	// =====================================================================
	std::atomic<float>     g_load{ 0.0f };
	std::atomic_bool       g_inPowerArmor{ false };
	std::atomic_bool       g_swimming{ false };
	std::atomic_bool       g_sneaking{ false };
	std::atomic_bool       g_cacheQueued{ false };
	std::atomic<long long> g_lastCacheRequest{ 0 };

	std::atomic<float> g_agility{ 5.0f };
	std::atomic<float> g_speedMult{ 1.0f };
	std::atomic_bool   g_firstPerson{ true };

	namespace Perks
	{
		constexpr int             kCount = 6;
		std::vector<RE::BGSPerk*> g_list[kCount];  // moving target, strong back, adamantium skeleton, aquaboy/girl, sneak, action boy/girl
		std::atomic<int>          g_rank[kCount]{};
		enum : int { kMovingTarget, kStrongBack, kAdamantium, kAquaboy, kSneak, kActionBoy };

		void Resolve()
		{
			for (auto& l : g_list) {
				l.clear();
			}
			auto dh = RE::TESDataHandler::GetSingleton();
			if (!dh) {
				return;
			}
			for (auto perk : dh->GetFormArray<RE::BGSPerk>()) {
				if (!perk) {
					continue;
				}
				const auto name = Lower(std::string(RE::TESFullName::GetFullName(*perk)));
				int        idx = -1;
				for (int i = 0; i < kCount && idx < 0; ++i) {
					std::string_view list = g_cfg.perkNames[i];
					while (!list.empty()) {
						const auto bar = list.find('|');
						if (list.substr(0, bar) == name) {
							idx = i;
							break;
						}
						if (bar == std::string_view::npos) {
							break;
						}
						list.remove_prefix(bar + 1);
					}
				}
				if (idx >= 0) {
					g_list[idx].push_back(perk);
				}
			}
			REX::INFO("Perks found (ranks): Moving Target {}, Strong Back {}, Adamantium Skeleton {}, Aquaboy {}, Sneak {}, Action Boy {}",
				g_list[0].size(), g_list[1].size(), g_list[2].size(), g_list[3].size(), g_list[4].size(), g_list[5].size());
		}

		void Update(RE::PlayerCharacter* a_player)
		{
			for (int i = 0; i < kCount; ++i) {
				int r = 0;
				for (auto p : g_list[i]) {
					if (a_player->GetPerkRank(p) > 0) {
						++r;
					}
				}
				g_rank[i] = r;
			}
		}

		int Rank(int a_idx) { return g_cfg.perkMode == 0 ? 0 : g_rank[a_idx].load(); }
		bool MaxRank(int a_idx) { return !g_list[a_idx].empty() && Rank(a_idx) >= static_cast<int>(g_list[a_idx].size()); }
		bool Bonus() { return g_cfg.perkMode != 0; }
		bool Unlock() { return g_cfg.perkMode == 2; }
	}

	// HL1 edge friction (PM_Friction): is there floor 16 units ahead, within 34 units down?
	std::atomic_bool  g_edgeAhead{ false };      // true = no floor ahead (friction is multiplied)
	std::atomic<float> g_moveDirX{ 0.0f }, g_moveDirY{ 0.0f };  // current horizontal move direction (from physics)
	int               g_edgeLogs{ 0 };

	void ProbeEdge(RE::PlayerCharacter* a_player)
	{
		const bool wanted = g_cfg.edgeFriction == 2 || (g_cfg.edgeFriction == 1 && g_cfg.mode == Mode::kHL1);
		if (!wanted) {
			g_edgeAhead = false;
			return;
		}
		auto cell = a_player->GetParentCell();
		const float dx = g_moveDirX.load(), dy = g_moveDirY.load();
		if (!cell || (std::fabs(dx) < 0.01f && std::fabs(dy) < 0.01f)) {
			g_edgeAhead = false;
			return;
		}
		constexpr float kHL = 1.0f / (0.0254f * 69.99124f);  // HL unit in game units
		const float ahead = 16.0f * kHL, down = 34.0f * kHL, half = 16.0f * kHL;  // HL1 hull: 32 wide
		const auto& pos = a_player->data.location;
		// three rays across the player's width (HL1 sweeps a box)
		int hits = 0;
		for (int i = -1; i <= 1; ++i) {
			const float side = static_cast<float>(i) * half * 0.9f;
			const RE::NiPoint3 start(pos.x + dx * ahead + dy * side, pos.y + dy * ahead - dx * side, pos.z + 2.0f);
			const RE::NiPoint3 end(start.x, start.y, start.z - down - 2.0f);
			RE::bhkPickData pick;
			pick.SetStartEnd(start, end);
			cell->Pick(pick);
			if (pick.HasHit()) {
				++hits;
			}
			if (g_cfg.debugLog && g_edgeLogs < 6) {
				++g_edgeLogs;
				REX::INFO("edge probe {}: hit {} fraction {:.3f}", i, pick.HasHit(), pick.HasHit() ? pick.GetHitFraction() : 1.0f);
			}
		}
		g_edgeAhead = hits == 0;
	}

	void UpdateCache()
	{
		g_cacheQueued = false;
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		g_inPowerArmor = RE::PowerArmor::PlayerInPowerArmor();
		g_swimming = player->ActorState::IsSwimming();
		g_sneaking = player->IsSneaking();
		if (auto avs = RE::ActorValue::GetSingleton()) {
			auto owner = static_cast<RE::ActorValueOwner*>(player);
			if (avs->agility) {
				g_agility = owner->GetActorValue(*avs->agility);
			}
			if (avs->speedMult) {
				g_speedMult = std::clamp(owner->GetActorValue(*avs->speedMult) / 100.0f, 0.1f, 5.0f);
			}
		}
		if (auto cam = RE::PlayerCamera::GetSingleton()) {
			auto cur = cam->currentState.get();
			g_firstPerson = cur && (cur == cam->cameraStates[RE::CameraStates::kFirstPerson].get() ||
			                           cur == cam->cameraStates[RE::CameraStates::kIronSights].get());
		}
		Perks::Update(player);
		ProbeEdge(player);

		float load = 0.0f;
		if (auto avs = RE::ActorValue::GetSingleton(); avs && avs->carryWeight) {
			const float cap = static_cast<RE::ActorValueOwner*>(player)->GetActorValue(*avs->carryWeight);
			const float weight = player->GetWeightInContainer();
			if (cap > 0.0f) {
				load = weight / cap;
			}
		}
		g_load = load;
	}

	void RequestCacheUpdate()
	{
		const auto now = NowMs();
		if (now - g_lastCacheRequest.load() < 50 || g_cacheQueued.exchange(true)) {
			return;
		}
		g_lastCacheRequest = now;
		if (auto task = F4SE::GetTaskInterface()) {
			task->AddTask([]() { UpdateCache(); });
		} else {
			g_cacheQueued = false;
		}
	}

	// =====================================================================
	// camera: Half-Life view bob, strafe roll and landing punch
	// =====================================================================
	namespace Camera
	{
		constexpr float kHLToGame = 1.0f / (0.0254f * 69.99124f);  // 1 HL unit (inch) in game units

		std::atomic<float> g_velX{ 0.0f };  // player velocity, HL units/s (written by the velocity hook)
		std::atomic<float> g_velY{ 0.0f };
		std::atomic_bool   g_onGround{ true };
		std::atomic<float> g_punchKick{ 0.0f };  // new landing punch (degrees), picked up by the camera

		struct State
		{
			std::chrono::steady_clock::time_point last{};
			bool  hasLast{ false };
			float bobTime{ 0.0f };
			float bob{ 0.0f };    // HL units
			float roll{ 0.0f };   // degrees
			float punch{ 0.0f };  // degrees
		} g_state;

		using GetRot_t = void (*)(const void*, RE::NiQuaternion&);
		using GetTrans_t = void (*)(const void*, RE::NiPoint3&);
		GetRot_t    g_origRot{ nullptr };
		GetTrans_t  g_origTrans{ nullptr };
		const void* g_fpState{ nullptr };

		bool BobOn() { return g_cfg.viewBob == 2 || (g_cfg.viewBob == 1 && g_cfg.mode == Mode::kHL1); }
		bool RollOn() { return g_cfg.viewRoll == 2 || (g_cfg.viewRoll == 1 && g_cfg.mode == Mode::kHL1); }

		bool Active(const void* a_state)
		{
			if (g_cfg.mode == Mode::kVanilla || a_state != g_fpState) {
				return false;
			}
			auto ui = RE::UI::GetSingleton();
			return !(ui && ui->menuMode != 0);
		}

		void OnLand(float a_fallSpeed)
		{
			const float threshold = g_cfg.mode == Mode::kHL1 ? 350.0f : 303.0f;  // PLAYER_FALL_PUNCH_THRESHOLD
			if (g_cfg.landingPunch && a_fallSpeed > threshold) {
				float kick = std::min(a_fallSpeed * 0.013f, 8.0f);
				if (Perks::Bonus()) {
					kick *= std::max(0.0f, 1.0f - 0.25f * static_cast<float>(Perks::Rank(Perks::kAdamantium)));  // Adamantium Skeleton
				}
				g_punchKick = kick;
			}
		}

		// once per frame (the game asks for rotation and position separately)
		void Step()
		{
			const auto now = std::chrono::steady_clock::now();
			float      dt = g_state.hasLast ? std::chrono::duration<float>(now - g_state.last).count() : 0.0f;
			if (g_state.hasLast && dt < 0.002f) {
				return;
			}
			g_state.last = now;
			g_state.hasLast = true;
			dt = std::clamp(dt, 0.0f, 0.1f);

			const float vx = g_velX.load();
			const float vy = g_velY.load();
			const float speed = std::hypot(vx, vy);

			// V_CalcBob: cl_bob 0.01, cl_bobcycle 0.8, cl_bobup 0.5
			g_state.bobTime += dt;
			if (BobOn() && g_onGround.load()) {
				constexpr float kCycle = 0.8f, kUp = 0.5f, kPi = 3.14159265f;
				float cycle = std::fmod(g_state.bobTime, kCycle) / kCycle;
				cycle = cycle < kUp ? kPi * cycle / kUp : kPi + kPi * (cycle - kUp) / (1.0f - kUp);
				float bob = speed * 0.01f;
				bob = bob * 0.3f + bob * 0.7f * std::sin(cycle);
				g_state.bob = std::clamp(bob, -7.0f, 4.0f);
			} else if (!BobOn()) {
				g_state.bob = 0.0f;
			} else {
				g_state.bob *= std::exp(-10.0f * dt);  // in the air: ease back to centre instead of freezing
			}

			// V_CalcRoll: cl_rollangle 2, cl_rollspeed 200
			g_state.roll = 0.0f;
			if (RollOn()) {
				auto player = RE::PlayerCharacter::GetSingleton();
				if (player) {
					const float yaw = player->data.angle.z;
					const float side = vx * std::cos(yaw) - vy * std::sin(yaw);  // speed to the right
					const float a = std::fabs(side);
					const float r = a < g_cfg.rollSpeed ? a * g_cfg.rollAngle / std::max(g_cfg.rollSpeed, 1.0f) : g_cfg.rollAngle;
					g_state.roll = side < 0.0f ? -r : r;
				}
			}

			// landing punch, PM_DropPunchAngle decay
			if (const float kick = g_punchKick.exchange(0.0f); kick > 0.0f) {
				g_state.punch = kick * g_cfg.punchScale;
			}
			if (g_state.punch > 0.0f) {
				g_state.punch = std::max(0.0f, g_state.punch - (10.0f + g_state.punch * 0.5f) * dt);
			}
		}

		void HookedGetTranslation(const void* a_this, RE::NiPoint3& a_out)
		{
			g_origTrans(a_this, a_out);
			if (!Active(a_this)) {
				return;
			}
			Step();
			a_out.z += g_state.bob * g_cfg.bobScale * kHLToGame;
		}

		void HookedGetRotation(const void* a_this, RE::NiQuaternion& a_out)
		{
			g_origRot(a_this, a_out);
			if (!Active(a_this)) {
				return;
			}
			Step();
			const float deg = g_state.roll + g_state.punch;
			if (std::fabs(deg) < 0.001f) {
				return;
			}
			// roll around the camera's own forward axis (local Y)
			const float h = deg * 3.14159265f / 180.0f * 0.5f;
			const float s = std::sin(h), c = std::cos(h);
			const RE::NiQuaternion q = a_out;  // (w, x, y, z)
			const float bw = c, bx = 0.0f, by = s, bz = 0.0f;
			a_out.w = q.w * bw - q.x * bx - q.y * by - q.z * bz;
			a_out.x = q.w * bx + q.x * bw + q.y * bz - q.z * by;
			a_out.y = q.w * by - q.x * bz + q.y * bw + q.z * bx;
			a_out.z = q.w * bz + q.x * by - q.y * bx + q.z * bw;
		}

		void Reset()
		{
			g_state = State{};
			g_punchKick = 0.0f;
			g_velX = 0.0f;
			g_velY = 0.0f;
			g_onGround = true;
		}

		void Install()
		{
			auto cam = RE::PlayerCamera::GetSingleton();
			if (!cam) {
				return;
			}
			auto state = cam->cameraStates[RE::CameraStates::kFirstPerson].get();
			if (!state) {
				return;
			}
			g_fpState = state;
			static bool done = false;
			if (done) {
				return;  // never hook twice (another plugin may have chained after us)
			}
			done = true;
			const char* name = RttiName(state);
			if (!name || std::string_view(name).find("FirstPersonState") == std::string_view::npos) {
				REX::ERROR("First person camera has an unexpected type ({}); camera effects NOT installed", name ? name : "unknown");
				return;
			}
			auto vtbl = *reinterpret_cast<std::uintptr_t* const*>(state);
			if (!Game().Contains(vtbl[0x0C]) || !Game().Contains(vtbl[0x0D])) {
				REX::WARN("Camera rotation/position already changed by another plugin (camera mod?); chaining after it");
			}
			g_origRot = reinterpret_cast<GetRot_t>(vtbl[0x0C]);
			REL::WriteSafeData(reinterpret_cast<std::uintptr_t>(vtbl + 0x0C), reinterpret_cast<std::uintptr_t>(&HookedGetRotation));
			g_origTrans = reinterpret_cast<GetTrans_t>(vtbl[0x0D]);
			REL::WriteSafeData(reinterpret_cast<std::uintptr_t>(vtbl + 0x0D), reinterpret_cast<std::uintptr_t>(&HookedGetTranslation));
			REX::INFO("Camera hooked ({})", name);
		}
	}

	// Footstep speed thresholds (HL units/s), worked out by the movement code from your current speeds.
	float g_stepVelWalk{ 48.0f };
	float g_stepVelRun{ 226.0f };

	// =====================================================================
	// sounds
	// =====================================================================
	namespace Sounds
	{
		// sound groups: files named <group><n>.wav in Data\Sound\FX\BHOP\HL1 or HL2
		struct Group
		{
			std::vector<std::string> files;  // game paths, sorted by number
		};

		std::unordered_map<std::string, Group>        g_groups[3];      // [set][group]
		std::unordered_map<std::uint32_t, std::string> g_materialCat;   // material id -> category
		const RE::BSISoundOutputModel*                  g_outputModel{ nullptr };
		std::uint32_t                                   g_playerHandle{ 0 };
		std::atomic_bool                                g_ready{ false };

		// category -> group name, per set
		std::string GroupFor(int a_set, const std::string& a_cat)
		{
			if (a_set == 1) {
				static const std::unordered_map<std::string, std::string> m{
					{ "concrete", "pl_step" }, { "metal", "pl_metal" }, { "grate", "pl_grate" }, { "chainlink", "pl_grate" },
					{ "duct", "pl_duct" }, { "wood", "pl_step" }, { "woodpanel", "pl_step" }, { "grass", "pl_dirt" },
					{ "gravel", "pl_dirt" }, { "mud", "pl_dirt" }, { "sand", "pl_dirt" }, { "snow", "pl_dirt" },
					{ "dirt", "pl_dirt" }, { "tile", "pl_tile" }, { "slosh", "pl_slosh" }, { "wade", "pl_wade" },
					{ "ladder", "pl_ladder" }, { "carpet", "pl_step" }, { "swim", "pl_swim" }, { "powerarmor", "pl_metal" },
					{ "ice", "pl_tile" }, { "glass", "pl_tile" }
				};
				auto it = m.find(a_cat);
				return it != m.end() ? it->second : "pl_step";
			}
			static const std::unordered_map<std::string, std::string> m{
				{ "concrete", "concrete" }, { "metal", "metal" }, { "grate", "metalgrate" }, { "chainlink", "chainlink" },
				{ "duct", "duct" }, { "wood", "wood" }, { "woodpanel", "woodpanel" }, { "grass", "grass" },
				{ "gravel", "gravel" }, { "mud", "mud" }, { "sand", "sand" }, { "snow", "gravel" },
				{ "dirt", "dirt" }, { "tile", "tile" }, { "slosh", "slosh" }, { "wade", "wade" },
				{ "ladder", "ladder" }, { "carpet", "softshoe_generic" }, { "swim", "wade" }, { "powerarmor", "hardboot_generic" },
				{ "ice", "tile" }, { "glass", "tile" }
			};
			auto it = m.find(a_cat);
			return it != m.end() ? it->second : "concrete";
		}

		std::string CategoryFromName(const std::string& a_lower)
		{
			auto has = [&](const char* s) { return a_lower.find(s) != std::string::npos; };
			if (has("grate")) return "grate";
			if (has("chain")) return "chainlink";
			if (has("metal") && (has("sheet") || has("thin") || has("hollow") || has("duct"))) return "duct";
			if (has("metal")) return "metal";
			if (has("wood") && (has("hollow") || has("panel") || has("plank"))) return "woodpanel";
			if (has("wood")) return "wood";
			if (has("grass") || has("foliage")) return "grass";
			if (has("gravel")) return "gravel";
			if (has("mud")) return "mud";
			if (has("sand")) return "sand";
			if (a_lower == "ice" || has("materialice") || has("iceslick")) return "ice";
			if (has("snow")) return "snow";
			if (has("dirt") || has("soil") || has("earth")) return "dirt";
			if (has("glass")) return "glass";
			if (has("tile") || has("ceramic") || has("porcelain") || has("marble")) return "tile";
			if (has("water")) return "slosh";
			if (has("carpet") || has("rug") || has("cloth") || has("fabric") || has("mattress")) return "carpet";
			if (has("concrete") || has("stone") || has("rock") || has("asphalt") || has("brick")) return "concrete";
			return {};
		}

		bool g_loggedMaterials{ false };

		void BuildMaterials()
		{
			g_materialCat.clear();
			auto dh = RE::TESDataHandler::GetSingleton();
			if (!dh) {
				return;
			}
			for (auto matt : dh->GetFormArray<RE::BGSMaterialType>()) {
				if (!matt) {
					continue;
				}
				std::string cat;
				std::string firstName;
				int         depth = 0;
				for (auto m = matt; m && depth < 8; m = m->parentType, ++depth) {
					const std::string name = Lower(m->materialName.c_str() ? m->materialName.c_str() : "");
					if (depth == 0) {
						firstName = name;
					}
					for (auto& [k, v] : g_cfg.materialOverrides) {
						if (k == name) {
							cat = v;
						}
					}
					if (cat.empty()) {
						cat = CategoryFromName(name);
					}
					if (!cat.empty()) {
						break;
					}
				}
				if (cat.empty()) {
					cat = "concrete";
				}
				g_materialCat[matt->materialID] = cat;
				if (g_cfg.debugLog && !g_loggedMaterials) {
					REX::INFO("Material {:08X} '{}' -> {}", matt->materialID, firstName, cat);
				}
			}
			g_loggedMaterials = true;
		}

		void BuildGroups()
		{
			namespace fs = std::filesystem;
			static constexpr const char* kFolders[3] = { "", "HL1", "HL2" };
			for (int set = 1; set <= 2; ++set) {
				g_groups[set].clear();
				const fs::path dir = fs::path("Data") / "Sound" / "FX" / "BHOP" / kFolders[set];
				std::error_code ec;
				if (!fs::is_directory(dir, ec)) {
					REX::WARN("Sound folder missing: {}", dir.string());
					continue;
				}
				std::map<std::string, std::map<int, std::string>> found;
				for (auto& e : fs::directory_iterator(dir, ec)) {
					if (!e.is_regular_file()) {
						continue;
					}
					auto stem = Lower(e.path().stem().string());
					auto ext = Lower(e.path().extension().string());
					if (ext != ".wav" && ext != ".xwm") {
						continue;
					}
					std::size_t d = stem.size();
					while (d > 0 && std::isdigit(static_cast<unsigned char>(stem[d - 1]))) {
						--d;
					}
					const int num = d < stem.size() ? std::stoi(stem.substr(d)) : 0;
					const auto gamePath = std::string("Sound\\FX\\BHOP\\") + kFolders[set] + "\\" + e.path().filename().string();
					found[stem.substr(0, d)][num] = gamePath;
				}
				for (auto& [name, files] : found) {
					auto& g = g_groups[set][name];
					for (auto& [n, p] : files) {
						g.files.push_back(p);
					}
				}
				REX::INFO("{}: {} sound groups found", kFolders[set], g_groups[set].size());
			}
		}

		// Output model of the vanilla player footsteps, so our sounds get the same
		// reverb / distance settings (and whatever reverb mods changed there).
		void FindOutputModel()
		{
			g_outputModel = nullptr;
			auto player = RE::PlayerCharacter::GetSingleton();
			auto race = player ? player->race : nullptr;
			auto skin = race ? race->formSkin : nullptr;
			if (!skin) {
				REX::WARN("No player skin found; sounds will use default output");
				return;
			}
			for (auto& addon : skin->modelArray) {
				auto arma = addon.armorAddon;
				auto set = arma ? arma->footstepSet : nullptr;
				if (!set) {
					continue;
				}
				for (auto& list : set->entries) {
					for (auto fs : list) {
						auto ipds = fs ? fs->impactSet : nullptr;
						if (!ipds) {
							continue;
						}
						for (auto& [mat, ipda] : ipds->impactMap) {
							auto snd = ipda ? ipda->sound1 : nullptr;
							auto impl = snd ? reinterpret_cast<const std::uintptr_t*>(snd->impl) : nullptr;
							if (!impl || !Readable(impl, 0x100)) {
								continue;
							}
							for (int i = 1; i < 0x100 / 8; ++i) {
								const auto p = reinterpret_cast<const void*>(impl[i]);
								const char* n = RttiName(p);
								if (n && std::string_view(n).find("BGSSoundOutput") != std::string_view::npos) {
									// The sound stores the form (BGSSoundOutput*). The audio engine needs the
									// BSISoundOutputModel part of it, which sits further inside the object.
									auto iface = SoundOutputInterface(p);
									if (!iface) {
										REX::WARN("Output model {:X} found but its interface could not be verified; not using it",
											reinterpret_cast<std::uintptr_t>(p));
										return;
									}
									g_outputModel = iface;
									auto form = reinterpret_cast<const RE::TESForm*>(reinterpret_cast<std::uintptr_t>(p) - SubobjectOffset(p));
									REX::INFO("Using vanilla footstep output model {:08X} (from sound {:08X})", form->GetFormID(), snd->GetFormID());
									return;
								}
							}
						}
					}
				}
			}
			REX::WARN("Vanilla footstep output model not found; sounds will use default output");
		}

		// ---------------------------------------------------------- playback
		struct Request
		{
			std::string group;
			int         index;  // -1 = random
			float       volume;
		};

		std::mutex           g_queueLock;
		std::vector<Request> g_queue;
		std::atomic_bool     g_playQueued{ false };
		std::mt19937         g_rng{ 12345 };
		std::mutex           g_rngLock;  // used from the physics thread and the main thread

		template <class T>
		T Random(T a_min, T a_max)
		{
			std::scoped_lock lock(g_rngLock);
			return std::uniform_int_distribution<T>(a_min, a_max)(g_rng);
		}
		int                  g_failLogs{ 0 };

		void PlayNow(const Request& a_req, int a_set)
		{
			auto it = g_groups[a_set].find(a_req.group);
			if (it == g_groups[a_set].end() || it->second.files.empty()) {
				it = g_groups[a_set].find(a_set == 1 ? "pl_step" : "concrete");
				if (it == g_groups[a_set].end() || it->second.files.empty()) {
					return;
				}
			}
			const auto& files = it->second.files;
			std::size_t idx = 0;
			if (a_req.index >= 0 && static_cast<std::size_t>(a_req.index) < files.size()) {
				idx = static_cast<std::size_t>(a_req.index);
			} else {
				idx = Random<std::size_t>(0, files.size() - 1);
			}

			auto audio = RE::BSAudioManager::GetSingleton();
			auto player = RE::PlayerCharacter::GetSingleton();
			if (!audio || !player) {
				return;
			}
			RE::BSSoundHandle   handle;
			RE::BSResource::ID  id(files[idx].c_str());
			audio->GetSoundHandleByFile(handle, id, g_cfg.fileSoundFlags, static_cast<std::uint8_t>(g_cfg.filePriority));
			if (!handle.IsValid()) {
				if (g_failLogs++ < 5) {
					REX::WARN("Could not create sound for {}", files[idx]);
				}
				return;
			}
			if (g_outputModel && g_cfg.useVanillaOutput) {
				handle.SetOutputModel(g_outputModel);
			}
			const RE::NiPoint3 pos(player->data.location.x, player->data.location.y, player->data.location.z);
			handle.SetPosition(pos);
			if (auto root = player->Get3D()) {
				handle.SetObjectToFollow(root);  // stays with you while you run, instead of being left behind
			}
			const float vol = std::clamp(a_req.volume * g_cfg.volume, 0.001f, 1.0f);
			const auto  att = static_cast<std::uint16_t>(std::clamp(-2000.0f * std::log10(vol), 0.0f, 10000.0f));  // 1/100 dB
			handle.SetStaticAttenuation(att);
			handle.Play();
		}

		void DrainQueue()
		{
			g_playQueued = false;
			std::vector<Request> todo;
			{
				std::scoped_lock lock(g_queueLock);
				todo.swap(g_queue);
			}
			const int set = SoundSet();
			if (set == 0 || !g_ready) {
				return;
			}
			for (auto& r : todo) {
				PlayNow(r, set);
			}
		}

		void Queue(std::string a_group, int a_index, float a_volume)
		{
			{
				std::scoped_lock lock(g_queueLock);
				if (g_queue.size() < 16) {
					g_queue.push_back({ std::move(a_group), a_index, a_volume });
				}
			}
			if (!g_playQueued.exchange(true)) {
				if (auto task = F4SE::GetTaskInterface()) {
					task->AddTask([]() { DrainQueue(); });
				} else {
					g_playQueued = false;
				}
			}
		}

		// ---------------------------------------------------------- step logic (Half-Life's PM_UpdateStepSound)
		struct StepState
		{
			float timer{ 0.0f };
			bool  left{ false };
		} g_step;

		std::string SurfaceCategory(RE::bhkCharacterController* a_con, float& a_waterDepth)
		{
			a_waterDepth = 0.0f;
			if (g_cfg.waterSounds) {
				auto        player = RE::PlayerCharacter::GetSingleton();
				const float wh = a_con->waterHeight;
				if (player && wh > -1.0e6f && wh < 1.0e6f) {
					a_waterDepth = wh - player->data.location.z;  // game units
				}
			}
			if (a_waterDepth > 1.0f && a_waterDepth < 200.0f) {
				return a_waterDepth > 35.0f ? "wade" : "slosh";
			}
			if (g_inPowerArmor.load() && g_cfg.powerArmorHardboot && SoundSet() == 2) {
				return "powerarmor";
			}
			auto it = g_materialCat.find(a_con->supportMaterial);
			return it != g_materialCat.end() ? it->second : "concrete";
		}

		// HL plays step 1 or 3 on the left foot, 2 or 4 on the right
		int StepIndex(int a_set, const std::string& a_group)
		{
			const int pick = Random<int>(0, 1);
			g_step.left = !g_step.left;
			if (a_set == 1 && a_group == "pl_tile" && Random<int>(0, 4) == 0) {
				return 4;  // pl_tile5, HL1 plays it now and then
			}
			auto it = g_groups[a_set].find(a_group);
			if (it == g_groups[a_set].end() || it->second.files.size() != 4) {
				return -1;
			}
			return g_step.left ? (pick ? 2 : 0) : (pick ? 3 : 1);
		}

		void PlayStep(RE::bhkCharacterController* a_con, float a_volume)
		{
			const int   set = SoundSet();
			float       depth = 0.0f;
			const auto  cat = SurfaceCategory(a_con, depth);
			const auto  group = GroupFor(set, cat);
			Queue(group, StepIndex(set, group), a_volume);
		}

		// called from the velocity hook for the player, every physics step
		// a_jumped / a_landed / a_fall come from the movement code, so sounds and camera share one event
		void Update(RE::bhkCharacterController* a_con, float a_dt, bool a_onGround, const RE::hkVector4f& a_actual, bool a_menus,
			bool a_jumped, bool a_landed, float a_fall)
		{
			const int set = SoundSet();
			if (set == 0 || !g_ready || a_menus || a_dt <= 0.0f || a_dt > 0.25f) {
				return;
			}

			const float hSpeed = std::hypot(a_actual.x, a_actual.y) / g_cfg.unitScale;

			// jump + landing
			if (g_cfg.jumpLandSounds) {
				if (a_jumped) {
					PlayStep(a_con, 1.0f);  // Half-Life plays the floor sound at full volume when you jump
				}
				if (a_landed) {
					const float fall = a_fall;
					float       vol = 0.0f;
					if (fall >= g_cfg.landHardSpeed) {
						vol = 1.0f;
					} else if (fall >= g_cfg.landSoftSpeed) {
						vol = 0.85f;
					}
					if (vol > 0.0f) {
						PlayStep(a_con, vol);
						g_step.timer = g_cfg.runInterval;
					}
					if (set == 1 && g_cfg.hl1FallPain && fall >= g_cfg.fallPainSpeed) {
						Queue("pl_fallpain", 1, 1.0f);  // pl_fallpain3, as in Half-Life
					}
				}
			}

			g_step.timer = std::max(0.0f, g_step.timer - a_dt);

			// swimming
			if (g_swimming.load()) {
				if (g_cfg.swimSounds && hSpeed > g_cfg.minStepSpeed && g_step.timer <= 0.0f) {
					Queue(GroupFor(set, "swim"), -1, 0.5f);
					g_step.timer = g_cfg.waterInterval;
				}
				return;
			}

			if (!a_onGround || hSpeed < 1.0f || g_step.timer > 0.0f) {
				return;
			}
			const bool hl1 = set == 1;
			// HL2 needs walking speed for a step; HL1 steps at any speed (PM_UpdateStepSound)
			if (!hl1 && hSpeed < std::max(g_stepVelWalk, g_cfg.minStepSpeed)) {
				return;
			}

			const float velRun = g_cfg.runStepSpeed > 0.0f ? g_cfg.runStepSpeed : g_stepVelRun;
			const bool  walking = hSpeed < velRun;
			const bool  ducking = g_sneaking.load();

			float      depth = 0.0f;
			const auto cat = SurfaceCategory(a_con, depth);
			auto       surf = hlmath::StepSurface::kNormal;
			if (cat == "wade") {
				surf = hlmath::StepSurface::kWade;
			} else if (cat == "duct") {
				surf = hlmath::StepSurface::kVent;
			} else if (cat == "dirt" || cat == "grass" || cat == "gravel" || cat == "mud" || cat == "sand" || cat == "snow") {
				surf = hlmath::StepSurface::kDirt;
			}
			const auto r = hlmath::StepSound(hl1, surf, walking, ducking);  // Valve's volumes and timing

			// your volume sliders scale Half-Life's own values (0.5 running / 0.2 walking)
			float vol = r.volume * (walking ? g_cfg.walkVolume / 0.2f : g_cfg.runVolume / 0.5f);
			float interval = surf == hlmath::StepSurface::kWade ? g_cfg.waterInterval : (walking ? g_cfg.walkInterval : g_cfg.runInterval);
			if (ducking) {
				interval += g_cfg.sneakExtraInterval;
				vol *= g_cfg.sneakVolumeMult;
				if (Perks::Bonus()) {
					vol *= std::max(0.3f, 1.0f - 0.1f * static_cast<float>(Perks::Rank(Perks::kSneak)));  // Sneak: quieter steps
				}
			}
			g_step.timer = interval;

			const auto group = GroupFor(set, cat);
			Queue(group, StepIndex(set, group), vol);
		}

		// ---------------------------------------------------------- muting vanilla player footsteps
		using ProcessEvent_t = std::int32_t (*)(void*, const void*, void*);
		ProcessEvent_t g_origFootstep{ nullptr };

		// where the player shows up inside a footstep event (found automatically)
		int g_handleOffset{ -1 };
		int g_ptrOffset{ -1 };
		int g_handleHits[16]{};
		int g_ptrHits[8]{};
		int g_footstepLogs{ 0 };

		bool IsPlayerFootstep(const void* a_event)
		{
			auto player = RE::PlayerCharacter::GetSingleton();
			if (!a_event || !player || !Readable(a_event, 0x40)) {
				return false;
			}
			const auto* u32 = reinterpret_cast<const std::uint32_t*>(a_event);
			const auto* u64 = reinterpret_cast<const std::uintptr_t*>(a_event);
			if (g_handleOffset >= 0) {
				return g_playerHandle && u32[g_handleOffset] == g_playerHandle;
			}
			if (g_ptrOffset >= 0) {
				return u64[g_ptrOffset] == reinterpret_cast<std::uintptr_t>(player);
			}
			// still learning: look for the player in the event
			bool found = false;
			for (int i = 0; i < 16; ++i) {
				if (g_playerHandle && u32[i] == g_playerHandle && ++g_handleHits[i] >= 1) {
					g_handleOffset = i;
					REX::INFO("Footstep events: player handle found at +{:X}", i * 4);
				}
				if (g_playerHandle && u32[i] == g_playerHandle) {
					found = true;
				}
			}
			for (int i = 0; i < 8; ++i) {
				if (u64[i] == reinterpret_cast<std::uintptr_t>(player)) {
					found = true;
					if (++g_ptrHits[i] >= 1) {
						g_ptrOffset = i;
						REX::INFO("Footstep events: player pointer found at +{:X}", i * 8);
					}
				}
			}
			return found && (g_handleOffset >= 0 || g_ptrOffset >= 0);
		}

		std::int32_t HookedFootstep(void* a_sink, const void* a_event, void* a_source)
		{
			if (g_cfg.debugLog && g_footstepLogs < 4) {
				++g_footstepLogs;
				if (Readable(a_event, 0x30)) {
					const auto* u = reinterpret_cast<const std::uint32_t*>(a_event);
					REX::INFO("footstep event: {:08X} {:08X} {:08X} {:08X} {:08X} {:08X} {:08X} {:08X} (player handle {:08X})",
						u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7], g_playerHandle);
				}
			}
			if (g_cfg.muteVanillaFootsteps && SoundSet() != 0 && g_ready && IsPlayerFootstep(a_event)) {
				return 0;  // kContinue: other listeners still get the event, only the sound is skipped
			}
			return g_origFootstep(a_sink, a_event, a_source);
		}

		bool g_footstepSearched{ false };

		void InstallFootstepHook()
		{
			if (g_origFootstep || g_footstepSearched || !g_cfg.muteVanillaFootsteps) {
				return;
			}
			g_footstepSearched = true;
			const auto vtables = FindVtables("BGSImpactManager");
			if (vtables.empty()) {
				REX::WARN("BGSImpactManager not found; vanilla player footsteps will still play");
				return;
			}
			for (auto& v : vtables) {
				const auto off = BaseOffset(v.classDescRva, "BSTEventSink@|BGSFootstepEvent@@");
				if (off >= 0 && static_cast<std::uint32_t>(off) == v.subobjectOffset) {
					auto* slot = reinterpret_cast<std::uintptr_t*>(v.vtbl) + 1;  // ProcessEvent
					g_origFootstep = reinterpret_cast<ProcessEvent_t>(*slot);
					REL::WriteSafeData(reinterpret_cast<std::uintptr_t>(slot), reinterpret_cast<std::uintptr_t>(&HookedFootstep));
					REX::INFO("Hooked footstep sounds (vtable {:X}, offset {:X})", v.vtbl - Game().base, off);
					return;
				}
			}
			REX::WARN("Footstep listener not found in {} BGSImpactManager vtables; vanilla player footsteps will still play", vtables.size());
			for (auto& v : vtables) {
				REX::INFO(" vtable +{:X}, part at +{:X}:", v.vtbl - Game().base, v.subobjectOffset);
			}
			LogBases(vtables.front().classDescRva);
		}

		void Init()
		{
			BuildGroups();
			BuildMaterials();
			FindOutputModel();
			InstallFootstepHook();
			g_ready = true;
		}

		void OnGameLoaded()
		{
			auto h = RE::PlayerCharacter::GetPlayerHandle();
			g_playerHandle = h.get_handle();
			if (!g_outputModel) {
				FindOutputModel();
			}
			g_step = StepState{};
		}
	}

	// =====================================================================
	// movement simulation
	// =====================================================================
	struct Sim
	{
		float vx{ 0.0f };  // Half-Life units / s
		float vy{ 0.0f };
		float acc{ 0.0f };  // leftover time for the fixed tick
		float pvx{ 0.0f };  // velocity at the previous tick (for interpolation)
		float pvy{ 0.0f };
		std::chrono::steady_clock::time_point last{};
		bool  hasLast{ false };
	} g_sim;

	std::mutex g_simLock;

	struct Stats
	{
		std::uint32_t calls{ 0 };
		std::uint32_t groundCalls{ 0 };
		std::uint32_t clips{ 0 };
		std::uint32_t pushes{ 0 };
		float         wishSpeed{ 0.0f };
		float         outSpeed{ 0.0f };
		float         actualSpeed{ 0.0f };
		int           state{ -1 };
		std::uint32_t material{ 0 };
		float         water{ 0.0f };
		long long     lastPrint{ 0 };
		int           firstCalls{ 0 };
	} g_stats;

	hlmath::Vec2 SimVel() { return { g_sim.vx, g_sim.vy }; }
	void         SetSimVel(hlmath::Vec2 a_v)
	{
		g_sim.vx = a_v.x;
		g_sim.vy = a_v.y;
	}

	void Friction(float a_dt, float a_frictionMult)
	{
		SetSimVel(hlmath::Friction(SimVel(), g_active.friction, g_active.stopSpeed, a_frictionMult, a_dt));
	}

	void Accelerate(float a_dirX, float a_dirY, float a_wishSpeed, float a_dt, float a_accelMult)
	{
		SetSimVel(hlmath::Accelerate(SimVel(), { a_dirX, a_dirY }, a_wishSpeed, g_active.accelerate, a_accelMult, a_dt));
	}

	void WeightMults(float& a_accel, float& a_friction)
	{
		a_accel = 1.0f;
		a_friction = 1.0f;
		if (g_cfg.carryWeight) {
			const float load = g_load.load();
			if (load > 1.0f) {
				a_accel = g_cfg.overencumberedAccelMult;
				a_friction = g_cfg.fullLoadFrictionMult;
			} else {
				const float t = std::clamp(load, 0.0f, 1.0f);
				a_accel = 1.0f + (g_cfg.fullLoadAccelMult - 1.0f) * t;
				a_friction = 1.0f + (g_cfg.fullLoadFrictionMult - 1.0f) * t;
			}
		}
		if (Perks::Bonus()) {
			const float keep = std::max(0.0f, 1.0f - 0.2f * static_cast<float>(Perks::Rank(Perks::kStrongBack)));  // Strong Back
			a_accel = 1.0f - (1.0f - a_accel) * keep;
			a_friction = 1.0f - (1.0f - a_friction) * keep;
		}
		if (g_inPowerArmor.load()) {
			a_accel *= g_cfg.powerArmorAccelMult;
			a_friction *= g_cfg.powerArmorFrictionMult;
		}
	}

	// =====================================================================
	// velocity hook
	// =====================================================================
	using SetVel_t = void (*)(RE::bhkCharacterController*, const RE::hkVector4f&);
	SetVel_t g_origSetVel{ nullptr };

	constexpr std::size_t kSetLinearVelocityIdx = 0x3A;

	RE::bhkCharacterController* PlayerController()
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->currentProcess || !player->currentProcess->middleHigh) {
			return nullptr;
		}
		return player->currentProcess->middleHigh->charController.get();
	}

	void ResetSim(const RE::hkVector4f& a_vanilla)
	{
		g_sim.vx = g_sim.pvx = a_vanilla.x / g_cfg.unitScale;
		g_sim.vy = g_sim.pvy = a_vanilla.y / g_cfg.unitScale;
		g_sim.acc = 0.0f;
	}

	void PrintStats()
	{
		if (!g_cfg.debugLog) {
			return;
		}
		const auto now = NowMs();
		if (now - g_stats.lastPrint < 1000) {
			return;
		}
		g_stats.lastPrint = now;
		REX::INFO("[1s] calls {} ground {} clips {} pushes {} | state {} | wish {:.0f} out {:.0f} actual {:.0f} (HL u/s) | material {:08X} | water {:.0f} | load {:.2f} PA {} swim {} sneak {} 1st {}",
			g_stats.calls, g_stats.groundCalls, g_stats.clips, g_stats.pushes, g_stats.state, g_stats.wishSpeed, g_stats.outSpeed, g_stats.actualSpeed,
			g_stats.material, g_stats.water, g_load.load(), g_inPowerArmor.load(), g_swimming.load(), g_sneaking.load(), g_firstPerson.load());
		g_stats.calls = 0;
		g_stats.groundCalls = 0;
		g_stats.clips = 0;
		g_stats.pushes = 0;
	}

	// ---------------------------------------------------------------------
	// surfaces (Source multiplies friction AND acceleration by the surface)
	// ---------------------------------------------------------------------
	float SurfaceFrictionFor(std::uint32_t a_material, float a_waterDepth)
	{
		if (!g_cfg.surfaceFriction) {
			return 1.0f;
		}
		float f = 1.0f;
		if (a_waterDepth > 1.0f && a_waterDepth < 200.0f) {
			f = Perks::Bonus() && Perks::Rank(Perks::kAquaboy) > 0 ? 1.0f : 0.8f;  // water drags you
		} else {
			auto it = Sounds::g_materialCat.find(a_material);
			const std::string& cat = it != Sounds::g_materialCat.end() ? it->second : std::string();
			if (cat == "ice") f = 0.15f;
			else if (cat == "snow") f = 0.70f;
			else if (cat == "mud") f = 0.75f;
			else if (cat == "sand") f = 0.85f;
			else if (cat == "gravel") f = 0.90f;
			else if (cat == "glass") f = 0.65f;
			else if (cat == "tile") f = 0.90f;
			else if (cat == "grass") f = 0.95f;
		}
		return 1.0f + (f - 1.0f) * std::clamp(g_cfg.surfaceStrength, 0.0f, 1.0f);
	}

	// ---------------------------------------------------------------------
	// input: what the player wants, straight from the stick
	// ---------------------------------------------------------------------
	enum class MoveClass { kWalk, kRun, kSprint, kSneak };

	struct Wish
	{
		float     dirX{ 0.0f }, dirY{ 0.0f };
		float     speed{ 0.0f };  // HL units/s
		float     inX{ 0.0f }, inY{ 0.0f };  // stick: x = right, y = forward
		float     mag{ 0.0f };
		MoveClass cls{ MoveClass::kRun };
	};

	struct InputState
	{
		// Fallout's own top speeds, learned while you play: [0] normal, [1] Power Armor.
		// Stored without your current speed multiplier, so buffs/debuffs don't spoil them.
		float learned[2][4]{ { 80.0f, 210.0f, 281.0f, 110.0f }, { 70.0f, 180.0f, 240.0f, 100.0f } };
		float heldTime{ 0.0f };
		MoveClass lastClass{ MoveClass::kRun };
		float blocked{ 0.0f };
		float forced{ 0.0f };
		float prevYaw{ 0.0f };
		bool  hasYaw{ false };
		float yawRate{ 0.0f };  // radians/s, positive = turning right
	} g_input;

	float ClassSpeed(MoveClass a_cls, bool a_sprinting)
	{
		float speed = 0.0f;
		if (g_cfg.speedMode == 1) {
			const bool hl1 = g_cfg.mode == Mode::kHL1;
			const float run = hl1 ? 320.0f : 190.0f;
			const float walk = hl1 ? 320.0f * 0.52f : 150.0f;
			switch (a_cls) {
			case MoveClass::kWalk: speed = walk; break;
			case MoveClass::kSprint: speed = hl1 ? (g_cfg.hl1Sprint == 1 ? run * g_cfg.hl1SprintMult : run) : 320.0f; break;
			case MoveClass::kSneak: speed = run * 0.3333f; break;
			default: speed = run; break;
			}
			speed *= g_speedMult.load();
			if (g_inPowerArmor.load()) {
				speed *= g_cfg.hlPowerArmorSpeedMult;
			}
		} else {
			speed = g_input.learned[g_inPowerArmor.load() ? 1 : 0][static_cast<int>(a_cls)] * g_speedMult.load();
		}
		if (a_sprinting && Perks::Bonus()) {
			speed *= 1.0f + 0.04f * static_cast<float>(Perks::Rank(Perks::kMovingTarget));  // Moving Target
		}
		return speed;
	}

	Wish ReadWish(const RE::hkVector4f& a_vanilla, float a_dt)
	{
		Wish w;
		auto player = RE::PlayerCharacter::GetSingleton();
		auto controls = RE::PlayerControls::GetSingleton();
		const float vanX = a_vanilla.x / g_cfg.unitScale;
		const float vanY = a_vanilla.y / g_cfg.unitScale;
		const float vanSpeed = std::hypot(vanX, vanY);

		const bool sprinting = player && player->DoGetSprinting();
		const bool running = controls ? controls->data.running : true;
		w.cls = g_sneaking.load() ? MoveClass::kSneak : sprinting ? MoveClass::kSprint : running ? MoveClass::kRun : MoveClass::kWalk;

		if (controls) {
			w.inX = std::clamp(controls->data.moveInputVec.x, -1.0f, 1.0f);
			w.inY = std::clamp(controls->data.moveInputVec.y, -1.0f, 1.0f);
		}
		if (controls && controls->data.autoMove && std::fabs(w.inY) < 0.05f) {
			w.inY = 1.0f;  // Fallout's auto-move
		}
		w.mag = std::min(1.0f, std::hypot(w.inX, w.inY));

		// yaw and how fast you're turning (used by the air strafe assist)
		const float yaw = player ? player->data.angle.z : 0.0f;
		if (g_input.hasYaw && a_dt > 0.0f) {
			float d = yaw - g_input.prevYaw;
			while (d > 3.14159265f) d -= 6.2831853f;
			while (d < -3.14159265f) d += 6.2831853f;
			g_input.yawRate = d / a_dt;
		}
		g_input.prevYaw = yaw;
		g_input.hasYaw = true;

		// learn Fallout's own top speed for this kind of movement (used with Fallout speeds)
		if (w.cls == g_input.lastClass && w.mag > 0.95f && vanSpeed > 1.0f) {
			g_input.heldTime += a_dt;
			if (g_input.heldTime > 0.5f) {
				auto&       l = g_input.learned[g_inPowerArmor.load() ? 1 : 0][static_cast<int>(w.cls)];
				const float base = vanSpeed / std::max(g_speedMult.load(), 0.1f);
				l += (base - l) * std::min(1.0f, a_dt * 2.0f);
			}
		} else {
			g_input.heldTime = 0.0f;
		}
		g_input.lastClass = w.cls;

		// Fallout moving you without the stick (scripted walks, auto-move from a mod): follow it.
		if (w.mag < 0.05f && vanSpeed > 30.0f) {
			g_input.forced += a_dt;
		} else {
			g_input.forced = 0.0f;
		}
		const bool direct = g_cfg.directInput && g_firstPerson.load() && g_input.forced < 0.3f;
		if (direct) {
			// Fallout still decides WHEN you may move: if it wants you still while you push the stick
			// (dialogue, scripted scenes...), respect that after a moment.
			if (vanSpeed < 1.0f && w.mag > 0.2f) {
				g_input.blocked += a_dt;
			} else {
				g_input.blocked = 0.0f;
			}
			if (w.mag < 0.05f || g_input.blocked > 0.25f) {
				return w;  // speed 0
			}
			const float fx = std::sin(yaw), fy = std::cos(yaw);   // forward
			const float rx = std::cos(yaw), ry = -std::sin(yaw);  // right
			float dx = fx * w.inY + rx * w.inX;
			float dy = fy * w.inY + ry * w.inX;
			const float len = std::hypot(dx, dy);
			if (len < 0.001f) {
				return w;
			}
			w.dirX = dx / len;
			w.dirY = dy / len;
			w.speed = ClassSpeed(w.cls, sprinting) * w.mag;
		} else {
			// third person: Fallout's direction (it turns the body), our speed
			if (vanSpeed < 0.5f) {
				return w;
			}
			w.dirX = vanX / vanSpeed;
			w.dirY = vanY / vanSpeed;
			w.speed = g_cfg.speedMode == 1 ? ClassSpeed(w.cls, sprinting) * std::max(w.mag, 0.3f) : vanSpeed;
		}
		return w;
	}

	// ---------------------------------------------------------------------
	// Half-Life air acceleration (PM_AirAccelerate) + optional stick assist
	// ---------------------------------------------------------------------
	void AirAccelerate(const Wish& a_wish, float a_dt, float a_accelMult)
	{
		if (a_wish.speed <= 0.0f) {
			return;
		}
		float dirX = a_wish.dirX, dirY = a_wish.dirY;
		const float speed = std::hypot(g_sim.vx, g_sim.vy);
		const float cap = g_cfg.airCap;
		const float accel = g_cfg.airAccelerate * a_accelMult;
		const float accelSpeedMax = accel * a_wish.speed * a_dt;

		// Stick assist: when you strafe and turn the same way (the Half-Life technique),
		// steer the wish direction toward the angle that gains the most speed.
		const float assist = std::clamp(g_cfg.strafeAssist, 0.0f, 1.0f);
		if (assist > 0.0f && speed > cap && std::fabs(a_wish.inX) > 0.3f && a_wish.inX * g_input.yawRate > 0.2f) {
			const float vx = g_sim.vx / speed, vy = g_sim.vy / speed;
			const float cross = vx * dirY - vy * dirX;  // which side of the velocity the wish is on
			const float side = cross >= 0.0f ? 1.0f : -1.0f;
			const float c = std::clamp((cap - std::min(accelSpeedMax, cap)) / speed, 0.0f, 1.0f);
			const float th = std::acos(c);
			const float cs = std::cos(th), sn = std::sin(th) * side;
			const float ox = vx * cs - vy * sn, oy = vx * sn + vy * cs;  // velocity rotated by th
			dirX = dirX + (ox - dirX) * assist;
			dirY = dirY + (oy - dirY) * assist;
			const float l = std::hypot(dirX, dirY);
			if (l > 0.001f) {
				dirX /= l;
				dirY /= l;
			}
		}

		SetSimVel(hlmath::AirAccelerate(SimVel(), { dirX, dirY }, a_wish.speed, accel, cap, 1.0f, a_dt));
	}

	// ---------------------------------------------------------------------
	// jumping: HL1 bunnyhop cap / HL2 forward boost (and its ABH quirk)
	// ---------------------------------------------------------------------
	void OnJump(const Wish& a_wish)
	{
		const float maxSpeed = std::max(ClassSpeed(a_wish.cls == MoveClass::kSprint ? MoveClass::kSprint : MoveClass::kRun, false), 1.0f);
		if (g_cfg.mode == Mode::kHL1) {
			if (g_cfg.hl1BhopCap) {
				SetSimVel(hlmath::HL1BunnyCap(SimVel(), maxSpeed));
			}
			return;
		}
		if (!g_cfg.hl2JumpBoost) {
			return;
		}
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		const float        yaw = player->data.angle.z;
		const hlmath::Vec2 forward{ std::sin(yaw), std::cos(yaw) };
		const bool         sprintOrDuck = a_wish.cls == MoveClass::kSprint || a_wish.cls == MoveClass::kSneak;
		const float        forwardMove = a_wish.inY * ClassSpeed(a_wish.cls, false);
		const hlmath::Vec2 before = SimVel();
		hlmath::Vec2       after = hlmath::HL2JumpBoost(before, forward, forwardMove, maxSpeed, sprintOrDuck);
		const bool abhAllowed = g_cfg.hl2ABH && !(Perks::Unlock() && Perks::Rank(Perks::kMovingTarget) < 3);
		if (!abhAllowed) {
			const float limit = std::max(before.Length(), maxSpeed * (sprintOrDuck ? 1.1f : 1.5f));
			const float len = after.Length();
			if (len > limit && len > 0.0f) {
				after = after * (limit / len);
			}
		}
		SetSimVel(after);
	}

	std::atomic<long long> g_autoJumpUntil{ 0 };  // auto-jump: re-jump if jump is held until this time (ms)

	// Auto-jump: the game only jumps when the jump key is freshly pressed. While you hold it, the jump
	// handler keeps getting "held" events; right after a landing we hand it one as a fresh press.
	namespace AutoJump
	{
		using OnButton_t = void (*)(void*, const RE::ButtonEvent*);
		OnButton_t g_orig{ nullptr };
		int        g_logs{ 0 };

		void Hooked(void* a_this, const RE::ButtonEvent* a_event)
		{
			if (a_event && a_event->QPressed() && a_event->QHeldDownSecs() > 0.0f && NowMs() < g_autoJumpUntil.load()) {
				g_autoJumpUntil = 0;
				alignas(16) std::byte buf[sizeof(RE::ButtonEvent)];
				std::memcpy(buf, a_event, sizeof(RE::ButtonEvent));
				reinterpret_cast<RE::ButtonEvent*>(buf)->heldDownSecs = 0.0f;  // looks like a fresh press
				if (g_cfg.debugLog && g_logs++ < 5) {
					REX::INFO("auto-jump");
				}
				return g_orig(a_this, reinterpret_cast<const RE::ButtonEvent*>(buf));
			}
			g_orig(a_this, a_event);
		}

		void Install()
		{
			static bool done = false;
			auto        controls = RE::PlayerControls::GetSingleton();
			if (done || !controls || !controls->jumpHandler) {
				return;
			}
			done = true;
			const void* handler = controls->jumpHandler;
			const char* name = RttiName(handler);
			if (!name || std::string_view(name).find("JumpHandler") == std::string_view::npos) {
				REX::ERROR("Jump handler has an unexpected type ({}); auto-jump NOT installed", name ? name : "unknown");
				return;
			}
			auto vtbl = *reinterpret_cast<std::uintptr_t* const*>(handler);
			if (!Game().Contains(vtbl[8])) {
				REX::WARN("Jump key handling already changed by another plugin; chaining after it");
			}
			g_orig = reinterpret_cast<OnButton_t>(vtbl[8]);  // BSInputEventUser::OnButtonEvent
			REL::WriteSafeData(reinterpret_cast<std::uintptr_t>(vtbl + 8), reinterpret_cast<std::uintptr_t>(&Hooked));
			REX::INFO("Auto-jump ready ({})", name);
		}
	}

	std::atomic<long long> g_crouchPressedMs{ 0 };  // when the sneak key was last pressed (ms)
	std::atomic_bool       g_landSneak{ false };     // land sneaking after a crouch-jump
	std::atomic_bool       g_crouchWasSneaking{ false };

	namespace CrouchKey
	{
		using OnButton_t = void (*)(void*, const RE::ButtonEvent*);
		OnButton_t g_orig{ nullptr };

		void Hooked(void* a_this, const RE::ButtonEvent* a_event)
		{
			if (a_event && a_event->QJustPressed()) {
				auto p = RE::PlayerCharacter::GetSingleton();
				g_crouchWasSneaking = p && p->IsSneaking();  // before Fallout toggles it
				g_crouchPressedMs = NowMs();                 // the movement code decides if it's a crouch-jump
			}
			g_orig(a_this, a_event);  // Fallout still gets the key as normal
		}

		void Install()
		{
			static bool done = false;
			auto        controls = RE::PlayerControls::GetSingleton();
			if (done || !controls || !controls->sneakHandler) {
				return;
			}
			done = true;
			const void* handler = controls->sneakHandler;
			const char* name = RttiName(handler);
			if (!name || std::string_view(name).find("SneakHandler") == std::string_view::npos) {
				REX::ERROR("Sneak handler has an unexpected type ({}); crouch-jump NOT installed", name ? name : "unknown");
				return;
			}
			auto vtbl = *reinterpret_cast<std::uintptr_t* const*>(handler);
			if (!Game().Contains(vtbl[8])) {
				REX::WARN("Sneak key handling already changed by another plugin; chaining after it");
			}
			g_orig = reinterpret_cast<OnButton_t>(vtbl[8]);
			REL::WriteSafeData(reinterpret_cast<std::uintptr_t>(vtbl + 8), reinterpret_cast<std::uintptr_t>(&Hooked));
			REX::INFO("Crouch-jump ready ({})", name);
		}
	}

	struct AirState
	{
		bool  prevGround{ true };
		bool  prevRawGround{ true };
		float airTime{ 0.0f };
		bool  jumpedThisAir{ false };
		float maxFall{ 0.0f };  // fastest downward speed this time in the air (HL units/s)
		hlmath::Vec2 lastOut{};  // what we told the game last step
		float jumpStartZ{ 0.0f }, jumpPeakZ{ 0.0f }, jumpTakeoffVz{ 0.0f }, jumpTime{ 0.0f };
		bool  measuring{ false };
		// crouch-jump
		bool  crouchJumped{ false };   // already used this jump
		float boostVz{ 0.0f };         // extra upward speed (m/s) we add
		int   boostMode{ 0 };          // 0 = not known yet, 1 = Fallout keeps our speed, 2 = we keep adding it
		float lastOutVz{ 0.0f };
		float lastInVz{ 0.0f };
		float gravity{ 13.24f };       // measured in the air (m/s^2); Fallout default 9.81 * 1.35
		float prevVz{ 0.0f };
		bool  hasPrevVz{ false };
		float jumpHeightHL{ 43.0f };   // Fallout's own jump height (HL units), learned from jumps
		bool  hasLastOut{ false };
		float grace{ 0.0f };
		bool  airborneSim{ false };  // our horizontal velocity is in charge while airborne
	} g_air;

	// Crouch-jump: Fallout can't shrink your body mid-air, so we give the upward speed that adds exactly
	// the height Half-Life's duck-jump gives. Physics still handles ceilings.
	// Returns the vertical speed (m/s) to hand to the game.
	float CrouchJumpVertical(float a_inVz, float a_actualVz, bool a_rawGround, float a_dt)
	{
		// measure gravity in the air from the game's own vertical speed
		if (!a_rawGround && g_air.hasPrevVz && a_dt > 0.001f && g_air.boostVz == 0.0f) {
			const float g = (g_air.prevVz - a_inVz) / a_dt;
			if (g > 3.0f && g < 40.0f) {
				g_air.gravity += (g - g_air.gravity) * 0.1f;
			}
		}
		g_air.prevVz = a_inVz;
		g_air.hasPrevVz = !a_rawGround;

		float outVz = a_inVz;
		if (a_rawGround || g_cfg.mode == Mode::kVanilla || !g_cfg.crouchJump) {
			return outVz;
		}

		// keep an active boost going, in whichever way the game needs
		if (g_air.boostVz > 0.0f) {
			if (g_air.boostMode == 0) {
				// first step after the boost: did Fallout carry on from our speed, or from its own?
				const float fromOurs = std::fabs(a_inVz - (g_air.lastOutVz - g_air.gravity * a_dt));
				const float fromItsOwn = std::fabs(a_inVz - (g_air.lastInVz - g_air.gravity * a_dt));
				g_air.boostMode = fromOurs <= fromItsOwn ? 1 : 2;
				if (g_cfg.debugLog) {
					REX::INFO("crouch-jump: Fallout {} our vertical speed", g_air.boostMode == 1 ? "keeps" : "ignores");
				}
			}
			if (g_air.boostMode == 2) {
				if (a_actualVz < g_air.lastOutVz - g_air.gravity * a_dt - 1.0f) {
					g_air.boostVz = 0.0f;  // hit a ceiling: stop pushing up
				} else {
					outVz = a_inVz + g_air.boostVz;
				}
			} else {
				g_air.boostVz = 0.0f;  // Fallout already carries it
			}
		}

		// a fresh crouch press in the air?
		const bool pressed = NowMs() - g_crouchPressedMs.load() < 120;
		if (pressed && !g_air.crouchJumped && !g_crouchWasSneaking.load() && g_air.airTime > 0.05f) {
			const float vz = a_actualVz;  // m/s, + is up
			const float window = g_air.gravity * g_cfg.crouchJumpWindow;  // speed lost in the window after the peak
			if (vz > -window) {
				const bool  hl1 = g_cfg.mode == Mode::kHL1;
				float       liftHL = hl1 ? 18.0f : 36.0f;  // HL1: hull centre, HL2: feet move up the full difference
				if (g_cfg.crouchJumpHeight == 1) {
					liftHL *= g_air.jumpHeightHL / (hl1 ? 45.0f : 21.3f);  // same share of Fallout's own jump
				}
				const float lift = liftHL * g_cfg.unitScale;  // metres
				const float newVz = hlmath::CrouchLiftSpeed(vz, g_air.gravity, lift);
				g_air.crouchJumped = true;
				g_landSneak = true;
				g_crouchPressedMs = 0;
				g_air.boostVz = newVz - a_inVz;
				g_air.boostMode = 0;
				outVz = newVz;
				if (g_cfg.debugLog) {
					REX::INFO("crouch-jump: +{:.0f} HL units, vertical {:.2f} -> {:.2f} m/s (gravity {:.2f})", liftHL, vz, newVz, g_air.gravity);
				}
			}
		}
		g_air.lastOutVz = outVz;
		g_air.lastInVz = a_inVz;
		return outVz;
	}

	void HookedSetLinearVelocity(RE::bhkCharacterController* a_this, const RE::hkVector4f& a_vel)
	{
		if (!a_this || a_this != PlayerController() || (g_cfg.mode == Mode::kVanilla && SoundSet() == 0)) {
			return g_origSetVel(a_this, a_vel);
		}

		RequestCacheUpdate();

		std::scoped_lock lock(g_simLock);

		const auto now = std::chrono::steady_clock::now();
		float      dt = 0.0f;
		if (g_sim.hasLast) {
			dt = std::chrono::duration<float>(now - g_sim.last).count();
		}
		g_sim.last = now;
		g_sim.hasLast = true;

		const float wallDt = dt;
		const float stepDt = a_this->stepInfo.m_deltaTime.val();
		if (stepDt > 0.0005f && stepDt < 0.25f) {
			dt = stepDt;  // the game's own step time: follows VATS slow-motion and other time changes
		}

		const int  state = static_cast<int>(a_this->context.m_currentState);
		const bool rawGround = a_this->context.m_currentState == RE::hknpCharacterState::hknpCharacterStateType::kOnGround;

		RE::hkVector4f actual;
		a_this->GetLinearVelocityImpl(actual);

		auto       ui = RE::UI::GetSingleton();
		const bool menus = ui && ui->menuMode != 0;

		// Stairs and bumps make the game report "in the air" for a moment. Unless you jumped,
		// short moments like that still count as ground, so you keep full control and friction.
		const bool jumpedNow = g_air.prevRawGround && !rawGround && a_vel.z / g_cfg.unitScale > 50.0f;
		g_air.prevRawGround = rawGround;
		if (rawGround) {
			g_air.airTime = 0.0f;
		} else {
			g_air.airTime += dt;
			if (jumpedNow) {
				g_air.jumpedThisAir = true;
			}
		}
		const bool onGround = rawGround || (!g_air.jumpedThisAir && g_air.airTime < 0.12f);

		float waterDepth = 0.0f;
		if (auto player = RE::PlayerCharacter::GetSingleton()) {
			const float wh = a_this->waterHeight;
			waterDepth = (wh > -1.0e6f && wh < 1.0e6f) ? wh - player->data.location.z : 0.0f;
		}

		g_stats.calls++;
		g_stats.state = state;
		g_stats.material = a_this->supportMaterial;
		g_stats.actualSpeed = std::hypot(actual.x, actual.y) / g_cfg.unitScale;
		g_stats.water = waterDepth;

		if (g_cfg.debugLog && g_stats.firstCalls < 10) {
			g_stats.firstCalls++;
			REX::INFO("call {}: dt {:.4f} (clock {:.4f}) state {} in ({:.3f}, {:.3f}, {:.3f}) actual ({:.3f}, {:.3f}, {:.3f}) flags {:08X}",
				g_stats.firstCalls, dt, wallDt, state, a_vel.x, a_vel.y, a_vel.z, actual.x, actual.y, actual.z, a_this->flags);
		}

		// measurements: Fallout's jump height (used by the scaled crouch-jump) + debug info
		{
			static bool logged = false;
			if (g_cfg.debugLog && !logged) {
				logged = true;
				REX::INFO("physics: gravity {:.3f} height {:.3f} radius {:.3f} scale {:.3f} actorHeight {:.3f} shapeType {}",
					a_this->gravity, a_this->height, a_this->radius, a_this->scale, a_this->actorHeight,
					static_cast<int>(a_this->shapeType.underlying()));
			}
			auto player = RE::PlayerCharacter::GetSingleton();
			const float z = player ? player->data.location.z : 0.0f;
			if (jumpedNow) {
				g_air.jumpStartZ = z;
				g_air.jumpPeakZ = z;
				g_air.jumpTakeoffVz = a_vel.z;
				g_air.jumpTime = 0.0f;
				g_air.measuring = true;
			} else if (g_air.measuring && !rawGround) {
				g_air.jumpPeakZ = std::max(g_air.jumpPeakZ, z);
				g_air.jumpTime += dt;
			}
			if (g_air.measuring && rawGround) {
				g_air.measuring = false;
				const float rise = g_air.jumpPeakZ - g_air.jumpStartZ;  // game units
				if (!g_air.crouchJumped && rise > 10.0f && rise < 200.0f) {
					const float hl = rise * (0.0254f * 69.99124f);
					g_air.jumpHeightHL += (hl - g_air.jumpHeightHL) * 0.3f;
				}
				if (g_cfg.debugLog) {
					REX::INFO("jump: takeoff {:.2f} m/s ({:.0f} HL u/s), peak +{:.1f} game units ({:.1f} HL units), air time {:.2f} s, gravity {:.2f} m/s2{}",
						g_air.jumpTakeoffVz, g_air.jumpTakeoffVz / g_cfg.unitScale, rise, rise * (0.0254f * 69.99124f), g_air.jumpTime,
						g_air.gravity, g_air.crouchJumped ? " (crouch-jump)" : "");
				}
			}
		}

		// one landing / takeoff event for sounds AND camera (each works without the other)
		if (!rawGround) {
			g_air.maxFall = std::max(g_air.maxFall, -actual.z / g_cfg.unitScale);
		}
		const bool landEvent = !g_air.prevGround && onGround;
		const float fallSpeed = g_air.maxFall;
		if (landEvent && !menus) {
			Camera::OnLand(fallSpeed);
			if (g_landSneak.exchange(false) && g_cfg.crouchLandSneak) {
				if (auto task = F4SE::GetTaskInterface()) {
					task->AddTask([]() {
						if (auto p = RE::PlayerCharacter::GetSingleton(); p && !p->IsSneaking()) {
							p->SetSneaking(true);  // Half-Life keeps you ducked after a duck-jump
						}
					});
				}
			}
		}
		if (rawGround) {
			g_air.crouchJumped = false;
			g_air.boostVz = 0.0f;
			g_air.boostMode = 0;
		}
		Sounds::Update(a_this, dt, onGround, actual, menus, jumpedNow, landEvent, fallSpeed);
		if (onGround) {
			g_air.maxFall = 0.0f;
		}
		Camera::g_velX = actual.x / g_cfg.unitScale;
		Camera::g_velY = actual.y / g_cfg.unitScale;
		Camera::g_onGround = onGround;

		const Wish wish = ReadWish(a_vel, dt);
		g_stats.wishSpeed = wish.speed;

		const bool jumped = jumpedNow;
		const bool landed = !g_air.prevGround && onGround && g_air.jumpedThisAir;  // landing from a real jump
		g_air.prevGround = onGround;
		if (rawGround) {
			g_air.jumpedThisAir = false;
		}

		// Fallout asking for far more speed than any walk/run/sprint means something else is moving you
		// (a mod's dash, a vehicle, a scripted launch): let Fallout do it.
		const float vanillaSpeed = std::hypot(a_vel.x, a_vel.y) / g_cfg.unitScale;
		const bool  externalMove = vanillaSpeed > 2.0f * std::max(ClassSpeed(MoveClass::kSprint, true), 150.0f);

		// footstep speed thresholds, scaled from Half-Life's to your current speeds
		{
			const bool  hl1 = SoundSet() == 1;
			const bool  hlSpeeds = g_cfg.speedMode == 1;
			const float walk = ClassSpeed(MoveClass::kWalk, false);
			const float runS = ClassSpeed(MoveClass::kRun, false);
			const float sprint = ClassSpeed(MoveClass::kSprint, false);
			const float sneak = ClassSpeed(MoveClass::kSneak, false);
			if (g_sneaking.load()) {
				// HL: 60 / 80 while ducking
				g_stepVelWalk = hlSpeeds ? 60.0f : 0.56f * sneak;
				g_stepVelRun = hlSpeeds ? 80.0f : (hl1 ? 0.75f * sneak : 1.0e9f);
			} else if (hl1) {
				// HL1: 120 / 210 with walk 166 and run 320
				g_stepVelWalk = hlSpeeds ? 120.0f : 0.72f * walk;
				g_stepVelRun = hlSpeeds ? 210.0f : walk + 0.286f * (runS - walk);
			} else {
				// HL2: 90 / 220 with walk 150, run 190, sprint 320 (normal running uses walking steps)
				g_stepVelWalk = hlSpeeds ? 90.0f : 0.6f * walk;
				g_stepVelRun = hlSpeeds ? 220.0f : runS + 0.231f * (sprint - runS);
			}
		}

		const bool skip = externalMove || g_cfg.mode == Mode::kVanilla || menus || g_swimming.load() ||
		                  (g_inPowerArmor.load() && !g_cfg.enableInPowerArmor) || dt <= 0.0f || dt > 0.25f;
		const bool airLocked = Perks::Unlock() && g_agility.load() < 5.0f;  // unlock mode: Agility 5 for air control
		const bool airSim = !onGround && g_cfg.airControl && !airLocked;
		if (skip || (!onGround && !airSim)) {
			g_air.hasLastOut = false;
			ResetSim(a_vel);
			g_air.airborneSim = false;
			g_stats.outSpeed = std::hypot(a_vel.x, a_vel.y) / g_cfg.unitScale;
			PrintStats();
			return g_origSetVel(a_this, a_vel);
		}

		// ---- collisions and pushes: compare what the game actually did with what we asked for last step
		{
			const hlmath::Vec2 act{ actual.x / g_cfg.unitScale, actual.y / g_cfg.unitScale };
			if (g_air.hasLastOut) {
				const hlmath::Vec2 asked = g_air.lastOut;
				const hlmath::Vec2 delta = act - asked;
				const float        askedLen = asked.Length();
				const float        actLen = act.Length();
				if (delta.Length() > 5.0f) {
					if (g_cfg.pushes && actLen > askedLen + g_cfg.pushThreshold) {
						// something pushed you (hit, explosion, script): keep it as momentum, Half-Life style
						SetSimVel(SimVel() + delta);
						g_stats.pushes++;
					} else if (g_cfg.wallClip && askedLen > 10.0f && actLen <= askedLen + 5.0f) {
						// a wall/NPC/door took part of the motion: the removed part points into the surface.
						// Remove that same part from our velocity (Source's ClipVelocity) so you slide along it.
						const hlmath::Vec2 normal = (asked - act).Normalized();
						const float        along = std::fabs(normal.Dot(asked.Normalized()));
						const float        lost = 1.0f - actLen / askedLen;
						// Straight-line losses under 50% are slopes and steps (Havok bends you along the ground),
						// not walls: leave those alone so you don't slow down going uphill.
						if (!(along > 0.95f && lost < 0.5f)) {
							SetSimVel(hlmath::ClipVelocity(SimVel(), normal));
							g_stats.clips++;
						}
					}
				}
			}
		}

		float accelMult, frictionMult;
		WeightMults(accelMult, frictionMult);

		// runs a step function at the chosen smoothness: every frame, or at Half-Life's tick (+ blending)
		auto run = [&](auto&& a_step) {
			if (g_cfg.smoothing == 2) {
				a_step(dt);  // every physics step, real step time: no added delay
				return SimVel();
			}
			g_sim.acc += dt;
			int steps = 0;
			while (g_sim.acc >= g_active.tick && steps < 16) {
				g_sim.pvx = g_sim.vx;
				g_sim.pvy = g_sim.vy;
				a_step(g_active.tick);
				g_sim.acc -= g_active.tick;
				++steps;
			}
			if (steps == 16) {
				g_sim.acc = 0.0f;
			}
			if (g_cfg.smoothing == 1) {
				const float t = std::clamp(g_sim.acc / g_active.tick, 0.0f, 1.0f);
				return hlmath::Vec2{ g_sim.pvx + (g_sim.vx - g_sim.pvx) * t, g_sim.pvy + (g_sim.vy - g_sim.pvy) * t };
			}
			return SimVel();
		};

		hlmath::Vec2 outV;
		if (!onGround) {
			// ---- in the air: Half-Life air control, momentum kept
			if (!g_air.airborneSim) {
				g_air.airborneSim = true;  // just left the ground: carry our ground velocity into the air
				g_sim.pvx = g_sim.vx;
				g_sim.pvy = g_sim.vy;
			}
			if (jumped) {
				OnJump(wish);
				g_sim.pvx = g_sim.vx;  // the jump boost is instant, don't blend it in
				g_sim.pvy = g_sim.vy;
			}
			float airMult = 1.0f;
			if (Perks::Bonus()) {
				airMult *= std::clamp(1.0f + (g_agility.load() - 5.0f) * 0.04f, 0.8f, 1.25f);  // Agility
			}
			outV = run([&](float a_t) { AirAccelerate(wish, a_t, airMult); });
		} else {
			// ---- on the ground
			g_air.airborneSim = false;
			g_stats.groundCalls++;
			const bool bhopAllowed = g_cfg.bhop && !(Perks::Unlock() && Perks::Rank(Perks::kMovingTarget) < 1);
			if (landed && bhopAllowed) {
				// jump again in time and friction never touches you; Action Boy/Girl widens the window
				const float perkBonus = Perks::Bonus() ? 0.025f * static_cast<float>(Perks::Rank(Perks::kActionBoy)) : 0.0f;
				g_air.grace = g_cfg.bhopGrace + perkBonus;
				const bool autoJump = g_cfg.autoJump == 2 || (g_cfg.autoJump == 1 && Perks::Bonus() && Perks::MaxRank(Perks::kActionBoy));
				if (autoJump) {
					g_autoJumpUntil = NowMs() + 150;  // the jump key handler re-jumps if you're holding jump
				}
			}
			const float surface = SurfaceFrictionFor(a_this->supportMaterial, waterDepth);
			outV = run([&](float a_t) {
				if (g_air.grace > 0.0f) {
					g_air.grace -= a_t;
				} else {
					const bool edge = g_edgeAhead.load() && (g_cfg.edgeFriction == 2 || (g_cfg.edgeFriction == 1 && g_cfg.mode == Mode::kHL1));
					Friction(a_t, frictionMult * surface * (edge ? g_cfg.edgeFrictionMult : 1.0f));
				}
				Accelerate(wish.dirX, wish.dirY, wish.speed, a_t, accelMult * surface);
			});
		}
		const float outX = outV.x, outY = outV.y;
		g_air.lastOut = outV;
		g_air.hasLastOut = true;

		RE::hkVector4f out = a_vel;
		out.x = outX * g_cfg.unitScale;
		out.y = outY * g_cfg.unitScale;
		out.z = CrouchJumpVertical(a_vel.z, actual.z, rawGround, dt);
		{
			const float sp = std::hypot(outX, outY);
			g_moveDirX = sp > 1.0f ? outX / sp : 0.0f;
			g_moveDirY = sp > 1.0f ? outY / sp : 0.0f;
		}
		g_stats.outSpeed = std::hypot(outX, outY);
		PrintStats();
		g_origSetVel(a_this, out);
	}

	bool TryInstallHook()
	{
		auto con = PlayerController();
		if (!con) {
			return false;
		}
		auto vtbl = *reinterpret_cast<std::uintptr_t**>(con);
		static std::uintptr_t* hookedVtbl = nullptr;
		if (vtbl == hookedVtbl) {
			return true;  // already hooked (never hook twice: another plugin may have chained after us)
		}
		if (hookedVtbl) {
			REX::WARN("Player controller changed type (vtable {:X}); Half-Life movement stays on the first one only",
				reinterpret_cast<std::uintptr_t>(vtbl));
			return true;
		}
		const char* name = RttiName(con);
		if (!name || std::string_view(name).find("bhkChar") == std::string_view::npos) {
			REX::ERROR("Player controller has an unexpected type ({}); movement hook NOT installed", name ? name : "unknown");
			hookedVtbl = vtbl;
			return true;
		}
		const auto slot = reinterpret_cast<std::uintptr_t>(vtbl + kSetLinearVelocityIdx);
		const auto current = vtbl[kSetLinearVelocityIdx];
		if (!Game().Contains(current)) {
			REX::WARN("SetLinearVelocity is already changed by another plugin; chaining after it");
		}
		g_origSetVel = reinterpret_cast<SetVel_t>(current);
		REL::WriteSafeData(slot, reinterpret_cast<std::uintptr_t>(&HookedSetLinearVelocity));
		hookedVtbl = vtbl;
		REX::INFO("Hooked player controller ({}): vtable +{:X}, slot 0x3A was +{:X}", name,
			reinterpret_cast<std::uintptr_t>(vtbl) - Game().base, current - Game().base);
		return true;
	}

	void ScheduleInstall(int a_triesLeft)
	{
		auto task = F4SE::GetTaskInterface();
		if (!task) {
			return;
		}
		task->AddTask([a_triesLeft]() {
			if (TryInstallHook()) {
				return;
			}
			if (a_triesLeft > 0) {
				ScheduleInstall(a_triesLeft - 1);
			} else {
				REX::WARN("Could not find the player's character controller");
			}
		});
	}

	// Fresh start after loading a save or starting a new game: nothing carries over.
	void ResetRuntimeState()
	{
		std::scoped_lock lock(g_simLock);
		g_sim = Sim{};
		g_air = AirState{};
		g_input = InputState{};
		g_autoJumpUntil = 0;
		g_crouchPressedMs = 0;
		g_landSneak = false;
		g_edgeAhead = false;
		g_stats = Stats{};
		Camera::Reset();
		Sounds::OnGameLoaded();
	}

	// MCM lives in the pause menu: when it closes, read the settings again so changes apply right away.
	void ReloadSettings()
	{
		{
			std::scoped_lock lock(g_simLock);  // the physics thread reads these while you move
			LoadConfig();
			Sounds::BuildMaterials();
		}
		Perks::Resolve();
		Sounds::InstallFootstepHook();
	}

	class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (!a_event.opening && (a_event.menuName == "PauseMenu"sv || a_event.menuName == "MainMenu"sv)) {
				if (auto task = F4SE::GetTaskInterface()) {
					task->AddTask([]() { ReloadSettings(); });
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	} g_menuSink;

	void OnMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case F4SE::MessagingInterface::kGameDataReady:
			Perks::Resolve();
			Sounds::Init();
			if (auto ui = RE::UI::GetSingleton()) {
				ui->RegisterSink<RE::MenuOpenCloseEvent>(&g_menuSink);
			}
			break;
		case F4SE::MessagingInterface::kPostLoadGame:
		case F4SE::MessagingInterface::kNewGame:
			ResetRuntimeState();
			ScheduleInstall(600);
			if (auto task = F4SE::GetTaskInterface()) {
				task->AddTask([]() {
					Camera::Install();
					AutoJump::Install();
					CrouchKey::Install();
				});
			}
			break;
		default:
			break;
		}
	}
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);
	LoadConfig();
	if (auto messaging = F4SE::GetMessagingInterface()) {
		messaging->RegisterListener(OnMessage);
	}
	REX::INFO("Boston Hopping Overhaul Project loaded");
	return true;
}
