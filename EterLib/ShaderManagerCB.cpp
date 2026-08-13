#include "StdAfx.h"
#include "ShaderManager.h"
#include "Camera.h"
#include "../eterBase/Debug.h"

bool CShaderManager::CreateConstantBuffers()
{
	if (!m_pCBPerFrame.Create(m_pDevice) ||
		!m_pCBPerObject.Create(m_pDevice) ||
		!m_pCBLighting.Create(m_pDevice) ||
		!m_pCBSpeedTree.Create(m_pDevice) ||
		!m_pCBSkinning.Create(m_pDevice) ||
		!m_pCBGodRays.Create(m_pDevice) ||
		!m_pCBSkyGradient.Create(m_pDevice) ||
		!m_pCBParticleCS.Create(m_pDevice) ||
		!m_pCBFlyTraceCS.Create(m_pDevice) ||
		!m_pCBWeaponTraceCS.Create(m_pDevice))
	{
		TraceError("CreateConstantBuffers: Failed to create constant buffers");
		return false;
	}

#ifdef ENABLE_BLOOM
	if (!m_pCBBloom.Create(m_pDevice))
	{
		TraceError("CreateConstantBuffers: Failed to create Bloom constant buffer");
		return false;
	}
#endif

#ifdef ENABLE_SSAO
	if (!m_pCBSSAO.Create(m_pDevice))
	{
		TraceError("CreateConstantBuffers: Failed to create SSAO constant buffer");
		return false;
	}
#endif

	for (UINT i = 0; i < SKINNING_CB_POOL_SIZE; ++i)
	{
		if (!m_pSkinningCBPool[i].Create(m_pDevice, D3D11_USAGE_DEFAULT))
		{
			TraceError("CreateConstantBuffers: Failed to create Skinning pool buffer %u", i);
			return false;
		}
	}

#ifdef ENABLE_SSAO
	m_cbSSAO.vSSAOParams = XMFLOAT4(0.5f, 0.025f, 1.5f, 0.0f);

	unsigned int seed = 12345u;
	auto nextRand = [&seed]()
		{
			seed = seed * 1103515245u + 12345u;
			return (float)(seed & 0x7FFFFFFFu) / (float)0x7FFFFFFFu;
		};

	for (int i = 0; i < SSAO_KERNEL_SIZE; ++i)
	{
		float x = nextRand() * 2.0f - 1.0f;
		float y = nextRand() * 2.0f - 1.0f;
		float z = nextRand();

		float len = sqrtf(x * x + y * y + z * z);
		if (len > 0.001f)
		{
			x /= len;
			y /= len;
			z /= len;
		}

		float scale = (float)i / (float)SSAO_KERNEL_SIZE;
		scale = 0.1f + scale * scale * 0.9f;

		m_cbSSAO.vSampleKernel[i] = XMFLOAT4(x * scale, y * scale, z * scale, 0.0f);
	}

	unsigned int noiseSeed = 54321u;
	auto noiseRand = [&noiseSeed]()
		{
			noiseSeed = noiseSeed * 1103515245u + 12345u;
			return (BYTE)((noiseSeed >> 16) & 0xFF);
		};

	BYTE noiseData[4 * 4 * 2];

	for (int i = 0; i < 16; ++i)
	{
		noiseData[i * 2] = noiseRand();
		noiseData[i * 2 + 1] = noiseRand();
	}

	D3D11_TEXTURE2D_DESC texDesc{};
	texDesc.Width = 4;
	texDesc.Height = 4;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_IMMUTABLE;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = noiseData;
	initData.SysMemPitch = 8;

	if (FAILED(m_pDevice->CreateTexture2D(&texDesc, &initData, &m_pSSAONoiseTex)))
	{
		TraceError("CreateConstantBuffers: Failed to create SSAO noise texture");
		return false;
	}

	if (FAILED(m_pDevice->CreateShaderResourceView(m_pSSAONoiseTex, nullptr, &m_pSSAONoiseSRV)))
	{
		TraceError("CreateConstantBuffers: Failed to create SSAO noise SRV");
		return false;
	}

	m_bSSAODirty = true;
#endif

	m_bSkyGradientDirty = false;

	for (int i = 0; i < MAX_BONES; ++i)
		m_cbSkinning.boneMatrices[i] = XMMatrixIdentity();

	m_cbGodRays.vLightScreenPos = XMFLOAT4(0.5f, 0.3f, 1.0f, 0.97f);
	m_cbGodRays.vRayParams = XMFLOAT4(0.5f, 0.5f, 0.3f, 64.0f);
	m_cbGodRays.vRayColor = XMFLOAT4(1.0f, 0.9f, 0.7f, 1.0f);

#ifdef ENABLE_BLOOM
	m_cbBloom.vBloomParams = XMFLOAT4(BLOOM_DEFAULT_THRESHOLD, BLOOM_DEFAULT_INTENSITY, 0.0f, 0.0f);
	m_cbBloom.vTexelSize = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	m_cbBloom.vBlurDirection = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
#endif

	return true;
}

