#pragma once
// B.H.O.P. - Player state read on the main thread (Power Armor, sneaking, perks, carry weight, edge probe).
// Part of the plugin's single build unit: included in order by main.cpp (see main.cpp).

namespace
{
	// =====================================================================
	// values read on the main thread
	// =====================================================================
	std::atomic<float>     g_load{ 0.0f };
	std::atomic_bool       g_inPowerArmor{ false };
	std::atomic<float>     g_waterDepth{ 0.0f };          // how deep you stand in water (game units), 0 = dry (set by the movement code)
	std::atomic<int>       g_waterLevel{ 0 };             // Half-Life's water level: 0 dry, 1 feet, 2 waist, 3 eyes under
	std::atomic<float>     g_waterSurfaceZ{ -1.0e30f };   // the water's surface where you are (game units), from the game
	std::atomic<float>     g_health{ -1.0f };             // your health (to hear drowning damage)
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
		std::atomic<int>          g_count[kCount]{};  // how many ranks each perk has (published for the physics thread)
		enum : int { kMovingTarget, kStrongBack, kAdamantium, kAquaboy, kSneak, kActionBoy };

		std::string g_resolvedNames;  // the names g_list was built from (main thread only)
		std::string g_title[kCount];  // each perk's name as your game shows it (main thread only)
		int         g_shown[kCount]{};     // ranks already announced (main thread only)
		bool        g_baseline{ false };   // false = first look after loading a save: nothing announced yet

		// The perks' rank 1 in Fallout4.esm. Form IDs stay the same whatever language the game is in and
		// whatever a mod changed about the perk (name, ranks, requirements), so these are found first;
		// later ranks come from each perk's own "next rank" link. 0 = found by name only.
		constexpr std::uint32_t kFormIDs[kCount]{ 0x0004DDEE, 0x0004B24E, 0x0004C92D, 0, 0, 0x0004D869 };
		constexpr const char*   kEditorIDs[kCount]{ "MovingTarget01", "StrongBack01", "AdamantiumSkeleton01", "Aquaboy01", "Sneak01", "ActionBoy01" };
		constexpr const char*   kEnglish[kCount]{ "Moving Target", "Strong Back", "Adamantium Skeleton", "Aquaboy", "Sneak", "Action Boy" };

		void AddChain(int a_idx, RE::BGSPerk* a_perk)
		{
			auto& l = g_list[a_idx];
			for (int guard = 0; a_perk && guard < 12; ++guard, a_perk = a_perk->nextPerk) {
				if (std::find(l.begin(), l.end(), a_perk) != l.end()) {
					return;  // already have it (and everything after it)
				}
				l.push_back(a_perk);
			}
		}

		// Main thread only. g_list is only ever touched on the main thread; the physics thread reads
		// the published rank counts (g_count / g_rank) instead.
		void Resolve()
		{
			std::string names;
			for (auto& n : g_cfg.perkNames) {
				names += n + ";";
			}
			if (names == g_resolvedNames) {
				return;  // nothing changed: don't rebuild every time the pause menu closes
			}
			g_resolvedNames = names;
			for (auto& l : g_list) {
				l.clear();
			}
			auto dh = RE::TESDataHandler::GetSingleton();
			if (!dh) {
				return;
			}
			int byId[kCount]{};
			// 1) by form ID / editor ID (any language, any perk mod that edits the vanilla perks)
			for (int i = 0; i < kCount; ++i) {
				if (kFormIDs[i] != 0) {
					AddChain(i, RE::TESForm::GetFormByID<RE::BGSPerk>(kFormIDs[i]));
				}
				AddChain(i, RE::TESForm::GetFormByEditorID<RE::BGSPerk>(kEditorIDs[i]));
				// your own entries in the settings file: a form ID like 0x0004D869 works as well as a name
				std::string_view list = g_cfg.perkNames[i];
				while (!list.empty()) {
					const auto        bar = list.find('|');
					const std::string item(list.substr(0, bar));
					if (item.rfind("0x", 0) == 0) {
						try {
							AddChain(i, RE::TESForm::GetFormByID<RE::BGSPerk>(static_cast<std::uint32_t>(std::stoul(item, nullptr, 16))));
						} catch (...) {
						}
					}
					if (bar == std::string_view::npos) {
						break;
					}
					list.remove_prefix(bar + 1);
				}
				byId[i] = static_cast<int>(g_list[i].size());
			}
			// 2) by name (ranks a mod added as separate perks, and perks we have no ID for)
			for (auto perk : dh->GetFormArray<RE::BGSPerk>()) {
				if (!perk || !perk->data.playable) {
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
					AddChain(idx, perk);
				}
			}
			for (int i = 0; i < kCount; ++i) {
				g_count[i] = static_cast<int>(g_list[i].size());
				g_title[i] = kEnglish[i];
				if (!g_list[i].empty()) {
					if (const auto t = std::string(RE::TESFullName::GetFullName(*g_list[i].front())); !t.empty()) {
						g_title[i] = t;
					}
				}
				REX::INFO("Perk {}: {} rank(s) found ({} by ID, {} by name){}", kEnglish[i], g_list[i].size(), byId[i],
					static_cast<int>(g_list[i].size()) - byId[i], g_list[i].empty() ? " - NOT FOUND, its bonus stays off" : "");
			}
		}

