// Unit tests for src/HLMath.h. Built and run by the GitHub workflow on every push.
// Expected values are worked out by hand from Valve's formulas.

#include "../src/HLMath.h"

#include <cstdio>
#include <cstdlib>

using namespace hlmath;

static int g_failures = 0;

static void Check(bool a_ok, const char* a_what, double a_got, double a_want)
{
	if (!a_ok) {
		std::printf("FAIL: %s (got %.4f, want %.4f)\n", a_what, a_got, a_want);
		++g_failures;
	} else {
		std::printf("ok:   %s\n", a_what);
	}
}

static void Near(double a_got, double a_want, double a_tol, const char* a_what)
{
	Check(std::fabs(a_got - a_want) <= a_tol, a_what, a_got, a_want);
}

int main()
{
	const float dt = 0.015f;  // one HL2 tick

	// --- friction: speed 320, friction 4, stopspeed 100: drop = 320*4*0.015 = 19.2
	{
		Vec2 v = Friction({ 320.0f, 0.0f }, 4.0f, 100.0f, 1.0f, dt);
		Near(v.x, 300.8, 1e-3, "friction above stop speed");
		// below stop speed: control = 100 -> drop = 6 -> 50 -> 44
		v = Friction({ 50.0f, 0.0f }, 4.0f, 100.0f, 1.0f, dt);
		Near(v.x, 44.0, 1e-3, "friction below stop speed uses stop speed");
		v = Friction({ 3.0f, 0.0f }, 4.0f, 100.0f, 1.0f, dt);
		Near(v.Length(), 0.0, 1e-6, "friction stops slow movement");
		v = Friction({ 0.05f, 0.0f }, 4.0f, 100.0f, 1.0f, dt);
		Near(v.Length(), 0.0, 1e-6, "friction zeroes below 0.1");
		// ice (surface 0.125): drop = 320*4*0.125*0.015 = 2.4
		v = Friction({ 320.0f, 0.0f }, 4.0f, 100.0f, 0.125f, dt);
		Near(v.x, 317.6, 1e-3, "surface friction scales the drop");
	}

	// --- ground acceleration: from 0 toward 190 at accel 10: 10*0.015*190 = 28.5
	{
		Vec2 v = Accelerate({}, { 1.0f, 0.0f }, 190.0f, 10.0f, 1.0f, dt);
		Near(v.x, 28.5, 1e-3, "accelerate from standstill");
		v = Accelerate({ 180.0f, 0.0f }, { 1.0f, 0.0f }, 190.0f, 10.0f, 1.0f, dt);
		Near(v.x, 190.0, 1e-3, "accelerate clamps to wish speed");
		v = Accelerate({ 400.0f, 0.0f }, { 1.0f, 0.0f }, 190.0f, 10.0f, 1.0f, dt);
		Near(v.x, 400.0, 1e-3, "accelerate never slows you");
		// reaching full speed takes about 0.1 s (7 ticks) at accel 10
		v = {};
		int ticks = 0;
		while (v.x < 189.99f && ticks < 100) {
			v = Accelerate(v, { 1.0f, 0.0f }, 190.0f, 10.0f, 1.0f, dt);
			++ticks;
		}
		Check(ticks == 7, "full speed in 7 ticks", ticks, 7);
	}

	// --- air acceleration
	{
		// straight ahead at 300: dot = 300 > 30, no gain (you can't speed up by holding forward in the air)
		Vec2 v = AirAccelerate({ 300.0f, 0.0f }, { 1.0f, 0.0f }, 320.0f, 10.0f, 30.0f, 1.0f, dt);
		Near(v.x, 300.0, 1e-3, "no air gain holding forward");
		// strafing at 90 degrees: dot = 0, add = 30, accelspeed = 10*320*0.015 = 48 -> clamp 30
		v = AirAccelerate({ 300.0f, 0.0f }, { 0.0f, 1.0f }, 320.0f, 10.0f, 30.0f, 1.0f, dt);
		Near(v.y, 30.0, 1e-3, "air strafe adds up to the 30 cap");
		Check(v.Length() > 300.0f, "air strafe gains speed", v.Length(), 301.5);
		// small wish speed (sneak, 100): accelspeed = 10*100*0.015 = 15
		v = AirAccelerate({ 300.0f, 0.0f }, { 0.0f, 1.0f }, 100.0f, 10.0f, 30.0f, 1.0f, dt);
		Near(v.y, 15.0, 1e-3, "air accel uses uncapped wish speed");
		// repeated optimal strafing (turning with the velocity) keeps gaining
		v = { 300.0f, 0.0f };
		for (int i = 0; i < 100; ++i) {
			const Vec2  n = v.Normalized();
			const Vec2  side{ -n.y, n.x };
			// optimal angle: dot(v, dir) = 30 - accelspeed (accelspeed 48 > 30, so dot = 0 -> 90 deg)
			v = AirAccelerate(v, side, 320.0f, 10.0f, 30.0f, 1.0f, dt);
		}
		Check(v.Length() > 330.0f, "chained air strafing builds speed", v.Length(), 330.0);
	}

	// --- clip velocity (walls)
	{
		// running at 45 degrees into a wall whose normal is -x: keep the sliding part
		Vec2 v = ClipVelocity({ 200.0f, 200.0f }, { 1.0f, 0.0f });
		Near(v.x, 0.0, 1e-3, "clip removes the part into the wall");
		Near(v.y, 200.0, 1e-3, "clip keeps the sliding part");
		v = ClipVelocity({ -100.0f, 50.0f }, { 1.0f, 0.0f });
		Near(v.x, -100.0, 1e-3, "clip ignores walls you move away from");
	}

	// --- HL1 bunnyhop cap: max 320 -> limit 544; at 600 you drop to 544*0.65 = 353.6
	{
		Vec2 v = HL1BunnyCap({ 600.0f, 0.0f }, 0.0f, 320.0f);
		Near(v.x, 353.6, 1e-2, "HL1 cap cuts mega bunnyhops");
		v = HL1BunnyCap({ 500.0f, 0.0f }, 0.0f, 320.0f);
		Near(v.x, 500.0, 1e-3, "HL1 cap leaves normal speed alone");
		// regression: the cap uses 3D speed. 500 horizontal + 268 vertical = 567.3 > 544:
		// fraction = 544 / 567.27 * 0.65 = 0.6233 -> 311.7
		v = HL1BunnyCap({ 500.0f, 0.0f }, 268.0f, 320.0f);
		Near(v.x, 500.0 * (544.0 / std::sqrt(500.0 * 500.0 + 268.0 * 268.0)) * 0.65, 1e-2, "HL1 cap counts vertical speed (3D)");
		v = HL1BunnyCap({ 500.0f, 0.0f }, -268.0f, 320.0f);
		Near(v.x, 311.7, 0.1, "HL1 cap: falling speed counts the same as rising");
		// with a 268 fall, the horizontal limit is sqrt(544^2 - 268^2) = 473.4
		v = HL1BunnyCap({ 473.0f, 0.0f }, -268.0f, 320.0f);
		Near(v.x, 473.0, 1e-3, "HL1 cap: just under the 3D limit is untouched");
	}

	// --- HL2 jump boost: max 190, forward 190 at standstill: +95 (to 285 = 190*1.5)
	{
		Vec2 v = HL2JumpBoost({}, { 1.0f, 0.0f }, 190.0f, 190.0f, false);
		Near(v.x, 95.0, 1e-3, "HL2 boost from standstill");
		v = HL2JumpBoost({ 250.0f, 0.0f }, { 1.0f, 0.0f }, 190.0f, 190.0f, false);
		Near(v.x, 285.0, 1e-3, "HL2 boost stops at 1.5x max speed");
		// moving backwards at 400 while holding back: the cap holds you at 1.5x max speed
		v = HL2JumpBoost({ -400.0f, 0.0f }, { 1.0f, 0.0f }, -190.0f, 190.0f, false);
		Near(v.x, -285.0, 1e-3, "HL2 boost caps backwards speed when facing away from it");
		// ABH: moving forward at 400 while holding the back key: the boost is flipped and added forward
		v = HL2JumpBoost({ 400.0f, 0.0f }, { 1.0f, 0.0f }, -190.0f, 190.0f, false);
		Near(v.x, 515.0, 1e-3, "ABH: holding back while moving forward speeds you up");
		v = HL2JumpBoost({}, { 1.0f, 0.0f }, 320.0f, 320.0f, true);
		Near(v.x, 32.0, 1e-3, "HL2 sprint jump gets 10%");
	}

	// --- step sounds
	{
		StepResult r = StepSound(true, StepSurface::kNormal, false, false);
		Near(r.volume, 0.5, 1e-6, "HL1 running step volume");
		Near(r.interval, 0.3, 1e-6, "HL1 running step time");
		r = StepSound(true, StepSurface::kDirt, true, false);
		Near(r.volume, 0.25, 1e-6, "dirt walking volume");
		r = StepSound(false, StepSurface::kVent, false, true);
		Near(r.volume, 0.7 * 0.65, 1e-6, "HL2 duct ducking volume");
		Near(r.interval, 0.4, 1e-6, "ducking adds 100 ms");
		r = StepSound(true, StepSurface::kNormal, true, true);
		Near(r.volume, 0.2 * 0.35, 1e-6, "HL1 ducking volume 35%");
		r = StepSound(false, StepSurface::kWade, false, false);
		Near(r.interval, 0.6, 1e-6, "wading step time");
	}

	// --- landing punch and decay
	{
		Near(LandingPunch(true, 349.0f), 0.0, 1e-6, "HL1 no punch below 350");
		Near(LandingPunch(true, 500.0f), 6.5, 1e-4, "HL1 punch = fall * 0.013");
		Near(LandingPunch(false, 303.0f), 3.939, 1e-3, "HL2 punch from 303");
		Near(DecayPunch(6.5f, 0.1f), 6.5 - (10 + 3.25) * 0.1, 1e-4, "punch decay");
		Near(DecayPunch(0.5f, 1.0f), 0.0, 1e-6, "punch decays to zero");
	}

	// --- HL2 rising air friction
	{
		Near(HL2AirSurface(160.0f, 140.0f), 1.0, 1e-6, "HL2: rising fast keeps full air control");
		Near(HL2AirSurface(100.0f, 140.0f), 0.25, 1e-6, "HL2: rising slowly quarters air control");
		Near(HL2AirSurface(140.0f, 140.0f), 0.25, 1e-6, "HL2: exactly 140 counts as slow");
		Near(HL2AirSurface(-50.0f, 140.0f), 1.0, 1e-6, "HL2: falling keeps full air control");
		const Vec2 slow = AirAccelerate({ 300.0f, 0.0f }, { 0.0f, 1.0f }, 100.0f, 10.0f, 30.0f, 0.25f, dt);
		Near(slow.y, 3.75, 1e-3, "HL2: quartered air acceleration (10*100*0.015*0.25)");
	}

	// --- crouch-jump lift
	{
		// from the top of the jump, +18 HL units (0.4572 m) at Fallout gravity 13.24: sqrt(2*13.24*0.4572)
		Near(CrouchLiftSpeed(0.0f, 13.24f, 0.4572f), 3.4795, 1e-3, "crouch lift from the peak");
		// already rising at 2 m/s: the lift is added on top of what you'd climb anyway
		const float v = CrouchLiftSpeed(2.0f, 13.24f, 0.4572f);
		Near((v * v - 4.0f) / (2.0f * 13.24f), 0.4572, 1e-4, "crouch lift adds exactly the lift height");
		Near(CrouchLiftSpeed(-1.0f, 13.24f, 0.4572f), 3.4795, 1e-3, "falling slightly: lift from here");
	}

	std::printf("\n%d failure(s)\n", g_failures);
	return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
