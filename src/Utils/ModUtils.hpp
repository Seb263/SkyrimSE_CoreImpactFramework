#pragma once

#include "DataHandler.hpp"
#include "Utils/MiscUtils.hpp"

class ModUtils
{
public:

	static float ResolveVariant(const std::variant<float, RE::TESGlobal*>& variant, const float defaultValue)
	{
		if (std::holds_alternative<float>(variant)) {
			return std::get<float>(variant);
		}

		if (auto* global = std::get_if<RE::TESGlobal*>(&variant)) {
			if (*global) return (*global)->value;
		}

		return defaultValue;
	}

	static float OffsetRealDamage(float damage, RE::Actor* aggressor, RE::Actor* victim)
	{
		if (!aggressor && !victim) return damage;

		auto difficulty = static_cast<std::uint32_t>(RE::PlayerCharacter::GetSingleton()->GetPlayerRuntimeData().difficulty);
		difficulty = std::clamp(difficulty, 0u, 5u);

		using namespace ModData;
		if (aggressor && (aggressor->IsPlayerRef() || aggressor->IsPlayerTeammate())) {
			damage *= fDiffMultHPByPC[difficulty];
		} else if (victim && (victim->IsPlayerRef() || victim->IsPlayerTeammate())) {
			damage *= fDiffMultHPToPC[difficulty];
		}

		return damage;
	}

	static RE::TESObjectARMO* GetBashShield(RE::Actor* aggressor)
	{
		if (!aggressor) return nullptr;
	
		if (auto* aiProcess = aggressor->GetActorRuntimeData().currentProcess;
			aiProcess && aiProcess->high && aiProcess->high->attackData &&
			aiProcess->high->attackData->data.flags.all(RE::AttackData::AttackFlag::kBashAttack)) {
		
			auto bashItem = aiProcess->GetEquippedLeftHand();
			if (bashItem && bashItem->As<RE::TESObjectARMO>()) {
				return bashItem->As<RE::TESObjectARMO>();
			}
		}

		return nullptr;
	}

	static RE::TESObjectWEAP* GetBashWeapon(RE::Actor* aggressor)
	{
		if (!aggressor) return nullptr;

		if (auto* aiProcess = aggressor->GetActorRuntimeData().currentProcess;
			aiProcess && aiProcess->high && aiProcess->high->attackData && aiProcess->high->attackData->data.flags.all(RE::AttackData::AttackFlag::kBashAttack)) {
			
			if (aiProcess->GetEquippedLeftHand() && aiProcess->GetEquippedLeftHand()->IsArmor()) return nullptr;
			RE::TESForm* object = aiProcess->high->attackData->IsLeftAttack() ? aiProcess->GetEquippedLeftHand() : aiProcess->GetEquippedRightHand();
			if (object && object->IsWeapon() && object->As<RE::TESObjectWEAP>()) return object->As<RE::TESObjectWEAP>();
		}

		return nullptr;
	}

	static RE::NiPoint3 GetActorFireNodePosition(RE::TESObjectREFR* shooterRef)
	{
		RE::NiPoint3 fireNodePos{};
		if (!shooterRef) fireNodePos;

		if (RE::NiNode* fireNode = shooterRef->GetFireNode()) {
			fireNodePos = fireNode->world.translate;
		}
		if (fireNodePos.Length() == 0.0f) {
			fireNodePos = shooterRef->GetPosition();
			if (shooterRef->formType == RE::FormType::ActorCharacter) {
				if (RE::Actor* shooterActor = shooterRef->As<RE::Actor>()) {
					fireNodePos.z += shooterActor->GetHeight() * 0.80f;
				}
			}
		}

		return fireNodePos;
	}

	static RE::Actor* GetCollisionSourceReference(RE::Projectile* projectile, RE::hkpAllCdPointCollector* allCdPointCollector)
	{
		if (!allCdPointCollector || allCdPointCollector->hits.empty()) return nullptr;

		auto getActorFromCollidable = [](const RE::hkpCollidable* collidable) -> RE::Actor* {
			if (!collidable) return nullptr;
			auto* refr = RE::TESHavokUtilities::FindCollidableRef(*collidable);
			return (refr && refr->formType == RE::FormType::ActorCharacter) ? refr->As<RE::Actor>() : nullptr;
		};

		for (auto& hit : allCdPointCollector->hits) {
			if (auto* actor = getActorFromCollidable(hit.rootCollidableB)) {
				return actor;
			}
		}

		return nullptr;
	}

