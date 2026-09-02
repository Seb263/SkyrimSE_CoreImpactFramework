#pragma once

#include "DataHandler.hpp"

class MiscUtils
{
	public:

	template <typename Container>
	static Container SplitString(const std::string& str, char delimiter, bool removeSpaces = true, bool toLower = false)
	{
		Container tokens;
		std::stringstream ss(str);
		std::string token;

		while (std::getline(ss, token, delimiter)) {
			if (removeSpaces) {
				token.erase(0, token.find_first_not_of(' '));
				token.erase(token.find_last_not_of(' ') + 1);
			}

			if (toLower) {
				std::transform(token.begin(), token.end(), token.begin(), ::tolower);
			}

			if (!token.empty()) {
				if constexpr (std::is_same_v<Container, std::unordered_set<std::string>>) {
					tokens.insert(token);
				} else {
					tokens.push_back(token);
				}
			}
		}
		return tokens;
	}

	template <typename T>
	static std::vector<T*> GetFormsFromString(const std::string& a_string)
	{
		std::vector<T*> formVector;
		std::stringstream ss(a_string);
		std::string item;

		while (std::getline(ss, item, "&")) {
			if (T* form = GetFormFromAssoc<T>(item)) {
				formVector.push_back(form);
			} else {
				logger::warn("GetFormsFromString: Failed to find form for \"{}\"", item);
			}
		}

		return formVector;
	}

	inline static std::unordered_map<std::string, RE::TESForm*> g_getFormCache;
	template <typename T>
	static T* GetFormFromAssoc(const std::string& a_string, const bool cacheResult = true)
	{
		if (a_string.empty()) return nullptr;

		if (const auto it = g_getFormCache.find(a_string); it != g_getFormCache.end()) {
			if constexpr (std::is_same_v<T, RE::TESForm>) return it->second;
			else if (auto* typed = it->second->As<T>()) return typed;
			else {
				logger::warn("GetFormFromAssoc: \"{}\" is incompatible with type \"{}\" (cache).", a_string, typeid(T).name());
				return nullptr;
			}
		}

		const std::size_t sep = a_string.find(':');
		if (sep == std::string::npos) {
			logger::warn("GetFormFromAssoc: Invalid format for \"{}\"", a_string);
			return nullptr;
		}

		const std::string hexPart = a_string.substr(sep + 1);
		std::size_t charsRead = 0;
		std::uint32_t formID = 0;

		try {
			formID = std::stoul(hexPart, &charsRead, 16);
		} catch (const std::invalid_argument&) {
			logger::warn("GetFormFromAssoc: Invalid hexadecimal value in \"{}\"", a_string);
			return nullptr;
		} catch (const std::out_of_range&) {
			logger::warn("GetFormFromAssoc: FormID out of range in \"{}\"", a_string);
			return nullptr;
		}

		if (charsRead != hexPart.size()) {
			logger::warn("GetFormFromAssoc: Malformed hexadecimal value in \"{}\" (invalid character at position {})", a_string, charsRead);
			return nullptr;
		}

		auto* base = RE::TESDataHandler::GetSingleton()->LookupForm(static_cast<RE::FormID>(formID), a_string.substr(0, sep));
		if (!base) {
			logger::warn("GetFormFromAssoc: \"{}\" could not be found.", a_string);
			return nullptr;
		}

		if (cacheResult) g_getFormCache[a_string] = base;

		if constexpr (std::is_same_v<T, RE::TESForm>) return base;
		else if (auto* typed = base->As<T>()) return typed;
		else {
			logger::warn("GetFormFromAssoc: \"{}\" is incompatible with type \"{}\".", a_string, typeid(T).name());
			return nullptr;
		}
	}

	inline static std::unordered_map<RE::FormType, std::unordered_map<std::string, RE::TESForm*>> g_getFormEditorCache;
	template <typename T>
	static T* GetFormFromEditorID(const std::string& a_string)
	{
		if (a_string.empty()) return nullptr;

		auto& cache = g_getFormEditorCache[T::FORMTYPE];

		if (cache.empty()) {
			auto* dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler) return nullptr;

			auto forms = dataHandler->GetFormArray<T>();
			for (auto* form : forms) {
				if (!form) continue;

				const char* editorID = form->GetFormEditorID();
				if (editorID && *editorID != '\0') cache.emplace(editorID, form);
			}
		}

		auto it = cache.find(a_string);
		if (it == cache.end()) {
			logger::warn("GetFormFromEditorID: \"{}\" not found.", a_string);
			return nullptr;
		}

		if constexpr (std::is_same_v<T, RE::TESForm>) return it->second;
		if (auto* typed = it->second->As<T>()) return typed;

