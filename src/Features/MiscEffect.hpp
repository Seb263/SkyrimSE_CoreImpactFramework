#pragma once

#include "DataHandler.hpp"

#include "Core/Structure.h"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"
#include "Utils/NativeUtils.hpp"

class MiscEffect
{
public:

	template <typename T>
	static void Initialize(CoreStructure::DeferredHitStruct& deferredHit, const std::optional<T> modifier)
	{
		auto* victim = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.victimHandle);
		auto* attacker = MiscUtils::ResolveHandle<RE::Actor>(deferredHit.attackerHandle);
		if (!victim || !modifier) return;
		
		if (!modifier->spells.empty() && attacker) {
			for (auto* spell : modifier->spells) {
				if (!spell) continue;
				CastSpell(victim, attacker, spell);
			}
		}

		if (!modifier->placedObjects.empty()) {
			for (auto* placedObject : modifier->placedObjects) {
				if (!placedObject) continue;
				PlaceObject(placedObject, victim, deferredHit.extraHitData.hitPosition, deferredHit.extraHitData.hitDirection);
			}
		}

		if (modifier->disarm.value_or(false)) {
			EjectWeapon(victim, deferredHit);
		}
		
		if (modifier->eject.value_or(false)) {
			EjectArmor(victim, deferredHit);
		}

		const float staggerMagnitude = ModUtils::ResolveVariant(modifier->stagger, -1.0f);
		if (staggerMagnitude >= 0.0f) {
			const auto from = attacker ? ModUtils::GetActorFireNodePosition(attacker) : deferredHit.extraHitData.hitPosition;
			StaggerActor(victim, from, staggerMagnitude);
		}
	}

