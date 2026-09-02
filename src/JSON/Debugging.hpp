#pragma once

#include "JSONHandler.h"

namespace JSONHandler
{
	class Debugging
	{
	public:

		static void TraceBipedRegistry()
		{
			TRACE("=== START - BipedRegistry ===");
			TraceBipedTrie(bipedRegistry.victimInclude, "victimInclude");
			TraceBipedTrie(bipedRegistry.victimExclude, "victimExclude");
			TRACE("=== END - BipedRegistry ===");
		}

		template <typename RegistryType>
		static void TraceRegistry(const RegistryType& registry, const std::string& registryName)
		{
			TRACE("=== START - {} ===", registryName);
			TraceActorTrie(registry.victimInclude, "victimInclude");
			TraceActorTrie(registry.victimExclude, "victimExclude");
			TraceActorTrie(registry.attackerInclude, "attackerInclude");
			TraceActorTrie(registry.attackerExclude, "attackerExclude");
			TraceHitContextTrie(registry.hitContextInclude, "hitContextInclude");
			TraceHitContextTrie(registry.hitContextExclude, "hitContextExclude");
			TRACE("=== END - {} ===", registryName);
		}

	private:

		template <typename MappingType>
		static void TraceActorTrie(const ActorTrieNode<MappingType>& root, const std::string& mapName)
		{
			TRACE("  [{}]", mapName);

			std::function<void(const ActorTrieNode<MappingType>&, int, uint8_t, uint32_t)> traverse;
			traverse = [&](const ActorTrieNode<MappingType>& node, int depth, uint8_t level, uint32_t value)
			{
				std::function<bool(const ActorTrieNode<MappingType>&)> hasMappings;
				hasMappings = [&](const ActorTrieNode<MappingType>& n) -> bool {
					for (const auto& b : n.buckets) if (!b.mappings.empty()) return true;
					for (const auto& [k, child] : n.children) if (hasMappings(child)) return true;
					return false;
				};
				if (!hasMappings(node)) return;

				// Buckets
				for (size_t i = 0; i < node.buckets.size(); ++i) {
					const auto& bucket = node.buckets[i];
					if (bucket.mappings.empty()) continue;

					if (!bucket.allFilters.empty()) {
						TRACE("      Bucket[{}] [kAll] - allFilters={} mappings={}", i, bucket.allFilters.size(), bucket.mappings.size());
						for (const auto& [level, id] : bucket.allFilters) {
							const char* levelName = "?";
							switch (static_cast<TrieLevelActor>(level)) {
								case TrieLevelActor::kRace: levelName = "Race"; break;
								case TrieLevelActor::kKeyword: levelName = "Keyword"; break;
								case TrieLevelActor::kSkin: levelName = "Skin"; break;
								case TrieLevelActor::kMaterial: levelName = "Material"; break;
								case TrieLevelActor::kWorn: levelName = "Worn"; break;
								case TrieLevelActor::kWornKeyword: levelName = "WornKeyword"; break;
								case TrieLevelActor::kPerk: levelName = "Perk"; break;
								case TrieLevelActor::kSpell: levelName = "Spell"; break;
								case TrieLevelActor::kMagicEffect: levelName = "MagicEffect"; break;
								case TrieLevelActor::kSkeleton: levelName = "SkeletonHash"; break;
								case TrieLevelActor::kFormID: levelName = "FormID"; break;
								default: levelName = "?"; break;
							}
							TRACE("        &{}={:08X}", levelName, id);
						}
					} else {
						TRACE("      Bucket[{}] - mappings={}", i, bucket.mappings.size());
						for (const auto* m : bucket.mappings) {
							if constexpr (std::is_same_v<MappingType, ImpactMapping>) {
								TRACE("        ImpactMapping id={} priority={} override={} overrideMerge={}",
									m->id, m->priority, m->override, m->overrideMerge);
							} else if constexpr (std::is_same_v<MappingType, HitMapping>) {
								TRACE("        HitMapping id={} class='{}' priority={} deferred={} override={} overrideMerge={}",
									m->id, m->className, m->priority, m->modifier.deferred, m->override, m->overrideMerge);
							}
						}
					}
				}

				// Childs
				for (const auto& [key, child] : node.children) {
					if (!hasMappings(child)) continue;
					uint8_t childLevel = (uint8_t)(key >> 32);
					uint32_t childValue = (uint32_t)(key & 0xFFFFFFFF);

					const char* levelName = "?";
					switch (static_cast<TrieLevelActor>(childLevel)) {
						case TrieLevelActor::kRace: levelName = "Race"; break;
						case TrieLevelActor::kKeyword: levelName = "Keyword"; break;
						case TrieLevelActor::kSkin: levelName = "Skin"; break;
						case TrieLevelActor::kSex: levelName = "Sex"; break;
						case TrieLevelActor::kMaterial: levelName = "Material"; break;
						case TrieLevelActor::kWorn: levelName = "Worn"; break;
						case TrieLevelActor::kWornKeyword: levelName = "WornKeyword"; break;
						case TrieLevelActor::kPerk: levelName = "Perk"; break;
						case TrieLevelActor::kSpell: levelName = "Spell"; break;
						case TrieLevelActor::kMagicEffect: levelName = "MagicEffect"; break;
						case TrieLevelActor::kSkeleton: levelName = "SkeletonHash"; break;
						case TrieLevelActor::kFormID: levelName = "FormID"; break;
						default: levelName = "?"; break;
					}
					TRACE("{}[{}] {}={:08X}", std::string(depth * 2, ' '), depth, levelName, childValue);
					traverse(child, depth + 1, childLevel, childValue);
				}
			};

			traverse(root, 0, 0xFF, 0);
		}

