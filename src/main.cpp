// B.H.O.P. - Boston Hopping Overhaul Project
//
// Stage 1: Half-Life ground acceleration, friction and weight.
// Sounds:  Half-Life 1 / 2 player footsteps, jump and landing sounds, picked by the
//          surface you stand on, timed the way Half-Life times them. The player's
//          vanilla footsteps are muted; NPC footsteps are left alone.
//
// Movement: the player's character controller is given its new velocity every
// physics step through SetLinearVelocityImpl. Vanilla Fallout passes the speed it
// wants you to move at, applied instantly. We treat that as Half-Life's "wish
// velocity" and run Half-Life's friction + acceleration on our own velocity.

//
// Layout: src/core/ holds the pure Half-Life movement rules (no Fallout code, unit-tested).
// src/fo4/ holds the Fallout side, one part per file, included here in order.

#include "fo4/Config.h"
#include "fo4/Rtti.h"
#include "fo4/GameState.h"
#include "fo4/Camera.h"
#include "fo4/Sounds.h"
#include "fo4/Movement.h"
#include "fo4/Record.h"
#include "fo4/Hook.h"

namespace
{
	// Fresh start after loading a save or starting a new game: nothing carries over.
	void ResetRuntimeState()
	{
		std::scoped_lock lock(g_simLock);
		g_sim = Sim{};
		g_air = AirState{};
		g_input = InputState{};
		g_autoJumpUntil = 0;
		g_jumpPressedMs = 0;
		g_jumpDown = false;
		AnimSpeed::g_applied = 0.0f;  // saves never hold our animation speed change (taken off before saving)
		AnimSpeed::g_smooth = 1.0f;
		Perks::g_baseline = false;  // the perk line shows again for this save
		g_crouchPressedMs = 0;
		g_landSneak = false;
		g_sneakWatchUntil = 0;
		g_standAskedMs = 0;
		g_edgeAhead = false;
		g_stats = Stats{};
		Sounds::g_water = Sounds::WaterState{};
		Camera::Reset();
		Sounds::OnGameLoaded();
	}

	// MCM lives in the pause menu: when it closes, read the settings again so changes apply right away.
	void ReloadSettings()
	{
		if (static bool logged = false; !logged && g_cfg.debugLog) {
			logged = true;
			REX::INFO("thread check: settings reload on thread {}", REX::W32::GetCurrentThreadId());
		}
		{
			std::scoped_lock lock(g_simLock);  // the physics thread reads these while you move
			LoadConfig();
			Sounds::BuildMaterials();
			if (g_cfg.record) {
				Record::Start(g_cfg.recordSeconds);  // the next seconds of play go to BHOP_record.csv
			}
		}
		Perks::Resolve();
		Sounds::InstallFootstepHook();
		AnimSpeed::Clear();  // comes back by itself within a moment if it's still wanted
	}

	class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (!a_event.opening && (a_event.menuName == "PauseMenu"sv || a_event.menuName == "MainMenu"sv)) {
				if (auto task = F4SE::GetTaskInterface()) {
					task->AddTask([]() { ReloadSettings(); });
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	} g_menuSink;

	void OnMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case F4SE::MessagingInterface::kGameDataReady:
			Perks::Resolve();
			Sounds::Init();
			if (auto ui = RE::UI::GetSingleton()) {
				ui->RegisterSink<RE::MenuOpenCloseEvent>(&g_menuSink);
			}
			break;
		case F4SE::MessagingInterface::kPreSaveGame:
			AnimSpeed::Clear();  // never saved into your save file
			break;
		case F4SE::MessagingInterface::kPostLoadGame:
		case F4SE::MessagingInterface::kNewGame:
			ResetRuntimeState();
			ScheduleInstall(600);
			if (auto task = F4SE::GetTaskInterface()) {
				task->AddTask([]() {
					Camera::Install();
					AutoJump::Install();
					CrouchKey::Install();
				});
			}
			break;
		default:
			break;
		}
	}
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);
	LoadConfig();
	if (auto messaging = F4SE::GetMessagingInterface()) {
		messaging->RegisterListener(OnMessage);
	}
	REX::INFO("Boston Hopping Overhaul Project loaded");
	return true;
}
