#include "StdAfx.h"
#include "DungeonBlock.h"

#include "../eterlib/ShaderManager.h"
#include "../eterlib/ShaderInit.h"

void CDungeonModelInstance::RenderDungeonBlock()
{
	if (IsEmpty())
		return;

	BeginShaderMesh2TexRender();

	SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_PNT2);
	ID3D11Buffer* lpd3dRigidPNTVtxBuf = m_pModel->GetPNTD3DVertexBuffer();
	if (lpd3dRigidPNTVtxBuf)
	{
		SHADERMANAGER.SetVertexBuffer(0, lpd3dRigidPNTVtxBuf, sizeof(TPNT2Vertex));
		RenderMeshNodeListWithTwoTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_BLEND_PNT);
	}

	EndShaderMesh2TexRender();
}

void CDungeonModelInstance::RenderDungeonBlockShadow()
{
	if (IsEmpty())
		return;

	BeginShaderShadowRender();

	if (SHADERMANAGER.IsInitialized())
		SHADERMANAGER.SetTextureFactor(0xffffffff);
	SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, TRUE);
	SHADERMANAGER.SavePipelineState(PSTATE_SRCBLEND, BLEND_ZERO);
	SHADERMANAGER.SavePipelineState(PSTATE_DESTBLEND, BLEND_SRCCOLOR);

	SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_PNT2);
	ID3D11Buffer* lpd3dRigidPNTVtxBuf = m_pModel->GetPNTD3DVertexBuffer();
	if (lpd3dRigidPNTVtxBuf)
	{
		SHADERMANAGER.SetVertexBuffer(0, lpd3dRigidPNTVtxBuf, sizeof(TPNT2Vertex));
		RenderMeshNodeListWithoutTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_BLEND_PNT);
	}

	SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE);
	SHADERMANAGER.RestorePipelineState(PSTATE_SRCBLEND);
	SHADERMANAGER.RestorePipelineState(PSTATE_DESTBLEND);

	EndShaderShadowRender();
}

struct FUpdate
{
	float fElapsedTime;
	Matrix * pmatWorld;
	void operator() (CGrannyModelInstance * pInstance)
	{
		pInstance->Update(CGrannyModelInstance::ANIFPS_MIN);
		pInstance->UpdateLocalTime(fElapsedTime);
		pInstance->Deform(pmatWorld);
	}
};

void CDungeonBlock::Update()
{
	Transform();

	FUpdate Update;
	Update.fElapsedTime = 0.0f;
	Update.pmatWorld = &m_worldMatrix;
	for_each(m_ModelInstanceContainer.begin(), m_ModelInstanceContainer.end(), Update);
}

struct FRender
{
	void operator() (CDungeonModelInstance * pInstance)
	{
		pInstance->RenderDungeonBlock();
	}
};

void CDungeonBlock::Render()
{
//	if (!isShow())
//		return;

	for_each(m_ModelInstanceContainer.begin(), m_ModelInstanceContainer.end(), FRender());
}

struct FRenderShadow
{
	void operator() (CDungeonModelInstance * pInstance)
	{
		pInstance->RenderDungeonBlockShadow();
	}
};

void CDungeonBlock::OnRenderShadow()
{
	for_each(m_ModelInstanceContainer.begin(), m_ModelInstanceContainer.end(), FRenderShadow());
}

struct FBoundBox
{
	Vector3 * m_pv3Min;
	Vector3 * m_pv3Max;

	FBoundBox(Vector3 * pv3Min, Vector3 * pv3Max)
	{
		m_pv3Min = pv3Min;
		m_pv3Max = pv3Max;
	}
	void operator() (CGrannyModelInstance * pInstance)
	{
		pInstance->GetBoundBox(m_pv3Min, m_pv3Max);
	}
};

bool CDungeonBlock::GetBoundingSphere(Vector3 & v3Center, float & fRadius)
{
	v3Center = m_v3Center;
	fRadius = m_fRadius;
	Vec3TransformCoord(&v3Center, &v3Center, &GetMatrix());
	return true;
}

void CDungeonBlock::OnUpdateCollisionData(const CStaticCollisionDataVector * pscdVector)
{
	assert(pscdVector);
	CStaticCollisionDataVector::const_iterator it;
	for(it = pscdVector->begin();it!=pscdVector->end();++it)
	{
		AddCollision(&(*it),&GetMatrix());
	}
}