	static bool RayCastSpellActor(RE::Actor* origin, RE::Actor* target,
		const RE::NiPoint3& position, const RE::NiPoint3& rotation, RE::NiPoint3& positionOut, RE::hkVector4& normalOut)
	{
		if (!origin || !target) return false;

		auto havokWorldScale = RE::bhkWorld::GetWorldScale();
		if (!havokWorldScale) return false;

		RE::NiPoint3 rayEnd = position + MiscUtils::AnglesToDir(rotation, 20000.0f);

		RE::bhkPickData pickData;
		pickData.rayInput.from = position * havokWorldScale;
		pickData.rayInput.to = rayEnd * havokWorldScale;
	
		RE::CFilter collisionFilterInfo{};
		origin->GetCollisionFilterInfo(collisionFilterInfo);
		const auto collisionGroup = collisionFilterInfo.GetSystemGroup();

		pickData.rayInput.filterInfo.SetSystemGroup(collisionGroup);
		pickData.rayInput.filterInfo.SetCollisionLayer(RE::COL_LAYER::kBiped);

		auto* originCell = origin->GetParentCell();
		if (!originCell) return false;

		auto* bhkWorld = originCell->GetbhkWorld();
		if (!bhkWorld) return false;

		bhkWorld->PickObject(pickData);
		if (pickData.rayOutput.HasHit()) {
			if (auto collidable = pickData.rayOutput.rootCollidable) {
				if (RE::TESObjectREFR* ref = RE::TESHavokUtilities::FindCollidableRef(*collidable)) {
					if (ref->As<RE::Actor>() == target) {
						positionOut = position + (rayEnd - position) * pickData.rayOutput.hitFraction;
						normalOut = pickData.rayOutput.normal;
						return true;
					}
				}
			}
		}
		return false;
	}

	static RE::NiPoint3 GetHitboxImpactPosition(RE::Actor* actor, RE::Projectile* projectile)
	{
		if (!actor || !projectile || !actor->Is3DLoaded()) return {};

		const auto actorPos = actor->data.location;
		const auto projPos = projectile->data.location;
		const auto dir = MiscUtils::AnglesToDir(projectile->data.angle, 1.0f);

		const float angle = actor->GetAngleZ();
		const float cosA = std::cos(-angle);
		const float sinA= std::sin(-angle);

		RE::NiPoint3 localOrigin = projPos - actorPos;

		RE::NiPoint3 rotatedOrigin;
		rotatedOrigin.x = localOrigin.x * cosA - localOrigin.y * sinA;
		rotatedOrigin.y = localOrigin.x * sinA + localOrigin.y * cosA;
		rotatedOrigin.z = localOrigin.z;

		RE::NiPoint3 rotatedDir;
		rotatedDir.x = dir.x * cosA - dir.y * sinA;
		rotatedDir.y = dir.x * sinA + dir.y * cosA;
		rotatedDir.z = dir.z;

		RE::NiPoint3 boundMin;
		RE::NiPoint3 boundMax;
		if (!TryGetActorBounds(actor, boundMin, boundMax)) return {};

		float tMin = -FLT_MAX;
		float tMax =  FLT_MAX;

		auto processAxis = [&](float orig, float d, float bMin, float bMax) {
			if (std::abs(d) < 1e-6f) return;
			float t1 = (bMin - orig) / d;
			float t2 = (bMax - orig) / d;
			if (t1 > t2) std::swap(t1, t2);
			tMin = std::max(tMin, t1);
			tMax = std::min(tMax, t2);
		};

		processAxis(rotatedOrigin.x, rotatedDir.x, boundMin.x, boundMax.x);
		processAxis(rotatedOrigin.y, rotatedDir.y, boundMin.y, boundMax.y);
		processAxis(rotatedOrigin.z, rotatedDir.z, boundMin.z, boundMax.z);

		const float tHit = std::max(tMin, 0.0f);

		RE::NiPoint3 localHit;
		localHit.x = rotatedOrigin.x + rotatedDir.x * tHit;
		localHit.y = rotatedOrigin.y + rotatedDir.y * tHit;
		localHit.z = rotatedOrigin.z + rotatedDir.z * tHit;

		const float cosB = std::cos(angle);
		const float sinB = std::sin(angle);

		RE::NiPoint3 worldHit;
		worldHit.x = localHit.x * cosB - localHit.y * sinB;
		worldHit.y = localHit.x * sinB + localHit.y * cosB;
		worldHit.z = localHit.z;

		return actorPos + worldHit;
	}

	static bool TryCheckConditions(RE::EffectSetting* baseEffect, RE::TESObjectREFR* target, RE::TESObjectREFR* caster)
	{
		if (!target || !baseEffect || !baseEffect->conditions || !baseEffect->conditions.head) return true;

		__try {
			return baseEffect->conditions.IsTrue(target, caster);
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			logger::warn("Corrupted conditions on effect {:08X}", baseEffect->formID);
			return false;
		}
	}

	static bool TryGetActorBounds(RE::Actor* actor, RE::NiPoint3& boundMin, RE::NiPoint3& boundMax)
	{
		if (!actor) return false;

		__try {
			boundMin = actor->GetBoundMin();
			boundMax = actor->GetBoundMax();
			return true;
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			logger::warn("Failed to retrieve bounds for actor {:08X}", actor->formID);
			return false;
		}
	}
};
