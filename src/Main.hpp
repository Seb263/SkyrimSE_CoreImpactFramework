#pragma once

#include "DataHandler.hpp"

#include "Core/Structure.h"
#include "Core/DeferredHit.hpp"

#include "Features/BloodSpray.hpp"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"

namespace Events
{
	using namespace ModData;

	class MainEvent
	{
	public:

		// Initialization of hooks and template functions
		static void InstallHitHook()
		{
			SKSE::AllocTrampoline(1 << 8);
			auto& trampoline = SKSE::GetTrampoline();
				
			_ProcessHit = trampoline.write_call<5>(REL::RelocationID(37673, 38627).address() + REL::Relocate(0x3C0, 0x4A8), ProcessHitTemplate);
			logger::info("ProcessHit hooked at address: 0x{:X}", _ProcessHit.address());

			_CheckAddEffect = trampoline.write_call<5>(REL::RelocationID(33763, 34547).address() + REL::Relocate(0x4A3, 0x656, 0x427), CheckAddEffectTemplate);
			logger::info("CheckAddEffectTemplate hooked at address: 0x{:X}", _CheckAddEffect.address());

			REL::Relocation<std::uintptr_t> arrowProjectileVtbl{ RE::VTABLE_ArrowProjectile[0] };
			REL::Relocation<std::uintptr_t> missileProjectileVtbl{ RE::VTABLE_MissileProjectile[0] };

			_AddImpactArrow = arrowProjectileVtbl.write_vfunc(REL::Relocate(0xBD, 0xBD, 0xBE), AddImpactArrowTemplate);
			logger::info("AddImpactArrow hooked at address: 0x{:X}", _AddImpactArrow.address());

			_AddImpactMissile = missileProjectileVtbl.write_vfunc(REL::Relocate(0xBD, 0xBD, 0xBE), AddImpactMissileTemplate);
			logger::info("AddImpactMissile hooked at address: 0x{:X}", _AddImpactMissile.address());

			_OnMissile3DLoaded = missileProjectileVtbl.write_vfunc(REL::Relocate(0xC0, 0xC0, 0xC1), OnMissile3DLoadedTemplate);
			logger::info("OnMissile3DLoaded hooked at address: 0x{:X}", _OnMissile3DLoaded.address());
		}

		// This event triggers deferred hits in post-runtime
		static void TESHitEvent(RE::Actor* victim)
		{
			if (!victim) return;

			std::jthread([victimFormID = victim->formID]() {
				std::this_thread::sleep_for(FRAME_DELAY());
				SKSE::GetTaskInterface()->AddTask([victimFormID]() {
					if (auto deferredHitOpt = DeferredHitFunctions::ReadDeferredHit(victimFormID)) {
						TRACE("TESHitEvent on actor \"{:08X}\"", victimFormID);
						DeferredHitFunctions::ProcessDeferredHitNormalized(*deferredHitOpt);
						DeferredHitFunctions::DestroyDeferredHitMap(victimFormID);
					}
				});
			}).detach();
		}

		// Used to resolve magical projectiles in post-runtime
		static void TESHitProjectileEvent(RE::Actor* victim, RE::BGSProjectile* projectileBase)
		{
			if (!victim || !projectileBase || projectileBase->IsArrow()) return;

			auto deferredHit = DeferredHitFunctions::ReadDeferredHit(victim->formID);
			if (deferredHit) return;

			TRACE("TESHitProjectileEvent on actor \"{:08X}\"", victim->formID);

			MainEvent::TryResolveProjectileHit(projectileBase, victim);
			deferredHit = DeferredHitFunctions::ReadDeferredHit(victim->formID);
		}

		// Used to trigger the DeathMapping
		static void TESDeathEvent(RE::Actor* victim)
		{
			if (!victim) return;

			auto deferredHit = DeferredHitFunctions::ReadDeferredHit(victim->formID);
			if (!deferredHit) return;
				
			TRACE("TESDeathEvent on actor \"{:08X}\"", victim->formID);

			deferredHit->ctx.state = CoreStructure::Filter::StateFilter::kDying;
			DeferredHitFunctions::SetDeferredHitMap(victim->formID, *deferredHit);
		}

