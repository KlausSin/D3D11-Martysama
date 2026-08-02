#include "StdAfx.h"
#include "StateManager.h"
#include "ShaderManager.h"
#include "../eterBase/Debug.h"

CStateManager::CStateManager(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: m_pDevice(nullptr)
	, m_pContext(nullptr)
	, m_pStateCache(nullptr)
	, m_pCurrentBlendState(nullptr)
	, m_pCurrentRasterizerState(nullptr)
	, m_pCurrentDepthStencilState(nullptr)
	, m_bBlendStateDirty(true)
	, m_bRasterizerStateDirty(true)
	, m_bDepthStencilStateDirty(true)
	, m_bFogEnabled(false)
	, m_fFogStart(0.0f)
	, m_fFogEnd(1000.0f)
	, m_dwFogColor(0xFFFFFFFF)
	, m_bAlphaTestEnabled(false)
	, m_fAlphaRef(0.0f)
	, m_bLightingEnabled(false)  // lighting disabled by default
	, m_dwTextureFactor(0xFFFFFFFF)
	, m_InputLayout(INPUT_LAYOUT_PDT)
	, m_SavedInputLayout(INPUT_LAYOUT_PDT)
	, m_pSavedVertexShader(nullptr)
	, m_pSavedPixelShader(nullptr)
	, m_pCurrentVertexShader(nullptr)
	, m_pCurrentPixelShader(nullptr)
	, m_pDynamicVertexBuffer(nullptr)
	, m_pDynamicIndexBuffer(nullptr)
{
	for (int i = 0; i < STATEMANAGER_MAX_STAGES; ++i)
	{
		m_pCurrentSamplerStates[i] = nullptr;
		m_pTextures[i] = nullptr;
		m_pSavedTextures[i] = nullptr;
		m_bSamplerStateDirty[i] = true;
	}

	for (int i = 0; i < STATEMANAGER_MAX_STREAMS; ++i)
	{
		m_StreamData[i] = CStreamData();
		m_SavedStreamData[i] = CStreamData();
	}

	ZeroMemory(&m_Material, sizeof(m_Material));
	ZeroMemory(&m_SavedMaterial, sizeof(m_SavedMaterial));

	SetDevice(pDevice, pContext);
}

CStateManager::~CStateManager()
{
	if (m_pDynamicVertexBuffer)
	{
		m_pDynamicVertexBuffer->Release();
		m_pDynamicVertexBuffer = nullptr;
	}

	if (m_pDynamicIndexBuffer)
	{
		m_pDynamicIndexBuffer->Release();
		m_pDynamicIndexBuffer = nullptr;
	}

	if (m_pStateCache)
	{
		delete m_pStateCache;
		m_pStateCache = nullptr;
	}

	if (m_pDevice)
	{
		m_pDevice->Release();
		m_pDevice = nullptr;
	}

	// Note: Context is not AddRef'd in this implementation
}

void CStateManager::SetDevice(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	assert(pDevice != nullptr);
	assert(pContext != nullptr);

	pDevice->AddRef();

	if (m_pDevice)
	{
		m_pDevice->Release();
		m_pDevice = nullptr;
	}

	m_pDevice = pDevice;
	m_pContext = pContext;

	if (m_pStateCache)
	{
		delete m_pStateCache;
	}
	m_pStateCache = new CStateObjectCache(pDevice);

	// Create reusable dynamic buffers for DrawDynamic
	if (m_pDynamicVertexBuffer)
	{
		m_pDynamicVertexBuffer->Release();
		m_pDynamicVertexBuffer = nullptr;
	}
	if (m_pDynamicIndexBuffer)
	{
		m_pDynamicIndexBuffer->Release();
		m_pDynamicIndexBuffer = nullptr;
	}

	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	bufferDesc.ByteWidth = UP_VERTEX_BUFFER_SIZE;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	HRESULT hr = pDevice->CreateBuffer(&bufferDesc, nullptr, &m_pDynamicVertexBuffer);
	if (FAILED(hr))
	{
		TraceError("CStateManager: Failed to create dynamic vertex buffer (hr=0x%08X)", hr);
		m_pDynamicVertexBuffer = nullptr;
	}

	bufferDesc.ByteWidth = UP_INDEX_BUFFER_SIZE;
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	hr = pDevice->CreateBuffer(&bufferDesc, nullptr, &m_pDynamicIndexBuffer);
	if (FAILED(hr))
	{
		TraceError("CStateManager: Failed to create dynamic index buffer (hr=0x%08X)", hr);
		m_pDynamicIndexBuffer = nullptr;
	}

	SetDefaultState();
}

void CStateManager::SetDefaultState()
{
	m_CurrentState.SetDefaults();
	m_SavedState.SetDefaults();

	m_bBlendStateDirty = true;
	m_bRasterizerStateDirty = true;
	m_bDepthStencilStateDirty = true;

	for (int i = 0; i < STATEMANAGER_MAX_STAGES; ++i)
	{
		m_bSamplerStateDirty[i] = true;
		m_pTextures[i] = nullptr;
	}

	// Initialize matrices
	for (int i = 0; i < STATEMANAGER_MAX_TRANSFORMSTATES; ++i)
	{
		MatrixIdentity(&m_Matrices[i]);
	}

	// Set default material
	m_Material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_Material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_Material.Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	m_Material.Emissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	m_Material.Power = 0.0f;

	// Apply default states
	CommitState();
}

void CStateManager::Restore()
{
	m_bBlendStateDirty = true;
	m_bRasterizerStateDirty = true;
	m_bDepthStencilStateDirty = true;

	for (int i = 0; i < STATEMANAGER_MAX_STAGES; ++i)
	{
		m_bSamplerStateDirty[i] = true;
	}

	CommitState();
}

// Material
void CStateManager::SaveMaterial()
{
	m_SavedMaterial = m_Material;
}

void CStateManager::SaveMaterial(const TMaterial* pMaterial)
{
	m_SavedMaterial = m_Material;
	SetMaterial(pMaterial);
}

void CStateManager::RestoreMaterial()
{
	m_Material = m_SavedMaterial;
}

void CStateManager::SetMaterial(const TMaterial* pMaterial)
{
	m_Material.Diffuse = XMFLOAT4(pMaterial->Diffuse.r, pMaterial->Diffuse.g, pMaterial->Diffuse.b, pMaterial->Diffuse.a);
	m_Material.Ambient = XMFLOAT4(pMaterial->Ambient.r, pMaterial->Ambient.g, pMaterial->Ambient.b, pMaterial->Ambient.a);
	m_Material.Specular = XMFLOAT4(pMaterial->Specular.r, pMaterial->Specular.g, pMaterial->Specular.b, pMaterial->Specular.a);
	m_Material.Emissive = XMFLOAT4(pMaterial->Emissive.r, pMaterial->Emissive.g, pMaterial->Emissive.b, pMaterial->Emissive.a);
	m_Material.Power = pMaterial->Power;

	// Forward to ShaderManager
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.SetDiffuseColor(pMaterial->Diffuse.r, pMaterial->Diffuse.g, pMaterial->Diffuse.b, pMaterial->Diffuse.a);
		SHADERMANAGER.SetEmissiveColor(pMaterial->Emissive.r, pMaterial->Emissive.g, pMaterial->Emissive.b);
		SHADERMANAGER.SetSpecularColor(pMaterial->Specular.r, pMaterial->Specular.g, pMaterial->Specular.b);
		SHADERMANAGER.SetMaterial(pMaterial->Power);
	}
}

