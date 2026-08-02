#include "StdAfx.h"
#include "../eterLib/ShaderManager.h"

#include "ActorInstance.h"

bool CActorInstance::ms_isDirLine=false;

bool CActorInstance::IsDirLine()
{
	return ms_isDirLine;
}

void CActorInstance::ShowDirectionLine(bool isVisible)
{
	ms_isDirLine=isVisible;
}

void CActorInstance::SetMaterialColor(DWORD dwColor)
{
	if (m_pkHorse)
		m_pkHorse->SetMaterialColor(dwColor);

	m_dwMtrlColor&=0xff000000;
	m_dwMtrlColor|=(dwColor&0x00ffffff);
}

void CActorInstance::SetMaterialAlpha(DWORD dwAlpha)
{
	m_dwMtrlAlpha=dwAlpha;
}

void CActorInstance::OnRender()
{
	TMaterial kMtrl;
	SHADERMANAGER.GetMaterial(&kMtrl);

	kMtrl.Diffuse=Color(m_dwMtrlColor);
	SHADERMANAGER.SetMaterial(&kMtrl);

	SHADERMANAGER.SavePipelineState(PSTATE_CULLMODE, CULL_NONE);

	switch(m_iRenderMode)
	{
		case RENDER_MODE_NORMAL:
			BeginDiffuseRender();
				RenderWithOneTexture();
			EndDiffuseRender();
			BeginOpacityRender();
				BlendRenderWithOneTexture();
			EndOpacityRender();
			break;
		case RENDER_MODE_BLEND:
			if (m_fAlphaValue == 1.0f)
			{
				BeginDiffuseRender();
					RenderWithOneTexture();
				EndDiffuseRender();
				BeginOpacityRender();
					BlendRenderWithOneTexture();
				EndOpacityRender();
			}
			else if (m_fAlphaValue > 0.0f)
			{
				BeginBlendRender();
					RenderWithOneTexture();
					BlendRenderWithOneTexture();
				EndBlendRender();
			}
			break;
		case RENDER_MODE_ADD:
			BeginAddRender();
				RenderWithOneTexture();
				BlendRenderWithOneTexture();
			EndAddRender();
			break;
		case RENDER_MODE_MODULATE:
			BeginModulateRender();
				RenderWithOneTexture();
				BlendRenderWithOneTexture();
			EndModulateRender();
			break;
	}

	SHADERMANAGER.RestorePipelineState(PSTATE_CULLMODE);

	kMtrl.Diffuse=Color(0xffffffff);
	SHADERMANAGER.SetMaterial(&kMtrl);

	if (ms_isDirLine)
	{
		Vector3 kD3DVt3Cur(m_x, m_y, m_z);

		Vector3 kD3DVt3LookDir(0.0f, -1.0f, 0.0f);
		Matrix kD3DMatLook;
		MatrixRotationZ(&kD3DMatLook, ToRadian(GetRotation()));
		Vec3TransformCoord(&kD3DVt3LookDir, &kD3DVt3LookDir, &kD3DMatLook);
		Vec3Scale(&kD3DVt3LookDir, &kD3DVt3LookDir, 200.0f);
		Vec3Add(&kD3DVt3LookDir, &kD3DVt3LookDir, &kD3DVt3Cur);

		Vector3 kD3DVt3AdvDir(0.0f, -1.0f, 0.0f);
		Matrix kD3DMatAdv;
		MatrixRotationZ(&kD3DMatAdv, ToRadian(GetAdvancingRotation()));
		Vec3TransformCoord(&kD3DVt3AdvDir, &kD3DVt3AdvDir, &kD3DMatAdv);
		Vec3Scale(&kD3DVt3AdvDir, &kD3DVt3AdvDir, 200.0f);
		Vec3Add(&kD3DVt3AdvDir, &kD3DVt3AdvDir, &kD3DVt3Cur);

		static CScreen s_kScreen;

		SHADERMANAGER.SavePipelineState(PSTATE_DEPTHENABLE, FALSE);
		SHADERMANAGER.SetLightingEnabled(false);

		s_kScreen.SetDiffuseColor(1.0f, 1.0f, 0.0f);
		s_kScreen.RenderLine3d(kD3DVt3Cur.x, kD3DVt3Cur.y, kD3DVt3Cur.z, kD3DVt3AdvDir.x, kD3DVt3AdvDir.y, kD3DVt3AdvDir.z);

		s_kScreen.SetDiffuseColor(0.0f, 1.0f, 1.0f);
		s_kScreen.RenderLine3d(kD3DVt3Cur.x, kD3DVt3Cur.y, kD3DVt3Cur.z, kD3DVt3LookDir.x, kD3DVt3LookDir.y, kD3DVt3LookDir.z);

		SHADERMANAGER.SetLightingEnabled(true);
		SHADERMANAGER.RestorePipelineState(PSTATE_DEPTHENABLE);
	}
}

