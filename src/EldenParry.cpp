#include "EldenParry.h"
#include "Settings.h"
#include "Utils.hpp"
using uniqueLocker = std::unique_lock<std::shared_mutex>;
using sharedLocker = std::shared_lock<std::shared_mutex>;

void EldenParry::init() {
	logger::info("Obtaining precision API...");
	_precision_API = reinterpret_cast<PRECISION_API::IVPrecision1*>(PRECISION_API::RequestPluginAPI());
	if (_precision_API) {
		logger::info("Precision API successfully obtained.");
		Settings::facts::isPrecisionAPIObtained = true;
		if (_precision_API->AddPreHitCallback(SKSE::GetPluginHandle(), precisionPrehitCallbackFunc) ==
			PRECISION_API::APIResult::OK) {
			logger::info("Successfully registered precision API prehit callback.");
		}
	} else {
		logger::info("Precision API not found.");
	}
	logger::info("Obtaining Valhalla Combat API...");
	_ValhallaCombat_API = reinterpret_cast<VAL_API::IVVAL1*>(VAL_API::RequestPluginAPI());
	if (_ValhallaCombat_API) {
		logger::info("Valhalla Combat API successfully obtained.");
		Settings::facts::isValhallaCombatAPIObtained = true;
	}
	else {
		logger::info("Valhalla Combat API not found.");
	}
	//read parry sound
	auto data = RE::TESDataHandler::GetSingleton();
	_parrySound_shd = data->LookupForm<RE::BGSSoundDescriptorForm>(0xD62, "EldenParry.esp");
	_parrySound_wpn = data->LookupForm<RE::BGSSoundDescriptorForm>(0xD63, "EldenParry.esp");
	if (!_parrySound_shd || !_parrySound_wpn) {
		RE::DebugMessageBox("Parry sound not found in EldenParry.esp");
		logger::error("Parry sound not found in EldenParry.esp");
	}

	//read fcombatHitConeAngle
	_GMST_fCombatHitConeAngle = RE::GameSettingCollection::GetSingleton()->GetSetting("fCombatHitConeAngle")->GetFloat();
	_parryAngle = _GMST_fCombatHitConeAngle;
}

void EldenParry::update() {
	if (!_bUpdate) {
		return;
	}
	uniqueLocker lock(mtx_parryTimer);
	auto it = _parryTimer.begin();
	if (it == _parryTimer.end()) {
		_bUpdate = false;
		return;
	}
	while (it != _parryTimer.end()) {
		if (!it->first) {
			it = _parryTimer.erase(it);
			continue;
		}
		if (it->second > Settings::fParryWindow_End) {
			it = _parryTimer.erase(it);
			continue;
		}
		static float* g_deltaTime = (float*)RELOCATION_ID(523660, 410199).address();          // 2F6B948
		it->second += *g_deltaTime;
		it++;
	}
}

void EldenParry::startTimingParry(RE::Actor* a_actor) {

	uniqueLocker lock(mtx_parryTimer);
	auto it = _parryTimer.find(a_actor);
	if (it != _parryTimer.end()) {
		it->second = 0;
	} else {
		_parryTimer.insert({a_actor, 0.0f});
	}
	
	_bUpdate = true;
}

void EldenParry::finishTimingParry(RE::Actor* a_actor) {
	uniqueLocker lock(mtx_parryTimer);
	_parryTimer.erase(a_actor);
}

/// <summary>
/// Check if the object is in the blocker's blocking angle.
/// </summary>
/// <param name="a_blocker"></param>
/// <param name="a_obj"></param>
/// <returns>True if the object is in blocker's blocking angle.</returns>
bool EldenParry::inBlockAngle(RE::Actor* a_blocker, RE::TESObjectREFR* a_obj)
{
	auto angle = a_blocker->GetHeadingAngle(a_obj->GetPosition(), false);
	return (angle <= _parryAngle && angle >= -_parryAngle);
}
/// <summary>
/// Check if the actor is in parry state i.e. they are able to parry the incoming attack/projectile.
/// </summary>
/// <param name="a_actor"></param>
/// <returns></returns>
bool EldenParry::inParryState(RE::Actor* a_actor)
{
	sharedLocker lock(mtx_parryTimer);
	auto it = _parryTimer.find(a_actor);
	if (it != _parryTimer.end()) {
		return it->second >= Settings::fParryWindow_Start;
	}
	return false;
}

bool EldenParry::canParry(RE::Actor* a_parrier, RE::TESObjectREFR* a_obj)
{
	logger::info("{}",a_parrier->GetName());
	return inParryState(a_parrier) && inBlockAngle(a_parrier, a_obj);
}


