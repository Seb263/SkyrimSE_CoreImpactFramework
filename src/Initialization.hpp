#pragma once

#include "DataHandler.hpp"
#include "Events.h"
#include "Main.hpp"
#include "SettingsIni.hpp"

#include "JSON/JSONHandler.h"

#include "Utils/MiscUtils.hpp"

#include "API/ModAPI.h"
#include "API/ModAPI-Legacy.h"

#include "API/DBF-API.h"
#include "API/DDO-API.h"
#include "API/DF-API.h"

namespace ModData
{
	class DataHandler
	{
	public:
		bool preLoaded = false;
		bool postLoaded = false;
		bool postLoadedAlternate = false;

		static DataHandler* GetSingleton()
		{
			static DataHandler singleton;
			return &singleton;
		}

		std::future<void> loadFuture;

		void WaitUntilReady()
		{
			if (SettingsIni::bGeneral_AsynchronousStartup && loadFuture.valid()) {
				loadFuture.get();
			}
		}

		void PreLoadData()
		{
			if (preLoaded) return;
			preLoaded = true;

			TESdataHandler = RE::TESDataHandler::GetSingleton();
			ExtractGameAssets();
			ApplyGameSettings();
			LoadPluginsForms();
			LoadDifficultySettings();
			InitializeProcessExplosionMarker();
			InitializeProcessImpactData();
			InitializeProcessCollisionLayer();
			InitializeProcessDefaultBloodImpact();
			DefineMaterialsAndImpacts();

			Events::ModEventSink::LoadEvents();
			Events::MainEvent::InstallHitHook();
			SettingsIni::TDMSettingsManager().ReadSettings();
			
			if (!CoreImpactFrameworkAPI_Legacy::g_API) CoreImpactFrameworkAPI_Legacy::g_API = new CoreImpactFrameworkAPI_Legacy::CoreImpactFrameworkAPI_Legacy;
			if (!SKSE::GetMessagingInterface()->RegisterListener(NULL, [](SKSE::MessagingInterface::Message* message) {
				switch (message->type) {
				case CIF_API_LEGACY_TYPE_KEY:
					message->dataLen = sizeof(CoreImpactFrameworkAPI_Legacy::CoreImpactFrameworkAPI_Legacy*);
					*(CoreImpactFrameworkAPI_Legacy::CoreImpactFrameworkAPI_Legacy**)message->data = CoreImpactFrameworkAPI_Legacy::g_API;
					break;
				}
			})) REPORT_AND_FAIL("Unable to register API message listener.");
			else logger::info("Successfully registered API message listener.");

			auto loadAndInitialize = []() {
				SKSE::GetTaskInterface()->AddTask([]() {
					JSONHandler::Main::LoadMappings();
				});
			};

			if (SettingsIni::bGeneral_AsynchronousStartup) loadFuture = std::async(std::launch::async, loadAndInitialize);
			else loadAndInitialize();
		}

		void PostLoadData()
		{
			if (postLoaded) return;
			postLoaded = true;

			WaitUntilReady();

			if (DF_API_Legacy::LoadAPI()) {
				DF_API_Legacy_Interface = DF_API_Legacy::g_API;
				const auto version = DF_API_Legacy_Interface->GetVersion();

				const auto major = (version >> 16) & 0xFF;
				const auto minor = (version >> 8) & 0xFF;
				const auto patch = version & 0xFF;

				logger::info("Dismembering Framework API [Legacy] v{}.{}.{}.0 registered successfully.", major, minor, patch);
			}

			if (auto* apiInterface = static_cast<DDO_API::Interface*>(DDO_API::GetAPI())) {
				DDO_API_Interface = apiInterface;
				logger::info("Death Drop Overhaul API v{} registered successfully.", apiInterface->GetVersion().string("."));
			} else {
				logger::info("Death Drop Overhaul API not found.");
			}

			if (auto* apiInterface = static_cast<DBF_API::Interface*>(DBF_API::GetAPI())) {
				DBF_API_Interface = apiInterface;
				logger::info("Dynamic Bloodpool Framework API v{} registered successfully.", apiInterface->GetVersion().string("."));
			} else {
				logger::info("Dynamic Bloodpool Framework API not found.");
			}
		}

