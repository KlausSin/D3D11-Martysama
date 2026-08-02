#include "StdAfx.h"
#include "VTFInstanceManager.h"
#include "ShaderManager.h"
#include "ShaderInit.h"
#include "../EterGrnLib/Model.h"
#include "../EterGrnLib/ModelInstance.h"
#include "../EterGrnLib/Mesh.h"
#include "../EterGrnLib/Material.h"

ID3D11DeviceContext* CVTFInstanceManager::GetActiveContext() const
{
	return m_pContext;
}

// Thread-local batch state definition
thread_local CVTFInstanceManager::SBatchState CVTFInstanceManager::s_batchState;

CVTFInstanceManager::CVTFInstanceManager()
	: m_pDevice(nullptr)
	, m_pContext(nullptr)
	, m_bInitialized(false)
	, m_pTexture(nullptr)
	, m_pSRV(nullptr)
{
}

CVTFInstanceManager::~CVTFInstanceManager()
{
	Shutdown();
}

bool CVTFInstanceManager::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	if (m_bInitialized)
		return true;

	if (!pDevice || !pContext)
		return false;

	m_pDevice = pDevice;
	m_pContext = pContext;

	// Create the VTF texture: R32G32B32A32_FLOAT format
	// Width = VTF_TEXTURE_WIDTH (4096), Height = 1
	// Each instance uses 4 consecutive texels
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = VTF_TEXTURE_WIDTH;
	texDesc.Height = VTF_TEXTURE_HEIGHT;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DYNAMIC;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = m_pDevice->CreateTexture2D(&texDesc, nullptr, &m_pTexture);
	if (FAILED(hr))
	{
		TraceError("CVTFInstanceManager::Initialize - Failed to create VTF texture (hr=0x%08X)", hr);
		return false;
	}

	// Create SRV
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	hr = m_pDevice->CreateShaderResourceView(m_pTexture, &srvDesc, &m_pSRV);
	if (FAILED(hr))
	{
		TraceError("CVTFInstanceManager::Initialize - Failed to create VTF SRV (hr=0x%08X)", hr);
		m_pTexture->Release();
		m_pTexture = nullptr;
		return false;
	}

	m_bInitialized = true;
	Tracef("CVTFInstanceManager initialized: %dx%d R32G32B32A32_FLOAT (max %d instances)\n",
		VTF_TEXTURE_WIDTH, VTF_TEXTURE_HEIGHT, VTF_MAX_INSTANCES);
	return true;
}

void CVTFInstanceManager::Shutdown()
{
	if (m_pSRV)
	{
		m_pSRV->Release();
		m_pSRV = nullptr;
	}
	if (m_pTexture)
	{
		m_pTexture->Release();
		m_pTexture = nullptr;
	}
	m_bInitialized = false;
}

void CVTFInstanceManager::BeginBatch()
{
	s_batchState.instanceData.clear();
	s_batchState.uInstanceCount = 0;
}

int CVTFInstanceManager::AddInstance(const Matrix* pWorld, float fDiffuseR, float fDiffuseG,
	float fDiffuseB, float fDiffuseA)
{
	XMFLOAT4 vParams(fDiffuseR, fDiffuseG, fDiffuseB, fDiffuseA);
	return AddInstance(pWorld, vParams);
}

int CVTFInstanceManager::AddInstance(const Matrix* pWorld, const XMFLOAT4& vParams)
{
	if (s_batchState.uInstanceCount >= VTF_MAX_INSTANCES)
		return -1;

	SVTFInstanceData data;

	// Store full 4x4 world matrix (row-major in memory)
	// Row 3 (m[12]-m[15]) contains translation (tx, ty, tz, 1)
	const float* m = reinterpret_cast<const float*>(pWorld);
	data.matWorldRow0 = XMFLOAT4(m[0], m[1], m[2], m[3]);
	data.matWorldRow1 = XMFLOAT4(m[4], m[5], m[6], m[7]);
	data.matWorldRow2 = XMFLOAT4(m[8], m[9], m[10], m[11]);
	data.matWorldRow3 = XMFLOAT4(m[12], m[13], m[14], m[15]);

	int idx = static_cast<int>(s_batchState.uInstanceCount);
	s_batchState.instanceData.push_back(data);
	s_batchState.uInstanceCount++;
	return idx;
}

