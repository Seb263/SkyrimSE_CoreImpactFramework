#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

constexpr std::uint32_t NonCollidableLayer = 49;

class NiUtils
{
public:

	static bool TraverseObjectsForward(RE::NiAVObject* a_object, std::function<bool(RE::NiAVObject*, int)> a_func, int depth = 0)
	{
		if (!a_object) return true;
		if (!a_func(a_object, depth)) return false;

		if (auto node = a_object->AsNode()) {
			for (auto& child : node->GetChildren()) {
				if (!TraverseObjectsForward(child.get(), a_func, depth + 1)) {
					return false;
				}
			}
		}

		return true;
	}

	static bool TraverseObjectsForward(RE::NiAVObject* a_object, std::function<bool(RE::NiAVObject*)> a_func)
	{
		return TraverseObjectsForward(a_object, [&](RE::NiAVObject* obj, int) { return a_func(obj); });
	}

	static RE::hkpRigidBody* GetRigidBody(RE::NiAVObject* a_object)
	{
		if (!a_object) return nullptr;

		auto* collisionObject = a_object->GetCollisionObject();
		if (!collisionObject) return nullptr;

		auto bhkRigidBody = RE::NiPointer<RE::bhkRigidBody>(collisionObject->GetRigidBody());
		if (!bhkRigidBody || !bhkRigidBody->referencedObject) return nullptr;

		auto* hkpRigidBody = static_cast<RE::hkpRigidBody*>(bhkRigidBody->referencedObject.get());
		return hkpRigidBody;
	}
};
