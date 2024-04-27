#pragma once

#include <SimpleIni.h>

static const char* settingsDir = "Data\\SKSE\\Plugins\\EldenParry.ini";
class Settings
{
public:
	class facts
	{
	public:
		static inline bool isPrecisionAPIObtained = false;
		static inline bool isValhallaCombatAPIObtained = false;
	};
	static inline float fParryWindow_Start = 0.0f;
	static inline float fParryWindow_End = 0.3f;

	static inline bool bEnableWeaponParry = true;
	static inline bool bEnableShieldParry = true;
	static inline bool bEnableNPCParry = true;
	static inline bool bSuccessfulParryNoCost = true;



	static inline bool bEnableSlowTimeEffect = false;
	static inline bool bEnableScreenShakeEffect = true;
	static inline bool bEnableParrySparkEffect = true;
	static inline bool bEnableParrySoundEffect = true;


	static inline bool bEnableArrowProjectileDeflection = true;
	static inline bool bEnableMagicProjectileDeflection = true;

	static inline bool bEnableShieldGuardBash = true;
	static inline bool bEnableWeaponGuardBash = true;

	static inline float fProjectileParryExp = 20.0f;
	static inline float fMeleeParryExp = 10.0f;
	static inline float fGuardBashExp = 10.0f;

	static void readSettings();

	private:
	static bool readSimpleIni(CSimpleIniA& a_ini, const char* a_iniAddress)
	{
		if (std::filesystem::exists(a_iniAddress)) {
			a_ini.LoadFile(a_iniAddress);
			return true;
		} else {
			logger::critical("{} is not a valid ini address.", a_iniAddress);
			return false;
		}
	}

	static void ReadIntSetting(CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName, uint32_t& a_setting)
	{
		const char* bFound = nullptr;
		bFound = a_ini.GetValue(a_sectionName, a_settingName);
		if (bFound) {
			a_setting = static_cast<int>(a_ini.GetDoubleValue(a_sectionName, a_settingName));
		}
	}
	static void ReadFloatSetting(CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName, float& a_setting)
	{
		const char* bFound = nullptr;
		bFound = a_ini.GetValue(a_sectionName, a_settingName);
		if (bFound) {
			a_setting = static_cast<float>(a_ini.GetDoubleValue(a_sectionName, a_settingName));
		}
	}

	static void ReadBoolSetting(CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName, bool& a_setting)
	{
		const char* bFound = nullptr;
		bFound = a_ini.GetValue(a_sectionName, a_settingName);
		if (bFound) {
			a_setting = a_ini.GetBoolValue(a_sectionName, a_settingName);
		}
	}

public:
	[[nodiscard]] static Settings *GetSingleton();

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
	Settings() = default;
	Settings(const Settings &) = delete;
	Settings(Settings &&) = delete;
	~Settings() = default;

	Settings &operator=(const Settings &) = delete;
	Settings &operator=(Settings &&) = delete;

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
