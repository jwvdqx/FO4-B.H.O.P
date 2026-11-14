// Unit tests for src/core/HLMath.h and src/core/MoveCore.h. Built and run by the GitHub workflow on every push.
// Expected values are worked out by hand from Valve's formulas.

#include "../src/core/HLMath.h"
#include "../src/core/MoveCore.h"

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

		// wall guess: sliding along a wall (normal +y) while Havok also shaved 10% off the sliding part
		Vec2 n = WallNormal({ 300.0f, 100.0f }, { 270.0f, 0.0f });
		Near(n.x, 0.0, 1e-4, "wall guess ignores Havok's forward loss (x)");
		Near(n.y, 1.0, 1e-4, "wall guess ignores Havok's forward loss (y)");
		v = ClipVelocity({ 300.0f, 100.0f }, n);
		Near(v.x, 300.0, 1e-3, "wall slide keeps all forward speed");
		// tiny jitter while already sliding: not a wall hit, no speed lost
		n = WallNormal({ 400.0f, 0.0f }, { 392.0f, 5.0f });
		Near(n.Length(), 0.0, 1e-6, "jitter is not a wall");
		// head-on: the wall is straight ahead
		n = WallNormal({ 400.0f, 0.0f }, { 0.0f, 0.0f });
		Near(n.x, 1.0, 1e-4, "head-on wall faces you");
		// glancing hit at about 30 degrees: normal is the sideways part
		n = WallNormal({ 346.4f, 200.0f }, { 346.4f, 0.0f });
		Near(n.y, 1.0, 1e-4, "glancing hit finds the wall");
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


	// --- movement core: Half-Life-sized steps with no delay
	{
		hlcore::Params p;
		p.tick = 0.01f;
		hlcore::StepInput in;
		in.ground = true;
		in.wishDir = { 1.0f, 0.0f };
		in.wishSpeed = 320.0f;
		hlcore::State a;
		Near(hlcore::Advance(a, in, p, 0.022f), 3.0, 1e-6, "core: a 22 ms frame is 10 + 10 + 2 ms");
		hlcore::State b;
		hlcore::Step(b, in, p, 0.01f);
		hlcore::Step(b, in, p, 0.01f);
		hlcore::Step(b, in, p, 0.002f);
		Near(a.vel.x, b.vel.x, 1e-4, "core: same result as doing the steps by hand");

		// stick air strafe: holding right and turning right at a steady 90 degrees/s. At 30 fps, split into
		// steps that each use the view of their moment, you gain what you would at 100 fps.
		auto gain = [&](float a_frame, bool a_split) {
			hlcore::State s;
			s.vel = { 0.0f, 400.0f };  // moving forward (yaw 0)
			hlcore::StepInput air;
			air.ground = false;
			air.wishSpeed = 320.0f;
			air.yawRate = 1.5707963f;
			float yaw = 0.0f;
			for (int i = 0; i < static_cast<int>(1.0f / a_frame + 0.5f); ++i) {
				yaw += air.yawRate * a_frame;
				air.wishDir = { std::cos(yaw), -std::sin(yaw) };  // right of the view
				if (a_split) {
					hlcore::Advance(s, air, p, a_frame);
				} else {
					hlcore::Step(s, air, p, a_frame);
				}
			}
			return s.vel.Length() - 400.0f;
		};
		const double at100 = gain(0.01f, true), at30 = gain(1.0f / 30.0f, true), at30single = gain(1.0f / 30.0f, false);
		Near(at30, at100, 0.03 * at100, "core: stick air strafe at 30 fps gains what 100 fps does (within 3%)");
		Check(std::fabs(at30single - at100) > 0.1 * at100, "core: one big step per frame would not (why we split)", at30single, at100);
	}

	// --- walls, slopes, pushes
	{
		using hlcore::Contact;
		hlcore::CollideInput c;
		c.asked = { 300.0f, 100.0f };
		c.actual = { 270.0f, 0.0f };  // wall at +y, and the game also shaved 10% off the slide
		c.sim = { 300.0f, 100.0f };
		auto r = hlcore::Collide(c);
		Check(r.kind == Contact::kWall, "collide: wall found", static_cast<double>(r.kind), 2.0);
		Near(r.sim.x, 300.0, 1e-3, "collide: wall slide keeps forward speed");
		Near(r.sim.y, 0.0, 1e-3, "collide: speed into the wall removed");

		c.asked = { 400.0f, 0.0f };
		c.actual = { 0.0f, 0.0f };
		c.sim = { 400.0f, 0.0f };
		r = hlcore::Collide(c);
		Near(r.sim.Length(), 0.0, 1e-3, "collide: head-on wall stops you");

		c.asked = { 300.0f, 0.0f };
		c.actual = { 300.0f, 120.0f };
		c.sim = { 300.0f, 0.0f };
		r = hlcore::Collide(c);
		Check(r.kind == Contact::kPush, "collide: sideways shove is a push", static_cast<double>(r.kind), 3.0);
		Near(r.sim.y, 120.0, 1e-3, "collide: push added to momentum");

		// going diagonally up a 30 degree slope: the game bends you along the ground. Not a wall.
		c.asked = { 300.0f, 300.0f };
		c.askedVz = 0.0f;
		c.hasGround = true;
		c.gnx = -0.5f;
		c.gny = 0.0f;
		c.gnz = 0.8660254f;
		const Vec2 bent = hlcore::AlongGround(c.asked, 0.0f, c.gnx, c.gny, c.gnz);
		Near(bent.x, 225.0, 1e-2, "slope: uphill part shortened (300 - 0.5*0.5*300)");
		c.actual = bent;
		c.sim = c.asked;
		r = hlcore::Collide(c);
		Check(r.slope && r.kind == Contact::kNone, "slope: explained by the floor, not a wall", static_cast<double>(r.kind), 0.0);
		Near(r.sim.Length(), c.asked.Length(), 1e-3, "slope: no speed taken");
		c.hasGround = false;

		// stairs/clutter: the game lifts you while bending you sideways: not a wall
		c.asked = { 300.0f, 0.0f };
		c.actual = { 270.0f, 60.0f };
		c.sim = c.asked;
		c.onGround = true;
		c.hasActualVz = true;
		c.actualVz = 150.0f;
		r = hlcore::Collide(c);
		Check(r.stepping && r.kind == Contact::kKept, "stairs: stepping up is not a wall", static_cast<double>(r.kind), 1.0);
		Near(r.sim.x, 300.0, 1e-3, "stairs: no speed taken, no sideways shove");
		// same bend with no lift on flat ground: that IS a wall
		c.actualVz = 0.0f;
		r = hlcore::Collide(c);
		Check(r.kind == Contact::kWall, "stairs: no lift on flat ground = wall", static_cast<double>(r.kind), 2.0);
		// stepping but nearly stopped: really blocked, still a wall
		c.actual = { 60.0f, 20.0f };
		c.actualVz = 150.0f;
		r = hlcore::Collide(c);
		Check(r.kind == Contact::kWall, "stairs: blocked while lifted is still a wall", static_cast<double>(r.kind), 2.0);
		c.onGround = false;
		c.hasActualVz = false;

		// the probe's surface direction replaces the guess only when they roughly agree
		Vec2 n = hlcore::RefineNormal({ 0.0f, 1.0f }, { 0.2f, 0.98f });
		Near(n.x, 0.2 / std::hypot(0.2, 0.98), 1e-4, "probe: close direction is used");
		n = hlcore::RefineNormal({ 0.0f, 1.0f }, { 1.0f, 0.0f });
		Near(n.x, 0.0, 1e-6, "probe: a different surface is ignored");
	}

	// --- Half-Life jumping and gravity: HL1 jump with sv_gravity 800 reaches 45 units
	{
		float vz = hlcore::JumpSpeed(800.0f, 45.0f);
		Near(vz, 268.33, 0.01, "jump: HL1 takeoff sqrt(2*800*45)");
		Near(hlcore::JumpSpeed(600.0f, 21.0f), 158.75, 0.01, "jump: HL2 takeoff sqrt(2*600*21)");
		float z = 0.0f, top = 0.0f;
		for (int i = 0; i < 200 && (i == 0 || z > 0.0f); ++i) {
			z += hlcore::GravityStep(vz, 800.0f, 0.01f) * 0.01f;
			top = std::max(top, z);
		}
		Near(top, 45.0, 0.6, "jump: the Half-Life way of adding gravity reaches 45 units");
	}


	// --- swimming: water friction, sinking, and the water surf (hold jump at the surface)
	{
		using hlcore::Vec3;
		// 4 friction for 0.1 s: speed * (1 - 0.01*4) per 10 ms step
		Vec3 v{ 300.0f, 0.0f, 0.0f };
		for (int i = 0; i < 10; ++i) {
			v = hlcore::WaterMove(v, {}, 320.0f, 4.0f, 10.0f, 1.0f, 0.01f);
		}
		Near(v.x, 300.0 * std::pow(0.96, 10), 0.01, "swim: water friction on your speed");
		// no input: you sink, at most 60*0.8 = 48 units/s
		v = {};
		for (int i = 0; i < 300; ++i) {
			v = hlcore::WaterMove(v, hlcore::WaterWish(false, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, 0.0f, 0.0f, false, 320.0f), 320.0f, 4.0f, 10.0f, 1.0f, 0.01f);
		}
		Near(v.z, -48.0, 0.5, "swim: idle you sink slowly (60 * 0.8)");
		// swim speed is 80% of max speed
		v = {};
		for (int i = 0; i < 300; ++i) {
			v = hlcore::WaterMove(v, hlcore::WaterWish(false, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, 320.0f, 0.0f, false, 320.0f), 320.0f, 4.0f, 10.0f, 1.0f, 0.01f);
		}
		Near(v.y, 256.0, 0.5, "swim: top speed is 80% of running");

		// the surf: waist in water = swimming (friction), above it = air (no friction, gravity).
		// Holding jump pushes you up at 100 every time your waist dips in, so you skim and keep speed.
		auto surf = [](bool a_jump, bool a_hl2, float a_z0 = -10.0f) {
			Vec3  vel{ 600.0f, 0.0f, 0.0f };
			float z = a_z0;  // waist height relative to the water surface
			for (int i = 0; i < 200; ++i) {
				if (z < 0.0f) {
					if (a_jump) {
						vel.z = hlcore::kWaterJumpSpeed;
					}
					vel = hlcore::WaterMove(vel, hlcore::WaterWish(a_hl2, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, 0.0f, 0.0f, a_jump, 320.0f), 320.0f, 4.0f, 10.0f, 1.0f, 0.01f);
				} else {
					vel.z -= (a_hl2 ? 600.0f : 800.0f) * 0.01f;
				}
				z += vel.z * 0.01f;
			}
			return std::hypot(vel.x, vel.y);
		};
		Check(surf(true, false) > 250.0, "surf: HL1 holding jump keeps your speed across water", surf(true, false), 250.0);
		Check(surf(false, false) < 5.0, "surf: without jump the water stops you", surf(false, false), 0.0);
		Check(surf(true, true) > 250.0, "surf: HL2 holding jump keeps your speed too", surf(true, true), 250.0);

		// the plugin's surf (Fallout keeps you at the surface, so no real bob): same speed as the bobbing above
		hlcore::Params p;
		p.tick = 0.01f;
		p.friction = 4.0f;
		hlcore::State st;
		st.vel = { 600.0f, 0.0f };
		hlcore::StepInput in;
		in.ground = false;
		in.water = 2;
		in.surfInWater = hlcore::SurfInWater(0.01f, 800.0f);
		for (int i = 0; i < 200; ++i) {
			hlcore::Step(st, in, p, 0.01f);
		}
		const double bob = surf(true, false, -0.5f);  // starting at the surface, like the plugin
		Near(st.vel.Length(), bob, 0.05 * bob, "surf: the plugin's surf matches HL1's bobbing (within 5%)");
		// swimming (flat) reaches 80% of max speed
		hlcore::State sw;
		in.water = 1;
		in.wishDir = { 1.0f, 0.0f };
		in.wishSpeed = 320.0f;
		in.waterMax = 320.0f;
		for (int i = 0; i < 300; ++i) {
			hlcore::Step(sw, in, p, 0.01f);
		}
		Near(sw.vel.x, 256.0, 0.5, "swim: the plugin's swimming tops out at 80%");
	}

	// ---- Half-Life slopes: crossing a slope at an angle keeps your heading
	{
		// a 30 degree slope rising toward +x; you walk diagonally across it
		const float nx = -0.5f, ny = 0.0f, nz = 0.8660254f;
		const Vec2  v{ 200.0f, 200.0f };
		const Vec2  bent = hlcore::AlongGround(v, 0.0f, nx, ny, nz);
		Check(std::fabs(bent.x - v.x) > 20.0f, "slope: a level velocity gets bent sideways by the game", bent.x, 150.0);
		const float vz = hlcore::SlopeClimbVz(v, 0.0f, nx, ny, nz, 0.7f);
		const Vec2  kept = hlcore::AlongGround(v, vz, nx, ny, nz);
		Near(kept.x, 200.0, 0.01, "slope: with the climb speed your heading is kept (x)");
		Near(kept.y, 200.0, 0.01, "slope: with the climb speed your heading is kept (y)");
		Near(vz, 200.0 * 0.5 / 0.8660254, 0.01, "slope: climb speed = rise of the slope under your path");
		Near(hlcore::SlopeClimbVz({ -200.0f, 0.0f }, 0.0f, nx, ny, nz, 0.7f), 0.0, 0.001, "slope: going down adds nothing");
		Near(hlcore::SlopeClimbVz(v, 0.0f, -0.8f, 0.0f, 0.6f, 0.7f), 0.0, 0.001, "slope: too steep to walk on is left to the game");
	}

	// ---- jump speed: the takeoff speed for a height really reaches that height
	{
		const float g = 13.24f, h = 43.0f * 0.0254f;  // Fallout's gravity (m/s2), its jump height (43 HL units in metres)
		float       vz = hlcore::JumpSpeed(g, h), z = 0.0f, peak = 0.0f;
		for (int i = 0; i < 2000 && (vz > 0.0f || z > 0.0f); ++i) {
			z += hlcore::GravityStep(vz, g, 0.001f) * 0.001f;
			peak = std::max(peak, z);
		}
		Near(peak, h, 0.002, "jump speed: takeoff speed reaches the height asked for");
		// Half-Life numbers used by the Full Half-Life profiles: 45 units at 800, 21 at 600
		Near(hlcore::JumpSpeed(800.0f, 45.0f), 268.33, 0.01, "HL1 jump: 268.3 units/s takeoff");
		Near(hlcore::JumpSpeed(600.0f, 21.0f), 158.75, 0.01, "HL2 jump: 158.7 units/s takeoff");
		// slope limit: 45.57 degrees is Half-Life's floor normal z 0.7
		Near(std::cos(45.57 * 0.01745329), 0.7, 0.0005, "slope limit: 45.57 degrees = normal z 0.7");
	}

	std::printf("\n%d failure(s)\n", g_failures);
	return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