void CStateManager::GetMaterial(TMaterial* pMaterial)
{
	pMaterial->Diffuse.r = m_Material.Diffuse.x;
	pMaterial->Diffuse.g = m_Material.Diffuse.y;
	pMaterial->Diffuse.b = m_Material.Diffuse.z;
	pMaterial->Diffuse.a = m_Material.Diffuse.w;
	pMaterial->Ambient.r = m_Material.Ambient.x;
	pMaterial->Ambient.g = m_Material.Ambient.y;
	pMaterial->Ambient.b = m_Material.Ambient.z;
	pMaterial->Ambient.a = m_Material.Ambient.w;
	pMaterial->Specular.r = m_Material.Specular.x;
	pMaterial->Specular.g = m_Material.Specular.y;
	pMaterial->Specular.b = m_Material.Specular.z;
	pMaterial->Specular.a = m_Material.Specular.w;
	pMaterial->Emissive.r = m_Material.Emissive.x;
	pMaterial->Emissive.g = m_Material.Emissive.y;
	pMaterial->Emissive.b = m_Material.Emissive.z;
	pMaterial->Emissive.a = m_Material.Emissive.w;
	pMaterial->Power = m_Material.Power;
}

// Lights - Forward to ShaderManager
void CStateManager::SetLight(DWORD index, const TLight* pLight)
{
	if (index >= MAX_SHADER_LIGHTS || !SHADERMANAGER.IsInitialized()) return;

	bool bEnabled = SHADERMANAGER.IsLightEnabled(index);

	DX11Light dx11Light;
	dx11Light.Position = XMFLOAT4(pLight->Position.x, pLight->Position.y, pLight->Position.z, (float)pLight->Type);
	dx11Light.Direction = XMFLOAT4(pLight->Direction.x, pLight->Direction.y, pLight->Direction.z, bEnabled ? 1.0f : 0.0f);
	dx11Light.Color = XMFLOAT4(pLight->Diffuse.r, pLight->Diffuse.g, pLight->Diffuse.b, 1.0f);
	dx11Light.Attenuation = XMFLOAT4(pLight->Attenuation0, pLight->Attenuation1, pLight->Attenuation2, pLight->Range);
	SHADERMANAGER.SetLight(index, dx11Light);
}

void CStateManager::GetLight(DWORD index, TLight* pLight)
{
	// Forward to ShaderManager
	if (SHADERMANAGER.IsInitialized())
		SHADERMANAGER.GetLight(index, pLight);
}

void CStateManager::LightEnable(DWORD index, BOOL bEnable)
{
	if (SHADERMANAGER.IsInitialized())
		SHADERMANAGER.EnableLight(index, bEnable != FALSE);
}

// Render states - Map to DX11 state components
void CStateManager::SavePipelineState(EPipelineState Type, DWORD dwValue)
{
	m_SavedRenderStates[Type] = GetPipelineState(Type);
	SetPipelineState(Type, dwValue);
}

void CStateManager::RestorePipelineState(EPipelineState Type)
{
	auto it = m_SavedRenderStates.find(Type);
	if (it != m_SavedRenderStates.end())
	{
		SetPipelineState(Type, it->second);
		m_SavedRenderStates.erase(it);
	}
}

