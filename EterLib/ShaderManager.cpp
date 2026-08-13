#include "StdAfx.h"
#include "ShaderManager.h"
#include "Camera.h"
#include "../eterBase/Debug.h"
#include "../eterPack/EterPackManager.h"

// HLSL sources are external and loaded through CEterPackManager/FoxFS.

static DXGI_FORMAT GetFloatFormat(UINT mask)
{
	if (mask == 1) return DXGI_FORMAT_R32_FLOAT;
	if (mask <= 3) return DXGI_FORMAT_R32G32_FLOAT;
	if (mask <= 7) return DXGI_FORMAT_R32G32B32_FLOAT;
	if (mask <= 15) return DXGI_FORMAT_R32G32B32A32_FLOAT;
	return DXGI_FORMAT_UNKNOWN;
}

static DXGI_FORMAT GetUIntFormat(UINT mask)
{
	if (mask == 1) return DXGI_FORMAT_R32_UINT;
	if (mask <= 3) return DXGI_FORMAT_R32G32_UINT;
	if (mask <= 7) return DXGI_FORMAT_R32G32B32_UINT;
	if (mask <= 15) return DXGI_FORMAT_R32G32B32A32_UINT;
	return DXGI_FORMAT_UNKNOWN;
}

static DXGI_FORMAT GetSIntFormat(UINT mask)
{
	if (mask == 1) return DXGI_FORMAT_R32_SINT;
	if (mask <= 3) return DXGI_FORMAT_R32G32_SINT;
	if (mask <= 7) return DXGI_FORMAT_R32G32B32_SINT;
	if (mask <= 15) return DXGI_FORMAT_R32G32B32A32_SINT;
	return DXGI_FORMAT_UNKNOWN;
}

static DXGI_FORMAT GetDXGIFormat(const D3D11_SIGNATURE_PARAMETER_DESC& desc)
{
	if (_stricmp(desc.SemanticName, "COLOR") == 0)
		return DXGI_FORMAT_B8G8R8A8_UNORM; //DWORD color format for vertex colors
	if (_stricmp(desc.SemanticName, "BLENDINDICES") == 0)
		return DXGI_FORMAT_R8G8B8A8_UINT;

	switch (desc.ComponentType)
	{
		case D3D_REGISTER_COMPONENT_FLOAT32: return GetFloatFormat(desc.Mask);
		case D3D_REGISTER_COMPONENT_UINT32: return GetUIntFormat(desc.Mask);
		case D3D_REGISTER_COMPONENT_SINT32: return GetSIntFormat(desc.Mask);
		default: return DXGI_FORMAT_UNKNOWN;
	}
}

HRESULT CreateShaderReflection(ComPtr<ID3DBlob> shaderBlob, std::vector<D3D11_INPUT_ELEMENT_DESC>& layout)
{
	static std::vector<std::string> semanticNames;
	layout.clear(); semanticNames.clear();

	ComPtr<ID3D11ShaderReflection> r;
#if _MSC_VER >= 1910 && _MSC_VER <= 1916 //vs 2017 v141
	HRESULT hr = D3DReflect(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), IID_ID3D11ShaderReflection, reinterpret_cast<void**>(&r));
#elif _MSC_VER >= 1930 && _MSC_VER < 1950 //vs 2022 v143
	HRESULT hr = D3DReflect(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), IID_PPV_ARGS(&r));
#elif _MSC_VER >= 1950 //vs 2026 v145
	HRESULT hr = D3DReflect(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), IID_PPV_ARGS(&r));
#endif

	if (FAILED(hr))
		return E_FAIL;

	D3D11_SHADER_DESC sd{}; r->GetDesc(&sd);
	layout.reserve(sd.InputParameters);
	semanticNames.reserve(sd.InputParameters);

	for (UINT i = 0; i < sd.InputParameters; ++i)
	{
		D3D11_SIGNATURE_PARAMETER_DESC p{};
		r->GetInputParameterDesc(i, &p);
		semanticNames.emplace_back(p.SemanticName);
		layout.push_back({ semanticNames.back().c_str(), p.SemanticIndex, GetDXGIFormat(p), 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 });
	}
	return hr;
}

thread_local UINT CShaderManager::t_subsystemDrawCount = 0;

CShaderManager::CShaderManager()
	: m_pDevice(nullptr)
	, m_pContext(nullptr)
	, m_bInitialized(false)
	, m_iFrameCount(0)
	, m_eCurrentShader(SHADER_NONE)
	, m_CurrentTopology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
	, m_bSkyGradientDirty(false)
	, m_bPerFrameDirty(true)
	, m_bPerObjectDirty(true)
	, m_bLightingDirty(true)
	, m_bSpeedTreeDirty(true)
	, m_bSkinningDirty(false)
	, m_iActiveBoneCount(0)
	, m_bParticleBatchingActive(false)
	, m_globalDrawCount(0)
	, m_bGodRaysDirty(false)
	, m_bGodRaysEnabled(false)
#ifdef ENABLE_BLOOM
	, m_pCBBloom(nullptr)
	, m_bBloomEnabled(true)
#endif
#ifdef ENABLE_SSAO
	, m_pCBSSAO(nullptr)
	, m_bSSAODirty(true)
	, m_pSSAONoiseTex(nullptr)
	, m_pSSAONoiseSRV(nullptr)
#endif
	, m_pDefaultTexture(nullptr)
	, m_pDefaultTextureSRV(nullptr)
	, m_pTransparentTexture(nullptr)
	, m_pTransparentTextureSRV(nullptr)
	, m_pActiveDefaultTextureSRV(nullptr)
	, m_pSamplerLinear(nullptr)
	, m_pSamplerPoint(nullptr)
	, m_pSamplerClamp(nullptr)
	, m_pSamplerShadowCmp(nullptr)
	// Render state management
	, m_pStateCache(nullptr)
	, m_pCurrentBlendState(nullptr)
	, m_pCurrentRasterizerState(nullptr)
	, m_pCurrentDepthStencilState(nullptr)
	, m_bBlendStateDirty(true)
	, m_bRasterizerStateDirty(true)
	, m_bDepthStencilStateDirty(true)
	, m_pCurrentIndexBuffer(nullptr)
	, m_IndexFormat(DXGI_FORMAT_R16_UINT)
	, m_IndexOffset(0)
	, m_pDynamicVertexBuffer(nullptr)
	, m_pDynamicIndexBuffer(nullptr)
	, m_CurrentInputLayout(INPUT_LAYOUT_PDT)
	, m_SavedInputLayout(INPUT_LAYOUT_PDT)
	, m_bLightingEnabled(true)
	, m_bFogEnabled(false)
	, m_bAlphaTestEnabled(false)
	, m_dwAlphaTestRef(0)
	, m_bTwoTextureBlend(false)
	, m_dwSkyTint(0xFFFFFFFF)
	, m_dwParticleColor(0xFFFFFFFF)
	, m_dwDynamicVBOffset(0)
	, m_dwDynamicIBOffset(0)
	, m_bDynamicBufferNeedsDiscard(true)
	, m_dwSkinningPoolIndex(0)
	, m_pCBParticleCS(nullptr)
	, m_pParticleCSIB(nullptr)
	, m_bComputeParticlesAvailable(false)
	, m_pCBFlyTraceCS(nullptr)
	, m_pFlyTraceCSIB(nullptr)
	, m_bFlyTraceCSAvailable(false)
	, m_pCBWeaponTraceCS(nullptr)
	, m_bWeaponTraceCSAvailable(false)
{

	// Initialize SpeedTree wind matrices to identity
	for (int i = 0; i < SPEEDTREE_NUM_WIND_MATRICES; ++i)
	{
		m_cbSpeedTree.matWindMatrices[i] = XMMatrixIdentity();
	}

	// Initialize sampler states to defaults
	for (DWORD i = 0; i < MAX_SAMPLER_SLOTS; ++i)
	{
		m_SamplerStates[i].minFilter = FILTER_LINEAR;
		m_SamplerStates[i].magFilter = FILTER_LINEAR;
		m_SamplerStates[i].mipFilter = FILTER_LINEAR;
		m_SamplerStates[i].addressU = ADDRESS_WRAP;
		m_SamplerStates[i].addressV = ADDRESS_WRAP;
		m_SamplerStates[i].addressW = ADDRESS_WRAP;
		m_SamplerStates[i].dirty = true;
	}

	// Initialize default material
	m_CurrentMaterial.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
	m_CurrentMaterial.Specular = Color(1.0f, 1.0f, 1.0f, 1.0f);
	m_CurrentMaterial.Emissive = Color(0.0f, 0.0f, 0.0f, 0.0f);
	m_CurrentMaterial.Ambient = Color(0.2f, 0.2f, 0.2f, 1.0f);
	m_CurrentMaterial.Power = 32.0f;

	m_RenderState.SetDefaults();

	// Initialize matrices to identity
	for (DWORD i = 0; i < MAX_TRANSFORMS; ++i)
	{
		MatrixIdentity(&m_Matrices[i]);
		MatrixIdentity(&m_SavedMatrices[i]);
	}

	m_cbLighting.globalAmbient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);  // Moderate ambient
	m_cbLighting.numActiveLights = 1;

	m_cbLighting.lights[0].Position = XMFLOAT4(0.0f, 0.0f, 0.0f, (float)LIGHT_DIRECTIONAL);
	m_cbLighting.lights[0].Direction = XMFLOAT4(-0.5f, -0.5f, -0.707f, 1.0f);  // w=1 enabled, matches default sun direction
	m_cbLighting.lights[0].Color = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);  // Soft white directional
	m_cbLighting.lights[0].Attenuation = XMFLOAT4(1.0f, 0.0f, 0.0f, 10000.0f);

	// Defaults
	m_cbPerFrame.matView = XMMatrixIdentity();
	m_cbPerFrame.matProjection = XMMatrixIdentity();
	m_cbPerFrame.vCameraPos = XMFLOAT4(0.0f, 0.0f, 0.0f, 1024.0f);  // w = viewport width
	m_cbPerFrame.vFogParams = XMFLOAT4(0.0f, 1000.0f, 768.0f, 0.0f);  // z = viewport height, w = fog disabled
	m_cbPerFrame.vFogColor = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	m_cbPerFrame.vTime = XMFLOAT4(0.0f, 0.0f, 0.5f, 0.0f);  // z = cloud layer2 speed multiplier
	m_cbPerFrame.vSunDirection = XMFLOAT4(-0.5f, -0.5f, -0.707f, 1.0f);  // Default sun direction behind/above camera, w = intensity
	// Note: Lighting data is now in CBLighting (m_cbLighting)

	m_cbPerObject.matWorld = XMMatrixIdentity();
	m_cbPerObject.matWorldViewProj = XMMatrixIdentity();
	m_cbPerObject.matTexture0 = XMMatrixIdentity();
	m_cbPerObject.matTexture1 = XMMatrixIdentity();
	m_cbPerObject.vDiffuseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_cbPerObject.vSkyTint = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_cbPerObject.vParticleColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_cbPerObject.vParticleParams = XMFLOAT4((float)PARTICLE_COLOROP_MODULATE, 0.0f, 0.0f, 0.0f);
	m_cbPerObject.vSpecularColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 32.0f);  // RGB = color, A = power
	m_cbPerObject.vEmissiveColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	m_cbPerObject.vMaterialParams = XMFLOAT4(0.0f, 0.0f, 32.0f, 0.0f);
	m_cbPerObject.vPBRParams = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);  // roughness=0 means auto-derive, metallic=0
}

CShaderManager::~CShaderManager()
{
	Shutdown();
}

bool CShaderManager::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	if (m_bInitialized)
		return true;

	if (!pDevice || !pContext)
		return false;

	m_pDevice = pDevice;
	m_pContext = pContext;

	{
		m_pContext1 = nullptr;
		pContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)&m_pContext1);

		D3D11_FEATURE_DATA_D3D11_OPTIONS opts = {};
		if (SUCCEEDED(pDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &opts, sizeof(opts))))
		{
			m_bCBRingSupported = (m_pContext1 != nullptr)
				&& opts.ConstantBufferOffsetting
				&& opts.MapNoOverwriteOnDynamicConstantBuffer;
		}

		m_bCBRingSupported = false;
	}

	if (!CompileAllShaders())
	{
		TraceError("CShaderManager: Failed to compile shaders");
		Shutdown();
		return false;
	}

	if (!CreateConstantBuffers())
	{
		TraceError("CShaderManager: Failed to create constant buffers");
		Shutdown();
		return false;
	}

	if (!CreateDefaultTexture())
	{
		TraceError("CShaderManager: Failed to create default texture");
		Shutdown();
		return false;
	}

	if (!CreateSamplerStates())
	{
		TraceError("CShaderManager: Failed to create sampler states");
		Shutdown();
		return false;
	}


	// Create state object cache
	m_pStateCache = new CStateObjectCache(pDevice);
	if (!m_pStateCache)
	{
		TraceError("CShaderManager: Failed to create state object cache");
		Shutdown();
		return false;
	}

	// Create dynamic buffers for DrawDynamic
	if (!CreateDynamicBuffers())
	{
		TraceError("CShaderManager: Failed to create dynamic buffers");
		Shutdown();
		return false;
	}

	if (!InitParticleCSResources())
	{
		Tracef("CShaderManager: Particle CS not available, using CPU fallback\n");
		m_bComputeParticlesAvailable = false;
	}

	if (!InitFlyTraceCSResources())
	{
		Tracef("CShaderManager: FlyTrace CS not available, using CPU fallback\n");
		m_bFlyTraceCSAvailable = false;
	}

	if (!InitWeaponTraceCSResources())
	{
		Tracef("CShaderManager: WeaponTrace CS not available, using CPU fallback\n");
		m_bWeaponTraceCSAvailable = false;
	}

	m_bInitialized = true;

	D3D11_VIEWPORT viewport = {};
	UINT numViewports = 1;
	pContext->RSGetViewports(&numViewports, &viewport);
	if (viewport.Width > 0 && viewport.Height > 0)
	{
		SetViewportSize(viewport.Width, viewport.Height);
	}
	else
	{
		SetViewportSize(1024.0f, 768.0f);
	}

	SetSkyTint(0xFFFFFFFF);
	SetParticleColor(0xFFFFFFFF);

	m_bShadowCullActive = false;
	memset(m_afShadowCullPlane, 0, sizeof(m_afShadowCullPlane));

	// Upload initial constant buffer data with default values
	m_bPerFrameDirty = true;
	m_bPerObjectDirty = true;
	CommitChanges();

	for (UINT i = 0; i < STATEMANAGER_MAX_STAGES; ++i)
	{
		SetShaderResource(i, NULL);  // Will use transparent fallback
	}

	Tracef("CShaderManager: Initialized (DX11 native)\n");
	return true;
}

void CShaderManager::Shutdown()
{
	for (int i = 0; i < SHADER_COUNT; ++i)
	{
		if (m_Shaders[i].pVertexShader) { m_Shaders[i].pVertexShader->Release(); m_Shaders[i].pVertexShader = nullptr; }
		if (m_Shaders[i].pPixelShader) { m_Shaders[i].pPixelShader->Release(); m_Shaders[i].pPixelShader = nullptr; }
		if (m_Shaders[i].pInputLayout) { m_Shaders[i].pInputLayout->Release(); m_Shaders[i].pInputLayout = nullptr; }
		if (m_Shaders[i].pVSBlob) { m_Shaders[i].pVSBlob->Release(); m_Shaders[i].pVSBlob = nullptr; }
	}

	for (int i = 0; i < CS_COUNT; ++i)
	{
		if (m_ComputeShaders[i]) { m_ComputeShaders[i]->Release(); m_ComputeShaders[i] = nullptr; }
	}

	if (m_pCBPerFrame) { m_pCBPerFrame.Reset(); }
	if (m_pCBPerObject) { m_pCBPerObject.Reset(); }
	if (m_pCBLighting) { m_pCBLighting.Reset(); }
	if (m_pCBSpeedTree) { m_pCBSpeedTree.Reset(); }
	if (m_pCBSkinning) { m_pCBSkinning.Reset(); }

	for (UINT i = 0; i < SKINNING_CB_POOL_SIZE; ++i)
	{
		if (m_pSkinningCBPool[i]) { m_pSkinningCBPool[i]->Release(); m_pSkinningCBPool[i] = nullptr; }
	}
	if (m_pCBGodRays) { m_pCBGodRays.Reset(); }
#ifdef ENABLE_BLOOM
	if (m_pCBBloom) { m_pCBBloom->Release(); m_pCBBloom = nullptr; }
#endif
#ifdef ENABLE_SSAO
	if (m_pCBSSAO) { m_pCBSSAO->Release(); m_pCBSSAO = nullptr; }
	if (m_pSSAONoiseSRV) { m_pSSAONoiseSRV->Release(); m_pSSAONoiseSRV = nullptr; }
	if (m_pSSAONoiseTex) { m_pSSAONoiseTex->Release(); m_pSSAONoiseTex = nullptr; }
#endif
	if (m_pCBSkyGradient) { m_pCBSkyGradient.Reset(); }

	if (m_pDefaultTextureSRV) { m_pDefaultTextureSRV->Release(); m_pDefaultTextureSRV = nullptr; }
	if (m_pDefaultTexture) { m_pDefaultTexture->Release(); m_pDefaultTexture = nullptr; }
	if (m_pTransparentTextureSRV) { m_pTransparentTextureSRV->Release(); m_pTransparentTextureSRV = nullptr; }
	if (m_pTransparentTexture) { m_pTransparentTexture->Release(); m_pTransparentTexture = nullptr; }
	if (m_pSamplerLinear) { m_pSamplerLinear->Release(); m_pSamplerLinear = nullptr; }
	if (m_pSamplerPoint) { m_pSamplerPoint->Release(); m_pSamplerPoint = nullptr; }
	if (m_pSamplerClamp) { m_pSamplerClamp->Release(); m_pSamplerClamp = nullptr; }
	if (m_pSamplerShadowCmp) { m_pSamplerShadowCmp->Release(); m_pSamplerShadowCmp = nullptr; }

	// Clean up particle CS resources
	ReleaseGpuBuffer(m_particleCSInput);
	ReleaseGpuBuffer(m_particleCSOutput);
	if (m_pCBParticleCS) { m_pCBParticleCS->Release(); m_pCBParticleCS = nullptr; }
	if (m_pParticleCSIB) { m_pParticleCSIB->Release(); m_pParticleCSIB = nullptr; }
	m_bComputeParticlesAvailable = false;

	// Clean up fly trace CS resources
	ReleaseGpuBuffer(m_flyTraceCSInput);
	ReleaseGpuBuffer(m_flyTraceCSOutput);
	if (m_pCBFlyTraceCS) { m_pCBFlyTraceCS->Release(); m_pCBFlyTraceCS = nullptr; }
	if (m_pFlyTraceCSIB) { m_pFlyTraceCSIB->Release(); m_pFlyTraceCSIB = nullptr; }
	m_bFlyTraceCSAvailable = false;

	// Clean up weapon trace CS resources
	ReleaseGpuBuffer(m_weaponTraceCSInput);
	ReleaseGpuBuffer(m_weaponTraceCSOutput);
	if (m_pCBWeaponTraceCS) { m_pCBWeaponTraceCS->Release(); m_pCBWeaponTraceCS = nullptr; }
	m_bWeaponTraceCSAvailable = false;

	// Clean up render state management resources
	if (m_pDynamicVertexBuffer) { m_pDynamicVertexBuffer->Release(); m_pDynamicVertexBuffer = nullptr; }
	if (m_pDynamicIndexBuffer) { m_pDynamicIndexBuffer->Release(); m_pDynamicIndexBuffer = nullptr; }
	if (m_pStateCache) { delete m_pStateCache; m_pStateCache = nullptr; }

	m_pCurrentBlendState = nullptr;
	m_pCurrentRasterizerState = nullptr;
	m_pCurrentDepthStencilState = nullptr;
	m_SavedRenderStates.clear();

	m_pDevice = nullptr;
	m_pContext = nullptr;
	m_bInitialized = false;
	m_eCurrentShader = SHADER_NONE;
}