		// What a rank gives you, for the on-screen note.
		std::string Effect(int a_idx, int a_rank, bool a_max)
		{
			const bool unlock = g_cfg.perkMode == 2;
			switch (a_idx) {
			case kMovingTarget:
				if (unlock && a_rank == 1) {
					return "bunnyhopping unlocked";
				}
				if (unlock && a_rank == 3) {
					return "back-hopping unlocked";
				}
				return "faster sprint";
			case kStrongBack: return "weight slows you less";
			case kAdamantium: return "softer landings";
			case kAquaboy: return "water no longer drags you";
			case kSneak: return "quieter steps while sneaking";
			case kActionBoy: return a_max && g_cfg.autoJump == 1 ? "hold jump to bunnyhop" : "more time to bunnyhop";
			default: return {};
			}
		}

		void Notify(const std::string& a_text)
		{
			RE::SendHUDMessage::ShowHUDMessage(a_text.c_str(), nullptr, true, true);
			REX::INFO("on screen: {}", a_text);
		}

		void Update(RE::PlayerCharacter* a_player)
		{
			int ranks[kCount]{};
			for (int i = 0; i < kCount; ++i) {
				int r = 0;
				for (auto p : g_list[i]) {
					if (a_player->GetPerkRank(p) > 0) {
						++r;
					}
				}
				g_rank[i] = r;
				ranks[i] = r;
			}
			// on-screen notes, so you can see the perks are picked up: one line after loading, and one
			// whenever you gain a rank
			if (g_cfg.perkMode == 0 || g_cfg.perkMessages == 0 || g_cfg.mode == Mode::kVanilla) {
				g_baseline = false;
				return;
			}
			if (!g_baseline) {
				g_baseline = true;
				std::string line;
				for (int i = 0; i < kCount; ++i) {
					g_shown[i] = ranks[i];
					if (ranks[i] > 0) {
						line += (line.empty() ? "" : ", ") + g_title[i] + " " + std::to_string(ranks[i]) + "/" + std::to_string(g_list[i].size());
					}
				}
				if (g_cfg.perkMessages == 2 && !line.empty()) {
					Notify("B.H.O.P. perks: " + line);
				}
				return;
			}
			for (int i = 0; i < kCount; ++i) {
				if (ranks[i] > g_shown[i]) {
					const bool max = ranks[i] >= static_cast<int>(g_list[i].size());
					Notify("B.H.O.P.: " + g_title[i] + " " + std::to_string(ranks[i]) + " - " + Effect(i, ranks[i], max));
				}
				g_shown[i] = ranks[i];
			}
		}

		int Rank(int a_idx) { return g_cfg.perkMode == 0 ? 0 : g_rank[a_idx].load(); }
		bool MaxRank(int a_idx)
		{
			const int count = g_count[a_idx].load();
			return count > 0 && Rank(a_idx) >= count;
		}
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
		constexpr float kHL = 0.0254f * 69.99125f;  // 1 HL unit (inch) = 1.778 game units
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
			static_cast<void>(cell->Pick(pick));
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

	// ---------------------------------------------------------------------
	// Wall probe (experiment): when the movement code finds a wall, it asks for the real surface to be
	// checked. Rays go from your body's edge toward the wall at knee, waist and chest height; the
	// surface's own direction then replaces the guess (when the two roughly agree).
	// Runs on the main thread, like the edge probe.
	// ---------------------------------------------------------------------
	namespace WallProbe
	{
		std::atomic<float>     g_reqX{ 0.0f }, g_reqY{ 0.0f };
		std::atomic_bool       g_queued{ false };
		std::atomic<float>     g_nx{ 0.0f }, g_ny{ 0.0f };  // into the wall
		std::atomic<long long> g_foundMs{ 0 };
		std::atomic<float>     g_radiusGame{ 22.0f };      // your body's radius (game units), from the controller
		int                    g_logs{ 0 };

