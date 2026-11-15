#include "../src/Formula.h"
#include <cstdio>
#include <random>

// Simulates many kills and checks the behaviour targets.
static void Sim(const char* name, int level, double hp, int value, double& none, double& avg, int& max)
{
	std::mt19937 rng{ 42 };
	std::uniform_real_distribution<double> u(0, 1);
	const double t = Formula::Threat(level, hp);
	int zero = 0; long sum = 0; max = 0;
	const int N = 200000;
	for (int i = 0; i < N; ++i) {
		int r = Formula::Rounds(t, value, u(rng), u(rng), u(rng));
		zero += r == 0; sum += r; if (r > max) max = r;
	}
	none = 100.0 * zero / N; avg = double(sum) / N;
	std::printf("%-22s threat %6.1f  nothing %5.1f%%  avg %.2f  max %d\n", name, t, none, avg, max);
}

int main()
{
	double none, avg; int max; int fail = 0;
	Sim("chicken/rat, 10mm", 1, 10, 1, none, avg, max);        fail += !(none > 88 && max <= 2);
	Sim("raider, 5.56", 10, 100, 2, none, avg, max);            fail += !(none < 70 && none > 40);
	Sim("deathclaw, .308", 40, 800, 3, none, avg, max);         fail += !(none < 25 && max >= 4);
	Sim("deathclaw, fusion core", 40, 800, 200, none, avg, max); fail += !(avg < 0.5);
	Sim("deathclaw, mini nuke", 40, 800, 100, none, avg, max);  fail += !(avg < 0.6);
	std::printf(fail ? "FAILED\n" : "all good\n");
	return fail;
}
