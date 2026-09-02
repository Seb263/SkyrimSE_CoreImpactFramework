#pragma once

#include "Core/Structure.h"
#include "Core/Filters.hpp"

#include "Utils/NiUtils.hpp"

class BipedFunctions
{
public:

	static std::vector<CoreStructure::BipedMapping*> LookupBipedTrie(const CoreStructure::BipedTrieNode& root, RE::Actor* target)
	{
		using namespace CoreStructure;
		std::vector<BipedMapping*> result;

		if (!target) return result;

		RE::TESNPC* base = target->GetActorBase();
		RE::TESRace* race = target->GetRace();
		RE::TESObjectARMO* skin = target->GetSkin();

		RE::BGSMaterialType* runtimeMat = race ? race->bloodImpactMaterial : nullptr;
		RE::BGSMaterialType* material = (runtimeMat && ModData::originalMaterialsReversed.contains(runtimeMat))
			? ModData::originalMaterialsReversed[runtimeMat] : nullptr;

		Filter::ActorSex sex = (base && base->GetSex() == RE::SEX::kFemale) ? Filter::ActorSex::kFemale : Filter::ActorSex::kMale;

		const RE::SEX sexIndex = (sex == Filter::ActorSex::kFemale) ? RE::SEX::kFemale : RE::SEX::kMale;
		RE::FormID skeletonHash = 0x0;
		if (race) {
			if (std::string path = race->skeletonModels[sexIndex].model.c_str(); !path.empty()) {
				std::ranges::transform(path, path.begin(), [](unsigned char c) { return std::tolower(c); });
				skeletonHash = static_cast<RE::FormID>(std::hash<std::string>{}(path));
			}
		}

		const RE::FormID raceFormID = race ? race->formID : 0x0;
		const RE::FormID skinFormID = skin ? skin->formID : 0x0;
		const RE::FormID materialFormID = material ? material->formID : 0x0;
		const RE::FormID baseFormID = base ? base->formID : 0x0;
		const RE::FormID formID = target->formID;

		const std::vector<RE::FormID> keywords = FiltersFunctions::GetActorKeywordsFormIDs(target);

		auto makeList = [](const std::vector<RE::FormID>& ids) {
			std::vector<RE::FormID> v = ids; v.push_back(0x0); return v;
		};

		const std::array<RE::FormID, 2> races = { raceFormID, 0x0 };
		const std::vector<RE::FormID> kws = makeList(keywords);
		const std::array<RE::FormID, 2> skins = { skinFormID, 0x0 };
		const std::array<RE::FormID, 2> materials = { materialFormID, 0x0 };
		const std::array<Filter::ActorSex, 2> sexes = { sex, Filter::ActorSex::kAny };
		const std::array<RE::FormID, 2> skeletons = { skeletonHash, 0x0 };
		const std::array<RE::FormID, 3> fids = { formID, baseFormID, 0x0 };

		auto step_buckets = [&](const BipedTrieNode& node) {
			for (const auto& bucket : node.buckets) {
				if (!bucket.conditions.empty() &&
					std::ranges::none_of(bucket.conditions, [&](auto* perk) { return FiltersFunctions::HasValidConditionFilter(target, nullptr, perk); })) continue;

				for (const auto& [level, id] : bucket.allFilters) {
					// kAll slow filters: only kKeyword is a list for the biped
					if (static_cast<TrieLevelActor>(level) == TrieLevelActor::kKeyword) {
						if (!std::ranges::contains(keywords, id)) goto skip;
					}
				}
				for (auto* m : bucket.mappings) {
					bool passesAll = true;
					for (const auto& rootBucket : root.buckets) {
						if (rootBucket.allFilters.empty()) continue;
						if (!std::ranges::contains(rootBucket.mappings, m)) continue;
						for (const auto& [level, id] : rootBucket.allFilters) {
							if (static_cast<TrieLevelActor>(level) == TrieLevelActor::kKeyword) {
								if (!std::ranges::contains(keywords, id)) { passesAll = false; break; }
							}
						}
						if (!passesAll) break;
					}
					if (passesAll) result.push_back(m);
				}
				continue;
				skip:;
			}
		};

		for (auto r : races) {
			auto it0 = root.children.find(TrieKey((uint8_t)TrieLevelActor::kRace, r));
			if (it0 == root.children.end()) continue;
		for (auto kw : kws) {
			auto it1 = it0->second.children.find(TrieKey((uint8_t)TrieLevelActor::kKeyword, kw));
			if (it1 == it0->second.children.end()) continue;
		for (auto sk : skins) {
			auto it2 = it1->second.children.find(TrieKey((uint8_t)TrieLevelActor::kSkin, sk));
			if (it2 == it1->second.children.end()) continue;
		for (auto mat : materials) {
			auto it3 = it2->second.children.find(TrieKey((uint8_t)TrieLevelActor::kMaterial, mat));
			if (it3 == it2->second.children.end()) continue;
		for (auto sx : sexes) {
			auto it4 = it3->second.children.find(TrieKey((uint8_t)TrieLevelActor::kSex, (uint32_t)sx));
			if (it4 == it3->second.children.end()) continue;
		for (auto sk2 : skeletons) {
			auto it4b = it4->second.children.find(TrieKey((uint8_t)TrieLevelActor::kSkeleton, sk2));
			if (it4b == it4->second.children.end()) continue;
		for (auto fid : fids) {
			auto it5 = it4b->second.children.find(TrieKey((uint8_t)TrieLevelActor::kFormID, fid));
			if (it5 == it4b->second.children.end()) continue;
			step_buckets(it5->second);
		}}}}}}}

		return result;
	}

