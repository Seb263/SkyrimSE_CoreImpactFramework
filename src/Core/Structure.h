#pragma once

#include "API/ModAPI.h"

namespace CoreStructure
{
	enum class TrieLevelActor : uint8_t
	{
		kRace = 0,
		kKeyword = 1,
		kSkin = 2,
		kSex = 3,
		kMaterial = 4,
		kWorn = 5,
		kWornKeyword = 6,
		kPerk = 7,
		kSpell = 8,
		kMagicEffect = 9,
		kSkeleton = 10,
		kFormID = 11
	};

	enum class TrieLevelHitContext : uint8_t
	{
		kWeapon = 0,
		kWeaponKeyword = 1,
		kMagicItem = 2,
		kProjectile = 3,
		kMagicEffect = 4,
		kArmor = 5,
		kArmorKeyword = 6,
		kWeaponType = 7,
		kArmorClass = 8,
		kBlocked = 9,
		kState = 10,
		kAttack = 11,
		kSource = 12,
		kCritical = 13,
		kSneak = 14,
		kGlobales = 15,
		kBipedSlot = 16,
		kBipedLimb = 17,
		kBipedNode = 18,
		kBipedKey = 19,
		kPercentage = 20,
		kMaxHealth = 21,
		kMinDamage = 22
	};

	struct BipedMapping
	{
		int id;
		int priority;

		struct BipedBoneData
		{
			int priority;
			bool isLimbEntry;
			std::vector<int> slots;
			std::vector<std::string> nodes;
		};

		using BipedBonesMap = std::unordered_map<std::string, BipedBoneData>;
		BipedBonesMap bipedBones;
	};

	struct BipedTrieNode
	{
		struct Bucket
		{
			std::vector<RE::BGSPerk*> conditions;
			std::vector<std::pair<uint8_t, RE::FormID>> allFilters;
			std::vector<BipedMapping*> mappings;
		};
		std::unordered_map<uint64_t, BipedTrieNode> children;
		std::vector<Bucket> buckets;
	};

	struct BipedRegistry
	{
		BipedTrieNode victimInclude;
		BipedTrieNode victimExclude;
	};

	struct Filter : CIF_API::Interface::Filter
	{
		enum class ComparisonType
		{
			kEqual,              // "=="
			kNotEqual,           // "!="
			kLessThan,           // "<"
			kGreaterThan,        // ">"
			kLessThanOrEqual,    // "<="
			kGreaterThanOrEqual, // ">="
			kInvalid             // Invalid
		};

		struct GlobalFilter
		{
			RE::TESGlobal* global;
			ComparisonType comparison;
			float value;

			bool operator==(const GlobalFilter& other) const
			{
				return global == other.global && comparison == other.comparison && value == other.value;
			}
		};
	};

	struct ImpactMapping
	{
		int id;
		int priority;
		bool override;
		bool overrideMerge;

		struct Modifier
		{
			RE::BGSImpactData* impactData = nullptr;
			RE::SpellItem* bloodSpray = nullptr;
			RE::BGSSoundDescriptorForm* soundOverride = nullptr;
			RE::BGSImpactData* decalOverride = nullptr;
			
			std::optional<bool> preserveOriginalSound = std::nullopt;
			std::optional<bool> preserveOriginalDecal = std::nullopt;
			std::optional<bool> removeDecal = std::nullopt;
			std::optional<bool> removeBloodSplatter = std::nullopt;
			std::optional<bool> removeSound = std::nullopt;
			std::optional<bool> impactBounce = std::nullopt;
			
			std::vector<std::variant<std::string, RE::BGSImpactData*>> extraImpactData;
			std::vector<RE::BGSSoundDescriptorForm*> extraSound;
			std::vector<RE::TESForm*> placedObjects;
			std::vector<RE::SpellItem*> spells;
			std::variant<float, RE::TESGlobal*> damageMult = 1.0f;
			std::optional<bool> dismemberAuto = std::nullopt;
			std::optional<bool> disarm = std::nullopt;
			std::optional<bool> eject = std::nullopt;

			std::variant<bool, RE::TESGlobal*> damageLimb = true;
			std::variant<float, RE::TESGlobal*> stagger = -1.0f;
			std::variant<float, RE::TESGlobal*> damageLimbMult = 1.0f;

			std::vector<std::string> bloodpools;

			RE::BGSImpactData* originalImpactData = nullptr;
			bool mergeWithPrevious = false;
		} modifier;
	};

	struct HitMapping
	{
		int id;
		int priority;
		std::string className;
		bool override;
		bool overrideMerge;

		struct Modifier
		{
			RE::SpellItem* bloodSpray = nullptr;
			std::vector<std::variant<std::string, RE::BGSImpactData*>> extraImpactData;
			std::vector<RE::BGSSoundDescriptorForm*> extraSound;
			std::vector<RE::TESForm*> placedObjects;
			std::vector<RE::SpellItem*> spells;
			std::variant<float, RE::TESGlobal*> damageMult = 1.0f;
			std::optional<bool> dismemberAuto = std::nullopt;
			std::optional<bool> disarm = std::nullopt;
			std::optional<bool> eject = std::nullopt;

