#pragma once
// B.H.O.P. - Movement state, input, air control, jumping, auto-jump, crouch-jump (Fallout side).
// Part of the plugin's single build unit: included in order by main.cpp (see main.cpp).

namespace
{
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
		int           detailLogs{ 0 };
		float         wishSpeed{ 0.0f };
		float         outSpeed{ 0.0f };
		float         actualSpeed{ 0.0f };
		float         reportedSpeed{ 0.0f };
		bool          measured{ false };
		float         measScale{ 1.0f };
		int           state{ -1 };
		std::uint32_t material{ 0 };
		float         water{ 0.0f };
		long long     lastPrint{ 0 };
		int           firstCalls{ 0 };
		int           landLogs{ 0 };
	} g_stats;

	hlmath::Vec2 SimVel() { return { g_sim.vx, g_sim.vy }; }
	void         SetSimVel(hlmath::Vec2 a_v)
	{
		g_sim.vx = a_v.x;
		g_sim.vy = a_v.y;
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
		bool  seen[2][4]{};     // learned from what Fallout actually did
		bool  warned[2][4]{};
	} g_input;

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
		REX::INFO("[1s] calls {} ground {} clips {} pushes {} | state {} | wish {:.0f} out {:.0f} actual {:.0f} (havok says {:.0f}{}) (HL u/s) | material {:08X} | water {:.0f} level {} | load {:.2f} PA {} swim {} sneak {} 1st {} | run speed data {:.0f} seen {:.0f} | slowest step sound {:.2f} ms | measure check {:.3f}",
			g_stats.calls, g_stats.groundCalls, g_stats.clips, g_stats.pushes, g_stats.state, g_stats.wishSpeed, g_stats.outSpeed, g_stats.actualSpeed, g_stats.reportedSpeed, g_stats.measured ? "" : ", not measured",
			g_stats.material, g_stats.water, g_waterLevel.load(), g_load.load(), g_inPowerArmor.load(), g_swimming.load(), g_sneaking.load(), g_firstPerson.load(),
			g_dataSpeed[g_inPowerArmor.load() ? 1 : 0][1].load(), g_input.learned[g_inPowerArmor.load() ? 1 : 0][1], Sounds::g_loadMaxMs.exchange(0.0f), g_stats.measScale);
		g_stats.calls = 0;
		g_stats.groundCalls = 0;
		g_stats.clips = 0;
		g_stats.pushes = 0;
	}

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
			if (g_cfg.baseSpeedCap > 0.0f) {
				speed = std::min(speed, g_cfg.baseSpeedCap);  // the cap is on the base speed: buffs, Power Armor and perks apply after it
			}
			speed *= g_speedMult.load();
			if (g_inPowerArmor.load()) {
				speed *= g_cfg.hlPowerArmorSpeedMult;
			}
		} else {
			// Fallout's own speed from its movement data; what we learned from watching Fallout is the backup,
			// and the check: if the two disagree a lot, the learned one wins (the data may not mean what we think).
			const int   pa = g_inPowerArmor.load() ? 1 : 0;
			const int   c = static_cast<int>(a_cls);
			float       base = g_input.learned[pa][c];
			const float data = g_dataSpeed[pa][c].load();
			if (data > 0.0f) {
				if (!g_input.seen[pa][c] || std::fabs(data - base) <= 0.25f * base) {
					base = data;
				} else if (!g_input.warned[pa][c]) {
					g_input.warned[pa][c] = true;
					REX::WARN("Fallout's movement data says {:.0f} but it actually moves you at {:.0f} (HL u/s, class {}): using what it does", data, base, c);
				}
			}
			if (g_cfg.baseSpeedCap > 0.0f) {
				base = std::min(base, g_cfg.baseSpeedCap);  // buffs and perks apply after the cap
			}
			speed = base * g_speedMult.load();
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
				if (g_input.heldTime > 2.0f) {
					g_input.seen[g_inPowerArmor.load() ? 1 : 0][static_cast<int>(w.cls)] = true;  // settled: good enough to check the data
				}
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
	// jumping: HL1 bunnyhop cap / HL2 forward boost (and its ABH quirk)
	// ---------------------------------------------------------------------
	// a_landingFall: fall speed still "in" the velocity when you jump right on landing (HL1 cap only)
	void OnJump(const Wish& a_wish, float a_landingFall)
	{
		const float maxSpeed = std::max(ClassSpeed(a_wish.cls == MoveClass::kSprint ? MoveClass::kSprint : MoveClass::kRun, false), 1.0f);
		if (g_cfg.mode == Mode::kHL1) {
			if (g_cfg.hl1BhopCap) {
				// HL1 checks 3D speed on the landing frame, when the fall speed is still in the velocity.
				// We count it when the jump comes right after a landing (the bunnyhop window).
				SetSimVel(hlmath::HL1BunnyCap(SimVel(), a_landingFall, maxSpeed));
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

	std::atomic<long long> g_autoJumpUntil{ 0 };  // a jump is wanted right after landing, until this time (ms)
	std::atomic<long long> g_jumpHeldMs{ 0 };     // last time the jump button was seen down (ms)
	std::atomic<long long> g_jumpPressedMs{ 0 };  // a fresh press of jump nothing has used yet (ms), 0 = none
	std::atomic_bool       g_jumpDown{ false };   // jump button is down (press seen, no release yet)
	std::atomic_bool       g_jumpHeldSeen{ false };  // this press has sent "still held" events

	// Is jump being held? Keyboards and gamepads send a "still held" event every frame; if a device only
	// sends the press and the release, the press counts until the release (5 s at most, in case a menu
	// swallowed the release).
	bool JumpHeld()
	{
		if (!g_jumpDown.load()) {
			return false;
		}
		const long long age = NowMs() - g_jumpHeldMs.load();
		return g_jumpHeldSeen.load() ? age < 250 : age < 5000;
	}

	// Jumps the game wouldn't do by itself. Fallout only jumps on a fresh press of jump while you stand on
	// the ground, so right after a landing we hand its jump handler a fresh press:
	//  - auto-jump: you're holding jump (keyboard or gamepad)
	//  - jump buffer: you pressed jump a moment before landing
	// While the button is held, the next "still held" event is turned into the press. If none is coming
	// (button already released, or a device that doesn't send them), the last press we saw is replayed.
	namespace AutoJump
	{
		using OnButton_t = void (*)(void*, const RE::ButtonEvent*);
		OnButton_t g_orig{ nullptr };
		int        g_logs{ 0 };
		int        g_deviceLogs{ 0 };
		// main thread only: the last press seen, to replay
		void*                 g_handler{ nullptr };
		alignas(16) std::byte g_last[sizeof(RE::ButtonEvent)]{};
		bool                  g_hasLast{ false };

		void Hooked(void* a_this, const RE::ButtonEvent* a_event)
		{
			if (a_event) {
				const bool      pressed = a_event->QPressed();
				const bool      fresh = pressed && a_event->QHeldDownSecs() <= 0.0f;
				const long long now = NowMs();
				g_jumpHeldMs = now;
				g_jumpDown = pressed;
				if (fresh) {
					g_jumpPressedMs = now;  // the movement code may use it: ledge jump, crouch-jump, jump buffer
					g_jumpHeldSeen = false;
					if (g_cfg.debugLog && g_deviceLogs < 6) {
						++g_deviceLogs;
						REX::INFO("jump pressed on device {} (0 keyboard, 1 mouse, 2 gamepad)", static_cast<int>(a_event->device.get()));
					}
				} else if (pressed) {
					g_jumpHeldSeen = true;
				}
				if (pressed) {
					std::memcpy(g_last, a_event, sizeof(RE::ButtonEvent));
					g_handler = a_this;
					g_hasLast = true;
				}
			}
			if (a_event && a_event->QPressed() && a_event->QHeldDownSecs() > 0.0f && NowMs() < g_autoJumpUntil.load()) {
				g_autoJumpUntil = 0;
				alignas(16) std::byte buf[sizeof(RE::ButtonEvent)];
				std::memcpy(buf, a_event, sizeof(RE::ButtonEvent));
				reinterpret_cast<RE::ButtonEvent*>(buf)->heldDownSecs = 0.0f;  // looks like a fresh press
				if (g_cfg.debugLog && g_logs++ < 10) {
					REX::INFO("auto-jump (held button, device {})", static_cast<int>(a_event->device.get()));
				}
				return g_orig(a_this, reinterpret_cast<const RE::ButtonEvent*>(buf));
			}
			g_orig(a_this, a_event);
		}

		// main thread, the frame after a landing that wants a jump
		void Fire()
		{
			const long long until = g_autoJumpUntil.load();
			const long long now = NowMs();
			if (until == 0 || now > until || !g_orig || !g_hasLast || !g_handler) {
				return;  // the held button already did it, or it's too late
			}
			if (g_jumpDown.load() && g_jumpHeldSeen.load() && now - g_jumpHeldMs.load() < 50) {
				return;  // "still held" events are coming in: the next one becomes the press
			}
			g_autoJumpUntil = 0;
			alignas(16) std::byte buf[sizeof(RE::ButtonEvent)];
			std::memcpy(buf, g_last, sizeof(RE::ButtonEvent));
			auto ev = reinterpret_cast<RE::ButtonEvent*>(buf);
			ev->heldDownSecs = 0.0f;
			ev->value = 1.0f;
			ev->next = nullptr;
			if (g_cfg.debugLog && g_logs++ < 10) {
				REX::INFO("auto-jump (replayed press, device {})", static_cast<int>(ev->device.get()));
			}
			g_orig(g_handler, ev);
		}

		// physics thread: you just landed and a jump is wanted
		void Request()
		{
			g_autoJumpUntil = NowMs() + 150;
			if (auto task = F4SE::GetTaskInterface()) {
				task->AddTask([]() { Fire(); });
			}
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

		// main thread only: the last press seen, to replay
		void*                 g_handler{ nullptr };
		alignas(16) std::byte g_last[sizeof(RE::ButtonEvent)]{};
		bool                  g_hasLast{ false };

		void Hooked(void* a_this, const RE::ButtonEvent* a_event)
		{
			if (a_event && a_event->QJustPressed()) {
				auto p = RE::PlayerCharacter::GetSingleton();
				g_crouchWasSneaking = p && p->IsSneaking();  // before Fallout toggles it
				g_crouchPressedMs = NowMs();                 // the movement code decides if it's a crouch-jump
				std::memcpy(g_last, a_event, sizeof(RE::ButtonEvent));
				g_handler = a_this;
				g_hasLast = true;
			}
			g_orig(a_this, a_event);  // Fallout still gets the key as normal
		}

		// Land sneaking after a crouch-jump (main thread). Fallout is asked the same way the sneak key asks it,
		// by replaying your press, so its own sneak on/off stays in step and the next press stands you up.
		// (Setting the sneaking state directly could leave you stuck sneaking.)
		void LandSneaking()
		{
			auto p = RE::PlayerCharacter::GetSingleton();
			if (!p || p->IsSneaking() || !g_orig || !g_hasLast || !g_handler) {
				return;  // already sneaking (Fallout took the press in the air), or nothing to replay
			}
			alignas(16) std::byte buf[sizeof(RE::ButtonEvent)];
			std::memcpy(buf, g_last, sizeof(RE::ButtonEvent));
			auto ev = reinterpret_cast<RE::ButtonEvent*>(buf);
			ev->heldDownSecs = 0.0f;
			ev->value = 1.0f;
			ev->next = nullptr;
			g_orig(g_handler, ev);
			if (g_cfg.debugLog) {
				REX::INFO("crouch-jump: landed, sneak asked through the sneak key (sneaking now: {})", p->IsSneaking());
			}
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
		float lastLandFall{ 0.0f };  // fall speed at the last landing (HL units/s)
		float sinceLanding{ 10.0f };  // seconds since the last landing
		hlmath::Vec2 lastOut{};  // what we told the game last step
		RE::hkVector4f prevPos{};  // controller position at the last step (Havok units)
		float          prevPosDt{ 0.0f };
		bool           hasPrevPos{ false };
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
		float takeoffHL{ 135.0f };     // Fallout's own jump takeoff speed (HL units/s), learned from jumps
		bool  hasLastOut{ false };
		float grace{ 0.0f };
		bool  airborneSim{ false };  // our horizontal velocity is in charge while airborne
		float sentVz{ 0.0f };        // vertical speed we handed the game last step (m/s)
		float measVz{ 0.0f };        // vertical speed you really moved at (m/s)
		bool  hasMeasVz{ false };
		float measScale{ 1.0f };     // measured speed / real speed, learned on flat ground (should stay ~1)
		bool  pushedLast{ false };
		// Half-Life jumping and gravity (iJumpPhysics 1/2)
		bool  vzOn{ false };         // we're in charge of your vertical speed
		float vz{ 0.0f };            // our vertical speed (m/s)
		bool  hlJump{ false };       // this jump used Half-Life physics (don't learn Fallout's jump from it)
		bool  ledgePending{ false }; // a ledge jump (jump pressed just after running off an edge) to start this step
	} g_air;

	float FalloutJumpHeightHL()
	{
		const float h = g_falloutJumpHL.load();
		return h > 0.0f ? h : g_air.jumpHeightHL;
	}

	// A fresh crouch press in the air that should become a crouch-jump? Returns the lift in metres (0 = no).
	float CrouchJumpLift(float a_vz, float a_gravity)
	{
		const long long now = NowMs();
		const bool      sneakPress = now - g_crouchPressedMs.load() < 120 && !g_crouchWasSneaking.load();
		// easier on a gamepad: jump pressed again while you're still going up
		const long long jp = g_jumpPressedMs.load();
		const bool      jumpPress = g_cfg.crouchJumpKey == 1 && jp != 0 && now - jp < 120 && g_air.jumpedThisAir && a_vz > 0.0f &&
		                            !g_inPowerArmor.load();
		if ((!sneakPress && !jumpPress) || g_air.crouchJumped || g_air.airTime <= 0.05f) {
			return 0.0f;
		}
		const float window = a_gravity * g_cfg.crouchJumpWindow;  // speed lost in the window after the peak
		if (a_vz <= -window) {
			return 0.0f;
		}
		const bool hl1 = g_cfg.mode == Mode::kHL1;
		float      liftHL = hl1 ? 18.0f : 36.0f;  // HL1: hull centre, HL2: feet move up the full difference
		if (g_cfg.crouchJumpHeight == 1 && g_cfg.jumpPhysics != 2) {
			liftHL *= g_air.jumpHeightHL / (hl1 ? 45.0f : 21.3f);  // same share of Fallout's own jump
		}
		g_air.crouchJumped = true;
		if (sneakPress) {
			g_landSneak = true;  // Half-Life keeps you ducked; a second press of jump doesn't sneak
			g_crouchPressedMs = 0;
		} else {
			g_jumpPressedMs = 0;
		}
		return liftHL * g_cfg.unitScale;
	}

	// Crouch-jump: Fallout can't shrink your body mid-air, so we give the upward speed that adds exactly
	// the height Half-Life's duck-jump gives. Physics still handles ceilings.
	// Returns the vertical speed (m/s) to hand to the game.
	float CrouchJumpVertical(float a_inVz, float a_actualVz, bool a_rawGround, float a_dt)
	{
		// measure gravity in the air from the game's own vertical speed
		if (!a_rawGround && g_air.hasPrevVz && a_dt > 0.001f && g_air.boostVz == 0.0f && !g_air.vzOn) {
			const float g = (g_air.prevVz - a_inVz) / a_dt;
			if (g > 3.0f && g < 40.0f) {
				g_air.gravity += (g - g_air.gravity) * 0.1f;
			}
		}
		g_air.prevVz = a_inVz;
		g_air.hasPrevVz = !a_rawGround;

		float outVz = a_inVz;
		if (a_rawGround || g_cfg.mode == Mode::kVanilla) {
			g_air.ledgePending = false;
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
				if (a_actualVz < g_air.lastOutVz - g_air.gravity * a_dt - 1.0f || a_inVz - g_air.lastInVz > 2.0f) {
					g_air.boostVz = 0.0f;  // hit a ceiling, or Fallout started a jump/launch of its own: stop pushing up
				} else {
					outVz = a_inVz + g_air.boostVz;
				}
			} else {
				g_air.boostVz = 0.0f;  // Fallout already carries it
			}
		}

		const float vz = a_actualVz;  // m/s, + is up
		if (g_air.ledgePending) {
			// ledge jump: Fallout won't jump once you're off the edge, so we give its jump's upward speed
			g_air.ledgePending = false;
			const float newVz = hlcore::JumpSpeed(g_air.gravity, FalloutJumpHeightHL() * g_cfg.unitScale);
			g_air.boostVz = newVz - a_inVz;
			g_air.boostMode = 0;
			outVz = newVz;
			if (g_cfg.debugLog) {
				REX::INFO("ledge jump: vertical {:.2f} -> {:.2f} m/s", vz, newVz);
			}
		} else if (!g_cfg.crouchJump) {
			// no crouch-jump
		} else if (const float lift = CrouchJumpLift(vz, g_air.gravity); lift > 0.0f) {  // a fresh crouch press in the air?
			const float newVz = hlmath::CrouchLiftSpeed(vz, g_air.gravity, lift);
			g_air.boostVz = newVz - a_inVz;
			g_air.boostMode = 0;
			outVz = newVz;
			if (g_cfg.debugLog) {
				REX::INFO("crouch-jump: +{:.0f} HL units, vertical {:.2f} -> {:.2f} m/s (gravity {:.2f})", lift / g_cfg.unitScale, vz, newVz, g_air.gravity);
			}
		}
		g_air.lastOutVz = outVz;
		g_air.lastInVz = a_inVz;
		return outVz;
	}

	// Half-Life jumping and gravity (iJumpPhysics 1 or 2). While you're in the air we hand the game our own
	// vertical speed every step, with Half-Life's gravity (sv_gravity: HL1 800, HL2 600 units/s^2).
	// Standing on the ground, Fallout is in charge as usual. Returns the vertical speed for this step (m/s).
	float HLVertical(float a_inVz, bool a_rawGround, bool a_onGround, bool a_jumpedNow, float a_dt)
	{
		if (a_rawGround) {
			g_air.vzOn = false;
			return a_inVz;
		}
		const bool  hl1 = g_cfg.mode == Mode::kHL1;
		const float g = (hl1 ? 800.0f : 600.0f) * g_cfg.unitScale;  // m/s^2
		if (a_jumpedNow) {
			// exact: HL1 PM_Jump sqrt(2*800*45) = 268 u/s (45 units high); HL2 CheckJumpButton sqrt(2*600*21) = 159 u/s (21 high)
			const float heightHL = g_cfg.jumpPhysics == 2 ? (hl1 ? 45.0f : 21.0f) : FalloutJumpHeightHL();
			g_air.vz = hlcore::JumpSpeed(g, heightHL * g_cfg.unitScale);
			g_air.vzOn = true;
			g_air.hlJump = true;
			if (g_cfg.debugLog) {
				REX::INFO("HL jump: {:.0f} HL units high, takeoff {:.0f} u/s, gravity {:.0f} u/s2", heightHL, g_air.vz / g_cfg.unitScale, g / g_cfg.unitScale);
			}
		} else if (!g_air.vzOn) {
			if (a_onGround) {
				return a_inVz;  // a short stair hop: Fallout keeps it
			}
			g_air.vz = g_air.hasMeasVz ? g_air.measVz : a_inVz;  // walked off a ledge: carry on from your real speed
			g_air.vzOn = true;
		} else {
			// a ceiling (or anything) stopping you going up: follow what really happened
			if (g_air.hasMeasVz && g_air.sentVz > 0.5f && g_air.measVz < g_air.sentVz * 0.8f - 0.3f) {
				g_air.vz = std::min(g_air.vz, g_air.measVz);
			}
			// Something pushing you up (explosion, script, a Power Armor jetpack): Fallout's own vertical speed
			// rose more than its gravity allows. Fallout may be carrying on from its own speed or from the one we
			// handed it, so compare with whichever fits better; anything clearly above that is added to ours.
			const float dFallout = a_inVz - (g_air.lastInVz - g_air.gravity * a_dt);
			const float dOurs = a_inVz - (g_air.sentVz - g_air.gravity * a_dt);
			const float kick = std::fabs(dFallout) < std::fabs(dOurs) ? dFallout : dOurs;
			// (in the first moment of a jump only a big kick counts, in case Fallout builds its jump up over a few steps)
			const float least = g_air.airTime > 0.1f ? 6.0f * a_dt + 0.05f : 3.0f;
			if (kick > least && a_inVz > g_air.lastInVz + 0.02f) {
				g_air.vz += kick;
			}
		}
		if (g_cfg.crouchJump) {
			if (const float lift = CrouchJumpLift(g_air.vz, g); lift > 0.0f) {
				const float before = g_air.vz;
				g_air.vz = hlmath::CrouchLiftSpeed(before, g, lift);
				if (g_cfg.debugLog) {
					REX::INFO("crouch-jump (HL gravity): +{:.0f} HL units, vertical {:.2f} -> {:.2f} m/s", lift / g_cfg.unitScale, before, g_air.vz);
				}
			}
		}
		const float maxV = 2000.0f * g_cfg.unitScale;  // sv_maxvelocity
		float       during = hlcore::GravityStep(g_air.vz, g, a_dt);
		g_air.vz = std::clamp(g_air.vz, -maxV, maxV);
		during = std::clamp(during, -maxV, maxV);
		g_air.lastInVz = a_inVz;
		return during;
	}
}