void CShaderManager::SetDefaultState()
{
	if (!m_bInitialized || !GetActiveContext())
		return;

	// Reset render state tracking
	m_RenderState.SetDefaults();
	m_SavedRenderStates.clear();

	// Mark all states as needing update
	m_bBlendStateDirty = true;
	m_bRasterizerStateDirty = true;
	m_bDepthStencilStateDirty = true;

	m_pCurrentBlendState = nullptr;
	m_pCurrentRasterizerState = nullptr;
	m_pCurrentDepthStencilState = nullptr;

	// Reset sampler states
	for (int i = 0; i < MAX_SAMPLER_SLOTS; ++i)
	{
		m_SamplerStates[i].dirty = true;
	}

	// Reset texture bindings
	for (int i = 0; i < STATEMANAGER_MAX_STAGES; ++i)
	{
		m_pTextures[i] = nullptr;
	}

	// Reset current shader
	m_eCurrentShader = SHADER_NONE;

	m_CurrentTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	for (DWORD i = 0; i < MAX_STREAMS; ++i)
	{
		m_Streams[i].pBuffer = nullptr;
		m_Streams[i].stride = 0;
		m_Streams[i].offset = 0;
	}
	m_pCurrentIndexBuffer = nullptr;
	m_IndexFormat = DXGI_FORMAT_UNKNOWN;
	m_IndexOffset = 0;

	// Apply default blend state (alpha blending enabled)
	ApplyRenderStates();
}

bool CShaderManager::CompileAllShaders()
{
	static const char* s_ShaderNames[] = {
		"UI", "Mesh", "Mesh2Tex", "Terrain", "Water", "Sky", "Particle", "Shadow",
		"ShadowSkinned", "SpeedTree", "SpeedTreeLeaf", "MeshNormal", "MeshSkinned", "GodRays",
		"MeshVTF", "ShadowVTF", "SpeedTreeVTF", "Mesh2TexVTF", "ParticlePCT",
#ifdef ENABLE_BLOOM
		"BloomBright", "BloomBlur", "BloomComposite",
#endif
#ifdef ENABLE_SSAO
		"SSAO", "SSAOBlur", "DepthResolve",
#endif
	};

	struct ShaderFile { EShaderType type; const char* vs; const char* ps; };
	static const ShaderFile shaders[] = {
		{ SHADER_UI, "shaders/ui_vs.hlsl", "shaders/ui_ps.hlsl" },
		{ SHADER_MESH, "shaders/mesh_vs.hlsl", "shaders/mesh_ps.hlsl" },
		{ SHADER_MESH_2TEX, "shaders/mesh2tex_vs.hlsl", "shaders/mesh2tex_ps.hlsl" },
		{ SHADER_TERRAIN, "shaders/terrain_vs.hlsl", "shaders/terrain_ps.hlsl" },
		{ SHADER_WATER, "shaders/water_vs.hlsl", "shaders/water_ps.hlsl" },
		{ SHADER_SKY, "shaders/sky_vs.hlsl", "shaders/sky_ps.hlsl" },
		{ SHADER_PARTICLE, "shaders/particle_vs.hlsl", "shaders/particle_ps.hlsl" },
		{ SHADER_SHADOW, "shaders/shadow_vs.hlsl", "shaders/shadow_ps.hlsl" },
		{ SHADER_SHADOW_SKINNED, "shaders/shadow_skinned_vs.hlsl", "shaders/shadow_skinned_ps.hlsl" },
		{ SHADER_SPEEDTREE, "shaders/speedtree_vs.hlsl", "shaders/speedtree_ps.hlsl" },
		{ SHADER_SPEEDTREE_LEAF, "shaders/speedtree_leaf_vs.hlsl", "shaders/speedtree_leaf_ps.hlsl" },
		{ SHADER_MESH_NORMAL, "shaders/mesh_normal_vs.hlsl", "shaders/mesh_normal_ps.hlsl" },
		{ SHADER_MESH_SKINNED, "shaders/mesh_skinned_vs.hlsl", "shaders/mesh_skinned_ps.hlsl" },
		{ SHADER_GODRAYS, "shaders/godrays_vs.hlsl", "shaders/godrays_ps.hlsl" },
		{ SHADER_MESH_VTF, "shaders/mesh_vtf_vs.hlsl", "shaders/mesh_vtf_ps.hlsl" },
		{ SHADER_SHADOW_VTF, "shaders/shadow_vtf_vs.hlsl", "shaders/shadow_vtf_ps.hlsl" },
		{ SHADER_SPEEDTREE_VTF, "shaders/speedtree_vtf_vs.hlsl", "shaders/speedtree_vtf_ps.hlsl" },
		{ SHADER_MESH_2TEX_VTF, "shaders/mesh2tex_vtf_vs.hlsl", "shaders/mesh2tex_vtf_ps.hlsl" },
		{ SHADER_PARTICLE_PCT, "shaders/particle_pct_vs.hlsl", "shaders/particle_ps.hlsl" },
#ifdef ENABLE_BLOOM
		{ SHADER_BLOOM_BRIGHT, "shaders/godrays_vs.hlsl", "shaders/bloom_bright_ps.hlsl" },
		{ SHADER_BLOOM_BLUR, "shaders/godrays_vs.hlsl", "shaders/bloom_blur_ps.hlsl" },
		{ SHADER_BLOOM_COMPOSITE, "shaders/godrays_vs.hlsl", "shaders/bloom_composite_ps.hlsl" },
#endif
#ifdef ENABLE_SSAO
		{ SHADER_SSAO, "shaders/godrays_vs.hlsl", "shaders/ssao_ps.hlsl" },
		{ SHADER_SSAO_BLUR, "shaders/godrays_vs.hlsl", "shaders/ssao_blur_ps.hlsl" },
		{ SHADER_DEPTH_RESOLVE, "shaders/godrays_vs.hlsl", "shaders/depth_resolve_ps.hlsl" },
#endif
	};

	bool bSuccess = true;
	for (const auto& s : shaders)
	{
		if (!CompileShader(s.type, s.vs, s.ps))
		{
			TraceError("CompileAllShaders: Failed to load/compile %s shader", s_ShaderNames[s.type]);
			bSuccess = false;
		}
	}

	if (!CompileComputeShader(CS_PARTICLE_BILLBOARD, "shaders/particle_billboard_cs.hlsl", "CSMain"))
		Tracef("CompileAllShaders: Particle billboard CS not available, using CPU fallback\n");
	if (!CompileComputeShader(CS_FLYTRACE, "shaders/flytrace_cs.hlsl", "CSMain"))
		Tracef("CompileAllShaders: FlyTrace CS not available, using CPU fallback\n");
	if (!CompileComputeShader(CS_WEAPONTRACE, "shaders/weapontrace_cs.hlsl", "CSMain"))
		Tracef("CompileAllShaders: WeaponTrace CS not available, using CPU fallback\n");

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
// Shader Cache Implementation
//////////////////////////////////////////////////////////////////////////

const char* CShaderManager::GetShaderCachePath()
{
	static char s_szCachePath[MAX_PATH] = { 0 };

	if (!s_szCachePath[0])
	{
		char exePath[MAX_PATH] = { 0 };
		GetModuleFileNameA(nullptr, exePath, MAX_PATH);

		char* slash = strrchr(exePath, '\\');
		if (slash)
			*slash = '\0';

		char cachePath[MAX_PATH] = { 0 };
		sprintf_s(cachePath, "%s\\cache", exePath);
		CreateDirectoryA(cachePath, nullptr);

		sprintf_s(s_szCachePath, "%s\\cache\\shaders", exePath);
		CreateDirectoryA(s_szCachePath, nullptr);

		Tracef("Shader cache path: %s\n", s_szCachePath);
	}

	return s_szCachePath;
}

UINT CShaderManager::ComputeShaderHash(const void* pVSData, size_t vsSize, const void* pPSData, size_t psSize)
{
	const UINT FNV_PRIME = 16777619u;
	UINT hash = 2166136261u;
	auto append = [&](const void* data, size_t size)
	{
		const BYTE* p = static_cast<const BYTE*>(data);
		for (size_t i = 0; i < size; ++i) { hash ^= p[i]; hash *= FNV_PRIME; }
	};
	if (pVSData && vsSize) append(pVSData, vsSize);
	if (pPSData && psSize) append(pPSData, psSize);
#ifdef _DEBUG
	static const char profile[] = "vs_5_0|ps_5_0|main|DEBUG_SKIP_OPT";
#else
	static const char profile[] = "vs_5_0|ps_5_0|main|OPT3";
#endif
	append(profile, sizeof(profile) - 1);
	return hash;
}

bool CShaderManager::LoadShaderFromCache(EShaderType type, UINT hash, ID3DBlob** ppVSBlob, ID3DBlob** ppPSBlob)
{
	char szVSPath[MAX_PATH], szPSPath[MAX_PATH], szHashPath[MAX_PATH];
	sprintf_s(szVSPath, "%s\\shader_%d_vs.cso", GetShaderCachePath(), type);
	sprintf_s(szPSPath, "%s\\shader_%d_ps.cso", GetShaderCachePath(), type);
	sprintf_s(szHashPath, "%s\\shader_%d.hash", GetShaderCachePath(), type);

	// Check if hash file exists and matches
	FILE* fp = nullptr;
	if (fopen_s(&fp, szHashPath, "rb") != 0 || !fp)
		return false;

	UINT storedHash = 0;
	fread(&storedHash, sizeof(UINT), 1, fp);
	fclose(fp);

	if (storedHash != hash)
		return false;  // Hash mismatch, need to recompile

	// Load VS bytecode
	if (fopen_s(&fp, szVSPath, "rb") != 0 || !fp)
		return false;

	fseek(fp, 0, SEEK_END);
	long vsSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (vsSize <= 0)
	{
		fclose(fp);
		return false;
	}

	HRESULT hr = D3DCreateBlob(vsSize, ppVSBlob);
	if (FAILED(hr))
	{
		fclose(fp);
		return false;
	}

	fread((*ppVSBlob)->GetBufferPointer(), 1, vsSize, fp);
	fclose(fp);

	// Load PS bytecode
	if (fopen_s(&fp, szPSPath, "rb") != 0 || !fp)
	{
		(*ppVSBlob)->Release();
		*ppVSBlob = nullptr;
		return false;
	}

	fseek(fp, 0, SEEK_END);
	long psSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (psSize <= 0)
	{
		fclose(fp);
		(*ppVSBlob)->Release();
		*ppVSBlob = nullptr;
		return false;
	}

	hr = D3DCreateBlob(psSize, ppPSBlob);
	if (FAILED(hr))
	{
		fclose(fp);
		(*ppVSBlob)->Release();
		*ppVSBlob = nullptr;
		return false;
	}

	fread((*ppPSBlob)->GetBufferPointer(), 1, psSize, fp);
	fclose(fp);

	return true;
}

bool CShaderManager::SaveShaderToCache(EShaderType type, UINT hash, ID3DBlob* pVSBlob, ID3DBlob* pPSBlob)
{
	char szVSPath[MAX_PATH], szPSPath[MAX_PATH], szHashPath[MAX_PATH];
	sprintf_s(szVSPath, "%s\\shader_%d_vs.cso", GetShaderCachePath(), type);
	sprintf_s(szPSPath, "%s\\shader_%d_ps.cso", GetShaderCachePath(), type);
	sprintf_s(szHashPath, "%s\\shader_%d.hash", GetShaderCachePath(), type);

	FILE* fp = nullptr;

	// Save VS bytecode
	if (fopen_s(&fp, szVSPath, "wb") != 0 || !fp)
		return false;
	fwrite(pVSBlob->GetBufferPointer(), 1, pVSBlob->GetBufferSize(), fp);
	fclose(fp);

	// Save PS bytecode
	if (fopen_s(&fp, szPSPath, "wb") != 0 || !fp)
		return false;
	fwrite(pPSBlob->GetBufferPointer(), 1, pPSBlob->GetBufferSize(), fp);
	fclose(fp);

	// Save hash
	if (fopen_s(&fp, szHashPath, "wb") != 0 || !fp)
		return false;
	fwrite(&hash, sizeof(UINT), 1, fp);
	fclose(fp);

	return true;
}

bool CShaderManager::CompileShader(EShaderType type, const char* szVSFile, const char* szPSFile)
{
	if (!m_pDevice || type < 0 || type >= SHADER_COUNT || !szVSFile || !szPSFile) return false;

	CMappedFile vsFile, psFile;
	LPCVOID pVSData = nullptr, pPSData = nullptr;
	if (!CEterPackManager::Instance().Get(vsFile, szVSFile, &pVSData) || !pVSData || !vsFile.Size())
	{
		TraceError("CompileShader(%d): VS '%s' not found in pack", type, szVSFile);
		return false;
	}
	if (!CEterPackManager::Instance().Get(psFile, szPSFile, &pPSData) || !pPSData || !psFile.Size())
	{
		TraceError("CompileShader(%d): PS '%s' not found in pack", type, szPSFile);
		return false;
	}

	const UINT shaderHash = ComputeShaderHash(pVSData, vsFile.Size(), pPSData, psFile.Size());
	ID3DBlob* pVSBlob = nullptr;
	ID3DBlob* pPSBlob = nullptr;
	ID3DBlob* pErrorBlob = nullptr;
	HRESULT hr;

	if (LoadShaderFromCache(type, shaderHash, &pVSBlob, &pPSBlob))
	{
		hr = m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_Shaders[type].pVertexShader);
		if (SUCCEEDED(hr)) hr = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_Shaders[type].pPixelShader);
		if (SUCCEEDED(hr))
		{
			std::vector<D3D11_INPUT_ELEMENT_DESC> layout;
			hr = CreateShaderReflection(pVSBlob, layout);
			if (SUCCEEDED(hr)) hr = m_pDevice->CreateInputLayout(layout.data(), static_cast<UINT>(layout.size()), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_Shaders[type].pInputLayout);
		}
		if (SUCCEEDED(hr))
		{
			pPSBlob->Release();
			m_Shaders[type].pVSBlob = pVSBlob;
			return true;
		}
		if (m_Shaders[type].pVertexShader) { m_Shaders[type].pVertexShader->Release(); m_Shaders[type].pVertexShader = nullptr; }
		if (m_Shaders[type].pPixelShader) { m_Shaders[type].pPixelShader->Release(); m_Shaders[type].pPixelShader = nullptr; }
		if (m_Shaders[type].pInputLayout) { m_Shaders[type].pInputLayout->Release(); m_Shaders[type].pInputLayout = nullptr; }
		pVSBlob->Release(); pVSBlob = nullptr;
		pPSBlob->Release(); pPSBlob = nullptr;
	}

	UINT compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#ifdef _DEBUG
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	hr = D3DCompile(pVSData, vsFile.Size(), szVSFile, nullptr, nullptr, "main", "vs_5_0", compileFlags, 0, &pVSBlob, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob) { TraceError("Shader %d VS [%s]: %s", type, szVSFile, (char*)pErrorBlob->GetBufferPointer()); pErrorBlob->Release(); }
		return false;
	}
	if (pErrorBlob) { pErrorBlob->Release(); pErrorBlob = nullptr; }

	hr = D3DCompile(pPSData, psFile.Size(), szPSFile, nullptr, nullptr, "main", "ps_5_0", compileFlags, 0, &pPSBlob, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob) { TraceError("Shader %d PS [%s]: %s", type, szPSFile, (char*)pErrorBlob->GetBufferPointer()); pErrorBlob->Release(); }
		pVSBlob->Release();
		return false;
	}
	if (pErrorBlob) pErrorBlob->Release();

	hr = m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_Shaders[type].pVertexShader);
	if (FAILED(hr)) { pVSBlob->Release(); pPSBlob->Release(); return false; }
	hr = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_Shaders[type].pPixelShader);
	if (FAILED(hr)) { m_Shaders[type].pVertexShader->Release(); m_Shaders[type].pVertexShader = nullptr; pVSBlob->Release(); pPSBlob->Release(); return false; }

	std::vector<D3D11_INPUT_ELEMENT_DESC> layout;
	hr = CreateShaderReflection(pVSBlob, layout);
	if (FAILED(hr)) { pVSBlob->Release(); pPSBlob->Release(); return false; }
	hr = m_pDevice->CreateInputLayout(layout.data(), static_cast<UINT>(layout.size()), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_Shaders[type].pInputLayout);
	if (FAILED(hr)) { pVSBlob->Release(); pPSBlob->Release(); return false; }

	SaveShaderToCache(type, shaderHash, pVSBlob, pPSBlob);
	pPSBlob->Release();
	m_Shaders[type].pVSBlob = pVSBlob;
	return true;
}

bool CShaderManager::CreateConstantBuffers()
{
	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	HRESULT hr;

	cbDesc.ByteWidth = sizeof(CBPerFrame);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, m_pCBPerFrame.GetAddressOf());
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create PerFrame buffer (size=%d, hr=0x%08X)", sizeof(CBPerFrame), hr);
		return false;
	}

	cbDesc.ByteWidth = m_bCBRingSupported
		? (CBRingAlign256(sizeof(CBPerObject)) * CB_RING_SLOTS_PEROBJECT)
		: sizeof(CBPerObject);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, m_pCBPerObject.GetAddressOf());
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create PerObject buffer (size=%d, hr=0x%08X)", sizeof(CBPerObject), hr);
		return false;
	}

	cbDesc.ByteWidth = sizeof(CBLighting);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, m_pCBLighting.GetAddressOf());
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create Lighting buffer (size=%d, hr=0x%08X)", sizeof(CBLighting), hr);
		return false;
	}

	cbDesc.ByteWidth = sizeof(CBSpeedTree);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, m_pCBSpeedTree.GetAddressOf());
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create SpeedTree buffer (size=%d, hr=0x%08X)", sizeof(CBSpeedTree), hr);
		return false;
	}

	cbDesc.ByteWidth = sizeof(CBSkinning);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, m_pCBSkinning.GetAddressOf());
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create Skinning buffer (size=%d, hr=0x%08X)", sizeof(CBSkinning), hr);
		return false;
	}

	{
		D3D11_BUFFER_DESC poolDesc = {};
		poolDesc.ByteWidth = sizeof(CBSkinning);
		poolDesc.Usage = D3D11_USAGE_DEFAULT;
		poolDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		poolDesc.CPUAccessFlags = 0;
		for (UINT i = 0; i < SKINNING_CB_POOL_SIZE; ++i)
		{
			hr = m_pDevice->CreateBuffer(&poolDesc, nullptr, &m_pSkinningCBPool[i]);
			if (FAILED(hr))
			{
				TraceError("CreateConstantBuffers: Failed to create Skinning pool buffer %d (hr=0x%08X)", i, hr);
				// Non-fatal: fall back to WRITE_DISCARD path for remaining
				break;
			}
		}
	}

	cbDesc.ByteWidth = sizeof(CBGodRays);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, m_pCBGodRays.GetAddressOf());
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create GodRays buffer (size=%d, hr=0x%08X)", sizeof(CBGodRays), hr);
		return false;
	}

