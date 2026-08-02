#include "StdAfx.h"
#include "../eterLib/ShaderManager.h"
#include "../eterLib/ResourceManager.h"
#include "EffectMeshInstance.h"
#include "../eterlib/GrpMath.h"

#include "../EterBase/StepTimer.h"

CDynamicPool<CEffectMeshInstance>		CEffectMeshInstance::ms_kPool;

void CEffectMeshInstance::DestroySystem()
{
	ms_kPool.Destroy();
}

CEffectMeshInstance* CEffectMeshInstance::New()
{
	return ms_kPool.Alloc();
}

void CEffectMeshInstance::Delete(CEffectMeshInstance* pkMeshInstance)
{
	pkMeshInstance->Destroy();
	ms_kPool.Free(pkMeshInstance);
}

BOOL CEffectMeshInstance::isActive()
{
	if (!CEffectElementBaseInstance::isActive())
		return FALSE;

	if (!m_MeshFrameController.isActive())
		return FALSE;

	for (DWORD j = 0; j < m_TextureInstanceVector.size(); ++j)
	{
		int iCurrentFrame = m_MeshFrameController.GetCurrentFrame();
		if (m_TextureInstanceVector[j].TextureFrameController.isActive(iCurrentFrame))
			return TRUE;
	}

	return FALSE;
}

bool CEffectMeshInstance::OnUpdate(float fElapsedTime)
{
	if (!isActive())
		return false;

	if (m_MeshFrameController.isActive())
		m_MeshFrameController.Update(fElapsedTime);

	for (DWORD j = 0; j < m_TextureInstanceVector.size(); ++j)
	{
		int iCurrentFrame = m_MeshFrameController.GetCurrentFrame();
		if (m_TextureInstanceVector[j].TextureFrameController.isActive(iCurrentFrame))
			m_TextureInstanceVector[j].TextureFrameController.Update(fElapsedTime);
	}

	return true;
}

