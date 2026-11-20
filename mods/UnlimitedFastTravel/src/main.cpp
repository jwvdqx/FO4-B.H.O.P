// Unlimited Fast Travel - Immersive Mode
//
// Companion plugin for Unlimited Fast Travel (it works next to UnlimitedFastTravel.dll and
// shares its MCM page). With Immersive Mode on, fast travel from the Pip-Boy map is refused
// between two hours set in MCM (night), and a message says why. When the night ends a second
// message says fast travel is back.
//
// How: PlayerCharacter::QueueFastTravel is the call the game makes once a fast travel has been
// confirmed (all of the game's own checks, and Unlimited Fast Travel's changes to them, are
// already done by then). It is hooked; at night, and only for a trip picked on the Pip-Boy map,
// the trip is dropped and the message is shown. Scripted trips (quests) are never touched.

#include <RE/Fallout.h>
#include <F4SE/F4SE.h>

#include <MinHook.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

#include "NightRule.h"

using namespace std::literals;

namespace
{
	// ---- settings (shared MCM page with Unlimited Fast Travel, section [Immersive]) -------------
	std::mutex          g_cfgLock;
	NightRule::Settings g_cfg;

	std::string Trim(const std::string& a_s)
	{
		const auto b = a_s.find_first_not_of(" \t\r\n");
		if (b == std::string::npos) {
			return {};
		}
		const auto e = a_s.find_last_not_of(" \t\r\n");
		return a_s.substr(b, e - b + 1);
	}

	void ReadIni(const char* a_path, NightRule::Settings& a_out)
	{
		std::ifstream in(a_path);
		bool          inSection = false;
		for (std::string line; std::getline(in, line);) {
			line = Trim(line);
			if (line.empty() || line[0] == ';' || line[0] == '#') {
				continue;
			}
			if (line[0] == '[') {
				inSection = line == "[Immersive]";
				continue;
			}
			const auto eq = line.find('=');
			if (inSection && eq != std::string::npos) {
				NightRule::ApplyKey(a_out, Trim(line.substr(0, eq)), Trim(line.substr(eq + 1)));
			}
		}
	}

	void LoadSettings()
	{
		NightRule::Settings s;
		ReadIni("Data/MCM/Config/UnlimitedFastTravel/settings.ini", s);  // defaults
		ReadIni("Data/MCM/Settings/UnlimitedFastTravel.ini", s);         // what MCM saved
		REX::INFO("Immersive Mode {} | night {}:00 -> {}:00 | sun message {}",
			s.enabled ? "ON" : "OFF", s.nightStart, s.nightEnd, s.sunMessage ? "on" : "off");
		std::scoped_lock l{ g_cfgLock };
		g_cfg = std::move(s);
	}

	NightRule::Settings Settings()
	{
		std::scoped_lock l{ g_cfgLock };
		return g_cfg;
	}

	// ---- game state --------------------------------------------------------------------------
	bool GameHour(float& a_out)
	{
		auto cal = RE::Calendar::GetSingleton();
		if (!cal || !cal->gameHour) {
			return false;
		}
		a_out = cal->gameHour->GetValue();
		return true;
	}

	bool IsNightNow(const NightRule::Settings& a_s)
	{
		float hour = 0.0f;
		return a_s.enabled && GameHour(hour) && NightRule::IsNight(hour, a_s.nightStart, a_s.nightEnd);
	}

	// A trip counts as picked on the map when the Pip-Boy is open or closed a moment ago
	// (confirming a fast travel closes it; the trip may be queued just after).
	std::atomic<std::int64_t> g_pipboyClosedMs{ -100000 };

	std::int64_t NowMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	bool FromPipboyMap()
	{
		static const RE::BSFixedString pipboy{ "PipboyMenu" };
		if (auto ui = RE::UI::GetSingleton(); ui && ui->GetMenuOpen(pipboy)) {
			return true;
		}
		return NowMs() - g_pipboyClosedMs.load() < 3000;
	}

	void Notify(const std::string& a_text, bool a_warning)
	{
		RE::SendHUDMessage::ShowHUDMessage(a_text.c_str(), nullptr, false, a_warning);
	}