		template <typename MappingType>
		static void TraceHitContextTrie(const HitCtxTrieNode<MappingType>& root, const std::string& mapName)
		{
			TRACE("  [{}]", mapName);

			std::function<void(const HitCtxTrieNode<MappingType>&, int)> traverse;
			traverse = [&](const HitCtxTrieNode<MappingType>& node, int depth)
			{
				std::function<bool(const HitCtxTrieNode<MappingType>&)> hasMappings;
				hasMappings = [&](const HitCtxTrieNode<MappingType>& n) -> bool {
					for (const auto& b : n.buckets) if (!b.mappings.empty()) return true;
					for (const auto& [k, child] : n.children) if (hasMappings(child)) return true;
					return false;
				};
				if (!hasMappings(node)) return;

				// Buckets
				for (size_t i = 0; i < node.buckets.size(); ++i) {
					const auto& bucket = node.buckets[i];
					if (bucket.mappings.empty()) continue;

					if (!bucket.allFilters.empty()) {
						TRACE("      Bucket[{}] [kAll] - allFilters={} mappings={}", i, bucket.allFilters.size(), bucket.mappings.size());
						for (const auto& [level, id] : bucket.allFilters) {
							const char* levelName = "?";
							switch (static_cast<TrieLevelHitContext>(level)) {
								case TrieLevelHitContext::kWeaponKeyword: levelName = "WeaponKeyword"; break;
								case TrieLevelHitContext::kArmorKeyword: levelName = "ArmorKeyword"; break;
								case TrieLevelHitContext::kBipedSlot: levelName = "BipedSlot"; break;
								case TrieLevelHitContext::kBipedKey: levelName = "BipedKey"; break;
								default: levelName = "?"; break;
							}
							TRACE("        &{}={:08X}", levelName, id);
						}
					} else {
						TRACE("      Bucket[{}] - globalesAny={} globalesAll={} globalesNone={} conditionsAny={} conditionsAll={} conditionsNone={} mappings={}",
							i, bucket.globalesAny.size(), bucket.globalesAll.size(), bucket.globalesNone.size(),
							bucket.conditionsAny.size(), bucket.conditionsAll.size(), bucket.conditionsNone.size(),
							bucket.mappings.size());

						auto traceGlobales = [](const std::vector<Filter::GlobalFilter>& globales, const char* label) {
							for (const auto& g : globales) {
								TRACE("        [{}] Global edid='{}' cmp={} val={}", label,
									g.global ? g.global->GetFormEditorID() : "null",
									magic_enum::enum_name(g.comparison), g.value);
							}
						};
						traceGlobales(bucket.globalesAny, "OR");
						traceGlobales(bucket.globalesAll, "AND");
						traceGlobales(bucket.globalesNone, "NOR");

						auto traceVariant = [](const std::variant<float, RE::TESGlobal*>& v, const std::string& label) {
							if (std::holds_alternative<float>(v)) {
								if (std::get<float>(v) >= 0.0f) TRACE(" {}: {}", label, std::get<float>(v));
							} else {
								auto* g = std::get<RE::TESGlobal*>(v);
								TRACE("        {}: Global='{}'", label, g ? g->GetFormEditorID() : "null");
							}
						};
						traceVariant(bucket.percentage, "Percentage");
						traceVariant(bucket.maxHealth, "MaxHealth");
						traceVariant(bucket.minDamage, "MinDamage");

						for (const auto* m : bucket.mappings) {
							if constexpr (std::is_same_v<MappingType, ImpactMapping>) {
								TRACE("        ImpactMapping id={} priority={} override={} overrideMerge={}",
									m->id, m->priority, m->override, m->overrideMerge);
							} else if constexpr (std::is_same_v<MappingType, HitMapping>) {
								TRACE("        HitMapping id={} class='{}' priority={} deferred={} override={} overrideMerge={}",
									m->id, m->className, m->priority, m->modifier.deferred, m->override, m->overrideMerge);
							}
						}
					}
				}

				// Childs
				for (const auto& [key, child] : node.children) {
					if (!hasMappings(child)) continue;
					uint8_t childLevel = (uint8_t)(key >> 32);
					uint32_t childValue = (uint32_t)(key & 0xFFFFFFFF);

					const char* levelName = "?";
					switch (static_cast<TrieLevelHitContext>(childLevel)) {
						case TrieLevelHitContext::kWeapon: levelName = "Weapon"; break;
						case TrieLevelHitContext::kWeaponKeyword: levelName = "WeaponKw"; break;
						case TrieLevelHitContext::kMagicItem: levelName = "MagicItem"; break;
						case TrieLevelHitContext::kProjectile: levelName = "Projectile"; break;
						case TrieLevelHitContext::kMagicEffect: levelName = "MagicEffect"; break;
						case TrieLevelHitContext::kArmor: levelName = "Armor"; break;
						case TrieLevelHitContext::kArmorKeyword: levelName = "ArmorKw"; break;
						case TrieLevelHitContext::kWeaponType: levelName = "WeaponType"; break;
						case TrieLevelHitContext::kArmorClass: levelName = "ArmorClass"; break;
						case TrieLevelHitContext::kBlocked: levelName = "Blocked"; break;
						case TrieLevelHitContext::kState: levelName = "State"; break;
						case TrieLevelHitContext::kAttack: levelName = "Attack"; break;
						case TrieLevelHitContext::kSource: levelName = "Source"; break;
						case TrieLevelHitContext::kCritical: levelName = "Critical"; break;
						case TrieLevelHitContext::kSneak: levelName = "Sneak"; break;
						case TrieLevelHitContext::kBipedSlot: levelName = "BipedSlot"; break;
						case TrieLevelHitContext::kBipedLimb: levelName = "BipedLimb"; break;
						case TrieLevelHitContext::kBipedNode: levelName = "BipedNode"; break;
						case TrieLevelHitContext::kBipedKey: levelName = "BipedKey"; break;
						default: levelName = "?"; break;
					}
					TRACE("{}[{}] {}={:08X}", std::string(depth * 2, ' '), depth, levelName, childValue);
					traverse(child, depth + 1);
				}
			};

			traverse(root, 0);
		}

