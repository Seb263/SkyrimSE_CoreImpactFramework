#include "Events.h"

namespace Events
{
	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESLoadGameEvent* event, RE::BSTEventSource<RE::TESLoadGameEvent>*)
	{
		ModData::lastLoadPoint = std::chrono::steady_clock::now();

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESHitEvent* event, RE::BSTEventSource<RE::TESHitEvent>*)
	{
		if (!event) return continueEvent;

		auto* victim = MiscUtils::ResolveHandle<RE::Actor>(event->target);
		if (!victim) return continueEvent;
		
		MainEvent::TESHitEvent(victim);

		auto* projectileBase = event->projectile ? RE::TESForm::LookupByID<RE::BGSProjectile>(event->projectile) : nullptr;
		if (!projectileBase) return continueEvent;

		MainEvent::TESHitProjectileEvent(victim, projectileBase);

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESDeathEvent* event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		if (!event || event->dead) return continueEvent;

		auto* victim = MiscUtils::ResolveHandle<RE::Actor>(event->actorDying);
		if (!victim) return continueEvent;

		MainEvent::TESDeathEvent(victim);

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::MenuOpenCloseEvent* event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
	{
		if (event->menuName == RE::JournalMenu::MENU_NAME && !event->opening) {
			SettingsIni::TDMSettingsManager().ReadSettings();
		}

		return continueEvent;
	}
}
