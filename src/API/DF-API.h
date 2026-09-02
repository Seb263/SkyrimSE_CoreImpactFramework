#pragma once

/*******************************************************************
* DISMEMBERING FRAMEWORK - API
* Do not forget to include this source file to your project!
*******************************************************************/

/* How to create a hook to the API and use it:
SKSE::GetMessagingInterface()->RegisterListener([](MessagingInterface::Message* message) 
{
	switch (message->type) 
	{
		case MessagingInterface::kPostLoadGame:
		case MessagingInterface::kNewGame:
		{
			if (!DismemberingFrameworkAPI::LoadAPI()) {
				util::report_and_fail("Failed to bound to the Dismembering Framework API");
			}
			DismemberingFrameworkAPI::g_API->GetVersion();
		}
		break;
	}
});
*/

// Define the API type key
#define DF_API_TYPE_KEY static_cast<uint32_t>(std::byteswap('DF'))

// Define the API version in a structured format
#define DF_API_VERSION_MAJOR 2
#define DF_API_VERSION_MINOR 0
#define DF_API_VERSION_PATCH 0

// Combine the version numbers into a single value
#define DF_API_VERSION ((DF_API_VERSION_MAJOR << 16) | (DF_API_VERSION_MINOR << 8) | DF_API_VERSION_PATCH)

namespace DF_API_Legacy
{
	struct DismembermentParams
	{
		std::vector<std::string> bipedKeys;
		RE::ActorHandle victim;
		RE::ActorHandle attacker;
		RE::NiPoint3 hitPosition;
		RE::NiPoint3 hitDirection;
		RE::FormID wornArmorFormID;
		RE::FormID weaponSourceFormID;
		float impulsionMult = 1.0f;
	};
	
	class Interface
	{
	public:
		// API functions
		virtual size_t GetVersion() const;

		virtual void Dismember(const DismembermentParams params) const;

		virtual bool IsDismembered(RE::Actor* actor) const;
		
		virtual bool IsDismemberedNode(RE::Actor* actor, const RE::BSFixedString& node) const;

		virtual void PostDecapitate(RE::Actor* actor, RE::Actor* head) const;
		
		virtual void RefreshActorDismemberedState(RE::Actor* actor) const;
	};

	// Global API pointer
	inline extern Interface* g_API = nullptr;

	// Call this function only after the kDataLoaded event
	inline bool LoadAPI()
	{
		if (g_API != nullptr) return true;
		SKSE::GetMessagingInterface()->Dispatch(DF_API_TYPE_KEY, (void*)&g_API, sizeof(void*), NULL);
		if (g_API) { // API successfully received!
			// Check if the API version matches
			return (g_API->GetVersion() == DF_API_VERSION);
		}
		return false;
	}
}