void CStateManager::SetPipelineState(EPipelineState Type, DWORD Value)
{
	switch (Type)
	{
	// Blend states
	case PSTATE_BLENDENABLE:
		m_CurrentState.bAlphaBlendEnable = (Value != 0);
		m_bBlendStateDirty = true;
		break;
	case PSTATE_SRCBLEND:
		m_CurrentState.srcBlend = (D3D11_BLEND)Value;
		m_bBlendStateDirty = true;
		break;
	case PSTATE_DESTBLEND:
		m_CurrentState.destBlend = (D3D11_BLEND)Value;
		m_bBlendStateDirty = true;
		break;
	case PSTATE_BLENDOP:
		m_CurrentState.blendOp = (D3D11_BLEND_OP)Value;
		m_bBlendStateDirty = true;
		break;
	case PSTATE_RTWRITEMASK:
		m_CurrentState.colorWriteMask = (UINT8)Value;
		m_bBlendStateDirty = true;
		break;

	// Rasterizer states
	case PSTATE_FILLMODE:
		m_CurrentState.fillMode = (Value == FILL_WIREFRAME) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
		m_bRasterizerStateDirty = true;
		break;
	case PSTATE_CULLMODE:
		switch (Value)
		{
		case CULL_NONE: m_CurrentState.cullMode = D3D11_CULL_NONE; break;
		case CULL_FRONT:   m_CurrentState.cullMode = D3D11_CULL_FRONT; break;
		case CULL_BACK:  m_CurrentState.cullMode = D3D11_CULL_BACK; break;
		}
		m_bRasterizerStateDirty = true;
		break;
	case PSTATE_SCISSORENABLE:
		m_CurrentState.bScissorEnable = (Value != 0);
		m_bRasterizerStateDirty = true;
		break;
	case PSTATE_MULTISAMPLEENABLE:
		m_CurrentState.bMultisampleEnable = (Value != 0);
		m_bRasterizerStateDirty = true;
		break;
	case PSTATE_ANTIALIASEDLINEENABLE:
		m_CurrentState.bAntialiasedLineEnable = (Value != 0);
		m_bRasterizerStateDirty = true;
		break;
	case PSTATE_DEPTHBIAS:
		m_CurrentState.depthBias = (INT)Value;
		m_bRasterizerStateDirty = true;
		break;
	case PSTATE_SLOPESCALEDDEPTHBIAS:
		m_CurrentState.slopeScaledDepthBias = *(float*)&Value;
		m_bRasterizerStateDirty = true;
		break;

	// Depth stencil states
	case PSTATE_DEPTHENABLE:
		m_CurrentState.bDepthEnable = (Value != 0);
		m_bDepthStencilStateDirty = true;
		break;
	case PSTATE_DEPTHWRITEMASK:
		m_CurrentState.bDepthWriteEnable = (Value != 0);
		m_bDepthStencilStateDirty = true;
		break;
	case PSTATE_DEPTHFUNC:
		m_CurrentState.depthFunc = (D3D11_COMPARISON_FUNC)Value;
		m_bDepthStencilStateDirty = true;
		break;
	case PSTATE_STENCILENABLE:
		m_CurrentState.bStencilEnable = (Value != 0);
		m_bDepthStencilStateDirty = true;
		break;
	case PSTATE_STENCILREADMASK:
		m_CurrentState.stencilReadMask = (UINT8)Value;
		m_bDepthStencilStateDirty = true;
		break;
	case PSTATE_STENCILWRITEMASK:
		m_CurrentState.stencilWriteMask = (UINT8)Value;
		m_bDepthStencilStateDirty = true;
		break;

	default:
		break;
	}
}

void CStateManager::GetPipelineState(EPipelineState Type, DWORD* pdwValue)
{
	*pdwValue = GetPipelineState(Type);
}

DWORD CStateManager::GetPipelineState(EPipelineState Type)
{
	switch (Type)
	{
	case PSTATE_BLENDENABLE: return m_CurrentState.bAlphaBlendEnable ? 1 : 0;
	case PSTATE_SRCBLEND: return (DWORD)m_CurrentState.srcBlend;
	case PSTATE_DESTBLEND: return (DWORD)m_CurrentState.destBlend;
	case PSTATE_BLENDOP: return (DWORD)m_CurrentState.blendOp;
	case PSTATE_RTWRITEMASK: return m_CurrentState.colorWriteMask;
	case PSTATE_FILLMODE: return (m_CurrentState.fillMode == D3D11_FILL_WIREFRAME) ? FILL_WIREFRAME : FILL_SOLID;
	case PSTATE_CULLMODE:
		switch (m_CurrentState.cullMode)
		{
		case D3D11_CULL_NONE: return CULL_NONE;
		case D3D11_CULL_FRONT: return CULL_FRONT;
		case D3D11_CULL_BACK: return CULL_BACK;
		}
		return CULL_NONE;
	case PSTATE_DEPTHENABLE: return m_CurrentState.bDepthEnable ? 1 : 0;
	case PSTATE_DEPTHWRITEMASK: return m_CurrentState.bDepthWriteEnable ? 1 : 0;
	case PSTATE_DEPTHFUNC: return (DWORD)m_CurrentState.depthFunc;
	case PSTATE_STENCILENABLE: return m_CurrentState.bStencilEnable ? 1 : 0;
	case PSTATE_STENCILREADMASK: return m_CurrentState.stencilReadMask;
	case PSTATE_STENCILWRITEMASK: return m_CurrentState.stencilWriteMask;
	default: return 0;
	}
}

//////////////////////////////////////////////////////////////////////////
// Shader Parameter Methods (Pure DX11)
//////////////////////////////////////////////////////////////////////////

void CStateManager::SetFogEnabled(bool bEnable)
{
	m_bFogEnabled = bEnable;
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.SetFog(m_bFogEnabled, m_fFogStart, m_fFogEnd, m_dwFogColor);
	}
}

void CStateManager::SetFogParams(float fStart, float fEnd, DWORD dwColor)
{
	m_fFogStart = fStart;
	m_fFogEnd = fEnd;
	m_dwFogColor = dwColor;
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.SetFog(m_bFogEnabled, m_fFogStart, m_fFogEnd, m_dwFogColor);
	}
}

void CStateManager::SetFogColor(DWORD dwColor)
{
	m_dwFogColor = dwColor;
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.SetFog(m_bFogEnabled, m_fFogStart, m_fFogEnd, m_dwFogColor);
	}
}