		void Probe()
		{
			g_queued = false;
			auto player = RE::PlayerCharacter::GetSingleton();
			auto cell = player ? player->GetParentCell() : nullptr;
			const float dx = g_reqX.load(), dy = g_reqY.load();
			if (!cell || std::hypot(dx, dy) < 0.5f) {
				return;
			}
			constexpr float kHL = 0.0254f * 69.99125f;  // HL unit in game units
			const auto&     pos = player->data.location;
			const float     startOff = g_radiusGame.load() + 1.0f;  // start just outside your body
			const float     reach = 24.0f * kHL;
			float           best = 2.0f, nx = 0.0f, ny = 0.0f;
			for (const float h : { 20.0f, 36.0f, 54.0f }) {
				const RE::NiPoint3 start(pos.x + dx * startOff, pos.y + dy * startOff, pos.z + h * kHL);
				const RE::NiPoint3 end(start.x + dx * reach, start.y + dy * reach, start.z);
				RE::bhkPickData pick;
				pick.SetStartEnd(start, end);
				static_cast<void>(cell->Pick(pick));
				if (!pick.HasHit()) {
					continue;
				}
				const auto& n = pick.result.normal;  // points out of the surface, toward you
				const float frac = pick.GetHitFraction();
				const float hlen = std::hypot(n.x, n.y);
				if (std::fabs(n.z) < 0.7f && hlen > 0.3f && frac < best) {
					best = frac;
					nx = -n.x / hlen;
					ny = -n.y / hlen;
				}
			}
			if (best <= 1.0f) {
				g_nx = nx;
				g_ny = ny;
				g_foundMs = NowMs();
			}
			if (g_cfg.debugLog && g_logs < 30) {
				++g_logs;
				REX::INFO("wall probe: asked ({:.2f},{:.2f}) found {} ({:.2f},{:.2f})", dx, dy, best <= 1.0f, nx, ny);
			}
		}

		// physics thread: check the wall in this direction (into the wall)
		void Request(hlmath::Vec2 a_dir)
		{
			g_reqX = a_dir.x;
			g_reqY = a_dir.y;
			if (!g_queued.exchange(true)) {
				if (auto task = F4SE::GetTaskInterface()) {
					task->AddTask([]() { Probe(); });
				} else {
					g_queued = false;
				}
			}
		}

		// the last surface direction found, if it's recent
		hlmath::Vec2 Recent()
		{
			if (NowMs() - g_foundMs.load() > 200) {
				return {};
			}
			return { g_nx.load(), g_ny.load() };
		}
	}

	// ---------------------------------------------------------------------
	// Fallout's own top speeds, read from the movement type the game is using for you right now
	// (normal, sneaking, Power Armor). [Power Armor][walk, run, sprint, sneak], HL units/s, before
	// SpeedMult. 0 = not known yet.
	// ---------------------------------------------------------------------
	std::atomic<float> g_dataSpeed[2][4]{};
	std::atomic<float> g_falloutJumpHL{ 0.0f };  // Fallout's jump height (fJumpHeightMin) in HL units, 0 = unknown
	int                g_speedLogs{ 0 };