	static std::optional<CoreStructure::BipedMapping::BipedBonesMap> GetBipedModifier(RE::Actor* target)
	{
		using namespace CoreStructure;
		if (!target) return std::nullopt;

		// Exclusions
		std::unordered_set<BipedMapping*> excluded;
		for (auto* m : LookupBipedTrie(bipedRegistry.victimExclude, target)) excluded.insert(m);

		// Inclusions
		auto candidates = LookupBipedTrie(bipedRegistry.victimInclude, target);

		std::ranges::sort(candidates, [](const BipedMapping* a, const BipedMapping* b) {
			return a->priority > b->priority;
		});

		TRACE("[GetBipedModifier] - {} candidate(s)", candidates.size());
		for (const auto* m : candidates) {
			TRACE("  BipedMapping id={} priority={} bipedBones={}",
				m->id, m->priority, m->bipedBones.size());
		}

		for (auto* m : candidates) {
			if (excluded.contains(m)) continue;
			TRACE("  -> Selected BipedMapping id={}", m->id);
			return m->bipedBones;
		}

		TRACE("  -> No BipedMapping matched");
		return std::nullopt;
	}

	static CoreStructure::BipedMapping::BipedBoneData GetExtraNodesMap(RE::Actor* target)
	{
		using namespace CoreStructure;

		BipedMapping::BipedBoneData extraData{};
		if (!target) return extraData;

		auto root = target->Get3D(false);
		if (!root) return extraData;

		static auto hasParentMatching = [](RE::NiAVObject* node, RE::NiAVObject* root) -> bool {
			while (node) {
				const char* name = node->name.c_str();
				if (_strnicmp(name, "weapon", 6) == 0 || _strnicmp(name, "shield", 6) == 0 || _strnicmp(name, "object", 6) == 0) return true;
				
				if (node == root) break;
				node = node->parent;
			}
			return false;
		};

		std::unordered_set<std::string> addedNodes;

		NiUtils::TraverseObjectsForward(root, [&](RE::NiAVObject* a_object) {
			if (!a_object) return true;

			auto* rigidBody = NiUtils::GetRigidBody(a_object);
			if (!rigidBody || !rigidBody->world) return true;

			auto* shape = rigidBody->GetShape();
			if (!shape || !hasParentMatching(a_object, root)) return true;

			std::string bipedNode = a_object->name.c_str();
			if (addedNodes.insert(bipedNode).second) {
				const auto materialID = shape->userData ? shape->userData->materialID : RE::MATERIAL_ID::kNone;

				int slot = -1;
				switch (materialID) {
					case RE::MATERIAL_ID::kShieldLight: slot = -4; break;
					case RE::MATERIAL_ID::kShieldHeavy: slot = -3; break;
					case RE::MATERIAL_ID::kBowsStaves: slot = -2; break;
					default: break;
				}

				extraData.nodes.push_back(bipedNode);
				extraData.slots.push_back(slot);
			}

			return true;
		});

		return extraData;
	}

