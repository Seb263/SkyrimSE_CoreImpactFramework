#include "API/ModAPI.h"

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Biped.hpp"
#include "Core/DeferredHit.hpp"
#include "Core/Modifiers.hpp"

#include "Features/BloodSpray.hpp"

namespace CIF_API
{
    class Impl_Interface : public Interface_V3
    {
    public:
        static Impl_Interface* GetSingleton() noexcept
        {
            static Impl_Interface instance;
            return &instance;
        }

        REL::Version GetVersion() noexcept override
        {
            const auto plugin{ SKSE::PluginDeclaration::GetSingleton() };
            const auto name{ plugin->GetName() };
            const auto version{ plugin->GetVersion() };

            return version;
        }

        IniValue GetIniValue(const std::string& key_section, const IniValue& defaultValue) noexcept override
        {
            return SettingsIni::SettingsManager::GetSingleton().GetValueVariant(key_section).value_or(defaultValue);
        }

        bool SetIniValue(const std::string& key_section, const IniValue& value) noexcept override
        {
            return std::visit([&](auto&& val) {
                return SettingsIni::SettingsManager::GetSingleton().SetValue(key_section, val);
            }, value);
        }

        BipedBonesMap GetBipedBonesMap(RE::Actor* actor, bool limbEntriesOnly) noexcept override
        {
            BipedBonesMap result;

            auto invertedMapOpt = BipedFunctions::GetBipedModifier(actor);
            if (!invertedMapOpt.has_value()) return result;

            for (const auto& [key, boneData] : invertedMapOpt.value()) {
                if (limbEntriesOnly && !boneData.isLimbEntry) continue;

                BipedBonesEntry entry;
                entry.priority = boneData.priority;
                entry.isLimbEntry = boneData.isLimbEntry;
                entry.bipedNodes = boneData.nodes;
                entry.bipedSlots = boneData.slots;

                result[key] = std::move(entry);
            }

            return result;
        }

        RuntimeHitContext GenerateContext(RE::Actor* victim, RE::Actor* attacker = nullptr) noexcept override
        {
            RE::HitData hitData{};
            CoreStructure::DeferredHitStruct deferredHit{};

            hitData.target = victim ? victim->GetHandle() : RE::ActorHandle{};
            hitData.aggressor = attacker ? attacker->GetHandle() : RE::ActorHandle{};

            deferredHit.victimHandle = hitData.target;
            deferredHit.attackerHandle = hitData.aggressor;

            ModifiersFunctions::BuildRuntimeContext(deferredHit, hitData);

            return deferredHit.ctx;
        }

        RE::BGSCollisionLayer* GetBloodCollisionLayer() noexcept override
        {
            return ModData::ModRuntimeBlood_CollisionLayer;
        }

        bool CastBloodSpray(RE::TESObjectREFR* caster, RE::SpellItem* spell, RE::NiPoint3 position, RE::NiPoint3 direction, float power = 1.0f, float spread = -1.0f) noexcept override
        {
            return BloodSpray::CastBloodSpray(caster, spell, position, direction, power, spread);
        }

        void RegisterPreHitCallback(const std::string& eventName, int priority, PreHitCallback callback) noexcept override
        {
            std::lock_guard lock(callbackMutex);

            std::erase_if(preHitCallbacks, [&](const auto& e) { return e.eventName == eventName; });
            preHitCallbacks.push_back({ priority, eventName, std::move(callback) });

            std::sort(preHitCallbacks.begin(), preHitCallbacks.end(), [](const auto& a, const auto& b) { return a.priority > b.priority; });
        }

        void RegisterPostHitCallback(const std::string& eventName, int priority, PostHitCallback callback) noexcept override
        {
            std::lock_guard lock(callbackMutex);

            std::erase_if(postHitCallbacks, [&](const auto& e) { return e.eventName == eventName; });
            postHitCallbacks.push_back({ priority, eventName, std::move(callback) });

            std::sort(postHitCallbacks.begin(), postHitCallbacks.end(), [](const auto& a, const auto& b) { return a.priority > b.priority; });
        }

        void RegisterPostDeferredHitCallback(const std::string& eventName, int priority, PostHitCallback callback) noexcept override
        {
            std::lock_guard lock(callbackMutex);

            std::erase_if(postDeferredHitCallbacks, [&](const auto& e) { return e.eventName == eventName; });
            postDeferredHitCallbacks.push_back({ priority, eventName, std::move(callback) });

            std::sort(postDeferredHitCallbacks.begin(), postDeferredHitCallbacks.end(), [](const auto& a, const auto& b) { return a.priority > b.priority; });
        }