bool EldenParry::processMeleeParry(RE::Actor* a_attacker, RE::Actor* a_parrier)
{
	if (canParry(a_parrier, a_attacker)) {
		playParryEffects(a_parrier);
		Utils::triggerStagger(a_parrier, a_attacker);
		if (Settings::facts::isValhallaCombatAPIObtained) {
			_ValhallaCombat_API->processStunDamage(VAL_API::STUNSOURCE::parry, nullptr, a_parrier, a_attacker, 0);
		}
		if (a_parrier->IsPlayerRef()) {
			RE::PlayerCharacter::GetSingleton()->AddSkillExperience(RE::ActorValue::kBlock, Settings::fMeleeParryExp);
		}
		if (Settings::bSuccessfulParryNoCost) {
			negateParryCost(a_parrier);
		}
		send_melee_parry_event(a_attacker);
		return true;
	}

	return false;

	
}

/// <summary>
/// Process a projectile parry; Return if the parry is successful.
/// </summary>
/// <param name="a_parrier"></param>
/// <param name="a_projectile"></param>
/// <param name="a_projectile_collidable"></param>
/// <returns>True if the projectile parry is successful.</returns>
bool EldenParry::processProjectileParry(RE::Actor* a_parrier, RE::Projectile* a_projectile, RE::hkpCollidable* a_projectile_collidable)
{
	if (canParry(a_parrier, a_projectile)) {
		RE::TESObjectREFR* shooter = nullptr;
		if (a_projectile->GetProjectileRuntimeData().shooter && a_projectile->GetProjectileRuntimeData().shooter.get()) {
			shooter = a_projectile->GetProjectileRuntimeData().shooter.get().get();
		}

		Utils::resetProjectileOwner(a_projectile, a_parrier, a_projectile_collidable);

		if (shooter && shooter->Is3DLoaded()) {
			Utils::RetargetProjectile(a_projectile, shooter);
		} else {
			Utils::ReflectProjectile(a_projectile);
		}
		
		playParryEffects(a_parrier);
		if (a_parrier->IsPlayerRef()) {
			RE::PlayerCharacter::GetSingleton()->AddSkillExperience(RE::ActorValue::kBlock, Settings::fProjectileParryExp);
		}
		if (Settings::bSuccessfulParryNoCost) {
			negateParryCost(a_parrier);
		}
		send_ranged_parry_event();
		return true;
	}
	return false;

}

void EldenParry::processGuardBash(RE::Actor* a_basher, RE::Actor* a_blocker)
{
	if (!a_blocker->IsBlocking() || !inBlockAngle(a_blocker, a_basher) || a_blocker->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kBash) {
		return;
	}
	Utils::triggerStagger(a_basher, a_blocker);
	playGuardBashEffects(a_basher);
	RE::PlayerCharacter::GetSingleton()->AddSkillExperience(RE::ActorValue::kBlock, Settings::fGuardBashExp);
}

void EldenParry::playParryEffects(RE::Actor* a_parrier) {
	if (Settings::bEnableParrySoundEffect) {
		if (Utils::isEquippedShield(a_parrier)) {
			Utils::playSound(a_parrier, _parrySound_shd);
		} else {
			Utils::playSound(a_parrier, _parrySound_wpn);
		}
	}
	if (Settings::bEnableParrySparkEffect) {
		blockSpark::playBlockSpark(a_parrier);
	}
	if (a_parrier->IsPlayerRef()) {
		if (Settings::bEnableSlowTimeEffect) {
			Utils::slowTime(0.2f, 0.3f);
		}
		if (Settings::bEnableScreenShakeEffect) {
			inlineUtils::shakeCamera(1.5, a_parrier->GetPosition(), 0.4f);
		}
	}
	
}

void EldenParry::applyParryCost(RE::Actor* a_actor) {
	//logger::logger::info("apply parry cost for {}", a_actor->GetName());
	std::lock_guard<std::shared_mutex> lock(mtx_parryCostQueue);
	std::lock_guard<std::shared_mutex> lock2(mtx_parrySuccessActors);
	if (_parryCostQueue.contains(a_actor)) {
		if (!_parrySuccessActors.contains(a_actor)) {
			inlineUtils::damageAv(a_actor, RE::ActorValue::kStamina, _parryCostQueue[a_actor]);
		}
		_parryCostQueue.erase(a_actor);
	}
	_parrySuccessActors.erase(a_actor);
}

