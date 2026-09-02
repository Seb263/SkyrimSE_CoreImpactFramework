#pragma once

#include "DataHandler.hpp"

#include "Core/Structure.h"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"
#include "Utils/PredictiveDamageUtils.hpp"

class DamageModifier
{
public:

	static void Initialize(CoreStructure::DeferredHitStruct& deferredHit, std::optional<RE::HitData*> hitData = std::nullopt)
	{
		const float damageMult = deferredHit.damageMult.value_or(1.0f);
		if (damageMult == 1.0f) return;

		AlterDamage(deferredHit, hitData.value_or(nullptr));
	}

	static void UpdateCalculatedLimbDamage(CoreStructure::DeferredHitStruct& deferredHit)
	{
		float damageLimbMult = 1.0f;

		if (auto& impactModifier = deferredHit.modifiers.impactModifier) {
			damageLimbMult *= ModUtils::ResolveVariant(impactModifier->damageLimbMult, 1.0f);
		}

		for (const auto& [className, hitModifier] : deferredHit.modifiers.hitModifiers) {
			if (!hitModifier) continue;
			damageLimbMult *= ModUtils::ResolveVariant(hitModifier->damageLimbMult, 1.0f);
		}

		deferredHit.damageLimbMult = damageLimbMult;
	}

	static float GetModifiersDamageMultiplier(CoreStructure::DeferredHitStruct::Modifiers& modifiers)
	{
		float damageMult = 1.0f;

		if (auto& impactModifier = modifiers.impactModifier) {
			damageMult *= ModUtils::ResolveVariant(impactModifier->damageMult, 1.0f);
		}

		for (const auto& [className, hitModifier] : modifiers.hitModifiers) {
			if (!hitModifier) continue;
			damageMult *= ModUtils::ResolveVariant(hitModifier->damageMult, 1.0f);
		}

		return damageMult;
	}

	static void UpdateCalculatedPhysicalDamage(CoreStructure::DeferredHitStruct& deferredHit, RE::HitData& hitData, const float damageMult = 1.0f)
	{
		const float initialDamage = hitData.totalDamage;

		auto* victim = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.victimHandle);
		if (!victim) return;

		float numWornPieces = 0.0f;
		for (const auto& [invItem, data] : victim->GetInventory()) {
			const auto& [count, entry] = data;
			if (count > 0 && invItem->IsArmor() && entry->IsWorn()) {
				if (auto armor = invItem->As<RE::TESObjectARMO>()) {
					numWornPieces++;
				}
			}
		}
		
		float finalDamage;
		if (deferredHit.runtimeReady) {
			finalDamage = initialDamage * damageMult;
		} else {
			const float damageResistancePercent = (victim->CalcArmorRating() * 0.12f) + (3.0f * numWornPieces);
			const float baseDamage = initialDamage * ((100.0f - damageResistancePercent) / 100.0f);
			finalDamage = baseDamage * damageMult;
		}

		deferredHit.calculatedDamage = finalDamage;
		deferredHit.damageMult = damageMult;
	}

	static void UpdateCalculatedMagicDamage(CoreStructure::DeferredHitStruct& deferredHit, const float damageMult)
	{
		auto* victim = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.victimHandle);
		auto* attacker = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.attackerHandle);
		if (!victim) return;

		auto* victimMagicTarget = victim->AsMagicTarget();
		if (!victimMagicTarget) return;

		RE::MagicItem* spell = deferredHit.extraHitData.magicItemBase;
		if (!spell) return;

		float totalHealthDamage = 0.0f;
		for (const auto& [effect, entry] : deferredHit.activeEffectsMap) {
			if (!effect || !entry.activeEffect) continue;

			auto* effectBase = effect && effect->baseEffect ? effect->baseEffect : nullptr;
			if (!effectBase) continue;

			const float adjustedMagnitude = PredictiveDamageUtils::ComputeRealMagnitude(entry.activeEffect, attacker, victim, RE::ActorValue::kHealth);

			TRACE("Effect: {:08X}, Power: {}, Magnitude: {}, Duration: {}", effectBase->formID, entry.power, adjustedMagnitude, entry.activeEffect->duration);
			
			totalHealthDamage += adjustedMagnitude;
		}

		const float finalDamage = totalHealthDamage * damageMult;
		deferredHit.calculatedDamage = finalDamage;

		deferredHit.damageMult = damageMult;
	}

	static float GetFinalCalculatedDamage(const float initialDamage, RE::Actor* aggressor, RE::Actor* victim, RE::HitData& hitData)
	{
		if (initialDamage == 0.0f) return 0.0f;

		float calculatedDamage = ModUtils::OffsetRealDamage(initialDamage, aggressor, victim);

		TRACE("GetFinalCalculatedDamage: [Initial Damage:{}], [Final Damage:{}]", initialDamage, calculatedDamage);

		return calculatedDamage;
	}

private:

	static void AlterDamage(CoreStructure::DeferredHitStruct& deferredHit, RE::HitData* hitData = nullptr)
	{
		const float damageMult = deferredHit.damageMult.value_or(1.0f);
		if (damageMult == 1.0f) return;

		// Physic
		if (hitData) hitData->totalDamage *= damageMult;

		// Magic
		for (auto& [effect, entry] : deferredHit.activeEffectsMap) {
			if (!entry.activeEffect) continue;
			entry.activeEffect->magnitude *= damageMult;
		}
	}
};
