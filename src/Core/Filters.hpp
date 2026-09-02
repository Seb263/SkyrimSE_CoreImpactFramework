#pragma once

#include "Core/Structure.h"

class FiltersFunctions
{
public:

	static bool EvaluateGlobalFilter(const CoreStructure::Filter::GlobalFilter& g)
	{
		using ComparisonType = CoreStructure::Filter::ComparisonType;
		if (!g.global) return false;
		
		const float v = g.global->value;
		switch (g.comparison) {
			case ComparisonType::kEqual: return v == g.value;
			case ComparisonType::kNotEqual: return v != g.value;
			case ComparisonType::kLessThan: return v < g.value;
			case ComparisonType::kGreaterThan: return v > g.value;
			case ComparisonType::kLessThanOrEqual: return v <= g.value;
			case ComparisonType::kGreaterThanOrEqual: return v >= g.value;
		}
		return false;
	}

	static bool HasValidConditionFilter(RE::Actor* victim, RE::Actor* attacker, RE::BGSPerk* perk)
	{
		if (!victim || !attacker || !perk) return false;
		
		return perk->perkConditions.IsTrue(attacker, victim);
	}

	static RE::TESObjectARMO* GetArmorFromSlots(RE::Actor* target, const std::vector<int>& bipedSlots)
	{
		for (int bipedSlot : bipedSlots) {
			if (bipedSlot <= 0) continue;

			const int biped_flag = (bipedSlot - 30) >= 0 ? 1 << (bipedSlot - 30) : 0;
			if (biped_flag == 0) continue;

			RE::TESObjectARMO* armor = target->GetWornArmor(static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(biped_flag));
			if (armor) return armor;
		}
		return nullptr;
	}

	static CoreStructure::Filter::WeaponType GetWeaponType(RE::TESObjectWEAP* weapon, RE::TESRace* targetRace)
	{
		using WeaponType = CoreStructure::Filter::WeaponType;

		if (!weapon) return WeaponType::kOther;

		static RE::BGSKeyword* weapTypeWarhammer = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("WeapTypeWarhammer");
		static RE::BGSKeyword* actorTypeNPC = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ActorTypeNPC");

		const RE::WEAPON_TYPE weaponType = weapon->GetWeaponType();

		if (weapTypeWarhammer && weapon->HasKeyword(weapTypeWarhammer)) {
			return WeaponType::kTwoHandMace;
		} else if (weaponType == RE::WEAPON_TYPE::kOneHandSword) {
			return WeaponType::kOneHandSword;
		} else if (weaponType == RE::WEAPON_TYPE::kTwoHandSword) {
			return WeaponType::kTwoHandSword;
		} else if (weaponType == RE::WEAPON_TYPE::kOneHandAxe) {
			return WeaponType::kOneHandAxe;
		} else if (weaponType == RE::WEAPON_TYPE::kTwoHandAxe) {
			return WeaponType::kTwoHandAxe;
		} else if (weaponType == RE::WEAPON_TYPE::kOneHandDagger) {
			return WeaponType::kDagger;
		} else if (weaponType == RE::WEAPON_TYPE::kOneHandMace) {
			return WeaponType::kOneHandMace;
		} else if (weaponType == RE::WEAPON_TYPE::kStaff) {
			return WeaponType::kMagic;
		} else if (weaponType == RE::WEAPON_TYPE::kBow || weaponType == RE::WEAPON_TYPE::kCrossbow) {
			return WeaponType::kRanged;
		} else if (weaponType == RE::WEAPON_TYPE::kHandToHandMelee) {
			return (!targetRace || (actorTypeNPC && targetRace->HasKeyword(actorTypeNPC))) ? WeaponType::kHandToHand : WeaponType::kBeast;
		}

		return WeaponType::kOther;
	}

	static CoreStructure::Filter::ArmorClassType GetArmorClassType(RE::TESObjectARMO* armor, const std::vector<int>& bipedSlots)
	{
		using namespace CoreStructure;

		if (bipedSlots.empty()) return Filter::ArmorClassType::kDefault;

		for (int bipedSlot : bipedSlots) {
			switch (bipedSlot) {
				case -1: return Filter::ArmorClassType::kWeaponDefault;
				case -2: return Filter::ArmorClassType::kWeaponWood;
				case -3: return Filter::ArmorClassType::kShieldHeavy;
				case -4: return Filter::ArmorClassType::kShieldLight;
				default: break;
			}
		}

		if (armor) {
			if (armor->IsHeavyArmor()) return Filter::ArmorClassType::kHeavy;
			if (armor->IsLightArmor()) return Filter::ArmorClassType::kLight;
			if (armor->IsClothing()) return Filter::ArmorClassType::kCloth;
		}

		return Filter::ArmorClassType::kDefault;
	}

