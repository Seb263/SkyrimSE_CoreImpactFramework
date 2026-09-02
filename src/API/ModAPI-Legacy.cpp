#include "API/ModAPI-Legacy.h"

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Biped.hpp"

size_t CoreImpactFrameworkAPI_Legacy::CoreImpactFrameworkAPI_Legacy::GetAPIVersion() const
{
	return CIF_API_LEGACY_VERSION;
}

std::vector<uint32_t> CoreImpactFrameworkAPI_Legacy::CoreImpactFrameworkAPI_Legacy::GetVersion() const
{
	using namespace SKSE;
	const auto* plugin = PluginDeclaration::GetSingleton();
	auto        version = plugin->GetVersion();

	uint32_t versionMajor = plugin->GetVersion().major();
	uint32_t versionMinor = plugin->GetVersion().minor();
	uint32_t versionPatch = plugin->GetVersion().patch();

	std::vector<uint32_t> versionVector;
	versionVector.push_back(versionMajor);
	versionVector.push_back(versionMinor);
	versionVector.push_back(versionPatch);

	return versionVector;
}

CoreImpactFrameworkAPI_Legacy::BipedBonesMap CoreImpactFrameworkAPI_Legacy::CoreImpactFrameworkAPI_Legacy::GetBipedBonesMap(RE::Actor* actor) const
{
	auto invertedMapOpt = BipedFunctions::GetBipedModifier(actor);
	if (!invertedMapOpt) return {};

	std::unordered_map<int, std::vector<std::string>> legacyMap;

	for (const auto& [groupName, boneData] : invertedMapOpt.value()) {
		for (const auto& bipedNode : boneData.nodes) {
			for (int slot : boneData.slots) {
				legacyMap[slot].push_back(bipedNode);
			}
		}
	}

	return legacyMap;
}

std::variant<bool, int, float, std::string> CoreImpactFrameworkAPI_Legacy::CoreImpactFrameworkAPI_Legacy::GetIniValue(const std::string& key_section) const
{
	std::string fixed_key_section = key_section;
	auto sep = fixed_key_section.find(':');
	if (sep != std::string::npos) {
		fixed_key_section = fixed_key_section.substr(sep + 1) + ":" + fixed_key_section.substr(0, sep);
	}

	return SettingsIni::SettingsManager::GetSingleton().GetValueVariant(fixed_key_section).value_or(false);
}

bool CoreImpactFrameworkAPI_Legacy::CoreImpactFrameworkAPI_Legacy::UpdateIniValue(const std::string& key_section, const std::variant<bool, int, float, std::string>& value) const
{
	std::string fixed_key_section = key_section;
	auto sep = fixed_key_section.find(':');
	if (sep != std::string::npos) {
		fixed_key_section = fixed_key_section.substr(sep + 1) + ":" + fixed_key_section.substr(0, sep);
	}

	return std::visit([&](auto&& val) {
		return SettingsIni::SettingsManager::GetSingleton().SetValue(fixed_key_section, val);
	}, value);
}

RE::BGSCollisionLayer* CoreImpactFrameworkAPI_Legacy::CoreImpactFrameworkAPI_Legacy::GetBloodCollisionLayer() const
{
	return ModData::ModRuntimeBlood_CollisionLayer;
}