	static CoreStructure::DeferredHitStruct::BipedEntry GetHitBipedSlots( RE::Actor* target, const RE::NiPoint3 hitPosition,
		const CoreStructure::BipedMapping::BipedBonesMap& bipedBones)
	{
		using namespace CoreStructure;

		if (bipedBones.empty()) return {};

		auto* actor3D = target->Get3D1(false);
		if (!actor3D) return {};

		static constexpr auto kExtraKey = "Extra";
		const BipedMapping::BipedBoneData extraData = GetExtraNodesMap(target);

		const float worldScaleInverse = RE::bhkWorld::GetWorldScaleInverse();
		const float worldScale = RE::bhkWorld::GetWorldScale();

		struct FlatTransform {
			float col0[4], col1[4], col2[4], trans[4];
		};
		auto extractTransform = [](const RE::hkTransform& t) -> FlatTransform {
			FlatTransform f;
			_mm_store_ps(f.col0, t.rotation.col0.quad);
			_mm_store_ps(f.col1, t.rotation.col1.quad);
			_mm_store_ps(f.col2, t.rotation.col2.quad);
			_mm_store_ps(f.trans, t.translation.quad);
			return f;
		};

		auto localToWorld = [](const FlatTransform& f, float lx, float ly, float lz) -> RE::NiPoint3 {
			return {
				f.col0[0] * lx + f.col1[0] * ly + f.col2[0] * lz + f.trans[0],
				f.col0[1] * lx + f.col1[1] * ly + f.col2[1] * lz + f.trans[1],
				f.col0[2] * lx + f.col1[1] * ly + f.col2[2] * lz + f.trans[2]
			};
		};

		auto worldToLocal = [](const FlatTransform& f, float wx, float wy, float wz) -> RE::NiPoint3 {
			const float rx = wx - f.trans[0];
			const float ry = wy - f.trans[1];
			const float rz = wz - f.trans[2];
			return {
				f.col0[0] * rx + f.col0[1] * ry + f.col0[2] * rz,
				f.col1[0] * rx + f.col1[1] * ry + f.col1[2] * rz,
				f.col2[0] * rx + f.col2[1] * ry + f.col2[2] * rz
			};
		};

		std::unordered_map<std::string, float> distanceCache;

		auto tryGetNodeDistance = [&](const std::string& bipedNode, float& outDistance) -> bool {
			if (auto cached = distanceCache.find(bipedNode); cached != distanceCache.end()) {
				outDistance = cached->second;
				return true;
			}

			auto node = actor3D->GetObjectByName(bipedNode);
			if (!node) return false;

			bool success = false;

			RE::bhkNiCollisionObject* collisionObject = node->collisionObject ? node->collisionObject->AsBhkNiCollisionObject() : nullptr;
			RE::bhkRigidBody* bhkRigid = collisionObject && collisionObject->body ? collisionObject->body->AsBhkRigidBody() : nullptr;
			RE::hkpRigidBody* hkpRigid = bhkRigid ? skyrim_cast<RE::hkpRigidBody*>(bhkRigid->referencedObject.get()) : nullptr;

			if (bhkRigid && hkpRigid) {
				const auto collisionLayer = static_cast<std::uint32_t>(hkpRigid->collidable.broadPhaseHandle.collisionFilterInfo.GetCollisionLayer());
				const RE::hkpShape* shape = collisionLayer != NonCollidableLayer ? hkpRigid->collidable.GetShape() : nullptr;

				if (shape && shape->type == RE::hkpShapeType::kCapsule) {
					auto* capsuleShape = static_cast<const RE::hkpCapsuleShape*>(shape);
					const auto ft = extractTransform(hkpRigid->motion.motionState.transform);

					float a[4], b[4];
					_mm_store_ps(a, capsuleShape->vertexA.quad);
					_mm_store_ps(b, capsuleShape->vertexB.quad);

					const RE::NiPoint3 vertexA = localToWorld(ft, a[0], a[1], a[2]) * worldScaleInverse;
					const RE::NiPoint3 vertexB = localToWorld(ft, b[0], b[1], b[2]) * worldScaleInverse;
					const float radius = capsuleShape->radius * worldScaleInverse;

					outDistance = std::min(hitPosition.GetDistance(vertexA), hitPosition.GetDistance(vertexB)) - radius;
					success = true;

				} else if (shape && shape->type == RE::hkpShapeType::kBox) {
					auto* boxShape = static_cast<const RE::hkpBoxShape*>(shape);
					const auto ft = extractTransform(hkpRigid->motion.motionState.transform);

					const RE::NiPoint3 hitHavok = hitPosition * worldScale;
					const RE::NiPoint3 local = worldToLocal(ft, hitHavok.x, hitHavok.y, hitHavok.z);

					float half[4];
					_mm_store_ps(half, boxShape->halfExtents.quad);

					const float cx = std::clamp(local.x, -half[0], half[0]);
					const float cy = std::clamp(local.y, -half[1], half[1]);
					const float cz = std::clamp(local.z, -half[2], half[2]);

					const RE::NiPoint3 closestWorld = localToWorld(ft, cx, cy, cz) * worldScaleInverse;

					outDistance = hitPosition.GetDistance(closestWorld);
					success = true;

				} else if (shape && shape->type == RE::hkpShapeType::kSphere) {
					auto* sphereShape = static_cast<const RE::hkpSphereShape*>(shape);

					RE::hkVector4 massCenter;
					bhkRigid->GetCenterOfMassWorld(massCenter);
					float massTrans[4];
					_mm_store_ps(massTrans, massCenter.quad);
					const RE::NiPoint3 centerPos = RE::NiPoint3(massTrans[0], massTrans[1], massTrans[2]) * worldScaleInverse;

					const float radius = sphereShape->radius * worldScaleInverse;
					outDistance = hitPosition.GetDistance(centerPos) - radius;
					success = true;
				}
			}

			if (!success) {
				outDistance = hitPosition.GetDistance(node->world.translate);
				success = true;
			}

			distanceCache[bipedNode] = outDistance;
			return success;
		};

		std::string closestNodeName;
		float closestNodeDistance = std::numeric_limits<float>::max();

		auto scanNodes = [&](const std::vector<std::string>& nodes) {
			for (const auto& bipedNode : nodes) {
				float dist = 0.0f;
				if (tryGetNodeDistance(bipedNode, dist) && dist < closestNodeDistance) {
					closestNodeDistance = dist;
					closestNodeName = bipedNode;
				}
			}
		};

		for (const auto& [groupName, boneData] : bipedBones) scanNodes(boneData.nodes);
		scanNodes(extraData.nodes);

		if (closestNodeName.empty()) return {};

		std::unordered_set<int> uniqueSlots;
		std::vector<std::string> relatedKeys;

		for (const auto& [groupName, boneData] : bipedBones) {
			if (std::find(boneData.nodes.begin(), boneData.nodes.end(), closestNodeName) != boneData.nodes.end()) {
				relatedKeys.push_back(groupName);
				uniqueSlots.insert(boneData.slots.begin(), boneData.slots.end());
			}
		}
		if (std::find(extraData.nodes.begin(), extraData.nodes.end(), closestNodeName) != extraData.nodes.end()) {
			relatedKeys.emplace_back(kExtraKey);
			uniqueSlots.insert(extraData.slots.begin(), extraData.slots.end());
		}

		auto priorityOf = [&](const std::string& key) -> int {
			if (key == kExtraKey) return extraData.priority;
			return bipedBones.at(key).priority;
		};

		std::stable_sort(relatedKeys.begin(), relatedKeys.end(), [&](const std::string& a, const std::string& b) {
			return priorityOf(a) > priorityOf(b);
		});

		std::string bipedLimb;
		for (const auto& key : relatedKeys) {
			const bool isLimb = (key == kExtraKey) ? extraData.isLimbEntry : bipedBones.at(key).isLimbEntry;
			if (isLimb) {
				bipedLimb = key;
				break;
			}
		}

		CoreStructure::DeferredHitStruct::BipedEntry result;
		result.bipedNode = closestNodeName;
		result.bipedLimb = bipedLimb;
		result.bipedKeys = std::move(relatedKeys);
		result.bipedSlots = std::vector<int>(uniqueSlots.begin(), uniqueSlots.end());

		if (debugVerboseMode > 1) {
			TRACE("Closest node: '{}', Keys: [{}], Slots: [{}], HitPosition: [{}]",
				result.bipedNode, fmt::join(result.bipedKeys, ", "), fmt::join(result.bipedSlots, ", "), hitPosition);
		}

		return result;
	}
};