		void PostLoadDataAlternate()
		{
			if (postLoadedAlternate) return;
			postLoadedAlternate = true;

			TimeUtils::DoWhile(100ms, [](TimeUtils::CallResult result, std::chrono::nanoseconds) {
				if (TimeUtils::IsEnd(result)) return true;

				auto player = RE::PlayerCharacter::GetSingleton();
				if (player && player->Is3DLoaded() && player->GetParentCell() && player->GetParentCell()->IsAttached()) {
					GetSingleton()->PostLoadData();
					return false;
				}

				return true;
			}, true);
		}

	private:
		static inline void LoadPluginsForms()
		{
			logger::info("Loading Plugins Froms Data...");

			for (const auto& formInfo : pluginForms) {
				*formInfo.formPtr = TESdataHandler->LookupForm(formInfo.formID, formInfo.pluginName.data());
				if (!*formInfo.formPtr) {
					REPORT_AND_FAIL("Error: Form \"{}\" not found in \"{}\".", formInfo.name, formInfo.pluginName);
				}
			}

			for (const auto& formInfo : defaultForms) {
				if (auto* foundForm = MiscUtils::GetFormFromAssoc<RE::TESForm>(formInfo.formStr)) {
					if (*formInfo.formPtr = foundForm) continue;
				}
				REPORT_AND_FAIL("Error: Form \"{}\" not found.", formInfo.formStr);
			}

			logger::info("Loading Plugins Froms Data: DONE");
		}

		static inline void ExtractGameAssets()
		{
			constexpr unsigned char PscBytes[] = {
				#include "CoreImpactFramework.psc.h"
			};

			constexpr unsigned char PexBytes[] = {
				#include "CoreImpactFramework.pex.h"
			};

			const std::string_view PscData{ reinterpret_cast<const char*>(PscBytes), sizeof(PscBytes) - 1 };

			const std::string_view PexData{ reinterpret_cast<const char*>(PexBytes), sizeof(PexBytes) - 1 };

			struct AssetEntry
			{
				std::string_view data;
				std::string_view dest;
				bool isSource;
			};

			const std::array<AssetEntry, 2> assets{{
				{ PscData, "Data/Source/Scripts/CoreImpactFramework.psc", true },
				{ PexData, "Data/Scripts/CoreImpactFramework.pex", false }
			}};

			for (const auto& asset : assets) {
				if (asset.isSource && !SettingsIni::bGeneral_ExtractScriptSources) {
					TRACE("ExtractGameAssets: Skipping source script '{}' (ExtractScriptSources disabled).", asset.dest);
					continue;
				}
				try {
					const std::size_t srcHash = std::hash<std::string_view>{}(asset.data);
					const std::filesystem::path destPath(asset.dest);
					if (std::filesystem::exists(destPath)) {
						std::ifstream existing(destPath, std::ios::binary);
						if (existing) {
							const std::string destData{
								std::istreambuf_iterator<char>{existing},
								std::istreambuf_iterator<char>{}
							};
							const std::size_t destHash = std::hash<std::string>{}(destData);
							if (srcHash == destHash) {
								TRACE("ExtractGameAssets: Asset '{}' is up-to-date, skipping.", asset.dest);
								continue;
							}
							if (!SettingsIni::bGeneral_OverwriteInvalidScripts) {
								TRACE("ExtractGameAssets: Asset '{}' differs but overwrite is disabled, skipping.", asset.dest);
								continue;
							}
							TRACE("ExtractGameAssets: Asset '{}' differs, replacing.", asset.dest);
						}
					}
					std::filesystem::create_directories(destPath.parent_path());

					std::ofstream out(destPath, std::ios::binary | std::ios::trunc);
					if (!out) {
						logger::error("ExtractGameAssets: Failed to open output stream for '{}'.", asset.dest);
						continue;
					}

					out.write(asset.data.data(), static_cast<std::streamsize>(asset.data.size()));
					if (!out) {
						std::filesystem::remove(destPath);
						logger::error("ExtractGameAssets: Failed to write '{}'.", asset.dest);
						continue;
					}
					TRACE("ExtractGameAssets: Asset '{}' extracted successfully.", asset.dest);
				} catch (const std::exception& e) {
					logger::error("ExtractGameAssets: Exception extracting '{}': {}", asset.dest, e.what());
				}
			}
		}