        BipedEntry GetBipedEntry(RE::Actor* actor, const RE::NiPoint3& hitPosition) noexcept override
        {
			if (!actor) return {};

            auto bipedBonesOpt = BipedFunctions::GetBipedModifier(actor);
            if (!bipedBonesOpt.has_value()) return {};

			const auto bipedBones = GetBipedBonesMap(actor, true);
			
			return BipedFunctions::GetHitBipedSlots(actor, hitPosition, *bipedBonesOpt);
        }
		
       	void ProcessHit(RE::HitData& hitData, const bool runtimeReady) noexcept override
        {
			if (auto* victim = MiscUtils::ResolveHandle<RE::Actor>(hitData.target)) {
				TRACE("API -> ProcessHit (hitData)");

				DeferredHitFunctions::SetDeferredHit(hitData, runtimeReady);
				DeferredHitFunctions::ProcessHitNormalized(victim->formID, &hitData);
			}
        }
		
       	void ProcessHit(RE::Actor* victim, RE::Projectile* projectile, const bool runtimeReady) noexcept override
        {
			if (!victim || !projectile) return;

			TRACE("API -> ProcessHit on actor \"{:08X}\"", victim->formID);

			DeferredHitFunctions::SetDeferredHit(projectile, victim, runtimeReady);
			DeferredHitFunctions::ProcessHitNormalized(victim->formID, std::nullopt);
        }

        void InvokePreHitCallbacks(RE::HitData& hitData)
        {
            std::shared_lock lock(callbackMutex);
            for (const auto& e : preHitCallbacks) e.callback(e.eventName, hitData);
        }

        void InvokePostHitCallbacks(const RuntimeHitContext& context)
        {
            std::shared_lock lock(callbackMutex);
            for (const auto& e : postHitCallbacks) e.callback(e.eventName, context);
        }

        void InvokePostDeferredHitCallbacks(const RuntimeHitContext& context)
        {
            std::shared_lock lock(callbackMutex);
            for (const auto& e : postDeferredHitCallbacks) e.callback(e.eventName, context);
        }

    private:
        struct PreHitEntry {
            int priority;
            std::string eventName;
            PreHitCallback callback;
        };
        struct PostHitEntry {
            int priority;
            std::string eventName;
            PostHitCallback callback;
        };

        std::shared_mutex callbackMutex;
        std::vector<PreHitEntry> preHitCallbacks;
        std::vector<PostHitEntry> postHitCallbacks;
        std::vector<PostHitEntry> postDeferredHitCallbacks;
    };
}

namespace CIF_API::Internal
{
    void InvokePreHitCallbacks(RE::HitData& hitData)
    {
        Impl_Interface::GetSingleton()->InvokePreHitCallbacks(hitData);
    }

    void InvokePostHitCallbacks(const Interface::RuntimeHitContext context)
    {
        Impl_Interface::GetSingleton()->InvokePostHitCallbacks(context);
    }

    void InvokePostDeferredHitCallbacks(const Interface::RuntimeHitContext context)
    {
        Impl_Interface::GetSingleton()->InvokePostDeferredHitCallbacks(context);
    }
}

extern "C" DLLEXPORT void* SKSEAPI RequestPluginAPI(CIF_API::InterfaceVersion version, const char* pluginName, REL::Version pluginVersion)
{
    if (!pluginName) {
        logger::error("CIF_API::RequestPluginAPI called with a nullptr plugin name");
        return nullptr;
    }

    void* api = nullptr;

    switch (version)
    {
        case CIF_API::InterfaceVersion::V1:
            api = static_cast<CIF_API::Interface_V1*>(CIF_API::Impl_Interface::GetSingleton());
            break;
        case CIF_API::InterfaceVersion::V2:
            api = static_cast<CIF_API::Interface_V2*>(CIF_API::Impl_Interface::GetSingleton());
            break;
        case CIF_API::InterfaceVersion::V3:
            api = static_cast<CIF_API::Interface_V3*>(CIF_API::Impl_Interface::GetSingleton());
            break;
        default:
            logger::warn("RequestPluginAPI called with invalid InterfaceVersion {}", static_cast<uint8_t>(version));
            return nullptr;
    }

    logger::info("RequestPluginAPI called: [InterfaceVersion:{}], [PluginName:{}], [PluginVersion:{}]",
        magic_enum::enum_name(version), pluginName, pluginVersion.string("."));

    return api;
}
