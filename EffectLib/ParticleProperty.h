#pragma once

#include <vector>
#include "../eterlib/GrpImageInstance.h"

#include "Type.h"

class CParticleProperty
{
	friend class CParticleSystemData;
	friend class CParticleSystemInstance;
	public:
		enum
		{
			ROTATION_TYPE_NONE,
			ROTATION_TYPE_TIME_EVENT,
			ROTATION_TYPE_CW,
			ROTATION_TYPE_CCW,
			ROTATION_TYPE_RANDOM_DIRECTION,
		};

		enum
		{
			TEXTURE_ANIMATION_TYPE_NONE,
			TEXTURE_ANIMATION_TYPE_CW,
			TEXTURE_ANIMATION_TYPE_CCW,
			TEXTURE_ANIMATION_TYPE_RANDOM_FRAME,
			TEXTURE_ANIMATION_TYPE_RANDOM_DIRECTION,
		};

	public:
		CParticleProperty();
		virtual ~CParticleProperty();

		void Clear();

		void InsertTexture(const char * c_szFileName);
		bool SetTexture(const char * c_szFileName);

		__forceinline BYTE GetTextureAnimationType()
		{
			return m_byTexAniType;
		}

		__forceinline DWORD GetTextureAnimationFrameCount()
		{
			return (DWORD)(m_ImageVector.size());
		}

		__forceinline float GetTextureAnimationFrameDelay()
		{
			return m_fTexAniDelay;
		}

		BYTE m_byTexAniType;
		float m_fTexAniDelay;
		BOOL m_bTexAniRandomStartFrameFlag;

		BYTE m_bySrcBlendType;
		BYTE m_byDestBlendType;
		BYTE m_byColorOperationType;
		BYTE m_byBillboardType;

		BYTE m_byRotationType;
		float m_fRotationSpeed;
		WORD m_wRotationRandomStartingBegin;
		WORD m_wRotationRandomStartingEnd;

		BOOL m_bAttachFlag;
		BOOL m_bStretchFlag;

		TTimeEventTableFloat m_TimeEventGravity;
		TTimeEventTableFloat m_TimeEventAirResistance;

		TTimeEventTableFloat m_TimeEventScaleX;
		TTimeEventTableFloat m_TimeEventScaleY;
#ifdef WORLD_EDITOR
		TTimeEventTableFloat m_TimeEventColorRed;
		TTimeEventTableFloat m_TimeEventColorGreen;
		TTimeEventTableFloat m_TimeEventColorBlue;
		TTimeEventTableFloat m_TimeEventAlpha;

		std::vector<std::string> m_TextureNameVector;
#else
		TTimeEventTableColor m_TimeEventColor;
#endif
		TTimeEventTableFloat m_TimeEventRotation;

		std::vector<CGraphicImage*> m_ImageVector;

		CParticleProperty & operator = ( const CParticleProperty& c_ParticleProperty );

		// pre-transformed variables
		Vector3 m_v3ZAxis;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
