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
	std::atomic<float>     g_waterDepth{ 0.0f };  // how deep you stand in water (game units), 0 = dry
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
			for (int i = 0; i < kCount; ++i) {
				g_count[i] = static_cast<int>(g_list[i].size());
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
			// the game's own water level where you stand (the character controller's copy is never filled in)
			const float d = player->GetRelevantWaterHeight() - player->data.location.z;
			g_waterDepth = std::isfinite(d) && d > 0.0f && d < 10000.0f ? d : 0.0f;
		}
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
