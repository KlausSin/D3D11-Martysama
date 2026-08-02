#include "StdAfx.h"
#include <algorithm>
#include "../eterBase/Timer.h"

#include "GrpLightManager.h"
#include "ShaderManager.h"

#include "../EterBase/StepTimer.h"

float CLightBase::ms_fCurTime = 0.0f;

CLightManager::CLightManager()
{
	m_v3CenterPosition			= Vector3(0.0f, 0.0f, 0.0f);
	m_v3LastSortPosition		= Vector3(0.0f, 0.0f, 0.0f);
	m_dwLimitLightCount			= LIGHT_LIMIT_DEFAULT;
	m_bLightsDirty				= true;
}

CLightManager::~CLightManager()
{
}

void CLightManager::Destroy()
{
	m_LightPool.Destroy();
}

void CLightManager::Initialize()
{
	SetSkipIndex(1);

	m_NonUsingLightIDDeque.clear();

	m_LightMap.clear();
	m_LightPool.FreeAll();
}

void CLightManager::RegisterLight(ELightBehavior /*LightBehavior*/, TLightID * poutLightID, TLight & LightData)
{
	CLight * pLight = m_LightPool.Alloc();
	TLightID ID = NewLightID();
	pLight->SetParameter(ID, LightData);
	m_LightMap[ID] = pLight;
	*poutLightID = ID;
	m_bLightsDirty = true;
}

void CLightManager::DeleteLight(TLightID LightID)
{
	TLightMap::iterator itor = m_LightMap.find(LightID);

	if (m_LightMap.end() == itor)
	{
		assert(!"CLightManager::DeleteLight - Failed to find light ID!");
		return;
	}

	CLight * pLight = itor->second;

	pLight->Clear();
	m_LightPool.Free(pLight);

	m_LightMap.erase(itor);

	ReleaseLightID(LightID);
	m_bLightsDirty = true;
}

CLight * CLightManager::GetLight(TLightID LightID)
{
	TLightMap::iterator itor = m_LightMap.find(LightID);

	if (m_LightMap.end() == itor)
	{
		assert(!"CLightManager::SetLightData - Failed to find light ID!");
		return NULL;
	}

	return itor->second;
}

void CLightManager::SetCenterPosition(const Vector3 & c_rv3Position)
{
	m_v3CenterPosition = c_rv3Position;
}

void CLightManager::SetLimitLightCount(DWORD dwLightCount)
{
	m_dwLimitLightCount = dwLightCount;
}

void CLightManager::SetSkipIndex(DWORD dwSkipIndex)
{
	m_dwSkipIndex = dwSkipIndex;
}

struct LightComp
{
	bool operator () (const CLight * l, const CLight * r) const
	{
		return l->GetDistance() < r->GetDistance();
	}
};

void CLightManager::FlushLight()
{
	Update();

	Vector3 v3PosDiff = m_v3CenterPosition - m_v3LastSortPosition;
	float fDistanceMoved = Vec3LengthSq(&v3PosDiff);
	bool bNeedResort = m_bLightsDirty || (fDistanceMoved > LIGHT_SORT_DISTANCE_THRESHOLD * LIGHT_SORT_DISTANCE_THRESHOLD);

	if (bNeedResort)
	{
		m_LightSortVector.clear();

		TLightMap::iterator itor = m_LightMap.begin();

		for (; itor != m_LightMap.end(); ++itor)
		{
			CLight * pLight = itor->second;

			Vector3 v3LightPos(pLight->GetPosition());
			Vector3 v3Distance(v3LightPos - m_v3CenterPosition);
			pLight->SetDistance(Vec3Length(&v3Distance));
			m_LightSortVector.push_back(pLight);
		}

		// quick sort lights
		std::sort(m_LightSortVector.begin(), m_LightSortVector.end(), LightComp());

		m_v3LastSortPosition = m_v3CenterPosition;
		m_bLightsDirty = false;
	}

	// Save and enable lighting
	m_bSavedLighting = SHADERMANAGER.GetLightingEnabled();
	SHADERMANAGER.SetLightingEnabled(true);

	for (DWORD k = 0; k < min(m_dwLimitLightCount, m_LightSortVector.size()); ++k)
	{
		m_LightSortVector[k]->Update();
		m_LightSortVector[k]->SetDeviceLight(TRUE);
	}
}

void CLightManager::RestoreLight()
{
	// Restore lighting state
	SHADERMANAGER.SetLightingEnabled(m_bSavedLighting);

	for (DWORD k = 0; k < min(m_dwLimitLightCount, m_LightSortVector.size()); ++k)
		m_LightSortVector[k]->SetDeviceLight(FALSE);
}

TLightID CLightManager::NewLightID()
{
	if (!m_NonUsingLightIDDeque.empty())
	{
		TLightID id = m_NonUsingLightIDDeque.back();
		m_NonUsingLightIDDeque.pop_back();
		return (id);
	}

	return (TLightID)(m_dwSkipIndex + m_LightMap.size());
}

void CLightManager::ReleaseLightID(TLightID LightID)
{
	m_NonUsingLightIDDeque.push_back(LightID);
}

void CLightManager::Update()
{
	ms_fCurTime = DX::StepTimer::Instance().GetTotalSeconds();
}

//////////////////////////////////////////////////////////////////////////
CLight::CLight()
{
	Initialize();
}

