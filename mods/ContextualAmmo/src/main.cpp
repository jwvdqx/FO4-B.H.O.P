#include <RE/Fallout.h>
#include <F4SE/F4SE.h>

#include <atomic>
#include <fstream>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "Formula.h"

using namespace std::literals;

namespace
{
	// ---- the one switch -------------------------------------------------------------------------
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

	// ---- vanilla perks (Fallout4.esm; every rank is its own perk form) --------------------------
	constexpr RE::TESFormID kScrounger[] = { 0x0004A0B0, 0x001ACF9A, 0x001ACF9B, 0x001EB99C };
	constexpr RE::TESFormID kDemolitionExpert4 = 0x00065E13;

	bool HasPerk(RE::Actor* a_actor, RE::TESFormID a_id)
	{
		auto perk = RE::TESForm::GetFormByID<RE::BGSPerk>(a_id);
		return perk && a_actor->GetPerkRank(perk) > 0;
	}

	int ScroungerRank(RE::Actor* a_actor)
	{
		for (int i = 3; i >= 0; --i) {
			if (HasPerk(a_actor, kScrounger[i])) {
				return i + 1;
			}
		}
		return 0;
	}

	// ---- explosives -----------------------------------------------------------------------------
	float ExplosionDamage(const RE::BGSProjectile* a_proj)
	{
		return a_proj && a_proj->data.explosionType ? a_proj->data.explosionType->data.damage : 0.0F;
	}

	float ExplosionDamage(RE::TESObjectWEAP* a_weap, RE::TESObjectWEAP::InstanceData* a_inst = nullptr)
	{
		const RE::TESAmmo* ammo = a_inst ? a_inst->ammo : a_weap->weaponData.ammo;
		return ExplosionDamage(RE::TESObjectWEAP::GetProjectile(a_weap, ammo, a_inst));
	}

	bool IsGrenade(RE::TESObjectWEAP* a_weap) { return a_weap && a_weap->IsThrownWeapon(); }
	bool IsHeavy(RE::TESObjectWEAP* a_weap) { return ExplosionDamage(a_weap) >= Formula::kHeavyExplosionDamage; }

	// Nuke mine explosion, found by data (strongest mine explosion) so it works in any language.
	RE::BGSExplosion* g_nukeBlast = nullptr;

	void FindNukeBlast()
	{
		auto dh = RE::TESDataHandler::GetSingleton();
		if (!dh) {
			return;
		}
		float best = 0.0F;
		for (auto weap : dh->GetFormArray<RE::TESObjectWEAP>()) {
			if (weap && weap->weaponData.type == RE::WEAPON_TYPE::kMine) {
				auto proj = RE::TESObjectWEAP::GetProjectile(weap, weap->weaponData.ammo, nullptr);
				if (const float d = ExplosionDamage(proj); d > best) {
					best = d;
					g_nukeBlast = proj->data.explosionType;
				}
			}
		}
	}

	// ---- per-victim hit tracking ----------------------------------------------------------------
	struct HitLog
	{
		float         grenadeDamage = 0.0F;
		RE::TESFormID lastWeapon = 0;
	};
	std::mutex                                   g_lock;
	std::unordered_map<RE::TESFormID, HitLog> g_hits;

	struct Stack
	{
		RE::TESBoundObject* object;
		std::int32_t        count;
	};

	// The victim's own grenades (light ones only for payouts; all of them for the chain blast).
	std::vector<Stack> Grenades(RE::Actor* a_victim, bool a_includeHeavy)
	{
		std::vector<Stack> out;
		auto list = a_victim->inventoryList;
		if (!list) {
			return out;
		}
		const RE::BSAutoReadLock l{ list->rwLock };
		for (auto& item : list->data) {
			auto weap = item.object ? item.object->As<RE::TESObjectWEAP>() : nullptr;
			if (IsGrenade(weap) && (a_includeHeavy || !IsHeavy(weap))) {
				out.push_back({ item.object, static_cast<std::int32_t>(item.GetCount()) });
			}
		}
		return out;
	}

	void PlaceAt(RE::TESObjectREFR* a_ref, RE::TESForm* a_form)
	{
		auto game = RE::GameVM::GetSingleton();
		auto vm = game ? game->GetVM() : nullptr;
		if (!vm || !a_form) {
			return;
		}
		const auto handle = vm->GetObjectHandlePolicy().GetHandleForObject(RE::Actor::TYPE_ID, a_ref);
		vm->DispatchMethodCall(handle, "ObjectReference", "PlaceAtMe", RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>{},
			a_form, static_cast<std::int32_t>(1), false, false, true);
	}

