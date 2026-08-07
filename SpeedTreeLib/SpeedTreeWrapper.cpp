///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper Class
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
//

#pragma warning(disable:4786)

///////////////////////////////////////////////////////////////////////
//	Include Files
#include "StdAfx.h"

#include <stdlib.h>
#include <stdio.h>
#include "../eterBase/Debug.h"
#include "../eterBase/Timer.h"
#include "../eterBase/Filename.h"
#include "../eterLib/ResourceManager.h"
#include "../eterLib/Camera.h"
#include "../eterLib/ShaderManager.h"

#include "SpeedTreeConfig.h"
#include "SpeedTreeForestDX11.h"
#include "SpeedTreeWrapper.h"
#include "VertexShaders.h"
#include "../eterLib/ShaderInit.h"

using namespace std;

ID3D11InputLayout* CSpeedTreeWrapper::ms_dwBranchVertexShader = 0;
ID3D11InputLayout* CSpeedTreeWrapper::ms_dwLeafVertexShader = 0;
bool CSpeedTreeWrapper::ms_bSelfShadowOn = true;

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::CSpeedTreeWrapper
CSpeedTreeWrapper::CSpeedTreeWrapper() :
	m_pSpeedTree(new CSpeedTreeRT),
	m_bIsInstance(false),
	m_pInstanceOf(NULL),
	m_pGeometryCache(NULL),
	m_usNumLeafLods(0),
	m_pBranchIndexCounts(NULL),
	m_pBranchIndexBuffer(NULL),
	m_pBranchVertexBuffer(NULL),
	m_pFrondIndexCounts(NULL),
	m_pFrondIndexBuffer(NULL),
	m_pFrondVertexBuffer(NULL),
	m_pLeafVertexBuffer(NULL),
	m_pLeafShadowVertexBuffer(NULL),
	m_pLeavesUpdatedByCpu(NULL),
	m_unBranchVertexCount(0),
	m_unFrondVertexCount(0),
	m_pTextureInfo(NULL)
{
	Initialize();
}

void CSpeedTreeWrapper::Initialize()
{
	// set initial position
	m_afPos[0] = m_afPos[1] = m_afPos[2] = 0.0f;

	if (m_pSpeedTree)
	{
		m_pSpeedTree->SetWindStrength(1.0f);
		m_pSpeedTree->SetLocalMatrices(0, 4);
	}
}

void CSpeedTreeWrapper::MakeSpeedTree()
{
	assert(m_pSpeedTree == nullptr);

	if (m_pSpeedTree)
		return;

	m_pSpeedTree = new CSpeedTreeRT;
	Initialize();
}

void CSpeedTreeWrapper::SetVertexShaders(ID3D11InputLayout* dwBranchVertexShader, ID3D11InputLayout* dwLeafVertexShader)
{
	ms_dwBranchVertexShader = dwBranchVertexShader;
	ms_dwLeafVertexShader = dwLeafVertexShader;
}

void CSpeedTreeWrapper::OnRenderPCBlocker()
{
	if (ms_dwBranchVertexShader == 0)
	{
		ms_dwBranchVertexShader = LoadBranchShader(ms_pDevice);
		//LogBox("Vertex Shader not assigned. You must call CSpeedTreeWrapper::VSSetShader for this");
	}

	if (ms_dwLeafVertexShader == 0)
	{
		ms_dwLeafVertexShader = LoadLeafShader(ms_pDevice);
		//LogBox("Vertex Shader not assigned. You must call CSpeedTreeWrapper::VSSetShader for this");
	}

	CSpeedTreeForestDX11::Instance().UpdateSystem(ELTimer_GetMSec() / 1000.0f);

	m_pSpeedTree->SetLodLevel(1.0f);
	//Advance();

	CCamera* pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return;

	CSpeedTreeForestDX11::Instance().UpdateCompundMatrix(pCamera->GetEye(), ms_matView, ms_matProj);


	bool bSavedLighting = SHADERMANAGER.GetLightingEnabled();
	bool bSavedFogEnable = SHADERMANAGER.GetFogEnabled();
	bool bSavedAlphaTestEnable = SHADERMANAGER.GetAlphaTestEnabled();
	DWORD dwAlphaBlendEnable = SHADERMANAGER.GetPipelineState(PSTATE_BLENDENABLE);
	SHADERMANAGER.SetLightingEnabled(true);  // SpeedTree shader handles lighting
	SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, TRUE);
	SHADERMANAGER.SetAlphaTestEnabled(true);
	SHADERMANAGER.SavePipelineState(PSTATE_CULLMODE, CULL_FRONT);
	SHADERMANAGER.SetFogEnabled(true);  // SpeedTree shader handles fog

	BeginShaderSpeedTreeRender(1.0f);

	// 	SetupBranchForTreeType():
	{
		// update the branch geometry for CPU wind
#ifdef WRAPPER_USE_CPU_WIND
		m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_BranchGeometry);

		if (m_pGeometryCache->m_sBranches.m_usNumStrips > 0)
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			if (SUCCEEDED(ms_pContext->Map(m_pBranchVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
			{
				SBranchVertex* pVertexBuffer = (SBranchVertex*)mappedResource.pData;
				for (UINT i = 0; i < m_unBranchVertexCount; ++i)
				{
					memcpy(&(pVertexBuffer[i].m_vPosition), &(m_pGeometryCache->m_sBranches.m_pCoords[i * 3]), 3 * sizeof(float));
				}
				ms_pContext->Unmap(m_pBranchVertexBuffer, 0);
			}
		}
#endif

		ID3D11ShaderResourceView* lpd3dTexture;

		// set texture map
		if ((lpd3dTexture = m_BranchImageInstance.GetTextureReference().GetD3DTexture()))
			SHADERMANAGER.SetShaderResource(0, lpd3dTexture);

		if (m_pGeometryCache->m_sBranches.m_usVertexCount > 0)
		{
			// activate the branch vertex buffer
			SHADERMANAGER.SetVertexBuffer(0, m_pBranchVertexBuffer, sizeof(SBranchVertex));
			// set the index buffer
			SHADERMANAGER.SetIndexBuffer(m_pBranchIndexBuffer);
		}
	}

	RenderBranches();

	SHADERMANAGER.SetShaderResource(0, m_CompositeImageInstance.GetTextureReference().GetD3DTexture());
	SHADERMANAGER.SetPipelineState(PSTATE_CULLMODE, CULL_NONE);

	// 	SetupFrondForTreeType();
	{
		// update the frond geometry for CPU wind
#ifdef WRAPPER_USE_CPU_WIND
		m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_FrondGeometry);
		if (m_pGeometryCache->m_sFronds.m_usNumStrips > 0)
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			if (SUCCEEDED(ms_pContext->Map(m_pFrondVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
			{
				SBranchVertex* pVertexBuffer = (SBranchVertex*)mappedResource.pData;
				for (UINT i = 0; i < m_unFrondVertexCount; ++i)
				{
					memcpy(&(pVertexBuffer[i].m_vPosition), &(m_pGeometryCache->m_sFronds.m_pCoords[i * 3]), 3 * sizeof(float));
				}
				ms_pContext->Unmap(m_pFrondVertexBuffer, 0);
			}
		}
#endif

		if (!m_CompositeImageInstance.IsEmpty())
			SHADERMANAGER.SetShaderResource(0, m_CompositeImageInstance.GetTextureReference().GetD3DTexture());

		if (m_pGeometryCache->m_sFronds.m_usVertexCount > 0)
		{
			// activate the frond vertex buffer
			SHADERMANAGER.SetVertexBuffer(0, m_pFrondVertexBuffer, sizeof(SBranchVertex));
			// set the index buffer
			SHADERMANAGER.SetIndexBuffer(m_pFrondIndexBuffer);
		}
	}
	RenderFronds();

	SHADERMANAGER.BeginSpeedTreeLeaf();

	SetupLeafForTreeType();
	RenderLeaves();
	EndLeafForTreeType();

	// Billboards use particle shader
	SHADERMANAGER.SetParticleColor(0xFFFFFFFF);  // White - billboards use per-vertex color
	SHADERMANAGER.BeginParticle();
	RenderBillboards();

	EndShaderSpeedTreeRender();

	SHADERMANAGER.RestorePipelineState(PSTATE_CULLMODE);
	SHADERMANAGER.SetAlphaTestEnabled(bSavedAlphaTestEnable);
	SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, dwAlphaBlendEnable);
	SHADERMANAGER.SetLightingEnabled(bSavedLighting);
	SHADERMANAGER.SetFogEnabled(bSavedFogEnable);
}