	static std::vector<RE::FormID> GetActorKeywordsFormIDs(RE::Actor* actor)
	{
		if (!actor) return {};

		std::unordered_set<RE::FormID> set;

		auto collect = [&](RE::BGSKeywordForm* form) {
			if (!form) return;

			for (auto* kw : form->GetKeywords()) {
				if (kw) set.insert(kw->GetFormID());
			}
		};

		collect(actor->As<RE::BGSKeywordForm>());
		collect(actor->GetActorBase() ? actor->GetActorBase()->As<RE::BGSKeywordForm>() : nullptr);
		collect(actor->GetRace() ? actor->GetRace()->As<RE::BGSKeywordForm>() : nullptr);

		return { set.begin(), set.end() };
	}

	static std::vector<RE::FormID> GetActorWornFormIDs(RE::Actor* actor)
	{
		if (!actor) return {};

		std::unordered_set<RE::FormID> set;

		const auto inventory = actor->GetInventory();
		for (const auto& [form, data] : inventory) {
			if (!form || !data.second->IsWorn()) continue;

			set.insert(form->GetFormID());
		}

		return { set.begin(), set.end() };
	}

	static std::vector<RE::FormID> GetActorWornKeywordsFormIDs(RE::Actor* actor)
	{
		if (!actor) return {};

		std::unordered_set<RE::FormID> set;

		const auto inventory = actor->GetInventory();
		for (const auto& [form, data] : inventory) {
			if (!form || !data.second->IsWorn()) continue;

			const auto keywordForm = form->As<RE::BGSKeywordForm>();
			if (!keywordForm) continue;

			for (auto* kw : keywordForm->GetKeywords()) {
				if (kw) set.insert(kw->GetFormID());
			}
		}

		return { set.begin(), set.end() };
	}

	static std::vector<RE::FormID> GetActorPerksFormIDs(RE::Actor* actor)
	{
		if (!actor) return {};

		auto* actorBase = actor->GetActorBase();
		if (!actorBase) return {};

		std::unordered_set<RE::FormID> set;

		// Base Perks
		const int numPerks = actorBase->perkCount;
		for (int i = 0; i < numPerks; i++) {
			RE::BGSPerk* perk = actorBase->perks[i].perk;
			if (!perk) continue;

			set.insert(perk->GetFormID());
		}

		// Added Perks
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (player && actor->IsPlayerRef()) {
			if (!REL::Module::IsVR()) { // Perk logic not working in VR
				const auto& addedPerks = player->GetPlayerRuntimeData().addedPerks;

				int numPlayerPerks = addedPerks.size();
				for (int i = 0; i < numPlayerPerks; i++) {
					RE::BGSPerk* perk = addedPerks[i]->perk;
					if (!perk) continue;

					set.insert(perk->GetFormID());
				}
			}
		}

		return { set.begin(), set.end() };
	}

	static std::vector<RE::FormID> GetActorSpellsFormIDs(RE::Actor* actor)
	{
		if (!actor) return {};

		std::unordered_set<RE::FormID> set;

		// Base Spells
		auto* actorBase = actor->GetActorBase();
		if (actorBase && actorBase->actorEffects) {
			const int numberOfBaseSpells = actorBase->actorEffects->numSpells;
			for (int i = 0; i < numberOfBaseSpells; i++) {
				RE::SpellItem* spell = actorBase->actorEffects->spells[i];
				if (!spell) continue;
			
				set.insert(spell->GetFormID());
			}
		}

		// Added Spells
		const int numberOfAddedSpells = actor->GetActorRuntimeData().addedSpells.size();
		for (int i = 0; i < numberOfAddedSpells; i++) {
			RE::SpellItem* spell = actor->GetActorRuntimeData().addedSpells[i];
			if (!spell) continue;
			
			set.insert(spell->GetFormID());
		}

		return { set.begin(), set.end() };
	}

	static std::vector<RE::FormID> GetMagicEffectsFormIDs(RE::Actor* actor)
	{
		if (!actor) return {};
		auto* magicTarget = actor->AsMagicTarget();
		if (!magicTarget) return {};

		std::unordered_set<RE::FormID> set;

		auto collectEffect = [&](RE::ActiveEffect* effect) {
			if (!effect || effect->flags.any(RE::ActiveEffect::Flag::kInactive)) return;
			auto* baseEffect = effect->GetBaseObject();
			if (!baseEffect) return;
			set.insert(baseEffect->GetFormID());
		};

		if (auto* effectList = magicTarget->GetActiveEffectList()) {
			for (auto itr = effectList->begin(); itr != effectList->end(); ++itr) {
				collectEffect(*itr);
			}
		}

		return { set.begin(), set.end() };
	}

	static std::vector<RE::FormID> GetItemKeywordsFormIDs(RE::TESForm* form)
	{
		if (!form) return {};

		auto* keywordForm = form->As<RE::BGSKeywordForm>();
		if (!keywordForm) return {};

		std::unordered_set<RE::FormID> set;

		for (auto* kw : keywordForm->GetKeywords()) {
			if (kw) set.insert(kw->GetFormID());
		}

		return { set.begin(), set.end() };
	}
};
