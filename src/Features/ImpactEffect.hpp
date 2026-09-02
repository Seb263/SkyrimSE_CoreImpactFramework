#pragma once

#include "API/ModAPI-Legacy.h"

#include "DataHandler.hpp"

#include "Core/Structure.h"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"
#include "Utils/NativeUtils.hpp"

class ImpactEffect
{
public:

	static RE::BGSImpactData* GetOriginalImpactData(CoreStructure::DeferredHitStruct& deferredHit, RE::BGSImpactDataSet* originalImpactDataSet = nullptr)
	{
		using namespace ModData;

		RE::Actor* victim = deferredHit.victimHandle.get() ? deferredHit.victimHandle.get().get() : nullptr;
		if (!victim) return nullptr;
		
		RE::Actor* attacker = deferredHit.attackerHandle.get() ? deferredHit.attackerHandle.get().get() : nullptr;

		RE::TESRace* targetRace = victim->GetRace();
		if (!targetRace || !targetRace->bloodImpactMaterial) return nullptr;

		RE::BGSMaterialType* victimMaterial = targetRace->bloodImpactMaterial;
		auto matIt = originalMaterialsReversed.find(targetRace->bloodImpactMaterial);
		if (matIt != originalMaterialsReversed.end()) victimMaterial = matIt->second;
		if (!victimMaterial) return nullptr;

		RE::BGSImpactDataSet* impactSet = originalImpactDataSet;
		if (!originalImpactDataSet) {
			const auto hitDataFlags = deferredHit.extraHitData.hitDataFlags;
			const auto weapon = deferredHit.extraHitData.weaponBase;
			
			if (hitDataFlags.any(RE::HitData::Flag::kBash) || hitDataFlags.any(RE::HitData::Flag::kBlocked)) {
				if (weapon) impactSet = weapon->blockBashImpactDataSet;
				else {
					if (!attacker) return nullptr;

					if (RE::TESObjectARMO* shield = ModUtils::GetBashShield(attacker)) {
						impactSet = shield->blockBashImpactDataSet;
					} else {
						impactSet = defaultUnarmedWeap->blockBashImpactDataSet ? defaultUnarmedWeap->blockBashImpactDataSet : defaultUnarmedWeap->impactDataSet;
					}
				}
			} else if (weapon && weapon == defaultUnarmedWeap) {
				if (!attacker) return nullptr;

				auto aggressorRace = attacker->GetRace();
				if (!aggressorRace || !aggressorRace->impactDataSet) return nullptr;

				auto impactIt = aggressorRace->impactDataSet->impactMap.find(victimMaterial);
				return (impactIt != aggressorRace->impactDataSet->impactMap.end()) ? impactIt->second : nullptr;
			} else if (weapon) {
				impactSet = weapon->impactDataSet;
			}
		}
		if (!impactSet) return nullptr;

		if (auto setIt = originalImpactMap.find(impactSet); setIt != originalImpactMap.end()) {
			if (auto matIt = setIt->second.find(victimMaterial); matIt != setIt->second.end()) {
				return matIt->second;
			}
		} else {
			auto matIt = impactSet->impactMap.find(victimMaterial);
			return (matIt != impactSet->impactMap.end()) ? matIt->second : nullptr;
		}
		
		return nullptr;
	}

	static void PlayImpactEffect(CoreStructure::DeferredHitStruct& deferredHit)
	{
		using namespace ModData;

		auto* victim = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.victimHandle);
		if (!victim) return;

		auto &impactModifier = deferredHit.modifiers.impactModifier;
		const bool isMaterialModifierValid = impactModifier.has_value();

		const auto magicEffect = deferredHit.extraHitData.effectBase;
		RE::BGSImpactDataSet* originalImpactDataSet = (magicEffect && magicEffect->data.impactDataSet ? magicEffect->data.impactDataSet : nullptr);
		
		RE::BGSImpactData* originalImpactData = GetOriginalImpactData(deferredHit, originalImpactDataSet);
		RE::BGSImpactData* impactData = isMaterialModifierValid ? impactModifier->impactData : nullptr;

		if (!impactData) impactData = originalImpactData;
		if (!impactData) return;

		ModRuntime_Impact->model = impactData->model;
		ModRuntime_Impact->data = impactData->data;
		ModRuntime_Impact->decalTextureSet = impactData->decalTextureSet;
		ModRuntime_Impact->decalTextureSet2 = impactData->decalTextureSet2;
		ModRuntime_Impact->sound1 = nullptr;
		ModRuntime_Impact->sound2 = nullptr;
		ModRuntime_Impact->hazard = impactData->hazard;
		ModRuntime_Impact->dData = impactData->dData;
		ModRuntime_Impact->padAC = impactData->padAC;