void EldenParry::cacheParryCost(RE::Actor* a_actor, float a_cost) {
	//logger::logger::info("cache parry cost for {}: {}", a_actor->GetName(), a_cost);
	std::lock_guard<std::shared_mutex> lock(mtx_parryCostQueue);
	_parryCostQueue[a_actor] = a_cost;
}

void EldenParry::negateParryCost(RE::Actor* a_actor) {
	//logger::logger::info("negate parry cost for {}", a_actor->GetName());
	std::lock_guard<std::shared_mutex> lock(mtx_parrySuccessActors);
	_parrySuccessActors.insert(a_actor);
}

void EldenParry::playGuardBashEffects(RE::Actor* a_actor) {
	if (Settings::bEnableParrySoundEffect) {
			Utils::playSound(a_actor, _parrySound_shd);
	}
	if (Settings::bEnableParrySparkEffect) {
		blockSpark::playBlockSpark(a_actor);
	}
	if (a_actor->IsPlayerRef()) {
		if (Settings::bEnableSlowTimeEffect) {
			Utils::slowTime(0.2f, 0.3f);
		}
		if (Settings::bEnableScreenShakeEffect) {
			inlineUtils::shakeCamera(1.5, a_actor->GetPosition(), 0.4f);
		}
	}
}

void EldenParry::send_melee_parry_event(RE::Actor* a_attacker) {
	SKSE::ModCallbackEvent modEvent{
				RE::BSFixedString("EP_MeleeParryEvent"),
				RE::BSFixedString(),
				0.0f,
				a_attacker
	};

	SKSE::GetModCallbackEventSource()->SendEvent(&modEvent);
	logger::info("Sent melee parry event");
}

void EldenParry::send_ranged_parry_event() {
	SKSE::ModCallbackEvent modEvent{
				RE::BSFixedString("EP_RangedParryEvent"),
				RE::BSFixedString(),
				0.0f,
				nullptr
	};

	SKSE::GetModCallbackEventSource()->SendEvent(&modEvent);
	logger::info("Sent ranged parry event");
}

PRECISION_API::PreHitCallbackReturn EldenParry::precisionPrehitCallbackFunc(const PRECISION_API::PrecisionHitData& a_precisionHitData) {
	PRECISION_API::PreHitCallbackReturn returnData;
	if (!a_precisionHitData.target || !a_precisionHitData.target->Is(RE::FormType::ActorCharacter)) {
		return returnData;
	}
	if (EldenParry::GetSingleton()->processMeleeParry(a_precisionHitData.attacker, a_precisionHitData.target->As<RE::Actor>())) {
		returnData.bIgnoreHit = true;
	}
	return returnData;
}

const RE::TESObjectWEAP *const EldenParry::GetAttackWeapon(RE::AIProcess *const aiProcess)
{
	if (aiProcess && aiProcess->high && aiProcess->high->attackData &&
		!aiProcess->high->attackData->data.flags.all(RE::AttackData::AttackFlag::kBashAttack))
	{

		const RE::TESForm *equipped = aiProcess->high->attackData->IsLeftAttack() ? aiProcess->GetEquippedLeftHand()
																				  : aiProcess->GetEquippedRightHand();

		if (equipped)
		{
			return equipped->As<RE::TESObjectWEAP>();
		}
	}

	return nullptr;
}

