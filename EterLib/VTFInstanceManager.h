#pragma once

/*
 * VTFInstanceManager.h
 * Vertex Texture Fetch (VTF) Instance Data Manager
 *
 * Packs per-instance data (world matrices, material params, texture indices)
 * into GPU textures that vertex shaders can sample via SV_InstanceID.
 * This enables batched rendering of many objects with a single draw call,
 * eliminating per-object state changes and constant buffer updates.
 *
 * DX11 / Shader Model 5.0 - Full vertex texture fetch support.
 */

#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include <unordered_map>
#include "GrpBase.h"
#include "../eterBase/Singleton.h"

// Forward declarations for deferred batch system
class CGrannyModel;
class CGrannyModelInstance;

using namespace DirectX;

static const UINT VTF_TEXELS_PER_INSTANCE = 4;

static const UINT VTF_MAX_INSTANCES = 1024;
static const UINT VTF_TEXTURE_WIDTH = VTF_MAX_INSTANCES * VTF_TEXELS_PER_INSTANCE;  // 4096
static const UINT VTF_TEXTURE_HEIGHT = 1;  // Single row texture

// Instance data structure (CPU side)
struct SVTFInstanceData
{
	XMFLOAT4 matWorldRow0;   // World matrix row 0 (m00, m01, m02, m03)
	XMFLOAT4 matWorldRow1;   // World matrix row 1 (m10, m11, m12, m13)
	XMFLOAT4 matWorldRow2;   // World matrix row 2 (m20, m21, m22, m23)
	XMFLOAT4 matWorldRow3;   // World matrix row 3 (tx, ty, tz, 1) - translation
};

class CVTFInstanceManager : public CSingleton<CVTFInstanceManager>
{
public:
	CVTFInstanceManager();
	~CVTFInstanceManager();

	bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	void Shutdown();
	bool IsInitialized() const { return m_bInitialized; }

	// Begin a new batch - clears instance data
	void BeginBatch();

	int AddInstance(const Matrix* pWorld, float fDiffuseR = 1.0f, float fDiffuseG = 1.0f,
		float fDiffuseB = 1.0f, float fDiffuseA = 1.0f);

	// Add an instance with custom params
	int AddInstance(const Matrix* pWorld, const XMFLOAT4& vParams);

	bool CommitAndBind(UINT vsSlot = 8);

	UINT GetInstanceCount() const { return s_batchState.uInstanceCount; }

	// Get the SRV for external binding
	ID3D11ShaderResourceView* GetSRV() const { return m_pSRV; }

	// Draw all instances using DrawIndexedInstanced
	void DrawIndexedInstanced(D3D11_PRIMITIVE_TOPOLOGY topology,
		UINT indexCountPerInstance, UINT startIndexLocation = 0,
		INT baseVertexLocation = 0);

	// Draw all instances using DrawInstanced (non-indexed)
	void DrawInstanced(D3D11_PRIMITIVE_TOPOLOGY topology,
		UINT vertexCountPerInstance, UINT startVertexLocation = 0);

	// Unbind the VTF texture from vertex shader
	void Unbind(UINT vsSlot = 8);


	// Info for a deferred rigid mesh instance
	struct SDeferredRigidInstance
	{
		CGrannyModelInstance* pModelInst;  // For accessing mesh matrices and material palette
	};

	struct SDeferredBatchKey
	{
		CGrannyModel* pModel;
		ID3D11ShaderResourceView* pFirstTexture;  // First diffuse texture as material fingerprint

		bool operator==(const SDeferredBatchKey& other) const
		{
			return pModel == other.pModel && pFirstTexture == other.pFirstTexture;
		}
	};

	struct SDeferredBatchKeyHash
	{
		size_t operator()(const SDeferredBatchKey& key) const
		{
			size_t h1 = std::hash<void*>()(key.pModel);
			size_t h2 = std::hash<void*>()(key.pFirstTexture);
			return h1 ^ (h2 << 1);
		}
	};

	// Call at the start of the character render pass
	void ClearDeferredRigidBatches();

	// Add a rigid model instance to the deferred batch
	// Returns true if successfully deferred
	bool DeferRigidModelInstance(CGrannyModel* pModel, CGrannyModelInstance* pModelInst);

	// Check if there are any deferred batches to flush
	bool HasDeferredRigidBatches() const { return !m_deferredRigidBatches.empty(); }

	void FlushDeferredRigidBatches();

	// Drop every deferred entry that points at this model instance. The batches hold RAW
	// CGrannyModelInstance* and are only cleared inside CPythonCharacterManager::Render(), so an
	// instance destroyed on any other screen (notably the create-character gender switch) could
	// stay referenced and be drawn again at its last transform — the "hair floating in the air".
	// Instances come from a CDynamicPool and are recycled, so a stale pointer can also resolve to a
	// DIFFERENT live instance. Called from CGrannyModelInstance::Clear() so it holds everywhere.
	void InvalidateModelInstance(const CGrannyModelInstance* pModelInst);

	size_t GetDeferredRigidCount() const;
	size_t GetDeferredShadowCount() const;

	void ClearDeferredShadowBatches();
	bool DeferRigidModelInstanceForShadow(CGrannyModel* pModel, CGrannyModelInstance* pModelInst);
	bool HasDeferredShadowBatches() const { return !m_deferredShadowBatches.empty(); }
	void FlushDeferredShadowBatches();

	// Thread-safe context accessor
	ID3D11DeviceContext* GetActiveContext() const;

	struct RigidBatchStats
	{
		UINT groups;              // total (model, texture) groups this frame
		UINT vtfBatchedInstances; // instances rendered via VTF path
		UINT fallbackInstances;   // instances rendered one-by-one (<3 per group)
		UINT vtfDraws;            // instanced draws issued
	};
	const RigidBatchStats& GetFrameStats() const { return m_frameStats; }
	void ResetFrameStats() { m_frameStats = {}; }

	struct SBatchState
	{
		std::vector<SVTFInstanceData> instanceData;
		UINT uInstanceCount;
		SBatchState() : uInstanceCount(0) { instanceData.reserve(VTF_MAX_INSTANCES); }
	};
	static thread_local SBatchState s_batchState;

private:
	ID3D11Device*             m_pDevice;
	ID3D11DeviceContext*      m_pContext;
	bool                      m_bInitialized;

	// GPU resources
	ID3D11Texture2D*          m_pTexture;
	ID3D11ShaderResourceView* m_pSRV;

	// Deferred rigid mesh batch data — keyed by (model + first texture) to prevent
	// different-textured instances from being batched together
	std::unordered_map<SDeferredBatchKey, std::vector<SDeferredRigidInstance>, SDeferredBatchKeyHash> m_deferredRigidBatches;

	std::unordered_map<CGrannyModel*, std::vector<SDeferredRigidInstance>> m_deferredShadowBatches;

	RigidBatchStats m_frameStats = {};
};

#define VTFMANAGER CVTFInstanceManager::Instance()