void CEffectMeshInstance::OnRender()
{
	if (!isActive())
		return;

	CEffectMesh * pEffectMesh = m_roMesh.GetPointer();
	if (!pEffectMesh)
		return;

	for (DWORD i = 0; i < pEffectMesh->GetMeshCount(); ++i)
	{
		assert(i < m_TextureInstanceVector.size());

		CFrameController & rTextureFrameController = m_TextureInstanceVector[i].TextureFrameController;
		if (!rTextureFrameController.isActive(m_MeshFrameController.GetCurrentFrame()))
			continue;

		int iBillboardType = m_pMeshScript->GetBillboardType(i);

		Matrix m_matWorld;
		MatrixIdentity(&m_matWorld);

		switch(iBillboardType)
		{
			case MESH_BILLBOARD_TYPE_ALL:
				{
					Matrix matTemp;
					MatrixRotationX(&matTemp, XM_PIDIV2);  // 90 degrees in radians (π/2)
					MatrixInverse(&m_matWorld, NULL, &CScreen::GetViewMatrix());

					m_matWorld = matTemp * m_matWorld;
				}
				break;

			case MESH_BILLBOARD_TYPE_Y:
				{
					Matrix matTemp;
					MatrixIdentity(&matTemp);

					MatrixInverse(&matTemp, NULL, &CScreen::GetViewMatrix());
					m_matWorld._11 = matTemp._11;
					m_matWorld._12 = matTemp._12;
					m_matWorld._21 = matTemp._21;
					m_matWorld._22 = matTemp._22;
				}
				break;

			case MESH_BILLBOARD_TYPE_MOVE:
				{
					Vector3 Position;
					m_pMeshScript->GetPosition(m_fLocalTime, Position);
					Vector3 LastPosition;
					m_pMeshScript->GetPosition(m_fLocalTime - DX::StepTimer::Instance().GetElapsedSeconds(), LastPosition);
					Position -= LastPosition;
					if (Vec3LengthSq(&Position)>0.001f)
					{
						Vec3Normalize(&Position,&Position);
						Quaternion q = SafeRotationNormalizedArc(Vector3(0.0f,-1.0f,0.0f),Position);
						MatrixRotationQuaternion(&m_matWorld,&q);
					}
				}
				break;
		}

		if (!m_pMeshScript->isBlendingEnable(i))
		{
			SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, FALSE);
		}
		else
		{
			int iBlendingSrcType = m_pMeshScript->GetBlendingSrcType(i);
			int iBlendingDestType = m_pMeshScript->GetBlendingDestType(i);
			SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, TRUE);
			SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND, iBlendingSrcType);
			SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, iBlendingDestType);
		}

		Vector3 Position;
		m_pMeshScript->GetPosition(m_fLocalTime, Position);
		m_matWorld._41 = Position.x;
		m_matWorld._42 = Position.y;
		m_matWorld._43 = Position.z;
		m_matWorld = m_matWorld * *mc_pmatLocal;
		SHADERMANAGER.SetMatrix(MATRIX_WORLD, &m_matWorld);

		BYTE byType = TEXOP_MODULATE;  // Default to modulate
		Color Color(1.0f, 1.0f, 1.0f, 1.0f);
		m_pMeshScript->GetColorOperationType(i, &byType);
		SHADERMANAGER.SetParticleColorOp(byType);
		m_pMeshScript->GetColorFactor(i, &Color);

		TTimeEventTableFloat * TableAlpha;

		float fAlpha = 1.0f;
		if (m_pMeshScript->GetTimeTableAlphaPointer(i, &TableAlpha) && !TableAlpha->empty())
			GetTimeEventBlendValue(m_fLocalTime,*TableAlpha, &fAlpha);

		// Render
		CEffectMesh::TEffectMeshData * pMeshData = pEffectMesh->GetMeshDataPointer(i);

		assert(m_MeshFrameController.GetCurrentFrame() < pMeshData->EffectFrameDataVector.size());
		CEffectMesh::TEffectFrameData & rFrameData = pMeshData->EffectFrameDataVector[m_MeshFrameController.GetCurrentFrame()];

		float fFinalAlpha = fAlpha * rFrameData.fVisibility;
		if (fFinalAlpha < 0.004f)  // ~1/255, essentially invisible
			continue;

		DWORD dwcurTextureFrame = rTextureFrameController.GetCurrentFrame();
		if (dwcurTextureFrame < m_TextureInstanceVector[i].TextureInstanceVector.size())
		{
			CGraphicImageInstance * pImageInstance = m_TextureInstanceVector[i].TextureInstanceVector[dwcurTextureFrame];
			if (pImageInstance && pImageInstance->GetTexturePointer())
				SHADERMANAGER.SetShaderResource(0, pImageInstance->GetTexturePointer()->GetD3DTexture());
		}

		int iBlendingSrcType = m_pMeshScript->GetBlendingSrcType(i);
		if (iBlendingSrcType == BLEND_ONE)
		{
			Color.r *= fFinalAlpha;
			Color.g *= fFinalAlpha;
			Color.b *= fFinalAlpha;
		}
		Color.a = fFinalAlpha;
		if (SHADERMANAGER.IsInitialized())
			SHADERMANAGER.SetTextureFactor(DWORD(Color));

		// Bind particle shader before rendering
		SHADERMANAGER.BeginParticle();
		SHADERMANAGER.SetTextureColorSwap(false);

		if (rFrameData.dwIndexCount) // @fixme027
		{
			SHADERMANAGER.DrawDynamic(TOPOLOGY_TRIANGLELIST,
									 rFrameData.dwIndexCount/3,
									 &rFrameData.PDTVertexVector[0],
									 sizeof(TPTVertex));
			SHADERMANAGER.StatsNoteMeshElem();
		}
	}
}