		static void TraceBipedTrie(const BipedTrieNode& root, const std::string& mapName)
		{
			TRACE("  [{}]", mapName);

			std::function<void(const BipedTrieNode&, int)> traverse;
			traverse = [&](const BipedTrieNode& node, int depth)
			{
				std::function<bool(const BipedTrieNode&)> hasMappings;
				hasMappings = [&](const BipedTrieNode& n) -> bool {
					for (const auto& b : n.buckets) if (!b.mappings.empty()) return true;
					for (const auto& [k, child] : n.children) if (hasMappings(child)) return true;
					return false;
				};
				if (!hasMappings(node)) return;

				// Buckets
				for (size_t i = 0; i < node.buckets.size(); ++i) {
					const auto& bucket = node.buckets[i];
					if (bucket.mappings.empty()) continue;

					if (!bucket.allFilters.empty()) {
						TRACE("      Bucket[{}] [kAll] - allFilters={} mappings={}", i, bucket.allFilters.size(), bucket.mappings.size());
						for (const auto& [level, id] : bucket.allFilters) {
							const char* levelName = "?";
							switch (static_cast<TrieLevelActor>(level)) {
								case TrieLevelActor::kRace: levelName = "Race"; break;
								case TrieLevelActor::kKeyword: levelName = "Keyword"; break;
								case TrieLevelActor::kSkin: levelName = "Skin"; break;
								case TrieLevelActor::kMaterial: levelName = "Material"; break;
								case TrieLevelActor::kSex: levelName = "Sex"; break;
								case TrieLevelActor::kSkeleton: levelName = "SkeletonHash"; break;
								case TrieLevelActor::kFormID: levelName = "FormID"; break;
								default: levelName = "?"; break;
							}
							TRACE("        &{}={:08X}", levelName, id);
						}
					} else {
						TRACE("      Bucket[{}] - conditions={} mappings={}", i, bucket.conditions.size(), bucket.mappings.size());
						for (const auto* m : bucket.mappings) {
							TRACE("        BipedMapping id={} priority={} bipedBones={}",
								m->id, m->priority, m->bipedBones.size());
						}
					}
				}

				// Childs
				for (const auto& [key, child] : node.children) {
					if (!hasMappings(child)) continue;
					uint8_t childLevel = (uint8_t)(key >> 32);
					uint32_t childValue = (uint32_t)(key & 0xFFFFFFFF);

					const char* levelName = "?";
					switch (static_cast<TrieLevelActor>(childLevel)) {
						case TrieLevelActor::kRace: levelName = "Race"; break;
						case TrieLevelActor::kKeyword: levelName = "Keyword"; break;
						case TrieLevelActor::kSkin: levelName = "Skin"; break;
						case TrieLevelActor::kMaterial: levelName = "Material"; break;
						case TrieLevelActor::kSex: levelName = "Sex"; break;
						case TrieLevelActor::kSkeleton: levelName = "SkeletonHash"; break;
						case TrieLevelActor::kFormID: levelName = "FormID"; break;
						default: levelName = "?"; break;
					}
					TRACE("{}[{}] {}={:08X}", std::string(depth * 2, ' '), depth, levelName, childValue);
					traverse(child, depth + 1);
				}
			};

			traverse(root, 0);
		}
	};
};