		static inline void ApplyGameSettings()
		{
			logger::info("Applying Game Settings...");

			default_fCombatEnvironmentBloodChance = MiscUtils::GetGameSetting<float>("fCombatEnvironmentBloodChance");
			MiscUtils::SetGameSetting("fCombatEnvironmentBloodChance", 0.0f);

			logger::info("Applying Game Settings: DONE");
		}

		static inline void LoadDifficultySettings()
		{
			logger::info("Loading Difficulty Settings...");

			RE::GameSettingCollection* gameSettings = RE::GameSettingCollection::GetSingleton();
			if (!gameSettings) REPORT_AND_FAIL("Error: GameSettingCollection not found.");

			const std::string difficultyDamageByPC[6] = {
				"fDiffMultHPByPCVE", "fDiffMultHPByPCE", "fDiffMultHPByPCN",
				"fDiffMultHPByPCH", "fDiffMultHPByPCVH", "fDiffMultHPByPCL"
			};

			const std::string difficultyDamageToPC[6] = {
				"fDiffMultHPToPCVE", "fDiffMultHPToPCE", "fDiffMultHPToPCN",
				"fDiffMultHPToPCH", "fDiffMultHPToPCVH", "fDiffMultHPToPCL"
			};

			for (int i = 0; i < 6; ++i) {
				fDiffMultHPByPC[i] = gameSettings->GetSetting(difficultyDamageByPC[i].c_str())->GetFloat();
				fDiffMultHPToPC[i] = gameSettings->GetSetting(difficultyDamageToPC[i].c_str())->GetFloat();
			}

			logger::info("Loading Difficulty Settings: DONE");
		}

		static inline void InitializeProcessExplosionMarker()
		{
			logger::info("Initialize Process Explosion Markers...");

			const auto explosionFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSExplosion>();
			ModRuntimeMarker_Explosion = explosionFactory ? explosionFactory->Create() : nullptr; 
			if (!ModRuntimeMarker_Explosion) REPORT_AND_FAIL("Failed to initialize explosion marker.");

			ModRuntimeMarker_Explosion->model = "Effects\\FXEmptyObject.nif";
			ModRuntimeMarker_Explosion->data.radius = 0.0f;
			ModRuntimeMarker_Explosion->data.force = 0.0f;
			ModRuntimeMarker_Explosion->data.damage = 0.0f;
			ModRuntimeMarker_Explosion->data.flags.set(RE::BGSExplosionData::Flag::kIgnoreLOSCheck);
			ModRuntimeMarker_Explosion->data.flags.set(RE::BGSExplosionData::Flag::kIgnoreImageSpaceSwap);
			ModRuntimeMarker_Explosion->data.eSoundLevel = RE::SOUND_LEVEL::kSilent;
			
			logger::info("Initialize Process Explosion Markers: DONE");
		}

