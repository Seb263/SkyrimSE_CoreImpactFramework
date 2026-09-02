#pragma once

#include "DataHandler.hpp"

#include "Core/Structure.h"

#include "Utils/MiscUtils.hpp"

#include "API/DBF-API.h"

class Bloodpool
{
public:

	template <typename T>
	static void Initialize(CoreStructure::DeferredHitStruct& deferredHit, const std::optional<T>& modifier)
	{
		if (!ModData::DBF_API_Interface || !modifier) return;

		if (!modifier->bloodpools.empty()) {
			GenerateBloodpools(modifier->bloodpools, deferredHit.victimHandle, deferredHit.ctx.bipedEntry.bipedNode);
		}
	}

private:

	static void GenerateBloodpools(const std::vector<std::string>& profiles, RE::ActorHandle victimHandle, const std::string& nodeName)
	{
		if (profiles.empty()) return;

		SKSE::GetTaskInterface()->AddTask([=]() {
			
			auto* victim = MiscUtils::ResolveHandle<RE::Actor>(victimHandle);
			if (!victim) return;

			auto* root = victim->Get3D1(false);
			auto* node = (root && !nodeName.empty()) ? root->GetObjectByName(nodeName) : nullptr;

			for (auto& profile : profiles) {
				DBF_API::Interface::Parameters f_params;
				f_params.profileID = profile;
				f_params.waitForStableOrigin = true;
				f_params.originRef = victim;
				f_params.originNodePos = node;
				f_params.override.spread = 360.0f;

				if (!ModData::DBF_API_Interface) return;
				ModData::DBF_API_Interface->SpawnBloodpool(f_params);
			}
		});
	}
};