#ifdef ENABLE_BLOOM
	cbDesc.ByteWidth = sizeof(CBBloom);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBBloom);
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create Bloom buffer (size=%d, hr=0x%08X)", sizeof(CBBloom), hr);
		return false;
	}
#endif

#ifdef ENABLE_SSAO
	cbDesc.ByteWidth = sizeof(CBSSAO);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBSSAO);
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create SSAO buffer (size=%d, hr=0x%08X)", sizeof(CBSSAO), hr);
		return false;
	}

	// Initialize SSAO parameters
	m_cbSSAO.vSSAOParams = XMFLOAT4(0.5f, 0.025f, 1.5f, 0.0f); // radius, bias, intensity

	{
		// Simple deterministic pseudo-random using a fixed seed
		unsigned int seed = 12345u;
		auto nextRand = [&seed]() -> float {
			seed = seed * 1103515245u + 12345u;
			return (float)(seed & 0x7FFFFFFFu) / (float)0x7FFFFFFFu;
			};

		for (int i = 0; i < SSAO_KERNEL_SIZE; ++i)
		{
			// Random direction in upper hemisphere
			float x = nextRand() * 2.0f - 1.0f;
			float y = nextRand() * 2.0f - 1.0f;
			float z = nextRand(); // z > 0 (upper hemisphere)

			// Normalize
			float len = sqrtf(x * x + y * y + z * z);
			if (len > 0.001f) { x /= len; y /= len; z /= len; }

			// Quadratic scale: more samples near origin
			float scale = (float)i / (float)SSAO_KERNEL_SIZE;
			scale = 0.1f + scale * scale * 0.9f; // lerp(0.1, 1.0, scale*scale)
			m_cbSSAO.vSampleKernel[i] = XMFLOAT4(x * scale, y * scale, z * scale, 0.0f);
		}
	}

	{
		unsigned int noiseSeed = 54321u;
		auto noiseRand = [&noiseSeed]() -> BYTE {
			noiseSeed = noiseSeed * 1103515245u + 12345u;
			return (BYTE)((noiseSeed >> 16) & 0xFF);
			};

		BYTE noiseData[4 * 4 * 2]; // 4x4 texels, 2 bytes each (RG)
		for (int i = 0; i < 4 * 4; ++i)
		{
			noiseData[i * 2 + 0] = noiseRand();
			noiseData[i * 2 + 1] = noiseRand();
		}

		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = 4;
		texDesc.Height = 4;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_IMMUTABLE;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = noiseData;
		initData.SysMemPitch = 4 * 2;

		hr = m_pDevice->CreateTexture2D(&texDesc, &initData, &m_pSSAONoiseTex);
		if (FAILED(hr))
		{
			TraceError("CreateConstantBuffers: Failed to create SSAO noise texture (hr=0x%08X)", hr);
			return false;
		}

		hr = m_pDevice->CreateShaderResourceView(m_pSSAONoiseTex, nullptr, &m_pSSAONoiseSRV);
		if (FAILED(hr))
		{
			TraceError("CreateConstantBuffers: Failed to create SSAO noise SRV (hr=0x%08X)", hr);
			return false;
		}
	}

	m_bSSAODirty = true;
#endif

	cbDesc.ByteWidth = sizeof(CBSkyGradient);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, m_pCBSkyGradient.GetAddressOf());
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create SkyGradient buffer (size=%d, hr=0x%08X)", sizeof(CBSkyGradient), hr);
		return false;
	}

	m_bSkyGradientDirty = false;

	// Initialize bone matrices to identity
	for (int i = 0; i < MAX_BONES; ++i)
		m_cbSkinning.boneMatrices[i] = XMMatrixIdentity();

	// Initialize god rays with default values
	m_cbGodRays.vLightScreenPos = XMFLOAT4(0.5f, 0.3f, 1.0f, 0.97f);  // Center-top, intensity=1, decay=0.97
	m_cbGodRays.vRayParams = XMFLOAT4(0.5f, 0.5f, 0.3f, 64.0f);      // density, weight, exposure, samples
	m_cbGodRays.vRayColor = XMFLOAT4(1.0f, 0.9f, 0.7f, 1.0f);        // Warm sun color


#ifdef ENABLE_BLOOM
	// Initialize bloom with default values
	m_cbBloom.vBloomParams = XMFLOAT4(BLOOM_DEFAULT_THRESHOLD, BLOOM_DEFAULT_INTENSITY, 0.0f, 0.0f);
	m_cbBloom.vTexelSize = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	m_cbBloom.vBlurDirection = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
#endif

	return true;
}

bool CShaderManager::CreateDefaultTexture()
{
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = 1;
	texDesc.Height = 1;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_IMMUTABLE;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	UINT32 whitePixel = 0xFFFFFFFF;
	D3D11_SUBRESOURCE_DATA initData = { &whitePixel, sizeof(UINT32), 0 };

	if (FAILED(m_pDevice->CreateTexture2D(&texDesc, &initData, &m_pDefaultTexture))) return false;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	if (FAILED(m_pDevice->CreateShaderResourceView(m_pDefaultTexture, &srvDesc, &m_pDefaultTextureSRV))) return false;

	UINT32 transparentPixel = 0x00000000;
	D3D11_SUBRESOURCE_DATA initDataTransparent = { &transparentPixel, sizeof(UINT32), 0 };

	if (FAILED(m_pDevice->CreateTexture2D(&texDesc, &initDataTransparent, &m_pTransparentTexture))) return false;
	if (FAILED(m_pDevice->CreateShaderResourceView(m_pTransparentTexture, &srvDesc, &m_pTransparentTextureSRV))) return false;

	m_pActiveDefaultTextureSRV = m_pTransparentTextureSRV;

	return true;
}

bool CShaderManager::CreateSamplerStates()
{
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	if (FAILED(m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerLinear))) return false;

	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = 0;  // Force base mip level only (no mipmap selection)
	if (FAILED(m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerPoint))) return false;

	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.BorderColor[0] = 1.0f;  // White border = no shadow outside projection
	sampDesc.BorderColor[1] = 1.0f;
	sampDesc.BorderColor[2] = 1.0f;
	sampDesc.BorderColor[3] = 1.0f;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	if (FAILED(m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerClamp))) return false;
	D3D11_SAMPLER_DESC cmpDesc = {};
	cmpDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	cmpDesc.AddressU = cmpDesc.AddressV = cmpDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	cmpDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	cmpDesc.MaxLOD = D3D11_FLOAT32_MAX;
	if (FAILED(m_pDevice->CreateSamplerState(&cmpDesc, &m_pSamplerShadowCmp))) return false;



	return true;
}