void CActorInstance::BeginDiffuseRender()
{
	SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, FALSE);
}

void CActorInstance::EndDiffuseRender()
{
	SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE);
}

void CActorInstance::BeginOpacityRender()
{
	m_bSavedAlphaTest = SHADERMANAGER.GetAlphaTestEnabled();
	m_dwSavedAlphaRef = SHADERMANAGER.GetAlphaTestRef();

	SHADERMANAGER.SetAlphaTestEnabled(true);
	SHADERMANAGER.SetAlphaTestRefByte(0);
}

void CActorInstance::EndOpacityRender()
{
	SHADERMANAGER.SetAlphaTestEnabled(m_bSavedAlphaTest);
	SHADERMANAGER.SetAlphaTestRefByte(m_dwSavedAlphaRef);
}

void CActorInstance::BeginBlendRender()
{
	SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, TRUE);
	SHADERMANAGER.SavePipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
	SHADERMANAGER.SavePipelineState(PSTATE_DESTBLEND, BLEND_INVSRCALPHA);
	if (SHADERMANAGER.IsInitialized())
		SHADERMANAGER.SetTextureFactor(Color(1.0f, 1.0f, 1.0f, m_fAlphaValue));
}

void CActorInstance::EndBlendRender()
{
	SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE);
	SHADERMANAGER.RestorePipelineState(PSTATE_SRCBLEND);
	SHADERMANAGER.RestorePipelineState(PSTATE_DESTBLEND);
}

void CActorInstance::BeginAddRender()
{
	if (SHADERMANAGER.IsInitialized())
		SHADERMANAGER.SetTextureFactor(m_AddColor);
	SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, FALSE);
}

void CActorInstance::EndAddRender()
{
	SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE);
}

void CActorInstance::RestoreRenderMode()
{
	m_iRenderMode = RENDER_MODE_NORMAL;
	if (m_kBlendAlpha.m_isBlending)
	{
		m_kBlendAlpha.m_iOldRenderMode = m_iRenderMode;
	}
}

void CActorInstance::SetAddRenderMode()
{
	m_iRenderMode = RENDER_MODE_ADD;
	if (m_kBlendAlpha.m_isBlending)
	{
		m_kBlendAlpha.m_iOldRenderMode = m_iRenderMode;
	}
}

void CActorInstance::SetRenderMode(int iRenderMode)
{
	m_iRenderMode = iRenderMode;
	if (m_kBlendAlpha.m_isBlending)
	{
		m_kBlendAlpha.m_iOldRenderMode = iRenderMode;
	}
}

void CActorInstance::SetAddColor(const Color & c_rColor)
{
	m_AddColor = c_rColor;
	m_AddColor.a = 1.0f;
}

void CActorInstance::BeginModulateRender()
{
	if (SHADERMANAGER.IsInitialized())
		SHADERMANAGER.SetTextureFactor(m_AddColor);
	SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, FALSE);
}

void CActorInstance::EndModulateRender()
{
	SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE);
}

void CActorInstance::SetModulateRenderMode()
{
	m_iRenderMode = RENDER_MODE_MODULATE;
	if (m_kBlendAlpha.m_isBlending)
	{
		m_kBlendAlpha.m_iOldRenderMode = m_iRenderMode;
	}
}

