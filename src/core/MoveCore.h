#pragma once
// B.H.O.P. movement core: the Half-Life movement rules as plain functions, with no Fallout code.
// The plugin feeds it what Fallout tells us; the tests and the replay tool feed it numbers.
// Units: Half-Life units (inches), seconds. z is up.

#include "HLMath.h"

#include <algorithm>
#include <cmath>

namespace hlcore
{
	using hlmath::Vec2;

	// ---------------------------------------------------------------------
	// Half-Life swimming (PM_WaterMove / CGameMovement::WaterMove)
	// ---------------------------------------------------------------------
	struct Vec3
	{
		float x{ 0.0f }, y{ 0.0f }, z{ 0.0f };
		Vec3  operator+(Vec3 o) const { return { x + o.x, y + o.y, z + o.z }; }
		Vec3  operator*(float s) const { return { x * s, y * s, z * s }; }
		float Length() const { return std::sqrt(x * x + y * y + z * z); }
	};

	inline constexpr float kWaterJumpSpeed = 100.0f;  // holding jump with your waist in water: PM_Jump / CheckJumpButton
	inline constexpr float kWaterSink = 60.0f;        // no input: drift toward the bottom

	// What you want to swim at: the view's forward (pitch included) and right, times the stick.
	// HL1: no input = sink. HL2 also: jump held = swim up, and swimming forward while looking up rises more.
	inline Vec3 WaterWish(bool a_hl2, Vec3 a_forward, Vec2 a_right, float a_forwardMove, float a_sideMove, bool a_jump, float a_maxSpeed)
	{
		Vec3       w{ a_forward.x * a_forwardMove + a_right.x * a_sideMove, a_forward.y * a_forwardMove + a_right.y * a_sideMove, a_forward.z * a_forwardMove };
		const bool noInput = a_forwardMove == 0.0f && a_sideMove == 0.0f;
		if (a_hl2) {
			if (a_jump) {
				w.z += a_maxSpeed;
			} else if (noInput) {
				w.z -= kWaterSink;
			} else {
				w.z += std::clamp(a_forwardMove * a_forward.z * 2.0f, 0.0f, a_maxSpeed);
			}
		} else if (noInput) {
			w.z -= kWaterSink;
		}
		return w;
	}

	// One step of swimming: speed capped to max and then cut to 80%, water friction on the whole 3D speed,
	// then acceleration toward the wish (like ground acceleration, but in 3D).
	inline Vec3 WaterMove(Vec3 a_vel, Vec3 a_wish, float a_maxSpeed, float a_friction, float a_accelerate, float a_surface, float a_dt)
	{
		float      wishSpeed = a_wish.Length();
		const Vec3 wishDir = wishSpeed > 1e-6f ? a_wish * (1.0f / wishSpeed) : Vec3{};
		if (wishSpeed > a_maxSpeed) {
			wishSpeed = a_maxSpeed;
		}
		wishSpeed *= 0.8f;

		const float speed = a_vel.Length();
		float       newSpeed = 0.0f;
		if (speed > 0.0f) {
			newSpeed = speed - a_dt * speed * a_friction * a_surface;
			if (newSpeed < 0.1f) {
				newSpeed = 0.0f;
			}
			a_vel = a_vel * (newSpeed / speed);
		}
		if (wishSpeed >= 0.1f) {
			const float addSpeed = wishSpeed - newSpeed;
			if (addSpeed > 0.0f) {
				const float accelSpeed = std::min(a_accelerate * wishSpeed * a_dt * a_surface, addSpeed);
				a_vel = a_vel + wishDir * accelSpeed;
			}
		}
		return a_vel;
	}

	// Water surfing: holding jump with your waist in water pushes you up at 100 u/s every time you dip in
	// (PM_Jump / CheckJumpButton), so you spend most of the time just above the water, moving like in the
	// air (no friction, air strafing), and only a moment in it. This is the share of time spent in the water:
	// one step in, then up and back down: 2 * 100 / gravity seconds out.
	inline float SurfInWater(float a_tick, float a_gravity)
	{
		return a_tick / (a_tick + 2.0f * kWaterJumpSpeed / std::max(a_gravity, 1.0f));
	}