void CShaderManager::BindShader(EShaderType type)
{
	if (!m_bInitialized || !GetActiveContext() || type < 0 || type >= SHADER_COUNT) return;

	EShaderType& eCurrentShader = m_eCurrentShader;
	if (eCurrentShader == type) return;

	ShaderProgram& shader = m_Shaders[type];
	if (!shader.pVertexShader || !shader.pPixelShader || !shader.pInputLayout)
	{
		TraceError("CShaderManager::BindShader - Shader %d not compiled", type);
		return;
	}
	GetActiveContext()->VSSetShader(shader.pVertexShader, nullptr, 0);
	GetActiveContext()->PSSetShader(shader.pPixelShader, nullptr, 0);
	GetActiveContext()->IASetInputLayout(shader.pInputLayout);

	if (type == SHADER_UI && m_pSamplerPoint)
		GetActiveContext()->PSSetSamplers(0, 1, &m_pSamplerPoint);
	else if (m_pSamplerLinear)
		GetActiveContext()->PSSetSamplers(0, 1, &m_pSamplerLinear);

	if (m_pSamplerShadowCmp)
		GetActiveContext()->PSSetSamplers(2, 1, &m_pSamplerShadowCmp);

	if ((type == SHADER_TERRAIN || type == SHADER_MESH || type == SHADER_MESH_2TEX || type == SHADER_MESH_SKINNED || type == SHADER_MESH_VTF || type == SHADER_MESH_2TEX_VTF) && m_pSamplerClamp)
		GetActiveContext()->PSSetSamplers(1, 1, &m_pSamplerClamp);

	ID3D11Buffer* pCBPerFrame = m_pCBPerFrame.Get();
	ID3D11Buffer* pCBPerObject = m_pCBPerObject.Get();
	ID3D11Buffer* pCBLighting = m_pCBLighting.Get();

	if (pCBPerFrame)
	{
		GetActiveContext()->VSSetConstantBuffers(0, 1, &pCBPerFrame);
		GetActiveContext()->PSSetConstantBuffers(0, 1, &pCBPerFrame);
	}
	if (pCBPerObject)
	{
		__BindCBRing(GetActiveContext(), m_pContext1,
			pCBPerObject,
			m_cbPerObjectBound,
			sizeof(CBPerObject), 1, 1, true);
	}
	if (pCBLighting)
	{
		GetActiveContext()->VSSetConstantBuffers(2, 1, &pCBLighting);
		GetActiveContext()->PSSetConstantBuffers(2, 1, &pCBLighting);
	}

	if (type == SHADER_SPEEDTREE || type == SHADER_SPEEDTREE_LEAF || type == SHADER_SPEEDTREE_VTF)
	{
		ID3D11Buffer* pCBSpeedTree = m_pCBSpeedTree.Get();
		if (pCBSpeedTree)
			GetActiveContext()->VSSetConstantBuffers(3, 1, &pCBSpeedTree);
	}

	// Bind sky gradient constant buffer for sky shader
	if (type == SHADER_SKY && m_pCBSkyGradient)
	{
		GetActiveContext()->VSSetConstantBuffers(2, 1, m_pCBSkyGradient.GetAddressOf());
		GetActiveContext()->PSSetConstantBuffers(2, 1, m_pCBSkyGradient.GetAddressOf());
	}

	eCurrentShader = type;

	D3D11_MAPPED_SUBRESOURCE mapped;
	if (m_bLightingDirty && m_pCBLighting)
	{
		if (SUCCEEDED(GetActiveContext()->Map(m_pCBLighting.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbLighting, sizeof(m_cbLighting));
			GetActiveContext()->Unmap(m_pCBLighting.Get(), 0);
			m_bLightingDirty = false;
		}
	}
}

void CShaderManager::BeginUI() { BindShader(SHADER_UI); }
void CShaderManager::BeginMesh() { BindShader(SHADER_MESH); }
void CShaderManager::BeginMesh2Tex() { BindShader(SHADER_MESH_2TEX); }
void CShaderManager::BeginTerrain() { BindShader(SHADER_TERRAIN); }
void CShaderManager::BeginWater()
{
	BindShader(SHADER_WATER);
}
void CShaderManager::BeginSky() { BindShader(SHADER_SKY); }

void CShaderManager::SetSkyGradient(const float* pColors, int count, int upperSegments)
{
	if (!m_pCBSkyGradient || !GetActiveContext()) return;
	count = min(count, SKY_GRADIENT_MAX_POINTS);
	for (int i = 0; i < count; ++i)
		m_cbSkyGradient.colors[i] = XMFLOAT4(pColors[i * 4], pColors[i * 4 + 1], pColors[i * 4 + 2], pColors[i * 4 + 3]);
	m_cbSkyGradient.colorCount = count;
	m_cbSkyGradient.upperSegments = upperSegments;
	m_bSkyGradientDirty = true;
}


void CShaderManager::BeginParticle() { BindShader(SHADER_PARTICLE); }
void CShaderManager::BeginShadow() { BindShader(SHADER_SHADOW); }
void CShaderManager::BeginShadowSkinned()
{
	BindShader(SHADER_SHADOW_SKINNED);
	ID3D11Buffer* pSkinCB = m_pCBSkinning.Get();
	if (pSkinCB)
		__BindCBRing(GetActiveContext(), m_pContext1,
			pSkinCB, 0,
			sizeof(CBSkinning), 3, -1, false);
}
void CShaderManager::BeginSpeedTree() { BindShader(SHADER_SPEEDTREE); }
void CShaderManager::BeginSpeedTreeLeaf() { BindShader(SHADER_SPEEDTREE_LEAF); }
void CShaderManager::BeginMeshNormal() { BindShader(SHADER_MESH_NORMAL); }

void CShaderManager::BeginMeshSkinned()
{
	BindShader(SHADER_MESH_SKINNED);
	ID3D11Buffer* pSkinCB = m_pCBSkinning.Get();
	if (pSkinCB)
		__BindCBRing(GetActiveContext(), m_pContext1,
			pSkinCB, 0,
			sizeof(CBSkinning), 3, -1, false);
}

void CShaderManager::BeginGodRays()
{
	BindShader(SHADER_GODRAYS);
	// Bind the god rays constant buffer to slot 0
	if (m_pCBGodRays)
		GetActiveContext()->PSSetConstantBuffers(0, 1, m_pCBGodRays.GetAddressOf());
}

#ifdef ENABLE_BLOOM
void CShaderManager::BeginBloomBright()
{
	BindShader(SHADER_BLOOM_BRIGHT);
	if (m_pCBBloom)
		GetActiveContext()->PSSetConstantBuffers(0, 1, &m_pCBBloom);
}

void CShaderManager::BeginBloomBlur()
{
	BindShader(SHADER_BLOOM_BLUR);
	if (m_pCBBloom)
		GetActiveContext()->PSSetConstantBuffers(0, 1, &m_pCBBloom);
}

void CShaderManager::BeginBloomComposite()
{
	BindShader(SHADER_BLOOM_COMPOSITE);
}

void CShaderManager::SetBloomEnabled(bool bEnabled)
{
	m_bBloomEnabled = bEnabled;
}

void CShaderManager::SetBloomParams(float threshold, float intensity)
{
	m_cbBloom.vBloomParams.x = threshold;
	m_cbBloom.vBloomParams.y = intensity;
}
#endif

void CShaderManager::BeginMeshVTF()
{
	BindShader(SHADER_MESH_VTF);
}

void CShaderManager::BeginShadowVTF()
{
	BindShader(SHADER_SHADOW_VTF);
}

void CShaderManager::BeginSpeedTreeVTF()
{
	BindShader(SHADER_SPEEDTREE_VTF);
}

void CShaderManager::BeginMesh2TexVTF()
{
	BindShader(SHADER_MESH_2TEX_VTF);
}

void CShaderManager::BeginParticlePCT() { BindShader(SHADER_PARTICLE_PCT); }

void CShaderManager::End()
{
	if (GetActiveContext())
	{
		GetActiveContext()->VSSetShader(nullptr, nullptr, 0);
		GetActiveContext()->PSSetShader(nullptr, nullptr, 0);
	}
	m_eCurrentShader = SHADER_NONE;
}

void CShaderManager::BindForInputLayout(EInputLayoutType type)
{
	EShaderType eCur = m_eCurrentShader;
	if (eCur != SHADER_NONE)
	{
		return;
	}

	switch (type)
	{
	case INPUT_LAYOUT_TRANSFORMED:
		BeginUI();
		break;
	case INPUT_LAYOUT_PNT:
	case INPUT_LAYOUT_SKINNED:
		BeginMesh();
		break;
	case INPUT_LAYOUT_PNT2:
		BeginMesh2Tex();
		break;
	case INPUT_LAYOUT_PN:
	case INPUT_LAYOUT_TERRAIN_HTP:
		BeginTerrain();
		break;
	case INPUT_LAYOUT_PD:
	case INPUT_LAYOUT_WATER:
		BeginWater();
		break;
	case INPUT_LAYOUT_PDT:
	case INPUT_LAYOUT_PDT2:
	default:
		BeginUI();
		break;
	case INPUT_LAYOUT_PT:
		BeginParticle();
		break;
	}
}

ID3D11InputLayout* CShaderManager::GetInputLayout(EShaderType type) const
{
	return (type >= 0 && type < SHADER_COUNT) ? m_Shaders[type].pInputLayout : nullptr;
}

// Per-Frame Updates
void CShaderManager::SetViewProjection(const Matrix* pView, const Matrix* pProj)
{
	m_cbPerFrame.matView = XMMatrixTranspose(XMLoadFloat4x4((XMFLOAT4X4*)pView));
	m_cbPerFrame.matProjection = XMMatrixTranspose(XMLoadFloat4x4((XMFLOAT4X4*)pProj));
	m_bPerFrameDirty = true;
	m_Matrices[MATRIX_VIEW] = *pView;
	m_Matrices[MATRIX_PROJECTION] = *pProj;
}

void CShaderManager::GetProjectionMatrix(Matrix* pProj) const
{
	if (!pProj) return;
	// Transpose back since we store transposed
	XMMATRIX mat = XMMatrixTranspose(m_cbPerFrame.matProjection);
	XMStoreFloat4x4((XMFLOAT4X4*)pProj, mat);
}

void CShaderManager::SetCameraPosition(const Vector3* pCameraPos)
{
	if (pCameraPos) { m_cbPerFrame.vCameraPos.x = pCameraPos->x; m_cbPerFrame.vCameraPos.y = pCameraPos->y; m_cbPerFrame.vCameraPos.z = pCameraPos->z; }
	m_bPerFrameDirty = true;
}

void CShaderManager::SetViewportSize(float width, float height)
{
	m_cbPerFrame.vCameraPos.w = width;
	m_cbPerFrame.vFogParams.z = height;
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

void CShaderManager::SetAmbient(const Color* pColor)
{
	if (pColor)
		SetGlobalAmbient(pColor->r, pColor->g, pColor->b, 1.0f);
}

void CShaderManager::SetTime(float fTotalTime, float fDeltaTime)
{
	m_cbPerFrame.vTime.x = fTotalTime;
	m_cbPerFrame.vTime.y = fDeltaTime;
	// z = cloud layer2 speed multiplier (set separately)
	m_bPerFrameDirty = true;
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

void CShaderManager::SetShadowOpacity(float fOpacity)
{
	m_cbPerFrame.vShadowParams.x = fOpacity;
	m_bPerFrameDirty = true;
}

void CShaderManager::SetShadowTexelSize(float fTexelSize)
{
	m_cbPerFrame.vShadowParams.z = fTexelSize;
	m_bPerFrameDirty = true;
}

void CShaderManager::SetShadowCullPlanes(const float* pafPlanes4x4)
{
	if (!pafPlanes4x4)
	{
		m_bShadowCullActive = false;
		return;
	}

	memcpy(m_afShadowCullPlane, pafPlanes4x4, sizeof(m_afShadowCullPlane));
	m_bShadowCullActive = true;
}

bool CShaderManager::IsInShadowCull(float x, float y, float z, float fRadius) const
{
	if (!m_bShadowCullActive)
		return true;

	for (int i = 0; i < 4; ++i)
	{
		const float* p = m_afShadowCullPlane[i];
		if (p[0] * x + p[1] * y + p[2] * z + p[3] < -fRadius)
			return false;
	}

	return true;
}

void CShaderManager::SetReflectionClipZ(float fWaterZ)
{
	m_cbPerFrame.vShadowParams.y = fWaterZ;
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

void CShaderManager::SetShadowMidFarMatrices(const Matrix* pMid, const Matrix* pFar)
{
	if (pMid)
		m_cbPerFrame.matShadowMid = XMMatrixTranspose(XMLoadFloat4x4((const XMFLOAT4X4*)pMid));
	if (pFar)
		m_cbPerFrame.matShadowFar = XMMatrixTranspose(XMLoadFloat4x4((const XMFLOAT4X4*)pFar));
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

void CShaderManager::SetShadowTextures(ID3D11ShaderResourceView* pBig, ID3D11ShaderResourceView* pLocal)
{
	if (!GetActiveContext())
		return;
	ID3D11ShaderResourceView* shadowTextures[2] = { pBig, pLocal };
	m_pTextures[2] = pBig;
	m_pTextures[3] = pLocal;
	GetActiveContext()->PSSetShaderResources(2, 2, shadowTextures);
}

void CShaderManager::SetShadowMidFarTextures(ID3D11ShaderResourceView* pMid, ID3D11ShaderResourceView* pFar)
{
	if (!GetActiveContext())
		return;
	ID3D11ShaderResourceView* shadowTextures[2] = { pMid, pFar };
	m_pTextures[4] = pMid;
	m_pTextures[5] = pFar;
	GetActiveContext()->PSSetShaderResources(4, 2, shadowTextures);
}

void CShaderManager::SetLightingEnabled(bool bEnabled)
{
	m_bLightingEnabled = bEnabled;
	// Enable/disable light 0 (the primary directional light)
	EnableLight(0, bEnabled);
}

// Per-Object Updates
void CShaderManager::SetWorldMatrix(const Matrix* pWorld)
{
	XMMATRIX matWorld = XMLoadFloat4x4((XMFLOAT4X4*)pWorld);
	XMMATRIX matWVP;
	matWVP = matWorld * XMMatrixTranspose(m_cbPerFrame.matView) * XMMatrixTranspose(m_cbPerFrame.matProjection);
	m_cbPerObject.matWorld = XMMatrixTranspose(matWorld);
	m_cbPerObject.matWorldViewProj = XMMatrixTranspose(matWVP);
	m_bPerObjectDirty = true;
}

void CShaderManager::SetDiffuseColor(float r, float g, float b, float a)
{
	XMFLOAT4& cur = m_cbPerObject.vDiffuseColor;
	if (cur.x == r && cur.y == g && cur.z == b && cur.w == a)
		return;
	cur = XMFLOAT4(r, g, b, a);
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

void CShaderManager::SetMaterial(float fSpecularPower)
{
	if (m_cbPerObject.vMaterialParams.z == fSpecularPower)
		return;
	m_cbPerObject.vMaterialParams.z = fSpecularPower;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetTextureColorSwap(bool bEnabled)
{
	float v = bEnabled ? 1.0f : 0.0f;
	if (m_cbPerObject.vMaterialParams.z == v) return;
	m_cbPerObject.vMaterialParams.z = v;
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

void CShaderManager::SetSpecularColor(float r, float g, float b)
{
	XMFLOAT4& cur = m_cbPerObject.vSpecularColor;
	if (cur.x == r && cur.y == g && cur.z == b) return;
	cur.x = r; cur.y = g; cur.z = b;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetSpecularPower(float power)
{
	if (m_cbPerObject.vSpecularColor.w == power) return;
	m_cbPerObject.vSpecularColor.w = power;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetEmissiveColor(float r, float g, float b)
{
	XMFLOAT4& cur = m_cbPerObject.vEmissiveColor;
	if (cur.x == r && cur.y == g && cur.z == b) return;
	cur = XMFLOAT4(r, g, b, 0.0f);
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

bool CShaderManager::IsTwoTextureBlendEnabled() const
{
	return m_bTwoTextureBlend;
}

void CShaderManager::SetParticleColorOp(BYTE byColorOp)
{
	const float v = (float)byColorOp;
	if (m_cbPerObject.vParticleParams.x == v)
		return;
	m_cbPerObject.vParticleParams.x = v;
	m_bPerObjectDirty = true;
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

void CShaderManager::SetCharacterShadowPass(bool bEnabled)
{
	float v = bEnabled ? 1.0f : 0.0f;
	if (m_cbPerObject.vRenderFlags.x == v)
		return;
	m_cbPerObject.vRenderFlags.x = v;
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

void CShaderManager::SetMeshTextureAlphaEnabled(bool bEnabled)
{
	const float v = bEnabled ? 1.0f : 0.0f;
	if (m_cbPerObject.vRenderFlags.y == v)
		return;

	m_cbPerObject.vRenderFlags.y = v;
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

void CShaderManager::__CommitCBRing(ID3D11DeviceContext* pCtx, ID3D11DeviceContext1* pCtx1,
	ID3D11Buffer* pBuf, UINT& rOffset, UINT& rBound, UINT ringBytes,
	const void* pSrc, UINT srcBytes, int vsSlot, int psSlot,
	bool* pForceDiscard)
{
	if (!pCtx || !pBuf || !pSrc) return;

	D3D11_MAPPED_SUBRESOURCE mapped;
	const UINT stride = CBRingAlign256(srcBytes);

	// Fallback: no offset-binding support (pre-D3D11.1 machines) — original behaviour.
	if (!m_bCBRingSupported || !pCtx1 || stride > ringBytes)
	{
		if (SUCCEEDED(pCtx->Map(pBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, pSrc, srcBytes);
			pCtx->Unmap(pBuf, 0);
			rBound = 0;
			if (vsSlot >= 0) pCtx->VSSetConstantBuffers((UINT)vsSlot, 1, &pBuf);
			if (psSlot >= 0) pCtx->PSSetConstantBuffers((UINT)psSlot, 1, &pBuf);
		}
		return;
	}

	// Wrap → DISCARD once (the sync point); otherwise append with NO_OVERWRITE.
	D3D11_MAP mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	if (pForceDiscard && *pForceDiscard)
	{
		rOffset = 0;
		mapType = D3D11_MAP_WRITE_DISCARD;
		*pForceDiscard = false;
	}
	else if (rOffset + stride > ringBytes)
	{
		rOffset = 0;
		mapType = D3D11_MAP_WRITE_DISCARD;
	}

	if (FAILED(pCtx->Map(pBuf, 0, mapType, 0, &mapped)))
		return;
	memcpy(static_cast<BYTE*>(mapped.pData) + rOffset, pSrc, srcBytes);
	pCtx->Unmap(pBuf, 0);

	const UINT firstConstant = rOffset / 16u;
	const UINT numConstants = stride / 16u;
	if (vsSlot >= 0) pCtx1->VSSetConstantBuffers1((UINT)vsSlot, 1, &pBuf, &firstConstant, &numConstants);
	if (psSlot >= 0) pCtx1->PSSetConstantBuffers1((UINT)psSlot, 1, &pBuf, &firstConstant, &numConstants);

	rBound = rOffset;
	rOffset += stride;
}

void CShaderManager::__BindCBRing(ID3D11DeviceContext* pCtx, ID3D11DeviceContext1* pCtx1,
	ID3D11Buffer* pBuf, UINT boundOffset, UINT srcBytes,
	int vsSlot, int psSlot, bool bRing)
{
	if (!pCtx || !pBuf) return;

	if (bRing && m_bCBRingSupported && pCtx1)
	{
		const UINT firstConstant = boundOffset / 16u;
		const UINT numConstants = CBRingAlign256(srcBytes) / 16u;
		if (vsSlot >= 0) pCtx1->VSSetConstantBuffers1((UINT)vsSlot, 1, &pBuf, &firstConstant, &numConstants);
		if (psSlot >= 0) pCtx1->PSSetConstantBuffers1((UINT)psSlot, 1, &pBuf, &firstConstant, &numConstants);
		return;
	}

	if (vsSlot >= 0) pCtx->VSSetConstantBuffers((UINT)vsSlot, 1, &pBuf);
	if (psSlot >= 0) pCtx->PSSetConstantBuffers((UINT)psSlot, 1, &pBuf);
}


void CShaderManager::CommitChanges()
{
	if (!GetActiveContext()) return;

	D3D11_MAPPED_SUBRESOURCE mapped;


	// ===== Main thread path: existing code =====
	if (m_bPerFrameDirty && m_pCBPerFrame)
	{
		if (SUCCEEDED(GetActiveContext()->Map(m_pCBPerFrame.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbPerFrame, sizeof(m_cbPerFrame));
			GetActiveContext()->Unmap(m_pCBPerFrame.Get(), 0);
			GetActiveContext()->VSSetConstantBuffers(0, 1, m_pCBPerFrame.GetAddressOf());
			GetActiveContext()->PSSetConstantBuffers(0, 1, m_pCBPerFrame.GetAddressOf());
			m_bPerFrameDirty = false;
		}
		else
		{
			TraceError("CommitChanges: Failed to map PerFrame constant buffer!");
		}
	}

	if (m_bPerObjectDirty && m_pCBPerObject)
	{
		__CommitCBRing(GetActiveContext(), m_pContext1, m_pCBPerObject.Get(),
			m_cbPerObjectOffset, m_cbPerObjectBound,
			CBRingAlign256(sizeof(CBPerObject)) * CB_RING_SLOTS_PEROBJECT,
			&m_cbPerObject, sizeof(m_cbPerObject), 1, 1);
		m_bPerObjectDirty = false;
	}

	if (m_bLightingDirty && m_pCBLighting)
	{
		if (SUCCEEDED(GetActiveContext()->Map(m_pCBLighting.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbLighting, sizeof(m_cbLighting));
			GetActiveContext()->Unmap(m_pCBLighting.Get(), 0);
			GetActiveContext()->VSSetConstantBuffers(2, 1, m_pCBLighting.GetAddressOf());
			GetActiveContext()->PSSetConstantBuffers(2, 1, m_pCBLighting.GetAddressOf());
			m_bLightingDirty = false;
		}
		else
		{
			TraceError("CommitChanges: Failed to map Lighting constant buffer!");
		}
	}

	if (m_bSpeedTreeDirty && m_pCBSpeedTree && (m_eCurrentShader == SHADER_SPEEDTREE || m_eCurrentShader == SHADER_SPEEDTREE_LEAF))
	{
		if (SUCCEEDED(GetActiveContext()->Map(m_pCBSpeedTree.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbSpeedTree, sizeof(m_cbSpeedTree));
			GetActiveContext()->Unmap(m_pCBSpeedTree.Get(), 0);
			GetActiveContext()->VSSetConstantBuffers(3, 1, m_pCBSpeedTree.GetAddressOf());
			m_bSpeedTreeDirty = false;
		}
		else
		{
			TraceError("CommitChanges: Failed to map SpeedTree constant buffer!");
		}
	}

	if (m_bSkinningDirty && m_pCBSkinning &&
		(m_eCurrentShader == SHADER_MESH_SKINNED || m_eCurrentShader == SHADER_SHADOW_SKINNED))
	{
		UINT poolIdx = m_dwSkinningPoolIndex;
		if (poolIdx < SKINNING_CB_POOL_SIZE && m_pSkinningCBPool[poolIdx])
		{
			GetActiveContext()->UpdateSubresource(m_pSkinningCBPool[poolIdx], 0, nullptr, &m_cbSkinning, 0, 0);
			GetActiveContext()->VSSetConstantBuffers(3, 1, &m_pSkinningCBPool[poolIdx]);
			m_dwSkinningPoolIndex++;
			m_bSkinningDirty = false;
		}
		else
		{
			if (SUCCEEDED(GetActiveContext()->Map(m_pCBSkinning.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
			{
				memcpy(mapped.pData, &m_cbSkinning, sizeof(CBSkinning));
				GetActiveContext()->Unmap(m_pCBSkinning.Get(), 0);
				GetActiveContext()->VSSetConstantBuffers(3, 1, m_pCBSkinning.GetAddressOf());
				m_bSkinningDirty = false;
			}
			else
			{
				TraceError("CommitChanges: Failed to map Skinning constant buffer!");
			}
		}
	}

	if (m_bSkyGradientDirty && m_pCBSkyGradient && m_eCurrentShader == SHADER_SKY)
	{
		if (SUCCEEDED(GetActiveContext()->Map(m_pCBSkyGradient.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbSkyGradient, sizeof(m_cbSkyGradient));
			GetActiveContext()->Unmap(m_pCBSkyGradient.Get(), 0);
			GetActiveContext()->PSSetConstantBuffers(2, 1, m_pCBSkyGradient.GetAddressOf());
			m_bSkyGradientDirty = false;
		}
	}

}

void CShaderManager::SetShaderResource(UINT slot, ID3D11ShaderResourceView* pSRV)
{
	if (!GetActiveContext()) return;

	ID3D11ShaderResourceView* pActualSRV = pSRV;
	if (!pSRV && m_pActiveDefaultTextureSRV)
		pActualSRV = m_pActiveDefaultTextureSRV;

	if (slot < STATEMANAGER_MAX_STAGES)
	{
		if (m_pTextures[slot] == pActualSRV)
			return;
		m_pTextures[slot] = pActualSRV;
	}
	GetActiveContext()->PSSetShaderResources(slot, 1, &pActualSRV);
}

void CShaderManager::SetDefaultTexture(UINT slot)
{
	if (!GetActiveContext()) return;

	ID3D11ShaderResourceView* pTexture = m_pDefaultTextureSRV;
	if (!pTexture) return;

	// Only update shared texture tracking on main thread
	if (slot < STATEMANAGER_MAX_STAGES)
		m_pTextures[slot] = pTexture;
	GetActiveContext()->PSSetShaderResources(slot, 1, &pTexture);
}

void CShaderManager::OnFrameComplete()
{
	++m_iFrameCount;

	if (m_iFrameCount == 15 && m_pActiveDefaultTextureSRV == m_pTransparentTextureSRV)
	{
		m_pActiveDefaultTextureSRV = m_pDefaultTextureSRV;
	}
}

//////////////////////////////////////////////////////////////////////////
// Multi-Light Support (Native DX11)
//////////////////////////////////////////////////////////////////////////

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

void CShaderManager::GetLight(UINT index, DX11Light* pLight) const
{
	if (index >= MAX_SHADER_LIGHTS || !pLight) return;
	*pLight = m_cbLighting.lights[index];
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

bool CShaderManager::IsLightEnabled(UINT index) const
{
	if (index >= MAX_SHADER_LIGHTS) return false;
	return m_cbLighting.lights[index].Direction.w > 0.5f;
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

void CShaderManager::SetGlobalAmbient(const XMFLOAT4& color)
{
	m_cbLighting.globalAmbient = color;
	m_bLightingDirty = true;
}

void CShaderManager::SetGlobalAmbient(float r, float g, float b, float a)
{
	SetGlobalAmbient(XMFLOAT4(r, g, b, a));
}


void PendingRenderState::SetDefaults()
{
	bAlphaBlendEnable = false;
	srcBlend = D3D11_BLEND_SRC_ALPHA;
	destBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendOp = D3D11_BLEND_OP_ADD;
	srcBlendAlpha = D3D11_BLEND_ONE;
	destBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendOpAlpha = D3D11_BLEND_OP_ADD;
	colorWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	// Rasterizer state defaults
	fillMode = D3D11_FILL_SOLID;
	cullMode = D3D11_CULL_FRONT;  // Match game convention: CULL_FRONT culls CW (front) faces, keeps CCW visible
	bScissorEnable = false;
	bMultisampleEnable = false;
	bAntialiasedLineEnable = false;
	depthBias = 0;
	depthBiasClamp = 0.0f;
	slopeScaledDepthBias = 0.0f;
	bDepthClipEnable = true;

	// Depth stencil state defaults
	bDepthEnable = true;
	bDepthWriteEnable = true;
	depthFunc = D3D11_COMPARISON_LESS_EQUAL;
	bStencilEnable = false;
	stencilReadMask = 0xFF;
	stencilWriteMask = 0xFF;
}

void CShaderManager::SetPipelineState(EPipelineState state, DWORD value)
{
	PendingRenderState& rs = m_RenderState;
	bool& bBlendDirty = m_bBlendStateDirty;
	bool& bRasterDirty = m_bRasterizerStateDirty;
	bool& bDepthDirty = m_bDepthStencilStateDirty;

	switch (state)
	{
		// Blend states
	case PSTATE_BLENDENABLE:
	{ bool v = (value != 0); if (rs.bAlphaBlendEnable == v) return; rs.bAlphaBlendEnable = v; }
	bBlendDirty = true;
	break;
	case PSTATE_SRCBLEND:
		if (rs.srcBlend == (D3D11_BLEND)value) return;
		rs.srcBlend = (D3D11_BLEND)value;
		bBlendDirty = true;
		break;
	case PSTATE_DESTBLEND:
		if (rs.destBlend == (D3D11_BLEND)value) return;
		rs.destBlend = (D3D11_BLEND)value;
		bBlendDirty = true;
		break;
	case PSTATE_BLENDOP:
		if (rs.blendOp == (D3D11_BLEND_OP)value) return;
		rs.blendOp = (D3D11_BLEND_OP)value;
		bBlendDirty = true;
		break;
	case PSTATE_RTWRITEMASK:
		if (rs.colorWriteMask == (UINT8)value) return;
		rs.colorWriteMask = (UINT8)value;
		bBlendDirty = true;
		break;

		// Rasterizer states
	case PSTATE_FILLMODE:
		rs.fillMode = (value == FILL_WIREFRAME) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
		bRasterDirty = true;
		break;
	case PSTATE_CULLMODE:
	{
		D3D11_CULL_MODE newCull = rs.cullMode;
		switch (value)
		{
		case CULL_NONE: newCull = D3D11_CULL_NONE; break;
		case CULL_FRONT:   newCull = D3D11_CULL_FRONT; break;
		case CULL_BACK:  newCull = D3D11_CULL_BACK; break;
		}
		if (rs.cullMode == newCull) return;
		rs.cullMode = newCull;
	}
	bRasterDirty = true;
	break;
	case PSTATE_SCISSORENABLE:
	{ bool v = (value != 0); if (rs.bScissorEnable == v) return; rs.bScissorEnable = v; }
	bRasterDirty = true;
	break;
	case PSTATE_DEPTHBIAS:
		if (rs.depthBias == (INT)value) return;
		rs.depthBias = (INT)value;
		bRasterDirty = true;
		break;
	case PSTATE_SLOPESCALEDDEPTHBIAS:
	{ float v = *(float*)&value; if (rs.slopeScaledDepthBias == v) return; rs.slopeScaledDepthBias = v; }
	bRasterDirty = true;
	break;

	// Depth stencil states
	case PSTATE_DEPTHENABLE:
	{ bool v = (value != 0); if (rs.bDepthEnable == v) return; rs.bDepthEnable = v; }
	bDepthDirty = true;
	break;
	case PSTATE_DEPTHWRITEMASK:
	{ bool v = (value != 0); if (rs.bDepthWriteEnable == v) return; rs.bDepthWriteEnable = v; }
	bDepthDirty = true;
	break;
	case PSTATE_DEPTHFUNC:
		if (rs.depthFunc == (D3D11_COMPARISON_FUNC)value) return;
		rs.depthFunc = (D3D11_COMPARISON_FUNC)value;
		bDepthDirty = true;
		break;
	case PSTATE_STENCILENABLE:
	{ bool v = (value != 0); if (rs.bStencilEnable == v) return; rs.bStencilEnable = v; }
	bDepthDirty = true;
	break;
	case PSTATE_STENCILREADMASK:
		if (rs.stencilReadMask == (UINT8)value) return;
		rs.stencilReadMask = (UINT8)value;
		bDepthDirty = true;
		break;
	case PSTATE_STENCILWRITEMASK:
		if (rs.stencilWriteMask == (UINT8)value) return;
		rs.stencilWriteMask = (UINT8)value;
		bDepthDirty = true;
		break;
	default:
		break;
	}
}

DWORD CShaderManager::GetPipelineState(EPipelineState state)
{
	const PendingRenderState& rs = m_RenderState;

	switch (state)
	{
	case PSTATE_BLENDENABLE: return rs.bAlphaBlendEnable ? 1 : 0;
	case PSTATE_SRCBLEND: return (DWORD)rs.srcBlend;
	case PSTATE_DESTBLEND: return (DWORD)rs.destBlend;
	case PSTATE_BLENDOP: return (DWORD)rs.blendOp;
	case PSTATE_RTWRITEMASK: return rs.colorWriteMask;
	case PSTATE_FILLMODE: return (rs.fillMode == D3D11_FILL_WIREFRAME) ? FILL_WIREFRAME : FILL_SOLID;
	case PSTATE_CULLMODE:
		switch (rs.cullMode)
		{
		case D3D11_CULL_NONE: return CULL_NONE;
		case D3D11_CULL_FRONT: return CULL_FRONT;
		case D3D11_CULL_BACK: return CULL_BACK;
		}
		return CULL_NONE;
	case PSTATE_DEPTHENABLE: return rs.bDepthEnable ? 1 : 0;
	case PSTATE_DEPTHWRITEMASK: return rs.bDepthWriteEnable ? 1 : 0;
	case PSTATE_DEPTHFUNC: return (DWORD)rs.depthFunc;
	case PSTATE_STENCILENABLE: return rs.bStencilEnable ? 1 : 0;
	case PSTATE_STENCILREADMASK: return rs.stencilReadMask;
	case PSTATE_STENCILWRITEMASK: return rs.stencilWriteMask;
	default: return 0;
	}
}

void CShaderManager::SavePipelineState(EPipelineState state, DWORD value)
{
	auto& savedStates = m_SavedRenderStates;
	savedStates[state] = GetPipelineState(state);
	SetPipelineState(state, value);
}

void CShaderManager::RestorePipelineState(EPipelineState state)
{
	auto& savedStates = m_SavedRenderStates;
	auto it = savedStates.find(state);
	if (it != savedStates.end())
	{
		SetPipelineState(state, it->second);
		savedStates.erase(it);
	}
}

void CShaderManager::UpdateBlendState()
{
	bool& bDirty = m_bBlendStateDirty;
	if (!bDirty || !GetActiveContext() || !m_pStateCache) return;

	const PendingRenderState& rs = m_RenderState;

	D3D11_BLEND_DESC desc = {};
	desc.RenderTarget[0].BlendEnable = rs.bAlphaBlendEnable;
	desc.RenderTarget[0].SrcBlend = rs.srcBlend;
	desc.RenderTarget[0].DestBlend = rs.destBlend;
	desc.RenderTarget[0].BlendOp = rs.blendOp;
	desc.RenderTarget[0].SrcBlendAlpha = rs.srcBlendAlpha;
	desc.RenderTarget[0].DestBlendAlpha = rs.destBlendAlpha;
	desc.RenderTarget[0].BlendOpAlpha = rs.blendOpAlpha;
	desc.RenderTarget[0].RenderTargetWriteMask = rs.colorWriteMask;

	ID3D11BlendState*& pCurrentState = m_pCurrentBlendState;
	ID3D11BlendState* pState = m_pStateCache->GetBlendState(desc);
	if (pState && pState != pCurrentState)
	{
		float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		GetActiveContext()->OMSetBlendState(pState, blendFactor, 0xFFFFFFFF);
		pCurrentState = pState;
	}
	bDirty = false;
}

void CShaderManager::UpdateRasterizerState()
{
	bool& bDirty = m_bRasterizerStateDirty;
	if (!bDirty || !GetActiveContext() || !m_pStateCache) return;

	const PendingRenderState& rs = m_RenderState;

	D3D11_RASTERIZER_DESC desc = {};
	desc.FillMode = rs.fillMode;
	desc.CullMode = rs.cullMode;
	desc.FrontCounterClockwise = FALSE;
	desc.DepthBias = rs.depthBias;
	desc.DepthBiasClamp = rs.depthBiasClamp;
	desc.SlopeScaledDepthBias = rs.slopeScaledDepthBias;
	desc.DepthClipEnable = rs.bDepthClipEnable;
	desc.ScissorEnable = rs.bScissorEnable;
	desc.MultisampleEnable = rs.bMultisampleEnable;
	desc.AntialiasedLineEnable = rs.bAntialiasedLineEnable;

	ID3D11RasterizerState*& pCurrentState = m_pCurrentRasterizerState;
	ID3D11RasterizerState* pState = m_pStateCache->GetRasterizerState(desc);
	if (pState && pState != pCurrentState)
	{
		GetActiveContext()->RSSetState(pState);
		pCurrentState = pState;
	}
	bDirty = false;
}

void CShaderManager::UpdateDepthStencilState()
{
	bool& bDirty = m_bDepthStencilStateDirty;
	if (!bDirty || !GetActiveContext() || !m_pStateCache) return;

	const PendingRenderState& rs = m_RenderState;

	D3D11_DEPTH_STENCIL_DESC desc = {};
	desc.DepthEnable = rs.bDepthEnable;
	desc.DepthWriteMask = rs.bDepthWriteEnable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = rs.depthFunc;
	desc.StencilEnable = rs.bStencilEnable;
	desc.StencilReadMask = rs.stencilReadMask;
	desc.StencilWriteMask = rs.stencilWriteMask;
	desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	desc.BackFace = desc.FrontFace;

	ID3D11DepthStencilState*& pCurrentState = m_pCurrentDepthStencilState;
	ID3D11DepthStencilState* pState = m_pStateCache->GetDepthStencilState(desc);
	if (pState && pState != pCurrentState)
	{
		GetActiveContext()->OMSetDepthStencilState(pState, 0);
		pCurrentState = pState;
	}
	bDirty = false;
}

void CShaderManager::CommitRenderState()
{
	UpdateBlendState();
	UpdateRasterizerState();
	UpdateDepthStencilState();
}

void CShaderManager::ApplyRenderStates()
{
	CommitRenderState();
}

void CShaderManager::SetVertexBuffer(UINT stream, ID3D11Buffer* pBuffer, UINT stride, UINT offset)
{
	if (stream >= MAX_STREAMS) return;
	if (!GetActiveContext()) return;

	if (m_Streams[stream].pBuffer == pBuffer &&
		m_Streams[stream].stride == stride &&
		m_Streams[stream].offset == offset)
		return;
	m_Streams[stream].pBuffer = pBuffer;
	m_Streams[stream].stride = stride;
	m_Streams[stream].offset = offset;

	GetActiveContext()->IASetVertexBuffers(stream, 1, &pBuffer, &stride, &offset);
}

void CShaderManager::SetIndexBuffer(ID3D11Buffer* pBuffer, DXGI_FORMAT format, UINT offset)
{
	if (!GetActiveContext()) return;

	if (m_pCurrentIndexBuffer == pBuffer &&
		m_IndexFormat == format &&
		m_IndexOffset == offset)
		return;
	m_pCurrentIndexBuffer = pBuffer;
	m_IndexFormat = format;
	m_IndexOffset = offset;

	GetActiveContext()->IASetIndexBuffer(pBuffer, format, offset);
}

void CShaderManager::SetPrimitiveTopologyIfChanged(D3D11_PRIMITIVE_TOPOLOGY topology)
{
	if (!GetActiveContext()) return;
	if (topology == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED) return;

	if (m_CurrentTopology == topology) return;
	m_CurrentTopology = topology;

	GetActiveContext()->IASetPrimitiveTopology(topology);
}

void CShaderManager::InvalidateIACache()
{
	for (DWORD i = 0; i < MAX_STREAMS; ++i)
	{
		m_Streams[i].pBuffer = nullptr;
		m_Streams[i].stride = 0;
		m_Streams[i].offset = 0;
	}
	m_pCurrentIndexBuffer = nullptr;
	m_IndexFormat = DXGI_FORMAT_UNKNOWN;
	m_IndexOffset = 0;
	m_CurrentTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

static D3D11_PRIMITIVE_TOPOLOGY GetD3D11Topology(EPrimitiveTopology type)
{
	switch (type)
	{
	case TOPOLOGY_NONE:          return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;  // Don't change topology
	case TOPOLOGY_POINTLIST:     return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	case TOPOLOGY_LINELIST:      return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	case TOPOLOGY_LINESTRIP:     return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case TOPOLOGY_TRIANGLELIST:  return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case TOPOLOGY_TRIANGLESTRIP: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	default:               return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
}

static UINT GetVertexCount(EPrimitiveTopology type, UINT primitiveCount)
{
	switch (type)
	{
	case TOPOLOGY_NONE:          return primitiveCount * 4;  // Tessellation patches (4 control points each)
	case TOPOLOGY_POINTLIST:     return primitiveCount;
	case TOPOLOGY_LINELIST:      return primitiveCount * 2;
	case TOPOLOGY_LINESTRIP:     return primitiveCount + 1;
	case TOPOLOGY_TRIANGLELIST:  return primitiveCount * 3;
	case TOPOLOGY_TRIANGLESTRIP: return primitiveCount + 2;
	default:               return primitiveCount * 3;
	}
}

static UINT GetIndexCount(EPrimitiveTopology type, UINT primitiveCount)
{
	switch (type)
	{
	case TOPOLOGY_NONE:          return primitiveCount * 4;  // Tessellation patches (4 control points each)
	case TOPOLOGY_POINTLIST:     return primitiveCount;
	case TOPOLOGY_LINELIST:      return primitiveCount * 2;
	case TOPOLOGY_LINESTRIP:     return primitiveCount + 1;
	case TOPOLOGY_TRIANGLELIST:  return primitiveCount * 3;
	case TOPOLOGY_TRIANGLESTRIP: return primitiveCount + 2;
	default:               return primitiveCount * 3;
	}
}

void CShaderManager::Draw(EPrimitiveTopology type, UINT startVertex, UINT primitiveCount)
{
	if (!GetActiveContext()) return;
	if (primitiveCount == 0) return;  // Prevent empty draws

	UINT vertexCount = GetVertexCount(type, primitiveCount);
	if (vertexCount == 0) return;

	// Use thread-local shader tracking on worker threads
	EShaderType eActiveShader = m_eCurrentShader;

	if (eActiveShader == SHADER_NONE)
	{
		BeginUI();
	}

	CommitRenderState();
	CommitChanges();

	SetPrimitiveTopologyIfChanged(GetD3D11Topology(type));
	GetActiveContext()->Draw(vertexCount, startVertex);
	IncrementGlobalDrawCount();
}

void CShaderManager::DrawIndexed(EPrimitiveTopology type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primitiveCount, INT baseVertex)
{
	if (!GetActiveContext())
	{
		TraceError("DrawIndexed: Context is NULL!");
		return;
	}
	if (primitiveCount == 0) return;  // Prevent empty draws

	// Use thread-local shader tracking on worker threads
	EShaderType eActiveShader = m_eCurrentShader;

	// Ensure a shader is bound — default to UI shader which is compatible
	// with PDT vertex format (POSITION+COLOR+TEXCOORD).
	if (eActiveShader == SHADER_NONE)
	{
		BeginUI();
		eActiveShader = m_eCurrentShader;
	}

	CommitRenderState();
	CommitChanges();

	{
		UINT indexCount = GetIndexCount(type, primitiveCount);
		if (indexCount == 0) return;
		if (type != TOPOLOGY_NONE)
			SetPrimitiveTopologyIfChanged(GetD3D11Topology(type));
		GetActiveContext()->DrawIndexed(indexCount, startIndex, baseVertex);
	}
	IncrementGlobalDrawCount();
}

void CShaderManager::DrawIndexed(EPrimitiveTopology type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primitiveCount)
{
	// 5-argument overload with baseVertex=0
	DrawIndexed(type, minIndex, numVertices, startIndex, primitiveCount, 0);
}

void CShaderManager::DrawIndexedInstanced(EPrimitiveTopology type, UINT indexCountPerInstance, UINT instanceCount, UINT startIndex, INT baseVertex, UINT startInstance)
{
	if (!GetActiveContext() || instanceCount == 0 || indexCountPerInstance == 0) return;

	EShaderType eActiveShader = m_eCurrentShader;
	if (eActiveShader == SHADER_NONE)
	{
		TraceError("DrawIndexedInstanced: No shader bound!");
		return;
	}

	CommitRenderState();
	CommitChanges();

	if (type != TOPOLOGY_NONE)
		SetPrimitiveTopologyIfChanged(GetD3D11Topology(type));

	GetActiveContext()->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndex, baseVertex, startInstance);
	IncrementGlobalDrawCount();
}

void CShaderManager::DrawInstanced(EPrimitiveTopology type, UINT vertexCountPerInstance, UINT instanceCount, UINT startVertex, UINT startInstance)
{
	if (!GetActiveContext() || instanceCount == 0 || vertexCountPerInstance == 0) return;

	EShaderType eActiveShader = m_eCurrentShader;
	if (eActiveShader == SHADER_NONE)
	{
		TraceError("DrawInstanced: No shader bound!");
		return;
	}

	CommitRenderState();
	CommitChanges();

	if (type != TOPOLOGY_NONE)
		SetPrimitiveTopologyIfChanged(GetD3D11Topology(type));

	GetActiveContext()->DrawInstanced(vertexCountPerInstance, instanceCount, startVertex, startInstance);
	IncrementGlobalDrawCount();
}

bool CShaderManager::CreateDynamicBuffers()
{
	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth = DYNAMIC_VB_SIZE;
	vbDesc.Usage = D3D11_USAGE_DYNAMIC;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(m_pDevice->CreateBuffer(&vbDesc, nullptr, &m_pDynamicVertexBuffer)))
		return false;

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = DYNAMIC_IB_SIZE;
	ibDesc.Usage = D3D11_USAGE_DYNAMIC;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(m_pDevice->CreateBuffer(&ibDesc, nullptr, &m_pDynamicIndexBuffer)))
		return false;

	// Initialize ring buffer tracking
	m_dwDynamicVBOffset = 0;
	m_dwDynamicIBOffset = 0;
	m_bDynamicBufferNeedsDiscard = true;

	return true;
}

void CShaderManager::ResetDynamicBuffers()
{
	// Called at frame start to reset ring buffer positions
	m_dwDynamicVBOffset = 0;
	m_dwDynamicIBOffset = 0;
	m_bDynamicBufferNeedsDiscard = true;
	m_dwSkinningPoolIndex = 0;
}

void CShaderManager::DrawDynamic(EPrimitiveTopology type, UINT primitiveCount, const void* pVertexData, UINT stride)
{
	if (!GetActiveContext() || !pVertexData) return;
	if (stride == 0 || primitiveCount == 0) return;  // Prevent division by zero and empty draws

	UINT vertexCount = GetVertexCount(type, primitiveCount);
	if (vertexCount == 0) return;

	EShaderType eActiveShader = m_eCurrentShader;
	if (eActiveShader == SHADER_NONE)
	{
		BeginUI();
	}

	UINT dataSize = vertexCount * stride;

	if (dataSize > DYNAMIC_VB_SIZE) return;

	// Select per-thread or shared dynamic buffer
	ID3D11Buffer*& pDynVB = m_pDynamicVertexBuffer;
	DWORD& dwVBOff = m_dwDynamicVBOffset;
	bool& bDiscard = m_bDynamicBufferNeedsDiscard;

	if (!pDynVB) return;

	// Check if we need to wrap around or discard
	D3D11_MAP mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	UINT bufferOffset = dwVBOff;

	if (bDiscard || (dwVBOff + dataSize > DYNAMIC_VB_SIZE))
	{
		mapType = D3D11_MAP_WRITE_DISCARD;
		bufferOffset = 0;
		dwVBOff = 0;
		bDiscard = false;
	}

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = GetActiveContext()->Map(pDynVB, 0, mapType, 0, &mapped);
	if (SUCCEEDED(hr))
	{
		memcpy((BYTE*)mapped.pData + bufferOffset, pVertexData, dataSize);
		GetActiveContext()->Unmap(pDynVB, 0);
	}
	else
	{
		TraceError("DrawDynamic: Failed to map vertex buffer (hr=0x%08X, size=%d, offset=%d)", hr, dataSize, bufferOffset);
		return;
	}

	// Update offset for next call
	dwVBOff = bufferOffset + dataSize;

	SetVertexBuffer(0, pDynVB, stride, bufferOffset);

	CommitRenderState();
	CommitChanges();

	SetPrimitiveTopologyIfChanged(GetD3D11Topology(type));
	GetActiveContext()->Draw(vertexCount, 0);
	IncrementGlobalDrawCount();
}

//--------------------------------------------------------------------
// Batched Rendering Support
//--------------------------------------------------------------------
bool CShaderManager::MapDynamicVB(UINT requiredBytes, MappedDynamicVB& outMapped)
{
	if (!GetActiveContext()) return false;

	ID3D11Buffer*& pDynVB = m_pDynamicVertexBuffer;
	DWORD& dwVBOff = m_dwDynamicVBOffset;
	bool& bDiscard = m_bDynamicBufferNeedsDiscard;

	if (!pDynVB || requiredBytes > DYNAMIC_VB_SIZE) return false;

	D3D11_MAP mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	UINT bufferOffset = dwVBOff;

	if (bDiscard || (dwVBOff + requiredBytes > DYNAMIC_VB_SIZE))
	{
		mapType = D3D11_MAP_WRITE_DISCARD;
		bufferOffset = 0;
		dwVBOff = 0;
		bDiscard = false;
	}

	D3D11_MAPPED_SUBRESOURCE mapped;
	if (FAILED(GetActiveContext()->Map(pDynVB, 0, mapType, 0, &mapped)))
		return false;

	outMapped.pData = (BYTE*)mapped.pData + bufferOffset;
	outMapped.byteOffset = bufferOffset;
	outMapped.maxBytes = DYNAMIC_VB_SIZE - bufferOffset;
	return true;
}

void CShaderManager::UnmapDynamicVB()
{
	ID3D11Buffer*& pDynVB = m_pDynamicVertexBuffer;
	if (pDynVB && GetActiveContext())
		GetActiveContext()->Unmap(pDynVB, 0);
}

void CShaderManager::AdvanceDynamicVBOffset(UINT bytesUsed)
{
	DWORD& dwVBOff = m_dwDynamicVBOffset;
	dwVBOff += bytesUsed;
}

void CShaderManager::DrawBatchedQuads(UINT stride, UINT vbByteOffset, UINT firstQuad, UINT quadCount)
{
	if (!GetActiveContext() || quadCount == 0) return;

	ID3D11Buffer*& pDynVB = m_pDynamicVertexBuffer;
	if (!pDynVB) return;

	SetVertexBuffer(0, pDynVB, stride, vbByteOffset);

	CommitRenderState();
	CommitChanges();

	SetPrimitiveTopologyIfChanged(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	GetActiveContext()->DrawIndexed(quadCount * 6, firstQuad * 6, 0);
	IncrementGlobalDrawCount();
}

void CShaderManager::DrawIndexedDynamic(EPrimitiveTopology type, UINT minIndex, UINT numVertices, UINT primitiveCount, const void* pIndexData, DXGI_FORMAT indexFormat, const void* pVertexData, UINT stride)
{
	if (!GetActiveContext() || !pVertexData || !pIndexData) return;
	if (stride == 0 || primitiveCount == 0 || numVertices == 0) return;  // Prevent division by zero and empty draws

	UINT indexCount = GetIndexCount(type, primitiveCount);
	if (indexCount == 0) return;

	EShaderType eActiveShader = m_eCurrentShader;
	if (eActiveShader == SHADER_NONE)
	{
		BeginUI();
	}

	UINT indexSize = (indexFormat == DXGI_FORMAT_R32_UINT) ? 4 : 2;
	UINT indexDataSize = indexCount * indexSize;
	UINT vertexDataSize = numVertices * stride;

	if (indexDataSize > DYNAMIC_IB_SIZE || vertexDataSize > DYNAMIC_VB_SIZE) return;

	// Select per-thread or shared dynamic buffers
	ID3D11Buffer*& pDynVB = m_pDynamicVertexBuffer;
	ID3D11Buffer*& pDynIB = m_pDynamicIndexBuffer;
	DWORD& dwVBOff = m_dwDynamicVBOffset;
	DWORD& dwIBOff = m_dwDynamicIBOffset;
	bool& bDiscard = m_bDynamicBufferNeedsDiscard;

	if (!pDynVB || !pDynIB) return;

	// Determine if we need to discard
	bool bNeedVBDiscard = bDiscard || (dwVBOff + vertexDataSize > DYNAMIC_VB_SIZE);
	bool bNeedIBDiscard = bDiscard || (dwIBOff + indexDataSize > DYNAMIC_IB_SIZE);

	// Track offsets
	UINT vbOffset = bNeedVBDiscard ? 0 : dwVBOff;
	UINT ibOffset = bNeedIBDiscard ? 0 : dwIBOff;

	// Map vertex buffer
	D3D11_MAP vbMapType = bNeedVBDiscard ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = GetActiveContext()->Map(pDynVB, 0, vbMapType, 0, &mapped);
	if (SUCCEEDED(hr))
	{
		memcpy((BYTE*)mapped.pData + vbOffset, pVertexData, vertexDataSize);
		GetActiveContext()->Unmap(pDynVB, 0);
	}
	else
	{
		TraceError("DrawIndexedDynamic: Failed to map vertex buffer (hr=0x%08X, size=%d, offset=%d)", hr, vertexDataSize, vbOffset);
		return;
	}

	// Map index buffer
	D3D11_MAP ibMapType = bNeedIBDiscard ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
	hr = GetActiveContext()->Map(pDynIB, 0, ibMapType, 0, &mapped);
	if (SUCCEEDED(hr))
	{
		memcpy((BYTE*)mapped.pData + ibOffset, pIndexData, indexDataSize);
		GetActiveContext()->Unmap(pDynIB, 0);
	}
	else
	{
		TraceError("DrawIndexedDynamic: Failed to map index buffer (hr=0x%08X, size=%d, offset=%d)", hr, indexDataSize, ibOffset);
		return;
	}

	// Update offsets for next call
	dwVBOff = bNeedVBDiscard ? vertexDataSize : (dwVBOff + vertexDataSize);
	dwIBOff = bNeedIBDiscard ? indexDataSize : (dwIBOff + indexDataSize);
	bDiscard = false;

	SetVertexBuffer(0, pDynVB, stride, vbOffset);
	SetIndexBuffer(pDynIB, indexFormat, ibOffset);

	CommitRenderState();
	CommitChanges();

	SetPrimitiveTopologyIfChanged(GetD3D11Topology(type));
	GetActiveContext()->DrawIndexed(indexCount, 0, 0);
	IncrementGlobalDrawCount();
}

void CShaderManager::SetMatrix(EMatrixSlot state, const Matrix* pMatrix)
{
	if (!pMatrix) return;

	if (state < MAX_TRANSFORMS)
	{
		m_Matrices[state] = *pMatrix;
	}
	else
	{
		return;
	}

	// Sync with shader constant buffers
	if (state == MATRIX_WORLD)
	{
		SetWorldMatrix(pMatrix);
	}
	else if (state == MATRIX_VIEW || state == MATRIX_PROJECTION)
	{
		SetViewProjection(&m_Matrices[MATRIX_VIEW], &m_Matrices[MATRIX_PROJECTION]);
	}
	else if (state == MATRIX_TEXTURE0)
	{
		SetTextureMatrix(0, pMatrix);
	}
	else if (state == MATRIX_TEXTURE1)
	{
		SetTextureMatrix(1, pMatrix);
	}
}

void CShaderManager::GetMatrix(EMatrixSlot state, Matrix* pMatrix)
{
	if (!pMatrix) return;

	if (state < MAX_TRANSFORMS)
	{
		*pMatrix = m_Matrices[state];
	}
}

void CShaderManager::SaveTransform(EMatrixSlot state, const Matrix* pMatrix)
{
	if (state < MAX_TRANSFORMS)
	{
		m_SavedMatrices[state] = m_Matrices[state];
	}
	else
	{
		return;
	}

	if (pMatrix)
		SetMatrix(state, pMatrix);
}

void CShaderManager::RestoreTransform(EMatrixSlot state)
{
	if (state < MAX_TRANSFORMS)
	{
		SetMatrix(state, &m_SavedMatrices[state]);
	}
}

void CShaderManager::SetInputLayout(EInputLayoutType type)
{
	m_CurrentInputLayout = type;
	BindForInputLayout(type);
}

void CShaderManager::SetInputLayout(ID3D11InputLayout* pLayout)
{
	if (GetActiveContext() && pLayout)
		GetActiveContext()->IASetInputLayout(pLayout);
}

void CShaderManager::SaveInputLayout(EInputLayoutType type)
{
	m_SavedInputLayout = m_CurrentInputLayout;
	SetInputLayout(type);
}

void CShaderManager::RestoreInputLayout()
{
	SetInputLayout(m_SavedInputLayout);
}

//--------------------------------------------------------------------
// Sampler State Management
//--------------------------------------------------------------------

void CShaderManager::SetSamplerState(UINT slot, ESamplerState state, DWORD value)
{
	if (slot >= MAX_SAMPLER_SLOTS || !GetActiveContext()) return;

	// Use thread-local sampler state on worker threads
	SamplerSlotState* pSlotStates = m_SamplerStates;

	// Store the state value
	switch (state)
	{
	case SAMPLER_MINFILTER: pSlotStates[slot].minFilter = value; break;
	case SAMPLER_MAGFILTER: pSlotStates[slot].magFilter = value; break;
	case SAMPLER_MIPFILTER: pSlotStates[slot].mipFilter = value; break;
	case SAMPLER_ADDRESSU:  pSlotStates[slot].addressU = value; break;
	case SAMPLER_ADDRESSV:  pSlotStates[slot].addressV = value; break;
	default: return;
	}

	// Create and set sampler state
	if (m_pStateCache)
	{
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;  // Default
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.MipLODBias = 0.0f;
		desc.MaxAnisotropy = 1;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		desc.MinLOD = 0;
		desc.MaxLOD = D3D11_FLOAT32_MAX;
		desc.BorderColor[0] = 1.0f;
		desc.BorderColor[1] = 1.0f;
		desc.BorderColor[2] = 1.0f;
		desc.BorderColor[3] = 1.0f;

		// Apply stored state
		bool bMinPoint = (pSlotStates[slot].minFilter == FILTER_POINT);
		bool bMagPoint = (pSlotStates[slot].magFilter == FILTER_POINT);
		bool bMipNone = (pSlotStates[slot].mipFilter == FILTER_NONE);

		if (bMinPoint && bMagPoint)
			desc.Filter = bMipNone ? D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
		else if (bMinPoint)
			desc.Filter = bMipNone ? D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT : D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
		else if (bMagPoint)
			desc.Filter = bMipNone ? D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR : D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
		else
			desc.Filter = bMipNone ? D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_LINEAR;

		if (pSlotStates[slot].minFilter == FILTER_ANISOTROPIC || pSlotStates[slot].magFilter == FILTER_ANISOTROPIC)
		{
			desc.Filter = D3D11_FILTER_ANISOTROPIC;
			desc.MaxAnisotropy = 16;
		}

		switch (pSlotStates[slot].addressU)
		{
		case ADDRESS_WRAP:   desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP; break;
		case ADDRESS_MIRROR: desc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR; break;
		case ADDRESS_CLAMP:  desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP; break;
		case ADDRESS_BORDER: desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER; break;
		}
		switch (pSlotStates[slot].addressV)
		{
		case ADDRESS_WRAP:   desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP; break;
		case ADDRESS_MIRROR: desc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR; break;
		case ADDRESS_CLAMP:  desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP; break;
		case ADDRESS_BORDER: desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER; break;
		}

		ID3D11SamplerState* pSampler = m_pStateCache->GetSamplerState(desc);
		if (pSampler)
			GetActiveContext()->PSSetSamplers(slot, 1, &pSampler);
	}
}

void CShaderManager::SaveSamplerState(UINT slot, ESamplerState state, DWORD value)
{
	if (slot >= MAX_SAMPLER_SLOTS) return;

	SamplerSlotState* pSlotStates = m_SamplerStates;
	auto& savedMap = m_SavedSamplerStates;

	// Save current value
	DWORD currentValue = 0;
	switch (state)
	{
	case SAMPLER_MINFILTER: currentValue = pSlotStates[slot].minFilter; break;
	case SAMPLER_MAGFILTER: currentValue = pSlotStates[slot].magFilter; break;
	case SAMPLER_MIPFILTER: currentValue = pSlotStates[slot].mipFilter; break;
	case SAMPLER_ADDRESSU:  currentValue = pSlotStates[slot].addressU; break;
	case SAMPLER_ADDRESSV:  currentValue = pSlotStates[slot].addressV; break;
	default: return;
	}
	savedMap[slot][state] = currentValue;

	// Set new value
	SetSamplerState(slot, state, value);
}

void CShaderManager::RestoreSamplerState(UINT slot, ESamplerState state)
{
	auto& savedMap = m_SavedSamplerStates;

	auto slotIt = savedMap.find(slot);
	if (slotIt == savedMap.end()) return;

	auto stateIt = slotIt->second.find(state);
	if (stateIt == slotIt->second.end()) return;

	SetSamplerState(slot, state, stateIt->second);
	slotIt->second.erase(stateIt);
}

//--------------------------------------------------------------------
// Fog/Lighting/AlphaTest State
//--------------------------------------------------------------------

void CShaderManager::SetFogEnabled(bool bEnabled)
{
	m_bFogEnabled = bEnabled;
	m_cbPerFrame.vFogParams.w = bEnabled ? 1.0f : 0.0f;
	m_bPerFrameDirty = true;
	ReapplyVolFogDials();
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

void CShaderManager::SetFogParams(float fStart, float fEnd, DWORD dwColor)
{
	m_cbPerFrame.vFogParams.x = fStart;
	m_cbPerFrame.vFogParams.y = fEnd;
	m_bPerFrameDirty = true;
	SetFogColor(dwColor);
	ReapplyVolFogDials();
}

void CShaderManager::SetBestFiltering(UINT slot)
{
	if (slot >= MAX_SAMPLER_SLOTS) return;
	SetSamplerState(slot, SAMPLER_MINFILTER, FILTER_LINEAR);
	SetSamplerState(slot, SAMPLER_MAGFILTER, FILTER_LINEAR);
	SetSamplerState(slot, SAMPLER_MIPFILTER, FILTER_LINEAR);
}

void CShaderManager::SetAlphaTestEnabled(bool bEnabled)
{
	m_bAlphaTestEnabled = bEnabled;
	m_cbPerObject.vMaterialParams.y = bEnabled ? 1.0f : 0.0f;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetAlphaTestRefByte(DWORD dwRef)
{
	m_dwAlphaTestRef = dwRef;
	m_cbPerObject.vMaterialParams.x = (float)dwRef / 255.0f;
	m_bPerObjectDirty = true;
}

//--------------------------------------------------------------------
// Legacy Material API
//--------------------------------------------------------------------

void CShaderManager::SetMaterial(const TMaterial* pMaterial)
{
	if (!pMaterial) return;

	m_CurrentMaterial = *pMaterial;

	SetDiffuseColor(pMaterial->Diffuse.r, pMaterial->Diffuse.g, pMaterial->Diffuse.b, pMaterial->Diffuse.a);
	SetSpecularColor(pMaterial->Specular.r, pMaterial->Specular.g, pMaterial->Specular.b);
	SetEmissiveColor(pMaterial->Emissive.r, pMaterial->Emissive.g, pMaterial->Emissive.b);
	SetMaterial(pMaterial->Power);
}

void CShaderManager::GetMaterial(TMaterial* pMaterial) const
{
	if (pMaterial)
		*pMaterial = m_CurrentMaterial;
}

void CShaderManager::SaveMaterial()
{
	m_SavedMaterial = m_CurrentMaterial;
}

void CShaderManager::RestoreMaterial()
{
	SetMaterial(&m_SavedMaterial);
}

//--------------------------------------------------------------------
// Legacy Light API
//--------------------------------------------------------------------

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

void CShaderManager::LightEnable(UINT index, BOOL bEnable)
{
	EnableLight(index, bEnable != FALSE);
}

//--------------------------------------------------------------------
// GPU Skinning - Bone Matrix Upload
//--------------------------------------------------------------------

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

//--------------------------------------------------------------------
// Multithreaded Rendering Support (Deferred Contexts)
//--------------------------------------------------------------------

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

void CShaderManager::BindShaderToContext(ID3D11DeviceContext* pDeferredCtx, EShaderType type)
{
	if (!pDeferredCtx || type < 0 || type >= SHADER_COUNT)
		return;

	const ShaderProgram& shader = m_Shaders[type];

	// Bind vertex shader
	pDeferredCtx->VSSetShader(shader.pVertexShader, nullptr, 0);

	// Bind pixel shader
	pDeferredCtx->PSSetShader(shader.pPixelShader, nullptr, 0);

	// Bind input layout
	if (shader.pInputLayout)
		pDeferredCtx->IASetInputLayout(shader.pInputLayout);

	// Bind default sampler states
	ID3D11SamplerState* samplers[] = { m_pSamplerLinear, m_pSamplerLinear };
	pDeferredCtx->PSSetSamplers(0, 2, samplers);
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

void CShaderManager::SetSpeedTreeTreePosition(const float* p)
{
	if (!p) return;
	m_cbSpeedTree.vTreePos = XMFLOAT4(p[0], p[1], p[2], p[3]);
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

void CShaderManager::SetSpeedTreeLeafLightingAdjustment(const float* p)
{
	if (!p) return;
	m_cbSpeedTree.vLeafLightingAdj = XMFLOAT4(p[0], p[1], p[2], p[3]);
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

void CShaderManager::SetSpeedTreeMaterial(const float* p)
{
	if (!p) return;
	m_cbSpeedTree.vMaterialDiffuse = XMFLOAT4(p[0], p[1], p[2], p[3]);
	m_cbSpeedTree.vMaterialAmbient = XMFLOAT4(p[4], p[5], p[6], p[7]);
	m_bSpeedTreeDirty = true;
}

void CShaderManager::SetSpeedTreeFogParams(const float* p)
{
	if (!p) return;
	m_cbSpeedTree.vFogParams = XMFLOAT4(p[0], p[1], p[2], p[3]);
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

//--------------------------------------------------------------------
// God Rays (Volumetric Light Scattering)
//--------------------------------------------------------------------

void CShaderManager::SetGodRaysParams(float fScreenX, float fScreenY, float fIntensity, float fDecay)
{
	m_cbGodRays.vLightScreenPos.x = fScreenX;
	m_cbGodRays.vLightScreenPos.y = fScreenY;
	m_cbGodRays.vLightScreenPos.z = fIntensity;
	m_cbGodRays.vLightScreenPos.w = fDecay;
	m_bGodRaysDirty = true;
}

void CShaderManager::SetGodRaysRayParams(float fDensity, float fWeight, float fExposure, int nSamples)
{
	m_cbGodRays.vRayParams.x = fDensity;
	m_cbGodRays.vRayParams.y = fWeight;
	m_cbGodRays.vRayParams.z = fExposure;
	m_cbGodRays.vRayParams.w = (float)nSamples;
	m_bGodRaysDirty = true;
}

void CShaderManager::SetGodRaysColor(float r, float g, float b)
{
	m_cbGodRays.vRayColor.x = r;
	m_cbGodRays.vRayColor.y = g;
	m_cbGodRays.vRayColor.z = b;
	m_bGodRaysDirty = true;
}

#ifdef ENABLE_GODRAYS
void CShaderManager::RenderGodRaysPass(
	ID3D11ShaderResourceView* pSceneSRV,
	ID3D11RenderTargetView* pGodRaysRTV,
	UINT w, UINT h)
{
	if (!m_bGodRaysEnabled || !pSceneSRV || !pGodRaysRTV || !m_pContext)
		return;

	// Fullscreen quad vertices (NDC: -1..1, UV: 0..1)
	struct PTVertex { float x, y, z; float u, v; };
	PTVertex quad[4] = {
		{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
		{  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
		{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
		{  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
	};

	// Clear god rays RTV to black
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_pContext->ClearRenderTargetView(pGodRaysRTV, clearColor);

	// Set viewport to quarter res
	D3D11_VIEWPORT vpGodRays = { 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f };
	m_pContext->RSSetViewports(1, &vpGodRays);

	// Bind god rays RTV (no depth)
	ID3D11ShaderResourceView* nullSRV = nullptr;
	m_pContext->PSSetShaderResources(0, 1, &nullSRV);
	m_pContext->OMSetRenderTargets(1, &pGodRaysRTV, nullptr);

	// Bind god rays shader + CB
	BeginGodRays();

	// Update god rays constant buffer if dirty
	if (m_bGodRaysDirty && m_pCBGodRays)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(m_pContext->Map(m_pCBGodRays.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbGodRays, sizeof(m_cbGodRays));
			m_pContext->Unmap(m_pCBGodRays.Get(), 0);
			m_bGodRaysDirty = false;
		}
	}

	// Bind scene texture SRV to t0
	m_pContext->PSSetShaderResources(0, 1, &pSceneSRV);

	// Draw fullscreen quad
	DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PTVertex));

	// Unbind SRV from t0
	m_pContext->PSSetShaderResources(0, 1, &nullSRV);
}
#endif // ENABLE_GODRAYS

#ifdef ENABLE_BLOOM
void CShaderManager::RenderBloom(
	ID3D11ShaderResourceView* pSceneSRV,
	ID3D11RenderTargetView* pBloomRTA_RTV, ID3D11ShaderResourceView* pBloomRTA_SRV,
	ID3D11RenderTargetView* pBloomRTB_RTV, ID3D11ShaderResourceView* pBloomRTB_SRV,
	UINT bloomW, UINT bloomH,
	ID3D11ShaderResourceView* pGodRaysSRV,
	ID3D11ShaderResourceView* pSSAO_SRV,
	ID3D11RenderTargetView* pOutputRTV, UINT outputW, UINT outputH)
{
	if (!m_bBloomEnabled || !pSceneSRV || !m_pContext || !m_pCBBloom)
		return;

	// Fullscreen quad vertices (NDC: -1..1, UV: 0..1)
	struct PTVertex { float x, y, z; float u, v; };
	PTVertex quad[4] = {
		{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
		{  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
		{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
		{  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
	};

	// Save state
	EShaderType savedShader = m_eCurrentShader;

	// Disable depth testing for post-process
	DWORD savedZEnable = GetPipelineState(PSTATE_DEPTHENABLE);
	DWORD savedZWrite = GetPipelineState(PSTATE_DEPTHWRITEMASK);
	SetPipelineState(PSTATE_DEPTHENABLE, FALSE);
	SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	SetPipelineState(PSTATE_BLENDENABLE, FALSE);
	CommitRenderState();

	{
		ID3D11ShaderResourceView* nullSRV = nullptr;
		m_pContext->PSSetShaderResources(0, 1, &nullSRV);  // Unbind scene SRV from output

		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		m_pContext->ClearRenderTargetView(pBloomRTA_RTV, clearColor);

		m_pContext->OMSetRenderTargets(1, &pBloomRTA_RTV, nullptr);

		D3D11_VIEWPORT vpBloom = { 0.0f, 0.0f, (float)bloomW, (float)bloomH, 0.0f, 1.0f };
		m_pContext->RSSetViewports(1, &vpBloom);

		BeginBloomBright();

		// Update CB
		m_cbBloom.vTexelSize = XMFLOAT4(1.0f / bloomW, 1.0f / bloomH, 0.0f, 0.0f);
		m_cbBloom.vBlurDirection = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(m_pContext->Map(m_pCBBloom, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbBloom, sizeof(m_cbBloom));
			m_pContext->Unmap(m_pCBBloom, 0);
		}

		m_pContext->PSSetShaderResources(0, 1, &pSceneSRV);
		DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PTVertex));
	}

	// --- Pass 2: Horizontal blur (bloom RTA -> bloom RTB) ---
	{
		ID3D11ShaderResourceView* nullSRV = nullptr;
		m_pContext->PSSetShaderResources(0, 1, &nullSRV);

		m_pContext->OMSetRenderTargets(1, &pBloomRTB_RTV, nullptr);

		BeginBloomBlur();

		m_cbBloom.vBlurDirection = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(m_pContext->Map(m_pCBBloom, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbBloom, sizeof(m_cbBloom));
			m_pContext->Unmap(m_pCBBloom, 0);
		}

		m_pContext->PSSetShaderResources(0, 1, &pBloomRTA_SRV);
		DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PTVertex));
	}

	// --- Pass 3: Vertical blur (bloom RTB -> bloom RTA) ---
	{
		ID3D11ShaderResourceView* nullSRV = nullptr;
		m_pContext->PSSetShaderResources(0, 1, &nullSRV);

		m_pContext->OMSetRenderTargets(1, &pBloomRTA_RTV, nullptr);

		// Reuse bloom blur shader (already bound)

		m_cbBloom.vBlurDirection = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(m_pContext->Map(m_pCBBloom, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbBloom, sizeof(m_cbBloom));
			m_pContext->Unmap(m_pCBBloom, 0);
		}

		m_pContext->PSSetShaderResources(0, 1, &pBloomRTB_SRV);
		DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PTVertex));
	}

	{
		ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
		m_pContext->PSSetShaderResources(0, 4, nullSRVs);

		m_pContext->OMSetRenderTargets(1, &pOutputRTV, nullptr);

		D3D11_VIEWPORT vpFull = { 0.0f, 0.0f, (float)outputW, (float)outputH, 0.0f, 1.0f };
		m_pContext->RSSetViewports(1, &vpFull);

		BeginBloomComposite();

		m_pContext->PSSetShaderResources(0, 1, &pSceneSRV);
		m_pContext->PSSetShaderResources(1, 1, &pBloomRTA_SRV);
		m_pContext->PSSetShaderResources(2, 1, pGodRaysSRV ? &pGodRaysSRV : &nullSRVs[0]);
		ID3D11ShaderResourceView* pSSAOBind = pSSAO_SRV ? pSSAO_SRV : m_pDefaultTextureSRV;
		m_pContext->PSSetShaderResources(3, 1, &pSSAOBind);
		DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PTVertex));
	}

	{
		ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
		m_pContext->PSSetShaderResources(0, 4, nullSRVs);

		// Restore MSAA render target + depth stencil
		ID3D11RenderTargetView* pRTV = CGraphicBase::GetRenderTargetView();
		ID3D11DepthStencilView* pDSV = CGraphicBase::GetDepthStencilView();
		m_pContext->OMSetRenderTargets(1, &pRTV, pDSV);

		// Restore viewport
		D3D11_VIEWPORT vpRestore = CGraphicBase::GetViewport();
		m_pContext->RSSetViewports(1, &vpRestore);
	}

	// Restore depth state
	SetPipelineState(PSTATE_DEPTHENABLE, savedZEnable);
	SetPipelineState(PSTATE_DEPTHWRITEMASK, savedZWrite);
	CommitRenderState();

	// Restore previous shader
	if (savedShader != SHADER_NONE)
		BindShader(savedShader);
	else
		End();
}
#endif // ENABLE_BLOOM

#ifdef ENABLE_SSAO
void CShaderManager::SetSSAOEnabled(bool bEnabled)
{
	CGraphicBase::SetSSAOEnabled(bEnabled);
}

void CShaderManager::RenderDepthResolve(
	ID3D11ShaderResourceView* pMSAADepthSRV,
	ID3D11RenderTargetView* pResolvedRTV,
	UINT w, UINT h)
{
	if (!pMSAADepthSRV || !pResolvedRTV || !m_pContext)
		return;

	struct PTVertex { float x, y, z; float u, v; };
	PTVertex quad[4] = {
		{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
		{  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
		{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
		{  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
	};

	// Save state
	DWORD savedZEnable = GetPipelineState(PSTATE_DEPTHENABLE);
	DWORD savedZWrite = GetPipelineState(PSTATE_DEPTHWRITEMASK);
	SetPipelineState(PSTATE_DEPTHENABLE, FALSE);
	SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	SetPipelineState(PSTATE_BLENDENABLE, FALSE);
	CommitRenderState();

	ID3D11ShaderResourceView* nullSRV = nullptr;
	m_pContext->PSSetShaderResources(0, 1, &nullSRV);
	m_pContext->OMSetRenderTargets(1, &pResolvedRTV, nullptr);

	D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f };
	m_pContext->RSSetViewports(1, &vp);

	BindShader(SHADER_DEPTH_RESOLVE);
	m_pContext->PSSetShaderResources(0, 1, &pMSAADepthSRV);
	m_pContext->PSSetSamplers(1, 1, &m_pSamplerPoint);
	DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PTVertex));

	m_pContext->PSSetShaderResources(0, 1, &nullSRV);

	SetPipelineState(PSTATE_DEPTHENABLE, savedZEnable);
	SetPipelineState(PSTATE_DEPTHWRITEMASK, savedZWrite);
	CommitRenderState();
}

void CShaderManager::RenderSSAOPass(
	ID3D11ShaderResourceView* pDepthSRV,
	ID3D11RenderTargetView* pSSAO_RTV,
	UINT ssaoW, UINT ssaoH, UINT fullW, UINT fullH)
{
	if (!pDepthSRV || !pSSAO_RTV || !m_pContext || !m_pCBSSAO)
		return;

	struct PTVertex { float x, y, z; float u, v; };
	PTVertex quad[4] = {
		{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
		{  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
		{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
		{  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
	};

	// Save state
	DWORD savedZEnable = GetPipelineState(PSTATE_DEPTHENABLE);
	DWORD savedZWrite = GetPipelineState(PSTATE_DEPTHWRITEMASK);
	SetPipelineState(PSTATE_DEPTHENABLE, FALSE);
	SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	SetPipelineState(PSTATE_BLENDENABLE, FALSE);
	CommitRenderState();

	// Update CBSSAO
	m_cbSSAO.matProjection = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&CGraphicBase::GetProjectionMatrix()));
	XMMATRIX matProj = m_cbSSAO.matProjection;
	XMVECTOR det;
	m_cbSSAO.matInvProjection = XMMatrixInverse(&det, matProj);
	m_cbSSAO.vTexelSize = XMFLOAT4(1.0f / ssaoW, 1.0f / ssaoH, 1.0f / fullW, 1.0f / fullH);

	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(m_pContext->Map(m_pCBSSAO, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &m_cbSSAO, sizeof(m_cbSSAO));
		m_pContext->Unmap(m_pCBSSAO, 0);
	}

	// Clear SSAO RT to white (1.0 = no occlusion)
	float clearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	m_pContext->ClearRenderTargetView(pSSAO_RTV, clearColor);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	m_pContext->PSSetShaderResources(0, 1, &nullSRV);
	m_pContext->PSSetShaderResources(1, 1, &nullSRV);
	m_pContext->OMSetRenderTargets(1, &pSSAO_RTV, nullptr);

	D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)ssaoW, (float)ssaoH, 0.0f, 1.0f };
	m_pContext->RSSetViewports(1, &vp);

	BindShader(SHADER_SSAO);

	// Bind CBSSAO at PS slot b0
	m_pContext->PSSetConstantBuffers(0, 1, &m_pCBSSAO);

	// Bind depth at t0, noise at t1
	m_pContext->PSSetShaderResources(0, 1, &pDepthSRV);
	m_pContext->PSSetShaderResources(1, 1, &m_pSSAONoiseSRV);

	// Point sampler at s1
	m_pContext->PSSetSamplers(1, 1, &m_pSamplerPoint);

	DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PTVertex));

	// Unbind
	m_pContext->PSSetShaderResources(0, 1, &nullSRV);
	m_pContext->PSSetShaderResources(1, 1, &nullSRV);

	SetPipelineState(PSTATE_DEPTHENABLE, savedZEnable);
	SetPipelineState(PSTATE_DEPTHWRITEMASK, savedZWrite);
	CommitRenderState();
}

void CShaderManager::RenderSSAOBlur(
	ID3D11ShaderResourceView* pSSAO_SRV,
	ID3D11ShaderResourceView* pDepthSRV,
	ID3D11RenderTargetView* pBlurRTV,
	UINT w, UINT h)
{
	if (!pSSAO_SRV || !pDepthSRV || !pBlurRTV || !m_pContext || !m_pCBSSAO)
		return;

	struct PTVertex { float x, y, z; float u, v; };
	PTVertex quad[4] = {
		{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
		{  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
		{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
		{  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
	};

	// Save state
	DWORD savedZEnable = GetPipelineState(PSTATE_DEPTHENABLE);
	DWORD savedZWrite = GetPipelineState(PSTATE_DEPTHWRITEMASK);
	SetPipelineState(PSTATE_DEPTHENABLE, FALSE);
	SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	SetPipelineState(PSTATE_BLENDENABLE, FALSE);
	CommitRenderState();

	// Update texel size for blur dimensions
	m_cbSSAO.vTexelSize.x = 1.0f / w;
	m_cbSSAO.vTexelSize.y = 1.0f / h;
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(m_pContext->Map(m_pCBSSAO, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &m_cbSSAO, sizeof(m_cbSSAO));
		m_pContext->Unmap(m_pCBSSAO, 0);
	}

	float clearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	m_pContext->ClearRenderTargetView(pBlurRTV, clearColor);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	m_pContext->PSSetShaderResources(0, 1, &nullSRV);
	m_pContext->PSSetShaderResources(1, 1, &nullSRV);
	m_pContext->OMSetRenderTargets(1, &pBlurRTV, nullptr);

	D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f };
	m_pContext->RSSetViewports(1, &vp);

	BindShader(SHADER_SSAO_BLUR);

	// Bind CBSSAO at PS slot b0
	m_pContext->PSSetConstantBuffers(0, 1, &m_pCBSSAO);

	// Bind SSAO at t0, depth at t1
	m_pContext->PSSetShaderResources(0, 1, &pSSAO_SRV);
	m_pContext->PSSetShaderResources(1, 1, &pDepthSRV);

	// Point sampler at s1
	m_pContext->PSSetSamplers(1, 1, &m_pSamplerPoint);

	DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PTVertex));

	// Unbind
	m_pContext->PSSetShaderResources(0, 1, &nullSRV);
	m_pContext->PSSetShaderResources(1, 1, &nullSRV);

	SetPipelineState(PSTATE_DEPTHENABLE, savedZEnable);
	SetPipelineState(PSTATE_DEPTHWRITEMASK, savedZWrite);
	CommitRenderState();
}
#endif // ENABLE_SSAO

//--------------------------------------------------------------------
// GPU Compute Shader Support
//--------------------------------------------------------------------

bool CShaderManager::CompileComputeShader(EComputeShader type, const char* szCSFile, const char* szEntryPoint)
{
	if (!m_pDevice || type < 0 || type >= CS_COUNT || !szCSFile || !szEntryPoint) return false;
	if (m_ComputeShaders[type]) { m_ComputeShaders[type]->Release(); m_ComputeShaders[type] = nullptr; }

	CMappedFile csFile;
	LPCVOID pCSData = nullptr;
	if (!CEterPackManager::Instance().Get(csFile, szCSFile, &pCSData) || !pCSData || !csFile.Size())
	{
		TraceError("CompileComputeShader(%d): '%s' not found in pack", type, szCSFile);
		return false;
	}

	const UINT FNV_PRIME = 16777619u;
	UINT hash = 2166136261u;
	auto hashBytes = [&](const void* data, size_t size)
	{
		const BYTE* p = static_cast<const BYTE*>(data);
		for (size_t i = 0; i < size; ++i) { hash ^= p[i]; hash *= FNV_PRIME; }
	};
	hashBytes(pCSData, csFile.Size());
	hashBytes(szEntryPoint, strlen(szEntryPoint));
#ifdef _DEBUG
	static const char profile[] = "cs_5_0|DEBUG_SKIP_OPT";
#else
	static const char profile[] = "cs_5_0|OPT3";
#endif
	hashBytes(profile, sizeof(profile) - 1);

	char csoPath[MAX_PATH], hashPath[MAX_PATH];
	sprintf_s(csoPath, "%s\\compute_%d.cso", GetShaderCachePath(), type);
	sprintf_s(hashPath, "%s\\compute_%d.hash", GetShaderCachePath(), type);
	ID3DBlob* pCSBlob = nullptr;
	FILE* fp = nullptr;
	UINT storedHash = 0;

	if (fopen_s(&fp, hashPath, "rb") == 0 && fp)
	{
		fread(&storedHash, sizeof(storedHash), 1, fp); fclose(fp); fp = nullptr;
		if (storedHash == hash && fopen_s(&fp, csoPath, "rb") == 0 && fp)
		{
			fseek(fp, 0, SEEK_END); long size = ftell(fp); fseek(fp, 0, SEEK_SET);
			if (size > 0 && SUCCEEDED(D3DCreateBlob(size, &pCSBlob))) fread(pCSBlob->GetBufferPointer(), 1, size, fp);
			fclose(fp); fp = nullptr;
		}
	}

	if (pCSBlob)
	{
		HRESULT hr = m_pDevice->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(), nullptr, &m_ComputeShaders[type]);
		pCSBlob->Release(); pCSBlob = nullptr;
		if (SUCCEEDED(hr)) return true;
		TraceError("CompileComputeShader(%d): cached CSO invalid, recompiling '%s'", type, szCSFile);
	}

	ID3DBlob* pErrorBlob = nullptr;
	UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#ifdef _DEBUG
	flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	HRESULT hr = D3DCompile(pCSData, csFile.Size(), szCSFile, nullptr, nullptr, szEntryPoint, "cs_5_0", flags, 0, &pCSBlob, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob) { TraceError("CompileComputeShader(%d) [%s]: %s", type, szCSFile, (char*)pErrorBlob->GetBufferPointer()); pErrorBlob->Release(); }
		return false;
	}
	if (pErrorBlob) pErrorBlob->Release();

	hr = m_pDevice->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(), nullptr, &m_ComputeShaders[type]);
	if (FAILED(hr)) { TraceError("CompileComputeShader(%d): CreateComputeShader failed (hr=0x%08X)", type, hr); pCSBlob->Release(); return false; }

	if (fopen_s(&fp, csoPath, "wb") == 0 && fp) { fwrite(pCSBlob->GetBufferPointer(), 1, pCSBlob->GetBufferSize(), fp); fclose(fp); }
	if (fopen_s(&fp, hashPath, "wb") == 0 && fp) { fwrite(&hash, sizeof(hash), 1, fp); fclose(fp); }
	pCSBlob->Release();
	return true;
}

void CShaderManager::DispatchCompute(EComputeShader type, UINT groupsX, UINT groupsY, UINT groupsZ)
{
	if (type < 0 || type >= CS_COUNT || !m_ComputeShaders[type] || !GetActiveContext())
		return;

	GetActiveContext()->CSSetShader(m_ComputeShaders[type], nullptr, 0);
	GetActiveContext()->Dispatch(groupsX, groupsY, groupsZ);
}

bool CShaderManager::CreateStructuredBuffer(UINT elementSize, UINT elementCount, bool bCpuWrite, GpuBuffer& outBuffer)
{
	if (!m_pDevice || elementSize == 0 || elementCount == 0)
		return false;

	ReleaseGpuBuffer(outBuffer);

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = elementSize * elementCount;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = elementSize;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	if (bCpuWrite)
	{
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}
	else
	{
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}

	HRESULT hr = m_pDevice->CreateBuffer(&desc, nullptr, &outBuffer.pBuffer);
	if (FAILED(hr))
	{
		TraceError("CreateStructuredBuffer: CreateBuffer failed (size=%d, hr=0x%08X)", desc.ByteWidth, hr);
		return false;
	}

	// Create SRV
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = elementCount;

	hr = m_pDevice->CreateShaderResourceView(outBuffer.pBuffer, &srvDesc, &outBuffer.pSRV);
	if (FAILED(hr))
	{
		TraceError("CreateStructuredBuffer: CreateSRV failed (hr=0x%08X)", hr);
		ReleaseGpuBuffer(outBuffer);
		return false;
	}

	// Create UAV for non-CPU-write buffers
	if (!bCpuWrite)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = elementCount;

		hr = m_pDevice->CreateUnorderedAccessView(outBuffer.pBuffer, &uavDesc, &outBuffer.pUAV);
		if (FAILED(hr))
		{
			TraceError("CreateStructuredBuffer: CreateUAV failed (hr=0x%08X)", hr);
			ReleaseGpuBuffer(outBuffer);
			return false;
		}
	}

	outBuffer.elementCount = elementCount;
	outBuffer.elementSize = elementSize;
	return true;
}

bool CShaderManager::CreateRawVertexUAVBuffer(UINT byteWidth, GpuBuffer& outBuffer)
{
	if (!m_pDevice || byteWidth == 0)
		return false;

	ReleaseGpuBuffer(outBuffer);

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = byteWidth;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_UNORDERED_ACCESS;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

	HRESULT hr = m_pDevice->CreateBuffer(&desc, nullptr, &outBuffer.pBuffer);
	if (FAILED(hr))
	{
		TraceError("CreateRawVertexUAVBuffer: CreateBuffer failed (size=%d, hr=0x%08X)", byteWidth, hr);
		return false;
	}

	// Create raw UAV (RWByteAddressBuffer in HLSL)
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = byteWidth / 4;
	uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

	hr = m_pDevice->CreateUnorderedAccessView(outBuffer.pBuffer, &uavDesc, &outBuffer.pUAV);
	if (FAILED(hr))
	{
		TraceError("CreateRawVertexUAVBuffer: CreateUAV failed (hr=0x%08X)", hr);
		ReleaseGpuBuffer(outBuffer);
		return false;
	}

	outBuffer.elementCount = byteWidth;
	outBuffer.elementSize = 1;
	return true;
}

void CShaderManager::ReleaseGpuBuffer(GpuBuffer& buffer)
{
	if (buffer.pUAV) { buffer.pUAV->Release(); buffer.pUAV = nullptr; }
	if (buffer.pSRV) { buffer.pSRV->Release(); buffer.pSRV = nullptr; }
	if (buffer.pBuffer) { buffer.pBuffer->Release(); buffer.pBuffer = nullptr; }
	buffer.elementCount = 0;
	buffer.elementSize = 0;
}

void CShaderManager::CSSetSRV(UINT slot, ID3D11ShaderResourceView* pSRV)
{
	if (GetActiveContext())
		GetActiveContext()->CSSetShaderResources(slot, 1, &pSRV);
}

void CShaderManager::CSSetUAV(UINT slot, ID3D11UnorderedAccessView* pUAV)
{
	if (GetActiveContext())
	{
		UINT initialCount = (UINT)-1;
		GetActiveContext()->CSSetUnorderedAccessViews(slot, 1, &pUAV, &initialCount);
	}
}

void CShaderManager::CSSetCB(UINT slot, ID3D11Buffer* pCB)
{
	if (GetActiveContext())
		GetActiveContext()->CSSetConstantBuffers(slot, 1, &pCB);
}

void CShaderManager::CSUnbindResources()
{
	if (!GetActiveContext()) return;

	ID3D11ShaderResourceView* nullSRV = nullptr;
	ID3D11UnorderedAccessView* nullUAV = nullptr;
	UINT initialCount = (UINT)-1;

	GetActiveContext()->CSSetShaderResources(0, 1, &nullSRV);
	GetActiveContext()->CSSetUnorderedAccessViews(0, 1, &nullUAV, &initialCount);
	GetActiveContext()->CSSetShader(nullptr, nullptr, 0);
}

//////////////////////////////////////////////////////////////////////////
// Particle Compute Shader Billboard System
//////////////////////////////////////////////////////////////////////////

bool CShaderManager::InitParticleCSResources()
{
	if (!m_pDevice || !m_ComputeShaders[CS_PARTICLE_BILLBOARD])
		return false;

	// Verify SHADER_PARTICLE_PCT compiled successfully
	if (!m_Shaders[SHADER_PARTICLE_PCT].pVertexShader)
		return false;

	// Create input structured buffer (DYNAMIC, CPU write)
	if (!CreateStructuredBuffer(sizeof(ParticleGPUInput), MAX_CS_PARTICLES, true, m_particleCSInput))
	{
		TraceError("InitParticleCSResources: Failed to create input structured buffer");
		return false;
	}

	// Create output raw VB+UAV buffer
	UINT outputVBSize = MAX_CS_QUADS * 4 * 24;  // 4 verts * 24 bytes per vert
	if (!CreateRawVertexUAVBuffer(outputVBSize, m_particleCSOutput))
	{
		TraceError("InitParticleCSResources: Failed to create output VB/UAV buffer");
		return false;
	}

	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.ByteWidth = sizeof(CBParticleCS);
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBParticleCS)))
	{
		TraceError("InitParticleCSResources: Failed to create CB");
		return false;
	}

	// Create index buffer for CS output quads
	UINT ibSize = MAX_CS_QUADS * 6 * sizeof(WORD);
	std::vector<WORD> indices(MAX_CS_QUADS * 6);
	for (UINT q = 0; q < MAX_CS_QUADS; ++q)
	{
		WORD base = (WORD)(q * 4);
		indices[q * 6 + 0] = base + 0;
		indices[q * 6 + 1] = base + 2;
		indices[q * 6 + 2] = base + 1;
		indices[q * 6 + 3] = base + 2;
		indices[q * 6 + 4] = base + 3;
		indices[q * 6 + 5] = base + 1;
	}

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = ibSize;
	ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = indices.data();
	if (FAILED(m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pParticleCSIB)))
	{
		TraceError("InitParticleCSResources: Failed to create IB");
		return false;
	}

	m_bComputeParticlesAvailable = true;
	Tracef("InitParticleCSResources: Particle CS billboard system ready (max %d particles)\n", MAX_CS_PARTICLES);
	return true;
}

bool CShaderManager::DispatchParticleBillboardCS(const ParticleGPUInput* pParticles, UINT count,
	UINT facesPerParticle, const float fRotations[3], const Matrix* pAttachMatrix)
{
	if (!m_bComputeParticlesAvailable || !pParticles || count == 0)
		return false;

	if (count > MAX_CS_PARTICLES)
		count = MAX_CS_PARTICLES;

	ID3D11DeviceContext* pCtx = GetActiveContext();
	if (!pCtx) return false;

	// Upload particle data to structured buffer
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (FAILED(pCtx->Map(m_particleCSInput.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return false;
	memcpy(mapped.pData, pParticles, count * sizeof(ParticleGPUInput));
	pCtx->Unmap(m_particleCSInput.pBuffer, 0);

	// Fill and upload CS constant buffer
	// Get camera vectors from current camera
	CCamera* pCam = CCameraManager::Instance().GetCurrentCamera();
	if (!pCam) return false;

	const Vector3& vUp = pCam->GetUp();
	const Vector3& vCross = pCam->GetCross();
	const Vector3& vView = pCam->GetView();

	m_cbParticleCS.camUp = XMFLOAT3(vUp.x, vUp.y, vUp.z);
	m_cbParticleCS.camCross = XMFLOAT3(vCross.x, vCross.y, vCross.z);
	m_cbParticleCS.camView = XMFLOAT3(vView.x, vView.y, vView.z);
	m_cbParticleCS.particleCount = count;
	m_cbParticleCS.facesPerParticle = facesPerParticle;
	m_cbParticleCS.faceRotations = XMFLOAT4(
		fRotations[0], fRotations[1], fRotations[2], 0.0f);

	if (pAttachMatrix)
	{
		m_cbParticleCS.hasAttachMatrix = 1;
		m_cbParticleCS.attachMatrix = XMMatrixTranspose(XMLoadFloat4x4(
			reinterpret_cast<const XMFLOAT4X4*>(pAttachMatrix)));
	}
	else
	{
		m_cbParticleCS.hasAttachMatrix = 0;
		m_cbParticleCS.attachMatrix = XMMatrixIdentity();
	}

	if (FAILED(pCtx->Map(m_pCBParticleCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return false;
	memcpy(mapped.pData, &m_cbParticleCS, sizeof(CBParticleCS));
	pCtx->Unmap(m_pCBParticleCS, 0);

	// Bind CS resources and dispatch
	CSSetSRV(0, m_particleCSInput.pSRV);
	CSSetUAV(0, m_particleCSOutput.pUAV);
	CSSetCB(0, m_pCBParticleCS);

	UINT totalThreads = count;
	UINT groups = (totalThreads + 63) / 64;
	DispatchCompute(CS_PARTICLE_BILLBOARD, groups);

	CSUnbindResources();

	InvalidateIACache();

	return true;
}

void CShaderManager::DrawParticleCSOutput(UINT quadCount)
{
	if (!m_bComputeParticlesAvailable || quadCount == 0)
		return;

	ID3D11DeviceContext* pCtx = GetActiveContext();
	if (!pCtx) return;

	// Bind CS output buffer as vertex buffer
	UINT stride = 24;  // float3 pos + DWORD color + float2 texcoord
	UINT offset = 0;
	SetVertexBuffer(0, m_particleCSOutput.pBuffer, stride, offset);

	// Bind particle CS index buffer
	SetIndexBuffer(m_pParticleCSIB, DXGI_FORMAT_R16_UINT, 0);

	// Commit render state and constant buffers
	CommitRenderState();
	CommitChanges();

	SetPrimitiveTopologyIfChanged(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCtx->DrawIndexed(quadCount * 6, 0, 0);
	IncrementGlobalDrawCount();
}

//--------------------------------------------------------------------
// Cross-PSI Particle Batcher Implementation
//--------------------------------------------------------------------
void CShaderManager::ResetParticleBatcher()
{
	for (auto& pair : m_particleBatches)
		pair.second.clear();
}

void CShaderManager::AddParticleToBatch(const ParticleBatchKey& key, const ParticleGPUInput& input)
{
	m_particleBatches[key].push_back(input);
	m_particleBatchStats.particlesBatched++;
}

void CShaderManager::FlushParticleBatches()
{
	if (!m_bComputeParticlesAvailable || m_particleBatches.empty())
		return;

	BeginParticle();
	BeginParticlePCT();

	for (auto& groupPair : m_particleBatches)
	{
		const ParticleBatchKey& key = groupPair.first;
		std::vector<ParticleGPUInput>& particles = groupPair.second;
		if (particles.empty())
			continue;

		m_particleBatchStats.batchGroups++;

		SetShaderResource(0, key.pTexture);
		SetPipelineState(PSTATE_SRCBLEND, key.srcBlend);
		SetPipelineState(PSTATE_DESTBLEND, key.destBlend);
		SetParticleColorOp(key.colorOp);

		const float fRots[3] = { key.rot0, key.rot1, key.rot2 };
		const UINT faces = (UINT)key.facesPerParticle;
		const UINT totalParticles = (UINT)particles.size();

		UINT offset = 0;
		while (offset < totalParticles)
		{
			UINT chunkSize = min(totalParticles - offset, MAX_CS_PARTICLES);
			if (!DispatchParticleBillboardCS(particles.data() + offset, chunkSize, faces, fRots, nullptr))
				break;
			DrawParticleCSOutput(chunkSize * faces);
			m_particleBatchStats.dispatches++;
			m_particleBatchStats.drawsIssued++;
			offset += chunkSize;
		}

		particles.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
// Fly Trace Compute Shader Billboard System
//////////////////////////////////////////////////////////////////////////

bool CShaderManager::InitFlyTraceCSResources()
{
	if (!m_pDevice || !m_ComputeShaders[CS_FLYTRACE])
		return false;

	if (!m_Shaders[SHADER_PARTICLE_PCT].pVertexShader)
		return false;

	// Create input structured buffer (DYNAMIC, CPU write)
	if (!CreateStructuredBuffer(sizeof(FlyTraceSegmentInput), MAX_FLYTRACE_SEGMENTS, true, m_flyTraceCSInput))
	{
		TraceError("InitFlyTraceCSResources: Failed to create input structured buffer");
		return false;
	}

	UINT outputVBSize = MAX_FLYTRACE_SEGMENTS * 6 * 24;
	if (!CreateRawVertexUAVBuffer(outputVBSize, m_flyTraceCSOutput))
	{
		TraceError("InitFlyTraceCSResources: Failed to create output VB/UAV buffer");
		return false;
	}

	// Create constant buffer for CS
	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.ByteWidth = sizeof(CBFlyTraceCS);
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBFlyTraceCS)))
	{
		TraceError("InitFlyTraceCSResources: Failed to create CB");
		return false;
	}

	UINT ibSize = MAX_FLYTRACE_SEGMENTS * 12 * sizeof(WORD);
	std::vector<WORD> indices(MAX_FLYTRACE_SEGMENTS * 12);
	for (UINT s = 0; s < MAX_FLYTRACE_SEGMENTS; ++s)
	{
		WORD base = (WORD)(s * 6);
		UINT i = s * 12;
		// tri0: 0,1,2
		indices[i + 0] = base + 0; indices[i + 1] = base + 1; indices[i + 2] = base + 2;
		// tri1: 2,1,3
		indices[i + 3] = base + 2; indices[i + 4] = base + 1; indices[i + 5] = base + 3;
		// tri2: 2,3,4
		indices[i + 6] = base + 2; indices[i + 7] = base + 3; indices[i + 8] = base + 4;
		// tri3: 4,3,5
		indices[i + 9] = base + 4; indices[i + 10] = base + 3; indices[i + 11] = base + 5;
	}

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = ibSize;
	ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = indices.data();
	if (FAILED(m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pFlyTraceCSIB)))
	{
		TraceError("InitFlyTraceCSResources: Failed to create IB");
		return false;
	}

	m_bFlyTraceCSAvailable = true;
	Tracef("InitFlyTraceCSResources: FlyTrace CS system ready (max %d segments)\n", MAX_FLYTRACE_SEGMENTS);
	return true;
}

bool CShaderManager::DispatchFlyTraceCS(const FlyTraceSegmentInput* pSegments, UINT count)
{
	if (!m_bFlyTraceCSAvailable || !pSegments || count == 0)
		return false;

	if (count > MAX_FLYTRACE_SEGMENTS)
		count = MAX_FLYTRACE_SEGMENTS;

	ID3D11DeviceContext* pCtx = GetActiveContext();
	if (!pCtx) return false;

	// Upload segment data to structured buffer
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (FAILED(pCtx->Map(m_flyTraceCSInput.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return false;
	memcpy(mapped.pData, pSegments, count * sizeof(FlyTraceSegmentInput));
	pCtx->Unmap(m_flyTraceCSInput.pBuffer, 0);

	// Fill and upload CS constant buffer
	CCamera* pCam = CCameraManager::Instance().GetCurrentCamera();
	if (!pCam) return false;

	const Vector3& vEye = pCam->GetEye();
	const Vector3& vView = pCam->GetView();

	m_cbFlyTraceCS.camEye = XMFLOAT3(vEye.x, vEye.y, vEye.z);
	m_cbFlyTraceCS.camFwd = XMFLOAT3(vView.x, vView.y, vView.z);
	m_cbFlyTraceCS.segmentCount = count;

	if (FAILED(pCtx->Map(m_pCBFlyTraceCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return false;
	memcpy(mapped.pData, &m_cbFlyTraceCS, sizeof(CBFlyTraceCS));
	pCtx->Unmap(m_pCBFlyTraceCS, 0);

	// Bind CS resources and dispatch
	CSSetSRV(0, m_flyTraceCSInput.pSRV);
	CSSetUAV(0, m_flyTraceCSOutput.pUAV);
	CSSetCB(0, m_pCBFlyTraceCS);

	UINT groups = (count + 63) / 64;
	DispatchCompute(CS_FLYTRACE, groups);

	CSUnbindResources();

	// UAV write auto-unbinds buffer from IA — invalidate cache so re-bind happens.
	InvalidateIACache();

	return true;
}

void CShaderManager::DrawFlyTraceCSOutput(UINT segmentCount)
{
	if (!m_bFlyTraceCSAvailable || segmentCount == 0)
		return;

	ID3D11DeviceContext* pCtx = GetActiveContext();
	if (!pCtx) return;

	// Bind CS output buffer as vertex buffer
	UINT stride = 24;  // float3 pos + DWORD color + float2 texcoord
	UINT offset = 0;
	SetVertexBuffer(0, m_flyTraceCSOutput.pBuffer, stride, offset);

	// Bind fly trace CS index buffer
	SetIndexBuffer(m_pFlyTraceCSIB, DXGI_FORMAT_R16_UINT, 0);

	// Commit render state and constant buffers
	CommitRenderState();
	CommitChanges();

	SetPrimitiveTopologyIfChanged(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCtx->DrawIndexed(segmentCount * 12, 0, 0);  // 12 indices per segment (4 triangles)
}

//////////////////////////////////////////////////////////////////////////
// Weapon Trace Compute Shader Spline System
//////////////////////////////////////////////////////////////////////////

bool CShaderManager::InitWeaponTraceCSResources()
{
	if (!m_pDevice || !m_ComputeShaders[CS_WEAPONTRACE])
		return false;

	if (!m_Shaders[SHADER_PARTICLE_PCT].pVertexShader)
		return false;

	if (!CreateStructuredBuffer(sizeof(WeaponTraceSplineSegment), MAX_WEAPONTRACE_SEGMENTS * 2, true, m_weaponTraceCSInput))
	{
		TraceError("InitWeaponTraceCSResources: Failed to create input structured buffer");
		return false;
	}

	UINT outputVBSize = MAX_WEAPONTRACE_SAMPLES * 2 * 24;
	if (!CreateRawVertexUAVBuffer(outputVBSize, m_weaponTraceCSOutput))
	{
		TraceError("InitWeaponTraceCSResources: Failed to create output VB/UAV buffer");
		return false;
	}

	// Constant buffer
	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.ByteWidth = sizeof(CBWeaponTraceCS);
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBWeaponTraceCS)))
	{
		TraceError("InitWeaponTraceCSResources: Failed to create CB");
		return false;
	}

	m_bWeaponTraceCSAvailable = true;
	Tracef("InitWeaponTraceCSResources: WeaponTrace CS system ready (max %d segments, %d samples)\n",
		MAX_WEAPONTRACE_SEGMENTS, MAX_WEAPONTRACE_SAMPLES);
	return true;
}

bool CShaderManager::DispatchWeaponTraceCS(const WeaponTraceSplineSegment* pSegments, UINT numSegments, const CBWeaponTraceCS& params)
{
	if (!m_bWeaponTraceCSAvailable || !pSegments || numSegments == 0)
		return false;

	if (numSegments > MAX_WEAPONTRACE_SEGMENTS)
		numSegments = MAX_WEAPONTRACE_SEGMENTS;

	UINT numSamples = params.numSamples;
	if (numSamples > MAX_WEAPONTRACE_SAMPLES)
		numSamples = MAX_WEAPONTRACE_SAMPLES;

	ID3D11DeviceContext* pCtx = GetActiveContext();
	if (!pCtx) return false;

	D3D11_MAPPED_SUBRESOURCE mapped;
	if (FAILED(pCtx->Map(m_weaponTraceCSInput.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return false;
	memcpy(mapped.pData, pSegments, numSegments * 2 * sizeof(WeaponTraceSplineSegment));
	pCtx->Unmap(m_weaponTraceCSInput.pBuffer, 0);

	// Upload CB
	m_cbWeaponTraceCS = params;
	m_cbWeaponTraceCS.numSegments = numSegments;
	m_cbWeaponTraceCS.numSamples = numSamples;

	if (FAILED(pCtx->Map(m_pCBWeaponTraceCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return false;
	memcpy(mapped.pData, &m_cbWeaponTraceCS, sizeof(CBWeaponTraceCS));
	pCtx->Unmap(m_pCBWeaponTraceCS, 0);

	// Bind and dispatch
	CSSetSRV(0, m_weaponTraceCSInput.pSRV);
	CSSetUAV(0, m_weaponTraceCSOutput.pUAV);
	CSSetCB(0, m_pCBWeaponTraceCS);

	UINT groups = (numSamples + 63) / 64;
	DispatchCompute(CS_WEAPONTRACE, groups);

	CSUnbindResources();

	// UAV write auto-unbinds buffer from IA — invalidate cache so re-bind happens.
	InvalidateIACache();

	return true;
}

void CShaderManager::DrawWeaponTraceCSOutput(UINT numSamples)
{
	if (!m_bWeaponTraceCSAvailable || numSamples < 2)
		return;

	ID3D11DeviceContext* pCtx = GetActiveContext();
	if (!pCtx) return;

	// Bind CS output buffer as vertex buffer
	UINT stride = 24;  // float3 pos + DWORD color + float2 texcoord (PDT)
	UINT offset = 0;
	SetVertexBuffer(0, m_weaponTraceCSOutput.pBuffer, stride, offset);

	// Commit render state and constant buffers
	CommitRenderState();
	CommitChanges();

	SetPrimitiveTopologyIfChanged(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pCtx->Draw(numSamples * 2, 0);
}
