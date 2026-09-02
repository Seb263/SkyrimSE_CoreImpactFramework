#pragma once

#include "DataHandler.hpp"

#include "Core/Structure.h"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"

#include "API/DF-API.h"

class Dismember
{
public:

	template <typename T>
	static void Initialize(CoreStructure::DeferredHitStruct& deferredHit, const std::optional<T>& modifier)
	{
		if (!ModData::DF_API_Legacy_Interface || !modifier) return;

		RE::FormID wornArmorFormID = deferredHit.ctx.armorFormID ? deferredHit.ctx.armorFormID : 0x0;
		RE::FormID weaponFormID = deferredHit.extraHitData.weaponBase ? deferredHit.extraHitData.weaponBase->formID :
			deferredHit.extraHitData.magicItemBase ? deferredHit.extraHitData.magicItemBase->formID : 0x0;

		if (modifier->dismemberAuto.value_or(false)) {
			DismemberKeys(deferredHit.bipedEntry.bipedKeys, deferredHit.victimHandle, deferredHit.attackerHandle, wornArmorFormID, weaponFormID,
				deferredHit.extraHitData.hitPosition, deferredHit.extraHitData.hitDirection, deferredHit.extraHitData.hitPower);
		}
	}

private:

	static void DismemberKeys(const std::vector<std::string>& bipedKeys, RE::ActorHandle victimHandle, RE::ActorHandle attackerHandle,
		RE::FormID wornArmorFormID, RE::FormID weaponFormID, const RE::NiPoint3& position, const RE::NiPoint3& direction, const float& power)
	{
		if (bipedKeys.empty()) return;

		SKSE::GetTaskInterface()->AddTask([=]() {
			DF_API_Legacy::DismembermentParams f_params{};
			f_params.bipedKeys = bipedKeys;
			f_params.victim = victimHandle;
			f_params.attacker = attackerHandle;
			f_params.hitPosition = position;
			f_params.hitDirection = direction;
			f_params.impulsionMult = power;
			f_params.wornArmorFormID = wornArmorFormID;
			f_params.weaponSourceFormID = weaponFormID;

			if (!ModData::DF_API_Legacy_Interface) return;
			ModData::DF_API_Legacy_Interface->Dismember(f_params);
		});
	}
};
