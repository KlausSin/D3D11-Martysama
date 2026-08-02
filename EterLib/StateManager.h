#pragma once

/*
 * StateManager.h
 * DirectX 11 State Management System
 *
 * This class manages native DX11 state objects:
 *   - Blend states (alpha blending)
 *   - Rasterizer states (culling, fill mode)
 *   - Depth-stencil states
 *   - Sampler states (texture filtering)
 *   - Transform matrices (stored for shader constant buffers)
 *   - Buffer binding (vertex/index buffers)
 *
 * LIGHTING & MATERIAL:
 * All lighting/material/texture-factor APIs forward directly to ShaderManager
 * which manages the CBLighting constant buffer for native DX11 multi-light rendering.
 * For new code, call SHADERMANAGER directly:
 *   - SHADERMANAGER.SetLight(index, DX11Light) - up to 16 lights
 *   - SHADERMANAGER.EnableLight(index, bool)
 *   - SHADERMANAGER.SetGlobalAmbient(r,g,b,a)
 *   - SHADERMANAGER.SetDiffuseColor/SetEmissiveColor/SetSpecularColor
 *   - SHADERMANAGER.SetTextureFactor(dwColor)
 */

#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include <map>

#include "../eterBase/Singleton.h"
#include "StateObjectCache.h"
#include "GrpStateEnum.h"
#include "GrpMathType.h"
#include "GrpDetector.h"
#include "GrpBase.h"

using namespace DirectX;

static const DWORD STATEMANAGER_MAX_STAGES = 8;
static const DWORD STATEMANAGER_MAX_VCONSTANTS = 256;
static const DWORD STATEMANAGER_MAX_PCONSTANTS = 32;
static const DWORD STATEMANAGER_MAX_TRANSFORMSTATES = 300;
static const DWORD STATEMANAGER_MAX_STREAMS = 16;
static const DWORD STATEMANAGER_MAX_LIGHTS = 16;  // Expanded from 8 for multi-light support

// Forward declarations
class CStateObjectCache;
struct TLight;
struct TMaterial;

// Stream data for vertex buffer binding
class CStreamData
{
public:
	CStreamData(ID3D11Buffer* pStreamData = nullptr, UINT Stride = 0, UINT Offset = 0)
		: m_pStreamData(pStreamData), m_Stride(Stride), m_Offset(Offset)
	{
	}

	bool operator == (const CStreamData& rhs) const
	{
		return (m_pStreamData == rhs.m_pStreamData) &&
			(m_Stride == rhs.m_Stride) &&
			(m_Offset == rhs.m_Offset);
	}

	ID3D11Buffer*	m_pStreamData;
	UINT			m_Stride;
	UINT			m_Offset;
};

// Index data for index buffer binding
class CIndexData
{
public:
	CIndexData(ID3D11Buffer* pIndexData = nullptr, DXGI_FORMAT Format = DXGI_FORMAT_R16_UINT, UINT Offset = 0)
		: m_pIndexData(pIndexData), m_Format(Format), m_Offset(Offset)
	{
	}

	bool operator == (const CIndexData& rhs) const
	{
		return (m_pIndexData == rhs.m_pIndexData) &&
			(m_Format == rhs.m_Format) &&
			(m_Offset == rhs.m_Offset);
	}

	ID3D11Buffer*	m_pIndexData;
	DXGI_FORMAT		m_Format;
	UINT			m_Offset;
};

struct SDX11PendingState
{
	// Blend state components
	bool	bAlphaBlendEnable;
	D3D11_BLEND		srcBlend;
	D3D11_BLEND		destBlend;
	D3D11_BLEND_OP	blendOp;
	D3D11_BLEND		srcBlendAlpha;
	D3D11_BLEND		destBlendAlpha;
	D3D11_BLEND_OP	blendOpAlpha;
	UINT8			colorWriteMask;

