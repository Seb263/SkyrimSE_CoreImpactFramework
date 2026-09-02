#pragma once

class NativeUtils
{
	public:

	static RE::TESObjectREFR* PlaceAtMe(RE::TESObjectREFR* ref, RE::TESForm* baseForm, RE::NiPoint3 position, RE::NiPoint3 angle = {}, bool forcePersist = false)
	{
		const auto boundObject = baseForm->As<RE::TESBoundObject>();
		if (!boundObject || !ref) return nullptr;

		const auto handle = RE::TESDataHandler::GetSingleton()->CreateReferenceAtLocation(boundObject, position, angle, ref->GetParentCell(), ref->GetWorldspace(), nullptr, nullptr, RE::ObjectRefHandle(), forcePersist, true);
		const auto handlePtr = handle.get();
		return (handlePtr && handlePtr.get() ? handlePtr.get() : nullptr);
	};

	static void PushActorAway(RE::AIProcess* a_causer, RE::Actor* a_target, RE::NiPoint3& a_origin, float a_magnitude)
	{
		using func_t = decltype(&PushActorAway);
		REL::Relocation<func_t> func{ RELOCATION_ID(38858, 39895) };
		return func(a_causer, a_target, a_origin, a_magnitude);
	};

	static void PlaySound(RE::BGSSoundDescriptorForm* descriptor, float volume = 1.0f,
		std::variant<std::monostate, RE::NiAVObject*, RE::TESObjectREFR*> source = std::monostate{}, std::optional<RE::NiPoint3> position = std::nullopt)
	{
		if (!descriptor) return;

		using SoundA = int(*)(void*, RE::BSSoundHandle*, int, int);
		using SoundB = void(*)(RE::BSSoundHandle*, RE::NiAVObject*);
		using SoundC = char(*)(RE::BSSoundHandle*);
		using SoundPos = char(*)(RE::BSSoundHandle*, float, float, float);
		static REL::Relocation<SoundA> soundA{ RELOCATION_ID(66401, 67663) };
		static REL::Relocation<SoundB> soundB{ RELOCATION_ID(66375, 67636) };
		static REL::Relocation<SoundC> soundC{ RELOCATION_ID(66355, 67616) };
		static REL::Relocation<SoundPos> soundPos{ RELOCATION_ID(66370, 67631) };

		RE::BSSoundHandle handle;
		handle.soundID = static_cast<uint32_t>(-1);
		handle.assumeSuccess = false;
		*(uint32_t*)&handle.state = 0;

		soundA(RE::BSAudioManager::GetSingleton(), &handle, descriptor->GetFormID(), 16);

		RE::NiPoint3 pos{};
		RE::NiAVObject* node = nullptr;
		std::visit([&](auto&& arg) {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, RE::NiAVObject*>) {
				if (arg) {
					pos = arg->world.translate;
					node = arg;
				}
			} else if constexpr (std::is_same_v<T, RE::TESObjectREFR*>) {
				if (arg) {
					pos = arg->data.location;
					node = arg->Get3D();
				}
			}
		}, source);

		if (position) pos = *position;

		if (soundPos(&handle, pos.x, pos.y, pos.z)) {
			if (node) soundB(&handle, node);
			handle.SetVolume(volume);
			soundC(&handle);
		}
	}
};