void CSpeedTreeWrapper::OnRender()
{
	if (ms_dwBranchVertexShader == 0)
	{
		ms_dwBranchVertexShader = LoadBranchShader(ms_pDevice);
		//LogBox("Vertex Shader not assigned. You must call CSpeedTreeWrapper::VSSetShader for this");
	}

	if (ms_dwLeafVertexShader == 0)
	{
		ms_dwLeafVertexShader = LoadLeafShader(ms_pDevice);
		//LogBox("Vertex Shader not assigned. You must call CSpeedTreeWrapper::VSSetShader for this");
	}

	CSpeedTreeForestDX11::Instance().UpdateSystem(ELTimer_GetMSec() / 1000.0f);

	m_pSpeedTree->SetLodLevel(1.0f);
	//Advance();

	CCamera* pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return;

	CSpeedTreeForestDX11::Instance().UpdateCompundMatrix(pCamera->GetEye(), ms_matView, ms_matProj);

	SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSU, ADDRESS_WRAP);
	SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSV, ADDRESS_WRAP);

	bool bSavedLighting = SHADERMANAGER.GetLightingEnabled();
	SHADERMANAGER.SetLightingEnabled(true);  // SpeedTree shader handles lighting
	bool bSavedAlphaTestEnable = SHADERMANAGER.GetAlphaTestEnabled();
	SHADERMANAGER.SetAlphaTestEnabled(true);
	SHADERMANAGER.SavePipelineState(PSTATE_CULLMODE, CULL_FRONT);
	bool bSavedFogEnable = SHADERMANAGER.GetFogEnabled();
	SHADERMANAGER.SetFogEnabled(true);  // SpeedTree shader handles fog

	BeginShaderSpeedTreeRender(1.0f);

	SetupBranchForTreeType();
	RenderBranches();

	SHADERMANAGER.SetShaderResource(0, m_CompositeImageInstance.GetTextureReference().GetD3DTexture());
	SHADERMANAGER.SetPipelineState(PSTATE_CULLMODE, CULL_NONE);

	SetupFrondForTreeType();
	RenderFronds();

	SHADERMANAGER.BeginSpeedTreeLeaf();

	SetupLeafForTreeType();
	RenderLeaves();
	EndLeafForTreeType();

	// Billboards use particle shader
	SHADERMANAGER.SetParticleColor(0xFFFFFFFF);  // White - billboards use per-vertex color
	SHADERMANAGER.BeginParticle();
	RenderBillboards();

	EndShaderSpeedTreeRender();

	SHADERMANAGER.SetLightingEnabled(bSavedLighting);
	SHADERMANAGER.SetAlphaTestEnabled(bSavedAlphaTestEnable);
	SHADERMANAGER.RestorePipelineState(PSTATE_CULLMODE);
	SHADERMANAGER.SetFogEnabled(bSavedFogEnable);
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::~CSpeedTreeWrapper