void CStateManager::SetAlphaTestEnabled(bool bEnable)
{
	m_bAlphaTestEnabled = bEnable;
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.SetAlphaTest(m_bAlphaTestEnabled, m_fAlphaRef);
	}
}

void CStateManager::SetAlphaTestRef(float fRef)
{
	m_fAlphaRef = fRef;
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.SetAlphaTest(m_bAlphaTestEnabled, m_fAlphaRef);
	}
}

void CStateManager::SetAlphaTestRefByte(DWORD dwRef)
{
	m_fAlphaRef = (float)dwRef / 255.0f;
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.SetAlphaTest(m_bAlphaTestEnabled, m_fAlphaRef);
	}
}

void CStateManager::SetLightingEnabled(bool bEnable)
{
	m_bLightingEnabled = bEnable;
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.SetLightingEnabled(m_bLightingEnabled);
	}
}

void CStateManager::SetTextureFactor(DWORD dwColor)
{
	m_dwTextureFactor = dwColor;
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.SetTextureFactor(m_dwTextureFactor);
	}
}

// Textures
void CStateManager::SaveTexture(DWORD dwStage, ID3D11ShaderResourceView* pTexture)
{
	if (dwStage >= STATEMANAGER_MAX_STAGES) return;
	m_pSavedTextures[dwStage] = m_pTextures[dwStage];
	SetShaderResource(dwStage, pTexture);
}

void CStateManager::RestoreTexture(DWORD dwStage)
{
	if (dwStage >= STATEMANAGER_MAX_STAGES) return;
	SetShaderResource(dwStage, m_pSavedTextures[dwStage]);
}

void CStateManager::SetShaderResource(DWORD dwStage, ID3D11ShaderResourceView* pTexture)
{
	if (dwStage >= STATEMANAGER_MAX_STAGES) return;

	ID3D11ShaderResourceView* pActualTexture = pTexture;
	if (!pTexture && SHADERMANAGER.IsInitialized())
	{
		pActualTexture = SHADERMANAGER.GetDefaultTexture();  // Returns active texture (transparent during init)
	}

	if (m_pTextures[dwStage] == pActualTexture) return;

	m_pTextures[dwStage] = pActualTexture;
	m_pContext->PSSetShaderResources(dwStage, 1, &pActualTexture);
}

void CStateManager::GetTexture(DWORD dwStage, ID3D11ShaderResourceView** ppTexture)
{
	if (dwStage >= STATEMANAGER_MAX_STAGES)
	{
		*ppTexture = nullptr;
		return;
	}
	*ppTexture = m_pTextures[dwStage];
}

// Sampler states
void CStateManager::SaveSamplerState(DWORD dwStage, ESamplerState Type, DWORD dwValue)
{
	if (dwStage >= STATEMANAGER_MAX_STAGES) return;
	auto key = std::make_pair(dwStage, Type);
	DWORD currentValue = 0;
	GetSamplerState(dwStage, Type, &currentValue);
	m_SavedSamplerStates[key] = currentValue;
	SetSamplerState(dwStage, Type, dwValue);
}

void CStateManager::RestoreSamplerState(DWORD dwStage, ESamplerState Type)
{
	if (dwStage >= STATEMANAGER_MAX_STAGES) return;
	auto key = std::make_pair(dwStage, Type);
	auto it = m_SavedSamplerStates.find(key);
	if (it != m_SavedSamplerStates.end())
	{
		SetSamplerState(dwStage, Type, it->second);
		m_SavedSamplerStates.erase(it);
	}
}

void CStateManager::SetSamplerState(DWORD dwStage, ESamplerState Type, DWORD dwValue)
{
	if (dwStage >= STATEMANAGER_MAX_STAGES) return;

	switch (Type)
	{
	case SAMPLER_MINFILTER:
	case SAMPLER_MAGFILTER:
		// Filter mapping for min/mag
		if (dwValue == FILTER_ANISOTROPIC)
			m_CurrentState.samplerFilter[dwStage] = D3D11_FILTER_ANISOTROPIC;
		else if (dwValue == FILTER_LINEAR)
			m_CurrentState.samplerFilter[dwStage] = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		else
			m_CurrentState.samplerFilter[dwStage] = D3D11_FILTER_MIN_MAG_MIP_POINT;
		m_bSamplerStateDirty[dwStage] = true;
		break;

	case SAMPLER_MIPFILTER:
		// FILTER_NONE means disable mipmapping (MaxLOD = 0)
		if (dwValue == FILTER_NONE)
		{
			m_CurrentState.bMipmapDisabled[dwStage] = true;
		}
		else
		{
			m_CurrentState.bMipmapDisabled[dwStage] = false;
			// Also update filter if mip filter is specified
			if (dwValue == FILTER_ANISOTROPIC)
				m_CurrentState.samplerFilter[dwStage] = D3D11_FILTER_ANISOTROPIC;
			else if (dwValue == FILTER_LINEAR)
				m_CurrentState.samplerFilter[dwStage] = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			else
				m_CurrentState.samplerFilter[dwStage] = D3D11_FILTER_MIN_MAG_MIP_POINT;
		}
		m_bSamplerStateDirty[dwStage] = true;
		break;

	case SAMPLER_ADDRESSU:
		m_CurrentState.addressU[dwStage] = (D3D11_TEXTURE_ADDRESS_MODE)dwValue;
		m_bSamplerStateDirty[dwStage] = true;
		break;

	case SAMPLER_ADDRESSV:
		m_CurrentState.addressV[dwStage] = (D3D11_TEXTURE_ADDRESS_MODE)dwValue;
		m_bSamplerStateDirty[dwStage] = true;
		break;

	case SAMPLER_ADDRESSW:
		m_CurrentState.addressW[dwStage] = (D3D11_TEXTURE_ADDRESS_MODE)dwValue;
		m_bSamplerStateDirty[dwStage] = true;
		break;

	case SAMPLER_MAXANISOTROPY:
		m_CurrentState.maxAnisotropy[dwStage] = dwValue;
		m_bSamplerStateDirty[dwStage] = true;
		break;
	}
}

