#include "../src/NightRule.h"

#include <cstdio>

static int g_fail = 0;
#define CHECK(x)                                                     \
	do {                                                             \
		if (!(x)) {                                                  \
			std::printf("FAIL line %d: %s\n", __LINE__, #x);         \
			++g_fail;                                                \
		}                                                            \
	} while (0)

int main()
{
	using namespace NightRule;
	// 9 PM -> 6 AM (crosses midnight)
	CHECK(!IsNight(20.99f, 21, 6));
	CHECK(IsNight(21.0f, 21, 6));
	CHECK(IsNight(23.9f, 21, 6));
	CHECK(IsNight(0.0f, 21, 6));
	CHECK(IsNight(5.99f, 21, 6));
	CHECK(!IsNight(6.0f, 21, 6));
	CHECK(!IsNight(12.0f, 21, 6));
	CHECK(IsNight(24.0f, 21, 6));  // GameHour can read 24.0 for a moment at midnight
	// 1 AM -> 5 AM (same day)
	CHECK(!IsNight(0.5f, 1, 5));
	CHECK(IsNight(1.0f, 1, 5));
	CHECK(IsNight(4.5f, 1, 5));
	CHECK(!IsNight(5.0f, 1, 5));
	CHECK(!IsNight(22.0f, 1, 5));
	// same hour = never, bad values
	CHECK(!IsNight(3.0f, 4, 4));
	CHECK(!IsNight(NAN, 21, 6));
	CHECK(!IsNight(22.0f, 99, -5) && IsNight(23.2f, 99, -5));  // clamped to 23 -> 0
	CHECK(!IsNight(22.5f, 23, 0) && IsNight(23.5f, 23, 0));

	// sun message: only on night -> day, never on the first step after a load
	SunWatch w;
	CHECK(!w.Step(false));
	CHECK(!w.Step(true));
	CHECK(w.Step(false));
	CHECK(!w.Step(false));
	w.Reset();
	CHECK(!w.Step(true));
	w.Reset();
	CHECK(!w.Step(false));  // loaded in daytime after a night save: no message

	// ini keys
	Settings s;
	CHECK(ApplyKey(s, "bImmersiveMode", "1") && s.enabled);
	CHECK(ApplyKey(s, "iNightStart", "22") && s.nightStart == 22);
	CHECK(ApplyKey(s, "iNightEnd", "30") && s.nightEnd == 23);
	CHECK(ApplyKey(s, "iNightEnd", "junk") && s.nightEnd == 23);
	CHECK(ApplyKey(s, "bSunMessage", "0") && !s.sunMessage);
	CHECK(ApplyKey(s, "sTooDarkMessage", "Dark!") && s.darkText == "Dark!");
	CHECK(!ApplyKey(s, "iConditionA", "1"));

	std::printf(g_fail ? "%d test(s) failed\n" : "all night rule tests passed\n", g_fail);
	return g_fail ? 1 : 0;
}
