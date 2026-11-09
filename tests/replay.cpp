// B.H.O.P. replay tool: reads a BHOP_record.csv written in game (MCM -> Debug -> Record movement) and runs
// the movement core on exactly the same moments, so changes can be checked without playing.
//
//   bhop_replay BHOP_record.csv            summary + wall/push events
//   bhop_replay BHOP_record.csv --steps    every step
//
// What it checks:
//  - walls/slopes/pushes: runs the current wall rules on each step's "asked vs what really happened"
//    and lists where the decision differs from the recording (useful after changing the rules)
//  - movement steps: re-runs the Half-Life steps from the recorded starting speed and inputs and compares
//    with what the plugin sent (should match: proves the offline core is the in-game core)
//  - wall slides: speed kept or lost while touching walls

#include "../src/core/HLMath.h"
#include "../src/core/MoveCore.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	std::map<std::string, float> ParseHeader(const std::string& a_line)
	{
		// "# BHOP record | mode 2 | friction 4 accelerate 10 stopspeed 100 tick 0.015 | airaccelerate 10 ..."
		std::map<std::string, float> out;
		std::istringstream           in(a_line);
		std::string                  word, prev;
		while (in >> word) {
			char* end = nullptr;
			const float v = std::strtof(word.c_str(), &end);
			if (end && *end == '\0' && !prev.empty()) {
				out[prev] = v;
			}
			prev = word;
		}
		return out;
	}

	const char* KindName(int a_k)
	{
		switch (a_k) {
		case 1: return "kept";
		case 2: return "wall";
		case 3: return "push";
		default: return "none";
		}
	}
}

int main(int argc, char** argv)
{
	if (argc < 2) {
		std::printf("usage: bhop_replay BHOP_record.csv [--steps]\n");
		return EXIT_FAILURE;
	}
	const bool    all = argc > 2 && std::strcmp(argv[2], "--steps") == 0;
	std::ifstream f(argv[1]);
	if (!f) {
		std::printf("can't open %s\n", argv[1]);
		return EXIT_FAILURE;
	}
	std::string header, names, line;
	std::getline(f, header);
	std::getline(f, names);
	const auto h = ParseHeader(header);
	auto       get = [&](const char* k, float d) { auto it = h.find(k); return it != h.end() ? it->second : d; };

	hlcore::Params p;
	p.friction = get("friction", 4.0f);
	p.accelerate = get("accelerate", 10.0f);
	p.stopSpeed = get("stopspeed", 100.0f);
	p.tick = get("tick", 0.01f);
	p.airAccelerate = get("airaccelerate", 10.0f);
	p.airCap = get("aircap", 30.0f);
	p.strafeAssist = get("assist", 0.5f);
	std::printf("%s\nparams: friction %.2f accelerate %.2f stopspeed %.0f tick %.4f airaccelerate %.1f cap %.0f assist %.2f\n\n",
		header.c_str(), p.friction, p.accelerate, p.stopSpeed, p.tick, p.airAccelerate, p.airCap, p.strafeAssist);

	int    rows = 0, groundRows = 0, differ = 0, events = 0, stepChecked = 0;
	int    kinds[4]{}, recKinds[4]{};
	double maxStepErr = 0.0, wallTime = 0.0, wallSpeedIn = 0.0, wallSpeedOut = 0.0;
	while (std::getline(f, line)) {
		if (line.empty() || line[0] == '#') {
			continue;
		}
		std::vector<float> v;
		std::stringstream  ss(line);
		std::string        cell;
		while (std::getline(ss, cell, ',')) {
			v.push_back(std::strtof(cell.c_str(), nullptr));
		}
		if (v.size() < 39) {
			continue;
		}
		++rows;
		// column order: see the second line of the file (Record.h)
		const float t = v[0], dt = v[1];
		const bool  ground = v[3] != 0.0f;
		groundRows += ground;

		hlcore::CollideInput ci;
		ci.asked = { v[10], v[11] };
		ci.askedVz = v[12];
		ci.actual = { v[13], v[14] };
		ci.hasGround = v[20] != 0.0f;
		ci.gnx = v[17];
		ci.gny = v[18];
		ci.gnz = v[19];
		ci.sim = { v[21], v[22] };
		const int  rec = static_cast<int>(v[23]);
		const auto r = hlcore::Collide(ci);
		const int  now = static_cast<int>(r.kind);
		kinds[now & 3]++;
		recKinds[rec & 3]++;
		if (now != rec) {
			++differ;
		}
		if (now == 2) {
			wallTime += dt;
			wallSpeedIn += ci.sim.Length() * dt;
			wallSpeedOut += r.sim.Length() * dt;
		}
		if ((now >= 2 || now != rec) && events < 40) {
			++events;
			std::printf("t %6.3f %s%s: asked (%.0f,%.0f) actual (%.0f,%.0f) game says (%.0f,%.0f) speed %.0f -> %.0f%s\n",
				t, KindName(now), now != rec ? " (recorded: different)" : "", ci.asked.x, ci.asked.y, ci.actual.x, ci.actual.y,
				v[15], v[16], ci.sim.Length(), r.sim.Length(), r.slope ? " [slope]" : "");
		}

		// re-run the Half-Life steps from the recorded start (exact only for the default smoothing)
		if (static_cast<int>(v[31]) == 2) {
			hlcore::StepInput in;
			in.ground = ground;
			in.wishDir = { v[4], v[5] };
			in.wishSpeed = v[6];
			in.stickX = v[7];
			in.yawRate = v[9];
			in.frictionMult = v[26];
			in.accelMult = v[27];
			in.airAccelMult = v[28];
			in.airSurface = v[29];
			hlcore::State st{ { v[24], v[25] }, v[30] };
			hlcore::Advance(st, in, p, dt);
			const double err = std::hypot(st.vel.x - v[32], st.vel.y - v[33]);
			maxStepErr = std::max(maxStepErr, err);
			++stepChecked;
		}
		if (all) {
			std::printf("t %6.3f dt %.4f %s wish %.0f out (%.0f,%.0f) actual (%.0f,%.0f) vz in %.0f out %.0f real %.0f\n", t, dt,
				ground ? "ground" : "air   ", v[6], v[32], v[33], v[13], v[14], v[35 + 1], v[37], v[38]);
		}
	}

	std::printf("\n%d steps (%d on the ground)\n", rows, groundRows);
	std::printf("wall rules now:      none %d, kept direction %d, wall %d, push %d\n", kinds[0], kinds[1], kinds[2], kinds[3]);
	std::printf("wall rules recorded: none %d, kept direction %d, wall %d, push %d (%d steps differ)\n", recKinds[0], recKinds[1], recKinds[2], recKinds[3], differ);
	if (wallTime > 0.0) {
		std::printf("touching walls %.2f s: average speed %.0f before the wall rule, %.0f after\n", wallTime, wallSpeedIn / wallTime, wallSpeedOut / wallTime);
	}
	if (stepChecked > 0) {
		std::printf("movement steps re-run: %d, biggest difference from the game %.3f units/s (should be ~0)\n", stepChecked, maxStepErr);
	}
	return EXIT_SUCCESS;
}
