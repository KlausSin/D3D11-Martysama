#include "StdAfx.h"
#include "../eterLib/ShaderManager.h"
#include "../eterLib/ResourceManager.h"
#include "../eterLib/ShaderInit.h"
#include "../eterLib/Camera.h"

#include "MapOutdoor.h"
#include "TerrainPatch.h"

#include "../EterBase/StepTimer.h"

void CMapOutdoor::LoadWaterTexture()
{
	UnloadWaterTexture();
	char buf[256];
	for (int i = 0; i < 30; ++i)
	{
		sprintf(buf, "d:/ymir Work/special/water/%02d.dds", i+1);
		m_WaterInstances[i].SetImagePointer((CGraphicImage *) CResourceManager::Instance().GetResourcePointer(buf));
	}
}

void CMapOutdoor::UnloadWaterTexture()
{
	for (int i = 0; i < 30; ++i)
		m_WaterInstances[i].Destroy();
}

void CMapOutdoor::RenderWater()
{
	if (m_PatchVector.empty() || !IsVisiblePart(PART_WATER))
		return;

	SHADERMANAGER.PushState();

	Matrix matTexTransformWater;

	SHADERMANAGER.SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, TRUE);
	SHADERMANAGER.SetPipelineState(PSTATE_CULLMODE, CULL_NONE);

	const DWORD dwWaterTime = (DWORD)DX::StepTimer::instance().GetTotalMillieSeconds();
	CGraphicTexture* pWaterTexture = m_WaterInstances[(dwWaterTime / 70) % 30].GetTexturePointer();
	if (pWaterTexture)
		SHADERMANAGER.SetShaderResource(0, pWaterTexture->GetD3DTexture());

	MatrixScaling(&matTexTransformWater, m_fWaterTexCoordBase, -m_fWaterTexCoordBase, 0.0f);
	SHADERMANAGER.SetMatrix(MATRIX_TEXTURE0, &matTexTransformWater);

	SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_PD);
	SHADERMANAGER.SetSamplerState(0, SAMPLER_MINFILTER, FILTER_LINEAR);
	SHADERMANAGER.SetSamplerState(0, SAMPLER_MAGFILTER, FILTER_LINEAR);
	SHADERMANAGER.SetSamplerState(0, SAMPLER_MIPFILTER, FILTER_LINEAR);
	SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSU, ADDRESS_WRAP);
	SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSV, ADDRESS_WRAP);
	SHADERMANAGER.SetShaderResource(1, nullptr);

	SHADERMANAGER.BeginWater();

	static float s_fWaterHeightCurrent = 0.0f;
	static float s_fWaterHeightBegin = 0.0f;
	static float s_fWaterHeightEnd = 0.0f;
	static DWORD s_dwLastHeightChangeTime = DX::StepTimer::instance().GetTotalMillieSeconds();
	static DWORD s_dwBlendtime = 300;

	DWORD now = DX::StepTimer::instance().GetTotalMillieSeconds();

	if (now - s_dwLastHeightChangeTime > s_dwBlendtime)
	{
		s_dwBlendtime = random_range(1000, 3000);
		s_fWaterHeightEnd = (s_fWaterHeightEnd == 0.0f) ? -random_range(0, 15) : 0.0f;
		s_fWaterHeightBegin = s_fWaterHeightCurrent;
		s_dwLastHeightChangeTime = now;
	}

	s_fWaterHeightCurrent = s_fWaterHeightBegin +
		(s_fWaterHeightEnd - s_fWaterHeightBegin) *
		(float)(now - s_dwLastHeightChangeTime) / (float)s_dwBlendtime;

	m_matWorldForCommonUse._41 = 0.0f;
	m_matWorldForCommonUse._42 = 0.0f;
	m_matWorldForCommonUse._43 = s_fWaterHeightCurrent;

	SHADERMANAGER.SetMatrix(MATRIX_WORLD, &m_matWorldForCommonUse);
	SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
	SHADERMANAGER.CommitChanges();

	for (const auto& patch : m_PatchVector)
		DrawWater(patch.second);

	m_matWorldForCommonUse._43 = 0.0f;

	SHADERMANAGER.End();
	SHADERMANAGER.PopState();
}

void CMapOutdoor::DrawWater(long patchnum)
{
	assert(NULL!=m_pTerrainPatchProxyList);
	if (!m_pTerrainPatchProxyList)
		return;

	CTerrainPatchProxy& rkTerrainPatchProxy = m_pTerrainPatchProxyList[patchnum];

	if (!rkTerrainPatchProxy.isUsed())
		return;

	if (!rkTerrainPatchProxy.isWaterExists())
		return;

	CGraphicVertexBuffer* pkVB=rkTerrainPatchProxy.GetWaterVertexBufferPointer();
	if (!pkVB)
		return;

	if (!pkVB->GetD3DVertexBuffer())
		return;

	UINT uPriCount=rkTerrainPatchProxy.GetWaterFaceCount();
	if (!uPriCount)
		return;

	SHADERMANAGER.SetVertexBuffer(0, pkVB->GetD3DVertexBuffer(), sizeof(SWaterVertex));
	SHADERMANAGER.Draw(TOPOLOGY_TRIANGLELIST, 0, uPriCount);

	ms_faceCount += uPriCount;
}

//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