CSpeedTreeWrapper::~CSpeedTreeWrapper()
{
	// if this is not an instance, clean up
	if (!m_bIsInstance)
	{
		SAFE_RELEASE(m_pBranchVertexBuffer);
		SAFE_RELEASE(m_pBranchIndexBuffer);
		SAFE_DELETE_ARRAY(m_pBranchIndexCounts);

		SAFE_RELEASE(m_pFrondVertexBuffer);
		SAFE_RELEASE(m_pFrondIndexBuffer);
		SAFE_DELETE_ARRAY(m_pFrondIndexCounts);

		for (short i = 0; i < m_usNumLeafLods; ++i)
		{
			if (m_pLeafVertexBuffer)
				SAFE_RELEASE(m_pLeafVertexBuffer[i]);
			if (m_pLeafShadowVertexBuffer)
				SAFE_RELEASE(m_pLeafShadowVertexBuffer[i]);
		}

		SAFE_DELETE_ARRAY(m_pLeavesUpdatedByCpu);
		SAFE_DELETE_ARRAY(m_pLeafVertexBuffer);
		SAFE_DELETE_ARRAY(m_pLeafShadowVertexBuffer);

		SAFE_DELETE(m_pTextureInfo);

		SAFE_DELETE(m_pGeometryCache);
	}

	// always delete the speedtree
	SAFE_DELETE(m_pSpeedTree);

	Clear();
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::LoadTree
bool CSpeedTreeWrapper::LoadTree(const char* pszSptFile, const BYTE* c_pbBlock, unsigned int uiBlockSize, UINT nSeed, float fSize, float fSizeVariance)
{
	bool bSuccess = false;

	// directx, so allow for flipping of the texture coordinate
#ifdef WRAPPER_FLIP_T_TEXCOORD
	m_pSpeedTree->SetTextureFlip(true);
#endif

	// load the tree file
	if (!m_pSpeedTree->LoadTree(c_pbBlock, uiBlockSize))
	{
		if (!m_pSpeedTree->LoadTree(pszSptFile))
		{
			TraceError("SpeedTreeRT Error: %s", CSpeedTreeRT::GetCurrentError());
			return false;
		}
	}

	// override the lighting method stored in the spt file
#ifdef WRAPPER_USE_DYNAMIC_LIGHTING
	m_pSpeedTree->SetBranchLightingMethod(CSpeedTreeRT::LIGHT_DYNAMIC);
	m_pSpeedTree->SetLeafLightingMethod(CSpeedTreeRT::LIGHT_DYNAMIC);
	m_pSpeedTree->SetFrondLightingMethod(CSpeedTreeRT::LIGHT_DYNAMIC);
#else
	m_pSpeedTree->SetBranchLightingMethod(CSpeedTreeRT::LIGHT_STATIC);
	m_pSpeedTree->SetLeafLightingMethod(CSpeedTreeRT::LIGHT_STATIC);
	m_pSpeedTree->SetFrondLightingMethod(CSpeedTreeRT::LIGHT_STATIC);
#endif

	// set the wind method
#ifdef WRAPPER_USE_GPU_WIND
	m_pSpeedTree->SetBranchWindMethod(CSpeedTreeRT::WIND_GPU);
	m_pSpeedTree->SetLeafWindMethod(CSpeedTreeRT::WIND_GPU);
	m_pSpeedTree->SetFrondWindMethod(CSpeedTreeRT::WIND_GPU);
#endif
#ifdef WRAPPER_USE_CPU_WIND
	m_pSpeedTree->SetBranchWindMethod(CSpeedTreeRT::WIND_CPU);
	m_pSpeedTree->SetLeafWindMethod(CSpeedTreeRT::WIND_CPU);
	m_pSpeedTree->SetFrondWindMethod(CSpeedTreeRT::WIND_CPU);
#endif
#ifdef WRAPPER_USE_NO_WIND
	m_pSpeedTree->SetBranchWindMethod(CSpeedTreeRT::WIND_NONE);
	m_pSpeedTree->SetLeafWindMethod(CSpeedTreeRT::WIND_NONE);
	m_pSpeedTree->SetFrondWindMethod(CSpeedTreeRT::WIND_NONE);
#endif

	m_pSpeedTree->SetNumLeafRockingGroups(1);

	// override the size, if necessary
	if (fSize >= 0.0f && fSizeVariance >= 0.0f)
		m_pSpeedTree->SetTreeSize(fSize, fSizeVariance);

	// generate tree geometry
	if (m_pSpeedTree->Compute(NULL, nSeed))
	{
		// get the dimensions
		m_pSpeedTree->GetBoundingBox(m_afBoundingBox);

		// make the leaves rock in the wind
		m_pSpeedTree->SetLeafRockingState(true);

		// billboard setup
#ifdef WRAPPER_NO_BILLBOARD_MODE
		CSpeedTreeRT::SetDropToBillboard(false);
#else
		CSpeedTreeRT::SetDropToBillboard(true);
#endif

		// query & set materials
		m_cBranchMaterial.Set(m_pSpeedTree->GetBranchMaterial());
		m_cFrondMaterial.Set(m_pSpeedTree->GetFrondMaterial());
		m_cLeafMaterial.Set(m_pSpeedTree->GetLeafMaterial());

		// adjust lod distances
		float fHeight = m_afBoundingBox[5] - m_afBoundingBox[2];
		m_pSpeedTree->SetLodLimits(fHeight * c_fNearLodFactor, fHeight * c_fFarLodFactor);

		// query textures
		m_pTextureInfo = new CSpeedTreeRT::STextures;
		m_pSpeedTree->GetTextures(*m_pTextureInfo);

		// load branch textures
		LoadTexture((CFileNameHelper::GetPath(string(pszSptFile)) + CFileNameHelper::NoExtension(string(m_pTextureInfo->m_pBranchTextureFilename)) + ".dds").c_str(), m_BranchImageInstance);

#ifdef WRAPPER_RENDER_SELF_SHADOWS
		if (m_pTextureInfo->m_pSelfShadowFilename != NULL)
			LoadTexture((CFileNameHelper::GetPath(string(pszSptFile)) + CFileNameHelper::NoExtension(string(m_pTextureInfo->m_pSelfShadowFilename)) + ".dds").c_str(), m_ShadowImageInstance);
#endif
		if (m_pTextureInfo->m_pCompositeFilename)
			LoadTexture((CFileNameHelper::GetPath(string(pszSptFile)) + CFileNameHelper::NoExtension(string(m_pTextureInfo->m_pCompositeFilename)) + ".dds").c_str(), m_CompositeImageInstance);

		// setup the index and vertex buffers
		SetupBuffers();

		// everything appeared to go well
		bSuccess = true;
	}
	else // tree failed to compute
		fprintf(stderr, "\nFatal Error, cannot compute tree [%s]\n\n", CSpeedTreeRT::GetCurrentError());

	return bSuccess;
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::SetupBuffers

void CSpeedTreeWrapper::SetupBuffers(void)
{
	m_pSpeedTree->SetLodLevel(1.0f);

	if (m_pGeometryCache == NULL)
		m_pGeometryCache = new CSpeedTreeRT::SGeometry;

	m_pSpeedTree->GetGeometry(*m_pGeometryCache);

	// setup the buffers for each part
	SetupBranchBuffers();
	SetupFrondBuffers();
	SetupLeafBuffers();
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::SetupBranchBuffers

void CSpeedTreeWrapper::SetupBranchBuffers(void)
{
	if (!ms_pDevice)
		return;

	// reference to branch structure
	CSpeedTreeRT::SGeometry::SIndexed* pBranches = &(m_pGeometryCache->m_sBranches);
	m_unBranchVertexCount = pBranches->m_usVertexCount; // we asked for a contiguous strip

	// check if this tree has branches
	if (m_unBranchVertexCount > 1)
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = m_unBranchVertexCount * sizeof(SBranchVertex);
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
#ifndef WRAPPER_USE_CPU_WIND
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.CPUAccessFlags = 0;
#else
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
#endif

		// Build vertex data in temporary buffer
		std::vector<SBranchVertex> vertexData(m_unBranchVertexCount);
		SBranchVertex* pVertexBuffer = vertexData.data();
		{
			for (UINT i = 0; i < m_unBranchVertexCount; ++i)
			{
				// position
				memcpy(&pVertexBuffer->m_vPosition, &(pBranches->m_pCoords[i * 3]), 3 * sizeof(float));

				// normal or color
#ifdef WRAPPER_USE_DYNAMIC_LIGHTING
				memcpy(&pVertexBuffer->m_vNormal, &(pBranches->m_pNormals[i * 3]), 3 * sizeof(float));
#else
				pVertexBuffer->m_dwDiffuseColor = pBranches->m_pColors[i];
#endif

				// texcoords for layer 0
				pVertexBuffer->m_fTexCoords[0] = pBranches->m_pTexCoords0[i * 2];
				pVertexBuffer->m_fTexCoords[1] = pBranches->m_pTexCoords0[i * 2 + 1];

				// texcoords for layer 1 (if enabled)
#ifdef WRAPPER_RENDER_SELF_SHADOWS
				pVertexBuffer->m_fShadowCoords[0] = pBranches->m_pTexCoords1[i * 2];
				pVertexBuffer->m_fShadowCoords[1] = pBranches->m_pTexCoords1[i * 2 + 1];
#endif

				// extra data for gpu wind
#ifdef WRAPPER_USE_GPU_WIND
				pVertexBuffer->m_fWindIndex = 4.0f * pBranches->m_pWindMatrixIndices[i];
				pVertexBuffer->m_fWindWeight = pBranches->m_pWindWeights[i];
#endif

				++pVertexBuffer;
			}
		}

		// Create buffer with initial data
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = vertexData.data();
		ms_pDevice->CreateBuffer(&bufferDesc, &initData, &m_pBranchVertexBuffer);

		// create and fill the index counts for each LOD
		UINT unNumLodLevels = m_pSpeedTree->GetNumBranchLodLevels();
		m_pBranchIndexCounts = new unsigned short[unNumLodLevels];
		for (UINT i = 0; i < unNumLodLevels; ++i)
		{
			// force update for particular LOD
			m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_BranchGeometry, i);

			// check if this LOD has branches
			if (pBranches->m_usNumStrips > 0)
				m_pBranchIndexCounts[i] = pBranches->m_pStripLengths[0];
			else
				m_pBranchIndexCounts[i] = 0;
		}
		// set back to highest LOD
		m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_BranchGeometry, 0);

		D3D11_BUFFER_DESC indexBufferDesc = {};
		indexBufferDesc.ByteWidth = m_pBranchIndexCounts[0] * sizeof(unsigned short);
		indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		indexBufferDesc.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA indexInitData = {};
		indexInitData.pSysMem = pBranches->m_pStrips[0];
		ms_pDevice->CreateBuffer(&indexBufferDesc, &indexInitData, &m_pBranchIndexBuffer);
	}
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::SetupFrondBuffers

void CSpeedTreeWrapper::SetupFrondBuffers(void)
{
	if (!ms_pDevice)
		return;

	// reference to frond structure
	CSpeedTreeRT::SGeometry::SIndexed* pFronds = &(m_pGeometryCache->m_sFronds);
	m_unFrondVertexCount = pFronds->m_usVertexCount; // we asked for a contiguous strip

	// check if tree has fronds
	if (m_unFrondVertexCount > 1)
	{
		D3D11_BUFFER_DESC frondBufferDesc = {};
		frondBufferDesc.ByteWidth = m_unFrondVertexCount * sizeof(SBranchVertex);
		frondBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
#ifndef WRAPPER_USE_CPU_WIND
		frondBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		frondBufferDesc.CPUAccessFlags = 0;
#else
		frondBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		frondBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
#endif

		// Build vertex data in temporary buffer
		std::vector<SBranchVertex> frondVertexData(m_unFrondVertexCount);
		SBranchVertex* pVertexBuffer = frondVertexData.data();
		for (UINT i = 0; i < m_unFrondVertexCount; ++i)
		{
			// position
			memcpy(&pVertexBuffer->m_vPosition, &(pFronds->m_pCoords[i * 3]), 3 * sizeof(float));

			// normal or color
#ifdef WRAPPER_USE_DYNAMIC_LIGHTING
			memcpy(&pVertexBuffer->m_vNormal, &(pFronds->m_pNormals[i * 3]), 3 * sizeof(float));
#else
			pVertexBuffer->m_dwDiffuseColor = pFronds->m_pColors[i];
#endif

			// texcoords for layer 0
			pVertexBuffer->m_fTexCoords[0] = pFronds->m_pTexCoords0[i * 2];
			pVertexBuffer->m_fTexCoords[1] = pFronds->m_pTexCoords0[i * 2 + 1];

			// texcoords for layer 1 (if enabled)
#ifdef WRAPPER_RENDER_SELF_SHADOWS
			pVertexBuffer->m_fShadowCoords[0] = pFronds->m_pTexCoords1[i * 2];
			pVertexBuffer->m_fShadowCoords[1] = pFronds->m_pTexCoords1[i * 2 + 1];
#endif

			// extra data for gpu wind
#ifdef WRAPPER_USE_GPU_WIND
			pVertexBuffer->m_fWindIndex = 4.0f * pFronds->m_pWindMatrixIndices[i];
			pVertexBuffer->m_fWindWeight = pFronds->m_pWindWeights[i];
#endif

			++pVertexBuffer;
		}

		// Create buffer with initial data
		D3D11_SUBRESOURCE_DATA frondInitData = {};
		frondInitData.pSysMem = frondVertexData.data();
		ms_pDevice->CreateBuffer(&frondBufferDesc, &frondInitData, &m_pFrondVertexBuffer);

		// create and fill the index counts for each LOD
		UINT unNumLodLevels = m_pSpeedTree->GetNumFrondLodLevels();
		m_pFrondIndexCounts = new unsigned short[unNumLodLevels];
		for (WORD j = 0; j < unNumLodLevels; ++j)
		{
			// force update for this LOD
			m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_FrondGeometry, -1, j);

			// check if this LOD has fronds
			if (pFronds->m_usNumStrips > 0)
				m_pFrondIndexCounts[j] = pFronds->m_pStripLengths[0];
			else
				m_pFrondIndexCounts[j] = 0;
		}
		// go back to highest LOD
		m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_FrondGeometry, -1, 0);

		D3D11_BUFFER_DESC frondIndexDesc = {};
		frondIndexDesc.ByteWidth = m_pFrondIndexCounts[0] * sizeof(unsigned short);
		frondIndexDesc.Usage = D3D11_USAGE_DEFAULT;
		frondIndexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		frondIndexDesc.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA frondIndexInitData = {};
		frondIndexInitData.pSysMem = pFronds->m_pStrips[0];
		ms_pDevice->CreateBuffer(&frondIndexDesc, &frondIndexInitData, &m_pFrondIndexBuffer);
	}
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::SetupLeafBuffers

