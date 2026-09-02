#pragma once

#include "JSONHandler.h"

namespace JSONHandler
{
	class Biped
	{
	public:

		static void ProcessBipedMapping(const json& jsonData)
		{
			int count = 0;

			if (jsonData.contains("BipedMapping")) {
				bipedMappings.reserve(jsonData["BipedMapping"].size());

				for (const auto& item : jsonData["BipedMapping"]) {
					BipedMapping mapping;
					mapping.priority = item.value("Priority", 0);

					if (!item.contains("BipedBones") || !item["BipedBones"].is_object()) {
						logger::warn("BipedMapping [{}] has no Data, skipping.\nJSON content : {}", count, item.dump(4));
						continue;
					}

					for (const auto& [boneGroupName, boneGroupData] : item["BipedBones"].items()) {
						if (!boneGroupData.is_object()) continue;

						BipedMapping::BipedBoneData boneData{};
						boneData.priority = boneGroupData.value("Priority", 0);
						boneData.isLimbEntry = boneGroupData.value("IsLimbEntry", false);

						if (boneGroupData.contains("BipedSlots") && boneGroupData["BipedSlots"].is_array()) {
							for (const auto& slot : boneGroupData["BipedSlots"]) {
								if (slot.is_number_integer()) boneData.slots.push_back(slot.get<int>());
							}
						} else continue;

						if (boneGroupData.contains("Nodes") && boneGroupData["Nodes"].is_array()) {
							for (const auto& node : boneGroupData["Nodes"]) {
								if (node.is_string()) boneData.nodes.push_back(node.get<std::string>());
							}
						} else continue;

						mapping.bipedBones[boneGroupName] = std::move(boneData);
					}

					ParsedBipedFilters filters;
					if (item.contains("Filters") && item["Filters"].is_object()) {
						ParseBipedActorFilters(item["Filters"], filters.victimInc, filters.victimExc, filters.victimAll);
					}

					mapping.id = count++;
					bipedMappings.push_back(std::move(mapping));
					RegisterBipedMapping(&bipedMappings.back(), filters);
				}
			}

			logger::info("BipedMapping : {} mapping(s) loaded.", count);
		}

	private:

		static void ParseBipedActorFilters(const json& j, BipedActorFilters& inc, BipedActorFilters& exc, BipedActorFilters& all)
		{
			auto parseSex = [&](const std::string& key, std::vector<Filter::ActorSex>& out) {
				if (!j.contains(key) || j[key].is_null()) return;
				auto entries = j[key].is_array() ? j[key] : json::array({ j[key] });
				for (const auto& e : entries) {
					if (!e.is_string()) continue;
					std::string s = e.get<std::string>();
					if (s == "Male") out.push_back(Filter::ActorSex::kMale);
					else if (s == "Female") out.push_back(Filter::ActorSex::kFemale);
				}
			};

			auto parseSkeletons = [&](const std::string& key, std::vector<RE::FormID>& out) {
				if (!j.contains(key) || j[key].is_null()) return;
				auto entries = j[key].is_array() ? j[key] : json::array({ j[key] });
				for (const auto& e : entries) {
					if (!e.is_string()) continue;
					std::string s = e.get<std::string>();
					std::ranges::transform(s, s.begin(), [](unsigned char c) { return std::tolower(c); });
					out.push_back(static_cast<RE::FormID>(std::hash<std::string>{}(s)));
				}
			};

			Main::ParseMappingFilterFormJson<RE::TESRace, RE::FormID> (j, "Races", inc.races, true);
			Main::ParseMappingFilterFormJson<RE::TESRace, RE::FormID> (j, "!Races", exc.races, true);
			Main::ParseMappingFilterFormJson<RE::BGSKeyword, RE::FormID> (j, "Keywords", inc.keywords, true);
			Main::ParseMappingFilterFormJson<RE::BGSKeyword, RE::FormID> (j, "!Keywords", exc.keywords, true);
			Main::ParseMappingFilterFormJson<RE::BGSKeyword, RE::FormID> (j, "&Keywords", all.keywords, true);
			Main::ParseMappingFilterFormJson<RE::TESObjectARMO, RE::FormID> (j, "Skins", inc.skins, false);
			Main::ParseMappingFilterFormJson<RE::TESObjectARMO, RE::FormID> (j, "!Skins", exc.skins, false);
			Main::ParseMappingFilterFormJson<RE::BGSMaterialType, RE::FormID> (j, "Materials", inc.materials, false);
			Main::ParseMappingFilterFormJson<RE::BGSMaterialType, RE::FormID> (j, "!Materials", exc.materials, false);
			Main::ParseMappingFilterFormJson<RE::TESForm, RE::FormID> (j, "FormID", inc.formIds, false);
			Main::ParseMappingFilterFormJson<RE::TESForm, RE::FormID> (j, "!FormID", exc.formIds, false);
			Main::ParseMappingFilterFormJson<RE::BGSPerk, RE::BGSPerk*> (j, "Conditions", inc.conditions, false);

			parseSex("Sex", inc.sexes);
			parseSex("!Sex", exc.sexes);
			parseSkeletons("Skeletons", inc.skeletons);
			parseSkeletons("!Skeletons", exc.skeletons);
		}

