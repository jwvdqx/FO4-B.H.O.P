#pragma once
// B.H.O.P. - Half-Life footsteps, jump and landing sounds; muting the player's vanilla footsteps.
// Part of the plugin's single build unit: included in order by main.cpp (see main.cpp).

namespace
{
	// =====================================================================
	// sounds
	// =====================================================================
	namespace Sounds
	{
		std::atomic<float> g_loadMaxMs{ 0.0f };  // slowest footstep sound creation since the last debug line (ms)

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
				if (g_groups[set].find(set == 1 ? "pl_step" : "concrete") == g_groups[set].end()) {
					REX::WARN("{}: basic footstep files are missing; vanilla footsteps will NOT be muted for this set", kFolders[set]);
				}
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
			const auto loadStart = std::chrono::steady_clock::now();
			audio->GetSoundHandleByFile(handle, id, g_cfg.fileSoundFlags, static_cast<std::uint8_t>(g_cfg.filePriority));
			{
				// how long creating the sound took (debug log shows the slowest each second: hitch check)
				const float ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - loadStart).count();
				float       prev = g_loadMaxMs.load();
				while (ms > prev && !g_loadMaxMs.compare_exchange_weak(prev, ms)) {}
			}
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
			const bool hl1Tile = a_set == 1 && a_group == "pl_tile";
			if (it == g_groups[a_set].end() || (it->second.files.size() != 4 && !(hl1Tile && it->second.files.size() >= 4))) {
				return -1;
			}
			// PM_PlayStepSound: (0|1) + left*2 -> files 1/3 on one foot, 2/4 on the other
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

		// The Half-Life footsteps for this set are installed (otherwise we never mute the vanilla ones)
		bool HasSounds(int a_set)
		{
			if (a_set < 1 || a_set > 2) {
				return false;
			}
			const auto it = g_groups[a_set].find(a_set == 1 ? "pl_step" : "concrete");
			return it != g_groups[a_set].end() && !it->second.files.empty();
		}

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
			if (g_cfg.muteVanillaFootsteps && SoundSet() != 0 && g_ready && HasSounds(SoundSet()) && IsPlayerFootstep(a_event)) {
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
}
