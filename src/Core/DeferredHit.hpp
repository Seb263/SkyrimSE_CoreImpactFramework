#pragma once

#include "Core/Structure.h"
#include "Core/Biped.hpp"
#include "Core/Filters.hpp"
#include "Core/Modifiers.hpp"

#include "Features/Bloodpool.hpp"
#include "Features/BloodSpray.hpp"
#include "Features/Dismember.hpp"
#include "Features/DamageModifier.hpp"
#include "Features/ImpactEffect.hpp"
#include "Features/MiscEffect.hpp"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"
#include "Utils/PredictiveDamageUtils.hpp"
#include "Utils/TimeUtils.hpp"

#include "API/ModAPIInternal.h"

class DeferredHitFunctions
{
public:

	// Set Deferred Hit from Projectile
	static void SetDeferredHit(RE::Projectile* projectile, RE::Actor* victimRef, const bool runtimeReady)
	{
		// NOTE: Damage cannot reliably account for critical hits.
		if (!projectile || !victimRef) return;

		auto& projectileRuntime = projectile->GetProjectileRuntimeData();

		auto* victim = MiscUtils::GetValidReference<RE::Actor>(victimRef);
		auto* attacker = MiscUtils::ResolveHandle<RE::Actor>(projectileRuntime.shooter);
		if (!attacker) return;

		RE::HitData hitData{};
		hitData.hitPosition = projectile->GetPosition();
		if (projectileRuntime.velocity != RE::NiPoint3{}) (hitData.hitDirection = projectileRuntime.velocity).Unitize();
		else (hitData.hitDirection = hitData.hitPosition - ModUtils::GetActorFireNodePosition(attacker)).Unitize();

		if (!projectileRuntime.ammoSource) { // Magic only
			RE::NiPoint3 impactPosition{};
			RE::hkVector4 impactNormal{};
			if (ModUtils::RayCastSpellActor(attacker, victim, projectile->data.location, projectile->data.angle, impactPosition, impactNormal)) {
				hitData.hitPosition = impactPosition;
				hitData.hitDirection = MiscUtils::HkVector4ToNiPoint3(impactNormal);
			} else if (victim) {
				hitData.hitPosition = ModUtils::GetHitboxImpactPosition(victim, projectile);
			}
		}

		if (victim->IsBlocking()) {
			hitData.flags.set(RE::HitData::Flag::kBlocked);
			RE::TESObjectARMO* blockWeapon = victim->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kShield);
			if (!blockWeapon) hitData.flags.set(RE::HitData::Flag::kBlockWithWeapon);
		}

		hitData.target = victim->GetHandle();
		hitData.aggressor = attacker->GetHandle();
		hitData.sourceRef = projectile->GetHandle();
		hitData.weapon = projectileRuntime.weaponSource;

		hitData.bonusHealthDamageMult = 1.0f;
		hitData.criticalDamageMult = 1.0f;
		if (!runtimeReady && attacker->IsSneaking() && victim->RequestDetectionLevel(attacker) < 1) {
			hitData.flags.set(RE::HitData::Flag::kSneakAttack);
			hitData.bonusHealthDamageMult = PredictiveDamageUtils::GetSneakAttackMult(attacker, victim, projectileRuntime.weaponSource);
			hitData.sneakAttackBonus = MiscUtils::GetGameSetting<float>("fCombatSneakAttackBonusMult", 100.0f);
		}

		hitData.totalDamage = projectileRuntime.weaponDamage;
		if (projectileRuntime.ammoSource) { // Arrow only
			hitData.totalDamage = PredictiveDamageUtils::ComputePredictedArrowDamage(projectile, attacker, victim);
		}
		hitData.totalDamage *= hitData.bonusHealthDamageMult * hitData.criticalDamageMult;
		hitData.physicalDamage = hitData.totalDamage;

