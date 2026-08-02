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
	if (m_PatchVector.empty())
		return;

	if (!IsVisiblePart(PART_WATER))
		return;

	//////////////////////////////////////////////////////////////////////////
	// RenderState
	Matrix matTexTransformWater;

	SHADERMANAGER.SavePipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, TRUE);
	SHADERMANAGER.SavePipelineState(PSTATE_CULLMODE, CULL_NONE);

	const DWORD dwWaterTime = (DWORD)DX::StepTimer::instance().GetTotalMillieSeconds();
	CGraphicTexture * pWaterTexture = m_WaterInstances[(dwWaterTime / 70) % 30].GetTexturePointer();
	if (pWaterTexture)
		SHADERMANAGER.SetShaderResource(0, pWaterTexture->GetD3DTexture());

	MatrixScaling(&matTexTransformWater, m_fWaterTexCoordBase, -m_fWaterTexCoordBase, 0.0f);
	SHADERMANAGER.SaveTransform(MATRIX_TEXTURE0, &matTexTransformWater);

	SHADERMANAGER.SaveInputLayout(INPUT_LAYOUT_PD);

	SHADERMANAGER.SaveSamplerState(0, SAMPLER_MINFILTER, FILTER_LINEAR);
	SHADERMANAGER.SaveSamplerState(0, SAMPLER_MAGFILTER, FILTER_LINEAR);
	SHADERMANAGER.SaveSamplerState(0, SAMPLER_MIPFILTER, FILTER_LINEAR);
	SHADERMANAGER.SaveSamplerState(0, SAMPLER_ADDRESSU, ADDRESS_WRAP);
	SHADERMANAGER.SaveSamplerState(0, SAMPLER_ADDRESSV, ADDRESS_WRAP);


	SHADERMANAGER.SetShaderResource(1,NULL);

	SHADERMANAGER.BeginWater();


	// RenderState
	//////////////////////////////////////////////////////////////////////////

	static float s_fWaterHeightCurrent = 0;
	static float s_fWaterHeightBegin = 0;
	static float s_fWaterHeightEnd = 0;
	static DWORD s_dwLastHeightChangeTime = DX::StepTimer::instance().GetTotalMillieSeconds();
	static DWORD s_dwBlendtime = 300;

	if ((DX::StepTimer::instance().GetTotalMillieSeconds() - s_dwLastHeightChangeTime) > s_dwBlendtime)
	{
		s_dwBlendtime = random_range(1000, 3000);

		if (s_fWaterHeightEnd == 0)
			s_fWaterHeightEnd = -random_range(0, 15);
		else
			s_fWaterHeightEnd = 0;

		s_fWaterHeightBegin = s_fWaterHeightCurrent;
		s_dwLastHeightChangeTime = DX::StepTimer::instance().GetTotalMillieSeconds();
	}

	s_fWaterHeightCurrent = s_fWaterHeightBegin + (s_fWaterHeightEnd - s_fWaterHeightBegin) * (float)((DX::StepTimer::instance().GetTotalMillieSeconds() - s_dwLastHeightChangeTime) / (float)s_dwBlendtime);
	m_matWorldForCommonUse._43 = s_fWaterHeightCurrent;

	m_matWorldForCommonUse._41 = 0.0f;
	m_matWorldForCommonUse._42 = 0.0f;
	SHADERMANAGER.SetMatrix(MATRIX_WORLD, &m_matWorldForCommonUse);

	SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
	SHADERMANAGER.CommitChanges();

	std::vector<std::pair<float, long> >::iterator i;
	for (i = m_PatchVector.begin(); i != m_PatchVector.end(); ++i)
	{
		DrawWater(i->second);
	}

	m_matWorldForCommonUse._43 = 0.0f;


	SHADERMANAGER.End();

	//////////////////////////////////////////////////////////////////////////
	// RenderState
	SHADERMANAGER.RestoreInputLayout();
	SHADERMANAGER.RestoreTransform(MATRIX_TEXTURE0);
	SHADERMANAGER.RestoreSamplerState(0, SAMPLER_MINFILTER);
	SHADERMANAGER.RestoreSamplerState(0, SAMPLER_MAGFILTER);
	SHADERMANAGER.RestoreSamplerState(0, SAMPLER_MIPFILTER);
	SHADERMANAGER.RestoreSamplerState(0, SAMPLER_ADDRESSU);
	SHADERMANAGER.RestoreSamplerState(0, SAMPLER_ADDRESSV);

	SHADERMANAGER.RestorePipelineState(PSTATE_DEPTHWRITEMASK);
	SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE);
	SHADERMANAGER.RestorePipelineState(PSTATE_CULLMODE);
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