void CStateManager::GetSamplerState(DWORD dwStage, ESamplerState Type, DWORD* pdwValue)
{
	if (dwStage >= STATEMANAGER_MAX_STAGES)
	{
		*pdwValue = 0;
		return;
	}

	switch (Type)
	{
	case SAMPLER_MINFILTER:
	case SAMPLER_MAGFILTER:
	case SAMPLER_MIPFILTER:
		if (m_CurrentState.samplerFilter[dwStage] == D3D11_FILTER_ANISOTROPIC)
			*pdwValue = FILTER_ANISOTROPIC;
		else if (m_CurrentState.samplerFilter[dwStage] == D3D11_FILTER_MIN_MAG_MIP_LINEAR)
			*pdwValue = FILTER_LINEAR;
		else
			*pdwValue = FILTER_POINT;
		break;
	case SAMPLER_ADDRESSU:
		*pdwValue = m_CurrentState.addressU[dwStage];
		break;
	case SAMPLER_ADDRESSV:
		*pdwValue = m_CurrentState.addressV[dwStage];
		break;
	case SAMPLER_MAXANISOTROPY:
		*pdwValue = m_CurrentState.maxAnisotropy[dwStage];
		break;
	default:
		*pdwValue = 0;
		break;
	}
}

void CStateManager::SetBestFiltering(DWORD dwStage)
{
	SetSamplerState(dwStage, SAMPLER_MINFILTER, FILTER_ANISOTROPIC);
	SetSamplerState(dwStage, SAMPLER_MAGFILTER, FILTER_ANISOTROPIC);
	SetSamplerState(dwStage, SAMPLER_MIPFILTER, FILTER_ANISOTROPIC);
}

// Shaders
void CStateManager::VSSetShader(ID3D11VertexShader* pShader)
{
	m_pCurrentVertexShader = pShader;
	m_pContext->VSSetShader(pShader, nullptr, 0);
}

void CStateManager::PSSetShader(ID3D11PixelShader* pShader)
{
	m_pCurrentPixelShader = pShader;
	m_pContext->PSSetShader(pShader, nullptr, 0);
}

void CStateManager::SetInputLayout(ID3D11InputLayout* pLayout)
{
	m_pContext->IASetInputLayout(pLayout);
}

// Transform
void CStateManager::SaveTransform(EMatrixSlot Transform, const Matrix* pMatrix)
{
	if ((UINT)Transform >= STATEMANAGER_MAX_TRANSFORMSTATES) return;
	m_SavedMatrices[Transform] = m_Matrices[Transform];
	SetMatrix(Transform, pMatrix);
}

void CStateManager::RestoreTransform(EMatrixSlot Transform)
{
	if ((UINT)Transform >= STATEMANAGER_MAX_TRANSFORMSTATES) return;
	SetMatrix(Transform, &m_SavedMatrices[Transform]);
}

void CStateManager::SetMatrix(EMatrixSlot Type, const Matrix* pMatrix)
{
	if ((UINT)Type >= STATEMANAGER_MAX_TRANSFORMSTATES) return;
	m_Matrices[Type] = *pMatrix;

	if (SHADERMANAGER.IsInitialized())
	{
		if (Type == MATRIX_VIEW || Type == MATRIX_PROJECTION)
		{
			// Get camera position from inverse view matrix
			Matrix matInvView;
			MatrixInverse(&matInvView, nullptr, &m_Matrices[MATRIX_VIEW]);
			Vector3 vCameraPos(matInvView._41, matInvView._42, matInvView._43);
			SHADERMANAGER.SetViewProjection(&m_Matrices[MATRIX_VIEW], &m_Matrices[MATRIX_PROJECTION]);
			SHADERMANAGER.SetCameraPosition(&vCameraPos);
			const D3D11_VIEWPORT& viewport = CGraphicBase::GetViewport();
			SHADERMANAGER.SetViewportSize(viewport.Width, viewport.Height);
		}
	}
}

void CStateManager::GetMatrix(EMatrixSlot Type, Matrix* pMatrix)
{
	if ((UINT)Type >= STATEMANAGER_MAX_TRANSFORMSTATES)
	{
		MatrixIdentity(pMatrix);
		return;
	}
	*pMatrix = m_Matrices[Type];
}

// Stream/Index binding
void CStateManager::SaveStreamSource(UINT StreamNumber, ID3D11Buffer* pStreamData, UINT Stride)
{
	if (StreamNumber >= STATEMANAGER_MAX_STREAMS) return;
	m_SavedStreamData[StreamNumber] = m_StreamData[StreamNumber];
	SetVertexBuffer(StreamNumber, pStreamData, Stride);
}

void CStateManager::RestoreStreamSource(UINT StreamNumber)
{
	if (StreamNumber >= STATEMANAGER_MAX_STREAMS) return;
	SetVertexBuffer(StreamNumber, m_SavedStreamData[StreamNumber].m_pStreamData,
		m_SavedStreamData[StreamNumber].m_Stride, m_SavedStreamData[StreamNumber].m_Offset);
}

void CStateManager::SetVertexBuffer(UINT StreamNumber, ID3D11Buffer* pStreamData, UINT Stride, UINT Offset)
{
	if (StreamNumber >= STATEMANAGER_MAX_STREAMS) return;

	CStreamData newData(pStreamData, Stride, Offset);
	if (m_StreamData[StreamNumber] == newData) return;

	m_StreamData[StreamNumber] = newData;
	m_pContext->IASetVertexBuffers(StreamNumber, 1, &pStreamData, &Stride, &Offset);
}

