///////////////////////////////////////////////////////////////////////
//	CSpeedTreeForestDX11 Class
//
//	(c) 2003 IDV, Inc.
//
//	This class is provided to illustrate one way to incorporate
//	SpeedTreeRT into an OpenGL application.  All of the SpeedTreeRT
//	calls that must be made on a per tree basis are done by this class.
//	Calls that apply to all trees (i.e. static SpeedTreeRT functions)
//	are made in the functions in main.cpp.
//
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization and may
//	not be copied or disclosed except in accordance with the terms of
//	that agreement.
//
//      Copyright (c) 2001-2003 IDV, Inc.
//      All Rights Reserved.
//
//		IDV, Inc.
//		1233 Washington St. Suite 610
//		Columbia, SC 29201
//		Voice: (803) 799-1699
//		Fax:   (803) 931-0320
//		Web:   http://www.idvinc.com

#include "StdAfx.h"

#include <stdio.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include "../eterLib/GrpMathType.h"
#include "../eterLib/GrpMathFunc.h"

#include "../eterBase/Timer.h"
#include "../eterlib/ShaderManager.h"
#include "../eterlib/Camera.h"

#include "SpeedTreeForestDX11.h"
#include "SpeedTreeConfig.h"
#include "VertexShaders.h"

#include "../EterBase/StepTimer.h"
#include "../EterLib/VTFInstanceManager.h"

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeForestDX11::CSpeedTreeForestDX11

