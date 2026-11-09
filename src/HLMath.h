#pragma once

// Pure Half-Life / Source movement math, with no game code in it, so it can be unit tested.
// Checked against Valve's published code:
//   Half-Life 1: pm_shared.c (PM_Friction, PM_Accelerate, PM_AirAccelerate, PM_PreventMegaBunnyJumping,
//                PM_UpdateStepSound, PM_CheckFalling)
//   Source/HL2:  gamemovement.cpp (Friction, Accelerate, AirAccelerate, ClipVelocity, CheckJumpButton),
//                baseplayer_shared.cpp (UpdateStepSound, SetStepSoundTime)
// Units are Half-Life units (inches) and seconds.

#include <algorithm>
#include <cmath>

namespace hlmath
{
	struct Vec2
	{
		float x{ 0.0f };
		float y{ 0.0f };

		Vec2 operator+(Vec2 o) const { return { x + o.x, y + o.y }; }
		Vec2 operator-(Vec2 o) const { return { x - o.x, y - o.y }; }
		Vec2 operator*(float s) const { return { x * s, y * s }; }
		float Dot(Vec2 o) const { return x * o.x + y * o.y; }
		float Length() const { return std::sqrt(x * x + y * y); }
		Vec2  Normalized() const
		{
			const float l = Length();
			return l > 1e-6f ? Vec2{ x / l, y / l } : Vec2{};
		}
	};

	// PM_Friction / CGameMovement::Friction (ground only)
	inline Vec2 Friction(Vec2 a_vel, float a_friction, float a_stopSpeed, float a_surface, float a_dt)
	{
		const float speed = a_vel.Length();
		if (speed < 0.1f) {
			return {};
		}
		const float control = speed < a_stopSpeed ? a_stopSpeed : speed;
		const float drop = control * a_friction * a_surface * a_dt;
		const float newSpeed = std::max(speed - drop, 0.0f);
		return a_vel * (newSpeed / speed);
	}

	// PM_Accelerate / CGameMovement::Accelerate
	inline Vec2 Accelerate(Vec2 a_vel, Vec2 a_wishDir, float a_wishSpeed, float a_accel, float a_surface, float a_dt)
	{
		const float current = a_vel.Dot(a_wishDir);
		const float add = a_wishSpeed - current;
		if (add <= 0.0f) {
			return a_vel;
		}
		const float accelSpeed = std::min(a_accel * a_dt * a_wishSpeed * a_surface, add);
		return a_vel + a_wishDir * accelSpeed;
	}

	// PM_AirAccelerate / CGameMovement::AirAccelerate (wish speed capped at a_cap = 30 for the gain test,
	// but the uncapped wish speed sets the acceleration, which is what makes air strafing work)
	inline Vec2 AirAccelerate(Vec2 a_vel, Vec2 a_wishDir, float a_wishSpeed, float a_accel, float a_cap, float a_surface, float a_dt)
	{
		const float wishSpd = std::min(a_wishSpeed, a_cap);
		const float current = a_vel.Dot(a_wishDir);
		const float add = wishSpd - current;
		if (add <= 0.0f) {
			return a_vel;
		}
		const float accelSpeed = std::min(a_accel * a_wishSpeed * a_dt * a_surface, add);
		return a_vel + a_wishDir * accelSpeed;
	}

	// CGameMovement::ClipVelocity with overbounce 1.0: remove the part going into a surface.
	inline Vec2 ClipVelocity(Vec2 a_vel, Vec2 a_normal)
	{
		const float backoff = a_vel.Dot(a_normal);
		if (backoff <= 0.0f) {
			return a_vel;  // moving away from it already
		}
		return a_vel - a_normal * backoff;
	}

