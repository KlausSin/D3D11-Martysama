#include "stdafx.h"
#include "SoundEngine.h"

#include "../eterBase/Random.h"
#include "../eterBase/StepTimer.h"
#include "../eterBase/MappedFile.h"
#include "../eterPack/EterPackManager.h"

SoundEngine::SoundEngine()
{
}

SoundEngine::~SoundEngine()
{
	if (m_bInitialized)
	{
		for (auto& [name, instance] : m_Sounds2D)
			instance.Destroy();

		for (auto& instance : m_Sounds3D)
			instance.Destroy();

		ma_engine_uninit(&m_Engine);
	}

	m_Files.clear();
	m_Sounds2D.clear();
}

bool SoundEngine::Initialize()
{
	HRESULT hrCom = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hrCom) && hrCom != RPC_E_CHANGED_MODE)
	{
		TraceError("SoundEngine::Initialize: COM initialization failed (0x%08X)", hrCom);
		// Continue anyway - COM might already be initialized
	}

	ma_engine_config engineConfig = ma_engine_config_init();
	engineConfig.listenerCount = 1;

	ma_result result = MA_ERROR;
	__try
	{
		result = ma_engine_init(&engineConfig, &m_Engine);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		TraceError("SoundEngine::Initialize: EXCEPTION during audio engine init (code: 0x%08X)", GetExceptionCode());
		TraceError("SoundEngine::Initialize: This usually means no audio device is available");
		m_bInitialized = false;
		return false;
	}

	if (result != MA_SUCCESS)
	{
		TraceError("SoundEngine::Initialize: Failed to initialize audio engine (error: %d)", result);
		TraceError("SoundEngine::Initialize: Audio will be disabled for this session");
		m_bInitialized = false;
		return false;  // Return false but don't crash - game can run without audio
	}

	m_bInitialized = true;
	ma_engine_listener_set_position(&m_Engine, 0, 0, 0, 0); // engine
	SetListenerPosition(0.0f, 0.0f, 0.0f); // character
	SetListenerOrientation(0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f);

	return true;
}

void SoundEngine::SetSoundVolume(float volume)
{
	m_SoundVolume = std::clamp<float>(volume, 0.0, 1.0);
}

bool SoundEngine::PlaySound2D(const std::string& name)
{
	if (!m_bInitialized)
		return false;

	if (!Internal_LoadSoundFromPack(name))
		return false;

	auto& instance = m_Sounds2D[name]; // 2d sounds are persistent, no need to destroy
	instance.InitFromBuffer(m_Engine, m_Files[name], name);
	instance.Config3D(false);
	instance.SetVolume(m_SoundVolume);
	return instance.Play();
}

MaSoundInstance* SoundEngine::PlaySound3D(const std::string& name, float fx, float fy, float fz)
{
	if (!m_bInitialized)
		return nullptr;

	auto instance = Internal_GetInstance3D(name);
	if (!instance)
	{
		TraceError("SoundEngine::PlaySound3D - Failed to get instance for '%s'", name.c_str());
		return nullptr;
	}

	constexpr float minDist = 100.0f; // 1m
	constexpr float maxDist = 5000.0f; // 50m

	float relX = fx - m_CharacterPosition.x;
	float relY = fy - m_CharacterPosition.y;
	float relZ = fz - m_CharacterPosition.z;
	float distance = sqrtf(relX * relX + relY * relY + relZ * relZ);

	bool isPlayerSound = (distance < 500.0f);

	instance->SetPosition(relX, relY, relZ);
	instance->Config3D(!isPlayerSound, minDist, maxDist); // Disable 3D for close sounds
	instance->SetVolume(m_SoundVolume);

	if (!instance->Play())
	{
		TraceError("SoundEngine::PlaySound3D - Play() failed for '%s'", name.c_str());
		return nullptr;
	}

	return instance;
}

MaSoundInstance* SoundEngine::PlayAmbienceSound3D(float fx, float fy, float fz, const std::string& name, int loopCount)
{
	if (!m_bInitialized)
		return nullptr;

	return PlaySound3D(name, fx, fy, fz);
}

void SoundEngine::StopAllSound3D()
{
	for (auto& instance : m_Sounds3D)
		instance.Stop();
}

void SoundEngine::UpdateSoundInstance(float fx, float fy, float fz, uint32_t dwcurFrame, const NSound::TSoundInstanceVector* c_pSoundInstanceVector, bool checkFrequency)
{
	if (c_pSoundInstanceVector->empty())
		return;

	for (uint32_t i = 0; i < c_pSoundInstanceVector->size(); ++i)
	{
		const NSound::TSoundInstance& c_rSoundInstance = c_pSoundInstanceVector->at(i);

		if (c_rSoundInstance.dwFrame != dwcurFrame)
			continue;

		if (checkFrequency)
		{
			float& lastPlay = m_PlaySoundHistoryMap[c_rSoundInstance.strSoundFileName];

			if (DX::StepTimer::Instance().GetTotalSeconds() - lastPlay < 0.3f)
				continue;

			lastPlay = (float)DX::StepTimer::Instance().GetTotalSeconds();
		}

		PlaySound3D(c_rSoundInstance.strSoundFileName, fx, fy, fz);
	}
}

