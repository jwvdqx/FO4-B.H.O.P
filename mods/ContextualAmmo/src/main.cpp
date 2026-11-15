#include <RE/Fallout.h>
#include <F4SE/F4SE.h>

#include <array>
#include <atomic>
#include <fstream>
#include <random>
#include <string>

#include "Formula.h"

using namespace std::literals;

namespace
{
	std::atomic_bool g_enabled{ true };

	void LoadSwitch()
	{
		std::ifstream in("Data/F4SE/Plugins/ContextualAmmo.ini");
		for (std::string line; std::getline(in, line);) {
			if (line.rfind("Enabled", 0) == 0) {
				const auto eq = line.find('=');
				if (eq != std::string::npos) {
					const auto v = line.find_first_not_of(" \t", eq + 1);
					g_enabled = v == std::string::npos || line[v] != '0';
				}
			}
		}
	}

	double Rand01()
	{
		static std::mt19937 rng{ std::random_device{}() };
		return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
	}

	// TESDeathEvent can fire twice for one death (dying, then dead): remember recent victims.
	std::array<std::uint32_t, 16> g_recent{};
	std::size_t                   g_recentPos = 0;

	bool AlreadyPaid(std::uint32_t a_id)
	{
		for (auto id : g_recent) {
			if (id == a_id) {
				return true;
			}
		}
		g_recent[g_recentPos++ % g_recent.size()] = a_id;
		return false;
	}

	void Pay(RE::Actor* a_victim)
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!g_enabled || !player || !a_victim || AlreadyPaid(a_victim->GetFormID())) {
			return;
		}
		// Ammo of the equipped weapon INSTANCE: follows caliber mods (e.g. a service rifle
		// switched to .308 gives .308). Melee/unarmed have none -> nothing.
		RE::BGSEquipIndex slot{ 0 };
		auto              ammo = player->GetCurrentAmmo(slot);
		if (!ammo) {
			return;
		}
		float maxHealth = 0.0F;
		if (auto avs = RE::ActorValue::GetSingleton(); avs && avs->health) {
			maxHealth = a_victim->GetPermanentActorValue(*avs->health);
		}
		const double threat = Formula::Threat(a_victim->GetLevel(), maxHealth);
		const int    count = Formula::Rounds(threat, ammo->GetFormValue(), Rand01(), Rand01(), Rand01());
		if (count > 0) {
			player->AddObjectToContainer(ammo, nullptr, count, nullptr, RE::ITEM_REMOVE_REASON::kNone);
		}
	}

	class DeathSink : public RE::BSTEventSink<RE::TESDeathEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent& a_event, RE::BSTEventSource<RE::TESDeathEvent>*) override
		{
			auto player = RE::PlayerCharacter::GetSingleton();
			if (g_enabled && player && a_event.actorKiller.get() == player && a_event.actorDying) {
				if (auto victim = a_event.actorDying->As<RE::Actor>(); victim && victim != player) {
					RE::NiPointer<RE::TESObjectREFR> keep = a_event.actorDying;
					if (auto task = F4SE::GetTaskInterface()) {
						task->AddTask([keep]() { Pay(keep->As<RE::Actor>()); });
					}
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	} g_deathSink;

	class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (!a_event.opening && a_event.menuName == "PauseMenu"sv) {
				LoadSwitch();
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	} g_menuSink;

	void OnMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case F4SE::MessagingInterface::kGameDataReady:
			if (auto src = RE::TESDeathEvent::GetEventSource()) {
				src->RegisterSink(&g_deathSink);
			}
			if (auto ui = RE::UI::GetSingleton()) {
				ui->RegisterSink<RE::MenuOpenCloseEvent>(&g_menuSink);
			}
			break;
		case F4SE::MessagingInterface::kPostLoadGame:
		case F4SE::MessagingInterface::kNewGame:
			LoadSwitch();
			break;
		default:
			break;
		}
	}
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);
	LoadSwitch();
	if (auto messaging = F4SE::GetMessagingInterface()) {
		messaging->RegisterListener(OnMessage);
	}
	REX::INFO("Contextual Ammo loaded");
	return true;
}
