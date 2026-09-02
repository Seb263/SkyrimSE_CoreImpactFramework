#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Structure.h"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"
#include "Utils/NativeUtils.hpp"
#include "Utils/TimeUtils.hpp"

class BloodSpray
{
public:

	template <typename T>
	static bool Initialize(CoreStructure::DeferredHitStruct& deferredHit, const std::optional<T> modifier)
	{
		using namespace ModData;

		auto* victim = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.victimHandle);
		if (!victim || !modifier) return false;

		if (victim->GetActorBase() && !victim->GetActorBase()->Bleeds()) return false;

		RE::SpellItem* originalSpell = modifier->bloodSpray;
		if (!originalSpell || originalSpell->effects.empty()) return false;

		static std::unordered_map<RE::Effect*, RE::TESCondition> customConditionsMap;
		static std::unordered_map<RE::SpellItem*, std::vector<RE::SpellItem*>> splitSpellsCache;
		static std::mutex splitSpellsCacheMutex;

		std::vector<RE::SpellItem*>* splitSpellsPtr = nullptr;

		{
			std::lock_guard<std::mutex> lock(splitSpellsCacheMutex);
			splitSpellsPtr = &splitSpellsCache[originalSpell];
			auto& splitSpells = *splitSpellsPtr;

			if (splitSpells.empty()) {
				RE::BSTArray<RE::Effect*> effectsCopy;
				effectsCopy.reserve(originalSpell->effects.size() + 1);
				effectsCopy.push_back(originalSpell->effects[0]);
				effectsCopy.push_back(originalSpell->effects[0]);
				for (RE::BSTArray<RE::Effect*>::size_type i = 1; i < originalSpell->effects.size(); ++i) {
					effectsCopy.push_back(originalSpell->effects[i]);
				}

				bool firstSpell = true;
				for (auto* effect : effectsCopy) {
					if (!effect || !effect->baseEffect) continue;

					RE::SpellItem* clonedSpell = originalSpell->CreateDuplicateForm(false, nullptr)->As<RE::SpellItem>();
					if (!clonedSpell) continue;
					clonedSpell->effects.clear();

					auto clonedEffectSetting = effect->baseEffect->CreateDuplicateForm(false, nullptr)->As<RE::EffectSetting>();
					if (!clonedEffectSetting) continue;
				
					if (!firstSpell) {
						auto originalImpactDataSet = clonedEffectSetting->data.impactDataSet;
						if (originalImpactDataSet) {
							auto clonedImpactDataSet = originalImpactDataSet->CreateDuplicateForm(false, nullptr)->As<RE::BGSImpactDataSet>();
							if (!clonedImpactDataSet) continue;
							clonedImpactDataSet->impactMap.clear();

							std::unordered_map<RE::BGSImpactData*, RE::BGSImpactData*> clonedImpactMap;

							for (auto& [material, impactData] : originalImpactDataSet->impactMap) {
								if (!impactData) continue;

								RE::BGSImpactData* clonedImpact = nullptr;
								auto it = clonedImpactMap.find(impactData);
								if (it != clonedImpactMap.end()) {
									clonedImpact = it->second;
								} else {
									clonedImpact = impactData->CreateDuplicateForm(false, nullptr)->As<RE::BGSImpactData>();
									if (!clonedImpact) continue;

									clonedImpact->decalTextureSet = impactData->decalTextureSet;
									clonedImpact->decalTextureSet2 = impactData->decalTextureSet2;
									clonedImpact->sound1 = nullptr;
									clonedImpact->sound2 = nullptr;
									clonedImpact->data = impactData->data;
									clonedImpact->dData = impactData->dData;
									clonedImpact->model = impactData->model;

									clonedImpactMap[impactData] = clonedImpact;
								}

								clonedImpactDataSet->impactMap.insert({ material, clonedImpact });
							}

							clonedEffectSetting->data.impactDataSet = clonedImpactDataSet;
						}
					} else firstSpell = false;

					auto originalProjectile = clonedEffectSetting->data.projectileBase;
					if (!originalProjectile) continue;

					auto clonedProjectile = originalProjectile->CreateDuplicateForm(false, nullptr)->As<RE::BGSProjectile>();
					if (!clonedProjectile) continue;
					clonedProjectile->model = originalProjectile->model;
					clonedProjectile->data.speed = originalProjectile->data.speed;
					clonedProjectile->data.gravity = originalProjectile->data.gravity;
					clonedProjectile->data.collisionLayer = ModRuntimeBlood_CollisionLayer;
					clonedEffectSetting->data.projectileBase = clonedProjectile;

					auto newEffect = new RE::Effect();
					newEffect->conditions.head;
					newEffect->cost = 0.0f;
					newEffect->baseEffect = clonedEffectSetting;
					newEffect->effectItem.magnitude = 0.0f;
					newEffect->effectItem.area = 0;
					newEffect->effectItem.duration = 0;
					if (effect->conditions && effect->conditions.head) {
						customConditionsMap[newEffect] = effect->conditions;
					}

					clonedSpell->effects.push_back(newEffect);
					splitSpells.push_back(clonedSpell);
				}
			}
		}