double EldenParry::GetScore(RE::Actor *actor, const Milf::Scores &scoreSettings)
{
	double score = 0.0;

	RE::TESObjectWEAP* weapon = nullptr

	bool WeaponShield = false;

	auto defenderLeftEquipped = actor->GetEquippedObject(true);
	auto defenderRightEquipped = actor->GetEquippedObject(false);
	
	if (defenderLeftEquipped && (defenderLeftEquipped->IsWeapon() || defenderLeftEquipped->IsArmor())) {

		if (defenderLeftEquipped->As<RE::TESObjectWEAP>()) {
			weapon = (defenderLeftEquipped->As<RE::TESObjectWEAP>());
		
			switch (weapon->GetWeaponType()) {
			case RE::WEAPON_TYPE::kOneHandSword:
				score += scoreSettings.oneHandSwordScore;
				break;
			case RE::WEAPON_TYPE::kOneHandAxe:
			    score += scoreSettings.oneHandAxeScore;
				break;
			case RE::WEAPON_TYPE::kOneHandMace:
			    score += scoreSettings.oneHandMaceScore;
				break;
			case RE::WEAPON_TYPE::kOneHandDagger:
			    score += scoreSettings.oneHandDaggerScore;
				break;
			}
		} else if (defenderLeftEquipped->IsArmor()) {
			WeaponShield = true;
			score += 70.0;
		}

	} else if (defenderRightEquipped && (defenderRightEquipped->IsWeapon())) {
		weapon = (defenderRightEquipped->As<RE::TESObjectWEAP>());
		switch (weapon->GetWeaponType())
		{
		case RE::WEAPON_TYPE::kOneHandSword:
			score += scoreSettings.oneHandSwordScore;
			break;
		case RE::WEAPON_TYPE::kOneHandAxe:
			score += scoreSettings.oneHandAxeScore;
			break;
		case RE::WEAPON_TYPE::kOneHandMace:
			score += scoreSettings.oneHandMaceScore;
			break;
		case RE::WEAPON_TYPE::kOneHandDagger:
			score += scoreSettings.oneHandDaggerScore;
			break;
		case RE::WEAPON_TYPE::kTwoHandAxe:
			score += scoreSettings.twoHandAxeScore;
			break;
		case RE::WEAPON_TYPE::kTwoHandSword:
			score += scoreSettings.twoHandSwordScore;
			break;
		case RE::WEAPON_TYPE::kHandToHandMelee:
			score += -50.0;
			break;
		}
	}

	if (WeaponShield == false) {

		const auto actorValue = weapon->weaponData.skill.get();
		switch (actorValue)
		{
		case RE::ActorValue::kOneHanded:
			score += (scoreSettings.weaponSkillWeight *
					  actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kOneHanded));
			break;
		case RE::ActorValue::kTwoHanded:
			score += (scoreSettings.weaponSkillWeight *
					  actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kTwoHanded));
			break;
		default:
			// Do nothing
			break;
		}
	} else {
		score += (scoreSettings.weaponSkillWeight *
				  actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kBlock));
	}

	
	const auto race = actor->GetRace();
	const auto raceFormID = race->formID;

	if (raceFormID == 0x13743 || raceFormID == 0x88840)
	{
		score += scoreSettings.altmerScore;
	}
	else if (raceFormID == 0x13740 || raceFormID == 0x8883A)
	{
		score += scoreSettings.argonianScore;
	}
	else if (raceFormID == 0x13749 || raceFormID == 0x88884)
	{
		score += scoreSettings.bosmerScore;
	}
	else if (raceFormID == 0x13741 || raceFormID == 0x8883C)
	{
		score += scoreSettings.bretonScore;
	}
	else if (raceFormID == 0x13742 || raceFormID == 0x8883D)
	{
		score += scoreSettings.dunmerScore;
	}
	else if (raceFormID == 0x13744 || raceFormID == 0x88844)
	{
		score += scoreSettings.imperialScore;
	}
	else if (raceFormID == 0x13745 || raceFormID == 0x88845)
	{
		score += scoreSettings.khajiitScore;
	}
	else if (raceFormID == 0x13746 || raceFormID == 0x88794)
	{
		score += scoreSettings.nordScore;
	}
	else if (raceFormID == 0x13747 || raceFormID == 0xA82B9)
	{
		score += scoreSettings.orcScore;
	}
	else if (raceFormID == 0x13748 || raceFormID == 0x88846)
	{
		score += scoreSettings.redguardScore;
	}

	const auto actorBase = actor->GetActorBase();
	if (actorBase && actorBase->IsFemale())
	{
		score += scoreSettings.femaleScore;
	}

	if (inlineUtils::isPowerAttacking(actor))
	{
		score += scoreSettings.powerAttackScore;
	}

	if (actor->IsPlayerRef())
	{
		score += scoreSettings.playerScore;
	}

	return score;
}


double EldenParry::AttackerBeatsParry(RE::Actor *attacker, RE::Actor *target)
{
	// if (!Milf::GetSingleton()->core.useScoreSystem)
	// {
	// 	// The score-based system has been disabled in INI, so attackers can never overpower parries
	// 	return false;
	// }
	const double attackerScore = GetScore(attacker, Milf::GetSingleton()->scores);
	const double targetScore = GetScore(target, Milf::GetSingleton()->scores);

	return (attackerScore - targetScore); // >= Milf::GetSingleton()->scores.scoreDiffThreshold);
}


Milf *Milf::GetSingleton()
{
	static Milf singleton;
	return std::addressof(singleton);
}

void Milf::Load()
{
	constexpr auto path = "Data\\SKSE\\Plugins\\EldenRiposteSystem.ini";

	CSimpleIniA ini;
	ini.SetUnicode();

	ini.LoadFile(path);

	core.Load(ini);
	scores.Load(ini);

	ini.SaveFile(path);
}