	// ---------------------------------------------------------------------
	// one movement step
	// ---------------------------------------------------------------------
	struct Params
	{
		float friction{ 4.0f };
		float stopSpeed{ 100.0f };
		float accelerate{ 10.0f };
		float airAccelerate{ 10.0f };
		float airCap{ 30.0f };
		float strafeAssist{ 0.0f };  // 0..1
		float tick{ 0.01f };         // Half-Life's step length (s)
	};

	struct StepInput
	{
		bool  ground{ true };
		Vec2  wishDir{};
		float wishSpeed{ 0.0f };
		float stickX{ 0.0f };        // -1..1, + = right (air strafe assist)
		float yawRate{ 0.0f };       // rad/s, + = turning right (air strafe assist, and turning within a frame)
		float frictionMult{ 1.0f };  // weight, Power Armor, surface, edge friction, all multiplied in
		float accelMult{ 1.0f };     // weight, Power Armor, surface
		float airAccelMult{ 1.0f };  // perks
		float airSurface{ 1.0f };    // HL2: 0.25 while rising slowly
		int   water{ 0 };            // 0 = not in water, 1 = swimming (WaterMove), 2 = water surfing
		float waterMax{ 320.0f };    // swimming: your max speed (you swim at 80% of it)
		float surfInWater{ 0.04f };  // water surfing: share of the time spent in the water (SurfInWater)
	};

	struct State
	{
		Vec2  vel{};
		float grace{ 0.0f };  // bunnyhop window: no friction while it lasts
	};

	// PM_AirAccelerate with the optional stick assist: when you strafe and turn the same way
	// (the Half-Life technique), steer the wish direction toward the angle that gains the most speed.
	inline Vec2 AirAccelerate(Vec2 a_vel, const StepInput& a_in, const Params& a_p, float a_dt)
	{
		if (a_in.wishSpeed <= 0.0f) {
			return a_vel;
		}
		Vec2        dir = a_in.wishDir;
		const float speed = a_vel.Length();
		const float cap = a_p.airCap;
		const float accel = a_p.airAccelerate * a_in.airAccelMult;
		const float accelSpeedMax = accel * a_in.wishSpeed * a_dt;
		const float assist = std::clamp(a_p.strafeAssist, 0.0f, 1.0f);
		if (assist > 0.0f && speed > cap && std::fabs(a_in.stickX) > 0.3f && a_in.stickX * a_in.yawRate > 0.2f) {
			const float vx = a_vel.x / speed, vy = a_vel.y / speed;
			const float cross = vx * dir.y - vy * dir.x;  // which side of the velocity the wish is on
			const float side = cross >= 0.0f ? 1.0f : -1.0f;
			const float c = std::clamp((cap - std::min(accelSpeedMax, cap)) / speed, 0.0f, 1.0f);
			const float th = std::acos(c);
			const float cs = std::cos(th), sn = std::sin(th) * side;
			const Vec2  best{ vx * cs - vy * sn, vx * sn + vy * cs };  // velocity rotated by th
			dir = (dir + (best - dir) * assist).Normalized();
		}
		return hlmath::AirAccelerate(a_vel, dir, a_in.wishSpeed, accel, cap, a_in.airSurface, a_dt);
	}

	// One Half-Life step: ground = friction (unless in the bunnyhop window) then accelerate; air = air accelerate.
	inline void Step(State& a_s, const StepInput& a_in, const Params& a_p, float a_dt)
	{
		if (a_in.water == 1) {
			// swimming, flat: Fallout keeps your up and down (floating, diving)
			const Vec3 wish{ a_in.wishDir.x * a_in.wishSpeed, a_in.wishDir.y * a_in.wishSpeed, 0.0f };
			const Vec3 v = WaterMove({ a_s.vel.x, a_s.vel.y, 0.0f }, wish, a_in.waterMax, a_p.friction, a_p.accelerate, 1.0f, a_dt);
			a_s.vel = { v.x, v.y };
			return;
		}
		if (a_in.water == 2) {
			// water surfing: moving like in the air, with water friction only for the moments you dip in
			a_s.vel = AirAccelerate(a_s.vel, a_in, a_p, a_dt);
			const float speed = a_s.vel.Length();
			if (speed > 0.0f) {
				const float newSpeed = std::max(0.0f, speed - a_dt * speed * a_p.friction * a_in.surfInWater);
				a_s.vel = a_s.vel * (newSpeed / speed);
			}
			return;
		}
		if (a_in.ground) {
			if (a_s.grace > 0.0f) {
				a_s.grace -= a_dt;
			} else {
				a_s.vel = hlmath::Friction(a_s.vel, a_p.friction, a_p.stopSpeed, a_in.frictionMult, a_dt);
			}
			a_s.vel = hlmath::Accelerate(a_s.vel, a_in.wishDir, a_in.wishSpeed, a_p.accelerate, a_in.accelMult, a_dt);
		} else {
			a_s.vel = AirAccelerate(a_s.vel, a_in, a_p, a_dt);
		}
	}

