#include "../src/Formula.h"
#include <cstdio>
#include <random>

static int fail = 0;

static void Sim(const char* name, int level, double hp, int value, int scrounger, double& none, int& max)
{
	std::mt19937 rng{ 42 };
	std::uniform_real_distribution<double> u(0, 1);
	const double t = Formula::Threat(level, hp);
	int zero = 0; long sum = 0; max = 0;
	const int N = 200000;
	for (int i = 0; i < N; ++i) {
		int r = Formula::Rounds(t, value, u(rng), u(rng), u(rng), scrounger);
		zero += r == 0; sum += r; if (r > max) max = r;
	}
	none = 100.0 * zero / N;
	std::printf("%-30s scr %d  nothing %5.1f%%  avg %.2f  max %d\n", name, scrounger, none, double(sum) / N, max);
}

int main()
{
	double none, none4; int max;
	Sim("chicken/rat, 10mm", 1, 10, 1, 0, none, max);       fail += !(none > 88 && max <= 2);
	Sim("raider, 5.56", 10, 100, 2, 0, none, max);           fail += !(none < 70 && none > 40);
	Sim("raider, 5.56", 10, 100, 2, 4, none4, max);          fail += !(none4 < none * 0.7);
	Sim("deathclaw, .308", 40, 800, 3, 0, none, max);        fail += !(none < 25 && max >= 4);
	Sim("deathclaw, fusion core", 40, 800, 200, 0, none, max);

	std::mt19937 rng{ 7 };
	std::uniform_real_distribution<double> u(0, 1);
	int gmax[3] = {};
	const double threats[3] = { Formula::Threat(10, 100), Formula::Threat(30, 300), Formula::Threat(50, 900) };
	for (int k = 0; k < 3; ++k)
		for (int i = 0; i < 100000; ++i) gmax[k] = std::max(gmax[k], Formula::Grenades(threats[k], u(rng)));
	std::printf("grenades max: raider %d  gunner %d  overlord %d\n", gmax[0], gmax[1], gmax[2]);
	fail += !(gmax[0] == 1 && gmax[1] == 2 && gmax[2] == 3);
	fail += !(Formula::GrenadeQualifies(20, 100, false) && !Formula::GrenadeQualifies(19, 100, false) &&
	          Formula::GrenadeQualifies(0, 100, true));

	std::printf(fail ? "FAILED\n" : "all good\n");
	return fail;
}