void Milf::Core::Load(CSimpleIniA &a_ini)
{
	static const char *section = "Core";

	detail::get_value(a_ini, useScoreSystem, section, "UseScoreSystem",
					  ";Use the score-based system to allow certain attacks to go through and ignore parries.");
}

void Milf::Scores::Load(CSimpleIniA &a_ini)
{
	static const char *section = "Scores";

	detail::get_value(a_ini, scoreDiffThreshold, section, "ScoreDiffThreshold",
					  ";If the difference in scores is at least equal to this threshold, attacks are not parried.");

	detail::get_value(a_ini, weaponSkillWeight, section, "WeaponSkillWeight",
					  ";Weapon Skill is multiplied by this weight and then added to the score.");

	detail::get_value(a_ini, oneHandDaggerScore, section, "OneHandDaggerScore",
					  ";Bonus score for attacks with daggers.");
	detail::get_value(a_ini, oneHandSwordScore, section, "OneHandSwordScore",
					  ";Bonus score for attacks with one-handed swords.");
	detail::get_value(a_ini, oneHandAxeScore, section, "OneHandAxeScore",
					  ";Bonus score for attacks with one-handed axes.");
	detail::get_value(a_ini, oneHandMaceScore, section, "OneHandMaceScore",
					  ";Bonus score for attacks with one-handed maces.");
	detail::get_value(a_ini, oneHandKatanaScore, section, "OneHandKatanaScore",
					  ";Bonus score for attacks with katanas (from Animated Armoury).");
	detail::get_value(a_ini, oneHandRapierScore, section, "OneHandRapierScore",
					  ";Bonus score for attacks with rapiers (from Animated Armoury).");
	detail::get_value(a_ini, oneHandClawsScore, section, "OneHandClawsScore",
					  ";Bonus score for attacks with claws (from Animated Armoury).");
	detail::get_value(a_ini, oneHandWhipScore, section, "OneHandWhipScore",
					  ";Bonus score for attacks with whips (from Animated Armoury).");
	detail::get_value(a_ini, twoHandSwordScore, section, "TwoHandSwordScore",
					  ";Bonus score for attacks with two-handed swords.");
	detail::get_value(a_ini, twoHandAxeScore, section, "TwoHandAxeScore",
					  ";Bonus score for attacks with two-handed axes.");
	detail::get_value(a_ini, twoHandWarhammerScore, section, "TwoHandWarhammerScore",
					  ";Bonus score for attacks with two-handed warhammers.");
	detail::get_value(a_ini, twoHandPikeScore, section, "TwoHandPikeScore",
					  ";Bonus score for attacks with two-handed pikes (from Animated Armoury).");
	detail::get_value(a_ini, twoHandHalberdScore, section, "TwoHandHalberdScore",
					  ";Bonus score for attacks with two-handed halberds (from Animated Armoury).");
	detail::get_value(a_ini, twoHandQuarterstaffScore, section, "TwoHandQuarterstaffScore",
					  ";Bonus score for attacks with two-handed quarterstaffs (from Animated Armoury).");

	detail::get_value(a_ini, altmerScore, section, "AltmerScore",
					  ";Bonus score for Altmer.");
	detail::get_value(a_ini, argonianScore, section, "ArgonianScore",
					  ";Bonus score for Argonians.");
	detail::get_value(a_ini, bosmerScore, section, "BosmerScore",
					  ";Bonus score for Bosmer.");
	detail::get_value(a_ini, bretonScore, section, "BretonScore",
					  ";Bonus score for Bretons.");
	detail::get_value(a_ini, dunmerScore, section, "DunmerScore",
					  ";Bonus score for Dunmer.");
	detail::get_value(a_ini, imperialScore, section, "ImperialScore",
					  ";Bonus score for Imperials.");
	detail::get_value(a_ini, khajiitScore, section, "KhajiitScore",
					  ";Bonus score for Khajiit.");
	detail::get_value(a_ini, nordScore, section, "NordScore",
					  ";Bonus score for Nords.");
	detail::get_value(a_ini, orcScore, section, "OrcScore",
					  ";Bonus score for Orcs.");
	detail::get_value(a_ini, redguardScore, section, "RedguardScore",
					  ";Bonus score for Redguard.");

	detail::get_value(a_ini, femaleScore, section, "FemaleScore",
					  ";Bonus score for female characters.");

	detail::get_value(a_ini, powerAttackScore, section, "PowerAttackScore",
					  ";Bonus score for power attacks.");

	detail::get_value(a_ini, playerScore, section, "PlayerScore", ";Bonus score for the Player.");
}