	// Rasterizer state components
	D3D11_FILL_MODE fillMode;
	D3D11_CULL_MODE cullMode;
	bool			bScissorEnable;
	bool			bMultisampleEnable;
	bool			bAntialiasedLineEnable;
	INT				depthBias;
	float			depthBiasClamp;
	float			slopeScaledDepthBias;
	bool			bDepthClipEnable;

	// Depth stencil state components
	bool			bDepthEnable;
	bool			bDepthWriteEnable;
	D3D11_COMPARISON_FUNC depthFunc;
	bool			bStencilEnable;
	UINT8			stencilReadMask;
	UINT8			stencilWriteMask;

	// Sampler state per stage
	D3D11_FILTER			samplerFilter[STATEMANAGER_MAX_STAGES];
	D3D11_TEXTURE_ADDRESS_MODE	addressU[STATEMANAGER_MAX_STAGES];
	D3D11_TEXTURE_ADDRESS_MODE	addressV[STATEMANAGER_MAX_STAGES];
	D3D11_TEXTURE_ADDRESS_MODE	addressW[STATEMANAGER_MAX_STAGES];
	UINT					maxAnisotropy[STATEMANAGER_MAX_STAGES];
	bool					bMipmapDisabled[STATEMANAGER_MAX_STAGES];  // FILTER_NONE for mipfilter

	void SetDefaults()
	{
		bAlphaBlendEnable = false;
		srcBlend = D3D11_BLEND_SRC_ALPHA;
		destBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendOp = D3D11_BLEND_OP_ADD;
		srcBlendAlpha = D3D11_BLEND_ONE;
		destBlendAlpha = D3D11_BLEND_ZERO;
		blendOpAlpha = D3D11_BLEND_OP_ADD;
		colorWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		fillMode = D3D11_FILL_SOLID;
		cullMode = D3D11_CULL_BACK;
		bScissorEnable = false;
		bMultisampleEnable = false;
		bAntialiasedLineEnable = false;
		depthBias = 0;
		depthBiasClamp = 0.0f;
		slopeScaledDepthBias = 0.0f;
		bDepthClipEnable = true;

		bDepthEnable = true;
		bDepthWriteEnable = true;
		depthFunc = D3D11_COMPARISON_LESS_EQUAL;
		bStencilEnable = false;
		stencilReadMask = 0xFF;
		stencilWriteMask = 0xFF;

		for (int i = 0; i < STATEMANAGER_MAX_STAGES; ++i)
		{
			samplerFilter[i] = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			addressU[i] = D3D11_TEXTURE_ADDRESS_WRAP;
			addressV[i] = D3D11_TEXTURE_ADDRESS_WRAP;
			addressW[i] = D3D11_TEXTURE_ADDRESS_WRAP;
			maxAnisotropy[i] = 16;
			bMipmapDisabled[i] = false;
		}
	}
};

struct SD3D11Material
{
	XMFLOAT4 Diffuse;
	XMFLOAT4 Ambient;
	XMFLOAT4 Specular;
	XMFLOAT4 Emissive;
	float Power;
	float _pad[3];
};

