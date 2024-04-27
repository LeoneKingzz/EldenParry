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
					RE::AIProcess *const actorAI, const Settings::Scores &scoreSettings);
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