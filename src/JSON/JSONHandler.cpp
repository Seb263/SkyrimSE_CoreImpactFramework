#include "JSONHandler.h"

namespace JSONHandler
{
	void Main::LoadMappings()
	{
		const auto start = std::chrono::high_resolution_clock::now();
		logger::info("Loading JSON files ({})...", (SettingsIni::bGeneral_AsynchronousStartup ? "asynchronous" : "synchronous"));

		std::vector<std::string> files = MiscUtils::GetAllFiles<true>("Data\\SKSE\\CoreImpactFramework"sv, ".json"sv);

		json mergedJson{};

		for (const auto& file : files) {
			try {
				std::ifstream fileStream(file);
				json fileData = json::parse(fileStream);
				logger::info("Parsing JSON Data In \"{}\"", file);
				JsonUtils::ProcessKeysWithDelimiter(fileData);
				JsonUtils::MergeJsonRecursive(mergedJson, fileData);
			} catch (const std::exception& e) {
				REPORT_AND_FAIL("Error while processing template JSON file '{}': {}", file, e.what());
			}
		}

		LegacyConverter::Convert(mergedJson);
		if (debugVerboseMode > 2) TRACE("Content of compiled JSON: {}", mergedJson.dump(4));

		ProcessJson(mergedJson);
		MiscUtils::ClearGetFormLookupCache();

		const auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed = end - start;
		logger::info("Loading JSON files ({}): DONE after {} seconds", (SettingsIni::bGeneral_AsynchronousStartup ? "asynchronous" : "synchronous"), elapsed.count());

		if (debugVerboseMode > 2) {
			Debugging::TraceRegistry(impactRegistry, "ImpactRegistry");
			Debugging::TraceRegistry(hitRegistry, "HitRegistry");
			Debugging::TraceBipedRegistry();
		}
	}

	void Main::ProcessJson(const json& jsonData) {
		Biped::ProcessBipedMapping(jsonData);
		Impact::ProcessImpactMapping(jsonData);
		Impact::ProcessHitMapping(jsonData);
	}

	template <typename Type>
	Type* Main::ResolveForm(const std::string& str, const bool useAssociatedForm)
	{
		if (str.empty()) return nullptr;

		return (useAssociatedForm && str.find(':') == std::string::npos) ?
			MiscUtils::GetFormFromEditorID<Type>(str) : MiscUtils::GetFormFromAssoc<Type>(str);
	}

	template <typename Type, typename Output>
	void Main::ParseMappingFilterFormJson(const json& filters, const std::string& key, std::vector<Output>& out, const bool useAssociatedForm)
	{
		if (!filters.contains(key) || filters[key].is_null()) return;
		const auto& value = filters[key];
		auto entries = value.is_array() ? value : json::array({ value });

		for (const auto& entry : entries) {
			if (!entry.is_string()) continue;
			Type* formatedForm = ResolveForm<Type>(entry.get<std::string>(), useAssociatedForm);
			if (!formatedForm) continue;
			if constexpr (std::is_same_v<Output, Type*>) out.push_back(formatedForm);
			else if constexpr (std::is_same_v<Output, RE::FormID>) {
				if (auto* form = formatedForm->As<RE::TESForm>()) {
					out.push_back(form->GetFormID());
				}
			}
		}
	}

	Filter::ComparisonType Main::ToComparisonType(const std::string& comparison)
	{
		using ComparisonType = Filter::ComparisonType;
		
		if (comparison == "==") return ComparisonType::kEqual;
		if (comparison == "!=" || comparison == "<>") return ComparisonType::kNotEqual;
		if (comparison == "<") return ComparisonType::kLessThan;
		if (comparison == ">") return ComparisonType::kGreaterThan;
		if (comparison == "<=") return ComparisonType::kLessThanOrEqual;
		if (comparison == ">=") return ComparisonType::kGreaterThanOrEqual;
		
		logger::error("Invalid comparison type \"{}\"", comparison);
		return ComparisonType::kInvalid;
	}
};