	private:

		// Runtime event triggered by a direct physical impact (weapons and arrows)
		static void ProcessHitTemplate(RE::Actor* victim, RE::HitData& hitData)
		{
			if (victim) {
				TRACE("ProcessHitTemplate on actor \"{:08X}\"", victim->formID);
				DeferredHitFunctions::SetDeferredHit(hitData, true);
				DeferredHitFunctions::ProcessHitNormalized(victim->formID, &hitData);
			}

			_ProcessHit(victim, hitData);
		}
		static inline REL::Relocation<decltype(ProcessHitTemplate)> _ProcessHit;

		// Pre-runtime event triggered when a MagicEffect is added to an NPC's effect stack
		static void CheckAddEffectTemplate(RE::ActiveEffect* a_this, float a_power, bool a_onlyHostile)
		{
			if (a_this) ProcessActiveEffect(a_this, a_power);

			_CheckAddEffect(a_this, a_power, a_onlyHostile);
		}
		static inline REL::Relocation<decltype(CheckAddEffectTemplate)> _CheckAddEffect;

		// Allows runtime projectiles (blood sprays) to be identified, ensuring they are never retained in save data
		static void OnMissile3DLoadedTemplate(RE::Projectile* a_projectile)
		{
			_OnMissile3DLoaded(a_projectile);
			
			if (!a_projectile) return;

			if (auto* projectileBase = a_projectile->GetProjectileBase();
				projectileBase && projectileBase->data.collisionLayer == ModRuntimeBlood_CollisionLayer) {
				a_projectile->inGameFormFlags.set(RE::TESForm::InGameFormFlag::kWantsDelete, RE::TESForm::InGameFormFlag::kRefPermanentlyDeleted);
			}
		}
		static inline REL::Relocation<decltype(OnMissile3DLoadedTemplate)> _OnMissile3DLoaded;