class CStateManager : public CSingleton<CStateManager>
{
public:
	CStateManager(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual ~CStateManager();

	void	SetDefaultState();
	void	Restore();

	// Material (forwards to ShaderManager - consider using SHADERMANAGER directly for new code)
	void	SaveMaterial();
	void	SaveMaterial(const TMaterial* pMaterial);
	void	RestoreMaterial();
	void	SetMaterial(const TMaterial* pMaterial);
	void	GetMaterial(TMaterial* pMaterial);

	// Lights (forwards to ShaderManager - consider using SHADERMANAGER directly for new code)
	void	SetLight(DWORD index, const TLight* pLight);
	void	GetLight(DWORD index, TLight* pLight);
	void	LightEnable(DWORD index, BOOL bEnable);


	// Fog control (forwards to SHADERMANAGER.SetFog)
	void	SetFogEnabled(bool bEnable);
	void	SetFogParams(float fStart, float fEnd, DWORD dwColor);
	void	SetFogColor(DWORD dwColor);
	bool	GetFogEnabled() const { return m_bFogEnabled; }

	void	SetAlphaTestEnabled(bool bEnable);
	void	SetAlphaTestRef(float fRef);  // 0.0 - 1.0
	void	SetAlphaTestRefByte(DWORD dwRef);  // 0 - 255
	bool	GetAlphaTestEnabled() const { return m_bAlphaTestEnabled; }
	float	GetAlphaTestRef() const { return m_fAlphaRef; }

	void	SetLightingEnabled(bool bEnable);
	bool	GetLightingEnabled() const { return m_bLightingEnabled; }

	void	SetTextureFactor(DWORD dwColor);
	DWORD	GetTextureFactor() const { return m_dwTextureFactor; }

	// Render states (mapped to DX11 state objects ONLY)
	void	SavePipelineState(EPipelineState Type, DWORD dwValue);
	void	RestorePipelineState(EPipelineState Type);
	void	SetPipelineState(EPipelineState Type, DWORD Value);
	void	GetPipelineState(EPipelineState Type, DWORD* pdwValue);
	DWORD	GetPipelineState(EPipelineState Type);

	// Textures
	void	SaveTexture(DWORD dwStage, ID3D11ShaderResourceView* pTexture);
	void	RestoreTexture(DWORD dwStage);
	void	SetShaderResource(DWORD dwStage, ID3D11ShaderResourceView* pTexture);
	void	GetTexture(DWORD dwStage, ID3D11ShaderResourceView** ppTexture);

	// Sampler states
	void	SaveSamplerState(DWORD dwStage, ESamplerState Type, DWORD dwValue);
	void	RestoreSamplerState(DWORD dwStage, ESamplerState Type);
	void	SetSamplerState(DWORD dwStage, ESamplerState Type, DWORD dwValue);
	void	GetSamplerState(DWORD dwStage, ESamplerState Type, DWORD* pdwValue);

	void	SetBestFiltering(DWORD dwStage);

	// Shaders
	void	VSSetShader(ID3D11VertexShader* pShader);
	void	PSSetShader(ID3D11PixelShader* pShader);
	void	SetInputLayout(ID3D11InputLayout* pLayout);
	void	SaveVertexShader();
	void	RestoreVertexShader();
	void	SavePixelShader();
	void	RestorePixelShader();

	// Input Layout (Native DX11)
	void	SetInputLayout(EInputLayoutType type);
	EInputLayoutType GetInputLayout() const;
	void	SaveInputLayout();
	void	SaveInputLayout(EInputLayoutType type);  // Save current and set new
	void	RestoreInputLayout();

	// Transform (stored for shader constant buffers)
	void	SaveTransform(EMatrixSlot Transform, const Matrix* pMatrix);
	void	RestoreTransform(EMatrixSlot Transform);
	void	SetMatrix(EMatrixSlot Type, const Matrix* pMatrix);
	void	GetMatrix(EMatrixSlot Type, Matrix* pMatrix);

	// Stream/Index binding
	void	SaveStreamSource(UINT StreamNumber, ID3D11Buffer* pStreamData, UINT Stride);
	void	RestoreStreamSource(UINT StreamNumber);
	void	SetVertexBuffer(UINT StreamNumber, ID3D11Buffer* pStreamData, UINT Stride, UINT Offset = 0);

	void	SaveIndices(ID3D11Buffer* pIndexData);
	void	RestoreIndices();
	void	SetIndexBuffer(ID3D11Buffer* pIndexData, DXGI_FORMAT Format = DXGI_FORMAT_R16_UINT, UINT Offset = 0);

	// Drawing
	HRESULT Draw(EPrimitiveTopology PrimitiveType, UINT StartVertex, UINT PrimitiveCount);
	HRESULT DrawDynamic(EPrimitiveTopology PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride);
	HRESULT DrawIndexed(EPrimitiveTopology PrimitiveType, UINT minIndex, UINT NumVertices, UINT startIndex, UINT primCount, INT baseVertexIndex = 0);
	HRESULT DrawIndexedDynamic(EPrimitiveTopology PrimitiveType, UINT MinVertexIndex, UINT NumVertexIndices, UINT PrimitiveCount, const void* pIndexData, DXGI_FORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride);

	// Commit pending state changes to hardware
	void	CommitState();

	// Accessors
	ID3D11Device* GetDevice() const { return m_pDevice; }
	ID3D11DeviceContext* GetContext() const { return m_pContext; }

private:
	void SetDevice(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	void UpdateBlendState();
	void UpdateRasterizerState();
	void UpdateDepthStencilState();
	void UpdateSamplerStates();
	void EnsureShadersBound();
	D3D11_PRIMITIVE_TOPOLOGY ConvertPrimitiveType(EPrimitiveTopology type);
	UINT GetPrimitiveVertexCount(EPrimitiveTopology type, UINT primitiveCount);

private:
	ID3D11Device*			m_pDevice;
	ID3D11DeviceContext*	m_pContext;
	CStateObjectCache*		m_pStateCache;

	// Current and saved state
	SDX11PendingState		m_CurrentState;
	SDX11PendingState		m_SavedState;

	// Currently bound state objects
	ID3D11BlendState*			m_pCurrentBlendState;
	ID3D11RasterizerState*		m_pCurrentRasterizerState;
	ID3D11DepthStencilState*	m_pCurrentDepthStencilState;
	ID3D11SamplerState*			m_pCurrentSamplerStates[STATEMANAGER_MAX_STAGES];

	// Dirty flags
	bool m_bBlendStateDirty;
	bool m_bRasterizerStateDirty;
	bool m_bDepthStencilStateDirty;
	bool m_bSamplerStateDirty[STATEMANAGER_MAX_STAGES];

	// Textures
	ID3D11ShaderResourceView*	m_pTextures[STATEMANAGER_MAX_STAGES];
	ID3D11ShaderResourceView*	m_pSavedTextures[STATEMANAGER_MAX_STAGES];

	// Transform matrices
	Matrix			m_Matrices[STATEMANAGER_MAX_TRANSFORMSTATES];
	Matrix			m_SavedMatrices[STATEMANAGER_MAX_TRANSFORMSTATES];

	// Material (light data managed entirely by ShaderManager)
	SD3D11Material	m_Material;
	SD3D11Material	m_SavedMaterial;

	// Stream and index data
	CStreamData		m_StreamData[STATEMANAGER_MAX_STREAMS];
	CStreamData		m_SavedStreamData[STATEMANAGER_MAX_STREAMS];
	CIndexData		m_IndexData;
	CIndexData		m_SavedIndexData;

	// Shader parameters (pure DX11 - constant buffer values)
	bool			m_bFogEnabled;
	float			m_fFogStart;
	float			m_fFogEnd;
	DWORD			m_dwFogColor;
	bool			m_bAlphaTestEnabled;
	float			m_fAlphaRef;
	bool			m_bLightingEnabled;
	DWORD			m_dwTextureFactor;

	// Input layout (for shader binding)
	EInputLayoutType	m_InputLayout;
	EInputLayoutType	m_SavedInputLayout;

	// Saved shader state
	ID3D11VertexShader*		m_pSavedVertexShader;
	ID3D11PixelShader*		m_pSavedPixelShader;
	ID3D11VertexShader*		m_pCurrentVertexShader;
	ID3D11PixelShader*		m_pCurrentPixelShader;

	// Render state storage for save/restore
	std::map<EPipelineState, DWORD>	m_SavedRenderStates;
	std::map<std::pair<DWORD, ESamplerState>, DWORD> m_SavedSamplerStates;

	static const UINT UP_VERTEX_BUFFER_SIZE = 65536;  // 64KB for vertex data
	static const UINT UP_INDEX_BUFFER_SIZE = 32768;   // 32KB for index data
	ID3D11Buffer*	m_pDynamicVertexBuffer;
	ID3D11Buffer*	m_pDynamicIndexBuffer;
};

#define STATEMANAGER (CStateManager::Instance())