			std::variant<bool, RE::TESGlobal*> damageLimb = true;
			std::variant<float, RE::TESGlobal*> stagger = -1.0f;
			std::variant<float, RE::TESGlobal*> damageLimbMult = 1.0f;

			std::vector<std::string> bloodpools;

			bool shouldBreak = false;
			bool mergeWithPrevious = false;
			bool deferred=false;
		} modifier;
	};

    template <typename MappingType>
    struct ActorTrieNode
    {
        struct Bucket
        {
			int idMapping = -1;
			std::vector<std::pair<uint8_t, RE::FormID>> allFilters;
			std::vector<std::pair<uint8_t, RE::FormID>> noneFilters;
			std::vector<MappingType*> mappings;
        };
        std::unordered_map<uint64_t, ActorTrieNode> children;
        std::vector<Bucket> buckets;
    };

    template <typename MappingType>
    struct HitCtxTrieNode
    {
        struct Bucket
        {
			int idMapping = -1;
			std::vector<Filter::GlobalFilter> globalesAny;
			std::vector<Filter::GlobalFilter> globalesAll;
			std::vector<Filter::GlobalFilter> globalesNone;
			std::vector<RE::BGSPerk*> conditionsAny;
			std::vector<RE::BGSPerk*> conditionsAll;
			std::vector<RE::BGSPerk*> conditionsNone;
			std::variant<float, RE::TESGlobal*> percentage = -1.0f;
			std::variant<float, RE::TESGlobal*> percentageMult = 1.0f;
			std::variant<float, RE::TESGlobal*> maxHealth = -1.0f;
			std::variant<float, RE::TESGlobal*> minDamage = -1.0f;
			std::variant<float, RE::TESGlobal*> maxLimbHealth = -1.0f;
			std::vector<std::pair<uint8_t, RE::FormID>> allFilters;
			std::vector<std::pair<uint8_t, RE::FormID>> noneFilters;
			std::vector<MappingType*> mappings;
        };
        std::unordered_map<uint64_t, HitCtxTrieNode> children;
        std::vector<Bucket> buckets;
    };

	struct DeferredHitStruct
	{
		using BipedEntry = CIF_API::Interface::BipedEntry;

		struct EffectEntry
		{
			float power;
			RE::ActiveEffect* activeEffect;
		};

		struct ExtraHitData
		{
			RE::NiPoint3 hitPosition;
			RE::NiPoint3 hitDirection;

			RE::ObjectRefHandle sourceHandle;
			REX::EnumSet<RE::HitData::Flag, std::uint32_t> hitDataFlags;

			Filter::WeaponType weaponType = Filter::WeaponType::kOther;
			RE::TESObjectWEAP* weaponBase = nullptr;
			RE::MagicItem* magicItemBase = nullptr;
			RE::EffectSetting* effectBase = nullptr;
			RE::BGSProjectile* projectileBase = nullptr;

			float spellMagnitude = 1.0f;
			float hitPower = 1.0f;
		} extraHitData;

		struct Modifiers
		{
			std::optional<ImpactMapping::Modifier> impactModifier;
			std::unordered_map<std::string, std::optional<HitMapping::Modifier>> hitModifiers;
		} modifiers;

		struct RuntimeHitContext : CIF_API::Interface::RuntimeHitContext
		{
			std::vector<uint32_t> bipedSlotHashes = {};
			std::vector<uint32_t> bipedKeyHashes = {};
			uint32_t bipedLimbHash = 0x0;
			uint32_t bipedNodeHash = 0x0;
			uint32_t victimSkeletonHash = 0x0;
			uint32_t attackerSkeletonHash = 0x0;
		} ctx;

		RE::ActorHandle attackerHandle;
		RE::ActorHandle victimHandle;

		BipedMapping::BipedBonesMap bipedBones;
		BipedEntry bipedEntry;

		bool isMagic = false;
		bool runtimeReady = false;

		uint32_t randomSeed = 0x0;
		std::optional<float> damageMult = std::nullopt;
		std::optional<float> calculatedDamage = std::nullopt;
		std::optional<float> damageLimbMult = std::nullopt;

		std::unordered_map<RE::Effect*, EffectEntry> activeEffectsMap;

		std::chrono::steady_clock::time_point timestamp;
	};

    template <typename MappingType>
	struct Registry
	{
		ActorTrieNode<MappingType> victimInclude;
		ActorTrieNode<MappingType> victimExclude;
		ActorTrieNode<MappingType> attackerInclude;
		ActorTrieNode<MappingType> attackerExclude;
		HitCtxTrieNode<MappingType> hitContextInclude;
		HitCtxTrieNode<MappingType> hitContextExclude;
	};

	inline uint64_t TrieKey(uint8_t level, uint32_t value) {
        return ((uint64_t)level << 32) | value;
	}

	inline ImpactMapping::Modifier weaponDefault, weaponWood, shieldHeavy, shieldLight;

	inline std::vector<BipedMapping> bipedMappings;
	inline std::vector<ImpactMapping> impactMappings;
	inline std::vector<HitMapping> hitMappings;

	inline BipedRegistry bipedRegistry;
	inline Registry<ImpactMapping> impactRegistry;
	inline Registry<HitMapping> hitRegistry;
};