		static inline void InitializeProcessImpactData()
		{
			logger::info("Initialize Process Impact Data...");

			const auto impactDataFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSImpactData>();
			ModRuntime_Impact = impactDataFactory ? impactDataFactory->Create() : nullptr;
			if (!ModRuntime_Impact) REPORT_AND_FAIL("Failed to initialize mod runtime impact data.");

			auto applyImpactData = [](RE::BGSImpactData* impactData) {
				if (!impactData) return;

				impactData->model = "";
				impactData->data = {};
				impactData->dData = {};
				impactData->padAC = 0;
				impactData->decalTextureSet = nullptr;
				impactData->decalTextureSet2 = nullptr;
				impactData->sound1 = nullptr;
				impactData->sound2 = nullptr;
				impactData->hazard = nullptr;
				impactData->data.resultOverride = RE::ImpactResult::kNone;
			};

			applyImpactData(ModRuntime_Impact);

			logger::info("Initialize Process Impact Data: DONE");
		}

		static inline void InitializeProcessDefaultBloodImpact()
		{
			logger::info("Initialize Process Default Blood Impact...");

			const auto projectileFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSProjectile>();
			ModRuntimeBloodSpray_Projectile = projectileFactory ? projectileFactory->Create() : nullptr;  
			if (!ModRuntimeBloodSpray_Projectile) REPORT_AND_FAIL("Failed to initialize mod runtime blood projectile.");

			ModRuntimeBloodSpray_Projectile->data.flags = RE::BGSProjectileData::BGSProjectileFlags::kPassSMTransparent;
			ModRuntimeBloodSpray_Projectile->data.types = RE::BGSProjectileData::Type::kMissile;
			ModRuntimeBloodSpray_Projectile->data.gravity = 70.0f;
			ModRuntimeBloodSpray_Projectile->data.speed = 2700.0f;
			ModRuntimeBloodSpray_Projectile->data.range = 512.0f;
			ModRuntimeBloodSpray_Projectile->data.light = nullptr;
			ModRuntimeBloodSpray_Projectile->data.muzzleFlashLight = nullptr;
			ModRuntimeBloodSpray_Projectile->data.tracerChance = 0.0f;
			ModRuntimeBloodSpray_Projectile->data.explosionProximity = 0.0f;
			ModRuntimeBloodSpray_Projectile->data.explosionTimer = 0.0f;
			ModRuntimeBloodSpray_Projectile->data.explosionType = nullptr;
			ModRuntimeBloodSpray_Projectile->data.activeSoundLoop = nullptr;
			ModRuntimeBloodSpray_Projectile->data.muzzleFlashDuration = 0.0f;
			ModRuntimeBloodSpray_Projectile->data.fadeOutTime = 0.0f;
			ModRuntimeBloodSpray_Projectile->data.force = 0.0f;
			ModRuntimeBloodSpray_Projectile->data.countdownSound = nullptr;
			ModRuntimeBloodSpray_Projectile->data.deactivateSound = nullptr;
			ModRuntimeBloodSpray_Projectile->data.defaultWeaponSource = nullptr;
			ModRuntimeBloodSpray_Projectile->data.coneSpread = 0.0f;
			ModRuntimeBloodSpray_Projectile->data.collisionRadius = 5.0f;
			ModRuntimeBloodSpray_Projectile->data.lifetime = 0.1f;
			ModRuntimeBloodSpray_Projectile->data.relaunchInterval = 0.1f;
			ModRuntimeBloodSpray_Projectile->data.decalData = nullptr;
			ModRuntimeBloodSpray_Projectile->data.collisionLayer = ModRuntimeBlood_CollisionLayer;
			ModRuntimeBloodSpray_Projectile->soundLevel = RE::SOUND_LEVEL::kSilent;
			ModRuntimeBloodSpray_Projectile->model = "Effects\\FXEmptyObject.nif";

			const auto effectFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::EffectSetting>();
			ModRuntimeBloodSpray_Effect = effectFactory ? effectFactory->Create() : nullptr;  
			if (!ModRuntimeBloodSpray_Effect) REPORT_AND_FAIL("Failed to initialize mod runtime blood effect.");

			ModRuntimeBloodSpray_Effect->filterValidationFunction = nullptr;
			ModRuntimeBloodSpray_Effect->filterValidationItem = nullptr;
			ModRuntimeBloodSpray_Effect->data = RE::EffectSetting::EffectSettingData();
			ModRuntimeBloodSpray_Effect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kNoHitEffect);
			ModRuntimeBloodSpray_Effect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kNoDuration);
			ModRuntimeBloodSpray_Effect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kNoMagnitude);
			ModRuntimeBloodSpray_Effect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kNoArea);
			ModRuntimeBloodSpray_Effect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kFXPersist);
			ModRuntimeBloodSpray_Effect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kHideInUI);
			ModRuntimeBloodSpray_Effect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kNoRecast);
			ModRuntimeBloodSpray_Effect->data.baseCost = 0.0f;
			ModRuntimeBloodSpray_Effect->data.associatedForm = nullptr;
			ModRuntimeBloodSpray_Effect->data.associatedSkill = RE::ActorValue::kNone;
			ModRuntimeBloodSpray_Effect->data.resistVariable = RE::ActorValue::kNone;
			ModRuntimeBloodSpray_Effect->data.numCounterEffects = 0;
			ModRuntimeBloodSpray_Effect->data.light = nullptr;
			ModRuntimeBloodSpray_Effect->data.taperWeight = 0.0f;
			ModRuntimeBloodSpray_Effect->data.effectShader = nullptr;
			ModRuntimeBloodSpray_Effect->data.enchantShader = nullptr;
			ModRuntimeBloodSpray_Effect->data.minimumSkill = 0;
			ModRuntimeBloodSpray_Effect->data.spellmakingArea = 0;
			ModRuntimeBloodSpray_Effect->data.spellmakingChargeTime = 0.0f;
			ModRuntimeBloodSpray_Effect->data.taperCurve = 0.0f;
			ModRuntimeBloodSpray_Effect->data.taperDuration = 0.0f;
			ModRuntimeBloodSpray_Effect->data.secondAVWeight = 0.0f;
			ModRuntimeBloodSpray_Effect->data.archetype = RE::EffectArchetype::kScript;
			ModRuntimeBloodSpray_Effect->data.primaryAV = RE::ActorValue::kNone;
			ModRuntimeBloodSpray_Effect->data.projectileBase = ModRuntimeBloodSpray_Projectile;
			ModRuntimeBloodSpray_Effect->data.explosion = nullptr;
			ModRuntimeBloodSpray_Effect->data.castingType = RE::MagicSystem::CastingType::kFireAndForget;
			ModRuntimeBloodSpray_Effect->data.delivery = RE::MagicSystem::Delivery::kAimed;
			ModRuntimeBloodSpray_Effect->data.secondaryAV = RE::ActorValue::kNone;
			ModRuntimeBloodSpray_Effect->data.castingArt = nullptr;
			ModRuntimeBloodSpray_Effect->data.hitEffectArt = nullptr;
			ModRuntimeBloodSpray_Effect->data.impactDataSet = nullptr;
			ModRuntimeBloodSpray_Effect->data.skillUsageMult = 1.0f;
			ModRuntimeBloodSpray_Effect->data.dualCastData = nullptr;
			ModRuntimeBloodSpray_Effect->data.dualCastScale = 1.0f;
			ModRuntimeBloodSpray_Effect->data.enchantEffectArt = nullptr;
			ModRuntimeBloodSpray_Effect->data.hitVisuals = nullptr;
			ModRuntimeBloodSpray_Effect->data.enchantVisuals = nullptr;
			ModRuntimeBloodSpray_Effect->data.equipAbility = nullptr;
			ModRuntimeBloodSpray_Effect->data.imageSpaceMod = nullptr;
			ModRuntimeBloodSpray_Effect->data.perk = nullptr;
			ModRuntimeBloodSpray_Effect->data.castingSoundLevel = RE::SOUND_LEVEL::kSilent;
			ModRuntimeBloodSpray_Effect->data.aiScore = 0.0f;
			ModRuntimeBloodSpray_Effect->data.aiDelayTimer = 0.0f;
			ModRuntimeBloodSpray_Effect->counterEffects.clear();
			ModRuntimeBloodSpray_Effect->effectSounds.clear();
			ModRuntimeBloodSpray_Effect->magicItemDescription = "";
			ModRuntimeBloodSpray_Effect->effectLoadedCount = 0;
			ModRuntimeBloodSpray_Effect->associatedItemLoadedCount = 0;
			ModRuntimeBloodSpray_Effect->conditions = RE::TESCondition();

			const auto spellFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::SpellItem>();
			ModRuntimeBloodSpray_Spell = spellFactory ? spellFactory->Create() : nullptr; 
			if (!ModRuntimeBloodSpray_Spell) REPORT_AND_FAIL("Failed to initialize mod runtime blood spell.");

			ModRuntimeBloodSpray_Spell->data.costOverride = 0;
			ModRuntimeBloodSpray_Spell->data.flags = RE::SpellItem::SpellFlag::kNone;
			ModRuntimeBloodSpray_Spell->data.spellType = RE::MagicSystem::SpellType::kSpell;
			ModRuntimeBloodSpray_Spell->data.chargeTime = 0.0f;
			ModRuntimeBloodSpray_Spell->data.castingType = RE::MagicSystem::CastingType::kFireAndForget;
			ModRuntimeBloodSpray_Spell->data.delivery = RE::MagicSystem::Delivery::kAimed;
			ModRuntimeBloodSpray_Spell->data.castDuration = 0.0f;
			ModRuntimeBloodSpray_Spell->data.range = 0.0f;
			ModRuntimeBloodSpray_Spell->data.castingPerk = nullptr;

			RE::Effect* baseEffect = new RE::Effect();
			baseEffect->effectItem.magnitude = 0.0f;
			baseEffect->effectItem.area = 0;
			baseEffect->effectItem.duration = 0;
			baseEffect->cost = 0.0f;
			baseEffect->baseEffect = ModRuntimeBloodSpray_Effect;
			baseEffect->conditions = RE::TESCondition();

			ModRuntimeBloodSpray_Spell->effects = RE::BSTArray<RE::Effect*>();
			ModRuntimeBloodSpray_Spell->effects.push_back(baseEffect);

			logger::info("Initialize Process Default Blood Impact: DONE");
		}

		static inline void InitializeProcessCollisionLayer()
		{
			logger::info("Initialize Process Collision Layer...");

			const auto collisionFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSCollisionLayer>();
			ModRuntimeBlood_CollisionLayer = collisionFactory ? collisionFactory->Create() : nullptr; 
			if (!ModRuntimeBlood_CollisionLayer) REPORT_AND_FAIL("Failed to initialize projectile collision layer.");

			ModRuntimeBlood_CollisionLayer->name = "L_CIF_BLOOD_PROJECTILE";
			ModRuntimeBlood_CollisionLayer->collisionIdx = SettingsIni::iBloodSprayCollisionLayerIndex;
			ModRuntimeBlood_CollisionLayer->debugColor = RE::Color(128, 0, 0, 255);
			ModRuntimeBlood_CollisionLayer->flags = RE::BGSCollisionLayer::FLAG::kNone;
			ModRuntimeBlood_CollisionLayer->collidesWith.clear();
			ModRuntimeBlood_CollisionLayer->pad3C = 0;

			static auto SetupLayerBitfields = []() -> std::uint64_t {
				return (1ull << static_cast<std::uint64_t>(RE::COL_LAYER::kStatic)) |
				       (1ull << static_cast<std::uint64_t>(RE::COL_LAYER::kAnimStatic)) |
				       (1ull << static_cast<std::uint64_t>(RE::COL_LAYER::kClutter)) |
				       (1ull << static_cast<std::uint64_t>(RE::COL_LAYER::kTrees)) |
				       (1ull << static_cast<std::uint64_t>(RE::COL_LAYER::kProps)) |
				       (1ull << static_cast<std::uint64_t>(RE::COL_LAYER::kWater)) |
				       (1ull << static_cast<std::uint64_t>(RE::COL_LAYER::kTerrain)) |
				       (1ull << static_cast<std::uint64_t>(RE::COL_LAYER::kGround));
			};

			static RE::FormID previousCell = 0x0;

			TimeUtils::DoWhile(1s, [](TimeUtils::CallResult result, std::chrono::nanoseconds) {
				if (TimeUtils::IsEnd(result)) return true;

				auto player = RE::PlayerCharacter::GetSingleton();
				if (!player) return true;

				auto currentCell = player->GetParentCell();
				if (!currentCell || currentCell->formID == previousCell) return true;

				auto world = RE::NiPointer<RE::bhkWorld>(currentCell->GetbhkWorld());
				if (!world) return true;

				previousCell = currentCell->formID;
				TRACE("Moved to new cell [{:08X}]", currentCell->formID);

				{
					RE::BSWriteLockGuard lock(world->worldLock);
					RE::bhkCollisionFilter* worldFilter = (RE::bhkCollisionFilter*)world->GetWorld1()->collisionFilter;
					worldFilter->layerBitfields[static_cast<int32_t>(SettingsIni::iBloodSprayCollisionLayerIndex)] = SetupLayerBitfields();
					worldFilter->collisionLayerNames[static_cast<int32_t>(SettingsIni::iBloodSprayCollisionLayerIndex)] = "L_CIF_BLOOD_PROJECTILE";
					MiscUtils::ReSyncLayerBitfields(worldFilter, SettingsIni::iBloodSprayCollisionLayerIndex);
				}

				return true;
			}, true);

			logger::info("Initialize Process Collision Layer: DONE");
		}

		static inline void DefineMaterialsAndImpacts()
		{
			using namespace CoreStructure;

			logger::info("Define Materials And Impacts...");

			for (auto* race : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESRace>()) {
				if (!race || !race->bloodImpactMaterial) continue;

				if (originalMaterials.contains(race->bloodImpactMaterial)) {
					race->bloodImpactMaterial = originalMaterials[race->bloodImpactMaterial];
				} else {
					RE::BGSMaterialType* clonedMaterial = race->bloodImpactMaterial->As<RE::TESForm>()->CreateDuplicateForm(false, nullptr)->As<RE::BGSMaterialType>();
					if (!clonedMaterial) {
						logger::error("Failed to create cloned material of \"{:08X}\".", race->bloodImpactMaterial->formID);
						continue;
					}

					originalMaterials[race->bloodImpactMaterial] = clonedMaterial;
					originalMaterialsReversed[clonedMaterial] = race->bloodImpactMaterial;
					clonedMaterial->parentType = race->bloodImpactMaterial;
					race->bloodImpactMaterial = clonedMaterial;
				}
			}

			for (auto* impactSet : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::BGSImpactDataSet>()) {
				if (!impactSet || impactSet->IsDynamicForm()) continue;

				RE::BSTHashMap<const RE::BGSMaterialType*, RE::BGSImpactData*> newMap;
				for (auto& entry : impactSet->impactMap) {
					newMap.insert({ entry.first, entry.second });
					if (const auto materialOrigin = const_cast<RE::BGSMaterialType*>(entry.first)) {
						originalImpactMap[impactSet][materialOrigin] = entry.second;
					}
				}
				for (const auto& [origin, clone] : originalMaterials) {
					newMap.insert({ clone, ModRuntime_Impact });
				}
				impactSet->impactMap = std::move(newMap);
			}

			weaponDefault.impactData = weaponDefaultImpactData;
			weaponWood.impactData = weaponWoodImpactData;
			shieldHeavy.impactData = shieldHeavyImpactData;
			shieldLight.impactData = shieldLightImpactData;

			logger::info("Define Materials And Impacts: DONE");
		}
	};
}