void CStateManager::SaveIndices(ID3D11Buffer* pIndexData)
{
	m_SavedIndexData = m_IndexData;
	SetIndexBuffer(pIndexData);
}

void CStateManager::RestoreIndices()
{
	SetIndexBuffer(m_SavedIndexData.m_pIndexData, m_SavedIndexData.m_Format, m_SavedIndexData.m_Offset);
}

void CStateManager::SetIndexBuffer(ID3D11Buffer* pIndexData, DXGI_FORMAT Format, UINT Offset)
{
	if (!pIndexData)
		return;

	CIndexData newData(pIndexData, Format, Offset);
	if (m_IndexData == newData) return;

	m_IndexData = newData;
	m_pContext->IASetIndexBuffer(pIndexData, Format, Offset);
}

// State object updates
void CStateManager::UpdateBlendState()
{
	if (!m_bBlendStateDirty) return;

	D3D11_BLEND_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.AlphaToCoverageEnable = FALSE;
	desc.IndependentBlendEnable = FALSE;
	desc.RenderTarget[0].BlendEnable = m_CurrentState.bAlphaBlendEnable;
	desc.RenderTarget[0].SrcBlend = m_CurrentState.srcBlend;
	desc.RenderTarget[0].DestBlend = m_CurrentState.destBlend;
	desc.RenderTarget[0].BlendOp = m_CurrentState.blendOp;
	desc.RenderTarget[0].SrcBlendAlpha = m_CurrentState.srcBlendAlpha;
	desc.RenderTarget[0].DestBlendAlpha = m_CurrentState.destBlendAlpha;
	desc.RenderTarget[0].BlendOpAlpha = m_CurrentState.blendOpAlpha;
	desc.RenderTarget[0].RenderTargetWriteMask = m_CurrentState.colorWriteMask;

	ID3D11BlendState* pState = m_pStateCache->GetBlendState(desc);
	if (pState && pState != m_pCurrentBlendState)
	{
		m_pCurrentBlendState = pState;
		float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		m_pContext->OMSetBlendState(pState, blendFactor, 0xFFFFFFFF);
	}

	m_bBlendStateDirty = false;
}

void CStateManager::UpdateRasterizerState()
{
	if (!m_bRasterizerStateDirty) return;

	D3D11_RASTERIZER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.FillMode = m_CurrentState.fillMode;
	desc.CullMode = m_CurrentState.cullMode;
	desc.FrontCounterClockwise = FALSE;
	desc.DepthBias = m_CurrentState.depthBias;
	desc.DepthBiasClamp = m_CurrentState.depthBiasClamp;
	desc.SlopeScaledDepthBias = m_CurrentState.slopeScaledDepthBias;
	desc.DepthClipEnable = m_CurrentState.bDepthClipEnable;
	desc.ScissorEnable = m_CurrentState.bScissorEnable;
	desc.MultisampleEnable = m_CurrentState.bMultisampleEnable;
	desc.AntialiasedLineEnable = m_CurrentState.bAntialiasedLineEnable;

	ID3D11RasterizerState* pState = m_pStateCache->GetRasterizerState(desc);
	if (pState && pState != m_pCurrentRasterizerState)
	{
		m_pCurrentRasterizerState = pState;
		m_pContext->RSSetState(pState);
	}

	m_bRasterizerStateDirty = false;
}

void CStateManager::UpdateDepthStencilState()
{
	if (!m_bDepthStencilStateDirty) return;

	D3D11_DEPTH_STENCIL_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.DepthEnable = m_CurrentState.bDepthEnable;
	desc.DepthWriteMask = m_CurrentState.bDepthWriteEnable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = m_CurrentState.depthFunc;
	desc.StencilEnable = m_CurrentState.bStencilEnable;
	desc.StencilReadMask = m_CurrentState.stencilReadMask;
	desc.StencilWriteMask = m_CurrentState.stencilWriteMask;
	desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	desc.BackFace = desc.FrontFace;

	ID3D11DepthStencilState* pState = m_pStateCache->GetDepthStencilState(desc);
	if (pState && pState != m_pCurrentDepthStencilState)
	{
		m_pCurrentDepthStencilState = pState;
		m_pContext->OMSetDepthStencilState(pState, 0);
	}

	m_bDepthStencilStateDirty = false;
}

void CStateManager::UpdateSamplerStates()
{
	for (DWORD i = 0; i < STATEMANAGER_MAX_STAGES; ++i)
	{
		if (!m_bSamplerStateDirty[i]) continue;

		D3D11_SAMPLER_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Filter = m_CurrentState.samplerFilter[i];
		desc.AddressU = m_CurrentState.addressU[i];
		desc.AddressV = m_CurrentState.addressV[i];
		desc.AddressW = m_CurrentState.addressW[i];
		desc.MipLODBias = 0.0f;
		desc.MaxAnisotropy = m_CurrentState.maxAnisotropy[i];
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		desc.BorderColor[0] = 0.0f;
		desc.BorderColor[1] = 0.0f;
		desc.BorderColor[2] = 0.0f;
		desc.BorderColor[3] = 0.0f;
		desc.MinLOD = 0.0f;
		desc.MaxLOD = m_CurrentState.bMipmapDisabled[i] ? 0.0f : D3D11_FLOAT32_MAX;

		ID3D11SamplerState* pState = m_pStateCache->GetSamplerState(desc);
		if (pState && pState != m_pCurrentSamplerStates[i])
		{
			m_pCurrentSamplerStates[i] = pState;
			m_pContext->PSSetSamplers(i, 1, &pState);
		}

		m_bSamplerStateDirty[i] = false;
	}
}

void CStateManager::CommitState()
{
	UpdateBlendState();
	UpdateRasterizerState();
	UpdateDepthStencilState();
	UpdateSamplerStates();
}

