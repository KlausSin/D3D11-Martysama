#pragma once

#include "../eterLib/ShaderManager.h"


struct GPUPTVertex
{
	float x, y, z;
	float u, v;
};

class CGpuParticlePool
{
public:
	static CGpuParticlePool& Instance()
	{
		static CGpuParticlePool s_instance;
		return s_instance;
	}

	static const UINT MAX_GPU_PARTICLES = 16384;
	static const UINT VERTEX_SIZE = 20;  // sizeof(GPUPTVertex)
	static const UINT VERTS_PER_PARTICLE = 4;
	static const UINT INDICES_PER_PARTICLE = 6;

	CGpuParticlePool();
	~CGpuParticlePool();

	bool IsAvailable();

	// Per-texture-frame batch interface
	void BeginBatch();
	bool AddQuad(const GPUPTVertex verts[4]);  // Add pre-computed quad
	UINT GetBatchCount() const { return m_dwBatchCount; }
	void FlushBatch();  // Upload + single DrawIndexed

	void Shutdown();

private:
	bool Initialize();
	bool CreateResources();
	bool CreateIndexBuffer();

	// CPU staging (4 vertices per quad)
	std::vector<GPUPTVertex> m_StagingBuffer;
	UINT m_dwBatchCount;  // quad count

	// GPU resources
	ID3D11Buffer* m_pDynamicVB;   // DYNAMIC vertex buffer for PT vertices
	ID3D11Buffer* m_pIndexBuffer; // Pre-built quad IB (32-bit indices)

	bool m_bInitialized;
	bool m_bInitAttempted;
};

#define GPU_PARTICLE_POOL CGpuParticlePool::Instance()