void CActorInstance::RenderCollisionData()
{
	static CScreen s_Screen;

	SHADERMANAGER.SetLightingEnabled(false);
	SHADERMANAGER.SavePipelineState(PSTATE_CULLMODE, CULL_NONE);
	if (m_pAttributeInstance)
	{
		for (DWORD col=0; col < GetCollisionInstanceCount(); ++col)
		{
			CBaseCollisionInstance * pInstance = GetCollisionInstanceData(col);
			pInstance->Render();
		}
	}

	SHADERMANAGER.SetPipelineState(PSTATE_DEPTHENABLE, FALSE);
	s_Screen.SetColorOperation();
	s_Screen.SetDiffuseColor(1.0f, 0.0f, 0.0f);
	TCollisionPointInstanceList::iterator itor;
	/*itor = m_AttackingPointInstanceList.begin();
	for (; itor != m_AttackingPointInstanceList.end(); ++itor)
	{
		const TCollisionPointInstance & c_rInstance = *itor;
		for (DWORD i = 0; i < c_rInstance.SphereInstanceVector.size(); ++i)
		{
			const CDynamicSphereInstance & c_rSphereInstance = c_rInstance.SphereInstanceVector[i];
			s_Screen.RenderCircle3d(c_rSphereInstance.v3Position.x,
									c_rSphereInstance.v3Position.y,
									c_rSphereInstance.v3Position.z,
									c_rSphereInstance.fRadius);
		}
	}*/
	s_Screen.SetDiffuseColor(1.0f, (isShow())?1.0f:0.0f, 0.0f);
	Vector3 center;
	float r;
	GetBoundingSphere(center,r);
	s_Screen.RenderCircle3d(center.x,center.y,center.z,r);

	s_Screen.SetDiffuseColor(0.0f, 0.0f, 1.0f);
	itor = m_DefendingPointInstanceList.begin();
	for (; itor != m_DefendingPointInstanceList.end(); ++itor)
	{
		const TCollisionPointInstance & c_rInstance = *itor;
		for (DWORD i = 0; i < c_rInstance.SphereInstanceVector.size(); ++i)
		{
			const CDynamicSphereInstance & c_rSphereInstance = c_rInstance.SphereInstanceVector[i];
			s_Screen.RenderCircle3d(c_rSphereInstance.v3Position.x,
									c_rSphereInstance.v3Position.y,
									c_rSphereInstance.v3Position.z,
									c_rSphereInstance.fRadius);
		}
	}

	s_Screen.SetDiffuseColor(0.0f, 1.0f, 0.0f);
	itor = m_BodyPointInstanceList.begin();
	for (; itor != m_BodyPointInstanceList.end(); ++itor)
	{
		const TCollisionPointInstance & c_rInstance = *itor;
		for (DWORD i = 0; i < c_rInstance.SphereInstanceVector.size(); ++i)
		{
			const CDynamicSphereInstance & c_rSphereInstance = c_rInstance.SphereInstanceVector[i];
			s_Screen.RenderCircle3d(c_rSphereInstance.v3Position.x,
									c_rSphereInstance.v3Position.y,
									c_rSphereInstance.v3Position.z,
									c_rSphereInstance.fRadius);
		}
	}

	s_Screen.SetDiffuseColor(1.0f, 0.0f, 0.0f);
//	if (m_SplashArea.fDisappearingTime > GetLocalTime())
	{
		CDynamicSphereInstanceVector::iterator itor = m_kSplashArea.SphereInstanceVector.begin();
		for (; itor != m_kSplashArea.SphereInstanceVector.end(); ++itor)
		{
			const CDynamicSphereInstance & c_rInstance = *itor;
			s_Screen.RenderCircle3d(c_rInstance.v3Position.x,
									c_rInstance.v3Position.y,
									c_rInstance.v3Position.z,
									c_rInstance.fRadius);
		}
	}

	SHADERMANAGER.SetPipelineState(PSTATE_DEPTHENABLE, TRUE);
	SHADERMANAGER.RestorePipelineState(PSTATE_CULLMODE);
	SHADERMANAGER.SetLightingEnabled(true);
}

void CActorInstance::RenderToShadowMap()
{
	if (RENDER_MODE_BLEND == m_iRenderMode)
	if (GetAlphaValue() < 0.5f)
		return;

	CGraphicThingInstance::RenderToShadowMap();

	if (m_pkHorse)
		m_pkHorse->RenderToShadowMap();
}

#ifdef ENABLE_RENDER_MODE_GROUPING
void CActorInstance::BeginBatchRender()
{
}

void CActorInstance::EndBatchRender()
{
	// States will be overwritten by next pass
}

void CActorInstance::RenderWithOneTexture()
{
	CGraphicThingInstance::RenderWithOneTexture();
}

void CActorInstance::BlendRenderWithOneTexture()
{
	CGraphicThingInstance::BlendRenderWithOneTexture();
}
#endif
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