	// ---- the hook ----------------------------------------------------------------------------
	using QueueFastTravel_t = void (*)(RE::PlayerCharacter*, RE::ObjectRefHandle, bool);
	QueueFastTravel_t g_queueFastTravel = nullptr;

	void QueueFastTravel(RE::PlayerCharacter* a_player, RE::ObjectRefHandle a_marker, bool a_allowAutoSave)
	{
		const auto s = Settings();
		if (IsNightNow(s) && FromPipboyMap()) {
			float hour = 0.0f;
			GameHour(hour);
			REX::INFO("fast travel refused: game hour {:.2f} is inside night {}:00 -> {}:00", hour, s.nightStart, s.nightEnd);
			Notify(s.darkText, true);
			return;
		}
		g_queueFastTravel(a_player, a_marker, a_allowAutoSave);
	}

	bool InstallHook()
	{
		static REL::Relocation<std::uintptr_t> target{ RE::ID::PlayerCharacter::QueueFastTravel };
		const auto addr = reinterpret_cast<void*>(target.address());
		if (const auto init = MH_Initialize(); init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
			REX::ERROR("MinHook failed to start");
			return false;
		}
		const auto created = MH_CreateHook(addr, reinterpret_cast<void*>(&QueueFastTravel), reinterpret_cast<void**>(&g_queueFastTravel));
		const auto enabled = created == MH_OK ? MH_EnableHook(addr) : created;
		if (enabled != MH_OK) {
			REX::ERROR("could not hook PlayerCharacter::QueueFastTravel at {:X} (MinHook {})", target.address(), MH_StatusToString(enabled));
			return false;
		}
		REX::INFO("hooked PlayerCharacter::QueueFastTravel at {:X}", target.address());
		return true;
	}

	// ---- the sun message: the clock is checked once a second on the main thread ---------------
	NightRule::SunWatch g_sun;  // main thread only
	std::atomic_bool    g_inGame{ false };

	void Tick()
	{
		if (!g_inGame) {
			return;
		}
		auto ui = RE::UI::GetSingleton();
		static const RE::BSFixedString loading{ "LoadingMenu" };
		if (!ui || ui->GetMenuOpen(loading) || !RE::PlayerCharacter::GetSingleton()) {
			return;
		}
		const auto s = Settings();
		if (!s.enabled) {
			g_sun.Reset();  // switched on again later: learn the state first, no message
			return;
		}
		if (g_sun.Step(IsNightNow(s)) && s.sunMessage) {
			REX::INFO("night is over, fast travel available");
			Notify(s.sunText, false);
		}
	}

	void StartClock()
	{
		static std::jthread clock([](std::stop_token a_stop) {
			while (!a_stop.stop_requested()) {
				std::this_thread::sleep_for(1s);
				if (auto task = F4SE::GetTaskInterface()) {
					task->AddTask([]() { Tick(); });
				}
			}
		});
	}

	// ---- events --------------------------------------------------------------------------------
	class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (a_event.opening) {
				return RE::BSEventNotifyControl::kContinue;
			}
			if (a_event.menuName == "PipboyMenu"sv) {
				g_pipboyClosedMs = NowMs();
			} else if (a_event.menuName == "PauseMenu"sv || a_event.menuName == "MainMenu"sv) {
				LoadSettings();  // MCM lives in the pause menu: changes apply when it closes
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	} g_menuSink;

	void OnMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case F4SE::MessagingInterface::kGameDataReady:
			if (auto ui = RE::UI::GetSingleton()) {
				ui->RegisterSink<RE::MenuOpenCloseEvent>(&g_menuSink);
			}
			StartClock();
			break;
		case F4SE::MessagingInterface::kPostLoadGame:
		case F4SE::MessagingInterface::kNewGame:
			if (auto task = F4SE::GetTaskInterface()) {
				task->AddTask([]() { g_sun.Reset(); });  // no sun message for a save loaded in daytime
			}
			g_inGame = true;
			break;
		default:
			break;
		}
	}
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);
	LoadSettings();
	if (!InstallHook()) {
		return false;
	}
	if (auto messaging = F4SE::GetMessagingInterface()) {
		messaging->RegisterListener(OnMessage);
	}
	REX::INFO("Unlimited Fast Travel - Immersive Mode loaded");
	return true;
}