D3D11_PRIMITIVE_TOPOLOGY CStateManager::ConvertPrimitiveType(EPrimitiveTopology type)
{
	switch (type)
	{
	case TOPOLOGY_POINTLIST:		return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	case TOPOLOGY_LINELIST:		return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	case TOPOLOGY_LINESTRIP:		return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case TOPOLOGY_TRIANGLELIST:	return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case TOPOLOGY_TRIANGLESTRIP:	return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	default:				return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
}

UINT CStateManager::GetPrimitiveVertexCount(EPrimitiveTopology type, UINT primitiveCount)
{
	switch (type)
	{
	case TOPOLOGY_POINTLIST:		return primitiveCount;
	case TOPOLOGY_LINELIST:		return primitiveCount * 2;
	case TOPOLOGY_LINESTRIP:		return primitiveCount + 1;
	case TOPOLOGY_TRIANGLELIST:	return primitiveCount * 3;
	case TOPOLOGY_TRIANGLESTRIP:	return primitiveCount + 2;
	default:				return 0;
	}
}

void CStateManager::EnsureShadersBound()
{
	if (!SHADERMANAGER.IsInitialized())
	{
		TraceError("EnsureShadersBound: ShaderManager not initialized!");
		return;
	}

	const Matrix& matProj = m_Matrices[MATRIX_PROJECTION];
	bool bIsOrtho = (fabsf(matProj._34) < 0.001f);

	// Check if a shader is already explicitly bound
	if (SHADERMANAGER.GetCurrentShader() != SHADER_NONE)
	{
		SHADERMANAGER.SetWorldMatrix(&m_Matrices[MATRIX_WORLD]);
		SHADERMANAGER.CommitChanges();
		return;
	}

	if (bIsOrtho)
	{
		SHADERMANAGER.BeginUI();
	}
	else
	{
		// 3D mode - select shader based on vertex format
		switch (m_InputLayout)
		{
		case INPUT_LAYOUT_PNT:
		case INPUT_LAYOUT_PNT2:
		case INPUT_LAYOUT_SKINNED:
			SHADERMANAGER.BeginMesh();
			break;
		case INPUT_LAYOUT_PN:
		case INPUT_LAYOUT_TERRAIN_HTP:
			SHADERMANAGER.BeginTerrain();
			break;
		case INPUT_LAYOUT_WATER:
			SHADERMANAGER.BeginWater();
			break;
		case INPUT_LAYOUT_PDT:
		case INPUT_LAYOUT_PDT2:
		case INPUT_LAYOUT_PT:
		case INPUT_LAYOUT_PD:
		default:
			SHADERMANAGER.BeginParticle();
			break;
		}
	}

	SHADERMANAGER.SetWorldMatrix(&m_Matrices[MATRIX_WORLD]);
	SHADERMANAGER.CommitChanges();
}

// Drawing
HRESULT CStateManager::Draw(EPrimitiveTopology PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
	if (!m_pContext)
	{
		TraceError("Draw: m_pContext is NULL!");
		return E_FAIL;
	}

	CommitState();
	EnsureShadersBound();

	m_pContext->IASetPrimitiveTopology(ConvertPrimitiveType(PrimitiveType));
	m_pContext->Draw(GetPrimitiveVertexCount(PrimitiveType, PrimitiveCount), StartVertex);
	return S_OK;
}

HRESULT CStateManager::DrawDynamic(EPrimitiveTopology PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	if (!pVertexStreamZeroData || PrimitiveCount == 0)
		return E_INVALIDARG;

	CommitState();
	EnsureShadersBound();

	UINT vertexCount = GetPrimitiveVertexCount(PrimitiveType, PrimitiveCount);
	UINT dataSize = vertexCount * VertexStreamZeroStride;

	ID3D11Buffer* pVertexBuffer = nullptr;
	bool bUsingTempBuffer = false;

	if (m_pDynamicVertexBuffer && dataSize <= UP_VERTEX_BUFFER_SIZE)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		HRESULT hr = m_pContext->Map(m_pDynamicVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		if (SUCCEEDED(hr))
		{
			memcpy(mapped.pData, pVertexStreamZeroData, dataSize);
			m_pContext->Unmap(m_pDynamicVertexBuffer, 0);
			pVertexBuffer = m_pDynamicVertexBuffer;
		}
	}

	// Fallback: create temporary buffer for large data
	if (!pVertexBuffer)
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = dataSize;
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = pVertexStreamZeroData;

		HRESULT hr = m_pDevice->CreateBuffer(&bufferDesc, &initData, &pVertexBuffer);
		if (FAILED(hr))
			return hr;
		bUsingTempBuffer = true;
	}

	// Bind and draw
	UINT offset = 0;
	m_pContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &VertexStreamZeroStride, &offset);
	m_pContext->IASetPrimitiveTopology(ConvertPrimitiveType(PrimitiveType));
	m_pContext->Draw(vertexCount, 0);

	// Release temporary buffer if created
	if (bUsingTempBuffer)
		pVertexBuffer->Release();

	return S_OK;
}

HRESULT CStateManager::DrawIndexed(EPrimitiveTopology PrimitiveType, UINT minIndex, UINT NumVertices, UINT startIndex, UINT primCount, INT baseVertexIndex)
{
	CommitState();
	EnsureShadersBound();

	m_pContext->IASetPrimitiveTopology(ConvertPrimitiveType(PrimitiveType));

	UINT indexCount = 0;
	switch (PrimitiveType)
	{
	case TOPOLOGY_TRIANGLELIST: indexCount = primCount * 3; break;
	case TOPOLOGY_TRIANGLESTRIP: indexCount = primCount + 2; break;
	case TOPOLOGY_LINELIST: indexCount = primCount * 2; break;
	case TOPOLOGY_LINESTRIP: indexCount = primCount + 1; break;
	default: indexCount = primCount * 3; break;
	}

	m_pContext->DrawIndexed(indexCount, startIndex, baseVertexIndex);
	return S_OK;
}

