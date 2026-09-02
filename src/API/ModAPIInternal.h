#pragma once

#include "API/ModAPI.h"

namespace CIF_API::Internal
{
	void InvokePreHitCallbacks(RE::HitData& hitData);

	void InvokePostHitCallbacks(const Interface::RuntimeHitContext context);

	void InvokePostDeferredHitCallbacks(const Interface::RuntimeHitContext context);
}