		// Pre-runtime event triggered by an arrow impact on an NPC
		static void AddImpactArrowTemplate(RE::ArrowProjectile* a_projectile, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc, const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6, std::uint32_t a_arg7)
		{
			if (a_projectile && a_ref) {
				if (RE::Actor* victim = a_ref->As<RE::Actor>()) {
					TRACE("AddImpactArrowTemplate on actor \"{:08X}\"", victim->formID);

					auto* collidableObj = a_collidable ? RE::TESHavokUtilities::FindCollidableObject(*a_collidable) : nullptr;
					const auto nodeName = collidableObj ? std::optional<std::string>(collidableObj->name.c_str()) : std::nullopt;

					DeferredHitFunctions::SetDeferredHitProjectile({ a_projectile, a_targetLoc, a_velocity, nodeName }, victim, false);
					DeferredHitFunctions::ProcessHitNormalized(victim->formID, std::nullopt);
				}
			}

			return _AddImpactArrow(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
		}
		static inline REL::Relocation<decltype(AddImpactArrowTemplate)> _AddImpactArrow;

		// Event triggered by missile-type spells (used only to apply a fix)
		static void AddImpactMissileTemplate(RE::MissileProjectile* a_projectile, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc, const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6, std::uint32_t a_arg7)
		{
			if (a_projectile && SettingsIni::bBloodSprayImpactWorkaroundEnabled) {
				if (auto* projectileBase = a_projectile->GetProjectileBase();
					projectileBase && projectileBase->data.collisionLayer == ModRuntimeBlood_CollisionLayer) {
					BloodSpray::ImpactWorkaround(a_projectile);
					return;
				}
			}

			return _AddImpactMissile(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
		}
		static inline REL::Relocation<decltype(AddImpactMissileTemplate)> _AddImpactMissile;

		// Function to find a a_projectile reference from its source form
		static void TryResolveProjectileHit(RE::BGSProjectile* projectileBase, RE::Actor* victim)
		{
			if (!projectileBase || !victim) return;

			auto find_projectile = [&](const RE::BSTArray<RE::ProjectileHandle>& projectiles) -> RE::Projectile* {
				for (auto& projectileHandle : projectiles) {
					if (!projectileHandle) continue;

					RE::NiPointer projectilePtr = projectileHandle.get();
					if (!projectilePtr) continue;

					RE::Projectile* projectile = projectilePtr.get();
					if (!projectile || projectile->GetProjectileBase() != projectileBase || !projectile->Is3DLoaded()) continue;

					return projectile;
				}
				return nullptr;
			};

			auto projectileManager = RE::Projectile::Manager::GetSingleton();
			if (!projectileManager) REPORT_AND_FAIL("Failed to retrieve Projectile Manager singleton.");

			RE::Projectile* projectile = find_projectile(projectileManager->pending);
			if (!projectile) projectile = find_projectile(projectileManager->unlimited);
			if (!projectile) return;

			DeferredHitFunctions::SetDeferredHitProjectile({ projectile }, victim, true);
			DeferredHitFunctions::ProcessHitNormalized(victim->formID, std::nullopt);
		}

		// Only for FireAndForget spells and staves - Does not trigger if the darget is already dead
		static void ProcessActiveEffect(RE::ActiveEffect* activeEffect, float effectPower)
		{
			using namespace CoreStructure;

			if (!activeEffect || !activeEffect->spell) return;
			auto* magicItemBase = activeEffect->spell;

			if (!activeEffect->effect || !activeEffect->effect->baseEffect || !activeEffect->effect->baseEffect->data.projectileBase) return;
			auto* projectileBase = activeEffect->effect->baseEffect->data.projectileBase;

			if (auto* enchItem = magicItemBase->As<RE::EnchantmentItem>()) {
				if (enchItem->data.delivery != RE::MagicSystem::Delivery::kAimed) return;
			}

			RE::Actor* attacker = MiscUtils::ResolveHandle<RE::Actor>(activeEffect->caster);
			RE::Actor* victim = MiscUtils::GetValidReference<RE::Actor>(activeEffect->GetVisualsTarget());
			if (!victim || !victim->Is3DLoaded() || (attacker && !attacker->Is3DLoaded())) return;

			auto getDeferredHit = [&]() -> std::optional<DeferredHitStruct> {
				auto deferredHitOpt = DeferredHitFunctions::ReadDeferredHit(victim->formID);
				if (deferredHitOpt) return deferredHitOpt;

				DeferredHitStruct newHit{};
				newHit.extraHitData.magicItemBase = magicItemBase;
				newHit.extraHitData.projectileBase = projectileBase;

				for (const auto& effect : activeEffect->spell->effects) {
					auto* base = effect->baseEffect;
					if (!base) continue;

					if (!base->IsDetrimental() || base->data.castingType != RE::MagicSystem::CastingType::kFireAndForget) continue;

					if (!base->HasArchetype(RE::EffectSetting::Archetype::kValueModifier) &&
						!base->HasArchetype(RE::EffectSetting::Archetype::kDualValueModifier)) continue;

					if (!ModUtils::TryCheckConditions(base, victim, attacker)) continue;

					newHit.activeEffectsMap[effect] = DeferredHitStruct::EffectEntry{};
				}
					
				if (newHit.activeEffectsMap.size() == 0) return std::nullopt;

				DeferredHitFunctions::SetDeferredHitMap(victim->formID, newHit);
				return newHit;
			};

			auto deferredHitOpt = getDeferredHit();
			if (!deferredHitOpt) return;
			DeferredHitStruct deferredHit = *deferredHitOpt;

			auto it = deferredHit.activeEffectsMap.find(activeEffect->effect);
			if (it == deferredHit.activeEffectsMap.end()) return;

			it->second.activeEffect = activeEffect;
			it->second.power = effectPower;

			DeferredHitFunctions::SetDeferredHitMap(victim->formID, deferredHit);

			const bool allSet = std::all_of(deferredHit.activeEffectsMap.begin(), deferredHit.activeEffectsMap.end(),
				[](const auto& pair) { return pair.second.activeEffect != nullptr; });

			if (!allSet) return;

			TRACE("ProcessActiveEffect on actor \"{:08X}\"", victim->formID);

			TryResolveProjectileHit(deferredHit.extraHitData.projectileBase, victim);
		}
	};
};