void CSpeedTreeWrapper::SetupLeafBuffers(void)
{
	if (!ms_pDevice)
		return;

	// set up constants
	const short anVertexIndices[6] = { 0, 1, 2, 0, 2, 3 };
	//const int nNumLeafMaps = m_pTextureInfo->m_uiLeafTextureCount;

	// set up the leaf counts for each LOD
	m_usNumLeafLods = m_pSpeedTree->GetNumLeafLodLevels();

	// create array of vertex buffers (one for each LOD)
	m_pLeafVertexBuffer = new ID3D11Buffer*[m_usNumLeafLods];
	m_pLeafShadowVertexBuffer = new ID3D11Buffer*[m_usNumLeafLods];

	m_pLeavesUpdatedByCpu = new bool[m_usNumLeafLods];

	// cycle through LODs
	for (UINT unLod = 0; unLod < m_usNumLeafLods; ++unLod)
	{
		m_pLeavesUpdatedByCpu[unLod] = false;
		m_pLeafVertexBuffer[unLod] = NULL;
		m_pLeafShadowVertexBuffer[unLod] = NULL;

		// Get geometry for this specific LOD level
		m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_LeafGeometry, -1, -1, unLod);

		// if this LOD has no leaves, skip it
		unsigned short usLeafCount = m_pGeometryCache->m_sLeaves0.m_usLeafCount;

		if (usLeafCount < 1)
			continue;

		D3D11_BUFFER_DESC leafBufferDesc = {};
		leafBufferDesc.ByteWidth = usLeafCount * 6 * sizeof(SLeafVertex);
		leafBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		leafBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		leafBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		// Build vertex data in temporary buffer
		std::vector<SLeafVertex> leafVertexData(usLeafCount * 6);
		SLeafVertex* pVertex = leafVertexData.data();
		for (UINT unLeaf = 0; unLeaf < usLeafCount; ++unLeaf)
		{
			const CSpeedTreeRT::SGeometry::SLeaf* pLeaf = &(m_pGeometryCache->m_sLeaves0);
			for (UINT unVert = 0; unVert < 6; ++unVert)  // 6 verts == 2 triangles
			{
				// position
				memcpy(pVertex->m_vPosition, &(pLeaf->m_pCenterCoords[unLeaf * 3]), 3 * sizeof(float));

#ifdef WRAPPER_USE_DYNAMIC_LIGHTING
				// normal
				memcpy(&pVertex->m_vNormal, &(pLeaf->m_pNormals[unLeaf * 3]), 3 * sizeof(float));
#else
				// color
				pVertex->m_dwDiffuseColor = pLeaf->m_pColors[unLeaf];
#endif

				// tex coord
				memcpy(pVertex->m_fTexCoords, &(pLeaf->m_pLeafMapTexCoords[unLeaf][anVertexIndices[unVert] * 2]), 2 * sizeof(float));

				// wind weights
#ifdef WRAPPER_USE_GPU_WIND
				pVertex->m_fWindIndex = 4.0f * pLeaf->m_pWindMatrixIndices[unLeaf];
				pVertex->m_fWindWeight = pLeaf->m_pWindWeights[unLeaf];

				pVertex->m_fLeafPlacementIndex = pVertex->m_fWindIndex;
				pVertex->m_fLeafScalarValue    = pVertex->m_fWindWeight;
#endif

				// GPU placement data
#ifdef WRAPPER_USE_GPU_LEAF_PLACEMENT
				pVertex->m_fLeafPlacementIndex = pLeaf->m_pLeafClusterIndices[unLeaf] * 4.0f + anVertexIndices[unVert];				pVertex->m_fLeafScalarValue = m_pSpeedTree->GetLeafLodSizeAdjustments()[unLod];
#endif

				++pVertex;
			}
		}

		// Create buffer with initial data
		D3D11_SUBRESOURCE_DATA leafInitData = {};
		leafInitData.pSysMem = leafVertexData.data();
		ms_pDevice->CreateBuffer(&leafBufferDesc, &leafInitData, &m_pLeafVertexBuffer[unLod]);
		std::vector<SLeafVertex> shadowVertexData(leafVertexData);
		{
			const CSpeedTreeRT::SGeometry::SLeaf* pShLeaf = &(m_pGeometryCache->m_sLeaves0);
			SLeafVertex* pShadowVertex = shadowVertexData.data();

			for (UINT unLeaf = 0; unLeaf < usLeafCount; ++unLeaf)
			{
				const float* c = pShLeaf->m_pLeafMapCoords[unLeaf];
				const float afEdgeU[3] = { c[4] - c[0], c[5] - c[1], c[6] - c[2] };
				const float afEdgeV[3] = { c[8] - c[4], c[9] - c[5], c[10] - c[6] };
				const float fHalfU = sqrtf(afEdgeU[0]*afEdgeU[0] + afEdgeU[1]*afEdgeU[1] + afEdgeU[2]*afEdgeU[2]) * 0.5f;
				const float fHalfV = sqrtf(afEdgeV[0]*afEdgeV[0] + afEdgeV[1]*afEdgeV[1] + afEdgeV[2]*afEdgeV[2]) * 0.5f;

				const float afCorner[4][2] =
				{
					{ -fHalfU, -fHalfV }, {  fHalfU, -fHalfV },
					{  fHalfU,  fHalfV }, { -fHalfU,  fHalfV },
				};

				const float* pShCenter = &(pShLeaf->m_pCenterCoords[unLeaf * 3]);
				for (UINT unVert = 0; unVert < 6; ++unVert)
				{
					const float* k = afCorner[anVertexIndices[unVert]];
					pShadowVertex->m_vPosition.x = pShCenter[0] + k[0];
					pShadowVertex->m_vPosition.y = pShCenter[1] + k[1];
					pShadowVertex->m_vPosition.z = pShCenter[2];
					++pShadowVertex;
				}
			}
		}

		D3D11_BUFFER_DESC shadowBufferDesc = leafBufferDesc;
		shadowBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		shadowBufferDesc.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA shadowInitData = {};
		shadowInitData.pSysMem = shadowVertexData.data();
		ms_pDevice->CreateBuffer(&shadowBufferDesc, &shadowInitData, &m_pLeafShadowVertexBuffer[unLod]);

	}
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::Advance

