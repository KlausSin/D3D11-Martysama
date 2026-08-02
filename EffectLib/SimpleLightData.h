#pragma once

// DX11 includes
#include <d3d11.h>
#include <DirectXMath.h>
#include "../eterLib/GrpMathType.h"
#include "../eterLib/GrpMathFunc.h"

#include "../eterLib/TextFileLoader.h"

#include "Type.h"
#include "EffectElementBase.h"

class CLightData : public CEffectElementBase
{
	friend class CLightInstance;
	public:
		CLightData();
		virtual ~CLightData();

		void GetRange(float fTime, float& rRange);
		float GetDuration();
		BOOL isLoop()
		{
			return m_bLoopFlag;
		}
		int GetLoopCount()
		{
			return m_iLoopCount;
		}
		void InitializeLight(TLight& light);

	protected:
		void OnClear();
		bool OnIsData();

		BOOL OnLoadScript(CTextFileLoader & rTextFileLoader);

	protected:
		float m_fMaxRange;
		float m_fDuration;
		TTimeEventTableFloat m_TimeEventTableRange;

		Color m_cAmbient;
		Color m_cDiffuse;

		BOOL m_bLoopFlag;
		int m_iLoopCount;

		float m_fAttenuation0;
		float m_fAttenuation1;
		float m_fAttenuation2;

	public:
		static void DestroySystem();

		static CLightData* New();
		static void Delete(CLightData* pkData);

		static CDynamicPool<CLightData>		ms_kPool;
};