void CDungeonBlock::OnUpdateHeighInstance(CAttributeInstance * pAttributeInstance)
{
	assert(pAttributeInstance);
	SetHeightInstance(pAttributeInstance);
}

bool CDungeonBlock::OnGetObjectHeight(float fX, float fY, float * pfHeight)
{
	if (m_pHeightAttributeInstance && m_pHeightAttributeInstance->GetHeight(fX, fY, pfHeight))
		return true;
	return false;
}

void CDungeonBlock::BuildBoundingSphere()
{
	Vector3 v3Min, v3Max;
	for_each(m_ModelInstanceContainer.begin(), m_ModelInstanceContainer.end(), FBoundBox(&v3Min, &v3Max));

	m_v3Center = (v3Min+v3Max) * 0.5f;
	auto val = (v3Max-v3Min);
	m_fRadius = Vec3Length(&val)*0.5f + 150.0f; // extra length for attached objects
}

bool CDungeonBlock::Intersect(float * pfu, float * pfv, float * pft)
{
	TModelInstanceContainer::iterator itor = m_ModelInstanceContainer.begin();
	for (; itor != m_ModelInstanceContainer.end(); ++itor)
	{
		CDungeonModelInstance * pInstance = *itor;
		if (pInstance->Intersect(&CGraphicObjectInstance::GetMatrix(), pfu, pfv, pft))
			return true;
	}

	return false;
}

void CDungeonBlock::GetBoundBox(Vector3 * pv3Min, Vector3 * pv3Max)
{
	pv3Min->x = +10000000.0f;
	pv3Min->y = +10000000.0f;
	pv3Min->z = +10000000.0f;
	pv3Max->x = -10000000.0f;
	pv3Max->y = -10000000.0f;
	pv3Max->z = -10000000.0f;

	TModelInstanceContainer::iterator itor = m_ModelInstanceContainer.begin();
	for (; itor != m_ModelInstanceContainer.end(); ++itor)
	{
		CDungeonModelInstance * pInstance = *itor;

		Vector3 v3Min;
		Vector3 v3Max;
		pInstance->GetBoundBox(&v3Min, &v3Max);

		pv3Min->x = min(v3Min.x, pv3Min->x);
		pv3Min->y = min(v3Min.x, pv3Min->y);
		pv3Min->z = min(v3Min.x, pv3Min->z);
		pv3Max->x = max(v3Max.x, pv3Max->x);
		pv3Max->y = max(v3Max.x, pv3Max->y);
		pv3Max->z = max(v3Max.x, pv3Max->z);
	}
}

bool CDungeonBlock::Load(const char * c_szFileName)
{
	Destroy();

	m_pThing = (CGraphicThing *)CResourceManager::Instance().GetResourcePointer(c_szFileName);

	m_pThing->AddReference();
	if (m_pThing->GetModelCount() <= 0)
	{
		TraceError("CDungeonBlock::Load(filename=%s) - model count is %d\n", c_szFileName, m_pThing->GetModelCount());
		return false;
	}

	m_ModelInstanceContainer.reserve(m_pThing->GetModelCount());

	for (int i = 0; i < m_pThing->GetModelCount(); ++i)
	{
		CDungeonModelInstance * pModelInstance = new CDungeonModelInstance;
		pModelInstance->SetMainModelPointer(m_pThing->GetModelPointer(i), &m_kDeformableVertexBuffer);
		DWORD dwVertexCount = pModelInstance->GetVertexCount();
		m_kDeformableVertexBuffer.Destroy();
		m_kDeformableVertexBuffer.Create(
			dwVertexCount,
			INPUT_LAYOUT_PNT,
			D3D11_USAGE_DEFAULT);
		m_ModelInstanceContainer.push_back(pModelInstance);
	}

	return true;
}

void CDungeonBlock::__Initialize()
{
	m_v3Center = Vector3(0.0f, 0.0f, 0.0f);
	m_fRadius = 0.0f;

	m_pThing = NULL;
}

void CDungeonBlock::Destroy()
{
	if (m_pThing)
	{
		m_pThing->Release();
		m_pThing = NULL;
	}

	stl_wipe(m_ModelInstanceContainer);

	__Initialize();
}

CDungeonBlock::CDungeonBlock()
{
	__Initialize();
}
CDungeonBlock::~CDungeonBlock()
{
	Destroy();
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
