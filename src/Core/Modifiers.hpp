#pragma once

#include "Core/Structure.h"
#include "Core/Biped.hpp"
#include "Core/Filters.hpp"

#include "Features/DamageModifier.hpp"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"

class ModifiersFunctions
{
public:

	enum class ActorPerspective { kVictim, kAttacker };

	static void BuildRuntimeContext(CoreStructure::DeferredHitStruct& deferredHit, RE::HitData& hitData)
	{
		using namespace CoreStructure;

		deferredHit.ctx = {};
		auto& ctx = deferredHit.ctx;
		const auto& extra = deferredHit.extraHitData;

		// Actors

		auto* victim = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.victimHandle);
		auto* attacker = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.attackerHandle);
		if (!victim) return;

		// Biped & armor

		auto hashStr = [](const std::string& s) -> uint32_t {
			return static_cast<uint32_t>(std::hash<std::string>{}(s));
		};

		deferredHit.bipedEntry = BipedFunctions::GetHitBipedSlots(victim, extra.hitPosition, deferredHit.bipedBones);
		ctx.bipedEntry = deferredHit.bipedEntry;

		ctx.bipedLimbHash = hashStr(deferredHit.bipedEntry.bipedLimb);
		ctx.bipedNodeHash = hashStr(deferredHit.bipedEntry.bipedNode);
		for (auto s : deferredHit.bipedEntry.bipedSlots) ctx.bipedSlotHashes.push_back(static_cast<uint32_t>(s));
		for (const auto& s : deferredHit.bipedEntry.bipedKeys) ctx.bipedKeyHashes.push_back(hashStr(s));

		// Victim actor data

		RE::TESNPC* victimBase = victim->GetActorBase();
		RE::TESRace* victimRace = victim->GetRace();
		RE::TESObjectARMO* victimSkin = victim->GetSkin();

		RE::BGSMaterialType* victimRuntimeMat = victimRace ? victimRace->bloodImpactMaterial : nullptr;
		RE::BGSMaterialType* victimMaterial = (victimRuntimeMat && ModData::originalMaterialsReversed.contains(victimRuntimeMat))
			? ModData::originalMaterialsReversed[victimRuntimeMat] : nullptr;

		ctx.victimFormID = victim->formID;
		ctx.victimBaseFormID = victimBase ? victimBase->formID : 0x0;
		ctx.victimRaceFormID = victimRace ? victimRace->formID : 0x0;
		ctx.victimSkinFormID = victimSkin ? victimSkin->formID : 0x0;
		ctx.victimMaterialFormID = victimMaterial ? victimMaterial->formID : 0x0;
		ctx.victimWornFormIDs = FiltersFunctions::GetActorWornFormIDs(victim);
		ctx.victimWornKeywordsFormIDs = FiltersFunctions::GetActorWornKeywordsFormIDs(victim);
		ctx.victimKeywordsFormIDs = FiltersFunctions::GetActorKeywordsFormIDs(victim);
		ctx.victimPerksFormIDs = FiltersFunctions::GetActorPerksFormIDs(victim);
		ctx.victimSpellsFormIDs = FiltersFunctions::GetActorSpellsFormIDs(victim);
		ctx.victimMagicEffectsFormIDs = FiltersFunctions::GetMagicEffectsFormIDs(victim);
		ctx.victimSex = (victimBase && victimBase->GetSex() == RE::SEX::kFemale) ? Filter::ActorSex::kFemale : Filter::ActorSex::kMale;

		const RE::SEX victimSexIdx = ctx.victimSex == Filter::ActorSex::kFemale ? RE::SEX::kFemale : RE::SEX::kMale;
		std::string model = victimRace->skeletonModels[victimSexIdx].model.c_str();
		std::transform(model.begin(), model.end(), model.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		ctx.victimSkeletonHash = hashStr(model);

		// Attacker actor data

		if (attacker) {
			RE::TESNPC* attackerBase = attacker->GetActorBase();
			RE::TESRace* attackerRace = attacker->GetRace();
			RE::TESObjectARMO* attackerSkin = attacker->GetSkin();

			victimRace->skeletonModels[victimBase->GetSex()].model;

			RE::BGSMaterialType* attackerRuntimeMat = attackerRace ? attackerRace->bloodImpactMaterial : nullptr;
			RE::BGSMaterialType* attackerMaterial = (attackerRuntimeMat && ModData::originalMaterialsReversed.contains(attackerRuntimeMat))
				? ModData::originalMaterialsReversed[attackerRuntimeMat] : nullptr;

			ctx.attackerFormID = attacker->formID;
			ctx.attackerBaseFormID = attackerBase ? attackerBase->formID : 0x0;
			ctx.attackerRaceFormID = attackerRace ? attackerRace->formID : 0x0;
			ctx.attackerSkinFormID = attackerSkin ? attackerSkin->formID : 0x0;
			ctx.attackerMaterialFormID = attackerMaterial ? attackerMaterial->formID : 0x0;
			ctx.attackerWornFormIDs = FiltersFunctions::GetActorWornFormIDs(attacker);
			ctx.attackerWornKeywordsFormIDs = FiltersFunctions::GetActorWornKeywordsFormIDs(attacker);
			ctx.attackerKeywordsFormIDs = FiltersFunctions::GetActorKeywordsFormIDs(attacker);
			ctx.attackerPerksFormIDs = FiltersFunctions::GetActorPerksFormIDs(attacker);
			ctx.attackerSpellsFormIDs = FiltersFunctions::GetActorSpellsFormIDs(attacker);
			ctx.attackerMagicEffectsFormIDs = FiltersFunctions::GetMagicEffectsFormIDs(attacker);
			ctx.attackerSex = (attackerBase && attackerBase->GetSex() == RE::SEX::kFemale) ? Filter::ActorSex::kFemale : Filter::ActorSex::kMale;

			const RE::SEX attackerSexIdx = ctx.attackerSex == Filter::ActorSex::kFemale ? RE::SEX::kFemale : RE::SEX::kMale;
			std::string model = attackerRace->skeletonModels[attackerSexIdx].model.c_str();
			std::transform(model.begin(), model.end(), model.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
			ctx.attackerSkeletonHash = hashStr(model);
		}

		// Hit context data

		ctx.weaponFormID = extra.weaponBase ? extra.weaponBase->formID : 0x0;
		ctx.magicItemFormID = extra.magicItemBase ? extra.magicItemBase->formID : 0x0;
		ctx.projectileFormID = extra.projectileBase ? extra.projectileBase->formID : 0x0;
		ctx.magicEffectFormID = extra.effectBase ? extra.effectBase->formID : 0x0;

		RE::TESObjectARMO* wornArmor = FiltersFunctions::GetArmorFromSlots(victim, deferredHit.bipedEntry.bipedSlots);
		ctx.armorClass = FiltersFunctions::GetArmorClassType(wornArmor, deferredHit.bipedEntry.bipedSlots);
		ctx.armorFormID = wornArmor ? wornArmor->formID : 0x0;

		ctx.weaponKeywordsFormIDs = FiltersFunctions::GetItemKeywordsFormIDs(extra.weaponBase);
		ctx.armorKeywordsFormIDs = FiltersFunctions::GetItemKeywordsFormIDs(wornArmor);

		ctx.weaponType = extra.weaponType;
		const float calcDmg = DamageModifier::GetFinalCalculatedDamage(deferredHit.calculatedDamage.value_or(0.0f), attacker, victim, hitData);
		const bool wasDead = victim->IsDead();
		const float baseHp = victim->AsActorValueOwner()->GetBaseActorValue(RE::ActorValue::kHealth);
		const float currentHp = wasDead ? 0.0f : victim->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
		const bool isFatal = !victim->IsEssential()
			&& (!victim->IsProtected() || (attacker && attacker->IsPlayerRef()))
			&& (currentHp - calcDmg <= 0.0f);

		ctx.percentHealth = ((currentHp - calcDmg) / baseHp) * 100.0f;
		ctx.percentDamage = (calcDmg / baseHp) * 100.0f;
		
		ctx.limbHealth = 100.0f; // Future implementation

		ctx.state =
			wasDead ? Filter::StateFilter::kDead :
			victim->IsInKillMove() ? Filter::StateFilter::kKillmove :
			isFatal ? Filter::StateFilter::kDying :
			Filter::StateFilter::kAlive;

		ctx.attack =
			extra.hitDataFlags.any(RE::HitData::Flag::kBash) ? Filter::AttackFilter::kBash :
			extra.hitDataFlags.any(RE::HitData::Flag::kPowerAttack) ? Filter::AttackFilter::kPower :
			Filter::AttackFilter::kRegular;
		
		switch (extra.weaponType) {
			case Filter::WeaponType::kTwoHandSword:
			case Filter::WeaponType::kTwoHandAxe:
			case Filter::WeaponType::kTwoHandMace:
				ctx.source = Filter::SourceFilter::kDualHand;
				break;
			case Filter::WeaponType::kMagic:
				if (extra.magicItemBase && extra.magicItemBase->GetSpellType() == RE::MagicSystem::SpellType::kVoicePower) {
					ctx.source = Filter::SourceFilter::kShout;
				} else ctx.source = Filter::SourceFilter::kOther;
				break;
			default:
				if (auto* attackData = hitData.attackData.get()) {
					ctx.source = attackData->IsLeftAttack() ? Filter::SourceFilter::kLeftHand : Filter::SourceFilter::kRightHand;
				} else {
					ctx.source = Filter::SourceFilter::kOther;
				}
				break;
		}

		if (extra.hitDataFlags.any(RE::HitData::Flag::kBlockWithWeapon)) {
			ctx.blocked = Filter::BlockedFilter::kWeapon;
		} else if (extra.hitDataFlags.any(RE::HitData::Flag::kBlocked)) {
			RE::TESObjectARMO* shield = victim->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kShield);
			ctx.blocked = (!shield || shield->IsHeavyArmor())
				? Filter::BlockedFilter::kShieldHeavy : Filter::BlockedFilter::kShieldLight;
		} else {
			ctx.blocked = Filter::BlockedFilter::kNo;
		}

		ctx.criticalAttack = (!wasDead
			&& extra.hitDataFlags.any(RE::HitData::Flag::kCritical)
			&& !extra.hitDataFlags.any(RE::HitData::Flag::kIgnoreCritical))
			? Filter::CriticalAttackFilter::kYes : Filter::CriticalAttackFilter::kNo;

		ctx.sneakAttack = (!wasDead && extra.hitDataFlags.any(RE::HitData::Flag::kSneakAttack))
			? Filter::SneakAttackFilter::kYes : Filter::SneakAttackFilter::kNo;

		ctx.hitPosition = extra.hitPosition;
		ctx.hitDirection = extra.hitDirection;
	}

	static void GetModifiers(CoreStructure::DeferredHitStruct& deferredHit, const bool deferred = false)
	{
		using namespace CoreStructure;

		auto traceCandidates = [](const auto& candidates, const std::string& label) {
			TRACE("  [{}] - {} candidate(s)", label, candidates.size());
			for (const auto* m : candidates) {
				if constexpr (std::is_same_v<std::decay_t<decltype(*m)>, CoreStructure::ImpactMapping>) {
					TRACE("    {} id={} priority={} override={} overrideMerge={}",
						label, m->id, m->priority, m->override, m->overrideMerge);
				} else {
					TRACE("    {} id={} class='{}' priority={} deferred={} override={} overrideMerge={}",
						label, m->id, m->className, m->priority, m->modifier.deferred, m->override, m->overrideMerge);
				}
			}
		};

		deferredHit.modifiers = DeferredHitStruct::Modifiers{};

		// Default impact modifier from armor class
		switch (deferredHit.ctx.armorClass) {
			case Filter::ArmorClassType::kWeaponDefault: deferredHit.modifiers.impactModifier = weaponDefault; break;
			case Filter::ArmorClassType::kWeaponWood: deferredHit.modifiers.impactModifier = weaponWood; break;
			case Filter::ArmorClassType::kShieldHeavy: deferredHit.modifiers.impactModifier = shieldHeavy; break;
			case Filter::ArmorClassType::kShieldLight: deferredHit.modifiers.impactModifier = shieldLight; break;
			default: break;
		}

		// ImpactMapping
		if (!deferred) {
			auto impactCandidates = CollectCandidates<ImpactMapping>(impactRegistry, deferredHit);
			traceCandidates(impactCandidates, "ImpactMapping");
			for (auto* mapping : impactCandidates) {
				auto& impactModifier = deferredHit.modifiers.impactModifier;

				const bool shouldContinue = MergeImpactModifier(impactModifier, *mapping);
				if (mapping->overrideMerge && impactModifier) impactModifier->mergeWithPrevious = true;
				if (!shouldContinue) break;
			}
		}

		// HitMapping
		auto hitCandidates = CollectCandidates<HitMapping>(hitRegistry, deferredHit);
		traceCandidates(hitCandidates, "HitMapping");
		for (auto* mapping : hitCandidates) {
			auto& hitMod = deferredHit.modifiers.hitModifiers[mapping->className];
			if (hitMod && hitMod->shouldBreak) continue;

			const bool shouldContinue = MergeHitModifier(hitMod, *mapping);
			if (mapping->overrideMerge && hitMod) hitMod->mergeWithPrevious = true;
			hitMod->shouldBreak = !shouldContinue;
		}
	}

	static const std::vector<RE::FormID>* ResolveActorFilterList(uint8_t level, const CoreStructure::DeferredHitStruct::RuntimeHitContext& ctx, ActorPerspective perspective)
	{
		using namespace CoreStructure;

		const bool v = (perspective == ActorPerspective::kVictim);
		switch (static_cast<TrieLevelActor>(level)) {
			case TrieLevelActor::kKeyword: return v ? &ctx.victimKeywordsFormIDs : &ctx.attackerKeywordsFormIDs;
			case TrieLevelActor::kWorn: return v ? &ctx.victimWornFormIDs : &ctx.attackerWornFormIDs;
			case TrieLevelActor::kWornKeyword: return v ? &ctx.victimWornKeywordsFormIDs : &ctx.attackerWornKeywordsFormIDs;
			case TrieLevelActor::kPerk: return v ? &ctx.victimPerksFormIDs : &ctx.attackerPerksFormIDs;
			case TrieLevelActor::kSpell: return v ? &ctx.victimSpellsFormIDs : &ctx.attackerSpellsFormIDs;
			case TrieLevelActor::kMagicEffect: return v ? &ctx.victimMagicEffectsFormIDs : &ctx.attackerMagicEffectsFormIDs;
			default: return nullptr; // Unsupported fields
		}
	}

	static const std::vector<RE::FormID>* ResolveHitContextFilterList(uint8_t level, const CoreStructure::DeferredHitStruct::RuntimeHitContext& ctx)
	{
		using namespace CoreStructure;

		switch (static_cast<TrieLevelHitContext>(level)) {
			case TrieLevelHitContext::kWeaponKeyword: return &ctx.weaponKeywordsFormIDs;
			case TrieLevelHitContext::kArmorKeyword: return &ctx.armorKeywordsFormIDs;
			default: return nullptr; // Unsupported fields
		}
	}

	static bool MatchesActorAttributePair(uint8_t level, RE::FormID id, const CoreStructure::DeferredHitStruct::RuntimeHitContext& ctx, ActorPerspective perspective)
	{
		using namespace CoreStructure;

		const bool v = (perspective == ActorPerspective::kVictim);

		switch (static_cast<TrieLevelActor>(level)) {
			case TrieLevelActor::kRace: return (v ? ctx.victimRaceFormID : ctx.attackerRaceFormID) == id;
			case TrieLevelActor::kSkin: return (v ? ctx.victimSkinFormID : ctx.attackerSkinFormID) == id;
			case TrieLevelActor::kMaterial: return (v ? ctx.victimMaterialFormID : ctx.attackerMaterialFormID) == id;
			case TrieLevelActor::kSex: return static_cast<RE::FormID>(v ? ctx.victimSex : ctx.attackerSex) == id;
			case TrieLevelActor::kSkeleton: return (v ? ctx.victimSkeletonHash : ctx.attackerSkeletonHash) == id;
			case TrieLevelActor::kKeyword:
			case TrieLevelActor::kWorn:
			case TrieLevelActor::kWornKeyword:
			case TrieLevelActor::kPerk:
			case TrieLevelActor::kSpell:
			case TrieLevelActor::kMagicEffect: {
				const std::vector<RE::FormID>* list = ResolveActorFilterList(level, ctx, perspective);
				return list && std::ranges::contains(*list, id);
			}
			case TrieLevelActor::kFormID:
				return (v ? ctx.victimFormID : ctx.attackerFormID) == id || (v ? ctx.victimBaseFormID : ctx.attackerBaseFormID) == id;
			default: return false; // Unsupported fields
		}
	}

	static bool MatchesHitContextAttributePair(uint8_t level, RE::FormID id, const CoreStructure::DeferredHitStruct::RuntimeHitContext& ctx)
	{
		using namespace CoreStructure;

		switch (static_cast<TrieLevelHitContext>(level)) {
			case TrieLevelHitContext::kWeapon: return ctx.weaponFormID == id;
			case TrieLevelHitContext::kMagicItem: return ctx.magicItemFormID == id;
			case TrieLevelHitContext::kProjectile: return ctx.projectileFormID == id;
			case TrieLevelHitContext::kMagicEffect: return ctx.magicEffectFormID == id;
			case TrieLevelHitContext::kArmor: return ctx.armorFormID == id;
			case TrieLevelHitContext::kWeaponType: return static_cast<RE::FormID>(ctx.weaponType) == id;
			case TrieLevelHitContext::kArmorClass: return static_cast<RE::FormID>(ctx.armorClass) == id;
			case TrieLevelHitContext::kBlocked: return static_cast<RE::FormID>(ctx.blocked) == id;
			case TrieLevelHitContext::kState: return static_cast<RE::FormID>(ctx.state) == id;
			case TrieLevelHitContext::kAttack: return static_cast<RE::FormID>(ctx.attack) == id;
			case TrieLevelHitContext::kSource: return static_cast<RE::FormID>(ctx.source) == id;
			case TrieLevelHitContext::kCritical: return static_cast<RE::FormID>(ctx.criticalAttack) == id;
			case TrieLevelHitContext::kSneak: return static_cast<RE::FormID>(ctx.sneakAttack) == id;
			case TrieLevelHitContext::kBipedSlot: return std::ranges::contains(ctx.bipedSlotHashes, static_cast<uint32_t>(id));
			case TrieLevelHitContext::kBipedLimb: return ctx.bipedLimbHash == static_cast<uint32_t>(id);
			case TrieLevelHitContext::kBipedNode: return ctx.bipedNodeHash == static_cast<uint32_t>(id);
			case TrieLevelHitContext::kBipedKey: return std::ranges::contains(ctx.bipedKeyHashes, static_cast<uint32_t>(id));
			case TrieLevelHitContext::kWeaponKeyword:
			case TrieLevelHitContext::kArmorKeyword: {
				const std::vector<RE::FormID>* list = ResolveHitContextFilterList(level, ctx);
				return list && std::ranges::contains(*list, id);
			}
			default: return false;
		}
	}

	template <typename Bucket>
	static bool MatchesActorExcludeBucket(const Bucket& bucket, const CoreStructure::DeferredHitStruct::RuntimeHitContext& ctx, ActorPerspective perspective)
	{
		for (const auto& [level, id] : bucket.noneFilters) {
			if (MatchesActorAttributePair(level, id, ctx, perspective)) return true;
		}
		return false;
	}

	template <typename Bucket>
	static bool MatchesHitContextExcludeBucket(const Bucket& bucket, const CoreStructure::DeferredHitStruct::RuntimeHitContext& ctx, RE::Actor* victim, RE::Actor* attacker)
	{
		if (!victim || !attacker) return false;

		for (const auto& [level, id] : bucket.noneFilters) {
			if (MatchesHitContextAttributePair(level, id, ctx)) return true;
		}

		if (!bucket.globalesNone.empty() &&
			std::ranges::any_of(bucket.globalesNone, [](const auto& g) { return FiltersFunctions::EvaluateGlobalFilter(g); })) return true;

		if (!bucket.conditionsNone.empty() &&
			std::ranges::any_of(bucket.conditionsNone, [&](auto* perk) { return FiltersFunctions::HasValidConditionFilter(victim, attacker, perk); })) return true;

		return false;
	}

	template <typename MappingType>
	static std::unordered_set<MappingType*> GetExcludedActorMappings(const CoreStructure::ActorTrieNode<MappingType>& root, const CoreStructure::DeferredHitStruct& deferredHit, ActorPerspective perspective)
	{
		std::unordered_set<MappingType*> excluded;
		for (const auto& bucket : root.buckets) {
			if (!MatchesActorExcludeBucket(bucket, deferredHit.ctx, perspective)) continue;
			for (auto* m : bucket.mappings) excluded.insert(m);
		}
		return excluded;
	}

	template <typename MappingType>
	static std::unordered_set<MappingType*> GetExcludedHitContextMappings(const CoreStructure::HitCtxTrieNode<MappingType>& root, const CoreStructure::DeferredHitStruct& deferredHit)
	{
		std::unordered_set<MappingType*> excluded;

		RE::Actor* victim = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.victimHandle);
		RE::Actor* attacker = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.attackerHandle);

		for (const auto& bucket : root.buckets) {
			if (!MatchesHitContextExcludeBucket(bucket, deferredHit.ctx, victim, attacker)) continue;
			for (auto* m : bucket.mappings) excluded.insert(m);
		}
		return excluded;
	}

	template <typename Bucket>
	static bool PassesActorSlowFilters(const Bucket& bucket, const CoreStructure::DeferredHitStruct& deferredHit, ActorPerspective perspective)
	{
		const auto& ctx = deferredHit.ctx;

		for (const auto& [level, id] : bucket.allFilters) {
			if (!MatchesActorAttributePair(level, id, ctx, perspective)) return false;
		}

		return true;
	}

	template <typename Bucket>
	static bool PassesHitContextSlowFilters(const Bucket& bucket, const CoreStructure::DeferredHitStruct& deferredHit, RE::Actor* victim, RE::Actor* attacker)
	{
		using namespace CoreStructure;
		const auto& ctx = deferredHit.ctx;

		// Globales: OR Filter
		if (!bucket.globalesAny.empty() &&
			std::ranges::none_of(bucket.globalesAny, [](const auto& g) { return FiltersFunctions::EvaluateGlobalFilter(g); })) return false;

		// Globales: AND Filter
		if (!bucket.globalesAll.empty() &&
			!std::ranges::all_of(bucket.globalesAll, [](const auto& g) { return FiltersFunctions::EvaluateGlobalFilter(g); })) return false;

		// Globales: NOR Filter
		if (!bucket.globalesNone.empty() && 
			std::ranges::any_of(bucket.globalesNone, [](const auto& g) { return FiltersFunctions::EvaluateGlobalFilter(g); })) return false;

		// Condition: OR Filter
		if (!bucket.conditionsAny.empty() &&
			std::ranges::none_of(bucket.conditionsAny, [&](auto* perk) { return FiltersFunctions::HasValidConditionFilter(victim, attacker, perk); })) return false;

		// Condition: AND Filter
		if (!bucket.conditionsAll.empty() &&
			!std::ranges::all_of(bucket.conditionsAll, [&](auto* perk) { return FiltersFunctions::HasValidConditionFilter(victim, attacker, perk); })) return false;

		// Condition: NOR Filter
		if (!bucket.conditionsNone.empty() &&
			std::ranges::any_of(bucket.conditionsNone, [&](auto* perk) { return FiltersFunctions::HasValidConditionFilter(victim, attacker, perk); })) return false;

		uint32_t hash = deferredHit.randomSeed ^ (static_cast<uint32_t>(bucket.idMapping) * 0x45d9f3b);
		const float rand = static_cast<float>(hash) / static_cast<float>(std::numeric_limits<uint32_t>::max());

		const float pct = ModUtils::ResolveVariant(bucket.percentage, -1.0f) * ModUtils::ResolveVariant(bucket.percentageMult, 1.0f);
		if (pct > -1.0f && rand * 100.0f >= pct) return false;
		
		if (const float maxHp = ModUtils::ResolveVariant(bucket.maxHealth, -1.0f); maxHp > -1.0f && ctx.percentHealth > maxHp) return false;
		if (const float minDmg = ModUtils::ResolveVariant(bucket.minDamage, -1.0f); minDmg > -1.0f && ctx.percentDamage < minDmg) return false;
		if (const float maxLimbHealth = ModUtils::ResolveVariant(bucket.maxLimbHealth, -1.0f); maxLimbHealth > -1.0f && ctx.limbHealth > maxLimbHealth) return false;

		for (const auto& [level, id] : bucket.allFilters) {
			if (!MatchesHitContextAttributePair(level, id, ctx)) return false;
		}

		return true;
	}

	template <typename MappingType>
	static std::vector<MappingType*> LookupActorTrie(const CoreStructure::ActorTrieNode<MappingType>& root, const CoreStructure::DeferredHitStruct& deferredHit, ActorPerspective perspective)
	{
		using namespace CoreStructure;
		std::vector<MappingType*> result;

		const auto& ctx = deferredHit.ctx;

		const RE::FormID ctxRace = (perspective == ActorPerspective::kVictim) ? ctx.victimRaceFormID : ctx.attackerRaceFormID;
		const std::vector<RE::FormID>& ctxKeywords = (perspective == ActorPerspective::kVictim) ? ctx.victimKeywordsFormIDs : ctx.attackerKeywordsFormIDs;
		const RE::FormID ctxSkin = (perspective == ActorPerspective::kVictim) ? ctx.victimSkinFormID : ctx.attackerSkinFormID;
		const Filter::ActorSex ctxSex = (perspective == ActorPerspective::kVictim) ? ctx.victimSex : ctx.attackerSex;
		const RE::FormID ctxMaterial = (perspective == ActorPerspective::kVictim) ? ctx.victimMaterialFormID : ctx.attackerMaterialFormID;
		const std::vector<RE::FormID>& ctxWorn = (perspective == ActorPerspective::kVictim) ? ctx.victimWornFormIDs : ctx.attackerWornFormIDs;
		const std::vector<RE::FormID>& ctxWornKeywords = (perspective == ActorPerspective::kVictim) ? ctx.victimWornKeywordsFormIDs : ctx.attackerWornKeywordsFormIDs;
		const std::vector<RE::FormID>& ctxPerks = (perspective == ActorPerspective::kVictim) ? ctx.victimPerksFormIDs : ctx.attackerPerksFormIDs;
		const std::vector<RE::FormID>& ctxSpells = (perspective == ActorPerspective::kVictim) ? ctx.victimSpellsFormIDs : ctx.attackerSpellsFormIDs;
		const std::vector<RE::FormID>& ctxMagicEffects = (perspective == ActorPerspective::kVictim) ? ctx.victimMagicEffectsFormIDs : ctx.attackerMagicEffectsFormIDs;
		const RE::FormID ctxSkeleton = (perspective == ActorPerspective::kVictim) ? ctx.victimSkeletonHash : ctx.attackerSkeletonHash;
		const RE::FormID ctxBaseFormID = (perspective == ActorPerspective::kVictim) ? ctx.victimBaseFormID : ctx.attackerBaseFormID;
		const RE::FormID ctxFormID = (perspective == ActorPerspective::kVictim) ? ctx.victimFormID : ctx.attackerFormID;

		auto makeList = [](const std::vector<RE::FormID>& ids) {
			std::vector<RE::FormID> v = ids; v.push_back(0x0); return v;
		};

		const std::array<RE::FormID, 2> races = { ctxRace, 0x0 };
		const std::vector<RE::FormID> keywords = makeList(ctxKeywords);
		const std::array<RE::FormID, 2> skins = { ctxSkin, 0x0 };
		const std::array<Filter::ActorSex, 2> sexes = { ctxSex, Filter::ActorSex::kAny };
		const std::array<RE::FormID, 2> materials = { ctxMaterial, 0x0 };
		const std::vector<RE::FormID> worn = makeList(ctxWorn);
		const std::vector<RE::FormID> wornKeywords = makeList(ctxWornKeywords);
		const std::vector<RE::FormID> perks = makeList(ctxPerks);
		const std::vector<RE::FormID> spells = makeList(ctxSpells);
		const std::vector<RE::FormID> magicEffects = makeList(ctxMagicEffects);
		const std::array<RE::FormID, 2> skeletons = { ctxSkeleton, 0x0 };
		const std::array<RE::FormID, 3> formIDs = { ctxFormID, ctxBaseFormID, 0x0 };

		auto step_buckets = [&](const ActorTrieNode<MappingType>& node) {
			for (const auto& bucket : node.buckets) {
				if (!PassesActorSlowFilters(bucket, deferredHit, perspective)) continue;

				for (auto* m : bucket.mappings) {
					// Check the kAll buckets at the root for this mapping
					bool passesAll = true;
					for (const auto& rootBucket : root.buckets) {
						if (rootBucket.allFilters.empty()) continue;
						if (!std::ranges::contains(rootBucket.mappings, m)) continue;
						if (!PassesActorSlowFilters(rootBucket, deferredHit, perspective)) {
							passesAll = false;
							break;
						}
					}
					if (passesAll) result.push_back(m);
				}
			}
		};

		for (auto r : races) {
			auto it0 = root.children.find(TrieKey((uint8_t)TrieLevelActor::kRace, r));
			if (it0 == root.children.end()) continue;
		for (auto kw : keywords) {
			auto it1 = it0->second.children.find(TrieKey((uint8_t)TrieLevelActor::kKeyword, kw));
			if (it1 == it0->second.children.end()) continue;
		for (auto sk : skins) {
			auto it2 = it1->second.children.find(TrieKey((uint8_t)TrieLevelActor::kSkin, sk));
			if (it2 == it1->second.children.end()) continue;
		for (auto sx : sexes) {
			auto it3 = it2->second.children.find(TrieKey((uint8_t)TrieLevelActor::kSex, (uint32_t)sx));
			if (it3 == it2->second.children.end()) continue;
		for (auto m : materials) {
			auto it4 = it3->second.children.find(TrieKey((uint8_t)TrieLevelActor::kMaterial, m));
			if (it4 == it3->second.children.end()) continue;
		for (auto wn : worn) {
			auto it5 = it4->second.children.find(TrieKey((uint8_t)TrieLevelActor::kWorn, wn));
			if (it5 == it4->second.children.end()) continue;
		for (auto wk : wornKeywords) {
			auto it6 = it5->second.children.find(TrieKey((uint8_t)TrieLevelActor::kWornKeyword, wk));
			if (it6 == it5->second.children.end()) continue;
		for (auto pk : perks) {
			auto it7 = it6->second.children.find(TrieKey((uint8_t)TrieLevelActor::kPerk, pk));
			if (it7 == it6->second.children.end()) continue;
		for (auto sp : spells) {
			auto it8 = it7->second.children.find(TrieKey((uint8_t)TrieLevelActor::kSpell, sp));
			if (it8 == it7->second.children.end()) continue;
		for (auto me : magicEffects) {
			auto it9 = it8->second.children.find(TrieKey((uint8_t)TrieLevelActor::kMagicEffect, me));
			if (it9 == it8->second.children.end()) continue;
		for (auto sk : skeletons) {
			auto it9b = it9->second.children.find(TrieKey((uint8_t)TrieLevelActor::kSkeleton, sk));
			if (it9b == it9->second.children.end()) continue;
		for (auto fid : formIDs) {
			auto it10 = it9b->second.children.find(TrieKey((uint8_t)TrieLevelActor::kFormID, fid));
			if (it10 == it9b->second.children.end()) continue;
			step_buckets(it10->second);
		}}}}}}}}}}}}

		return result;
	}

	template <typename MappingType>
	static std::vector<MappingType*> LookupHitContextTrie(const CoreStructure::HitCtxTrieNode<MappingType>& root, const CoreStructure::DeferredHitStruct& deferredHit)
	{
		using namespace CoreStructure;
		std::vector<MappingType*> result;

		const auto& ctx = deferredHit.ctx;

		RE::Actor* victim = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.victimHandle);
		RE::Actor* attacker = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.attackerHandle);

		auto makeList = [](const auto& ids) {
			auto v = ids; v.push_back({}); return v;
		};

		const std::array<RE::FormID, 2> weapons = { ctx.weaponFormID, 0x0 };
		const std::vector<RE::FormID> weaponKeywords = makeList(ctx.weaponKeywordsFormIDs);
		const std::array<RE::FormID, 2> magicItems = { ctx.magicItemFormID, 0x0 };
		const std::array<RE::FormID, 2> projectiles = { ctx.projectileFormID, 0x0 };
		const std::array<RE::FormID, 2> magicEffects = { ctx.magicEffectFormID, 0x0 };
		const std::array<RE::FormID, 2> armors = { ctx.armorFormID, 0x0 };
		const std::vector<RE::FormID> armorKeywords = makeList(ctx.armorKeywordsFormIDs);
		const std::array<Filter::WeaponType, 2> weaponTypes = { ctx.weaponType, Filter::WeaponType::kAny };
		const std::array<Filter::ArmorClassType, 2> armorClasses = { ctx.armorClass, Filter::ArmorClassType::kAny };
		const std::array<Filter::BlockedFilter, 2> blockedStates = { ctx.blocked, Filter::BlockedFilter::kAny };
		const std::array<Filter::StateFilter, 2> states = { ctx.state, Filter::StateFilter::kAny };
		const std::array<Filter::AttackFilter, 2> attackTypes = { ctx.attack, Filter::AttackFilter::kAny };
		const std::array<Filter::SourceFilter, 2> sources = { ctx.source, Filter::SourceFilter::kAny };
		const std::array<Filter::CriticalAttackFilter, 2> critical = { ctx.criticalAttack, Filter::CriticalAttackFilter::kAny };
		const std::array<Filter::SneakAttackFilter, 2> sneak = { ctx.sneakAttack, Filter::SneakAttackFilter::kAny };
		const std::vector<uint32_t> slots = makeList(ctx.bipedSlotHashes);
		const std::vector<uint32_t> limbHash = { ctx.bipedLimbHash, 0x0 };
		const std::vector<uint32_t> nodeHash = { ctx.bipedNodeHash, 0x0 };
		const std::vector<uint32_t> keyHashes = makeList(ctx.bipedKeyHashes);

		auto step_buckets = [&](const HitCtxTrieNode<MappingType>& node) {
			for (const auto& bucket : node.buckets) {
				if (!PassesHitContextSlowFilters(bucket, deferredHit, victim, attacker)) continue;

				for (auto* m : bucket.mappings) {
					bool passesAll = true;
					for (const auto& rootBucket : root.buckets) {
						if (rootBucket.allFilters.empty() &&
							rootBucket.globalesAny.empty() && rootBucket.globalesAll.empty() && rootBucket.globalesNone.empty() &&
							rootBucket.conditionsAny.empty() && rootBucket.conditionsAll.empty() && rootBucket.conditionsNone.empty()) continue;
						if (!std::ranges::contains(rootBucket.mappings, m)) continue;
						if (!PassesHitContextSlowFilters(rootBucket, deferredHit, victim, attacker)) {
							passesAll = false;
							break;
						}
					}
					if (passesAll) result.push_back(m);
				}
			}
		};

		for (auto weap : weapons) {
			auto it0 = root.children.find(TrieKey((uint8_t)TrieLevelHitContext::kWeapon, weap));
			if (it0 == root.children.end()) continue;
		for (auto weapK : weaponKeywords) {
			auto it1 = it0->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kWeaponKeyword, weapK));
			if (it1 == it0->second.children.end()) continue;
		for (auto magic : magicItems) {
			auto it2 = it1->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kMagicItem, magic));
			if (it2 == it1->second.children.end()) continue;
		for (auto proj : projectiles) {
			auto it3 = it2->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kProjectile, proj));
			if (it3 == it2->second.children.end()) continue;
		for (auto me : magicEffects) {
			auto it4 = it3->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kMagicEffect, me));
			if (it4 == it3->second.children.end()) continue;
		for (auto armor : armors) {
			auto it5 = it4->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kArmor, armor));
			if (it5 == it4->second.children.end()) continue;
		for (auto armorK : armorKeywords) {
			auto it6 = it5->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kArmorKeyword, armorK));
			if (it6 == it5->second.children.end()) continue;
		for (auto wt : weaponTypes) {
			auto it7 = it6->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kWeaponType, (uint32_t)wt));
			if (it7 == it6->second.children.end()) continue;
		for (auto ac : armorClasses) {
			auto it8 = it7->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kArmorClass, (uint32_t)ac));
			if (it8 == it7->second.children.end()) continue;
		for (auto bl : blockedStates) {
			auto it9 = it8->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kBlocked, (uint32_t)bl));
			if (it9 == it8->second.children.end()) continue;
		for (auto st : states) {
			auto it10 = it9->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kState, (uint32_t)st));
			if (it10 == it9->second.children.end()) continue;
		for (auto atk : attackTypes) {
			auto it11 = it10->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kAttack, (uint32_t)atk));
			if (it11 == it10->second.children.end()) continue;
		for (auto src : sources) {
			auto it12 = it11->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kSource, (uint32_t)src));
			if (it12 == it11->second.children.end()) continue;
		for (auto crit : critical) {
			auto it13 = it12->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kCritical, (uint32_t)crit));
			if (it13 == it12->second.children.end()) continue;
		for (auto snk : sneak) {
			auto it14 = it13->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kSneak, (uint32_t)snk));
			if (it14 == it13->second.children.end()) continue;
		for (auto slot : slots) {
			auto it15 = it14->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kBipedSlot, slot));
			if (it15 == it14->second.children.end()) continue;
		for (auto limbH : limbHash) {
			auto it16 = it15->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kBipedLimb, limbH));
			if (it16 == it15->second.children.end()) continue;
		for (auto nodeH : nodeHash) {
			auto it17 = it16->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kBipedNode, nodeH));
			if (it17 == it16->second.children.end()) continue;
		for (auto keyH : keyHashes) {
			auto it18 = it17->second.children.find(TrieKey((uint8_t)TrieLevelHitContext::kBipedKey, keyH));
			if (it18 == it17->second.children.end()) continue;
			step_buckets(it18->second);
		}}}}}}}}}}}}}}}}}}}

		return result;
	}

	template <typename MappingType, typename RegistryType>
	static std::vector<MappingType*> CollectCandidates(const RegistryType& registry, CoreStructure::DeferredHitStruct& deferredHit)
	{
		// Exclusions
		std::unordered_set<MappingType*> excluded;
		for (auto* m : GetExcludedActorMappings(registry.victimExclude, deferredHit, ActorPerspective::kVictim)) excluded.insert(m);
		for (auto* m : GetExcludedActorMappings(registry.attackerExclude, deferredHit, ActorPerspective::kAttacker)) excluded.insert(m);
		for (auto* m : GetExcludedHitContextMappings(registry.hitContextExclude, deferredHit)) excluded.insert(m);

		// Intersection
		auto victimMatches = LookupActorTrie(registry.victimInclude, deferredHit, ActorPerspective::kVictim);
		auto attackerMatches = LookupActorTrie(registry.attackerInclude, deferredHit, ActorPerspective::kAttacker);
		auto hitContextMatches = LookupHitContextTrie(registry.hitContextInclude, deferredHit);

		std::unordered_set<MappingType*> inVictim(victimMatches.begin(), victimMatches.end());
		std::unordered_set<MappingType*> inAttacker(attackerMatches.begin(), attackerMatches.end());
		std::unordered_set<MappingType*> inHitCtx(hitContextMatches.begin(), hitContextMatches.end());

		std::vector<MappingType*> candidates;
		for (auto* m : victimMatches) {
			if (excluded.contains(m)) continue;
			if (!inAttacker.contains(m)) continue;
			if (!inHitCtx.contains(m)) continue;
			candidates.push_back(m);
		}

		std::ranges::sort(candidates, [](const MappingType* a, const MappingType* b) {
			return a->priority > b->priority;
		});

		return candidates;
	}

	 template <typename ModifierType>
	 static void MergeVectors(ModifierType& dst, const ModifierType& src)
	 {
		 auto merge = [](auto& dstVec, const auto& srcVec) {
			 using E = typename std::decay_t<decltype(dstVec)>::value_type;
			 std::unordered_set<E> seen(dstVec.begin(), dstVec.end());
			 for (auto* e : srcVec) if (seen.insert(e).second) dstVec.push_back(e);
		 };

		 auto mergeStr = [](auto& dstVec, const auto& srcVec) {
			 std::unordered_set<std::string> seen(dstVec.begin(), dstVec.end());
			 for (const auto& s : srcVec) if (seen.insert(s).second) dstVec.push_back(s);
		 };

		for (const auto& entry : src.extraImpactData) {
			if (std::ranges::find(dst.extraImpactData, entry) == dst.extraImpactData.end())
				dst.extraImpactData.push_back(entry);
		}

		merge(dst.extraSound, src.extraSound);
		merge(dst.spells, src.spells);
		merge(dst.placedObjects, src.placedObjects);
		mergeStr(dst.bloodpools, src.bloodpools);
	}

	 template <typename ModifierType>
	 static void AssignIfEmpty(ModifierType& dst, const ModifierType& src)
	 {
		 if (dst.extraImpactData.empty()) dst.extraImpactData = src.extraImpactData;
		 if (dst.extraSound.empty()) dst.extraSound = src.extraSound;
		 if (dst.spells.empty()) dst.spells = src.spells;
		 if (dst.placedObjects.empty()) dst.placedObjects = src.placedObjects;
		 if (dst.bloodpools.empty()) dst.bloodpools = src.bloodpools;
	 }

	static bool MergeImpactModifier(std::optional<CoreStructure::ImpactMapping::Modifier>& finalMod, const CoreStructure::ImpactMapping& mapping)
	{
		const auto& src = mapping.modifier;
		if (!finalMod) {
			finalMod = src;
			return mapping.override;
		}
		if (!finalMod->impactData) finalMod->impactData = src.impactData;
		if (!finalMod->soundOverride) finalMod->soundOverride = src.soundOverride;
		if (!finalMod->decalOverride) finalMod->decalOverride = src.decalOverride;
		if (!finalMod->preserveOriginalSound.has_value()) finalMod->preserveOriginalSound = src.preserveOriginalSound;
		if (!finalMod->preserveOriginalDecal.has_value()) finalMod->preserveOriginalDecal = src.preserveOriginalDecal;
		if (!finalMod->removeDecal.has_value()) finalMod->removeDecal = src.removeDecal;
		if (!finalMod->removeBloodSplatter.has_value()) finalMod->removeBloodSplatter = src.removeBloodSplatter;
		if (!finalMod->removeSound.has_value()) finalMod->removeSound = src.removeSound;
		if (!finalMod->impactBounce.has_value()) finalMod->impactBounce = src.impactBounce;
		if (!finalMod->bloodSpray) finalMod->bloodSpray = src.bloodSpray;
		if (!finalMod->dismemberAuto.has_value()) finalMod->dismemberAuto = src.dismemberAuto;
		if (!finalMod->disarm.has_value()) finalMod->disarm = src.disarm;
		if (!finalMod->eject.has_value()) finalMod->eject = src.eject;

		src.mergeWithPrevious ? MergeVectors(*finalMod, src) : AssignIfEmpty(*finalMod, src);

		return mapping.override;
	}

	static bool MergeHitModifier(std::optional<CoreStructure::HitMapping::Modifier>& finalMod, const CoreStructure::HitMapping& mapping)
	{
		const auto& src = mapping.modifier;
		if (!finalMod) {
			finalMod = src;
			return mapping.override;
		}
		if (!finalMod->bloodSpray) finalMod->bloodSpray = src.bloodSpray;
		if (!finalMod->dismemberAuto.has_value()) finalMod->dismemberAuto = src.dismemberAuto;
		if (!finalMod->disarm.has_value()) finalMod->disarm = src.disarm;
		if (!finalMod->eject.has_value()) finalMod->eject = src.eject;

		src.mergeWithPrevious ? MergeVectors(*finalMod, src) : AssignIfEmpty(*finalMod, src);

		return mapping.override;
	}

	static void TraceRuntimeContext(const CoreStructure::DeferredHitStruct& deferredHit, const bool deferred = false)
	{
		using namespace CoreStructure;

		const auto& ctx = deferredHit.ctx;

		auto formatFormIDs = [](const std::vector<RE::FormID>& keywords) {
			return fmt::format("{}", fmt::join(keywords | std::views::transform([](RE::FormID id) {
				return fmt::format("{:08X}", id);
			}), ", "));
		};

		auto formatBipedSlots = [&]() {
			if (deferredHit.bipedEntry.bipedSlots.empty()) return std::string("none");
			return fmt::format("{}", fmt::join(deferredHit.bipedEntry.bipedSlots | std::views::transform([](int slot) {
				return std::to_string(slot);
			}), ", "));
		};

		auto formatBipedKeys = [&]() {
			if (deferredHit.bipedEntry.bipedKeys.empty()) return std::string("none");
			return fmt::format("{}", fmt::join(deferredHit.bipedEntry.bipedKeys, ", "));
		};

		TRACE("=== RuntimeHitContext{} ===", deferred ? " (DEFERRED)" : "");

		auto* victim = MiscUtils::GetValidReference<RE::Actor>(ctx.victimFormID);
		auto* attacker = MiscUtils::GetValidReference<RE::Actor>(ctx.attackerFormID);

		// Victim
		TRACE("  [Victim]");
		TRACE("    FormID={:08X} | Base={:08X} | Race={:08X} | Skin={:08X} | Material={:08X}",
			victim ? victim->formID : 0x0, ctx.victimBaseFormID, ctx.victimRaceFormID,
			ctx.victimSkinFormID, ctx.victimMaterialFormID);
		TRACE("    Keywords=[{}]", formatFormIDs(ctx.victimKeywordsFormIDs));
		TRACE("    Worn=[{}]", formatFormIDs(ctx.victimWornFormIDs));
		TRACE("    WornKeywords=[{}]", formatFormIDs(ctx.victimWornKeywordsFormIDs));
		TRACE("    Perks=[{}]", formatFormIDs(ctx.victimPerksFormIDs));
		TRACE("    Spells=[{}]", formatFormIDs(ctx.victimSpellsFormIDs));
		TRACE("    MagicEffects=[{}]", formatFormIDs(ctx.victimMagicEffectsFormIDs));
		TRACE("    Sex={}", magic_enum::enum_name(ctx.victimSex));
		TRACE("    SkeletonHash=[{}]", ctx.victimSkeletonHash);

		// Attacker
		TRACE("  [Attacker]");
		TRACE("    FormID={:08X} | Base={:08X} | Race={:08X} | Skin={:08X} | Material={:08X}",
			attacker ? attacker->formID : 0x0, ctx.attackerBaseFormID, ctx.attackerRaceFormID,
			ctx.attackerSkinFormID, ctx.attackerMaterialFormID);
		TRACE("    Keywords=[{}]", formatFormIDs(ctx.attackerKeywordsFormIDs));
		TRACE("    Worn=[{}]", formatFormIDs(ctx.attackerWornFormIDs));
		TRACE("    WornKeywords=[{}]", formatFormIDs(ctx.attackerWornKeywordsFormIDs));
		TRACE("    Perks=[{}]", formatFormIDs(ctx.attackerPerksFormIDs));
		TRACE("    Spells=[{}]", formatFormIDs(ctx.attackerSpellsFormIDs));
		TRACE("    MagicEffects=[{}]", formatFormIDs(ctx.attackerMagicEffectsFormIDs));
		TRACE("    Sex={}", magic_enum::enum_name(ctx.attackerSex));
		TRACE("    SkeletonHash=[{}]", ctx.attackerSkeletonHash);

		// Hit context
		TRACE("  [HitContext]");
		TRACE("    Weapon={:08X} | MagicItem={:08X} | Projectile={:08X} | MagicEffect={:08X} | Armor={:08X}",
			ctx.weaponFormID, ctx.magicItemFormID, ctx.projectileFormID, ctx.magicEffectFormID, ctx.armorFormID);
		TRACE("    WeaponKeywords=[{}] | ArmorKeywords=[{}]", formatFormIDs(ctx.weaponKeywordsFormIDs), formatFormIDs(ctx.armorKeywordsFormIDs));
		TRACE("    WeaponType={} | ArmorClass={} | Blocked={} | State={} | Attack={} | Source={} | Critical={} | Sneak={}",
			magic_enum::enum_name(ctx.weaponType), magic_enum::enum_name(ctx.armorClass), magic_enum::enum_name(ctx.blocked),
			magic_enum::enum_name(ctx.state), magic_enum::enum_name(ctx.attack), magic_enum::enum_name(ctx.source),
			magic_enum::enum_name(ctx.criticalAttack), magic_enum::enum_name(ctx.sneakAttack));
		TRACE("    PercentHealth={:.2f}% | PercentDamage={:.2f}% | LimbHealth={:.2f}%", ctx.percentHealth, ctx.percentDamage, ctx.limbHealth);

		// Biped
		TRACE("  [Biped]");
		TRACE("    Limb='{}'", deferredHit.bipedEntry.bipedLimb.empty() ? "none" : deferredHit.bipedEntry.bipedLimb);
		TRACE("    Node='{}'", deferredHit.bipedEntry.bipedNode.empty() ? "none" : deferredHit.bipedEntry.bipedNode);
		TRACE("    Slots=[{}]", formatBipedSlots());
		TRACE("    Keys=[{}]", formatBipedKeys());

		// Modifiers
		TRACE("  [Modifiers]");
		TRACE("    DamageMult={:.3f} | DamageCalc={:.3f} | DamageLimbMult={:.3f}", ctx.damageMult, ctx.damageCalc, ctx.damageLimbMult);

		TRACE("=== END RuntimeHitContext{} ===", deferred ? " (DEFERRED)" : "");
	}
};
