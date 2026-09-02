#pragma once

#include "API/DBF-API.h"
#include "API/DDO-API.h"
#include "API/DF-API.h"

namespace ModData
{
	constexpr std::string_view MOD_NAME = "Core Impact Framework";

	struct PluginForm
	{
		std::string_view name;
		void** formPtr;
		uint32_t formID;
		std::string_view pluginName;
	};
	
	struct DefaultForm
	{
		void** formPtr;
		std::string formStr;
	};

	// Properties storing game form references
	inline RE::TESObjectWEAP* defaultUnarmedWeap;

	// Default form references
	inline RE::BGSImpactData* weaponDefaultImpactData;
	inline RE::BGSImpactData* weaponWoodImpactData;
	inline RE::BGSImpactData* shieldHeavyImpactData;
	inline RE::BGSImpactData* shieldLightImpactData;

	static inline const std::vector<PluginForm> pluginForms = {
		{ "defaultUnarmedWeap", reinterpret_cast<void**>(&defaultUnarmedWeap), 0x1F4, "Skyrim.esm" }
	};

	inline std::vector<DefaultForm> defaultForms = {};

	inline auto lastLoadPoint = std::chrono::steady_clock::now();

	inline RE::TESDataHandler* TESdataHandler;
	inline std::unordered_map<std::string, RE::TESGlobal*> pluginGlobalVariables;

	inline std::unordered_map<RE::BGSImpactDataSet*, std::unordered_map<RE::BGSMaterialType*, RE::BGSImpactData*>> originalImpactMap;
	inline std::unordered_map<RE::BGSMaterialType*, RE::BGSMaterialType*> originalMaterials; // origin => clone
	inline std::unordered_map<RE::BGSMaterialType*, RE::BGSMaterialType*> originalMaterialsReversed; // clone => origin
	inline std::unordered_map<RE::SpellItem*, RE::BGSProjectile*> spellProjectileMap;

	inline RE::BGSImpactData* ModRuntime_Impact;
	inline RE::SpellItem* ModRuntimeBloodSpray_Spell;
	inline RE::EffectSetting* ModRuntimeBloodSpray_Effect;
	inline RE::BGSProjectile* ModRuntimeBloodSpray_Projectile;
	inline RE::BGSExplosion* ModRuntimeMarker_Explosion;
	inline RE::BGSCollisionLayer* ModRuntimeBlood_CollisionLayer;

	inline float default_fCombatEnvironmentBloodChance;

	// Global variables to store difficulty settings
	inline float fDiffMultHPByPC[6];
	inline float fDiffMultHPToPC[6];

	inline DF_API_Legacy::Interface* DF_API_Legacy_Interface = nullptr;
	inline DBF_API::Interface* DBF_API_Interface = nullptr;
	inline DDO_API::Interface* DDO_API_Interface = nullptr;
};