bool CVTFInstanceManager::CommitAndBind(UINT vsSlot)
{
	if (!m_bInitialized || s_batchState.uInstanceCount == 0)
		return false;

	// Map the texture and write instance data
	// MAP_WRITE_DISCARD on deferred contexts creates independent memory per command list — safe for concurrent use
	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = GetActiveContext()->Map(m_pTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	if (FAILED(hr))
	{
		TraceError("CVTFInstanceManager::CommitAndBind - Map failed (hr=0x%08X)", hr);
		return false;
	}

	XMFLOAT4* pTexels = reinterpret_cast<XMFLOAT4*>(mapped.pData);

	for (UINT i = 0; i < s_batchState.uInstanceCount; ++i)
	{
		const SVTFInstanceData& inst = s_batchState.instanceData[i];
		UINT baseTexel = i * VTF_TEXELS_PER_INSTANCE;
		pTexels[baseTexel + 0] = inst.matWorldRow0;
		pTexels[baseTexel + 1] = inst.matWorldRow1;
		pTexels[baseTexel + 2] = inst.matWorldRow2;
		pTexels[baseTexel + 3] = inst.matWorldRow3;
	}

	GetActiveContext()->Unmap(m_pTexture, 0);

	// Bind SRV to vertex shader
	GetActiveContext()->VSSetShaderResources(vsSlot, 1, &m_pSRV);

	return true;
}

void CVTFInstanceManager::DrawIndexedInstanced(D3D11_PRIMITIVE_TOPOLOGY topology,
	UINT indexCountPerInstance, UINT startIndexLocation, INT baseVertexLocation)
{
	if (!m_bInitialized || s_batchState.uInstanceCount == 0)
		return;

	SHADERMANAGER.SetPrimitiveTopologyIfChanged(topology);
	GetActiveContext()->DrawIndexedInstanced(indexCountPerInstance, s_batchState.uInstanceCount,
		startIndexLocation, baseVertexLocation, 0);
}

void CVTFInstanceManager::DrawInstanced(D3D11_PRIMITIVE_TOPOLOGY topology,
	UINT vertexCountPerInstance, UINT startVertexLocation)
{
	if (!m_bInitialized || s_batchState.uInstanceCount == 0)
		return;

	SHADERMANAGER.SetPrimitiveTopologyIfChanged(topology);
	GetActiveContext()->DrawInstanced(vertexCountPerInstance, s_batchState.uInstanceCount,
		startVertexLocation, 0);
}

void CVTFInstanceManager::Unbind(UINT vsSlot)
{
	if (GetActiveContext())
	{
		ID3D11ShaderResourceView* pNull = nullptr;
		GetActiveContext()->VSSetShaderResources(vsSlot, 1, &pNull);
	}
}

// ===============================================================
// Deferred Rigid Mesh Batching
// ===============================================================

void CVTFInstanceManager::ClearDeferredRigidBatches()
{
	m_deferredRigidBatches.clear();
	m_frameStats = {};  // reset per-frame stats
}

bool CVTFInstanceManager::DeferRigidModelInstance(CGrannyModel* pModel, CGrannyModelInstance* pModelInst)
{
	if (!m_bInitialized || !pModel || !pModelInst)
		return false;

	ID3D11ShaderResourceView* pFirstTex = nullptr;
	CGrannyMaterialPalette& rkMtrlPal = pModelInst->GetMaterialPaletteRef();
	if (rkMtrlPal.GetMaterialCount() > 0)
		pFirstTex = rkMtrlPal.GetMaterialRef(0).GetD3DTexture(0);

	SDeferredBatchKey key;
	key.pModel = pModel;
	key.pFirstTexture = pFirstTex;

	SDeferredRigidInstance inst;
	inst.pModelInst = pModelInst;
	m_deferredRigidBatches[key].push_back(inst);
	return true;
}

void CVTFInstanceManager::FlushDeferredRigidBatches()
{
	if (!m_bInitialized || m_deferredRigidBatches.empty())
		return;

	for (auto& groupPair : m_deferredRigidBatches)
	{
		CGrannyModel* pModel = groupPair.first.pModel;
		std::vector<SDeferredRigidInstance>& instances = groupPair.second;

		m_frameStats.groups++;

		if (instances.size() < 3)
		{
			m_frameStats.fallbackInstances += (UINT)instances.size();
			for (auto& inst : instances)
			{
				inst.pModelInst->RenderWithOneTexture();
			}
			continue;
		}

		// VTF batch render this group
		ID3D11Buffer* pRigidVB = pModel->GetPNTD3DVertexBuffer();
		ID3D11Buffer* pIdxBuf = pModel->GetD3DIndexBuffer();
		if (!pRigidVB || !pIdxBuf)
		{
			// Fallback: render individually
			m_frameStats.fallbackInstances += (UINT)instances.size();
			for (auto& inst : instances)
				inst.pModelInst->RenderWithOneTexture();
			continue;
		}

		m_frameStats.vtfBatchedInstances += (UINT)instances.size();

		CGrannyMaterialPalette& rkMtrlPal = instances[0].pModelInst->GetMaterialPaletteRef();

		// Set up VTF mesh shader
		SHADERMANAGER.BeginMeshVTF();
		SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
		SHADERMANAGER.SetAlphaTest(false, 0.0f);
		SHADERMANAGER.SetTwoTextureBlend(false);
		SHADERMANAGER.CommitChanges();
		SHADERMANAGER.SetVertexBuffer(0, pRigidVB, sizeof(TPNTVertex));
		SHADERMANAGER.SetIndexBuffer(pIdxBuf, DXGI_FORMAT_R16_UINT, 0);

		// Iterate over DIFFUSE_PNT rigid mesh nodes
		const CGrannyModel::TMeshNode* pMeshNode = pModel->GetMeshNodeList(
			CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_DIFFUSE_PNT);

		while (pMeshNode)
		{
			const CGrannyMesh* pMesh = pMeshNode->pMesh;
			int iMeshIdx = pMeshNode->iMesh;
			int vtxBasePos = pMesh->GetVertexBasePosition();

			// Upload per-instance mesh matrices
			BeginBatch();
			for (auto& inst : instances)
			{
				const Matrix* pMeshMatrices = inst.pModelInst->GetMeshMatricesPtr();
				if (pMeshMatrices)
					AddInstance(&pMeshMatrices[iMeshIdx]);
			}
			if (!CommitAndBind())
			{
				Unbind();
				pMeshNode = pMeshNode->pNextMeshNode;
				continue;
			}

			// Render each triangle group with its material
			const CGrannyMesh::TTriGroupNode* pTriGroupNode = pMesh->GetTriGroupNodeList(
				CGrannyMaterial::TYPE_DIFFUSE_PNT);

			while (pTriGroupNode)
			{
				CGrannyMaterial& rkMtrl = rkMtrlPal.GetMaterialRef(pTriGroupNode->mtrlIndex);
				rkMtrl.ApplyRenderState();

				SHADERMANAGER.DrawIndexedInstanced(
					TOPOLOGY_TRIANGLELIST,
					pTriGroupNode->triCount * 3,
					GetInstanceCount(),
					pTriGroupNode->idxPos,
					vtxBasePos,
					0);
				m_frameStats.vtfDraws++;

				rkMtrl.RestorePipelineState();
				pTriGroupNode = pTriGroupNode->pNextTriGroupNode;
			}

			Unbind();
			pMeshNode = pMeshNode->pNextMeshNode;
		}


		SHADERMANAGER.End();
	}

	m_deferredRigidBatches.clear();
}

// ===============================================================
// Shadow-pass Deferred Rigid Mesh Batching
// ===============================================================
void CVTFInstanceManager::ClearDeferredShadowBatches()
{
	m_deferredShadowBatches.clear();
}

bool CVTFInstanceManager::DeferRigidModelInstanceForShadow(CGrannyModel* pModel, CGrannyModelInstance* pModelInst)
{
	if (!m_bInitialized || !pModel || !pModelInst)
		return false;
	SDeferredRigidInstance inst;
	inst.pModelInst = pModelInst;
	m_deferredShadowBatches[pModel].push_back(inst);
	return true;
}

void CVTFInstanceManager::FlushDeferredShadowBatches()
{
	if (!m_bInitialized || m_deferredShadowBatches.empty())
		return;

	SHADERMANAGER.BeginShadowVTF();
	SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
	SHADERMANAGER.SetAlphaTest(false, 0.0f);
	SHADERMANAGER.SetTwoTextureBlend(false);
	SHADERMANAGER.CommitChanges();

	for (auto& groupPair : m_deferredShadowBatches)
	{
		CGrannyModel* pModel = groupPair.first;
		std::vector<SDeferredRigidInstance>& instances = groupPair.second;
		if (instances.empty())
			continue;

		ID3D11Buffer* pRigidVB = pModel->GetPNTD3DVertexBuffer();
		ID3D11Buffer* pIdxBuf = pModel->GetD3DIndexBuffer();
		if (!pRigidVB || !pIdxBuf)
			continue;

		SHADERMANAGER.SetVertexBuffer(0, pRigidVB, sizeof(TPNTVertex));
		SHADERMANAGER.SetIndexBuffer(pIdxBuf, DXGI_FORMAT_R16_UINT, 0);

		const int meshCount = pModel->GetMeshCount();

		const CGrannyModel::TMeshNode* pMeshNode = pModel->GetMeshNodeList(
			CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_DIFFUSE_PNT);

		while (pMeshNode)
		{
			const CGrannyMesh* pMesh = pMeshNode->pMesh;
			int iMeshIdx = pMeshNode->iMesh;
			int vtxBasePos = pMesh->GetVertexBasePosition();

			if (iMeshIdx < 0 || iMeshIdx >= meshCount)
			{
				pMeshNode = pMeshNode->pNextMeshNode;
				continue;
			}

			BeginBatch();
			for (auto& inst : instances)
			{
				const Matrix* pMeshMatrices = inst.pModelInst->GetMeshMatricesPtr();
				if (pMeshMatrices)
					AddInstance(&pMeshMatrices[iMeshIdx]);
			}
			if (!CommitAndBind())
			{
				Unbind();
				pMeshNode = pMeshNode->pNextMeshNode;
				continue;
			}

			const CGrannyMesh::TTriGroupNode* pTriGroupNode = pMesh->GetTriGroupNodeList(
				CGrannyMaterial::TYPE_DIFFUSE_PNT);

			while (pTriGroupNode)
			{
				SHADERMANAGER.DrawIndexedInstanced(
					TOPOLOGY_TRIANGLELIST,
					pTriGroupNode->triCount * 3,
					GetInstanceCount(),
					pTriGroupNode->idxPos,
					vtxBasePos,
					0);
				m_frameStats.vtfDraws++;

				pTriGroupNode = pTriGroupNode->pNextTriGroupNode;
			}

			Unbind();
			pMeshNode = pMeshNode->pNextMeshNode;
		}
	}

	SHADERMANAGER.End();

}
