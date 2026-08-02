#include "stdafx.h"
#include "../eterLib/ShaderManager.h"
#include "../eterLib/Camera.h"

#include "FlyingData.h"
#include "FlyTrace.h"

#include "../EterBase/StepTimer.h"

CDynamicPool<CFlyTrace>		CFlyTrace::ms_kPool;

void CFlyTrace::DestroySystem()
{
	ms_kPool.Destroy();
}

CFlyTrace* CFlyTrace::New()
{
	return ms_kPool.Alloc();
}

void CFlyTrace::Delete(CFlyTrace* pkInst)
{
	pkInst->Destroy();
	ms_kPool.Free(pkInst);
}

CFlyTrace::CFlyTrace()
{
	__Initialize();

	/*
	// Code for texture
	CGraphicImage * pImage = (CGraphicImage *)CResourceManager::Instance().GetResourcePointer("d:/ray.jpg");
	m_ImageInstance.SetImagePointer(pImage);

	CGraphicTexture * pTexture = m_ImageInstance.GetTexturePointer();
	m_lpTexture = pTexture->GetD3DTexture();
	*/
}

CFlyTrace::~CFlyTrace()
{
	Destroy();
}

void CFlyTrace::__Initialize()
{
	m_bRectShape=false;
	m_dwColor=0;
	m_fSize=0.0f;
	m_fTailLength=0.0f;
}

void CFlyTrace::Destroy()
{
	m_TimePositionDeque.clear();

	__Initialize();
}

void CFlyTrace::UpdateNewPosition(const Vector3 & v3Position)
{
	m_TimePositionDeque.push_front(TTimePosition(DX::StepTimer::Instance().GetTotalSeconds(), v3Position));
	while (!m_TimePositionDeque.empty() && m_TimePositionDeque.back().first + m_fTailLength < DX::StepTimer::Instance().GetTotalSeconds())
	{
		m_TimePositionDeque.pop_back();
	}
}

void CFlyTrace::Create(const CFlyingData::TFlyingAttachData & rFlyingAttachData)
{
	//assert(rFlyingAttachData.bHasTail);
	m_dwColor = rFlyingAttachData.dwTailColor;
	m_fTailLength = rFlyingAttachData.fTailLength;
	m_fSize = rFlyingAttachData.fTailSize;
	m_bRectShape = rFlyingAttachData.bRectShape;
}

void CFlyTrace::Update()
{
}

struct TFlyVertex
{
	Vector3 p;
	DWORD c;
	Vector2 t;
	TFlyVertex(){};
	TFlyVertex(const Vector3& p, DWORD c, const Vector2 & t):p(p),c(c),t(t){}
};

static std::vector<FlyTraceSegmentInput> s_csSegmentBuffer;