CLight::~CLight()
{
	Clear();
}

void CLight::Initialize()
{
	m_LightID	= 0;
	m_isEdited	= TRUE;
	m_fDistance	= 0.0f;

	memset(&m_d3dLight, 0, sizeof(m_d3dLight));

	m_d3dLight.Type			= LIGHT_POINT;
	m_d3dLight.Attenuation0	= 0.0f;
	m_d3dLight.Attenuation1	= 1.0f;
	m_d3dLight.Attenuation2	= 0.0f;
}

void CLight::Clear()
{
	if (m_LightID)
		SetDeviceLight(FALSE);
	Initialize();
}

void CLight::SetDeviceLight(BOOL bActive)
{
	if (bActive && m_isEdited)
	{
		SHADERMANAGER.SetLight(m_LightID, &m_d3dLight);
	}
	SHADERMANAGER.LightEnable(m_LightID, bActive);
}

void CLight::SetParameter(TLightID id, const TLight & c_rLight)
{
	m_LightID	= id;
	m_d3dLight	= c_rLight;
}

void CLight::SetDiffuseColor(float fr, float fg, float fb, float fa)
{
	if (m_d3dLight.Diffuse.r == fr
		&& m_d3dLight.Diffuse.g == fg
		&& m_d3dLight.Diffuse.b == fb
		&& m_d3dLight.Diffuse.a == fa
		)
		return;
	m_d3dLight.Diffuse.r = fr;
	m_d3dLight.Diffuse.g = fg;
	m_d3dLight.Diffuse.b = fb;
	m_d3dLight.Diffuse.a = fa;
	m_isEdited = TRUE;
}

void CLight::SetAmbientColor(float fr, float fg, float fb, float fa)
{
	if (m_d3dLight.Ambient.r == fr
		&& m_d3dLight.Ambient.g == fg
		&& m_d3dLight.Ambient.b == fb
		&& m_d3dLight.Ambient.a == fa
		)
		return;
	m_d3dLight.Ambient.r = fr;
	m_d3dLight.Ambient.g = fg;
	m_d3dLight.Ambient.b = fb;
	m_d3dLight.Ambient.a = fa;
	m_isEdited = TRUE;
}

void CLight::SetRange(float fRange)
{
	if (m_d3dLight.Range == fRange)
		return;

	m_d3dLight.Range = fRange;
	m_isEdited = TRUE;
}

const Vector3 & CLight::GetPosition() const
{
	return m_d3dLight.Position;
}

void CLight::SetPosition(float fx, float fy, float fz)
{
	if (m_d3dLight.Position.x == fx && m_d3dLight.Position.y == fy && m_d3dLight.Position.z == fz)
		return;

	m_d3dLight.Position.x = fx;
	m_d3dLight.Position.y = fy;
	m_d3dLight.Position.z = fz;
	m_isEdited = TRUE;
}

void CLight::SetDistance(float fDistance)
{
	m_fDistance = fDistance;
}

void CLight::BlendDiffuseColor(const Color & c_rColor, float fBlendTime, float fDelayTime)
{
	Color Color(m_d3dLight.Diffuse);
	m_DiffuseColorTransitor.SetTransition(Color, c_rColor, ms_fCurTime + fDelayTime, fBlendTime);
}

void CLight::BlendAmbientColor(const Color & c_rColor, float fBlendTime, float fDelayTime)
{
	Color Color(m_d3dLight.Ambient);
	m_AmbientColorTransitor.SetTransition(Color, c_rColor, ms_fCurTime + fDelayTime, fBlendTime);
}

void CLight::BlendRange(float fRange, float fBlendTime, float fDelayTime)
{
	m_RangeTransitor.SetTransition(m_d3dLight.Range, fRange, ms_fCurTime + fDelayTime, fBlendTime);
}

void CLight::Update()
{
	if (m_AmbientColorTransitor.isActiveTime(ms_fCurTime))
	{
		if (!m_AmbientColorTransitor.isActive())
		{
			m_AmbientColorTransitor.SetActive();
			m_AmbientColorTransitor.SetSourceValue(m_d3dLight.Ambient);
		}
		else
		{
			Color Color;

			m_AmbientColorTransitor.GetValue(ms_fCurTime, &Color);
			SetAmbientColor(Color.r, Color.g, Color.b, Color.a);
		}
	}

	if (m_DiffuseColorTransitor.isActiveTime(ms_fCurTime))
	{
		if (!m_DiffuseColorTransitor.isActive())
		{
			m_DiffuseColorTransitor.SetActive();
			m_DiffuseColorTransitor.SetSourceValue(m_d3dLight.Diffuse);
		}
		else
		{
			Color Color;
			m_DiffuseColorTransitor.GetValue(ms_fCurTime, &Color);
			SetDiffuseColor(Color.r, Color.g, Color.b, Color.a);
		}
	}

	if (m_RangeTransitor.isActiveTime(ms_fCurTime))
	{
		if (!m_RangeTransitor.isActive())
		{
			m_RangeTransitor.SetActive();
			m_RangeTransitor.SetSourceValue(m_d3dLight.Range);
		}
		else
		{
			float fRange;
			m_RangeTransitor.GetValue(ms_fCurTime, &fRange);
			SetRange(fRange);
		}
	}
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