	// ---- payout ---------------------------------------------------------------------------------
	void Pay(RE::Actor* a_victim, HitLog a_log)
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!g_enabled || !player || !a_victim) {
			return;
		}
		float maxHealth = 0.0F;
		if (auto avs = RE::ActorValue::GetSingleton(); avs && avs->health) {
			maxHealth = a_victim->GetPermanentActorValue(*avs->health);
		}
		const double threat = Formula::Threat(a_victim->GetLevel(), maxHealth);
		auto         killer = RE::TESForm::GetFormByID<RE::TESObjectWEAP>(a_log.lastWeapon);

		// Gun kill: ammo of the equipped weapon INSTANCE (follows caliber mods), only if that gun
		// landed the kill. Heavy explosive ammo (mini nukes...) never pays.
		if (killer && killer->IsGunWeapon()) {
			RE::BGSEquipIndex  slot{ 0 };
			RE::BGSObjectInstance inst{ nullptr, nullptr };
			player->GetEquippedItem(&inst, slot);
			auto equipped = inst.object ? inst.object->As<RE::TESObjectWEAP>() : nullptr;
			auto ammo = player->GetCurrentAmmo(slot);
			auto idata = static_cast<RE::TESObjectWEAP::InstanceData*>(inst.instanceData.get());
			if (equipped == killer && ammo && ExplosionDamage(equipped, idata) < Formula::kHeavyExplosionDamage) {
				const int n = Formula::Rounds(threat, ammo->GetFormValue(), Rand01(), Rand01(), Rand01(), ScroungerRank(player));
				if (n > 0) {
					player->AddObjectToContainer(ammo, nullptr, n, nullptr, RE::ITEM_REMOVE_REASON::kNone);
				}
			}
		}

		// Grenades: needs Demolition Expert rank 4, a grenade-carrying victim, and grenades did
		// >= 20% of its max health or landed the kill.
		const bool grenadeKill = IsGrenade(killer) && !IsHeavy(killer);
		if (!HasPerk(player, kDemolitionExpert4) || !Formula::GrenadeQualifies(a_log.grenadeDamage, maxHealth, grenadeKill)) {
			return;
		}
		if (grenadeKill && Rand01() < Formula::kChainChance) {
			auto all = Grenades(a_victim, true);
			if (!all.empty() && g_nukeBlast) {
				for (auto& s : all) {
					RE::TESObjectREFR::RemoveItemData rid{ s.object, s.count };
					a_victim->RemoveItem(rid);
				}
				PlaceAt(a_victim, g_nukeBlast);
				return;  // their grenades went off: nothing for you
			}
		}
		auto light = Grenades(a_victim, false);
		if (light.empty()) {
			return;  // this enemy can't use grenades
		}
		const auto& pick = light[static_cast<std::size_t>(Rand01() * light.size()) % light.size()];
		if (const int n = Formula::Grenades(threat, Rand01()); n > 0) {
			player->AddObjectToContainer(pick.object, nullptr, n, nullptr, RE::ITEM_REMOVE_REASON::kNone);
		}
	}

	// ---- events ---------------------------------------------------------------------------------
	class HitSink : public RE::BSTEventSink<RE::TESHitEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent& a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
		{
			auto player = RE::PlayerCharacter::GetSingleton();
			if (!g_enabled || !player || a_event.cause.get() != player || !a_event.target) {
				return RE::BSEventNotifyControl::kContinue;
			}
			auto weap = RE::TESForm::GetFormByID<RE::TESObjectWEAP>(a_event.sourceFormID);
			if (!weap) {
				return RE::BSEventNotifyControl::kContinue;
			}
			std::scoped_lock l{ g_lock };
			if (g_hits.size() > 1024) {
				g_hits.clear();
			}
			auto& log = g_hits[a_event.target->GetFormID()];
			log.lastWeapon = weap->GetFormID();
			if (IsGrenade(weap) && !IsHeavy(weap)) {
				log.grenadeDamage += a_event.usesHitData ? a_event.hitData.totalDamage : ExplosionDamage(weap);
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	} g_hitSink;

	class DeathSink : public RE::BSTEventSink<RE::TESDeathEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent& a_event, RE::BSTEventSource<RE::TESDeathEvent>*) override
		{
			auto player = RE::PlayerCharacter::GetSingleton();
			if (!g_enabled || !player || a_event.actorKiller.get() != player || !a_event.actorDying) {
				return RE::BSEventNotifyControl::kContinue;
			}
			auto victim = a_event.actorDying->As<RE::Actor>();
			if (!victim || victim == player) {
				return RE::BSEventNotifyControl::kContinue;
			}
			HitLog log;
			{
				// taking the entry out also stops the second (dying/dead) event paying twice
				std::scoped_lock l{ g_lock };
				auto it = g_hits.find(victim->GetFormID());
				if (it == g_hits.end()) {
					return RE::BSEventNotifyControl::kContinue;
				}
				log = it->second;
				g_hits.erase(it);
			}
			RE::NiPointer<RE::TESObjectREFR> keep = a_event.actorDying;
			if (auto task = F4SE::GetTaskInterface()) {
				task->AddTask([keep, log]() { Pay(keep->As<RE::Actor>(), log); });
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
			FindNukeBlast();
			if (auto src = RE::TESHitEvent::GetEventSource()) {
				src->RegisterSink(&g_hitSink);
			}
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
			{
				std::scoped_lock l{ g_lock };
				g_hits.clear();
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
	LoadSwitch();
	if (auto messaging = F4SE::GetMessagingInterface()) {
		messaging->RegisterListener(OnMessage);
	}
	REX::INFO("Contextual Ammo loaded");
	return true;
}