	// Turn a direction the way the view turns when yaw changes by a_yaw (radians, + = right).
	// Fallout's forward is (sin yaw, cos yaw).
	inline Vec2 RotateYaw(Vec2 a_v, float a_yaw)
	{
		const float c = std::cos(a_yaw), s = std::sin(a_yaw);
		return { a_v.x * c + a_v.y * s, -a_v.x * s + a_v.y * c };
	}

	// Run a frame's worth of time as Half-Life-sized steps, all right now (no added delay).
	// A 22 ms frame becomes 10 + 10 + 2 ms, and each step uses the direction you were facing at that
	// moment of the frame (you turned smoothly during it). Half-Life's results then don't depend on
	// your frame rate: air strafing gains what it would at 100 fps.
	inline int Advance(State& a_s, const StepInput& a_in, const Params& a_p, float a_dt)
	{
		const float tick = std::clamp(a_p.tick, 0.002f, 0.05f);
		float       left = a_dt;
		int         n = 0;
		StepInput   in = a_in;
		while (left > 1e-6f && n < 64) {
			const float h = std::min(left, tick);
			left -= h;
			// the input is for the end of the frame; this step ends 'left' seconds earlier
			in.wishDir = left > 0.0f && a_in.yawRate != 0.0f ? RotateYaw(a_in.wishDir, -a_in.yawRate * left) : a_in.wishDir;
			Step(a_s, in, a_p, h);
			++n;
		}
		return n;
	}

	// ---------------------------------------------------------------------
	// walls, slopes and pushes: what did the game do with the velocity we asked for?
	// ---------------------------------------------------------------------
	enum class Contact : int
	{
		kNone = 0,  // moved as asked
		kKept,      // lost speed but kept the direction (slope, step, the game's own loss): left alone
		kWall,      // a wall/NPC/door took the part going into it: removed from our velocity too
		kPush,      // something pushed you: added to our momentum
	};

	struct CollideInput
	{
		Vec2  asked{};    // horizontal velocity we asked for last step
		float askedVz{ 0.0f };
		Vec2  actual{};   // horizontal velocity you really moved at
		Vec2  sim{};      // our velocity now
		bool  hasGround{ false };  // ground normal valid (standing on something)
		float gnx{ 0.0f }, gny{ 0.0f }, gnz{ 1.0f };
		bool  wallClip{ true };
		bool  pushes{ true };
		float pushThreshold{ 60.0f };
		// stairs and clutter: on the ground, the game lifting or dropping you (more than the floor's slope
		// explains) means it's stepping you over something, not that you hit a wall
		bool  onGround{ false };
		bool  hasActualVz{ false };
		float actualVz{ 0.0f };  // vertical speed you really moved at (HL units/s)
	};

	struct CollideResult
	{
		Contact kind{ Contact::kNone };
		Vec2    sim{};
		Vec2    normal{};    // wall direction (into the wall) for kWall
		Vec2    expected{};  // what we expected you to move at (asked, bent along the slope)
		bool    slope{ false };  // expected came from the slope model
		bool    stepping{ false };  // the game was stepping you up or down (stairs, clutter)
	};

	// On a slope the game bends your motion along the ground: v - n*(v.n) when you push into it.
	// Returns the horizontal part of that.
	inline Vec2 AlongGround(Vec2 a_v, float a_vz, float a_nx, float a_ny, float a_nz)
	{
		const float d = a_v.x * a_nx + a_v.y * a_ny + a_vz * a_nz;
		if (d >= 0.0f) {
			return a_v;
		}
		return { a_v.x - a_nx * d, a_v.y - a_ny * d };
	}