void CSpeedTreeWrapper::Advance(void)
{
	// compute LOD level (based on distance from camera)
	m_pSpeedTree->ComputeLodLevel();
	m_pSpeedTree->SetLodLevel(1.0f);

	// compute wind
#ifdef WRAPPER_USE_CPU_WIND
	m_pSpeedTree->ComputeWindEffects(true, true, true);
#endif
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::MakeInstance
CSpeedTreeWrapper* CSpeedTreeWrapper::MakeInstance()
{
	CSpeedTreeWrapper* pInstance = new CSpeedTreeWrapper;

	// make an instance of this object's SpeedTree
	pInstance->m_bIsInstance = true;
	pInstance->m_pSpeedTree = m_pSpeedTree->MakeInstance();

	if (pInstance->m_pSpeedTree)
	{
		// use the same materials
		pInstance->m_cBranchMaterial = m_cBranchMaterial;
		pInstance->m_cLeafMaterial = m_cLeafMaterial;
		pInstance->m_cFrondMaterial = m_cFrondMaterial;
		pInstance->m_CompositeImageInstance.SetImagePointer(m_CompositeImageInstance.GetGraphicImagePointer());
		pInstance->m_BranchImageInstance.SetImagePointer(m_BranchImageInstance.GetGraphicImagePointer());

		if (!m_ShadowImageInstance.IsEmpty())
			pInstance->m_ShadowImageInstance.SetImagePointer(m_ShadowImageInstance.GetGraphicImagePointer());

		pInstance->m_pTextureInfo = m_pTextureInfo;

		// use the same geometry cache
		pInstance->m_pGeometryCache = m_pGeometryCache;

		// use the same buffers
		pInstance->m_pBranchIndexBuffer = m_pBranchIndexBuffer;
		pInstance->m_pBranchIndexCounts = m_pBranchIndexCounts;
		pInstance->m_pBranchVertexBuffer = m_pBranchVertexBuffer;
		pInstance->m_unBranchVertexCount = m_unBranchVertexCount;

		pInstance->m_pFrondIndexBuffer = m_pFrondIndexBuffer;
		pInstance->m_pFrondIndexCounts = m_pFrondIndexCounts;
		pInstance->m_pFrondVertexBuffer = m_pFrondVertexBuffer;
		pInstance->m_unFrondVertexCount = m_unFrondVertexCount;

		pInstance->m_pLeafVertexBuffer = m_pLeafVertexBuffer;
		pInstance->m_pLeafShadowVertexBuffer = m_pLeafShadowVertexBuffer;
		pInstance->m_usNumLeafLods = m_usNumLeafLods;
		pInstance->m_pLeavesUpdatedByCpu = m_pLeavesUpdatedByCpu;

		// new stuff
		memcpy(pInstance->m_afPos, m_afPos, 3 * sizeof(float));
		memcpy(pInstance->m_afBoundingBox, m_afBoundingBox, 6 * sizeof(float));
		pInstance->m_pInstanceOf = this;
		m_vInstances.push_back(pInstance);
	}
	else
	{
		fprintf(stderr, "SpeedTreeRT Error: %s\n", m_pSpeedTree->GetCurrentError());
		delete pInstance;
		pInstance = NULL;
	}

	return pInstance;
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::GetInstances

CSpeedTreeWrapper** CSpeedTreeWrapper::GetInstances(UINT& nCount)
{
	nCount = (UINT)(m_vInstances.size());
	if (nCount)
		return &(m_vInstances[0]);
	else
		return NULL;
}

void CSpeedTreeWrapper::DeleteInstance(CSpeedTreeWrapper* pInstance)
{
	std::vector<CSpeedTreeWrapper*>::iterator itor = m_vInstances.begin();

	while (itor != m_vInstances.end())
	{
		if (*itor == pInstance)
		{
			itor = m_vInstances.erase(itor);
		}
		else
			++itor;
	}
	delete pInstance;
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::SetupBranchForTreeType

void CSpeedTreeWrapper::SetupBranchForTreeType(void) const
{
#ifdef WRAPPER_USE_DYNAMIC_LIGHTING
	// set lighting material
	SHADERMANAGER.SetMaterial(m_cBranchMaterial.Get());
	SetShaderConstants(m_pSpeedTree->GetBranchMaterial());
#endif

	// update the branch geometry for CPU wind
#ifdef WRAPPER_USE_CPU_WIND
	m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_BranchGeometry);

	if (m_pGeometryCache->m_sBranches.m_usNumStrips > 0)
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		if (SUCCEEDED(ms_pContext->Map(m_pBranchVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
		{
			SBranchVertex* pVertexBuffer = (SBranchVertex*)mappedResource.pData;
			for (UINT i = 0; i < m_unBranchVertexCount; ++i)
			{
				memcpy(&(pVertexBuffer[i].m_vPosition), &(m_pGeometryCache->m_sBranches.m_pCoords[i * 3]), 3 * sizeof(float));
			}
			ms_pContext->Unmap(m_pBranchVertexBuffer, 0);
		}
	}
#endif

	ID3D11ShaderResourceView* lpd3dTexture;

	// set texture map
	if ((lpd3dTexture = m_BranchImageInstance.GetTextureReference().GetD3DTexture()))
		SHADERMANAGER.SetShaderResource(0, lpd3dTexture);

	// bind shadow texture
#ifdef WRAPPER_RENDER_SELF_SHADOWS
	if (ms_bSelfShadowOn && (lpd3dTexture = m_ShadowImageInstance.GetTextureReference().GetD3DTexture()))
		SHADERMANAGER.SetShaderResource(1, lpd3dTexture);
	else
		SHADERMANAGER.SetShaderResource(1, NULL);
#endif

	if (m_pGeometryCache->m_sBranches.m_usVertexCount > 0)
	{
		// activate the branch vertex buffer
		SHADERMANAGER.SetVertexBuffer(0, m_pBranchVertexBuffer, sizeof(SBranchVertex));
		// set the index buffer
		SHADERMANAGER.SetIndexBuffer(m_pBranchIndexBuffer);
	}
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::RenderBranches

void CSpeedTreeWrapper::RenderBranches(void) const
{
	m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_BranchGeometry);

	if (m_pGeometryCache->m_fBranchAlphaTestValue)
	{
		PositionTree();

		// set alpha test value
		SHADERMANAGER.SetAlphaTestRefByte(DWORD(m_pGeometryCache->m_fBranchAlphaTestValue));

		// render if this LOD has branches
		if (m_pBranchIndexCounts &&
			m_pBranchIndexCounts[m_pGeometryCache->m_sBranches.m_nDiscreteLodLevel] > 0)
		{
			ms_faceCount += m_pBranchIndexCounts[m_pGeometryCache->m_sBranches.m_nDiscreteLodLevel] - 2;
			SHADERMANAGER.DrawIndexed(TOPOLOGY_TRIANGLESTRIP, 0, m_pGeometryCache->m_sBranches.m_usVertexCount, 0, m_pBranchIndexCounts[m_pGeometryCache->m_sBranches.m_nDiscreteLodLevel] - 2);
		}
	}
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::SetupFrondForTreeType

void CSpeedTreeWrapper::SetupFrondForTreeType(void) const
{
#ifdef SPEEDTREE_LIGHTING_DYNAMIC
	// set lighting material
	SHADERMANAGER.SetMaterial(m_cFrondMaterial.Get());
	SetShaderConstants(m_pSpeedTree->GetFrondMaterial());
#endif

	// update the frond geometry for CPU wind
#ifdef WRAPPER_USE_CPU_WIND
	m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_FrondGeometry);
	if (m_pGeometryCache->m_sFronds.m_usNumStrips > 0)
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		if (SUCCEEDED(ms_pContext->Map(m_pFrondVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
		{
			SBranchVertex* pVertexBuffer = (SBranchVertex*)mappedResource.pData;
			for (UINT i = 0; i < m_unFrondVertexCount; ++i)
			{
				memcpy(&(pVertexBuffer[i].m_vPosition), &(m_pGeometryCache->m_sFronds.m_pCoords[i * 3]), 3 * sizeof(float));
			}
			ms_pContext->Unmap(m_pFrondVertexBuffer, 0);
		}
	}
#endif

	if (!m_CompositeImageInstance.IsEmpty())
		SHADERMANAGER.SetShaderResource(0, m_CompositeImageInstance.GetTextureReference().GetD3DTexture());

	// bind shadow texture
#ifdef WRAPPER_RENDER_SELF_SHADOWS
	ID3D11ShaderResourceView* lpd3dTexture;

	if ((lpd3dTexture = m_ShadowImageInstance.GetTextureReference().GetD3DTexture()))
		SHADERMANAGER.SetShaderResource(1, lpd3dTexture);
#endif

	if (m_pGeometryCache->m_sFronds.m_usVertexCount > 0)
	{
		// activate the frond vertex buffer
		SHADERMANAGER.SetVertexBuffer(0, m_pFrondVertexBuffer, sizeof(SBranchVertex));
		// set the index buffer
		SHADERMANAGER.SetIndexBuffer(m_pFrondIndexBuffer);
	}
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::RenderFronds

void CSpeedTreeWrapper::RenderFronds(void) const
{
	m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_FrondGeometry);

	if (m_pGeometryCache->m_fFrondAlphaTestValue > 0.0f)
	{
		PositionTree();

		// set alpha test value
		SHADERMANAGER.SetAlphaTestRefByte(DWORD(m_pGeometryCache->m_fFrondAlphaTestValue));

		// render if this LOD has fronds
		if (m_pFrondIndexCounts &&
			m_pFrondIndexCounts[m_pGeometryCache->m_sFronds.m_nDiscreteLodLevel] > 0)
		{
			ms_faceCount += m_pFrondIndexCounts[m_pGeometryCache->m_sFronds.m_nDiscreteLodLevel] - 2;
			SHADERMANAGER.DrawIndexed(TOPOLOGY_TRIANGLESTRIP, 0, m_pGeometryCache->m_sFronds.m_usVertexCount, 0, m_pFrondIndexCounts[m_pGeometryCache->m_sFronds.m_nDiscreteLodLevel] - 2);
		}
	}
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::SetupLeafForTreeType

void CSpeedTreeWrapper::SetupLeafForTreeType(void) const
{
#ifdef SPEEDTREE_LIGHTING_DYNAMIC
	// set lighting material
	SHADERMANAGER.SetMaterial(m_cLeafMaterial.Get());
	SetShaderConstants(m_pSpeedTree->GetLeafMaterial());
#endif

	// pass leaf tables to shader
#ifdef WRAPPER_USE_GPU_LEAF_PLACEMENT
	UploadLeafTables(0);
#endif

	if (!m_CompositeImageInstance.IsEmpty())
		SHADERMANAGER.SetShaderResource(0, m_CompositeImageInstance.GetTextureReference().GetD3DTexture());

	// bind shadow texture
#ifdef WRAPPER_RENDER_SELF_SHADOWS
	SHADERMANAGER.SetShaderResource(1, NULL);
#endif
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::UploadLeafTables

#ifdef WRAPPER_USE_GPU_LEAF_PLACEMENT
void CSpeedTreeWrapper::UploadLeafTables(int nFirstEntry) const
{
	// query leaf cluster table from RT
	UINT uiEntryCount = 0;
	const float* pTable = m_pSpeedTree->GetLeafBillboardTable(uiEntryCount);

	// upload for vertex shader use
	SHADERMANAGER.SetSpeedTreeLeafTables(nFirstEntry, pTable, uiEntryCount / 4);
}
#endif

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::RenderLeaves

void CSpeedTreeWrapper::RenderLeaves(bool bShadowPass) const
{
	// update leaf geometry
	m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_LeafGeometry);

	for (UINT i = 0; !bShadowPass && i < 2; ++i)
	{
		const CSpeedTreeRT::SGeometry::SLeaf* pLeaf = (i == 0) ? &m_pGeometryCache->m_sLeaves0 : &m_pGeometryCache->m_sLeaves1;
		int unLod = pLeaf->m_nDiscreteLodLevel;

		if (unLod < 0 || unLod >= (int)m_usNumLeafLods)
			continue;

		if (pLeaf->m_bIsActive && m_pLeafVertexBuffer[unLod])
		{
			const UINT VERTEX_NUM = 8192;
			if (pLeaf->m_usLeafCount * 6 >= VERTEX_NUM)
				continue;

			D3D11_MAPPED_SUBRESOURCE mappedResource;
			if (SUCCEEDED(ms_pContext->Map(m_pLeafVertexBuffer[unLod], 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mappedResource)))
			{
				SLeafVertex* pVertex = (SLeafVertex*)mappedResource.pData;
				const float* center = pLeaf->m_pCenterCoords;
				for (UINT unLeaf = 0; unLeaf < pLeaf->m_usLeafCount; ++unLeaf)
				{
					const float* c = pLeaf->m_pLeafMapCoords[unLeaf];
					SLeafVertex* v = pVertex + unLeaf * 6;

					const Vector3 v0(c[0] + center[0], c[1] + center[1], c[2] + center[2]);
					const Vector3 v2(c[8] + center[0], c[9] + center[1], c[10] + center[2]);

					v[0].m_vPosition = v0;
					v[1].m_vPosition = Vector3(c[4] + center[0], c[5] + center[1], c[6] + center[2]);
					v[2].m_vPosition = v2;
					v[3].m_vPosition = v0;
					v[4].m_vPosition = v2;
					v[5].m_vPosition = Vector3(c[12] + center[0], c[13] + center[1], c[14] + center[2]);

					center += 3;
				}

				ms_pContext->Unmap(m_pLeafVertexBuffer[unLod], 0);
			}
		}
	}

	PositionTree();

	// render LODs, if needed
	for (UINT unLeafLevel = 0; unLeafLevel < 2; ++unLeafLevel)
	{
		const CSpeedTreeRT::SGeometry::SLeaf* pLeaf = (unLeafLevel == 0) ?
			&m_pGeometryCache->m_sLeaves0 : &m_pGeometryCache->m_sLeaves1;

		int unLod = pLeaf->m_nDiscreteLodLevel;

		// Bounds and null safety check for leaf buffer
		if (unLod > -1 && unLod < (int)m_usNumLeafLods &&
			m_pLeafVertexBuffer[unLod] != NULL &&
			pLeaf->m_bIsActive && pLeaf->m_usLeafCount > 0)
		{
			ID3D11Buffer* pkLeafVB = m_pLeafVertexBuffer[unLod];
			if (bShadowPass && m_pLeafShadowVertexBuffer && m_pLeafShadowVertexBuffer[unLod])
				pkLeafVB = m_pLeafShadowVertexBuffer[unLod];

			SHADERMANAGER.SetVertexBuffer(0, pkLeafVB, sizeof(SLeafVertex));
			SHADERMANAGER.SetAlphaTestRefByte(DWORD(pLeaf->m_fAlphaTestValue));

			ms_faceCount += pLeaf->m_usLeafCount * 2;
			SHADERMANAGER.Draw(TOPOLOGY_TRIANGLELIST, 0, pLeaf->m_usLeafCount * 2);
		}
	}
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::EndLeafForTreeType

void CSpeedTreeWrapper::EndLeafForTreeType(void)
{
	// reset copy flags for CPU wind
	for (UINT i = 0; i < m_usNumLeafLods; ++i)
		m_pLeavesUpdatedByCpu[i] = false;
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::RenderBillboards

void CSpeedTreeWrapper::RenderBillboards(void) const
{
#ifdef WRAPPER_BILLBOARD_MODE
	if (!m_CompositeImageInstance.IsEmpty())
		SHADERMANAGER.SetShaderResource(0, m_CompositeImageInstance.GetTextureReference().GetD3DTexture());

	PositionTree();

	struct SBillboardVertex
	{
		float fX, fY, fZ;
		DWORD dwColor;
		float fU, fV;
	};

	m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_BillboardGeometry);

	SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_PDT);

	if (m_pGeometryCache->m_sBillboard0.m_bIsActive)
	{
		const float* pCoords = m_pGeometryCache->m_sBillboard0.m_pCoords;
		const float* pTexCoords = m_pGeometryCache->m_sBillboard0.m_pTexCoords;
		SBillboardVertex sVertex[4] =
		{
			{ pCoords[0], pCoords[1], pCoords[2], 0xFFFFFFFF, pTexCoords[0], pTexCoords[1] },
			{ pCoords[3], pCoords[4], pCoords[5], 0xFFFFFFFF, pTexCoords[2], pTexCoords[3] },
			{ pCoords[6], pCoords[7], pCoords[8], 0xFFFFFFFF, pTexCoords[4], pTexCoords[5] },
			{ pCoords[9], pCoords[10], pCoords[11], 0xFFFFFFFF, pTexCoords[6], pTexCoords[7] },
		};

		SHADERMANAGER.SetAlphaTestRefByte(DWORD(m_pGeometryCache->m_sBillboard0.m_fAlphaTestValue));

		ms_faceCount += 2;
		SHADERMANAGER.DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, sVertex, sizeof(SBillboardVertex));
	}

	if (m_pGeometryCache->m_sBillboard1.m_bIsActive)
	{
		const float* pCoords = m_pGeometryCache->m_sBillboard1.m_pCoords;
		const float* pTexCoords = m_pGeometryCache->m_sBillboard1.m_pTexCoords;
		SBillboardVertex sVertex[4] =
		{
			{ pCoords[0], pCoords[1], pCoords[2], 0xFFFFFFFF, pTexCoords[0], pTexCoords[1] },
			{ pCoords[3], pCoords[4], pCoords[5], 0xFFFFFFFF, pTexCoords[2], pTexCoords[3] },
			{ pCoords[6], pCoords[7], pCoords[8], 0xFFFFFFFF, pTexCoords[4], pTexCoords[5] },
			{ pCoords[9], pCoords[10], pCoords[11], 0xFFFFFFFF, pTexCoords[6], pTexCoords[7] },
		};
		SHADERMANAGER.SetAlphaTestRefByte(DWORD(m_pGeometryCache->m_sBillboard1.m_fAlphaTestValue));

		ms_faceCount += 2;
		SHADERMANAGER.DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, sVertex, sizeof(SBillboardVertex));
	}

#ifdef WRAPPER_RENDER_HORIZONTAL_BILLBOARD
	// render horizontal billboard (if enabled)
	if (m_pGeometryCache->m_sHorizontalBillboard.m_bIsActive)
	{
		const float* pCoords = m_pGeometryCache->m_sHorizontalBillboard.m_pCoords;
		const float* pTexCoords = m_pGeometryCache->m_sHorizontalBillboard.m_pTexCoords;
		SBillboardVertex sVertex[4] =
		{
			{ pCoords[0], pCoords[1], pCoords[2], 0xFFFFFFFF, pTexCoords[0], pTexCoords[1] },
			{ pCoords[3], pCoords[4], pCoords[5], 0xFFFFFFFF, pTexCoords[2], pTexCoords[3] },
			{ pCoords[6], pCoords[7], pCoords[8], 0xFFFFFFFF, pTexCoords[4], pTexCoords[5] },
			{ pCoords[9], pCoords[10], pCoords[11], 0xFFFFFFFF, pTexCoords[6], pTexCoords[7] },
		};
		SHADERMANAGER.SetAlphaTestRefByte(DWORD(m_pGeometryCache->m_sHorizontalBillboard.m_fAlphaTestValue));

		ms_faceCount += 2;
		SHADERMANAGER.DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, sVertex, sizeof(SBillboardVertex));
	}

#endif
#endif
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::CleanUpMemory

void CSpeedTreeWrapper::CleanUpMemory(void)
{
	if (!m_bIsInstance)
		m_pSpeedTree->DeleteTransientData();
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::PositionTree

void CSpeedTreeWrapper::PositionTree(void) const
{
	Vector3 vecPosition = m_pSpeedTree->GetTreePosition();
	Matrix matTranslation;
	MatrixIdentity(&matTranslation);
	MatrixTranslation(&matTranslation, vecPosition.x, vecPosition.y, vecPosition.z);

	// store translation for client-side transformation
	SHADERMANAGER.SetMatrix(MATRIX_WORLD, &matTranslation);

	// store translation for use in vertex shader
	Vector4 vecConstant(vecPosition[0], vecPosition[1], vecPosition[2], 0.0f);
	SHADERMANAGER.SetSpeedTreeTreePosition((const float*)&vecConstant);
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::LoadTexture

bool CSpeedTreeWrapper::LoadTexture(const char* pFilename, CGraphicImageInstance& rImage)
{
	CResource* pResource = CResourceManager::Instance().GetResourcePointer(pFilename);
	rImage.SetImagePointer(static_cast<CGraphicImage*>(pResource));

	if (rImage.IsEmpty())
		return false;

	//TraceError("SpeedTreeWrapper::LoadTexture: %s", pFilename);
	return true;
}

///////////////////////////////////////////////////////////////////////
//	CSpeedTreeWrapper::SetShaderConstants

void CSpeedTreeWrapper::SetShaderConstants(const float* pMaterial) const
{
	const float afUsefulConstants[] =
	{
		m_pSpeedTree->GetLeafLightingAdjustment(), 0.0f, 0.0f, 0.0f,
	};

	SHADERMANAGER.SetSpeedTreeLeafLightingAdjustment(afUsefulConstants);

	const float afMaterial[] =
	{
		pMaterial[0], pMaterial[1], pMaterial[2], 1.0f,
			pMaterial[3], pMaterial[4], pMaterial[5], 1.0f
	};

	SHADERMANAGER.SetSpeedTreeMaterial(afMaterial);
}

void CSpeedTreeWrapper::SetPosition(float x, float y, float z)
{
	m_afPos[0] = x;
	m_afPos[1] = y;
	m_afPos[2] = z;
	m_pSpeedTree->SetTreePosition(x, y, z);
	CGraphicObjectInstance::SetPosition(x, y, z);
}

bool CSpeedTreeWrapper::GetBoundingSphere(Vector3& v3Center, float& fRadius)
{
	float fX, fY, fZ;

	fX = m_afBoundingBox[3] - m_afBoundingBox[0];
	fY = m_afBoundingBox[4] - m_afBoundingBox[1];
	fZ = m_afBoundingBox[5] - m_afBoundingBox[2];

	v3Center.x = 0.0f;
	v3Center.y = 0.0f;
	v3Center.z = fZ * 0.5f;

	fRadius = sqrtf(fX * fX + fY * fY + fZ * fZ) * 0.5f * 0.9f; // 0.9f for reduce size

	Vector3 vec = m_pSpeedTree->GetTreePosition();

	v3Center += vec;

	return true;
}

void CSpeedTreeWrapper::CalculateBBox()
{
	float fX, fY, fZ;

	fX = m_afBoundingBox[3] - m_afBoundingBox[0];
	fY = m_afBoundingBox[4] - m_afBoundingBox[1];
	fZ = m_afBoundingBox[5] - m_afBoundingBox[2];

	m_v3BBoxMin.x = -fX / 2.0f;
	m_v3BBoxMin.y = -fY / 2.0f;
	m_v3BBoxMin.z = 0.0f;
	m_v3BBoxMax.x = fX / 2.0f;
	m_v3BBoxMax.y = fY / 2.0f;
	m_v3BBoxMax.z = fZ;

	m_v4TBBox[0] = Vector4(m_v3BBoxMin.x, m_v3BBoxMin.y, m_v3BBoxMin.z, 1.0f);
	m_v4TBBox[1] = Vector4(m_v3BBoxMin.x, m_v3BBoxMax.y, m_v3BBoxMin.z, 1.0f);
	m_v4TBBox[2] = Vector4(m_v3BBoxMax.x, m_v3BBoxMin.y, m_v3BBoxMin.z, 1.0f);
	m_v4TBBox[3] = Vector4(m_v3BBoxMax.x, m_v3BBoxMax.y, m_v3BBoxMin.z, 1.0f);
	m_v4TBBox[4] = Vector4(m_v3BBoxMin.x, m_v3BBoxMin.y, m_v3BBoxMax.z, 1.0f);
	m_v4TBBox[5] = Vector4(m_v3BBoxMin.x, m_v3BBoxMax.y, m_v3BBoxMax.z, 1.0f);
	m_v4TBBox[6] = Vector4(m_v3BBoxMax.x, m_v3BBoxMin.y, m_v3BBoxMax.z, 1.0f);
	m_v4TBBox[7] = Vector4(m_v3BBoxMax.x, m_v3BBoxMax.y, m_v3BBoxMax.z, 1.0f);

	const Matrix& c_rmatTransform = GetMatrix();

	for (DWORD i = 0; i < 8; ++i)
	{
		Vec4Transform(&m_v4TBBox[i], &m_v4TBBox[i], &c_rmatTransform);
		if (0 == i)
		{
			m_v3TBBoxMin.x = m_v4TBBox[i].x;
			m_v3TBBoxMin.y = m_v4TBBox[i].y;
			m_v3TBBoxMin.z = m_v4TBBox[i].z;
			m_v3TBBoxMax.x = m_v4TBBox[i].x;
			m_v3TBBoxMax.y = m_v4TBBox[i].y;
			m_v3TBBoxMax.z = m_v4TBBox[i].z;
		}
		else
		{
			if (m_v3TBBoxMin.x > m_v4TBBox[i].x)
				m_v3TBBoxMin.x = m_v4TBBox[i].x;
			if (m_v3TBBoxMax.x < m_v4TBBox[i].x)
				m_v3TBBoxMax.x = m_v4TBBox[i].x;
			if (m_v3TBBoxMin.y > m_v4TBBox[i].y)
				m_v3TBBoxMin.y = m_v4TBBox[i].y;
			if (m_v3TBBoxMax.y < m_v4TBBox[i].y)
				m_v3TBBoxMax.y = m_v4TBBox[i].y;
			if (m_v3TBBoxMin.z > m_v4TBBox[i].z)
				m_v3TBBoxMin.z = m_v4TBBox[i].z;
			if (m_v3TBBoxMax.z < m_v4TBBox[i].z)
				m_v3TBBoxMax.z = m_v4TBBox[i].z;
		}
	}
}

// collision detection routines
UINT CSpeedTreeWrapper::GetCollisionObjectCount()
{
	assert(m_pSpeedTree);
	return m_pSpeedTree->GetCollisionObjectCount();
}

void CSpeedTreeWrapper::GetCollisionObject(UINT nIndex, CSpeedTreeRT::ECollisionObjectType& eType, float* pPosition, float* pDimensions)
{
	assert(m_pSpeedTree);
	m_pSpeedTree->GetCollisionObject(nIndex, eType, pPosition, pDimensions);
}

const float* CSpeedTreeWrapper::GetPosition()
{
	return m_afPos;
}

void CSpeedTreeWrapper::GetTreeSize(float& r_fSize, float& r_fVariance)
{
	m_pSpeedTree->GetTreeSize(r_fSize, r_fVariance);
}

// pscdVector may be null
void CSpeedTreeWrapper::OnUpdateCollisionData(const CStaticCollisionDataVector* /*pscdVector*/)
{
	Matrix mat;
	MatrixTranslation(&mat, m_afPos[0], m_afPos[1], m_afPos[2]);

	/////
	for (UINT i = 0; i < GetCollisionObjectCount(); ++i)
	{
		CSpeedTreeRT::ECollisionObjectType ObjectType;
		CStaticCollisionData CollisionData;

		GetCollisionObject(i, ObjectType, (float*)&CollisionData.v3Position, CollisionData.fDimensions);

		if (ObjectType == CSpeedTreeRT::CO_BOX)
			continue;

		switch (ObjectType)
		{
		case CSpeedTreeRT::CO_SPHERE:
			CollisionData.dwType = COLLISION_TYPE_SPHERE;
			CollisionData.fDimensions[0] = CollisionData.fDimensions[0] /** fSizeRatio*/;
			//AddCollision(&CollisionData);
			break;

		case CSpeedTreeRT::CO_CYLINDER:
			CollisionData.dwType = COLLISION_TYPE_CYLINDER;
			CollisionData.fDimensions[0] = CollisionData.fDimensions[0] /** fSizeRatio*/;
			CollisionData.fDimensions[1] = CollisionData.fDimensions[1] /** fSizeRatio*/;
			//AddCollision(&CollisionData);
			break;

			/*case CSpeedTreeRT::CO_BOX:
			break;*/
		}
		AddCollision(&CollisionData, &mat);
	}
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec