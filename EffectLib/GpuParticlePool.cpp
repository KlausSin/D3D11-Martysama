#include "StdAfx.h"
#include "../EterLib/ShaderManager.h"
#include "GpuParticlePool.h"


CGpuParticlePool::CGpuParticlePool()
	: m_dwBatchCount(0)
	, m_pDynamicVB(nullptr)
	, m_pIndexBuffer(nullptr)
	, m_bInitialized(false)
	, m_bInitAttempted(false)
{
	m_StagingBuffer.reserve(4096);
}

CGpuParticlePool::~CGpuParticlePool()
{
	Shutdown();
}

bool CGpuParticlePool::IsAvailable()
{
	if (!m_bInitAttempted)
	{
		m_bInitAttempted = true;
		Initialize();
	}
	return m_bInitialized;
}

bool CGpuParticlePool::Initialize()
{
	if (m_bInitialized)
		return true;

	if (!SHADERMANAGER.IsInitialized())
		return false;

	if (!CreateResources())
	{
		TraceError("CGpuParticlePool: Failed to create resources");
		return false;
	}

	if (!CreateIndexBuffer())
	{
		TraceError("CGpuParticlePool: Failed to create index buffer");
		return false;
	}

	m_bInitialized = true;
	Tracef("CGpuParticlePool: Initialized (max %d particles)\n", MAX_GPU_PARTICLES);
	return true;
}

void CGpuParticlePool::Shutdown()
{
	if (m_pDynamicVB) { m_pDynamicVB->Release(); m_pDynamicVB = nullptr; }
	if (m_pIndexBuffer) { m_pIndexBuffer->Release(); m_pIndexBuffer = nullptr; }
	m_StagingBuffer.clear();
	m_bInitialized = false;
	m_bInitAttempted = false;
}

bool CGpuParticlePool::CreateResources()
{
	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth = MAX_GPU_PARTICLES * VERTS_PER_PARTICLE * VERTEX_SIZE;
	vbDesc.Usage = D3D11_USAGE_DYNAMIC;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	return SUCCEEDED(SHADERMANAGER.GetDevice()->CreateBuffer(&vbDesc, nullptr, &m_pDynamicVB));
}

bool CGpuParticlePool::CreateIndexBuffer()
{
	std::vector<UINT> indices(MAX_GPU_PARTICLES * INDICES_PER_PARTICLE);
	const UINT pattern[6] = { 0, 2, 1, 2, 3, 1 };

	for (UINT i = 0; i < MAX_GPU_PARTICLES; ++i)
	{
		UINT base = i * 4;
		for (int j = 0; j < 6; ++j)
			indices[i * 6 + j] = base + pattern[j];
	}

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = (UINT)(indices.size() * sizeof(UINT));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = indices.data();

	return SUCCEEDED(SHADERMANAGER.GetDevice()->CreateBuffer(&desc, &initData, &m_pIndexBuffer));
}

void CGpuParticlePool::BeginBatch()
{
	m_StagingBuffer.clear();
	m_dwBatchCount = 0;
}

bool CGpuParticlePool::AddQuad(const GPUPTVertex verts[4])
{
	if (m_dwBatchCount >= MAX_GPU_PARTICLES)
		return false;

	m_StagingBuffer.push_back(verts[0]);
	m_StagingBuffer.push_back(verts[1]);
	m_StagingBuffer.push_back(verts[2]);
	m_StagingBuffer.push_back(verts[3]);
	m_dwBatchCount++;
	return true;
}

void CGpuParticlePool::FlushBatch()
{
	if (m_dwBatchCount == 0 || !m_bInitialized)
		return;

	auto* pCtx = SHADERMANAGER.GetActiveContext();
	if (!pCtx) return;

	// Upload staging buffer to dynamic VB
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (FAILED(pCtx->Map(m_pDynamicVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return;

	memcpy(mapped.pData, m_StagingBuffer.data(), m_dwBatchCount * VERTS_PER_PARTICLE * VERTEX_SIZE);
	pCtx->Unmap(m_pDynamicVB, 0);

	UINT stride = VERTEX_SIZE;
	UINT offset = 0;
	SHADERMANAGER.SetVertexBuffer(0, m_pDynamicVB, stride, offset);
	SHADERMANAGER.SetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Commit caller's render state and draw
	SHADERMANAGER.CommitRenderState();
	SHADERMANAGER.CommitChanges();
	SHADERMANAGER.SetPrimitiveTopologyIfChanged(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCtx->DrawIndexed(m_dwBatchCount * INDICES_PER_PARTICLE, 0, 0);
}
