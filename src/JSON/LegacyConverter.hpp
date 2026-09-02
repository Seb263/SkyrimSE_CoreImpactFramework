#pragma once

#include "JSONHandler.h"

namespace JSONHandler
{
	class LegacyConverter
	{
	public:

		static void Convert(json& root)
		{
			logger::info("[Legacy] Converting V1 JSON to V2 format...");

			// BipedMapping
			if (root.contains("BipedMapping") && root["BipedMapping"].is_array()) {
				const int converted = ConvertBipedMapping(root["BipedMapping"]);
				if (converted > 0) logger::info("[Legacy] BipedMapping converted ({}/{} entries).", converted, root["BipedMapping"].size());
			}

			// ImpactMapping
			if (root.contains("ImpactMapping") && root["ImpactMapping"].is_array()) {
				int converted = 0;
				for (auto& item : root["ImpactMapping"]) {
					if (ConvertMappingFilters(item)) ++converted;
				}
				if (converted > 0) logger::info("[Legacy] ImpactMapping filters converted ({}/{} entries).", converted, root["ImpactMapping"].size());
			}

			// HitMapping
			if (root.contains("HitMapping") && root["HitMapping"].is_array()) {
				int converted = 0;
				for (auto& item : root["HitMapping"])
					if (ConvertMappingFilters(item)) ++converted;
				if (converted > 0) logger::info("[Legacy] HitMapping filters converted ({}/{} entries).", converted, root["HitMapping"].size());
			}

			logger::info("[Legacy] Converting V1 JSON to V2 format: DONE");
		}

	private:

		// -------------------------------------------------------------------------
		// BipedMapping conversion
		//
		// V1:  "BipedBones": { "30": ["NPC Spine", "NPC Spine1"], "32": ["NPC Head"] }
		//        key   = biped slot (int as string)
		//        value = array of bone node names
		//
		// V2:  "BipedBones": {
		//          "group_30": { "Priority": 0, "BipedSlots": [30], "Nodes": ["NPC Spine", "NPC Spine1"] },
		//          "group_32": { "Priority": 0, "BipedSlots": [32], "Nodes": ["NPC Head"] }
		//      }
		//
		// Strategy: one group per original slot key, named "group_<slot>".
		// -------------------------------------------------------------------------
		static int ConvertBipedMapping(json& bipedMappingArray)
		{
			int convertedCount = 0;
			if (!bipedMappingArray.is_array()) return convertedCount;

			for (auto& item : bipedMappingArray) {
				if (!item.contains("BipedBones") || !item["BipedBones"].is_object()) continue;

				json newBipedBones = json::object();
				for (const auto& [slotKey, nodesValue] : item["BipedBones"].items()) {
					if (nodesValue.is_object()) continue; // Already in V2 format

					int slotInt = -1;
					try { slotInt = std::stoi(slotKey); }
					catch (...) {
						logger::warn("[Legacy] BipedBones: invalid slot key '{}', skipping.", slotKey);
						continue;
					}

					if (!nodesValue.is_array()) continue;

					json groupData = json::object();
					groupData["Priority"] = 0;
					groupData["BipedSlots"] = json::array({ slotInt });
					groupData["Nodes"] = nodesValue; // already an array of strings

					newBipedBones["group_" + slotKey] = std::move(groupData);
				}

				if (!newBipedBones.empty()) {
					item["BipedBones"] = std::move(newBipedBones);
					convertedCount++;
				}
			}

			return convertedCount;
		}

		// -------------------------------------------------------------------------
		// Filter conversion for ImpactMapping / HitMapping entries
		//
		// V1 flat "Filters" object  =>  V2 "Filters": { "Victim": {...}, "HitContext": {...} }
		// -------------------------------------------------------------------------
		static bool ConvertMappingFilters(json& item)
		{
			if (!item.contains("Filters") || !item["Filters"].is_object()) return false;

			json& src = item["Filters"];

			// Already V2 if any known namespace key is present
			if (src.contains("Victim") || src.contains("Attacker") || src.contains("HitContext")) return false;

			json victim     = json::object();
			json hitContext = json::object();

			// Victim filters
			MoveKey(src, victim, "Races");
			MoveKey(src, victim, "Keywords");
			MoveKey(src, victim, "Skins");
			MoveKey(src, victim, "Materials");
			MoveKey(src, victim, "FormID");

			// HitContext filters (with renames)
			MoveKey(src, hitContext, "WeaponsType", "WeaponTypes");
			MoveKey(src, hitContext, "Weapons");
			MoveKey(src, hitContext, "WeaponKeywords", "SourceKeywords");
			MoveKey(src, hitContext, "Armors");
			MoveKey(src, hitContext, "ArmorKeywords");
			MoveKey(src, hitContext, "ArmorClasses");
			MoveKey(src, hitContext, "Projectiles");
			MoveKey(src, hitContext, "Attacks");
			MoveKey(src, hitContext, "Blocked");
			MoveKey(src, hitContext, "States");
			MoveKey(src, hitContext, "CriticalAttack", "Critical");
			MoveKey(src, hitContext, "SneakAttack", "Sneak");
			MoveKey(src, hitContext, "Globales", "&Globales");
			MoveKey(src, hitContext, "BipedSlot", "BipedSlots");
			MoveKey(src, hitContext, "Percentage");
			MoveKey(src, hitContext, "MaxHealth");
			MoveKey(src, hitContext, "MinDamage");
			MoveKey(src, hitContext, "Conditions", "&Conditions");

			// Build new Filters object
			json newFilters = json::object();
			if (!victim.empty()) newFilters["Victim"] = std::move(victim);
			if (!hitContext.empty()) newFilters["HitContext"] = std::move(hitContext);

			// Keep any remaining unknown keys at root level (forward-compat)
			for (auto& [k, v] : src.items()) {
				newFilters[k] = std::move(v);
			}

			if (!newFilters.empty()) item["Filters"] = std::move(newFilters);

			return true;
		}

		// Helpers

		static void MoveKey(json& src, json& dst, const std::string& srcKey, const std::string& dstKey = "")
		{
			if (!src.contains(srcKey) || src[srcKey].is_null()) return;
			dst[dstKey.empty() ? srcKey : dstKey] = std::move(src[srcKey]);
			src.erase(srcKey);
		}
	};
};