	// PM_PreventMegaBunnyJumping (HL1): above 1.7x max speed you're cut to 0.65 of that.
	// The original measures the FULL 3D velocity. It runs inside PM_Jump before the jump's upward speed
	// is set, but on the landing frame (the frame a bunnyhop jumps on) the velocity still holds the
	// downward speed of the fall, because HL1 zeroes it only after the jump check. a_verticalSpeed is that
	// speed (sign doesn't matter). Only the horizontal result is returned: the vertical part is replaced
	// by the jump speed right after, in the original too.
	inline Vec2 HL1BunnyCap(Vec2 a_vel, float a_verticalSpeed, float a_maxSpeed)
	{
		const float maxScaled = 1.7f * a_maxSpeed;
		if (maxScaled <= 0.0f) {
			return a_vel;
		}
		const float speed = std::sqrt(a_vel.x * a_vel.x + a_vel.y * a_vel.y + a_verticalSpeed * a_verticalSpeed);
		if (speed <= maxScaled) {
			return a_vel;
		}
		return a_vel * ((maxScaled / speed) * 0.65f);
	}

	// CGameMovement::CheckJumpButton, HL2 single player forward boost (source of ABH).
	// a_forwardMove: forward input in HL units/s (negative = backwards), a_forward: facing direction.
	inline Vec2 HL2JumpBoost(Vec2 a_vel, Vec2 a_forward, float a_forwardMove, float a_maxSpeed, bool a_sprintOrDuck)
	{
		const float perc = a_sprintOrDuck ? 0.1f : 0.5f;
		float       addition = std::fabs(a_forwardMove * perc);
		const float maxSpeed = a_maxSpeed + a_maxSpeed * perc;
		const float newSpeed = addition + a_vel.Length();
		if (newSpeed > maxSpeed) {
			addition -= newSpeed - maxSpeed;
		}
		if (a_forwardMove < 0.0f) {
			addition *= -1.0f;
		}
		return a_vel + a_forward * addition;
	}

	// Step sound rules (PM_UpdateStepSound / CBasePlayer::UpdateStepSound)
	enum class StepSurface { kNormal, kDirt, kVent, kWade, kLadder };

	struct StepResult
	{
		float volume;
		float interval;  // seconds until the next step
	};

	inline StepResult StepSound(bool a_hl1, StepSurface a_surface, bool a_walking, bool a_ducking)
	{
		StepResult r{ a_walking ? 0.2f : 0.5f, a_walking ? 0.4f : 0.3f };
		switch (a_surface) {
		case StepSurface::kDirt:
			r.volume = a_walking ? 0.25f : 0.55f;
			break;
		case StepSurface::kVent:
			r.volume = a_walking ? 0.4f : 0.7f;
			break;
		case StepSurface::kWade:
			r = { 0.65f, 0.6f };
			break;
		case StepSurface::kLadder:
			r = { 0.35f, 0.35f };
			break;
		default:
			break;
		}
		if (a_ducking) {
			r.interval += 0.1f;                    // flduck / SetStepSoundTime +100 ms
			r.volume *= a_hl1 ? 0.35f : 0.65f;     // HL1 35%, HL2 65%
		}
		return r;
	}

	// Landing camera punch (roll), both games: fall speed * 0.013 once past the punch threshold.
	inline float LandingPunch(bool a_hl1, float a_fallSpeed)
	{
		const float threshold = a_hl1 ? 350.0f : 303.0f;  // PLAYER_FALL_PUNCH_THRESHOLD
		return a_fallSpeed >= threshold ? a_fallSpeed * 0.013f : 0.0f;
	}

	// CGameMovement::CategorizePosition (HL2): rising slower than the threshold (140) sets surface
	// friction to 0.25, which scales air acceleration; rising faster or falling keeps 1.0.
	inline float HL2AirSurface(float a_verticalSpeed, float a_threshold)
	{
		return (a_verticalSpeed > 0.0f && a_verticalSpeed <= a_threshold) ? 0.25f : 1.0f;
	}

	// Crouch-jump lift: the upward speed (m/s) that adds a_lift metres of height from the current point,
	// keeping any upward speed you already have.
	inline float CrouchLiftSpeed(float a_upSpeed, float a_gravity, float a_lift)
	{
		const float up = std::max(a_upSpeed, 0.0f);
		return std::sqrt(up * up + 2.0f * a_gravity * a_lift);
	}

	// PM_DropPunchAngle (HL1 decay, also used for HL2 here)
	inline float DecayPunch(float a_punch, float a_dt)
	{
		return std::max(0.0f, a_punch - (10.0f + a_punch * 0.5f) * a_dt);
	}
}
