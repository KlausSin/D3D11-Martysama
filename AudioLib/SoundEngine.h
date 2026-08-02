#pragma once
#include "../eterBase/Singleton.h"
#include "Type.h"
#include "MaSoundInstance.h"

#include <array>
#include <unordered_map>

struct SoundFile
{
	std::string name;
	std::vector<std::byte> buffer; // raw file data. 
};

class SoundEngine : public CSingleton<SoundEngine>
{
public:
	enum ESoundConfig
	{
		SOUND_INSTANCE_3D_MAX_NUM = 128,
	};
public:
	SoundEngine();

	~SoundEngine();

	bool Initialize();

	void SetSoundVolume(float volume);

	bool PlaySound2D(const std::string& name);

	MaSoundInstance* PlaySound3D(const std::string& name, float fx, float fy, float fz);

	MaSoundInstance* PlayAmbienceSound3D(float fx, float fy, float fz, const std::string& name, int loopCount = 1);

	void StopAllSound3D();

	void UpdateSoundInstance(float fx, float fy, float fz, uint32_t dwcurFrame, const NSound::TSoundInstanceVector* c_pSoundInstanceVector, bool checkFrequency = false);

	bool FadeInMusic(const std::string& path, float targetVolume = 1.0f, float fadeInDurationSecondsFromMin = 1.5f);

	void FadeOutMusic(const std::string& name, float targetVolume = 0.0f, float fadeOutDurationSecondsFromMax = 1.5f);

	void FadeOutAllMusic();

	void SetMusicVolume(float volume);

	float GetMusicVolume() const;

	void SaveVolume(bool isMinimized);

	void RestoreVolume();

	void SetMasterVolume(float volume);

	void SetListenerPosition(float x, float y, float z);

	void SetListenerOrientation(float forwardX, float forwardY, float forwardZ,
								float upX, float upY, float upZ);

	void Update();


private:
	MaSoundInstance* Internal_GetInstance3D(const std::string& name);

	bool Internal_LoadSoundFromPack(const std::string& name);

private:
	struct { float x, y, z; } m_CharacterPosition{};

	ma_engine m_Engine{};
	std::unordered_map<std::string, std::vector<uint8_t>> m_Files;
	std::unordered_map<std::string, MaSoundInstance> m_Sounds2D;
	std::array<MaSoundInstance, SOUND_INSTANCE_3D_MAX_NUM> m_Sounds3D;
	std::unordered_map<std::string, float> m_PlaySoundHistoryMap;

	std::array<MaSoundInstance, 2> m_Music;
	int m_CurrentMusicIndex{};
	float m_MusicVolume{ 1.0f };
	float m_SoundVolume{ 1.0f };

	float m_MasterVolume{ 1.0f };
	float m_MasterVolumeFadeTarget{};
	float m_MasterVolumeFadeRatePerFrame{};

	bool m_bInitialized{ false };  // True if audio engine initialized successfully
};