		logger::warn("GetFormFromEditorID: \"{}\" is incompatible with requested type.", a_string);
		return nullptr;
	}

	static void ClearGetFormLookupCache()
	{
		g_getFormCache.clear();
		g_getFormEditorCache.clear();
	}

	template <bool RECURSIVE = false>
	static std::vector<std::string> GetAllFiles(std::string_view a_path = {}, std::string_view a_ext = {},
		std::string_view a_prefix = {}, std::string_view a_suffix = {}) noexcept {
		using dir_iterator = std::conditional_t<RECURSIVE, std::filesystem::recursive_directory_iterator, std::filesystem::directory_iterator>;

		std::vector<std::string> files;

		auto file_iterator = [&](const std::filesystem::directory_entry& a_file) {
			if (a_file.exists() && !a_file.path().empty()) {
				if (!a_ext.empty() && a_file.path().extension() != a_ext) {
					return;
				}

				const auto path = a_file.path().string();

				if ((!a_prefix.empty() && path.find(a_prefix) != std::string::npos) ||
					(!a_suffix.empty() && path.rfind(a_suffix) != std::string::npos) ||
					(a_prefix.empty() && a_suffix.empty())) {
					files.push_back(path);
				}
			}
		};

		std::string dir(MAX_PATH + 1, ' ');
		auto res = GetModuleFileNameA(nullptr, dir.data(), MAX_PATH + 1);
		if (res == 0) REPORT_AND_FAIL("Unable to acquire valid path using default null path argument!\nExpected: Current directory\nResolved: NULL");

		auto eol = dir.find_last_of("\\/");
		dir = dir.substr(0, eol);

		std::filesystem::path path = a_path.empty() ? std::filesystem::path{dir} : std::filesystem::path{a_path};

		if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
			path = dir / path;
		}

		if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
			for (const auto& entry : dir_iterator(path)) {
				file_iterator(entry);
			}
			std::ranges::sort(files);
		} else {
			logger::warn("Provided path is invalid or not a directory: {}", path.string());
		}

		return files;
	}

	template <typename T = RE::TESObjectREFR, typename HandleT>
	static T* ResolveHandle(const HandleT& handle)
	{
		auto ptr = handle ? handle.get() : nullptr;
		if (!ptr) return nullptr;

		return ptr->As<T>();
	}

	static bool IsFormIDValid(const RE::FormID formID)
	{
		return (formID > 0x0 && formID < 0xFFFFFFFF);
	}

	template <typename T = RE::TESObjectREFR>
	static T* GetValidReference(RE::FormID formID, const bool extraChecks = false)
	{
		if (!MiscUtils::IsFormIDValid(formID)) return nullptr;
		return GetValidReference<T>(RE::TESForm::LookupByID<RE::TESObjectREFR>(formID), extraChecks);
	}

	template <typename T = RE::TESObjectREFR>
	static T* GetValidReference(RE::TESObjectREFR* ref, const bool extraChecks = false)
	{
		using namespace ModData;

		if (!ref || !ref->As<T>() || !MiscUtils::IsFormIDValid(ref->formID) || ref->IsDeleted())
			return nullptr;

		if (extraChecks) {
			if (ref->IsDisabled() || ref->IsMarkedForDeletion()) return nullptr;
		}

		if constexpr (std::is_same_v<T, RE::Actor>) {
			auto* refActor = ref->As<RE::Actor>();
			if (!refActor || !ref->Is(RE::FormType::ActorCharacter)) return nullptr;

			if (extraChecks && (refActor->GetActorRuntimeData().criticalStage != RE::ACTOR_CRITICAL_STAGE::kNone)) return nullptr;
		}

		return ref->As<T>();
	}

	static void ReSyncLayerBitfields(RE::bhkCollisionFilter* a_filter, int32_t a_layer)
	{
		uint64_t bitfield = a_filter->layerBitfields[static_cast<uint8_t>(a_layer)];
		for (int i = 0; i < 64; i++) {
			if ((bitfield >> i) & 1) {
				a_filter->layerBitfields[i] |= (static_cast<uint64_t>(1) << static_cast<uint8_t>(a_layer));
			} else {
				a_filter->layerBitfields[i] &= ~(static_cast<uint64_t>(1) << static_cast<uint8_t>(a_layer));
			}
		}
	}

	template<typename T>
	static T GetGameSetting(const std::string& settingName, const T& defaultValue = T{})
	{
		auto* gsc = RE::GameSettingCollection::GetSingleton();
		if (!gsc) return defaultValue;

		auto* setting = gsc->GetSetting(settingName.c_str());
		if (!setting) {
			logger::warn("GetGameSetting: setting \"{}\" not found", settingName);
			return defaultValue;
		}

		using SettingType = RE::Setting::Type;
		switch (setting->GetType()) {
			case SettingType::kBool: if constexpr (std::is_same_v<T, bool>) return setting->data.b; break;
			case SettingType::kFloat: if constexpr (std::is_same_v<T, float>) return setting->data.f; break;
			case SettingType::kInteger: if constexpr (std::is_same_v<T, int32_t>) return setting->data.i; break;
			case SettingType::kUnsignedInteger: if constexpr (std::is_same_v<T, uint32_t>) return setting->data.u; break;
			case SettingType::kString:
				if constexpr (std::is_same_v<T, std::string>) {
					return (setting->data.s && !IsBadReadPtr(setting->data.s, 1)) ? std::string(setting->data.s) : defaultValue;
				}
				break;
			default: break;
		}

		return defaultValue;
	}

	static bool SetGameSetting(const std::string& settingName, const std::variant<bool, float, int32_t, uint32_t, std::string>& newValue)
	{
		auto* gsc = RE::GameSettingCollection::GetSingleton();
		if (!gsc || settingName.empty()) return false;

		auto* setting = gsc->GetSetting(settingName.c_str());
		if (!setting) {
			logger::warn("SetGameSetting: setting \"{}\" not found", settingName);
			return false;
		}

		using SettingType = RE::Setting::Type;
		auto settingType = setting->GetType();

		switch (settingType) {
			case SettingType::kBool: if (auto value = std::get_if<bool>(&newValue)) { setting->data.b = *value; return true; } break;
			case SettingType::kFloat: if (auto value = std::get_if<float>(&newValue)) { setting->data.f = *value; return true; } break;
			case SettingType::kInteger: if (auto value = std::get_if<int32_t>(&newValue)) { setting->data.i = *value; return true; } break;
			case SettingType::kUnsignedInteger: if (auto value = std::get_if<uint32_t>(&newValue)) { setting->data.u = *value; return true; } break;
			case SettingType::kString:
				if (auto value = std::get_if<std::string>(&newValue)) {
					free(setting->data.s);
					setting->data.s = _strdup(value->c_str());
					return true;
				} break;
			default: return false;
		}

		return false;
	}

	static RE::NiPoint3 ApplySpreadToDirection(const RE::NiPoint3& direction, float spread)
	{
		if (spread <= 0.0f) return direction;
		if (spread > 1.0f) spread = 1.0f;

		const float maxAngle = spread * (RE::NI_PI / 2.0f);
		const float angle = MiscUtils::GetRandomNumber(0.0f, maxAngle);
		const float theta = MiscUtils::GetRandomNumber(0.0f, RE::NI_TWO_PI);

		RE::NiPoint3 up = std::abs(direction.z) < 0.999f ?
			RE::NiPoint3{ 0.0f, 0.0f, 1.0f } :
			RE::NiPoint3{ 1.0f, 0.0f, 0.0f };

		RE::NiPoint3 right = direction.Cross(up);
		right.Unitize();

		RE::NiPoint3 forward = right.Cross(direction);
		forward.Unitize();

		RE::NiPoint3 spreadDir =
			direction * std::cos(angle) +
			right * std::sin(angle) * std::cos(theta) +
			forward * std::sin(angle) * std::sin(theta);

		spreadDir.Unitize();
		return spreadDir;
	}

	static RE::NiPoint3 AnglesToDir(const RE::NiPoint3& angles, const float distance = 1.0f)
	{
		RE::NiPoint3 ans;

		float sinx = sinf(angles.x);
		float cosx = cosf(angles.x);
		float sinz = sinf(angles.z);
		float cosz = cosf(angles.z);

		ans.x = cosx * sinz;
		ans.y = cosx * cosz;
		ans.z = -sinx;

		return ans * distance;
	}

	static RE::Projectile::ProjectileRot DirToAngles(const RE::NiPoint3& dir, bool invert = false)
	{
		RE::Projectile::ProjectileRot rot{};
		RE::NiPoint3 norm = dir;
		norm.Unitize();

		if (!invert) {
			rot.x = -asinf(norm.z);
			rot.z = atan2f(norm.x, norm.y);
		} else {
			rot.x = asinf(norm.z);
			rot.z = atan2f(-norm.x, -norm.y);
		}

		return rot;
	}

	static RE::NiPoint3 HkVector4ToNiPoint3(const RE::hkVector4& vec)
	{
		float x = _mm_cvtss_f32(vec.quad);
		float y = _mm_cvtss_f32(_mm_shuffle_ps(vec.quad, vec.quad, _MM_SHUFFLE(1, 1, 1, 1)));
		float z = _mm_cvtss_f32(_mm_shuffle_ps(vec.quad, vec.quad, _MM_SHUFFLE(2, 2, 2, 2)));

		return RE::NiPoint3(x, y, z);
	}

	static float GetRandomNumber(float min = 0.0f, float max = 1.0f)
	{
		static std::mt19937 generator(std::random_device{}());
		std::uniform_real_distribution<float> distribution(min, max);
		return distribution(generator);
	}
};