void CShaderManager::SetSkyGradient(const float* pColors, int count, int upperSegments)
{
	if (!m_pCBSkyGradient.IsValid() || !m_pContext) return;
	count = min(count, SKY_GRADIENT_MAX_POINTS);
	for (int i = 0; i < count; ++i)
		m_cbSkyGradient.colors[i] = XMFLOAT4(pColors[i * 4], pColors[i * 4 + 1], pColors[i * 4 + 2], pColors[i * 4 + 3]);
	m_cbSkyGradient.colorCount = count;
	m_cbSkyGradient.upperSegments = upperSegments;
	m_bSkyGradientDirty = true;
}

void CShaderManager::SetViewProjection(const Matrix* pView, const Matrix* pProj)
{
	m_cbPerFrame.matView = XMMatrixTranspose(XMLoadFloat4x4((XMFLOAT4X4*)pView));
	m_cbPerFrame.matProjection = XMMatrixTranspose(XMLoadFloat4x4((XMFLOAT4X4*)pProj));
	m_bPerFrameDirty = true;
	m_Matrices[MATRIX_VIEW] = *pView;
	m_Matrices[MATRIX_PROJECTION] = *pProj;
}

void CShaderManager::SetCameraPosition(const Vector3* pCameraPos)
{
	if (pCameraPos) { m_cbPerFrame.vCameraPos.x = pCameraPos->x; m_cbPerFrame.vCameraPos.y = pCameraPos->y; m_cbPerFrame.vCameraPos.z = pCameraPos->z; }
	m_bPerFrameDirty = true;
}

void CShaderManager::SetFog(bool bEnabled, float fStart, float fEnd, DWORD dwColor)
{
	m_cbPerFrame.vFogParams.x = fStart;
	m_cbPerFrame.vFogParams.y = fEnd;
	m_cbPerFrame.vFogParams.w = bEnabled ? 1.0f : 0.0f;
	m_cbPerFrame.vFogColor = XMFLOAT4(((dwColor >> 16) & 0xFF) / 255.0f, ((dwColor >> 8) & 0xFF) / 255.0f, (dwColor & 0xFF) / 255.0f, 1.0f);
	m_bPerFrameDirty = true;
}

void CShaderManager::SetAmbient(const Color* pColor)
{
	if (pColor)
		SetGlobalAmbient(pColor->r, pColor->g, pColor->b, 1.0f);
}

void CShaderManager::SetSunDirection(float x, float y, float z, float intensity)
{
	float len = sqrtf(x * x + y * y + z * z);
	if (len > 0.0001f)
	{
		x /= len;
		y /= len;
		z /= len;
	}
	m_cbPerFrame.vSunDirection = XMFLOAT4(x, y, z, intensity);
	m_bPerFrameDirty = true;
}

void CShaderManager::SetShadowTexelSize(float fTexelSize)
{
	m_cbPerFrame.vShadowParams.z = fTexelSize;
	m_bPerFrameDirty = true;
}

void CShaderManager::SetReflectionClipZ(float fWaterZ)
{
	m_cbPerFrame.vShadowParams.y = fWaterZ;
	m_bPerFrameDirty = true;
}

void CShaderManager::SetShadowMidFarMatrices(const Matrix* pMid, const Matrix* pFar)
{
	if (pMid)
		m_cbPerFrame.matShadowMid = XMMatrixTranspose(XMLoadFloat4x4((const XMFLOAT4X4*)pMid));
	if (pFar)
		m_cbPerFrame.matShadowFar = XMMatrixTranspose(XMLoadFloat4x4((const XMFLOAT4X4*)pFar));
	m_bPerFrameDirty = true;
}

void CShaderManager::SetLightingEnabled(bool bEnabled)
{
	m_bLightingEnabled = bEnabled;
	// Enable/disable light 0 (the primary directional light)
	EnableLight(0, bEnabled);
}

void CShaderManager::SetWorldMatrix(const Matrix* pWorld)
{
	XMMATRIX matWorld = XMLoadFloat4x4((XMFLOAT4X4*)pWorld);
	XMMATRIX matWVP;
	matWVP = matWorld * XMMatrixTranspose(m_cbPerFrame.matView) * XMMatrixTranspose(m_cbPerFrame.matProjection);
	m_cbPerObject.matWorld = XMMatrixTranspose(matWorld);
	m_cbPerObject.matWorldViewProj = XMMatrixTranspose(matWVP);
	m_bPerObjectDirty = true;
}