		auto& splitSpells = *splitSpellsPtr;
		bool spellWasCasted = false;

		const float speedMultiplier = MiscUtils::GetRandomNumber(SettingsIni::fBloodSpraySpeedFactor * 0.85f, SettingsIni::fBloodSpraySpeedFactor / 0.85f);
		const float gravityMultiplier = MiscUtils::GetRandomNumber(SettingsIni::fBloodSprayGravityFactor * 0.85f, SettingsIni::fBloodSprayGravityFactor / 0.85f);

		auto ApplyBloodSpray = [&](const bool firstCall) {
			float velocityFactor = 1.0f;
			for (size_t i = 0; i < splitSpells.size(); ++i) {
				if ((firstCall && i == 1) || (!firstCall && i == 0)) continue;

				auto* spell = splitSpells[i];
				if (!spell || spell->effects.empty() || !spell->effects[0] || !spell->effects[0]->baseEffect) return;

				auto it = customConditionsMap.find(spell->effects[0]);
				if (it != customConditionsMap.end()) {
					if (!it->second.IsTrue(victim, victim)) return;
				}

				CastBloodSpray(victim, spell, deferredHit.extraHitData.hitPosition, deferredHit.extraHitData.hitDirection,
					deferredHit.extraHitData.hitPower * velocityFactor, SettingsIni::fBloodSpraySpread, speedMultiplier, gravityMultiplier);

				velocityFactor *= SettingsIni::fBloodSprayVelocityDecayFactor;
				spellWasCasted = true;
			}
		};

		for (int i = 0; i < SettingsIni::iBloodSpraySpawnMult; ++i) {
			ApplyBloodSpray(i == 0);
		}

		return spellWasCasted;
	}

	static void InitializeInstant(CoreStructure::DeferredHitStruct& deferredHit)
	{
		using namespace ModData;

		auto* victim = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.victimHandle);
		if (!victim) return;

		if (ModData::default_fCombatEnvironmentBloodChance < MiscUtils::GetRandomNumber()) return;

		if (victim->GetActorBase() && !victim->GetActorBase()->Bleeds()) return;

		RE::TESRace* race = victim ? victim->GetRace() : nullptr;
		if (!race || race->data.flags.any(RE::RACE_DATA::Flag::kDontShowBloodSpray) ||
			race->data.flags.any(RE::RACE_DATA::Flag::kDontShowBloodDecal) || race->data.flags.any(RE::RACE_DATA::Flag::kDontShowWeaponBlood)) return;
		if (!race->bodyPartData || !race->bodyPartData->parts) return;

		auto part = race->bodyPartData->parts[RE::BGSBodyPartDefs::LIMB_ENUM::kTorso];
		if (!part) return;

		ModRuntimeBloodSpray_Effect->data.impactDataSet = (part->data.dismemberImpactDataSet ?
			part->data.dismemberImpactDataSet : part->data.explosionImpactDataSet);

		RE::SpellItem* spell = ModData::ModRuntimeBloodSpray_Spell;
		if (!spell) return;

		CastBloodSpray(victim, spell, deferredHit.extraHitData.hitPosition, deferredHit.extraHitData.hitDirection, MiscUtils::GetRandomNumber(0.2f, 1.0f));
	}

	static bool CastBloodSpray(RE::TESObjectREFR* caster, RE::SpellItem* spell, RE::NiPoint3 position, RE::NiPoint3 direction,
		const float power = 1.0f, const float spread = -1.0f, const float speedMultiplier = 1.0f, const float gravityMultiplier = 1.0f)
	{
		if (!caster || !spell) return false;
		if (spell->effects.empty() || !spell->effects[0] || !spell->effects[0]->baseEffect) return false;
		
		auto* projectile = spell->effects[0]->baseEffect->data.projectileBase;
		if (!projectile) return false;

		direction.Unitize();
		RE::NiPoint3 finalDirection = MiscUtils::ApplySpreadToDirection(direction, (spread >= 0.0f ? spread : SettingsIni::fBloodSpraySpread));

		SetBloodProjectileData(projectile, power, speedMultiplier, gravityMultiplier);

		(!caster->As<RE::Actor>() || SettingsIni::iUseAlternateBloodSprayAlgorithm == 2 ||
			(SettingsIni::iUseAlternateBloodSprayAlgorithm == 1 && SettingsIni::bTDMTargetLockMissileFeatureEnabled)) ?
			CastBloodSprayAlternate(spell, caster, position, position + finalDirection) :
			CastBloodSprayRegular(spell, caster->As<RE::Actor>(), position, MiscUtils::DirToAngles(finalDirection));

		return true;
	}

	static void ImpactWorkaround(RE::Projectile* projectile)
	{
		if (!projectile) return;

		auto& projectileRuntime = projectile->GetProjectileRuntimeData();
		if (projectileRuntime.flags.any(RE::Projectile::Flags::kDestroyed)) return;

		auto direction = projectileRuntime.velocity;
		direction.Unitize();

		auto* spell = CreateWorkaroundImpactSpell(projectile);
		if (!spell) return;

		CastBloodSprayAlternate(spell, projectile, projectile->GetPosition(), projectile->GetPosition() + direction);
		
		projectileRuntime.flags.set(RE::Projectile::Flags::kDestroyed);
	}