void CFlyTrace::Render()
{
	if (m_TimePositionDeque.size() <= 1)
		return;

	SHADERMANAGER.SavePipelineState(PSTATE_DEPTHFUNC, COMPARISON_LESS);

	Matrix matWorld;
	MatrixIdentity(&matWorld);

	SHADERMANAGER.SaveTransform(MATRIX_WORLD, &matWorld);
	SHADERMANAGER.SaveInputLayout(INPUT_LAYOUT_PDT);
	SHADERMANAGER.SavePipelineState(PSTATE_CULLMODE, CULL_NONE);

	SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, TRUE);
	SHADERMANAGER.SavePipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
	SHADERMANAGER.SavePipelineState(PSTATE_DESTBLEND, BLEND_ONE);

	bool bSavedAlphaTest = SHADERMANAGER.GetAlphaTestEnabled();
	SHADERMANAGER.SetAlphaTestEnabled(true);

	SHADERMANAGER.SavePipelineState(PSTATE_BLENDOP, BLENDOP_ADD);

	SHADERMANAGER.SetLightingEnabled(false);
	SHADERMANAGER.SetDefaultTexture(0);
	SHADERMANAGER.SetShaderResource(1, NULL);

	CScreen s;
	s.UpdateViewMatrix();
	CCamera* pCurrentCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCurrentCamera)
		return;

	Frustum& frustum = s.GetFrustum();

	if (SHADERMANAGER.IsFlyTraceCSAvailable())
	{
		s_csSegmentBuffer.clear();

		TTimePositionDeque::iterator it1, it2;
		it2 = it1 = m_TimePositionDeque.begin();
		++it2;
		for (; it2 != m_TimePositionDeque.end(); ++it2, ++it1)
		{
			const Vector3& rkOld = it1->second;
			const Vector3& rkNew = it2->second;
			Vector3 B = rkNew - rkOld;

			float radius = max(fabs(B.x), max(fabs(B.y), fabs(B.z))) / 2;
			Vector3d c(rkOld.x + B.x * 0.5f, rkOld.y + B.y * 0.5f, rkOld.z + B.z * 0.5f);
			if (frustum.ViewVolumeTest(c, radius) == VS_OUTSIDE)
				continue;

			float rate1 = (1 - (DX::StepTimer::Instance().GetTotalSeconds() - it1->first) / m_fTailLength);
			float rate2 = (1 - (DX::StepTimer::Instance().GetTotalSeconds() - it2->first) / m_fTailLength);
			float size1 = m_fSize;
			float size2 = m_fSize;
			if (!m_bRectShape)
			{
				size1 *= rate1;
				size2 *= rate2;
			}

			FlyTraceSegmentInput seg;
			seg.pos1X = rkOld.x; seg.pos1Y = rkOld.y; seg.pos1Z = rkOld.z;
			seg.size1 = size1;
			seg.pos2X = rkNew.x; seg.pos2Y = rkNew.y; seg.pos2Z = rkNew.z;
			seg.size2 = size2;
			seg.color = m_dwColor;
			seg._pad[0] = seg._pad[1] = seg._pad[2] = 0;
			s_csSegmentBuffer.push_back(seg);
		}

		if (!s_csSegmentBuffer.empty())
		{
			UINT segCount = (UINT)s_csSegmentBuffer.size();

			SHADERMANAGER.SetTextureFactor(0xFFFFFFFF);
			SHADERMANAGER.BeginParticlePCT();

			if (SHADERMANAGER.DispatchFlyTraceCS(s_csSegmentBuffer.data(), segCount))
			{
				SHADERMANAGER.DrawFlyTraceCSOutput(segCount);
			}
		}
	}
	else
	{
		Vector3 F(pCurrentCamera->GetView());
		Matrix m;
		MatrixIdentity(&m);
		m._31 = F.x;
		m._32 = F.y;
		m._33 = F.z;

		std::vector<TFlyVertex> batchedVertices;

		TTimePositionDeque::iterator it1, it2;
		it2 = it1 = m_TimePositionDeque.begin();
		++it2;
		for (; it2 != m_TimePositionDeque.end(); ++it2, ++it1)
		{
			const Vector3& rkOld = it1->second;
			const Vector3& rkNew = it2->second;
			Vector3 B = rkNew - rkOld;

			float radius = max(fabs(B.x), max(fabs(B.y), fabs(B.z))) / 2;
			Vector3d c(rkOld.x + B.x * 0.5f, rkOld.y + B.y * 0.5f, rkOld.z + B.z * 0.5f);
			if (frustum.ViewVolumeTest(c, radius) == VS_OUTSIDE)
				continue;

			float rate1 = (1 - (DX::StepTimer::Instance().GetTotalSeconds() - it1->first) / m_fTailLength);
			float rate2 = (1 - (DX::StepTimer::Instance().GetTotalSeconds() - it2->first) / m_fTailLength);
			float size1 = m_fSize;
			float size2 = m_fSize;
			if (!m_bRectShape)
			{
				size1 *= rate1;
				size2 *= rate2;
			}

			TFlyVertex v[6] =
			{
				TFlyVertex(Vector3(0.0f, size1, 0.0f),  m_dwColor, Vector2(0.0f, 0.0f)),
				TFlyVertex(Vector3(-size1, 0.0f, 0.0f), m_dwColor, Vector2(0.0f, 0.5f)),
				TFlyVertex(Vector3(size1, 0.0f, 0.0f),  m_dwColor, Vector2(0.5f, 0.0f)),
				TFlyVertex(Vector3(-size2, 0.0f, 0.0f), m_dwColor, Vector2(0.5f, 1.0f)),
				TFlyVertex(Vector3(size2, 0.0f, 0.0f),  m_dwColor, Vector2(1.0f, 0.5f)),
				TFlyVertex(Vector3(0.0f, -size2, 0.0f), m_dwColor, Vector2(1.0f, 1.0f)),
			};

			Vector3 E = pCurrentCamera->GetEye();
			E -= it1->second;

			Vector3 P;
			Vec3Cross(&P, &B, &E);
			Vector3 U;
			Vec3Cross(&U, &F, &P);
			Vec3Normalize(&U, &U);
			Vector3 R;
			Vec3Cross(&R, &F, &U);

			m._21 = U.x; m._22 = U.y; m._23 = U.z;
			m._11 = R.x; m._12 = R.y; m._13 = R.z;

			int i;
			for (i = 0; i < 6; i++)
				Vec3TransformNormal(&v[i].p, &v[i].p, &m);
			for (i = 0; i < 3; i++)
				v[i].p += it1->second;
			for (; i < 6; i++)
				v[i].p += it2->second;

			// Convert triangle strip to triangle list directly (no sort needed — additive blend)
			batchedVertices.push_back(v[0]); batchedVertices.push_back(v[1]); batchedVertices.push_back(v[2]);
			batchedVertices.push_back(v[2]); batchedVertices.push_back(v[1]); batchedVertices.push_back(v[3]);
			batchedVertices.push_back(v[2]); batchedVertices.push_back(v[3]); batchedVertices.push_back(v[4]);
			batchedVertices.push_back(v[4]); batchedVertices.push_back(v[3]); batchedVertices.push_back(v[5]);
		}

		if (!batchedVertices.empty())
		{
			SHADERMANAGER.SetTextureFactor(0xFFFFFFFF);
			SHADERMANAGER.BeginParticlePCT();

			SHADERMANAGER.DrawDynamic(TOPOLOGY_TRIANGLELIST,
				(UINT)(batchedVertices.size() / 3),
				&batchedVertices[0],
				sizeof(TFlyVertex));
		}
	}

	SHADERMANAGER.RestorePipelineState(PSTATE_DESTBLEND);
	SHADERMANAGER.RestorePipelineState(PSTATE_SRCBLEND);
	SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE);
	SHADERMANAGER.RestorePipelineState(PSTATE_CULLMODE);
	SHADERMANAGER.RestoreInputLayout();
	SHADERMANAGER.RestoreTransform(MATRIX_WORLD);
	SHADERMANAGER.RestorePipelineState(PSTATE_DEPTHFUNC);
	SHADERMANAGER.RestorePipelineState(PSTATE_BLENDOP);

	SHADERMANAGER.SetAlphaTestEnabled(bSavedAlphaTest);
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
