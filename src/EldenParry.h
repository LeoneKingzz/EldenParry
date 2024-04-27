#pragma once
#include <memory>
#include "lib/PrecisionAPI.h"
#include "lib/ValhallaCombatAPI.h"
#include <mutex>
#include <shared_mutex>

#include <unordered_set>

class EldenParry
{
public:
	bool AttackerBeatsParry(RE::Actor *attacker, RE::Actor *target, const RE::TESObjectWEAP *attackerWeapon,
							const RE::TESObjectWEAP *targetWeapon, RE::AIProcess *const attackerAI,
							RE::AIProcess *const targetAI);
	double GetScore(RE::Actor *actor, const RE::TESObjectWEAP *weapon,
					RE::AIProcess *const actorAI, const Milf::Scores& scoreMilf);
	const RE::TESObjectWEAP *const GetAttackWeapon(RE::AIProcess *const aiProcess);

	static double riposteScore;

	static EldenParry* GetSingleton() {
		static EldenParry singleton;
		return std::addressof(singleton);
	}

	void init();

	/// <summary>
	/// Try to process a parry by the parrier.
	/// </summary>
	/// <param name="a_attacker"></param>
	/// <param name="a_parrier"></param>
	/// <returns>True if the parry is successful.</returns>
	bool processMeleeParry(RE::Actor* a_attacker, RE::Actor* a_parrier);

	bool processProjectileParry(RE::Actor* a_blocker, RE::Projectile* a_projectile, RE::hkpCollidable* a_projectile_collidable);

	void processGuardBash(RE::Actor* a_basher, RE::Actor* a_blocker);

	PRECISION_API::IVPrecision1* _precision_API;
	VAL_API::IVVAL1* _ValhallaCombat_API;

	void applyParryCost(RE::Actor* a_actor);
	void cacheParryCost(RE::Actor* a_actor, float a_cost);
	void negateParryCost(RE::Actor* a_actor);

	void playGuardBashEffects(RE::Actor* a_actor);

	void startTimingParry(RE::Actor* a_actor);
	void finishTimingParry(RE::Actor* a_actor);

	void send_melee_parry_event(RE::Actor* a_attacker);
	void send_ranged_parry_event();

	void update();

private:
	void playParryEffects(RE::Actor* a_parrier);

	bool inParryState(RE::Actor* a_parrier);
	bool canParry(RE::Actor* a_parrier, RE::TESObjectREFR* a_obj, RE::Actor* a_attacker);
	bool canParryProj(RE::Actor* a_parrier, RE::TESObjectREFR* a_obj);
	bool inBlockAngle(RE::Actor* a_blocker, RE::TESObjectREFR* a_obj);
	bool ParryContext(RE::Actor* a_agressor, RE::Actor* a_victim);
	static PRECISION_API::PreHitCallbackReturn precisionPrehitCallbackFunc(const PRECISION_API::PrecisionHitData& a_precisionHitData);

	std::unordered_map<RE::Actor*, float> _parryCostQueue;
	std::unordered_set<RE::Actor*> _parrySuccessActors;
	std::unordered_map<RE::Actor*, float> _parryTimer;



	RE::BGSSoundDescriptorForm* _parrySound_shd;
	RE::BGSSoundDescriptorForm* _parrySound_wpn;
	float _GMST_fCombatHitConeAngle;
	float _parryAngle;

	std::shared_mutex mtx_parryCostQueue;
	std::shared_mutex mtx_parrySuccessActors;
	std::shared_mutex mtx_parryTimer;

	bool _bUpdate;
};

class Milf
{
public:
	[[nodiscard]] static Milf *GetSingleton();

	void Load();

	struct Core
	{
		void Load(CSimpleIniA &a_ini);

		bool useScoreSystem{true};
	} core;

	struct Scores
	{
		void Load(CSimpleIniA &a_ini);

		double scoreDiffThreshold{20.0};

		double weaponSkillWeight{1.0};

		double oneHandDaggerScore{0.0};
		double oneHandSwordScore{20.0};
		double oneHandAxeScore{25.0};
		double oneHandMaceScore{25.0};
		double oneHandKatanaScore{30.0};
		double oneHandRapierScore{15.0};
		double oneHandClawsScore{10.0};
		double oneHandWhipScore{-100.0};
		double twoHandSwordScore{40.0};
		double twoHandAxeScore{50.0};
		double twoHandWarhammerScore{50.0};
		double twoHandPikeScore{30.0};
		double twoHandHalberdScore{45.0};
		double twoHandQuarterstaffScore{50.0};

		double altmerScore{-15.0};
		double argonianScore{0.0};
		double bosmerScore{-10.0};
		double bretonScore{-10.0};
		double dunmerScore{-5.0};
		double imperialScore{0.0};
		double khajiitScore{5.0};
		double nordScore{10.0};
		double orcScore{20.0};
		double redguardScore{10.0};

		double femaleScore{-10.0};

		double powerAttackScore{25.0};

		double playerScore{0.0};
	} scores;

private:
	Milf() = default;
	Milf(const Milf &) = delete;
	Milf(Milf &&) = delete;
	~Milf() = default;

	Milf &operator=(const Milf &) = delete;
	Milf &operator=(Milf &&) = delete;

	struct detail
	{

		// Thanks to: https://github.com/powerof3/CLibUtil
		template <class T>
		static T &get_value(CSimpleIniA &a_ini, T &a_value, const char *a_section, const char *a_key, const char *a_comment,
							const char *a_delimiter = R"(|)")
		{
			if constexpr (std::is_same_v<T, bool>)
			{
				a_value = a_ini.GetBoolValue(a_section, a_key, a_value);
				a_ini.SetBoolValue(a_section, a_key, a_value, a_comment);
			}
			else if constexpr (std::is_floating_point_v<T>)
			{
				a_value = static_cast<float>(a_ini.GetDoubleValue(a_section, a_key, a_value));
				a_ini.SetDoubleValue(a_section, a_key, a_value, a_comment);
			}
			else if constexpr (std::is_enum_v<T>)
			{
				a_value = string::template to_num<T>(
					a_ini.GetValue(a_section, a_key, std::to_string(std::to_underlying(a_value)).c_str()));
				a_ini.SetValue(a_section, a_key, std::to_string(std::to_underlying(a_value)).c_str(), a_comment);
			}
			else if constexpr (std::is_arithmetic_v<T>)
			{
				a_value = string::template to_num<T>(a_ini.GetValue(a_section, a_key, std::to_string(a_value).c_str()));
				a_ini.SetValue(a_section, a_key, std::to_string(a_value).c_str(), a_comment);
			}
			else if constexpr (std::is_same_v<T, std::vector<std::string>>)
			{
				a_value = string::split(a_ini.GetValue(a_section, a_key, string::join(a_value, a_delimiter).c_str()),
										a_delimiter);
				a_ini.SetValue(a_section, a_key, string::join(a_value, a_delimiter).c_str(), a_comment);
			}
			else
			{
				a_value = a_ini.GetValue(a_section, a_key, a_value.c_str());
				a_ini.SetValue(a_section, a_key, a_value.c_str(), a_comment);
			}
			return a_value;
		}
	};

};