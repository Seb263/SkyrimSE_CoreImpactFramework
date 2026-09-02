#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Structure.h"

#include "Utils/JsonUtils.hpp"
#include "Utils/MiscUtils.hpp"

namespace JSONHandler
{
	using namespace CoreStructure;

	// BIPED MAPPING

	struct BipedActorFilters
	{
		std::vector<RE::FormID> races;
		std::vector<RE::FormID> keywords;
		std::vector<RE::FormID> skins;
		std::vector<RE::FormID> materials;
		std::vector<RE::FormID> formIds;
		std::vector<RE::FormID> skeletons;
		std::vector<RE::BGSPerk*> conditions;
		std::vector<Filter::ActorSex> sexes;
	};

	struct ParsedBipedFilters
	{
		BipedActorFilters victimInc, victimExc, victimAll;
	};

	// IMPACT / HIT MAPPING

	struct ActorFilters
	{
		std::vector<RE::FormID> races;
		std::vector<RE::FormID> keywords;
		std::vector<RE::FormID> skins;
		std::vector<Filter::ActorSex> sexes;
		std::vector<RE::FormID> materials;
		std::vector<RE::FormID> worn;
		std::vector<RE::FormID> wornKeywords;
		std::vector<RE::FormID> perks;
		std::vector<RE::FormID> spells;
		std::vector<RE::FormID> magicEffects;
		std::vector<RE::FormID> skeletons;
		std::vector<RE::FormID> formIds;
	};

	struct HitContextFilters
	{
		std::vector<RE::FormID> weapons;
		std::vector<RE::FormID> weaponKeywords;
		std::vector<RE::FormID> magicItems;
		std::vector<RE::FormID> projectiles;
		std::vector<RE::FormID> magicEffects;
		std::vector<RE::FormID> armors;
		std::vector<RE::FormID> armorKeywords;
		std::vector<Filter::WeaponType> weaponTypes;
		std::vector<Filter::ArmorClassType> armorClasses;
		std::vector<Filter::BlockedFilter> blockeds;
		std::vector<Filter::StateFilter> states;
		std::vector<Filter::AttackFilter> attacks;
		std::vector<Filter::SourceFilter> sources;
		std::vector<Filter::CriticalAttackFilter> criticals;
		std::vector<Filter::SneakAttackFilter> sneaks;
		std::vector<Filter::GlobalFilter> globalesAny;
		std::vector<Filter::GlobalFilter> globalesAll;
		std::vector<Filter::GlobalFilter> globalesNone;
		std::vector<RE::BGSPerk*> conditionsAny;
		std::vector<RE::BGSPerk*> conditionsAll;
		std::vector<RE::BGSPerk*> conditionsNone;
		std::vector<int> bipedSlots;
		std::vector<std::string> bipedLimbs;
		std::vector<std::string> bipedNodes;
		std::vector<std::string> bipedKeys;
		std::variant<float, RE::TESGlobal*> percentage = -1.0f;
		std::variant<float, RE::TESGlobal*> percentageMult = 1.0f;
		std::variant<float, RE::TESGlobal*> maxHealth = -1.0f;
		std::variant<float, RE::TESGlobal*> minDamage = -1.0f;
		std::variant<float, RE::TESGlobal*> maxLimbHealth = -1.0f;
	};

	struct ParsedMappingFilters
	{
		ActorFilters victimInc, victimExc, victimAll;
		ActorFilters attackerInc, attackerExc, attackerAll;
		HitContextFilters hitInc, hitExc, hitAll;
	};

	enum class TrieFillMode
	{
		kInclude,
		kExclude,
		kAll
	};

	class Main
	{
	public:
		static void LoadMappings();
		
		template <typename Type>
		static Type* ResolveForm(const std::string& str, const bool useAssociatedForm = false);

		template <typename Type, typename Output>
		static void ParseMappingFilterFormJson(const json& filters, const std::string& key, std::vector<Output>& out, const bool useAssociatedForm = false);

		static Filter::ComparisonType ToComparisonType(const std::string& comparison);

	private:
		static void ProcessJson(const json& jsonData);
	};
};

#include "JSON/Biped.hpp"
#include "JSON/Impact.hpp"
#include "JSON/LegacyConverter.hpp"
#include "JSON/Debugging.hpp"