private:

	static void CastSpell(RE::Actor* targetRef, RE::Actor* casterRef, RE::SpellItem* spell)
	{
		SKSE::GetTaskInterface()->AddTask([targetHandle = targetRef->GetHandle(), attackerHandle = casterRef->GetHandle(), spell]() {
			auto* target = MiscUtils::ResolveHandle<RE::Actor>(targetHandle);
			auto* attacker = MiscUtils::ResolveHandle<RE::Actor>(attackerHandle);
			if (!target || !attacker || !target->parentCell || !target->parentCell->IsAttached()) return;

			auto* caster = attacker->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
			if (!caster) return;

			caster->CastSpellImmediate(spell, true, target, 0.0f, true, 0.0f, nullptr);
		});
	}

	static void PlaceObject(RE::TESForm* formObject, RE::Actor* targetRef, const RE::NiPoint3& position, const RE::NiPoint3& direction)
	{
		SKSE::GetTaskInterface()->AddTask([formObject, targetHandle = targetRef->GetHandle(), position, direction]() {
			auto* target = MiscUtils::ResolveHandle<RE::Actor>(targetHandle);
			if (!target) return;

			if (!formObject) return;

			NativeUtils::PlaceAtMe(target, formObject, position, direction);
		});
	}

	static void StaggerActor(RE::Actor* targetRef, const RE::NiPoint3& position, const float& magnitude)
	{
		SKSE::GetTaskInterface()->AddTask([targetHandle = targetRef->GetHandle(), position, magnitude]() {
			auto* target = MiscUtils::ResolveHandle<RE::Actor>(targetHandle);
			if (!target) return;

			RE::NiPoint3 repulseDir = target->GetPosition() - position;
			float length = repulseDir.Length();
			if (length <= 0.0f) return;
			repulseDir /= length;

			RE::NiPoint3 sourcePos = target->GetPosition() - repulseDir;
			auto headingAngle = target->GetHeadingAngle(sourcePos, true);
			const float staggerDir = headingAngle < 90.0f ? 0.0f : 0.5f;

			if (magnitude > 1.0f) {
				if (auto& currentProcess = target->GetActorRuntimeData().currentProcess) {
					NativeUtils::PushActorAway(currentProcess, target, const_cast<RE::NiPoint3&>(position), magnitude);
					return;
				}
			}

			target->SetGraphVariableFloat("StaggerMagnitude", magnitude);
			target->SetGraphVariableFloat("StaggerDirection", staggerDir);
			target->NotifyAnimationGraph("StaggerStart");
		});
	}

	static void EjectWeapon(RE::Actor* targetRef, CoreStructure::DeferredHitStruct& deferredHit)
	{
		if (!ModData::DDO_API_Interface || !targetRef) return;
		if (!targetRef->AsActorState() || !targetRef->AsActorState()->IsWeaponDrawn()) return;

		using BlockedFilter = CIF_API::Interface::Filter::BlockedFilter;
		
		RE::TESBoundObject* weaponForm = nullptr;

		auto getEquippedBoundObject = [&](bool leftHand) -> RE::TESBoundObject* {
			auto* equipped = targetRef->GetEquippedObject(leftHand);
			return equipped ? equipped->As<RE::TESBoundObject>() : nullptr;
		};

		bool isLeftHand = false;
		switch (deferredHit.ctx.blocked) {
			case BlockedFilter::kShieldLight:
			case BlockedFilter::kShieldHeavy:
				weaponForm = getEquippedBoundObject(true);
				isLeftHand = true;
				break;
			default:
				if (targetRef->IsBlocking()) {
					if (auto* leftItem = getEquippedBoundObject(true)) {
						weaponForm = leftItem;
						isLeftHand = true;
					} else {
						weaponForm = getEquippedBoundObject(false);
					}
				} else {
					weaponForm = getEquippedBoundObject(false);
				}
				break;
		}

		const char* nodeName = isLeftHand ? "SHIELD" : "WEAPON";
		auto* node = targetRef->GetNodeByName(nodeName);
		if (!node) node = targetRef->GetNodeByName(deferredHit.bipedEntry.bipedNode);

		if (!weaponForm || !node) return;

		RE::NiPoint3 position = node->world.translate;
		RE::NiPoint3 angle; node->world.rotate.ToEulerAnglesXYZ(angle);

		const float linearVelocityMult = MiscUtils::GetRandomNumber(-SettingsIni::fDisarmMinLinearVelocity, SettingsIni::fDisarmMaxLinearVelocity);
		RE::hkVector4 linearVelocity = deferredHit.extraHitData.hitDirection;
		
		float len = linearVelocity.Length3();
		if (len > 1e-6f) linearVelocity = (linearVelocity / len) * linearVelocityMult;

		RE::hkVector4 angularVelocity{
			MiscUtils::GetRandomNumber(-SettingsIni::fDisarmMaxAngularVelocity, SettingsIni::fDisarmMaxAngularVelocity),
			MiscUtils::GetRandomNumber(-SettingsIni::fDisarmMaxAngularVelocity, SettingsIni::fDisarmMaxAngularVelocity),
			MiscUtils::GetRandomNumber(-SettingsIni::fDisarmMaxAngularVelocity, SettingsIni::fDisarmMaxAngularVelocity),
			0.0f
		};

		auto* extraDataList = ModData::DDO_API_Interface->GetEquippedExtraDataList(targetRef, weaponForm, isLeftHand);
		ModData::DDO_API_Interface->DropItemFromActor(targetRef, weaponForm, position, angle, linearVelocity, angularVelocity, 1, extraDataList);
	}

	static void EjectArmor(RE::Actor* targetRef, CoreStructure::DeferredHitStruct& deferredHit)
	{
		if (!ModData::DDO_API_Interface || !targetRef) return;

		auto* armorForm = RE::TESForm::LookupByID<RE::TESBoundObject>(deferredHit.ctx.armorFormID);
		if (!armorForm || deferredHit.bipedEntry.bipedNode.empty()) return;

		auto* node = targetRef->GetNodeByName(deferredHit.bipedEntry.bipedNode);
		if (!node) return;

		RE::NiPoint3 position = node->world.translate;
		RE::NiPoint3 angle; node->world.rotate.ToEulerAnglesXYZ(angle);

		const float linearVelocityMult = MiscUtils::GetRandomNumber(-SettingsIni::fDisarmMinLinearVelocity, SettingsIni::fDisarmMaxLinearVelocity);
		RE::hkVector4 linearVelocity = deferredHit.extraHitData.hitDirection;
		
		float len = linearVelocity.Length3();
		if (len > 1e-6f) linearVelocity = (linearVelocity / len) * linearVelocityMult;

		RE::hkVector4 angularVelocity{
			MiscUtils::GetRandomNumber(-SettingsIni::fDisarmMaxAngularVelocity, SettingsIni::fDisarmMaxAngularVelocity),
			MiscUtils::GetRandomNumber(-SettingsIni::fDisarmMaxAngularVelocity, SettingsIni::fDisarmMaxAngularVelocity),
			MiscUtils::GetRandomNumber(-SettingsIni::fDisarmMaxAngularVelocity, SettingsIni::fDisarmMaxAngularVelocity),
			0.0f
		};

		auto* extraDataList = ModData::DDO_API_Interface->GetEquippedExtraDataList(targetRef, armorForm);
		ModData::DDO_API_Interface->DropItemFromActor(targetRef, armorForm, position, angle, linearVelocity, angularVelocity, 1, extraDataList);
	}
};