		if (isMaterialModifierValid) {
			if (impactData == weaponDefaultImpactData ||
				impactData == weaponWoodImpactData ||
				impactData == shieldHeavyImpactData ||
				impactData == shieldLightImpactData) {
				impactModifier->impactBounce = true;
			}

			RE::ImpactResult impactResult = (impactModifier->impactBounce.value_or(false) ? RE::ImpactResult::kBounce : RE::ImpactResult::kNone);
			if (impactModifier.has_value()) ModRuntime_Impact->data.resultOverride = impactResult;

			if (impactModifier->removeDecal.value_or(false)) {
				ModRuntime_Impact->decalTextureSet = nullptr;
				ModRuntime_Impact->decalTextureSet2 = nullptr;
				ModRuntime_Impact->dData = RE::DecalData{};
			} else if (impactModifier->decalOverride) {
				ModRuntime_Impact->decalTextureSet = impactModifier->decalOverride->decalTextureSet;
				ModRuntime_Impact->decalTextureSet2 = impactModifier->decalOverride->decalTextureSet2;
				ModRuntime_Impact->dData = impactModifier->decalOverride->dData;
			} else if (impactModifier->preserveOriginalDecal.value_or(false) && originalImpactData) {
				ModRuntime_Impact->decalTextureSet = originalImpactData->decalTextureSet;
				ModRuntime_Impact->decalTextureSet2 = originalImpactData->decalTextureSet2;
				ModRuntime_Impact->dData = originalImpactData->dData;
			}
		}
		if (!deferredHit.runtimeReady) return;

		// Disable non-arrow not sticky projectiles
		if (auto* projectile = MiscUtils::ResolveHandle<RE::Projectile>(deferredHit.extraHitData.sourceHandle)) {
			auto& projectileRuntime = projectile->GetProjectileRuntimeData();
			
			if (!projectileRuntime.ammoSource &&
				(impactData->data.resultOverride.any(RE::ImpactResult::kBounce) || impactData->data.resultOverride.any(RE::ImpactResult::kDestroy))) {
				projectileRuntime.flags.set(RE::Projectile::Flags::kDestroyed);
			}
		}

		if (deferredHit.isMagic) {
			SpawnParticle(victim, ModRuntime_Impact, deferredHit.extraHitData.hitPosition, deferredHit.extraHitData.hitDirection);
		}

		if (isMaterialModifierValid) {
			PlayExtraEffects(deferredHit, impactModifier);
		}

		if (!isMaterialModifierValid || !impactModifier->removeSound.value_or(false)) {
			if (isMaterialModifierValid && impactModifier->soundOverride) {
				NativeUtils::PlaySound(impactModifier->soundOverride, 1.0f, victim, deferredHit.extraHitData.hitPosition);
			} else {
				auto* attacker = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.attackerHandle);
				auto* soundImpactData = (isMaterialModifierValid && impactModifier->preserveOriginalSound.value_or(false) && originalImpactData ? originalImpactData : impactData);
				RE::BGSImpactManager::ImpactSoundData soundData{
					.impactData = soundImpactData,
					.position = &deferredHit.extraHitData.hitPosition,
					.objectToFollow = nullptr,
					.sound1 = nullptr,
					.sound2 = nullptr,
					.playSound1 = true,
					.playSound2 = attacker && attacker->IsPlayerRef(),
					.lowPriority = false,
					.pool = nullptr
				};
				RE::BGSImpactManager::GetSingleton()->PlayImpactDataSounds(soundData);
			}
		}

		SKSE::GetTaskInterface()->AddTask([]() {
			ModRuntime_Impact->data.resultOverride = RE::ImpactResult::kNone;
		});
	}

	template <typename T>
	static void PlayExtraEffects(CoreStructure::DeferredHitStruct& deferredHit, const std::optional<T> modifier)
	{
		auto* victim = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.victimHandle);
		if (!victim || !modifier) return;

		SpawnExtraImpactData(victim, modifier->extraImpactData, deferredHit.extraHitData.hitPosition, deferredHit.extraHitData.hitDirection);

		for (auto* extraSound : modifier->extraSound) {
			if (extraSound) NativeUtils::PlaySound(extraSound, 1.0f, victim, deferredHit.extraHitData.hitPosition);
		}
	}

	static void SpawnExtraImpactData(RE::TESObjectREFR* ref, const std::vector<std::variant<std::string, RE::BGSImpactData*>>& extraImpactData,
		const RE::NiPoint3& position, const RE::NiPoint3& direction)
	{
		for (const auto& entry : extraImpactData) {
			std::visit([&](const auto& val) {
				using T = std::decay_t<decltype(val)>;
				if constexpr (std::is_same_v<T, RE::BGSImpactData*>) {
					if (val) SpawnParticle(ref, val, position, direction);
				} else if constexpr (std::is_same_v<T, std::string>) {
					SpawnParticle(ref, val.c_str(), position, direction);
				}
			}, entry);
		}
	}

	static bool SpawnParticle(RE::TESObjectREFR* ref, const char* model, const RE::NiPoint3 position, const RE::NiPoint3 direction)
	{
		if (!ref || !model) return false;

		RE::TESObjectCELL* cell = ref->GetParentCell();
		if (!cell) return false;

		RE::NiAVObject* avObject = ref->Get3D();
		if (!avObject) return false;

		return RE::BSTempEffectParticle::Spawn(cell, 3.0f, model, direction, position, 1.0f, 7, avObject);
	}

	static bool SpawnParticle(RE::TESObjectREFR* ref, RE::BGSImpactData* impact, const RE::NiPoint3 position, const RE::NiPoint3 direction)
	{
		if (!ref || !impact) return false;

		return SpawnParticle(ref, impact->GetModel(), position, direction);
	}
};