private:

	static void CastBloodSprayRegular(RE::SpellItem* spell, RE::Actor* shooter, const RE::NiPoint3 position, const RE::Projectile::ProjectileRot rotation)
	{
		if (!spell || !shooter) return;

		SKSE::GetTaskInterface()->AddTask([=, shooterFormID = shooter->formID]() { // AddTask otherwise, crash in VR
			auto* shooter = RE::TESForm::LookupByID<RE::Actor>(shooterFormID);
			
			if (!spell || !shooter || !shooter->Is3DLoaded()) return;

			const float speedMultiplier = MiscUtils::GetRandomNumber(SettingsIni::fBloodSpraySpeedFactor * 0.85f, SettingsIni::fBloodSpraySpeedFactor / 0.85f);

			RE::ProjectileHandle projectileHandle;
			RE::Projectile::LaunchSpell(&projectileHandle, shooter, spell, position, rotation);
			if (auto* genProjectile = projectileHandle ? projectileHandle.get().get() : nullptr) {
				genProjectile->inGameFormFlags.set(RE::TESForm::InGameFormFlag::kWantsDelete, RE::TESForm::InGameFormFlag::kRefPermanentlyDeleted);
			}
		});
	}

	static void CastBloodSprayAlternate(RE::SpellItem* spell, RE::TESObjectREFR* shooter, const RE::NiPoint3 position, const RE::NiPoint3 direction)
	{
		if (!spell || !shooter) return;

		RE::TESObjectREFR* markerSource = NativeUtils::PlaceAtMe(shooter, ModData::ModRuntimeMarker_Explosion, position);
		RE::TESObjectREFR* markerTarget = NativeUtils::PlaceAtMe(shooter, ModData::ModRuntimeMarker_Explosion, direction);
		if (!markerSource || !markerTarget) return;

		SKSE::GetTaskInterface()->AddTask([spell, markerSourceHandle = markerSource->GetHandle(), markerTargetHandle = markerTarget->GetHandle()]() {
			auto* markerSource = MiscUtils::GetValidReference(MiscUtils::ResolveHandle(markerSourceHandle));
			auto* markerTarget = MiscUtils::GetValidReference(MiscUtils::ResolveHandle(markerTargetHandle));
			if (!markerSource || !markerTarget) return;
			
			auto* caster = markerSource->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
			if (caster && markerTarget && markerTarget->parentCell && markerTarget->parentCell->IsAttached()) {
				caster->CastSpellImmediate(spell, true, markerTarget, 0.0f, true, 0.0f, nullptr);
			}

			TimeUtils::WaitAndCall(300ms, [markerSourceHandle, markerTargetHandle](TimeUtils::CallResult result, const std::chrono::nanoseconds) {
				if (result != TimeUtils::CallResult::kEndDone) return true;
				
				auto* markerSource = MiscUtils::GetValidReference(MiscUtils::ResolveHandle(markerSourceHandle));
				auto* markerTarget = MiscUtils::GetValidReference(MiscUtils::ResolveHandle(markerTargetHandle));
				if (!markerSource || !markerTarget) return true;

				markerSource->inGameFormFlags.set(RE::TESForm::InGameFormFlag::kWantsDelete, RE::TESForm::InGameFormFlag::kRefPermanentlyDeleted);
				markerTarget->inGameFormFlags.set(RE::TESForm::InGameFormFlag::kWantsDelete, RE::TESForm::InGameFormFlag::kRefPermanentlyDeleted);
				
				return true;
			});
		});
	}

	static void SetBloodProjectileData(RE::BGSProjectile* bloodSprayProjectile, const float power, const float speedMultiplier, const float gravityMultiplier)
	{
		if (!bloodSprayProjectile) return;

		static std::unordered_map<RE::BGSProjectile*, std::pair<float, float>> initialValues;
		static std::mutex initialValuesMutex;

		{
			std::lock_guard<std::mutex> lock(initialValuesMutex);

			if (initialValues.find(bloodSprayProjectile) == initialValues.end()) {
				initialValues[bloodSprayProjectile] = { bloodSprayProjectile->data.speed, bloodSprayProjectile->data.gravity };
			}

			const float baseSpeed = initialValues[bloodSprayProjectile].first;
			const float baseGravity = initialValues[bloodSprayProjectile].second;

			bloodSprayProjectile->data.speed = baseSpeed * speedMultiplier * power;
			bloodSprayProjectile->data.gravity = baseGravity * gravityMultiplier;
		}
	}

	static RE::SpellItem* CreateWorkaroundImpactSpell(RE::Projectile* projectile)
	{
		static std::unordered_map<std::uint64_t, RE::SpellItem*> g_bloodSpellMap;
		static std::mutex g_bloodSpellMapMutex;

		if (!projectile || !projectile->GetProjectileRuntimeData().avEffect) return nullptr;

		auto* originProjectileBase = projectile->GetProjectileBase();
		auto* originImpactSet = projectile->GetProjectileRuntimeData().avEffect->data.impactDataSet;
		if (!originProjectileBase || !originImpactSet) return nullptr;

		auto key = (static_cast<std::uint64_t>(originProjectileBase->GetFormID()) << 32) | originImpactSet->GetFormID();

		{
			std::lock_guard<std::mutex> lock(g_bloodSpellMapMutex);
			if (auto it = g_bloodSpellMap.find(key); it != g_bloodSpellMap.end()) return it->second;

			const auto projectileFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSProjectile>();
			auto* projectileBase = projectileFactory ? projectileFactory->Create() : nullptr;  
			if (!projectileBase) REPORT_AND_FAIL("Failed to initialize mod runtime blood projectile.");

			projectileBase->data.flags.set(RE::BGSProjectileData::BGSProjectileFlags::kPassSMTransparent);
			projectileBase->data.types = RE::BGSProjectileData::Type::kBeam;
			projectileBase->data.gravity = 0.0f;
			projectileBase->data.speed = 10000.0f;
			projectileBase->data.range = 512.0f;
			projectileBase->data.light = nullptr;
			projectileBase->data.muzzleFlashLight = nullptr;
			projectileBase->data.tracerChance = 0.0f;
			projectileBase->data.explosionProximity = 0.0f;
			projectileBase->data.explosionTimer = 0.0f;
			projectileBase->data.explosionType = nullptr;
			projectileBase->data.activeSoundLoop = nullptr;
			projectileBase->data.muzzleFlashDuration = 0.0f;
			projectileBase->data.fadeOutTime = 0.1f;
			projectileBase->data.force = 0.0f;
			projectileBase->data.countdownSound = nullptr;
			projectileBase->data.deactivateSound = nullptr;
			projectileBase->data.defaultWeaponSource = nullptr;
			projectileBase->data.coneSpread = 0.0f;
			projectileBase->data.collisionRadius = 10.0f;
			projectileBase->data.lifetime = 0.0f;
			projectileBase->data.relaunchInterval = 0.1f;
			projectileBase->data.decalData = nullptr;
			projectileBase->data.collisionLayer = ModData::ModRuntimeBlood_CollisionLayer;
			projectileBase->soundLevel = RE::SOUND_LEVEL::kSilent;
			projectileBase->model = "Effects\\FXEmptyObject.nif";

			const auto effectFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::EffectSetting>();
			auto* effect = effectFactory ? effectFactory->Create() : nullptr;  
			if (!effect) REPORT_AND_FAIL("Failed to initialize mod runtime blood effect.");

			effect->filterValidationFunction = nullptr;
			effect->filterValidationItem = nullptr;
			effect->data = RE::EffectSetting::EffectSettingData();
			effect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kNoHitEffect);
			effect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kNoDuration);
			effect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kNoMagnitude);
			effect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kNoArea);
			effect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kFXPersist);
			effect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kHideInUI);
			effect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kNoRecast);
			effect->data.baseCost = 0.0f;
			effect->data.associatedForm = nullptr;
			effect->data.associatedSkill = RE::ActorValue::kNone;
			effect->data.resistVariable = RE::ActorValue::kNone;
			effect->data.numCounterEffects = 0;
			effect->data.light = nullptr;
			effect->data.taperWeight = 0.0f;
			effect->data.effectShader = nullptr;
			effect->data.enchantShader = nullptr;
			effect->data.minimumSkill = 0;
			effect->data.spellmakingArea = 0;
			effect->data.spellmakingChargeTime = 0.0f;
			effect->data.taperCurve = 0.0f;
			effect->data.taperDuration = 0.0f;
			effect->data.secondAVWeight = 0.0f;
			effect->data.archetype = RE::EffectArchetype::kScript;
			effect->data.primaryAV = RE::ActorValue::kNone;
			effect->data.projectileBase = projectileBase;
			effect->data.explosion = nullptr;
			effect->data.castingType = RE::MagicSystem::CastingType::kFireAndForget;
			effect->data.delivery = RE::MagicSystem::Delivery::kAimed;
			effect->data.secondaryAV = RE::ActorValue::kNone;
			effect->data.castingArt = nullptr;
			effect->data.hitEffectArt = nullptr;
			effect->data.impactDataSet = originImpactSet;
			effect->data.skillUsageMult = 1.0f;
			effect->data.dualCastData = nullptr;
			effect->data.dualCastScale = 1.0f;
			effect->data.enchantEffectArt = nullptr;
			effect->data.hitVisuals = nullptr;
			effect->data.enchantVisuals = nullptr;
			effect->data.equipAbility = nullptr;
			effect->data.imageSpaceMod = nullptr;
			effect->data.perk = nullptr;
			effect->data.castingSoundLevel = RE::SOUND_LEVEL::kSilent;
			effect->data.aiScore = 0.0f;
			effect->data.aiDelayTimer = 0.0f;
			effect->counterEffects.clear();
			effect->effectSounds.clear();
			effect->magicItemDescription = "";
			effect->effectLoadedCount = 0;
			effect->associatedItemLoadedCount = 0;
			effect->conditions = RE::TESCondition();

			const auto spellFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::SpellItem>();
			auto* spell = spellFactory ? spellFactory->Create() : nullptr; 
			if (!spell) REPORT_AND_FAIL("Failed to initialize mod runtime blood spell.");

			spell->data.costOverride = 0;
			spell->data.flags = RE::SpellItem::SpellFlag::kNone;
			spell->data.spellType = RE::MagicSystem::SpellType::kSpell;
			spell->data.chargeTime = 0.0f;
			spell->data.castingType = RE::MagicSystem::CastingType::kFireAndForget;
			spell->data.delivery = RE::MagicSystem::Delivery::kAimed;
			spell->data.castDuration = 0.0f;
			spell->data.range = 0.0f;
			spell->data.castingPerk = nullptr;

			RE::Effect* baseEffect = new RE::Effect();
			baseEffect->effectItem.magnitude = 0.0f;
			baseEffect->effectItem.area = 0;
			baseEffect->effectItem.duration = 0;
			baseEffect->cost = 0.0f;
			baseEffect->baseEffect = effect;
			baseEffect->conditions = RE::TESCondition();

			spell->effects = RE::BSTArray<RE::Effect*>();
			spell->effects.push_back(baseEffect);

			g_bloodSpellMap.emplace(key, spell);
			return spell;
		}
	}
};