		static void FillBipedTrie(BipedTrieNode& root, const BipedActorFilters& f, BipedMapping* mapping, TrieFillMode mode)
		{
			switch (mode) {
				case TrieFillMode::kExclude: {
					bool hasAny =
						!f.races.empty() || !f.keywords.empty() || !f.skins.empty() ||
						!f.materials.empty() || !f.sexes.empty() || !f.formIds.empty() ||
						!f.skeletons.empty();
					if (!hasAny) return;
					break;
				}
				case TrieFillMode::kAll: {
					std::vector<std::pair<uint8_t, RE::FormID>> allFilters;
					for (auto id : f.races) allFilters.emplace_back((uint8_t)TrieLevelActor::kRace, id);
					for (auto id : f.keywords) allFilters.emplace_back((uint8_t)TrieLevelActor::kKeyword, id);
					for (auto id : f.skins) allFilters.emplace_back((uint8_t)TrieLevelActor::kSkin, id);
					for (auto id : f.materials) allFilters.emplace_back((uint8_t)TrieLevelActor::kMaterial, id);
					for (auto id : f.formIds) allFilters.emplace_back((uint8_t)TrieLevelActor::kFormID, id);
					if (allFilters.empty()) return;

					auto it = std::ranges::find_if(root.buckets, [&](const auto& b) {
						return b.conditions == f.conditions && b.allFilters == allFilters;
					});
					if (it == root.buckets.end()) {
						root.buckets.push_back({ f.conditions, allFilters, {} });
						it = root.buckets.end() - 1;
					}
					it->mappings.push_back(mapping);
					return;
				}
				case TrieFillMode::kInclude:
				default: break;
			}

			auto toIDs = [](const std::vector<RE::FormID>& v) { return v.empty() ? std::vector<RE::FormID>{0x0} : v; };
			auto toEnum = [](const auto& v, auto any) {
				using T = std::decay_t<decltype(any)>;
				return v.empty() ? std::vector<T>{any} : v;
			};

			for (auto race : toIDs(f.races))
			for (auto keyword : toIDs(f.keywords))
			for (auto skin : toIDs(f.skins))
			for (auto material : toIDs(f.materials))
			for (auto sex : toEnum(f.sexes, Filter::ActorSex::kAny))
			for (auto skeleton : toIDs(f.skeletons))
			for (auto formID : toIDs(f.formIds)) {
				BipedTrieNode* node = &root;
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kRace, race)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kKeyword, keyword)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kSkin, skin)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kMaterial, material)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kSex, (uint32_t)sex)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kSkeleton, skeleton)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kFormID, formID)];

				auto it = std::ranges::find_if(node->buckets, [&](const auto& b) {
					return b.conditions == f.conditions;
				});
				if (it == node->buckets.end()) {
					node->buckets.push_back({ f.conditions, {}, {} });
					it = node->buckets.end() - 1;
				}
				it->mappings.push_back(mapping);
			}
		}

		static void RegisterBipedMapping(BipedMapping* mapping, const ParsedBipedFilters& filters)
		{
			FillBipedTrie(bipedRegistry.victimInclude, filters.victimInc, mapping, TrieFillMode::kInclude);
			FillBipedTrie(bipedRegistry.victimExclude, filters.victimExc, mapping, TrieFillMode::kExclude);
			FillBipedTrie(bipedRegistry.victimInclude, filters.victimAll, mapping, TrieFillMode::kAll);
		}
	};
};