CSpeedTreeForestDX11::CSpeedTreeForestDX11()
	: m_pDx(nullptr)
	, m_pBranchInputLayout(nullptr)
	, m_pLeafInputLayout(nullptr)
{
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeForestDX11::~CSpeedTreeForestDX11

CSpeedTreeForestDX11::~CSpeedTreeForestDX11()
{
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeForestDX11::InitVertexShaders
bool CSpeedTreeForestDX11::InitVertexShaders(void)
{
	ID3D11InputLayout* pSpeedTreeLayout = SHADERMANAGER.GetInputLayout(SHADER_SPEEDTREE);

	if (pSpeedTreeLayout)
	{
		m_pBranchInputLayout = pSpeedTreeLayout;
		m_pLeafInputLayout = pSpeedTreeLayout;
		CSpeedTreeWrapper::SetVertexShaders(m_pBranchInputLayout, m_pLeafInputLayout);
		return true;
	}

	// Fallback: input layout not available yet
	TraceError("CSpeedTreeForestDX11::InitVertexShaders - SpeedTree input layout not available");
	return true;
}

bool CSpeedTreeForestDX11::SetRenderingDevice(ID3D11Device* lpDevice)
{
	m_pDx = lpDevice;

	if (!InitVertexShaders())
		return false;

	const float c_afLightPosition[4] = { -0.707f, -0.300f, 0.707f, 0.0f };
	const float	c_afLightAmbient[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
	const float	c_afLightDiffuse[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	const float	c_afLightSpecular[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	float afLight1[] =
	{
		c_afLightPosition[0], c_afLightPosition[1], c_afLightPosition[2],	// pos
		c_afLightDiffuse[0], c_afLightDiffuse[1], c_afLightDiffuse[2],		// diffuse
		c_afLightAmbient[0], c_afLightAmbient[1], c_afLightAmbient[2],		// ambient
		c_afLightSpecular[0], c_afLightSpecular[1], c_afLightSpecular[2],	// specular
		c_afLightPosition[3],												// directional flag
		1.0f, 0.0f, 0.0f													// attenuation (constant, linear, quadratic)
	};

	CSpeedTreeRT::SetNumWindMatrices(c_nNumWindMatrices);

	CSpeedTreeRT::SetLightAttributes(0, afLight1);
	CSpeedTreeRT::SetLightState(0, true);
	return true;
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeForestDX11::UploadWindMatrix

void CSpeedTreeForestDX11::UploadWindMatrix(int nIndex, const float* pMatrix) const
{
	SHADERMANAGER.SetSpeedTreeWindMatrix(nIndex, pMatrix);
}

void CSpeedTreeForestDX11::UpdateCompundMatrix(const Vector3 & c_rEyeVec, const Matrix & c_rmatView, const Matrix & c_rmatProj)
{
    // setup composite matrix for shader
	Matrix matBlend;
	MatrixIdentity(&matBlend);

	Matrix matBlendShader;
	MatrixMultiply(&matBlendShader, &c_rmatView, &c_rmatProj);

	float afDirection[3];
	afDirection[0] = matBlendShader.m[0][2];
	afDirection[1] = matBlendShader.m[1][2];
	afDirection[2] = matBlendShader.m[2][2];
	CSpeedTreeRT::SetCamera(c_rEyeVec, afDirection);

	MatrixTranspose(&matBlendShader, &matBlendShader);
	SHADERMANAGER.SetSpeedTreeCompoundMatrix((const float*)&matBlendShader);
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeForestDX11::Render

void CSpeedTreeForestDX11::Render(unsigned long ulRenderBitVector)
{
	UpdateSystem(DX::StepTimer::instance().GetTotalSeconds());

	if (m_pMainTreeMap.empty())
		return;

	if (!m_pBranchInputLayout || !m_pLeafInputLayout)
	{
		ID3D11InputLayout* pSpeedTreeLayout = SHADERMANAGER.GetInputLayout(SHADER_SPEEDTREE);
		if (pSpeedTreeLayout)
		{
			m_pBranchInputLayout = pSpeedTreeLayout;
			m_pLeafInputLayout = pSpeedTreeLayout;
			CSpeedTreeWrapper::SetVertexShaders(m_pBranchInputLayout, m_pLeafInputLayout);
		}
	}

	if (!(ulRenderBitVector & Forest_RenderToShadow) && !(ulRenderBitVector & Forest_RenderToMiniMap))
	{
		CCamera* pCamera = CCameraManager::Instance().GetCurrentCamera();
		if (!pCamera)
			return;
		UpdateCompundMatrix(pCamera->GetEye(), ms_matView, ms_matProj);
	}


	bool bSavedLightState = SHADERMANAGER.GetLightingEnabled();

#ifdef WRAPPER_USE_DYNAMIC_LIGHTING
	SHADERMANAGER.SetLightingEnabled(true);
#else
	SHADERMANAGER.SetLightingEnabled(false);
#endif

	TTreeMap::iterator itor;
	UINT uiCount;

	itor = m_pMainTreeMap.begin();

	while (itor != m_pMainTreeMap.end())
	{
		CSpeedTreeWrapper * pMainTree = (itor++)->second;
		CSpeedTreeWrapper ** ppInstances = pMainTree->GetInstances(uiCount);

		for (UINT i = 0; i < uiCount; ++i)
		{
			ppInstances[i]->Advance();
		}
	}

	SHADERMANAGER.SetSpeedTreeLight(m_afLighting);
	SHADERMANAGER.SetSpeedTreeFogParams(m_afFog);

	if (!(ulRenderBitVector & Forest_RenderToShadow))
	{
		SHADERMANAGER.SetSamplerState(0, SAMPLER_MINFILTER, FILTER_LINEAR);
		SHADERMANAGER.SetSamplerState(0, SAMPLER_MAGFILTER, FILTER_LINEAR);
		SHADERMANAGER.SetSamplerState(0, SAMPLER_MIPFILTER, FILTER_LINEAR);

		SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSU, ADDRESS_WRAP);
		SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSV, ADDRESS_WRAP);
	}

	bool bSavedAlphaTestEnable = SHADERMANAGER.GetAlphaTestEnabled();
	SHADERMANAGER.SetAlphaTestEnabled(true);
	SHADERMANAGER.SavePipelineState(PSTATE_CULLMODE, CULL_FRONT);

	SHADERMANAGER.BeginSpeedTree();

	SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);

	if (!(ulRenderBitVector & Forest_RenderToShadow))
		SHADERMANAGER.SetTextureFactor(0xFFFFFFFF);

	if (ulRenderBitVector & Forest_RenderBranches)
	{
		itor = m_pMainTreeMap.begin();

		while (itor != m_pMainTreeMap.end())
		{
			CSpeedTreeWrapper * pMainTree = (itor++)->second;
			CSpeedTreeWrapper ** ppInstances = pMainTree->GetInstances(uiCount);

			pMainTree->SetupBranchForTreeType();

			for (UINT i = 0; i < uiCount; ++i)
				if (ppInstances[i]->isShow())
					ppInstances[i]->RenderBranches();
		}
	}

	// set render states
	SHADERMANAGER.SetPipelineState(PSTATE_CULLMODE, CULL_NONE);

	if (ulRenderBitVector & Forest_RenderFronds)
	{
		itor = m_pMainTreeMap.begin();

		while (itor != m_pMainTreeMap.end())
		{
			CSpeedTreeWrapper * pMainTree = (itor++)->second;
			CSpeedTreeWrapper ** ppInstances = pMainTree->GetInstances(uiCount);

			pMainTree->SetupFrondForTreeType();

			for (UINT i = 0; i < uiCount; ++i)
				if (ppInstances[i]->isShow())
					ppInstances[i]->RenderFronds();
		}
	}

	// render leaves
	if (ulRenderBitVector & Forest_RenderLeaves)
	{

		DWORD dwSavedAlphaRef = 0;
		if (ulRenderBitVector & Forest_RenderToShadow || ulRenderBitVector & Forest_RenderToMiniMap)
		{
			dwSavedAlphaRef = SHADERMANAGER.GetAlphaTestRef();
			SHADERMANAGER.SetAlphaTestRefByte(0x00000000);
		}


		itor = m_pMainTreeMap.begin();

		while (itor != m_pMainTreeMap.end())
		{
			CSpeedTreeWrapper * pMainTree = (itor++)->second;
			CSpeedTreeWrapper ** ppInstances = pMainTree->GetInstances(uiCount);

			pMainTree->SetupLeafForTreeType();

			for (UINT i = 0; i < uiCount; ++i)
				if (ppInstances[i]->isShow())
					ppInstances[i]->RenderLeaves((ulRenderBitVector & Forest_RenderToShadow) != 0);
		}

		if (ulRenderBitVector & Forest_RenderToShadow || ulRenderBitVector & Forest_RenderToMiniMap)
		{
			SHADERMANAGER.SetAlphaTestRefByte(dwSavedAlphaRef);
		}
	}

	// render billboards
	#ifndef WRAPPER_NO_BILLBOARD_MODE
		if (ulRenderBitVector & Forest_RenderBillboards)
		{
			SHADERMANAGER.SetLightingEnabled(false);

			itor = m_pMainTreeMap.begin();

			while (itor != m_pMainTreeMap.end())
			{
				CSpeedTreeWrapper * pMainTree = (itor++)->second;
				CSpeedTreeWrapper ** ppInstances = pMainTree->GetInstances(uiCount);

				pMainTree->SetupBranchForTreeType();

				for (UINT i = 0; i < uiCount; ++i)
					if (ppInstances[i]->isShow())
						ppInstances[i]->RenderBillboards();
			}
		}
	#endif

	SHADERMANAGER.SetLightingEnabled(bSavedLightState);

	SHADERMANAGER.SetAlphaTestEnabled(bSavedAlphaTestEnable);
	SHADERMANAGER.RestorePipelineState(PSTATE_CULLMODE);
}

void CSpeedTreeForestDX11::RenderToShadowMap()
{
	Render(Forest_RenderToShadow | Forest_RenderBranches | Forest_RenderFronds | Forest_RenderLeaves);
}

void CSpeedTreeForestDX11::RenderBranchesBatched()
{
	SHADERMANAGER.BeginSpeedTreeVTF();

	TTreeMap::iterator itor = m_pMainTreeMap.begin();
	while (itor != m_pMainTreeMap.end())
	{
		CSpeedTreeWrapper* pMainTree = (itor++)->second;
		UINT uiCount = 0;
		CSpeedTreeWrapper** ppInstances = pMainTree->GetInstances(uiCount);

		pMainTree->SetupBranchForTreeType();

		// Get geometry cache to check LOD and index counts
		CSpeedTreeRT::SGeometry* pMainGeoCache = pMainTree->GetGeometryCache();
		if (!pMainGeoCache || !pMainTree->GetBranchIndexCounts())
			continue;

		pMainTree->GetSpeedTree()->GetGeometry(*pMainGeoCache, SpeedTree_BranchGeometry);

		// Collect visible instances into VTF batch
		VTFMANAGER.BeginBatch();

		for (UINT i = 0; i < uiCount; ++i)
		{
			CSpeedTreeWrapper* pInst = ppInstances[i];
			if (!pInst->isShow())
				continue;

			// Get geometry for this instance to check alpha test
			CSpeedTreeRT::SGeometry* pInstGeoCache = pInst->GetGeometryCache();
			if (!pInstGeoCache)
				continue;

			pInst->GetSpeedTree()->GetGeometry(*pInstGeoCache, SpeedTree_BranchGeometry);
			if (!pInstGeoCache->m_fBranchAlphaTestValue)
				continue;

			int nLod = pInstGeoCache->m_sBranches.m_nDiscreteLodLevel;
			if (pMainTree->GetBranchIndexCounts()[nLod] <= 0)
				continue;

			// Build world matrix from tree position
			Vector3 vecPos = pInst->GetSpeedTree()->GetTreePosition();
			Matrix matTranslation;
			MatrixIdentity(&matTranslation);
			MatrixTranslation(&matTranslation, vecPos.x, vecPos.y, vecPos.z);

			VTFMANAGER.AddInstance(&matTranslation);
		}

		UINT instanceCount = VTFMANAGER.GetInstanceCount();
		if (instanceCount == 0)
			continue;

		// Upload and bind VTF texture
		VTFMANAGER.CommitAndBind(8);

		// Get index count for the current LOD
		int nLod = pMainGeoCache->m_sBranches.m_nDiscreteLodLevel;
		UINT indexCount = pMainTree->GetBranchIndexCounts()[nLod];
		if (indexCount <= 2)
			continue;

		SHADERMANAGER.SetAlphaTestRefByte(DWORD(pMainGeoCache->m_fBranchAlphaTestValue));

		ms_faceCount += (indexCount - 2) * instanceCount;
		SHADERMANAGER.DrawIndexedInstanced(TOPOLOGY_TRIANGLESTRIP, indexCount, instanceCount);

		VTFMANAGER.Unbind(8);
	}

	SHADERMANAGER.BeginSpeedTree();
}

///////////////////////////////////////////////////////////////////////
// VTF Batched Rendering - Fronds
void CSpeedTreeForestDX11::RenderFrondsBatched()
{
	SHADERMANAGER.BeginSpeedTreeVTF();

	TTreeMap::iterator itor = m_pMainTreeMap.begin();
	while (itor != m_pMainTreeMap.end())
	{
		CSpeedTreeWrapper* pMainTree = (itor++)->second;
		UINT uiCount = 0;
		CSpeedTreeWrapper** ppInstances = pMainTree->GetInstances(uiCount);

		pMainTree->SetupFrondForTreeType();

		CSpeedTreeRT::SGeometry* pMainGeoCache = pMainTree->GetGeometryCache();
		if (!pMainGeoCache || !pMainTree->GetFrondIndexCounts())
			continue;

		pMainTree->GetSpeedTree()->GetGeometry(*pMainGeoCache, SpeedTree_FrondGeometry);

		VTFMANAGER.BeginBatch();

		for (UINT i = 0; i < uiCount; ++i)
		{
			CSpeedTreeWrapper* pInst = ppInstances[i];
			if (!pInst->isShow())
				continue;

			CSpeedTreeRT::SGeometry* pInstGeoCache = pInst->GetGeometryCache();
			if (!pInstGeoCache)
				continue;

			pInst->GetSpeedTree()->GetGeometry(*pInstGeoCache, SpeedTree_FrondGeometry);
			if (pInstGeoCache->m_fFrondAlphaTestValue <= 0.0f)
				continue;

			int nLod = pInstGeoCache->m_sFronds.m_nDiscreteLodLevel;
			if (pMainTree->GetFrondIndexCounts()[nLod] <= 0)
				continue;

			Vector3 vecPos = pInst->GetSpeedTree()->GetTreePosition();
			Matrix matTranslation;
			MatrixIdentity(&matTranslation);
			MatrixTranslation(&matTranslation, vecPos.x, vecPos.y, vecPos.z);

			VTFMANAGER.AddInstance(&matTranslation);
		}

		UINT instanceCount = VTFMANAGER.GetInstanceCount();
		if (instanceCount == 0)
			continue;

		VTFMANAGER.CommitAndBind(8);

		int nLod = pMainGeoCache->m_sFronds.m_nDiscreteLodLevel;
		UINT indexCount = pMainTree->GetFrondIndexCounts()[nLod];
		if (indexCount <= 2)
			continue;

		SHADERMANAGER.SetAlphaTestRefByte(DWORD(pMainGeoCache->m_fFrondAlphaTestValue));

		ms_faceCount += (indexCount - 2) * instanceCount;
		SHADERMANAGER.DrawIndexedInstanced(TOPOLOGY_TRIANGLESTRIP, indexCount, instanceCount);

		VTFMANAGER.Unbind(8);
	}

	SHADERMANAGER.BeginSpeedTree();
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