	void ReadMoveSpeeds(RE::PlayerCharacter* a_player)
	{
		if (!a_player->currentProcess || !a_player->currentProcess->high) {
			return;
		}
		const auto&   sp = a_player->currentProcess->high->currentMovementType.defaultData.speeds;
		constexpr int kFwd = static_cast<int>(RE::Movement::SPEED_DIRECTION::kForward);
		constexpr float kToHL = 1.0f / (0.0254f * 69.99125f);  // game units -> HL units
		const int     pa = g_inPowerArmor.load() ? 1 : 0;
		auto          set = [&](int a_cls, float a_game) {
			const float hl = a_game * kToHL;
			if (std::isfinite(hl) && hl > 5.0f && hl < 2000.0f) {
				g_dataSpeed[pa][a_cls] = hl;
			}
		};
		// speeds[direction][standing, walk, run, sprint]
		if (a_player->IsSneaking()) {
			auto controls = RE::PlayerControls::GetSingleton();
			const bool running = controls ? controls->data.running : true;
			set(3, sp[kFwd][running ? 2 : 1]);
		} else {
			set(0, sp[kFwd][1]);
			set(1, sp[kFwd][2]);
			set(2, sp[kFwd][3]);
		}
		if (auto gs = RE::GameSettingCollection::GetSingleton()) {
			if (auto jump = gs->GetSetting("fJumpHeightMin"); jump) {
				const float h = jump->GetFloat() * kToHL;
				if (std::isfinite(h) && h > 5.0f && h < 500.0f) {
					g_falloutJumpHL = h;
				}
			}
		}
		if (g_cfg.debugLog && g_speedLogs < 6) {
			++g_speedLogs;
			REX::INFO("movement type '{}': forward walk {:.0f} run {:.0f} sprint {:.0f} game units/s (sneaking {}, PA {})",
				a_player->currentProcess->high->currentMovementType.typeName.c_str(), sp[kFwd][1], sp[kFwd][2], sp[kFwd][3],
				a_player->IsSneaking(), pa == 1);
		}
	}

	// ---------------------------------------------------------------------
	// Third person: Fallout plays your walk/run animation for the speed IT wants you to move at. With
	// Half-Life speeds, momentum or slippery ground you really move faster or slower, and the feet slide.
	// The animation is sped up or slowed down by the same ratio through the game's own animation speed
	// value (AnimationMult), as a temporary change that is taken off before every save, in first
	// person and when the option or the mod is off.
	// ---------------------------------------------------------------------
	namespace AnimSpeed
	{
		std::atomic<float> g_wanted{ 1.0f };  // real speed / Fallout's speed (written by the movement code)
		float              g_applied{ 0.0f };  // what we've added to AnimationMult (main thread only)
		float              g_smooth{ 1.0f };
		int                g_logs{ 0 };

		void Clear()
		{
			auto player = RE::PlayerCharacter::GetSingleton();
			auto avs = RE::ActorValue::GetSingleton();
			if (g_applied != 0.0f && player && avs && avs->animationMult) {
				static_cast<RE::ActorValueOwner*>(player)->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, *avs->animationMult, -g_applied);
			}
			g_applied = 0.0f;
			g_smooth = 1.0f;
		}

		void Update(RE::PlayerCharacter* a_player)
		{
			auto avs = RE::ActorValue::GetSingleton();
			if (!g_cfg.animSpeed || g_cfg.mode == Mode::kVanilla || g_firstPerson.load() || !avs || !avs->animationMult) {
				Clear();
				return;
			}
			auto        owner = static_cast<RE::ActorValueOwner*>(a_player);
			const float base = owner->GetActorValue(*avs->animationMult) - g_applied;  // the game's own value, without ours
			if (!std::isfinite(base) || base <= 0.0f) {
				Clear();
				return;
			}
			float target = std::clamp(g_wanted.load(), 0.7f, 1.6f);
			if (std::fabs(target - 1.0f) < 0.06f) {
				target = 1.0f;  // close enough: leave Fallout's animation alone
			}
			g_smooth += (target - g_smooth) * 0.5f;
			const float delta = std::fabs(g_smooth - 1.0f) < 0.02f ? 0.0f : base * (g_smooth - 1.0f);
			if (std::fabs(delta - g_applied) > 0.02f * base || (delta == 0.0f && g_applied != 0.0f)) {
				owner->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, *avs->animationMult, delta - g_applied);
				g_applied = delta;
				if (g_cfg.debugLog && g_logs < 20) {
					++g_logs;
					REX::INFO("third person animation speed x{:.2f} (the game's value {:.2f})", g_smooth, base);
				}
			}
		}
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
		{
			// the game's own water surface where you are (the character controller's copy is never filled in);
			// the movement code turns it into depth every step, from your live position
			const float wh = player->GetRelevantWaterHeight();
			g_waterSurfaceZ = std::isfinite(wh) && wh > -1.0e6f && wh < 1.0e6f ? wh : -1.0e30f;
		}
		g_sneaking = player->IsSneaking();
		if (auto avs = RE::ActorValue::GetSingleton()) {
			auto owner = static_cast<RE::ActorValueOwner*>(player);
			if (avs->agility) {
				g_agility = owner->GetActorValue(*avs->agility);
			}
			if (avs->health) {
				g_health = owner->GetActorValue(*avs->health);
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
		AnimSpeed::Update(player);
		ProbeEdge(player);
		ReadMoveSpeeds(player);

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
}
