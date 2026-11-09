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
	};

	struct CollideResult
	{
		Contact kind{ Contact::kNone };
		Vec2    sim{};
		Vec2    normal{};    // wall direction (into the wall) for kWall
		Vec2    expected{};  // what we expected you to move at (asked, bent along the slope)
		bool    slope{ false };  // expected came from the slope model
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