HRESULT CStateManager::DrawIndexedDynamic(EPrimitiveTopology PrimitiveType, UINT MinVertexIndex, UINT NumVertexIndices, UINT PrimitiveCount, const void* pIndexData, DXGI_FORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	if (!pIndexData || !pVertexStreamZeroData || PrimitiveCount == 0)
		return E_INVALIDARG;

	CommitState();
	EnsureShadersBound();

	// Calculate sizes
	UINT vertexDataSize = NumVertexIndices * VertexStreamZeroStride;
	UINT indexSize = (IndexDataFormat == DXGI_FORMAT_R32_UINT) ? 4 : 2;
	UINT indexCount = 0;
	switch (PrimitiveType)
	{
	case TOPOLOGY_TRIANGLELIST: indexCount = PrimitiveCount * 3; break;
	case TOPOLOGY_TRIANGLESTRIP: indexCount = PrimitiveCount + 2; break;
	case TOPOLOGY_LINELIST: indexCount = PrimitiveCount * 2; break;
	case TOPOLOGY_LINESTRIP: indexCount = PrimitiveCount + 1; break;
	default: indexCount = PrimitiveCount * 3; break;
	}
	UINT indexDataSize = indexCount * indexSize;

	ID3D11Buffer* pVertexBuffer = nullptr;
	ID3D11Buffer* pIndexBuffer = nullptr;
	bool bUsingTempVB = false;
	bool bUsingTempIB = false;

	// Use reusable dynamic vertex buffer if data fits
	if (m_pDynamicVertexBuffer && vertexDataSize <= UP_VERTEX_BUFFER_SIZE)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		HRESULT hr = m_pContext->Map(m_pDynamicVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		if (SUCCEEDED(hr))
		{
			memcpy(mapped.pData, pVertexStreamZeroData, vertexDataSize);
			m_pContext->Unmap(m_pDynamicVertexBuffer, 0);
			pVertexBuffer = m_pDynamicVertexBuffer;
		}
	}

	// Fallback: create temporary vertex buffer
	if (!pVertexBuffer)
	{
		D3D11_BUFFER_DESC vbDesc = {};
		vbDesc.ByteWidth = vertexDataSize;
		vbDesc.Usage = D3D11_USAGE_DYNAMIC;
		vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		D3D11_SUBRESOURCE_DATA vbData = {};
		vbData.pSysMem = pVertexStreamZeroData;

		HRESULT hr = m_pDevice->CreateBuffer(&vbDesc, &vbData, &pVertexBuffer);
		if (FAILED(hr))
			return hr;
		bUsingTempVB = true;
	}

	// Use reusable dynamic index buffer if data fits
	if (m_pDynamicIndexBuffer && indexDataSize <= UP_INDEX_BUFFER_SIZE)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		HRESULT hr = m_pContext->Map(m_pDynamicIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		if (SUCCEEDED(hr))
		{
			memcpy(mapped.pData, pIndexData, indexDataSize);
			m_pContext->Unmap(m_pDynamicIndexBuffer, 0);
			pIndexBuffer = m_pDynamicIndexBuffer;
		}
	}

	// Fallback: create temporary index buffer
	if (!pIndexBuffer)
	{
		D3D11_BUFFER_DESC ibDesc = {};
		ibDesc.ByteWidth = indexDataSize;
		ibDesc.Usage = D3D11_USAGE_DYNAMIC;
		ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		D3D11_SUBRESOURCE_DATA ibData = {};
		ibData.pSysMem = pIndexData;

		HRESULT hr = m_pDevice->CreateBuffer(&ibDesc, &ibData, &pIndexBuffer);
		if (FAILED(hr))
		{
			if (bUsingTempVB) pVertexBuffer->Release();
			return hr;
		}
		bUsingTempIB = true;
	}

	// Bind and draw
	UINT offset = 0;
	m_pContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &VertexStreamZeroStride, &offset);
	m_pContext->IASetIndexBuffer(pIndexBuffer, IndexDataFormat, 0);
	m_pContext->IASetPrimitiveTopology(ConvertPrimitiveType(PrimitiveType));
	m_pContext->DrawIndexed(indexCount, 0, MinVertexIndex);

	// Release temporary buffers if created
	if (bUsingTempVB) pVertexBuffer->Release();
	if (bUsingTempIB) pIndexBuffer->Release();

	return S_OK;
}

void CStateManager::SetInputLayout(EInputLayoutType type)
{
	m_InputLayout = type;

	// Bind appropriate shader and input layout
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.BindForInputLayout(type);
	}
}

EInputLayoutType CStateManager::GetInputLayout() const
{
	return m_InputLayout;
}

void CStateManager::SaveInputLayout()
{
	m_SavedInputLayout = m_InputLayout;
}

void CStateManager::SaveInputLayout(EInputLayoutType type)
{
	m_SavedInputLayout = m_InputLayout;
	SetInputLayout(type);
}

void CStateManager::RestoreInputLayout()
{
	SetInputLayout(m_SavedInputLayout);
}


void CStateManager::SaveVertexShader()
{
	m_pSavedVertexShader = m_pCurrentVertexShader;
}

void CStateManager::RestoreVertexShader()
{
	VSSetShader(m_pSavedVertexShader);
}

void CStateManager::SavePixelShader()
{
	m_pSavedPixelShader = m_pCurrentPixelShader;
}

void CStateManager::RestorePixelShader()
{
	PSSetShader(m_pSavedPixelShader);
}