void CEffectMeshInstance::OnSetDataPointer(CEffectElementBase * pElement)
{
	CEffectMeshScript * pMesh = (CEffectMeshScript *)pElement;
	m_pMeshScript = pMesh;

	const char * c_szMeshFileName = pMesh->GetMeshFileName();

	m_pEffectMesh = (CEffectMesh *) CResourceManager::Instance().GetResourcePointer(c_szMeshFileName);

	if (!m_pEffectMesh)
		return;

	m_roMesh.SetPointer(m_pEffectMesh);

	m_MeshFrameController.Clear();
	m_MeshFrameController.SetMaxFrame(m_roMesh.GetPointer()->GetFrameCount());
	m_MeshFrameController.SetFrameTime(pMesh->GetMeshAnimationFrameDelay());
	m_MeshFrameController.SetLoopFlag(pMesh->isMeshAnimationLoop());
	m_MeshFrameController.SetLoopCount(pMesh->GetMeshAnimationLoopCount());
	m_MeshFrameController.SetStartFrame(0);

	m_TextureInstanceVector.clear();
	m_TextureInstanceVector.resize(m_pEffectMesh->GetMeshCount());
	for (DWORD j = 0; j < m_TextureInstanceVector.size(); ++j)
	{
		CEffectMeshScript::TMeshData * pMeshData;
		if (!m_pMeshScript->GetMeshDataPointer(j, &pMeshData))
			continue;

		CEffectMesh* pkEftMesh=m_roMesh.GetPointer();

		if (!pkEftMesh)
			continue;

		std::vector<CGraphicImage*>* pTextureVector = pkEftMesh->GetTextureVectorPointer(j);
		if (!pTextureVector)
			continue;

		std::vector<CGraphicImage*>& rTextureVector = *pTextureVector;

		CFrameController & rFrameController = m_TextureInstanceVector[j].TextureFrameController;
		rFrameController.Clear();
		rFrameController.SetMaxFrame((DWORD)(rTextureVector.size()));
		rFrameController.SetFrameTime(pMeshData->fTextureAnimationFrameDelay);
		rFrameController.SetLoopFlag(pMeshData->bTextureAnimationLoopEnable);
		rFrameController.SetStartFrame(pMeshData->dwTextureAnimationStartFrame);

		std::vector<CGraphicImageInstance*> & rImageInstanceVector = m_TextureInstanceVector[j].TextureInstanceVector;
		rImageInstanceVector.clear();
		rImageInstanceVector.reserve(rTextureVector.size());
		for (std::vector<CGraphicImage*>::iterator itor = rTextureVector.begin(); itor != rTextureVector.end(); ++itor)
		{
			CGraphicImage * pImage = *itor;
			CGraphicImageInstance * pImageInstance = CGraphicImageInstance::ms_kPool.Alloc();
			pImageInstance->SetImagePointer(pImage);
			rImageInstanceVector.push_back(pImageInstance);
		}
	}
}

void CEffectMeshInstance_DeleteImageInstance(CGraphicImageInstance * pkInstance)
{
	CGraphicImageInstance::ms_kPool.Free(pkInstance);
}

void CEffectMeshInstance_DeleteTextureInstance(CEffectMeshInstance::TTextureInstance & rkInstance)
{
	std::vector<CGraphicImageInstance*> & rVector = rkInstance.TextureInstanceVector;
	for_each(rVector.begin(), rVector.end(), CEffectMeshInstance_DeleteImageInstance);
	rVector.clear();
}

void CEffectMeshInstance::OnInitialize()
{
}

void CEffectMeshInstance::OnDestroy()
{
	for_each(m_TextureInstanceVector.begin(), m_TextureInstanceVector.end(), CEffectMeshInstance_DeleteTextureInstance);
	m_TextureInstanceVector.clear();
	m_roMesh.SetPointer(NULL);
}

CEffectMeshInstance::CEffectMeshInstance()
{
	Initialize();
}

CEffectMeshInstance::~CEffectMeshInstance()
{
	Destroy();
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