void CShaderManager::SetAlphaTest(bool bEnabled, float fRef)
{
	float fEnabledVal = bEnabled ? 1.0f : 0.0f;
	if (m_cbPerObject.vMaterialParams.x == fRef &&
		m_cbPerObject.vMaterialParams.y == fEnabledVal)
		return;
	m_cbPerObject.vMaterialParams.x = fRef;
	m_cbPerObject.vMaterialParams.y = fEnabledVal;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetTextureColorSwap(bool bEnabled)
{
	float v = bEnabled ? 1.0f : 0.0f;
	if (m_cbPerObject.vMaterialParams.z == v) return;
	m_cbPerObject.vMaterialParams.z = v;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetSpecularColor(float r, float g, float b)
{
	XMFLOAT4& cur = m_cbPerObject.vSpecularColor;
	if (cur.x == r && cur.y == g && cur.z == b) return;
	cur.x = r; cur.y = g; cur.z = b;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetEmissiveColor(float r, float g, float b)
{
	XMFLOAT4& cur = m_cbPerObject.vEmissiveColor;
	if (cur.x == r && cur.y == g && cur.z == b) return;
	cur = XMFLOAT4(r, g, b, 0.0f);
	m_bPerObjectDirty = true;
}

bool CShaderManager::IsTwoTextureBlendEnabled() const
{
	return m_bTwoTextureBlend;
}

void CShaderManager::SetMaterialParams(float x, float y, float z, float w)
{
	XMFLOAT4& cur = m_cbPerObject.vMaterialParams;
	if (cur.x == x && cur.y == y && cur.z == z && cur.w == w)
		return;
	cur.x = x;
	cur.y = y;
	cur.z = z;
	cur.w = w;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetCharacterShadowPass(bool bEnabled)
{
	float v = bEnabled ? 1.0f : 0.0f;
	if (m_cbPerObject.vRenderFlags.x == v)
		return;
	m_cbPerObject.vRenderFlags.x = v;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetMeshTextureAlphaEnabled(bool bEnabled)
{
	const float v = bEnabled ? 1.0f : 0.0f;
	if (m_cbPerObject.vRenderFlags.y == v)
		return;

	m_cbPerObject.vRenderFlags.y = v;
	m_bPerObjectDirty = true;
}

void CShaderManager::CommitChanges()
{
	if (!m_pContext)
		return;

	if (m_bPerFrameDirty)
	{
		if (m_pCBPerFrame.Update(m_pContext, m_cbPerFrame))
		{
			m_pCBPerFrame.BindVSPS(m_pContext, 0);
			m_bPerFrameDirty = false;
		}
	}

	if (m_bPerObjectDirty)
	{
		if (m_pCBPerObject.Update(m_pContext, m_cbPerObject))
		{
			m_pCBPerObject.BindVSPS(m_pContext, 1);
			m_bPerObjectDirty = false;
		}
	}

	if (m_bLightingDirty)
	{
		if (m_pCBLighting.Update(m_pContext, m_cbLighting))
		{
			m_pCBLighting.BindVSPS(m_pContext, 2);
			m_bLightingDirty = false;
		}
	}

	if (m_bSpeedTreeDirty && (m_eCurrentShader == SHADER_SPEEDTREE || m_eCurrentShader == SHADER_SPEEDTREE_LEAF))
	{
		if (m_pCBSpeedTree.Update(m_pContext, m_cbSpeedTree))
		{
			m_pCBSpeedTree.BindVS(m_pContext, 3);
			m_bSpeedTreeDirty = false;
		}
	}

	if (m_bSkinningDirty && (m_eCurrentShader == SHADER_MESH_SKINNED || m_eCurrentShader == SHADER_SHADOW_SKINNED))
	{
		UINT i = m_dwSkinningPoolIndex;

		if (i < SKINNING_CB_POOL_SIZE && m_pSkinningCBPool[i].Update(m_pContext, m_cbSkinning))
		{
			m_pSkinningCBPool[i].BindVS(m_pContext, 3);
			++m_dwSkinningPoolIndex;
			m_bSkinningDirty = false;
		}
		else if (m_pCBSkinning.Update(m_pContext, m_cbSkinning))
		{
			m_pCBSkinning.BindVS(m_pContext, 3);
			m_bSkinningDirty = false;
		}
	}

	if (m_bSkyGradientDirty && m_eCurrentShader == SHADER_SKY)
	{
		if (m_pCBSkyGradient.Update(m_pContext, m_cbSkyGradient))
		{
			m_pCBSkyGradient.BindPS(m_pContext, 2);
			m_bSkyGradientDirty = false;
		}
	}
}

void CShaderManager::SetLight(UINT index, const DX11Light& light)
{
	if (index >= MAX_SHADER_LIGHTS) return;


	m_cbLighting.lights[index] = light;
	m_bLightingDirty = true;

	// Update active light count
	int numActive = 0;
	for (int i = 0; i < MAX_SHADER_LIGHTS; ++i)
	{
		if (m_cbLighting.lights[i].Direction.w > 0.5f)
			++numActive;
	}
	m_cbLighting.numActiveLights = numActive;
}

void CShaderManager::GetLight(UINT index, TLight* pLight) const
{
	if (index >= MAX_SHADER_LIGHTS || !pLight) return;

	const DX11Light& src = m_cbLighting.lights[index];

	// Convert DX11Light back to TLight format
	pLight->Type = (ELightType)(int)src.Position.w;
	pLight->Position.x = src.Position.x;
	pLight->Position.y = src.Position.y;
	pLight->Position.z = src.Position.z;
	pLight->Direction.x = src.Direction.x;
	pLight->Direction.y = src.Direction.y;
	pLight->Direction.z = src.Direction.z;
	pLight->Diffuse.r = src.Color.x;
	pLight->Diffuse.g = src.Color.y;
	pLight->Diffuse.b = src.Color.z;
	pLight->Diffuse.a = src.Color.w;
	pLight->Attenuation0 = src.Attenuation.x;
	pLight->Attenuation1 = src.Attenuation.y;
	pLight->Attenuation2 = src.Attenuation.z;
	pLight->Range = src.Attenuation.w;
	// Initialize other fields to defaults
	pLight->Ambient = Color(0.0f, 0.0f, 0.0f, 1.0f);
	pLight->Specular = Color(0.0f, 0.0f, 0.0f, 1.0f);
	pLight->Falloff = 1.0f;
	pLight->Theta = 0.0f;
	pLight->Phi = 0.0f;
}

void CShaderManager::EnableLight(UINT index, bool bEnable)
{
	if (index >= MAX_SHADER_LIGHTS) return;


	m_cbLighting.lights[index].Direction.w = bEnable ? 1.0f : 0.0f;
	m_bLightingDirty = true;

	// Update active light count
	int numActive = 0;
	for (int i = 0; i < MAX_SHADER_LIGHTS; ++i)
	{
		if (m_cbLighting.lights[i].Direction.w > 0.5f)
			++numActive;
	}
	m_cbLighting.numActiveLights = numActive;
}

void CShaderManager::SetGlobalAmbient(float r, float g, float b, float a)
{
	SetGlobalAmbient(XMFLOAT4(r, g, b, a));
}

void CShaderManager::SetFogEnabled(bool bEnabled)
{
	m_bFogEnabled = bEnabled;
	m_cbPerFrame.vFogParams.w = bEnabled ? 1.0f : 0.0f;
	m_bPerFrameDirty = true;
	ReapplyVolFogDials();
}

void CShaderManager::SetFogParams(float fStart, float fEnd, DWORD dwColor)
{
	m_cbPerFrame.vFogParams.x = fStart;
	m_cbPerFrame.vFogParams.y = fEnd;
	m_bPerFrameDirty = true;
	SetFogColor(dwColor);
	ReapplyVolFogDials();
}

void CShaderManager::SetAlphaTestEnabled(bool bEnabled)
{
	m_bAlphaTestEnabled = bEnabled;
	m_cbPerObject.vMaterialParams.y = bEnabled ? 1.0f : 0.0f;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetMaterial(const TMaterial* pMaterial)
{
	if (!pMaterial) return;

	m_CurrentMaterial = *pMaterial;

	SetDiffuseColor(pMaterial->Diffuse.r, pMaterial->Diffuse.g, pMaterial->Diffuse.b, pMaterial->Diffuse.a);
	SetSpecularColor(pMaterial->Specular.r, pMaterial->Specular.g, pMaterial->Specular.b);
	SetEmissiveColor(pMaterial->Emissive.r, pMaterial->Emissive.g, pMaterial->Emissive.b);
	SetMaterial(pMaterial->Power);
}

void CShaderManager::SetLight(UINT index, const TLight* pLight)
{
	if (!pLight || index >= MAX_SHADER_LIGHTS) return;

	DX11Light dx11Light;
	dx11Light.Position = XMFLOAT4(pLight->Position.x, pLight->Position.y, pLight->Position.z, (float)pLight->Type);
	dx11Light.Direction = XMFLOAT4(pLight->Direction.x, pLight->Direction.y, pLight->Direction.z,
		m_cbLighting.lights[index].Direction.w);  // Preserve enabled state
	dx11Light.Color = XMFLOAT4(pLight->Diffuse.r, pLight->Diffuse.g, pLight->Diffuse.b, 1.0f);
	dx11Light.Attenuation = XMFLOAT4(pLight->Attenuation0, pLight->Attenuation1, pLight->Attenuation2, pLight->Range);

	SetLight(index, dx11Light);
}

void CShaderManager::SetBoneMatrices(const Matrix* pMatrices, int count)
{
	if (!pMatrices || count <= 0)
		return;

	// Clamp to maximum bones
	if (count > MAX_BONES)
		count = MAX_BONES;

	const size_t bytes = (size_t)count * sizeof(XMMATRIX);

	memcpy(m_cbSkinning.boneMatrices, pMatrices, bytes);
	static thread_local int s_lastBoneCount = 0;
	const int fillEnd = (s_lastBoneCount > count) ? s_lastBoneCount : count;
	for (int i = count; i < fillEnd; ++i)
		m_cbSkinning.boneMatrices[i] = XMMatrixIdentity();
	s_lastBoneCount = count;
	m_iActiveBoneCount = count;
	m_bSkinningDirty = true;
}

void CShaderManager::SyncPerFrameToContext(ID3D11DeviceContext* pDeferredCtx, ID3D11Buffer* pCBPerFrame)
{
	if (!pDeferredCtx || !pCBPerFrame)
		return;

	// Map and copy the per-frame constant buffer data
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(pDeferredCtx->Map(pCBPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &m_cbPerFrame, sizeof(CBPerFrame));
		pDeferredCtx->Unmap(pCBPerFrame, 0);
	}

	// Bind the constant buffer to the deferred context
	pDeferredCtx->VSSetConstantBuffers(0, 1, &pCBPerFrame);
	pDeferredCtx->PSSetConstantBuffers(0, 1, &pCBPerFrame);
}

void CShaderManager::SyncAllConstantBuffers(ID3D11DeviceContext* pDeferredCtx,
	ID3D11Buffer* pCBPerFrame, ID3D11Buffer* pCBPerObject,
	ID3D11Buffer* pCBLighting, ID3D11Buffer* pCBSkinning)
{
	if (!pDeferredCtx)
		return;

	// Sync per-frame data
	if (pCBPerFrame)
	{
		SyncPerFrameToContext(pDeferredCtx, pCBPerFrame);
	}

	// Sync per-object data (initial state)
	if (pCBPerObject)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(pDeferredCtx->Map(pCBPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbPerObject, sizeof(CBPerObject));
			pDeferredCtx->Unmap(pCBPerObject, 0);
		}
		pDeferredCtx->VSSetConstantBuffers(1, 1, &pCBPerObject);
		pDeferredCtx->PSSetConstantBuffers(1, 1, &pCBPerObject);
	}

	// Sync lighting data
	if (pCBLighting)
	{
		SyncLightingToContext(pDeferredCtx, pCBLighting);
	}

	if (pCBSkinning)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(pDeferredCtx->Map(pCBSkinning, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbSkinning, sizeof(CBSkinning));
			pDeferredCtx->Unmap(pCBSkinning, 0);
		}
		pDeferredCtx->VSSetConstantBuffers(4, 1, &pCBSkinning);
	}
}

void CShaderManager::UpdatePerObjectOnContext(ID3D11DeviceContext* pDeferredCtx, ID3D11Buffer* pCBPerObject,
	const Matrix* pWorld, const XMFLOAT4* pDiffuseColor)
{
	if (!pDeferredCtx || !pCBPerObject || !pWorld)
		return;

	// Build the per-object constant buffer data
	CBPerObject cbData = m_cbPerObject;  // Start with current state

	// Update world matrix
	cbData.matWorld = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(pWorld));

	// Calculate world-view-proj
	XMMATRIX matWVP = cbData.matWorld * m_cbPerFrame.matView * m_cbPerFrame.matProjection;
	cbData.matWorldViewProj = matWVP;

	// Update diffuse color if provided
	if (pDiffuseColor)
		cbData.vDiffuseColor = *pDiffuseColor;

	// Map and update
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(pDeferredCtx->Map(pCBPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &cbData, sizeof(CBPerObject));
		pDeferredCtx->Unmap(pCBPerObject, 0);
	}
}

void CShaderManager::SetSpeedTreeWindMatrix(int nIndex, const float* p)
{
	if (!p || nIndex < 0 || nIndex >= SPEEDTREE_NUM_WIND_MATRICES)
		return;

	m_cbSpeedTree.matWindMatrices[nIndex] = XMMATRIX(
		p[0], p[1], p[2], p[3],
		p[4], p[5], p[6], p[7],
		p[8], p[9], p[10], p[11],
		p[12], p[13], p[14], p[15]);
	m_bSpeedTreeDirty = true;
}

void CShaderManager::SetSpeedTreeLeafTables(int nFirstEntry, const float* p, UINT uiEntryCount)
{
	if (!p || nFirstEntry < 0) return;

	const int nMax = min((int)uiEntryCount, SPEEDTREE_MAX_LEAF_TABLES - nFirstEntry);
	for (int i = 0; i < nMax; ++i)
	{
		m_cbSpeedTree.vLeafTables[nFirstEntry + i] =
			XMFLOAT4(p[i * 4 + 0], p[i * 4 + 1], p[i * 4 + 2], p[i * 4 + 3]);
	}
	m_cbSpeedTree.nNumLeafTables = max(m_cbSpeedTree.nNumLeafTables, nFirstEntry + nMax);
	m_bSpeedTreeDirty = true;
}

void CShaderManager::SetSpeedTreeLight(const float* p)
{
	if (!p) return;
	m_cbSpeedTree.vLightDir = XMFLOAT4(p[0], p[1], p[2], p[3]);
	m_cbSpeedTree.vLightDiffuse = XMFLOAT4(p[4], p[5], p[6], p[7]);
	m_cbSpeedTree.vLightAmbient = XMFLOAT4(p[8], p[9], p[10], p[11]);
	m_bSpeedTreeDirty = true;
}

void CShaderManager::SetSpeedTreeFogParams(const float* p)
{
	if (!p) return;
	m_cbSpeedTree.vFogParams = XMFLOAT4(p[0], p[1], p[2], p[3]);
	m_bSpeedTreeDirty = true;
}

void CShaderManager::SetGodRaysParams(float fScreenX, float fScreenY, float fIntensity, float fDecay)
{
	m_cbGodRays.vLightScreenPos.x = fScreenX;
	m_cbGodRays.vLightScreenPos.y = fScreenY;
	m_cbGodRays.vLightScreenPos.z = fIntensity;
	m_cbGodRays.vLightScreenPos.w = fDecay;
	m_bGodRaysDirty = true;
}

void CShaderManager::SetGodRaysColor(float r, float g, float b)
{
	m_cbGodRays.vRayColor.x = r;
	m_cbGodRays.vRayColor.y = g;
	m_cbGodRays.vRayColor.z = b;
	m_bGodRaysDirty = true;
}

#ifdef ENABLE_BLOOM
void CShaderManager::SetBloomParams(float threshold, float intensity)
{
	m_cbBloom.vBloomParams.x = threshold;
	m_cbBloom.vBloomParams.y = intensity;
}
#endif


void CShaderManager::GetProjectionMatrix(Matrix* pProj) const
{
	if (!pProj) return;
	// Transpose back since we store transposed
	XMMATRIX mat = XMMatrixTranspose(m_cbPerFrame.matProjection);
	XMStoreFloat4x4((XMFLOAT4X4*)pProj, mat);
}

void CShaderManager::SetViewportSize(float width, float height)
{
	m_cbPerFrame.vCameraPos.w = width;
	m_cbPerFrame.vFogParams.z = height;
	m_bPerFrameDirty = true;
}

void CShaderManager::SetLight(const Vector3* pDirection, const Color* pColor, float fIntensity)
{
	// Legacy API - sets light 0 as a directional light
	DX11Light light;
	light.Position = XMFLOAT4(0, 0, 0, LIGHT_DIRECTIONAL);  // w = type
	light.Direction = XMFLOAT4(
		pDirection ? pDirection->x : 0.0f,
		pDirection ? pDirection->y : -1.0f,
		pDirection ? pDirection->z : 0.0f,
		1.0f);  // w = enabled
	light.Color = XMFLOAT4(
		pColor ? pColor->r : 1.0f,
		pColor ? pColor->g : 1.0f,
		pColor ? pColor->b : 1.0f,
		fIntensity);
	light.Attenuation = XMFLOAT4(1.0f, 0.0f, 0.0f, 10000.0f);  // No attenuation for directional
	SetLight(0, light);
}

void CShaderManager::SetTime(float fTotalTime, float fDeltaTime)
{
	m_cbPerFrame.vTime.x = fTotalTime;
	m_cbPerFrame.vTime.y = fDeltaTime;
	// z = cloud layer2 speed multiplier (set separately)
	m_bPerFrameDirty = true;
}

void CShaderManager::SetShadowOpacity(float fOpacity)
{
	m_cbPerFrame.vShadowParams.x = fOpacity;
	m_bPerFrameDirty = true;
}

void CShaderManager::SetShadowMatrices(const Matrix* pBig, const Matrix* pLocal)
{
	if (pBig)
		m_cbPerFrame.matShadowBig = XMMatrixTranspose(XMLoadFloat4x4((const XMFLOAT4X4*)pBig));
	if (pLocal)
		m_cbPerFrame.matShadowLocal = XMMatrixTranspose(XMLoadFloat4x4((const XMFLOAT4X4*)pLocal));
	m_bPerFrameDirty = true;
}

void CShaderManager::SetCascadeSplits(float s0, float s1, float s2, float s3)
{
	m_cbPerFrame.vCascadeSplits.x = s0;
	m_cbPerFrame.vCascadeSplits.y = s1;
	m_cbPerFrame.vCascadeSplits.z = s2;
	m_cbPerFrame.vCascadeSplits.w = s3;
	m_bPerFrameDirty = true;
}

void CShaderManager::SetDiffuseColor(float r, float g, float b, float a)
{
	XMFLOAT4& cur = m_cbPerObject.vDiffuseColor;
	if (cur.x == r && cur.y == g && cur.z == b && cur.w == a)
		return;
	cur = XMFLOAT4(r, g, b, a);
	m_bPerObjectDirty = true;
}

void CShaderManager::SetMaterial(float fSpecularPower)
{
	if (m_cbPerObject.vMaterialParams.z == fSpecularPower)
		return;
	m_cbPerObject.vMaterialParams.z = fSpecularPower;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetSpecularTune(float fIntensity, float fPower)
{
	XMFLOAT4& p = m_cbPerObject.vPBRParams;
	if (p.z == fIntensity && p.w == fPower) return;
	p.z = fIntensity;
	p.w = fPower;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetSpecularPower(float power)
{
	if (m_cbPerObject.vSpecularColor.w == power) return;
	m_cbPerObject.vSpecularColor.w = power;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetTwoTextureBlend(bool bEnabled)
{
	m_bTwoTextureBlend = bEnabled;

	float v = bEnabled ? 1.0f : 0.0f;
	if (m_cbPerObject.vMaterialParams.w == v)
		return;
	m_cbPerObject.vMaterialParams.w = v;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetParticleColorOp(BYTE byColorOp)
{
	const float v = (float)byColorOp;
	if (m_cbPerObject.vParticleParams.x == v)
		return;
	m_cbPerObject.vParticleParams.x = v;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetTextureMatrix(int slot, const Matrix* pMatrix)
{
	if (!pMatrix) return;

	XMMATRIX mat = XMLoadFloat4x4((const XMFLOAT4X4*)pMatrix);
	mat = XMMatrixTranspose(mat);

	if (slot == 0)
		m_cbPerObject.matTexture0 = mat;
	else if (slot == 1)
		m_cbPerObject.matTexture1 = mat;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetParticleColor(DWORD dwColor)
{
	m_dwParticleColor = dwColor;
	XMFLOAT4 v(
		((dwColor >> 16) & 0xFF) / 255.0f,
		((dwColor >> 8) & 0xFF) / 255.0f,
		(dwColor & 0xFF) / 255.0f,
		((dwColor >> 24) & 0xFF) / 255.0f
	);
	XMFLOAT4& cur = m_cbPerObject.vParticleColor;
	if (cur.x == v.x && cur.y == v.y && cur.z == v.z && cur.w == v.w)
		return;
	cur = v;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetSkyTint(DWORD dwColor)
{
	XMFLOAT4 vFactor(
		((dwColor >> 16) & 0xFF) / 255.0f,
		((dwColor >> 8) & 0xFF) / 255.0f,
		(dwColor & 0xFF) / 255.0f,
		((dwColor >> 24) & 0xFF) / 255.0f
	);
	m_dwSkyTint = dwColor;
	XMFLOAT4& cur = m_cbPerObject.vSkyTint;
	if (cur.x == vFactor.x && cur.y == vFactor.y && cur.z == vFactor.z && cur.w == vFactor.w)
		return;
	cur = vFactor;
	m_bPerObjectDirty = true;
}

bool CShaderManager::IsLightEnabled(UINT index) const
{
	if (index >= MAX_SHADER_LIGHTS) return false;
	return m_cbLighting.lights[index].Direction.w > 0.5f;
}

void CShaderManager::SetFogColor(DWORD dwColor)
{
	float a = ((dwColor >> 24) & 0xFF) / 255.0f;
	float r = ((dwColor >> 16) & 0xFF) / 255.0f;
	float g = ((dwColor >> 8) & 0xFF) / 255.0f;
	float b = (dwColor & 0xFF) / 255.0f;
	m_cbPerFrame.vFogColor = XMFLOAT4(r, g, b, a);
	m_bPerFrameDirty = true;
	ReapplyVolFogDials();
}

void CShaderManager::SetAlphaTestRefByte(DWORD dwRef)
{
	m_dwAlphaTestRef = dwRef;
	m_cbPerObject.vMaterialParams.x = (float)dwRef / 255.0f;
	m_bPerObjectDirty = true;
}

void CShaderManager::GetMaterial(TMaterial* pMaterial) const
{
	if (pMaterial)
		*pMaterial = m_CurrentMaterial;
}

void CShaderManager::LightEnable(UINT index, BOOL bEnable)
{
	EnableLight(index, bEnable != FALSE);
}

void CShaderManager::SyncLightingToContext(ID3D11DeviceContext* pDeferredCtx, ID3D11Buffer* pCBLighting)
{
	if (!pDeferredCtx || !pCBLighting)
		return;

	// Map and copy the lighting constant buffer data
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(pDeferredCtx->Map(pCBLighting, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &m_cbLighting, sizeof(CBLighting));
		pDeferredCtx->Unmap(pCBLighting, 0);
	}

	pDeferredCtx->VSSetConstantBuffers(2, 1, &pCBLighting);
	pDeferredCtx->PSSetConstantBuffers(2, 1, &pCBLighting);
}

void CShaderManager::UpdateSkinningOnContext(ID3D11DeviceContext* pDeferredCtx, ID3D11Buffer* pCBSkinning,
	const Matrix* pBoneMatrices, int boneCount)
{
	if (!pDeferredCtx || !pCBSkinning || !pBoneMatrices || boneCount <= 0)
		return;

	// Clamp to maximum bones
	if (boneCount > MAX_BONES)
		boneCount = MAX_BONES;

	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(pDeferredCtx->Map(pCBSkinning, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		CBSkinning* pData = reinterpret_cast<CBSkinning*>(mapped.pData);

		// Copy bone matrices
		for (int i = 0; i < boneCount; ++i)
		{
			pData->boneMatrices[i] = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&pBoneMatrices[i]));
		}

		// Fill remaining with identity
		for (int i = boneCount; i < MAX_BONES; ++i)
		{
			pData->boneMatrices[i] = XMMatrixIdentity();
		}

		pDeferredCtx->Unmap(pCBSkinning, 0);
	}
}

void CShaderManager::SetSpeedTreeTreePosition(const float* p)
{
	if (!p) return;
	m_cbSpeedTree.vTreePos = XMFLOAT4(p[0], p[1], p[2], p[3]);
	m_bSpeedTreeDirty = true;
}

void CShaderManager::SetSpeedTreeLeafLightingAdjustment(const float* p)
{
	if (!p) return;
	m_cbSpeedTree.vLeafLightingAdj = XMFLOAT4(p[0], p[1], p[2], p[3]);
	m_bSpeedTreeDirty = true;
}

void CShaderManager::SetSpeedTreeMaterial(const float* p)
{
	if (!p) return;
	m_cbSpeedTree.vMaterialDiffuse = XMFLOAT4(p[0], p[1], p[2], p[3]);
	m_cbSpeedTree.vMaterialAmbient = XMFLOAT4(p[4], p[5], p[6], p[7]);
	m_bSpeedTreeDirty = true;
}

void CShaderManager::SetSpeedTreeCompoundMatrix(const float* p)
{
	if (!p) return;
	m_cbPerObject.matWorldViewProj = XMMATRIX(
		p[0], p[1], p[2], p[3],
		p[4], p[5], p[6], p[7],
		p[8], p[9], p[10], p[11],
		p[12], p[13], p[14], p[15]);
	m_bPerObjectDirty = true;
}

void CShaderManager::SetGodRaysRayParams(float fDensity, float fWeight, float fExposure, int nSamples)
{
	m_cbGodRays.vRayParams.x = fDensity;
	m_cbGodRays.vRayParams.y = fWeight;
	m_cbGodRays.vRayParams.z = fExposure;
	m_cbGodRays.vRayParams.w = (float)nSamples;
	m_bGodRaysDirty = true;
}

bool CShaderManager::DispatchFlyTraceCS(const FlyTraceSegmentInput* pSegments, UINT count)
{
	if (!m_bFlyTraceCSAvailable || !pSegments || count == 0)
		return false;

	if (count > MAX_FLYTRACE_SEGMENTS)
		count = MAX_FLYTRACE_SEGMENTS;

	ID3D11DeviceContext* pCtx = m_pContext;
	if (!pCtx)
		return false;

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(pCtx->Map(m_flyTraceCSInput.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return false;

	memcpy(mapped.pData, pSegments, count * sizeof(FlyTraceSegmentInput));
	pCtx->Unmap(m_flyTraceCSInput.pBuffer, 0);

	CCamera* pCam = CCameraManager::Instance().GetCurrentCamera();
	if (!pCam)
		return false;

	const Vector3& vEye = pCam->GetEye();
	const Vector3& vView = pCam->GetView();

	m_cbFlyTraceCS.camEye = XMFLOAT3(vEye.x, vEye.y, vEye.z);
	m_cbFlyTraceCS.camFwd = XMFLOAT3(vView.x, vView.y, vView.z);
	m_cbFlyTraceCS.segmentCount = count;

	if (!m_pCBFlyTraceCS.Update(pCtx, m_cbFlyTraceCS))
		return false;

	CSSetSRV(0, m_flyTraceCSInput.pSRV);
	CSSetUAV(0, m_flyTraceCSOutput.pUAV);
	m_pCBFlyTraceCS.BindCS(pCtx, 0);

	UINT groups = (count + 63) / 64;
	DispatchCompute(CS_FLYTRACE, groups);

	CSUnbindResources();
	InvalidateIACache();

	return true;
}