	// Half-Life slopes: on a slope you can walk on (the originals: floor normal z >= 0.7, about 45 degrees)
	// you keep your heading and your speed over the ground; you just climb. The game instead bends a
	// level velocity along the slope, which turns you sideways when you cross it at an angle and slows
	// you going up. Handing it a velocity that already lies in the slope (this upward speed) leaves
	// nothing to bend. Only ever adds upward speed, and only when you're moving into the slope.
	inline float SlopeClimbVz(Vec2 a_v, float a_vz, float a_nx, float a_ny, float a_nz, float a_minNz)
	{
		if (a_nz < a_minNz || a_nz >= 0.9995f) {
			return a_vz;  // too steep to walk on, or flat
		}
		const float d = a_v.x * a_nx + a_v.y * a_ny + a_vz * a_nz;
		return d < 0.0f ? a_vz - d / a_nz : a_vz;
	}

	inline CollideResult Collide(const CollideInput& a_in)
	{
		CollideResult r;
		r.sim = a_in.sim;
		r.expected = a_in.asked;
		// slopes: if the game's ground direction explains what happened better than "as asked", use it
		if (a_in.hasGround && a_in.gnz > 0.5f && a_in.gnz < 0.999f) {
			const Vec2 slope = AlongGround(a_in.asked, a_in.askedVz, a_in.gnx, a_in.gny, a_in.gnz);
			if ((a_in.actual - slope).Length() < (a_in.actual - a_in.asked).Length()) {
				r.expected = slope;
				r.slope = true;
			}
		}
		const Vec2  exp = r.expected;
		const Vec2  delta = a_in.actual - exp;
		const float expLen = exp.Length();
		const float actLen = a_in.actual.Length();
		if (delta.Length() <= 5.0f) {
			return r;
		}
		// A wall can only take speed away; extra speed that isn't against your motion is a push.
		// 15%: slack for small step-timing differences in the measurement.
		const float dLen = delta.Length();
		const float along = expLen > 1.0f ? delta.Dot(exp * (1.0f / expLen)) : 0.0f;
		if (a_in.pushes && dLen > expLen * 0.15f + a_in.pushThreshold && along >= -0.2f * dLen) {
			r.kind = Contact::kPush;
			r.sim = a_in.sim + delta;
			return r;
		}
		if (!a_in.wallClip || expLen <= 10.0f || actLen > expLen * 1.15f + 5.0f) {
			return r;
		}
		// Stepping over stairs or clutter bends and slows you for a moment: leave your speed alone, so you go
		// up and over instead of being shoved sideways or stopped. (Really blocked = less than half the speed.)
		if (a_in.onGround && a_in.hasActualVz && actLen >= 0.5f * expLen) {
			const float slopeVz = a_in.hasGround && a_in.gnz > 0.5f ? -(a_in.gnx * a_in.actual.x + a_in.gny * a_in.actual.y) / a_in.gnz : 0.0f;
			if (std::fabs(a_in.actualVz - slopeVz) > 30.0f) {
				r.kind = Contact::kKept;
				r.stepping = true;
				return r;
			}
		}
		const Vec2 n = hlmath::WallNormal(exp, a_in.actual);
		if (n.Length() < 0.5f) {
			r.kind = Contact::kKept;
			return r;
		}
		r.kind = Contact::kWall;
		r.normal = n;
		r.sim = hlmath::ClipVelocity(a_in.sim, n);
		return r;
	}

	// A measured wall direction (from probing the real surface) replaces the guess when they agree
	// roughly (within ~45 degrees): the guess finds THAT there is a wall, the probe says exactly how it faces.
	inline Vec2 RefineNormal(Vec2 a_guess, Vec2 a_probe)
	{
		const Vec2 p = a_probe.Normalized();
		if (p.Length() < 0.5f || a_guess.Length() < 0.5f) {
			return a_guess;
		}
		return p.Dot(a_guess) > 0.7f ? p : a_guess;
	}

	// ---------------------------------------------------------------------
	// Half-Life jumping and gravity (optional)
	// ---------------------------------------------------------------------
	// Takeoff speed that reaches a_height with gravity a_g.
	inline float JumpSpeed(float a_g, float a_height)
	{
		return std::sqrt(std::max(0.0f, 2.0f * a_g * a_height));
	}

	// One step of gravity, done the Half-Life way (half before the move, half after):
	// returns the vertical speed to move at during this step and updates a_vz to the end of it.
	inline float GravityStep(float& a_vz, float a_g, float a_dt)
	{
		const float during = a_vz - 0.5f * a_g * a_dt;
		a_vz -= a_g * a_dt;
		return during;
	}

}