		SetDeferredHit(hitData, runtimeReady);
	}

	// Set Deferred Hit from Hit Data
	static void SetDeferredHit(RE::HitData& hitData, const bool runtimeReady)
	{
		using namespace CoreStructure;

		if (runtimeReady) CIF_API::Internal::InvokePreHitCallbacks(hitData);

		auto* victim = MiscUtils::ResolveHandle<RE::Actor>(hitData.target);
		auto* attacker = MiscUtils::ResolveHandle<RE::Actor>(hitData.aggressor);
		if (!victim || !attacker) return;

		const auto start = std::chrono::high_resolution_clock::now();
		TRACE("Setting and running of Deferred Hit [{:08X} => {:08X}]...", attacker->formID, victim->formID);

		auto deferredHitOpt = DeferredHitFunctions::ReadDeferredHit(victim->formID);
		auto deferredHit = deferredHitOpt ? *deferredHitOpt : DeferredHitStruct{};
		auto& extraHitData = deferredHit.extraHitData;

		if (runtimeReady) deferredHit.runtimeReady = true;

		deferredHit.victimHandle = hitData.target;
		deferredHit.attackerHandle = hitData.aggressor;

		if (extraHitData.hitPosition == RE::NiPoint3{}) extraHitData.hitPosition = hitData.hitPosition;
		if (extraHitData.hitDirection == RE::NiPoint3{}) (extraHitData.hitDirection = hitData.hitDirection).Unitize();
		extraHitData.hitDataFlags = hitData.flags;
		extraHitData.weaponBase = hitData.weapon;
		deferredHit.bipedBones = BipedFunctions::GetBipedModifier(victim).value_or(BipedMapping::BipedBonesMap{});
		deferredHit.randomSeed = static_cast<uint32_t>(MiscUtils::GetRandomNumber(0.0f, static_cast<float>(UINT32_MAX)));

		if (hitData.sourceRef) { // Ranged & magic
			extraHitData.sourceHandle = hitData.sourceRef;
			if (auto* projectile = MiscUtils::ResolveHandle<RE::Projectile>(hitData.sourceRef)) {
				extraHitData.projectileBase = projectile->GetProjectileBase();

				auto& projectileRuntime = projectile->GetProjectileRuntimeData();
				if (projectileRuntime.ammoSource) { // Arrow
					extraHitData.weaponType = Filter::WeaponType::kRanged;
					extraHitData.hitPower = projectileRuntime.power;
				} else { // Magic
					deferredHit.isMagic = true;
					extraHitData.weaponType = Filter::WeaponType::kMagic;
					extraHitData.effectBase = projectileRuntime.avEffect;
					extraHitData.magicItemBase = projectileRuntime.spell ? projectileRuntime.spell->As<RE::SpellItem>() : nullptr;
					extraHitData.spellMagnitude = projectileRuntime.weaponDamage;
				}
			}
		} else { // Melee
			extraHitData.weaponType = FiltersFunctions::GetWeaponType(hitData.weapon, attacker->GetRace());
			extraHitData.hitPower = (hitData.flags.any(RE::HitData::Flag::kPowerAttack) ? 1.5f : 1.0f);

			if (!hitData.weapon && hitData.flags.any(RE::HitData::Flag::kBash)) {
				extraHitData.weaponBase = ModUtils::GetBashWeapon(attacker);
			}
		}

		if (extraHitData.weaponBase && !extraHitData.magicItemBase) {
			if (extraHitData.weaponBase->formEnchanting) extraHitData.magicItemBase = extraHitData.weaponBase->formEnchanting;
		}

		const float damageMult = DamageModifier::GetModifiersDamageMultiplier(deferredHit.modifiers);
		if (deferredHit.isMagic) DamageModifier::UpdateCalculatedMagicDamage(deferredHit, damageMult);
		else DamageModifier::UpdateCalculatedPhysicalDamage(deferredHit, hitData, damageMult);

		ModifiersFunctions::BuildRuntimeContext(deferredHit, hitData);
		ModifiersFunctions::GetModifiers(deferredHit);
				
		const float damageMultUpdate = DamageModifier::GetModifiersDamageMultiplier(deferredHit.modifiers);
		if (damageMult != damageMultUpdate) {
			if (deferredHit.isMagic) DamageModifier::UpdateCalculatedMagicDamage(deferredHit, damageMultUpdate);
			else DamageModifier::UpdateCalculatedPhysicalDamage(deferredHit, hitData, damageMultUpdate);

			ModifiersFunctions::BuildRuntimeContext(deferredHit, hitData);
			ModifiersFunctions::GetModifiers(deferredHit);
		}

		deferredHit.ctx.damageMult = deferredHit.damageMult.value_or(1.0f);
		deferredHit.ctx.damageCalc = deferredHit.calculatedDamage.value_or(0.0f);

		DamageModifier::UpdateCalculatedLimbDamage(deferredHit);
		deferredHit.ctx.damageLimbMult = deferredHit.damageLimbMult.value_or(1.0f);

		if (deferredHit.runtimeReady && debugVerboseMode > 1) {
			ModifiersFunctions::TraceRuntimeContext(deferredHit);
			TraceModifierDamage(victim, deferredHit);
		}

		SetDeferredHitMap(victim->formID, deferredHit);

		const auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed = end - start;
		TRACE("Setting and running of Deferred Hit [{:08X} => {:08X}]: DONE after {} seconds", attacker->formID, victim->formID, elapsed.count());
	}

	static void ProcessHitNormalized(const RE::FormID victimFormID, std::optional<RE::HitData*> hitData = std::nullopt)
	{
		using namespace CoreStructure;

		RE::Actor* victim = RE::TESForm::LookupByID<RE::Actor>(victimFormID);
		if (!victim) return;

		auto deferredHitOpt = DeferredHitFunctions::ReadDeferredHit(victim->formID);
		auto deferredHit = deferredHitOpt ? *deferredHitOpt : DeferredHitStruct{};

		auto& impactModifier = deferredHit.modifiers.impactModifier;
		auto& hitModifiers = deferredHit.modifiers.hitModifiers;
		const auto hitDataFlags = deferredHit.extraHitData.hitDataFlags;

		bool removeDefaultBloodSpray = false;

		ImpactEffect::PlayImpactEffect(deferredHit);

		if (deferredHit.runtimeReady) {
			if (impactModifier) {
				if (SettingsIni::bBloodSprayStatus && impactModifier->bloodSpray) {
					if (BloodSpray::Initialize(deferredHit, impactModifier)) removeDefaultBloodSpray = true;
				} else if (impactModifier->impactData == ModData::weaponDefaultImpactData ||
						 impactModifier->impactData == ModData::weaponWoodImpactData ||
						 impactModifier->impactData == ModData::shieldHeavyImpactData ||
						 impactModifier->impactData == ModData::shieldLightImpactData) {
					removeDefaultBloodSpray = true;
				}

				if (impactModifier->removeBloodSplatter.value_or(false)) removeDefaultBloodSpray = true;

				MiscEffect::Initialize(deferredHit, impactModifier);
				Dismember::Initialize(deferredHit, impactModifier);
				Bloodpool::Initialize(deferredHit, impactModifier);
			}

			for (const auto& [className, hitModifier] : hitModifiers) {
				if (!hitModifier || hitModifier->deferred) continue;

				ImpactEffect::PlayExtraEffects(deferredHit, hitModifier);
				MiscEffect::Initialize(deferredHit, hitModifier);
				Dismember::Initialize(deferredHit, hitModifier);
				Bloodpool::Initialize(deferredHit, hitModifier);

				if (SettingsIni::bBloodSprayStatus && hitModifier->bloodSpray) {
					removeDefaultBloodSpray = true;
					BloodSpray::Initialize(deferredHit, hitModifier);
				}
			}

			if (!removeDefaultBloodSpray && !hitDataFlags.any(RE::HitData::Flag::kBash) &&
				!hitDataFlags.any(RE::HitData::Flag::kBlocked) && !deferredHit.isMagic) {
				BloodSpray::InitializeInstant(deferredHit);
			}

			if (deferredHit.damageMult.has_value()) {
				DamageModifier::Initialize(deferredHit, hitData);
			}

			CIF_API::Internal::InvokePostHitCallbacks(deferredHit.ctx);
		}
	}

	static void ProcessDeferredHitNormalized(CoreStructure::DeferredHitStruct deferredHit)
	{
		auto* victim = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.victimHandle);
		if (!victim || !deferredHit.runtimeReady) return;

		using StateFilter = CoreStructure::Filter::StateFilter;
		if ( deferredHit.ctx.state != StateFilter::kKillmove && !victim->IsDead()) deferredHit.ctx.state = CoreStructure::Filter::StateFilter::kAlive;

		deferredHit.ctx.limbHealth = 100.0f; // Future implementation

		ModifiersFunctions::GetModifiers(deferredHit, true);
		auto& hitModifiers = deferredHit.modifiers.hitModifiers;
		
		if (debugVerboseMode > 1) ModifiersFunctions::TraceRuntimeContext(deferredHit, true);

		for (const auto& [className, hitModifier] : hitModifiers) {
			if (!hitModifier || !hitModifier->deferred) continue;

			ImpactEffect::PlayExtraEffects(deferredHit, hitModifier);
			MiscEffect::Initialize(deferredHit, hitModifier);
			Dismember::Initialize(deferredHit, hitModifier);
			Bloodpool::Initialize(deferredHit, hitModifier);

			if (SettingsIni::bBloodSprayStatus && hitModifier->bloodSpray) {
				BloodSpray::Initialize(deferredHit, hitModifier);
			}
		}

		CIF_API::Internal::InvokePostDeferredHitCallbacks(deferredHit.ctx);
	}

	static std::optional<CoreStructure::DeferredHitStruct> ReadDeferredHit(RE::FormID victimFormID)
	{
		std::shared_lock readLock(deferredHitMapMutex);

		if (auto it = deferredHitMap.find(victimFormID); it != deferredHitMap.end()) {
			return it->second;
		}

		return std::nullopt;
	}

	static void SetDeferredHitMap(RE::FormID victimFormID, CoreStructure::DeferredHitStruct& deferredHit)
	{
		const auto timestamp = std::chrono::steady_clock::now();
		{
			std::lock_guard lock(deferredHitMapMutex);
			deferredHit.timestamp = timestamp;
			deferredHitMap[victimFormID] = deferredHit;
		}

		std::jthread([victimFormID, timestamp]() {
			std::this_thread::sleep_for(100ms);
			SKSE::GetTaskInterface()->AddTask([victimFormID, timestamp]() {
				DestroyDeferredHitMap(victimFormID, timestamp);
			});
		}).detach();
	}

	static void DestroyDeferredHitMap(RE::FormID victimFormID, std::optional<std::chrono::steady_clock::time_point> timestamp = std::nullopt)
	{
		std::lock_guard lock(deferredHitMapMutex);

		if (auto it = deferredHitMap.find(victimFormID); it != deferredHitMap.end()) {
			if (!timestamp || it->second.timestamp == timestamp) {
				deferredHitMap.erase(it);
			}
		}
	}

	static void TraceModifierDamage(RE::Actor* victim, const CoreStructure::DeferredHitStruct& deferredHit)
	{
		RE::ActorValueOwner* victimAV = victim->AsActorValueOwner();
		if (!victimAV) return;

		TRACE("Modifier Damage : [Damage Mult:{}], [Computed Damage:{}], [Computed FINAL Health:{}]",
			deferredHit.damageMult.value_or(-1.0f),
			deferredHit.calculatedDamage.value_or(-1.0f),
			victimAV->GetActorValue(RE::ActorValue::kHealth) - deferredHit.calculatedDamage.value_or(-1.0f));

		TimeUtils::WaitAndCall(FRAME_DELAY(), [victimHandle = victim->GetHandle()](TimeUtils::CallResult result, const std::chrono::nanoseconds) {
			if (!TimeUtils::IsEnd(result)) return true;

			auto* victim = MiscUtils::ResolveHandle<RE::Actor>(victimHandle);
			if (!victim) return false;

			RE::ActorValueOwner* victimAV = victim->AsActorValueOwner();
			if (!victimAV) return false;

			const float victimBaseHealth = victimAV->GetActorValue(RE::ActorValue::kHealth);
			TRACE("Modifier Damage : [REAL FINAL health:{}]", victimBaseHealth);

			return true;
		});
	}

	static inline std::unordered_map<RE::FormID, CoreStructure::DeferredHitStruct> deferredHitMap;
	static inline std::shared_mutex deferredHitMapMutex;
};
