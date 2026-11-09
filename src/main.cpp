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
		bool debugLog{ true };
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
		float sneakVolumeMult{ 0.5f };
		float runStepSpeed{ 140.0f };  // HL units/s; Half-Life itself uses 220
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
	void LoadConfig()
	{
		g_cfg.materialOverrides.clear();
		const bool defaults = ReadIni("Data/MCM/Config/BHOP/settings.ini");
		const bool user = ReadIni("Data/MCM/Settings/BHOP.ini");
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
			if (name.find(a_baseSubstr) != std::string_view::npos) {
				return static_cast<std::int32_t>(b[2]);  // mdisp
			}
		}
		return -1;
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
		if (now - g_lastCacheRequest.load() < 100 || g_cacheQueued.exchange(true)) {
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
				g_punchKick = std::min(a_fallSpeed * 0.013f, 8.0f);
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
			auto vtbl = *reinterpret_cast<std::uintptr_t* const*>(state);
			const auto rotMine = reinterpret_cast<std::uintptr_t>(&HookedGetRotation);
			const auto transMine = reinterpret_cast<std::uintptr_t>(&HookedGetTranslation);
			if (vtbl[0x0C] != rotMine) {
				g_origRot = reinterpret_cast<GetRot_t>(vtbl[0x0C]);
				REL::WriteSafeData(reinterpret_cast<std::uintptr_t>(vtbl + 0x0C), rotMine);
			}
			if (vtbl[0x0D] != transMine) {
				g_origTrans = reinterpret_cast<GetTrans_t>(vtbl[0x0D]);
				REL::WriteSafeData(reinterpret_cast<std::uintptr_t>(vtbl + 0x0D), transMine);
			}
			REX::INFO("Camera hooked (first person state {:X})", reinterpret_cast<std::uintptr_t>(state));
		}
	}

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
					{ "ladder", "pl_ladder" }, { "carpet", "pl_step" }, { "swim", "pl_swim" }, { "powerarmor", "pl_metal" }
				};
				auto it = m.find(a_cat);
				return it != m.end() ? it->second : "pl_step";
			}
			static const std::unordered_map<std::string, std::string> m{
				{ "concrete", "concrete" }, { "metal", "metal" }, { "grate", "metalgrate" }, { "chainlink", "chainlink" },
				{ "duct", "duct" }, { "wood", "wood" }, { "woodpanel", "woodpanel" }, { "grass", "grass" },
				{ "gravel", "gravel" }, { "mud", "mud" }, { "sand", "sand" }, { "snow", "gravel" },
				{ "dirt", "dirt" }, { "tile", "tile" }, { "slosh", "slosh" }, { "wade", "wade" },
				{ "ladder", "ladder" }, { "carpet", "softshoe_generic" }, { "swim", "wade" }, { "powerarmor", "hardboot_generic" }
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
			if (has("snow") || has("ice")) return "snow";
			if (has("dirt") || has("soil") || has("earth")) return "dirt";
			if (has("tile") || has("ceramic") || has("porcelain") || has("glass") || has("marble")) return "tile";
			if (has("water")) return "slosh";
			if (has("carpet") || has("rug") || has("cloth") || has("fabric") || has("mattress")) return "carpet";
			if (has("concrete") || has("stone") || has("rock") || has("asphalt") || has("brick")) return "concrete";
			return {};
		}

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
				if (g_cfg.debugLog) {
					REX::INFO("Material {:08X} '{}' -> {}", matt->materialID, firstName, cat);
				}
			}
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
				idx = std::uniform_int_distribution<std::size_t>(0, files.size() - 1)(g_rng);
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
			bool  prevGround{ true };
			float maxFall{ 0.0f };  // HL units/s, downward
			int   rngTile{ 0 };
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
			std::uniform_int_distribution<int> coin(0, 1);
			const int pick = coin(g_rng);
			g_step.left = !g_step.left;
			if (a_set == 1 && a_group == "pl_tile" && std::uniform_int_distribution<int>(0, 4)(g_rng) == 0) {
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
		void Update(RE::bhkCharacterController* a_con, float a_dt, bool a_onGround, const RE::hkVector4f& a_actual, bool a_menus)
		{
			const int set = SoundSet();
			if (set == 0 || !g_ready || a_menus || a_dt <= 0.0f || a_dt > 0.25f) {
				g_step.prevGround = a_onGround;
				return;
			}

			const float hSpeed = std::hypot(a_actual.x, a_actual.y) / g_cfg.unitScale;
			const float vSpeed = a_actual.z / g_cfg.unitScale;

			// jump + landing
			if (g_cfg.jumpLandSounds) {
				if (!a_onGround) {
					g_step.maxFall = std::max(g_step.maxFall, -vSpeed);
				}
				if (g_step.prevGround && !a_onGround && vSpeed > 50.0f) {
					PlayStep(a_con, 1.0f);  // Half-Life plays the floor sound at full volume when you jump
				}
				if (!g_step.prevGround && a_onGround) {
					const float fall = g_step.maxFall;
					Camera::OnLand(fall);
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
					g_step.maxFall = 0.0f;
				}
			}
			if (a_onGround) {
				g_step.maxFall = 0.0f;
			}
			g_step.prevGround = a_onGround;

			g_step.timer = std::max(0.0f, g_step.timer - a_dt);

			// swimming
			if (g_swimming.load()) {
				if (g_cfg.swimSounds && hSpeed > g_cfg.minStepSpeed && g_step.timer <= 0.0f) {
					Queue(GroupFor(set, "swim"), -1, 0.5f);
					g_step.timer = g_cfg.waterInterval;
				}
				return;
			}

			if (!a_onGround || hSpeed < g_cfg.minStepSpeed || g_step.timer > 0.0f) {
				return;
			}

			const bool running = hSpeed >= g_cfg.runStepSpeed;
			float      vol = running ? g_cfg.runVolume : g_cfg.walkVolume;
			float      interval = running ? g_cfg.runInterval : g_cfg.walkInterval;

			float      depth = 0.0f;
			const auto cat = SurfaceCategory(a_con, depth);
			if (cat == "wade") {
				interval = g_cfg.waterInterval;
				vol = 0.65f;
			}
			if (g_sneaking.load()) {
				vol *= g_cfg.sneakVolumeMult;
				interval += g_cfg.sneakExtraInterval;
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
				if (g_playerHandle && u32[i] == g_playerHandle && ++g_handleHits[i] >= 3) {
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
					if (++g_ptrHits[i] >= 3) {
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

		void InstallFootstepHook()
		{
			if (g_origFootstep || !g_cfg.muteVanillaFootsteps) {
				return;
			}
			const auto vtables = FindVtables("BGSImpactManager");
			if (vtables.empty()) {
				REX::WARN("BGSImpactManager not found; vanilla player footsteps will still play");
				return;
			}
			for (auto& v : vtables) {
				const auto off = BaseOffset(v.classDescRva, "BSTEventSink@VBGSFootstepEvent");
				if (off >= 0 && static_cast<std::uint32_t>(off) == v.subobjectOffset) {
					auto* slot = reinterpret_cast<std::uintptr_t*>(v.vtbl) + 1;  // ProcessEvent
					g_origFootstep = reinterpret_cast<ProcessEvent_t>(*slot);
					REL::WriteSafeData(reinterpret_cast<std::uintptr_t>(slot), reinterpret_cast<std::uintptr_t>(&HookedFootstep));
					REX::INFO("Hooked footstep sounds (vtable {:X}, offset {:X})", v.vtbl - Game().base, off);
					return;
				}
			}
			REX::WARN("Footstep listener not found in {} BGSImpactManager vtables; vanilla player footsteps will still play", vtables.size());
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
		float         wishSpeed{ 0.0f };
		float         outSpeed{ 0.0f };
		float         actualSpeed{ 0.0f };
		int           state{ -1 };
		std::uint32_t material{ 0 };
		float         water{ 0.0f };
		long long     lastPrint{ 0 };
		int           firstCalls{ 0 };
	} g_stats;

	void Friction(float a_dt, float a_frictionMult)
	{
		const float speed = std::hypot(g_sim.vx, g_sim.vy);
		if (speed < 0.1f) {
			g_sim.vx = g_sim.vy = 0.0f;
			return;
		}
		const float control = std::max(speed, g_active.stopSpeed);
		const float drop = control * g_active.friction * a_frictionMult * a_dt;
		const float scale = std::max(speed - drop, 0.0f) / speed;
		g_sim.vx *= scale;
		g_sim.vy *= scale;
	}

	void Accelerate(float a_dirX, float a_dirY, float a_wishSpeed, float a_dt, float a_accelMult)
	{
		const float current = g_sim.vx * a_dirX + g_sim.vy * a_dirY;
		const float add = a_wishSpeed - current;
		if (add <= 0.0f) {
			return;
		}
		const float accelSpeed = std::min(g_active.accelerate * a_accelMult * a_dt * a_wishSpeed, add);
		g_sim.vx += accelSpeed * a_dirX;
		g_sim.vy += accelSpeed * a_dirY;
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
		REX::INFO("[1s] calls {} ground {} | state {} | wish {:.0f} out {:.0f} actual {:.0f} (HL u/s) | material {:08X} | water {:.0f} | load {:.2f} PA {} swim {} sneak {}",
			g_stats.calls, g_stats.groundCalls, g_stats.state, g_stats.wishSpeed, g_stats.outSpeed, g_stats.actualSpeed,
			g_stats.material, g_stats.water, g_load.load(), g_inPowerArmor.load(), g_swimming.load(), g_sneaking.load());
		g_stats.calls = 0;
		g_stats.groundCalls = 0;
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

		const int  state = static_cast<int>(a_this->context.m_currentState);
		const bool onGround = a_this->context.m_currentState == RE::hknpCharacterState::hknpCharacterStateType::kOnGround;

		RE::hkVector4f actual;
		a_this->GetLinearVelocityImpl(actual);

		auto       ui = RE::UI::GetSingleton();
		const bool menus = ui && ui->menuMode != 0;

		g_stats.calls++;
		g_stats.state = state;
		g_stats.material = a_this->supportMaterial;
		g_stats.actualSpeed = std::hypot(actual.x, actual.y) / g_cfg.unitScale;
		if (auto player = RE::PlayerCharacter::GetSingleton()) {
			g_stats.water = a_this->waterHeight - player->data.location.z;
		}

		if (g_cfg.debugLog && g_stats.firstCalls < 10) {
			g_stats.firstCalls++;
			REX::INFO("call {}: dt {:.4f} state {} in ({:.3f}, {:.3f}, {:.3f}) actual ({:.3f}, {:.3f}, {:.3f}) flags {:08X}",
				g_stats.firstCalls, dt, state, a_vel.x, a_vel.y, a_vel.z, actual.x, actual.y, actual.z, a_this->flags);
		}

		Sounds::Update(a_this, dt, onGround, actual, menus);
		Camera::g_velX = actual.x / g_cfg.unitScale;
		Camera::g_velY = actual.y / g_cfg.unitScale;
		Camera::g_onGround = onGround;

		g_stats.wishSpeed = std::hypot(a_vel.x, a_vel.y) / g_cfg.unitScale;

		const bool skip = g_cfg.mode == Mode::kVanilla || !onGround || menus || g_swimming.load() ||
		                  (g_inPowerArmor.load() && !g_cfg.enableInPowerArmor) || dt <= 0.0f || dt > 0.25f;
		if (skip) {
			ResetSim(a_vel);
			g_stats.outSpeed = g_stats.wishSpeed;
			PrintStats();
			return g_origSetVel(a_this, a_vel);
		}

		g_stats.groundCalls++;

		// Something (a wall, an NPC, a door) took speed away: keep that loss, like HL's velocity clipping.
		if (g_cfg.wallClip) {
			const float simSpeed = std::hypot(g_sim.vx, g_sim.vy);
			if (simSpeed > 1.0f && g_stats.actualSpeed < simSpeed * 0.9f) {
				g_sim.vx = actual.x / g_cfg.unitScale;
				g_sim.vy = actual.y / g_cfg.unitScale;
			}
		}

		// Fallout's wanted velocity = Half-Life's wish velocity
		const float wishX = a_vel.x / g_cfg.unitScale;
		const float wishY = a_vel.y / g_cfg.unitScale;
		const float wishSpeed = std::hypot(wishX, wishY);
		const float dirX = wishSpeed > 0.001f ? wishX / wishSpeed : 0.0f;
		const float dirY = wishSpeed > 0.001f ? wishY / wishSpeed : 0.0f;

		float accelMult, frictionMult;
		WeightMults(accelMult, frictionMult);
		const float surfaceFriction = 1.0f;  // stage 2: per material

		float outX = 0.0f, outY = 0.0f;
		if (g_cfg.smoothing == 2) {
			// every physics step, with the real step time: no added delay
			Friction(dt, frictionMult * surfaceFriction);
			Accelerate(dirX, dirY, wishSpeed, dt, accelMult * surfaceFriction);
			outX = g_sim.vx;
			outY = g_sim.vy;
		} else {
			g_sim.acc += dt;
			int steps = 0;
			while (g_sim.acc >= g_active.tick && steps < 16) {
				g_sim.pvx = g_sim.vx;
				g_sim.pvy = g_sim.vy;
				Friction(g_active.tick, frictionMult * surfaceFriction);
				Accelerate(dirX, dirY, wishSpeed, g_active.tick, accelMult * surfaceFriction);
				g_sim.acc -= g_active.tick;
				++steps;
			}
			if (steps == 16) {
				g_sim.acc = 0.0f;
			}
			if (g_cfg.smoothing == 1) {
				// blend between the last two ticks so speed changes smoothly between frames
				const float t = std::clamp(g_sim.acc / g_active.tick, 0.0f, 1.0f);
				outX = g_sim.pvx + (g_sim.vx - g_sim.pvx) * t;
				outY = g_sim.pvy + (g_sim.vy - g_sim.pvy) * t;
			} else {
				outX = g_sim.vx;
				outY = g_sim.vy;
			}
		}

		RE::hkVector4f out = a_vel;
		out.x = outX * g_cfg.unitScale;
		out.y = outY * g_cfg.unitScale;
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
		auto       vtbl = *reinterpret_cast<std::uintptr_t**>(con);
		const auto slot = reinterpret_cast<std::uintptr_t>(vtbl + kSetLinearVelocityIdx);
		const auto current = vtbl[kSetLinearVelocityIdx];
		const auto mine = reinterpret_cast<std::uintptr_t>(&HookedSetLinearVelocity);
		if (current == mine) {
			return true;
		}
		g_origSetVel = reinterpret_cast<SetVel_t>(current);
		REL::WriteSafeData(slot, mine);
		REX::INFO("Hooked player controller: vtable {:X}, SetLinearVelocityImpl {:X}", reinterpret_cast<std::uintptr_t>(vtbl), current);
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

	// MCM lives in the pause menu: when it closes, read the settings again so changes apply right away.
	void ReloadSettings()
	{
		{
			std::scoped_lock lock(g_simLock);
			LoadConfig();
		}
		Sounds::BuildMaterials();
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
			Sounds::Init();
			if (auto ui = RE::UI::GetSingleton()) {
				ui->RegisterSink<RE::MenuOpenCloseEvent>(&g_menuSink);
			}
			break;
		case F4SE::MessagingInterface::kPostLoadGame:
		case F4SE::MessagingInterface::kNewGame:
			{
				std::scoped_lock lock(g_simLock);
				g_sim = Sim{};
				Sounds::OnGameLoaded();
			}
			ScheduleInstall(600);
			if (auto task = F4SE::GetTaskInterface()) {
				task->AddTask([]() { Camera::Install(); });
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
