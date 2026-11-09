#pragma once
// B.H.O.P. - First-person camera effects: bob, strafe roll, landing punch.
// Part of the plugin's single build unit: included in order by main.cpp (see main.cpp).

namespace
{
	// =====================================================================
	// camera: Half-Life view bob, strafe roll and landing punch
	// =====================================================================
	namespace Camera
	{
		constexpr float kHLToGame = 0.0254f * 69.99125f;  // 1 HL unit (inch) = 1.778 game units

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
			if (static bool logged = false; !logged && g_cfg.debugLog) {
				logged = true;
				REX::INFO("thread check: camera on thread {}", REX::W32::GetCurrentThreadId());
			}
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
}
