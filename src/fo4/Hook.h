#pragma once
// B.H.O.P. - The velocity hook: where Fallout hands us your movement every physics step.
// Part of the plugin's single build unit: included in order by main.cpp (see main.cpp).

namespace
{
	void HookedSetLinearVelocity(RE::bhkCharacterController* a_this, const RE::hkVector4f& a_vel)
	{
		if (!a_this || a_this != PlayerController()) {
			return g_origSetVel(a_this, a_vel);
		}

		// Thread model: this runs on a physics thread. Settings are only ever replaced on the main thread
		// while holding g_simLock, so every setting read here happens under that same lock.
		std::unique_lock lock(g_simLock);
		if (g_cfg.mode == Mode::kVanilla && SoundSet() == 0) {
			lock.unlock();
			return g_origSetVel(a_this, a_vel);
		}

		RequestCacheUpdate();

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

		// What Havok reports as your velocity may just be the velocity we asked for (walls not taken into
		// account), so measure what really happened: how far you moved since the last step.
		RE::hkVector4f reported;
		a_this->GetLinearVelocityImpl(reported);
		RE::hkVector4f actual = reported;
		{
			RE::hkVector4f pos;
			a_this->GetPositionImpl(pos, false);
			bool measured = false;
			g_air.hasMeasVz = false;
			if (g_air.hasPrevPos && g_air.prevPosDt > 0.0005f) {
				const float mx = (pos.x - g_air.prevPos.x) / g_air.prevPosDt;
				const float my = (pos.y - g_air.prevPos.y) / g_air.prevPosDt;
				const float mz = (pos.z - g_air.prevPos.z) / g_air.prevPosDt;
				g_air.hasMeasVz = std::isfinite(mz) && std::fabs(mz) / g_cfg.unitScale < 3000.0f;
				g_air.measVz = g_air.hasMeasVz ? mz : 0.0f;
				// anything faster than ~75 m/s is a teleport (doors, fast travel, scripts), not movement
				if (std::isfinite(mx) && std::isfinite(my) && std::hypot(mx, my) / g_cfg.unitScale < 3000.0f) {
					actual.x = mx;
					actual.y = my;
					measured = true;
				}
			}
			g_air.prevPos = pos;
			g_air.prevPosDt = dt;
			g_air.hasPrevPos = true;
			g_stats.measured = measured;
			g_stats.reportedSpeed = std::hypot(reported.x, reported.y) / g_cfg.unitScale;
		}

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
				REX::INFO("thread check: movement on thread {}", REX::W32::GetCurrentThreadId());
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
				constexpr float kToHL = 1.0f / (0.0254f * 69.99125f);  // game units -> HL units (1 game unit = 0.5625 in)
				if (!g_air.crouchJumped && !g_air.hlJump && rise > 10.0f && rise < 200.0f) {
					const float hl = rise * kToHL;
					g_air.jumpHeightHL += (hl - g_air.jumpHeightHL) * 0.3f;
					const float takeoff = g_air.jumpTakeoffVz / g_cfg.unitScale;
					if (takeoff > 30.0f && takeoff < 600.0f) {
						g_air.takeoffHL += (takeoff - g_air.takeoffHL) * 0.3f;
					}
				}
				if (g_cfg.debugLog) {
					REX::INFO("jump: takeoff {:.2f} m/s ({:.0f} HL u/s), peak +{:.1f} game units ({:.1f} HL units), air time {:.2f} s, gravity {:.2f} m/s2{}",
						g_air.jumpTakeoffVz, g_air.jumpTakeoffVz / g_cfg.unitScale, rise, rise * kToHL, g_air.jumpTime,
						g_air.gravity, g_air.crouchJumped ? " (crouch-jump)" : g_air.hlJump ? " (Half-Life jump)" : "");
				}
			}
		}

		// one landing / takeoff event for sounds AND camera (each works without the other)
		if (!rawGround) {
			g_air.maxFall = std::max(g_air.maxFall, -actual.z / g_cfg.unitScale);
		}
		const bool landEvent = !g_air.prevGround && onGround;
		const float fallSpeed = g_air.maxFall;
		if (landEvent) {
			g_air.lastLandFall = fallSpeed;
			g_air.sinceLanding = 0.0f;
		} else {
			g_air.sinceLanding = std::min(g_air.sinceLanding + dt, 10.0f);
		}
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
		if (rawGround && !g_air.measuring) {
			g_air.hlJump = false;
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

		// vertical: optional Half-Life jumping and gravity (we're in charge while you're in the air)
		float hlVz = a_vel.z;
		if (!skip && g_cfg.jumpPhysics != 0) {
			hlVz = HLVertical(a_vel.z, rawGround, onGround, jumpedNow, dt);
		} else {
			g_air.vzOn = false;
		}

		const bool airLocked = Perks::Unlock() && g_agility.load() < 5.0f;  // unlock mode: Agility 5 for air control
		const bool airSim = !onGround && g_cfg.airControl && !airLocked;
		if (skip || (!onGround && !airSim)) {
			g_air.hasLastOut = false;
			ResetSim(a_vel);
			g_air.airborneSim = false;
			g_stats.outSpeed = std::hypot(a_vel.x, a_vel.y) / g_cfg.unitScale;
			RE::hkVector4f pass = a_vel;
			if (g_air.vzOn) {
				pass.z = hlVz;
			}
			g_air.sentVz = pass.z;
			PrintStats();
			return g_origSetVel(a_this, pass);
		}

		// ---- walls, slopes and pushes: compare what really happened with what we asked for last step
		const hlmath::Vec2 act{ actual.x / g_cfg.unitScale, actual.y / g_cfg.unitScale };
		const hlmath::Vec2 asked = g_air.lastOut;
		const float        askedVzHL = g_air.sentVz / g_cfg.unitScale;
		const hlmath::Vec2 simBefore = SimVel();
		hlcore::Contact    contact = hlcore::Contact::kNone;
		bool               hasGround = false;
		float              gnx = 0.0f, gny = 0.0f, gnz = 1.0f;
		{
			const auto& sn = a_this->supportNorm;  // the floor's direction under you, from the game
			const float l = std::sqrt(sn.x * sn.x + sn.y * sn.y + sn.z * sn.z);
			if (rawGround && std::isfinite(l) && l > 0.9f && l < 1.1f) {
				hasGround = true;
				gnx = sn.x / l;
				gny = sn.y / l;
				gnz = sn.z / l;
			}
			const float r = a_this->radius * 69.99125f;  // your body's radius, Havok -> game units
			WallProbe::g_radiusGame = std::isfinite(r) && r > 5.0f && r < 80.0f ? r : 22.0f;
		}
		if (g_air.hasLastOut) {
			hlcore::CollideInput ci;
			ci.asked = asked;
			ci.askedVz = g_air.sentVz / g_cfg.unitScale;
			ci.actual = act;
			ci.sim = simBefore;
			ci.hasGround = hasGround;
			ci.gnx = gnx;
			ci.gny = gny;
			ci.gnz = gnz;
			ci.wallClip = g_cfg.wallClip;
			ci.pushes = g_cfg.pushes;
			ci.pushThreshold = g_cfg.pushThreshold;
			auto r = hlcore::Collide(ci);
			contact = r.kind;
			bool probed = false;
			if (r.kind == hlcore::Contact::kWall && g_cfg.wallProbe) {
				// the guess says THAT there's a wall; the probe of the real surface says exactly how it faces
				const hlmath::Vec2 n = hlcore::RefineNormal(r.normal, WallProbe::Recent());
				if (n.x != r.normal.x || n.y != r.normal.y) {
					r.sim = hlmath::ClipVelocity(simBefore, n);
					r.normal = n;
					probed = true;
				}
				WallProbe::Request(r.normal);
			}
			SetSimVel(r.sim);
			const bool log = g_cfg.debugLog && g_stats.detailLogs < 60;
			switch (r.kind) {
			case hlcore::Contact::kPush:
				g_stats.pushes++;
				if (log) {
					++g_stats.detailLogs;
					REX::INFO("push: expected ({:.0f},{:.0f}) actual ({:.0f},{:.0f}) -> sim ({:.0f},{:.0f})", r.expected.x, r.expected.y, act.x, act.y, r.sim.x, r.sim.y);
				}
				break;
			case hlcore::Contact::kWall:
				g_stats.clips++;
				if (log) {
					++g_stats.detailLogs;
					REX::INFO("clip: asked ({:.0f},{:.0f}) expected ({:.0f},{:.0f}){} actual ({:.0f},{:.0f}) havok says ({:.0f},{:.0f}) normal ({:.2f},{:.2f}){} sim ({:.0f},{:.0f}) -> ({:.0f},{:.0f})",
						asked.x, asked.y, r.expected.x, r.expected.y, r.slope ? " slope" : "", act.x, act.y, reported.x / g_cfg.unitScale, reported.y / g_cfg.unitScale,
						r.normal.x, r.normal.y, probed ? " probed" : "", simBefore.x, simBefore.y, r.sim.x, r.sim.y);
				}
				break;
			case hlcore::Contact::kKept:
				if (log) {
					++g_stats.detailLogs;
					REX::INFO("no wall (direction kept): expected ({:.0f},{:.0f}){} actual ({:.0f},{:.0f}) floor ({:.2f},{:.2f},{:.2f})",
						r.expected.x, r.expected.y, r.slope ? " slope" : "", act.x, act.y, gnx, gny, gnz);
				}
				break;
			default:
				break;
			}
		}

		float accelMult, frictionMult;
		WeightMults(accelMult, frictionMult);

		hlcore::Params p;
		p.friction = g_active.friction;
		p.stopSpeed = g_active.stopSpeed;
		p.accelerate = g_active.accelerate;
		p.airAccelerate = g_cfg.airAccelerate;
		p.airCap = g_cfg.airCap;
		p.strafeAssist = g_cfg.strafeAssist;
		p.tick = g_active.tick;

		hlcore::StepInput in;
		in.ground = onGround;
		in.wishDir = { wish.dirX, wish.dirY };
		in.wishSpeed = wish.speed;
		in.stickX = wish.inX;
		in.yawRate = g_input.yawRate;

		if (!onGround) {
			// ---- in the air: Half-Life air control, momentum kept
			if (!g_air.airborneSim) {
				g_air.airborneSim = true;  // just left the ground: carry our ground velocity into the air
				g_sim.pvx = g_sim.vx;
				g_sim.pvy = g_sim.vy;
			}
			if (jumped) {
				// HL1 checks 3D speed on the landing frame, when the fall speed is still in the velocity.
				// We count it when the jump comes right after a landing (the bunnyhop window).
				const float window = g_cfg.bhopGrace + 0.025f * static_cast<float>(Perks::Bonus() ? Perks::Rank(Perks::kActionBoy) : 0) + 0.02f;
				OnJump(wish, g_air.sinceLanding <= window ? g_air.lastLandFall : 0.0f);
				g_sim.pvx = g_sim.vx;  // the jump boost is instant, don't blend it in
				g_sim.pvy = g_sim.vy;
			}
			float airMult = 1.0f;
			if (Perks::Bonus()) {
				airMult *= std::clamp(1.0f + (g_agility.load() - 5.0f) * 0.04f, 0.8f, 1.25f);  // Agility
			}
			// HL2 (CGameMovement::CategorizePosition): while you rise slower than 140 units/s, the player's
			// surface friction is 0.25, which also scales air acceleration. Rising fast (start of a jump) or
			// falling: 1.0. HL1 keeps 1.0 in the air.
			float airSurface = 1.0f;
			if (g_cfg.mode == Mode::kHL2 && g_cfg.hl2AirFriction != 0) {
				const float vzHL = (g_air.vzOn ? g_air.vz : actual.z) / g_cfg.unitScale;
				// exact 140, or scaled to Fallout's takeoff speed (160 = HL2's); with exact HL jumping it's HL's own
				const bool  exact = g_cfg.hl2AirFriction == 1 || g_cfg.jumpPhysics == 2;
				const float threshold = exact ? 140.0f : 140.0f * (g_air.takeoffHL / 160.0f);
				airSurface = hlmath::HL2AirSurface(vzHL, threshold);
			}
			in.airAccelMult = airMult;
			in.airSurface = airSurface;
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
			const bool  edge = g_edgeAhead.load() && (g_cfg.edgeFriction == 2 || (g_cfg.edgeFriction == 1 && g_cfg.mode == Mode::kHL1));
			in.frictionMult = frictionMult * surface * (edge ? g_cfg.edgeFrictionMult : 1.0f);
			in.accelMult = accelMult * surface;
		}

		// run the Half-Life steps at the chosen smoothness
		const hlmath::Vec2 stepStart = SimVel();
		const float        graceBefore = g_air.grace;
		hlcore::State      st{ stepStart, graceBefore };
		hlmath::Vec2  outV;
		if (g_cfg.smoothing == 2) {
			hlcore::Advance(st, in, p, dt);  // this frame's time in Half-Life-sized steps, all now: no added delay
			outV = st.vel;
		} else {
			g_sim.acc += dt;
			int steps = 0;
			while (g_sim.acc >= p.tick && steps < 16) {
				g_sim.pvx = st.vel.x;
				g_sim.pvy = st.vel.y;
				hlcore::Step(st, in, p, p.tick);
				g_sim.acc -= p.tick;
				++steps;
			}
			if (steps == 16) {
				g_sim.acc = 0.0f;
			}
			outV = st.vel;
			if (g_cfg.smoothing == 1) {
				const float t = std::clamp(g_sim.acc / p.tick, 0.0f, 1.0f);
				outV = { g_sim.pvx + (st.vel.x - g_sim.pvx) * t, g_sim.pvy + (st.vel.y - g_sim.pvy) * t };
			}
		}
		SetSimVel(st.vel);
		g_air.grace = st.grace;

		const float outX = outV.x, outY = outV.y;
		g_air.lastOut = outV;
		g_air.hasLastOut = true;

		RE::hkVector4f out = a_vel;
		out.x = outX * g_cfg.unitScale;
		out.y = outY * g_cfg.unitScale;
		out.z = g_air.vzOn ? hlVz : CrouchJumpVertical(a_vel.z, actual.z, rawGround, dt);
		g_air.sentVz = out.z;
		{
			const float sp = std::hypot(outX, outY);
			g_moveDirX = sp > 1.0f ? outX / sp : 0.0f;
			g_moveDirY = sp > 1.0f ? outY / sp : 0.0f;
		}

		if (Record::g_on) {
			Record::Row row{};
			row.dt = dt;
			row.rawGround = rawGround;
			row.ground = onGround;
			row.wishX = wish.dirX;
			row.wishY = wish.dirY;
			row.wishSpeed = wish.speed;
			row.stickX = wish.inX;
			row.stickY = wish.inY;
			row.yawRate = g_input.yawRate;
			row.askedX = asked.x;
			row.askedY = asked.y;
			row.askedVz = askedVzHL;
			row.actX = act.x;
			row.actY = act.y;
			row.repX = reported.x / g_cfg.unitScale;
			row.repY = reported.y / g_cfg.unitScale;
			row.gnx = gnx;
			row.gny = gny;
			row.gnz = gnz;
			row.hasGround = hasGround;
			row.simBeforeX = simBefore.x;
			row.simBeforeY = simBefore.y;
			row.contact = static_cast<int>(contact);
			row.simX = stepStart.x;  // start of the Half-Life steps (after walls, pushes and the jump boost)
			row.simY = stepStart.y;
			row.frictionMult = in.frictionMult;
			row.accelMult = in.accelMult;
			row.airAccelMult = in.airAccelMult;
			row.airSurface = in.airSurface;
			row.grace = graceBefore;
			row.smoothing = g_cfg.smoothing;
			row.outX = outX;
			row.outY = outY;
			row.inX = a_vel.x / g_cfg.unitScale;
			row.inY = a_vel.y / g_cfg.unitScale;
			row.inVz = a_vel.z / g_cfg.unitScale;
			row.outVz = out.z / g_cfg.unitScale;
			row.measVz = g_air.measVz / g_cfg.unitScale;
			Record::Add(row);
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
}