bool SoundEngine::FadeInMusic(const std::string& path, float targetVolume /* 1.0f by default */, float fadeInDurationSecondsFromMin)
{
	if (!m_bInitialized || path.empty())
		return false;

	auto& fadeOutMusic = m_Music[m_CurrentMusicIndex];
	if (fadeOutMusic.IsPlaying() && path == fadeOutMusic.GetIdentity())
	{
		fadeOutMusic.Fade(targetVolume, fadeInDurationSecondsFromMin);
		return fadeOutMusic.Resume();
	}

	// We're basically just swapping
	FadeOutMusic(fadeOutMusic.GetIdentity());
	m_CurrentMusicIndex = int(!m_CurrentMusicIndex);

	auto& music = m_Music[m_CurrentMusicIndex];
	music.Destroy();
	music.InitFromFile(m_Engine, path);
	music.Config3D(false);
	music.Loop();
	music.SetVolume(0.0f);
	music.Fade(targetVolume, fadeInDurationSecondsFromMin);
	return music.Play();
}

void SoundEngine::FadeOutMusic(const std::string& name, float targetVolume, float fadeOutDurationSecondsFromMax)
{
	for (auto& music : m_Music)
	{
		if (music.GetIdentity() == name)
			music.Fade(targetVolume, fadeOutDurationSecondsFromMax);
	}
}

void SoundEngine::FadeOutAllMusic()
{
	for (auto& music : m_Music)
		FadeOutMusic(music.GetIdentity());
}

void SoundEngine::SetMusicVolume(float volume)
{
	m_MusicVolume = std::clamp<float>(volume, 0.0f, 1.0f);
	m_Music[m_CurrentMusicIndex].StopFading();
	m_Music[m_CurrentMusicIndex].SetVolume(m_MusicVolume);
}

float SoundEngine::GetMusicVolume() const
{
	return m_MusicVolume;
}

void SoundEngine::SaveVolume(bool isMinimized)
{
	constexpr float ratePerSecond = 1.0f / CS_CLIENT_FPS;
	// 1.0 to 0 in 1s if minimized, 3s if just out of focus
	const float durationOnFullVolume = isMinimized ? 1.0f : 3.0f;

	float outOfFocusVolume = 0.35f;
	if (m_MasterVolume <= outOfFocusVolume)
		outOfFocusVolume = m_MasterVolume;

	m_MasterVolumeFadeTarget = isMinimized ? 0.0f : outOfFocusVolume;
	m_MasterVolumeFadeRatePerFrame = -ratePerSecond / durationOnFullVolume;
}

void SoundEngine::RestoreVolume()
{
	constexpr float ratePerSecond = 1.0f / CS_CLIENT_FPS;
	constexpr float durationToFullVolume = 4.0f; // 0 to 1.0 in 4s
	m_MasterVolumeFadeTarget = m_MasterVolume;
	m_MasterVolumeFadeRatePerFrame = ratePerSecond / durationToFullVolume;
}

void SoundEngine::SetMasterVolume(float volume)
{
	m_MasterVolume = volume;
	if (m_bInitialized)
		ma_engine_set_volume(&m_Engine, volume);
}

void SoundEngine::SetListenerPosition(float x, float y, float z)
{
	m_CharacterPosition.x = x;
	m_CharacterPosition.y = y;
	m_CharacterPosition.z = z;
}

void SoundEngine::SetListenerOrientation(float forwardX, float forwardY, float forwardZ,
										 float upX, float upY, float upZ)
{
	if (!m_bInitialized)
		return;

	ma_engine_listener_set_direction(&m_Engine, 0, forwardX, forwardY, -forwardZ);
	ma_engine_listener_set_world_up(&m_Engine, 0, upX, -upY, upZ);
}

void SoundEngine::Update()
{
	if (!m_bInitialized)
		return;

	for (auto& music : m_Music)
		music.Update();

	if (m_MasterVolumeFadeRatePerFrame)
	{
		float volume = ma_engine_get_volume(&m_Engine) + m_MasterVolumeFadeRatePerFrame;
		if ((m_MasterVolumeFadeRatePerFrame > 0.0f && volume >= m_MasterVolumeFadeTarget) || (m_MasterVolumeFadeRatePerFrame < 0.0f && volume <= m_MasterVolumeFadeTarget))
		{
			volume = m_MasterVolumeFadeTarget;
			m_MasterVolumeFadeRatePerFrame = 0.0f;
		}
		ma_engine_set_volume(&m_Engine, volume);
	}
}

MaSoundInstance* SoundEngine::Internal_GetInstance3D(const std::string& name)
{
	if (!m_bInitialized)
		return nullptr;

	if (!Internal_LoadSoundFromPack(name))
	{
		TraceError("Internal_GetInstance3D: Failed to load sound from pack: %s", name.c_str());
		return nullptr;
	}

	int slotIndex = 0;
	for (auto& instance : m_Sounds3D)
	{
		if (!instance.IsPlaying())
		{
			instance.Destroy();
			if (!instance.InitFromBuffer(m_Engine, m_Files[name], name))
			{
				TraceError("Internal_GetInstance3D: InitFromBuffer failed for '%s'", name.c_str());
				return nullptr;
			}
			return &instance;
		}
		slotIndex++;
	}

	TraceError("Internal_GetInstance3D: No available sound slots (all %d are playing)", SOUND_INSTANCE_3D_MAX_NUM);
	return nullptr;
}

bool SoundEngine::Internal_LoadSoundFromPack(const std::string& name)
{
	if (m_Files.find(name) == m_Files.end())
	{
		CMappedFile kMappedFile;
		LPCVOID pData;
		if (!CEterPackManager::Instance().Get(kMappedFile, name.c_str(), &pData))
		{
			TraceError("Internal_LoadSoundFromPack: SoundEngine: Failed to register file '%s' - not found.", name.c_str());
			return false;
		}

		auto& buffer = m_Files[name];
		buffer.resize(kMappedFile.Size());
		memcpy(buffer.data(), pData, kMappedFile.Size());
	}
	return true;
}
