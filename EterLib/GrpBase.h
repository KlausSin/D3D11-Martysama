#pragma once

#include "GrpDetector.h"
#include "GrpStateEnum.h"
#include "Ray.h"
#include "MatrixStack.h"
#include "GrpMathType.h"
#include "GrpMathFunc.h"
#include <vector>
#include <d3d11.h>
#include <DirectXMath.h>

using namespace DirectX;

#ifdef ENABLE_FIX_MOBS_LAG
static const std::size_t TINY_PDT_VERTEX_BUFFER_SIZE = 8;      // Single quads, UI elements
static const std::size_t SMALL_PDT_VERTEX_BUFFER_SIZE = 32;    // Small batches
static const std::size_t MEDIUM_PDT_VERTEX_BUFFER_SIZE = 128;  // Medium batches
static const std::size_t LARGE_PDT_VERTEX_BUFFER_SIZE = 512;   // Large batches
static const std::size_t XLARGE_PDT_VERTEX_BUFFER_SIZE = 2048; // Very large batches (particles, text)
#endif

void PixelPositionToVector3(const Vector3& c_rkPPosSrc, Vector3* pv3Dst);
void Vector3ToPixelPosition(const Vector3& c_rv3Src, Vector3* pv3Dst);

class CGraphicTexture;

typedef WORD TIndex;

typedef struct SFace
{
	TIndex indices[3];
} TFace;

typedef Vector3 TPosition;

typedef Vector3 TNormal;

typedef Vector2 TTextureCoordinate;

typedef DWORD TDiffuse;
typedef DWORD TAmbient;
typedef DWORD TSpecular;

typedef union UDepth
{
	float	f;
	long	l;
	DWORD	dw;
} TDepth;

typedef struct SVertex
{
	float x, y, z;
	DWORD color;
	float u, v;
} TVertex;

struct SPVertex
{
	float x, y, z;
};

typedef struct SPDVertex
{
	float x, y, z;
	DWORD color;
} TPDVertex;

struct SPDTVertexRaw
{
	float px, py, pz;
	DWORD diffuse;
	float u, v;
};

typedef struct SPTVertex
{
	TPosition position;
	TTextureCoordinate texCoord;
} TPTVertex;

typedef struct SPDTVertex
{
	TPosition	position;
	TDiffuse	diffuse;
	TTextureCoordinate texCoord;
} TPDTVertex;

typedef struct SPNTVertex
{
	TPosition			position;
	TNormal				normal;
	TTextureCoordinate	texCoord;
} TPNTVertex;

typedef struct SPNT2Vertex
{
	TPosition	position;
	TNormal		normal;
	TTextureCoordinate texCoord;
	TTextureCoordinate texCoord2;
} TPNT2Vertex;

typedef struct SPDT2Vertex
{
	TPosition	position;
	DWORD		diffuse;
	TTextureCoordinate texCoord;
	TTextureCoordinate texCoord2;
} TPDT2Vertex;

typedef struct SSkinnedVertex
{
	TPosition			position;		// 12 bytes
	TNormal				normal;			// 12 bytes
	TTextureCoordinate	texCoord;		// 8 bytes
	float				blendWeights[4];// 16 bytes (4 bone weights as floats)
	BYTE				blendIndices[4];// 4 bytes (4 bone indices as bytes)
} TSkinnedVertex;						// Total: 52 bytes

typedef struct SNameInfo
{
	DWORD	name;
	TDepth	depth;
} TNameInfo;

typedef struct SBoundBox
{
	float sx, sy, sz;
	float ex, ey, ez;
	int meshIndex;
	int boneIndex;
} TBoundBox;

const WORD c_FillRectIndices[6] = { 0, 2, 1, 2, 3, 1 };

//////////////////////////////////////////////////////////////////////////
// DX11 Input Layout Definitions
//////////////////////////////////////////////////////////////////////////

// Input element descriptions for each vertex type
namespace InputLayouts
{
	static const D3D11_INPUT_ELEMENT_DESC PDT_ELEMENTS[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,     0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	static const UINT PDT_ELEMENT_COUNT = ARRAYSIZE(PDT_ELEMENTS);

	// Position + Normal + TexCoord (PNT)
	static const D3D11_INPUT_ELEMENT_DESC PNT_ELEMENTS[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	static const UINT PNT_ELEMENT_COUNT = ARRAYSIZE(PNT_ELEMENTS);

	// Position + Normal + 2x TexCoord (PNT2)
	static const D3D11_INPUT_ELEMENT_DESC PNT2_ELEMENTS[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,       0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	static const UINT PNT2_ELEMENT_COUNT = ARRAYSIZE(PNT2_ELEMENTS);

	// Position + TexCoord (PT)
	static const D3D11_INPUT_ELEMENT_DESC PT_ELEMENTS[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	static const UINT PT_ELEMENT_COUNT = ARRAYSIZE(PT_ELEMENTS);

	static const D3D11_INPUT_ELEMENT_DESC PD_ELEMENTS[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,     0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	static const UINT PD_ELEMENT_COUNT = ARRAYSIZE(PD_ELEMENTS);

	// Position + Diffuse + 2x TexCoord (PDT2)
	static const D3D11_INPUT_ELEMENT_DESC PDT2_ELEMENTS[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,     0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	static const UINT PDT2_ELEMENT_COUNT = ARRAYSIZE(PDT2_ELEMENTS);

	static const D3D11_INPUT_ELEMENT_DESC SKINNED_ELEMENTS[] = {
		{"POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},  // 12 bytes
		{"NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},  // 12 bytes
		{"TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},  // 8 bytes
		{"BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},  // 16 bytes (4 bone weights as floats)
		{"BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,      0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},  // 4 bytes (4 bone indices as bytes)
	};
	static const UINT SKINNED_ELEMENT_COUNT = ARRAYSIZE(SKINNED_ELEMENTS);

	static const D3D11_INPUT_ELEMENT_DESC PN_ELEMENTS[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	static const UINT PN_ELEMENT_COUNT = ARRAYSIZE(PN_ELEMENTS);

	static const D3D11_INPUT_ELEMENT_DESC TRANSFORMED_ELEMENTS[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	static const UINT TRANSFORMED_ELEMENT_COUNT = ARRAYSIZE(TRANSFORMED_ELEMENTS);
}

//////////////////////////////////////////////////////////////////////////
// Input Layout Type Enumeration
//////////////////////////////////////////////////////////////////////////
enum EInputLayoutType
{
	INPUT_LAYOUT_PDT,
	INPUT_LAYOUT_PNT,
	INPUT_LAYOUT_PNT2,
	INPUT_LAYOUT_PT,
	INPUT_LAYOUT_PD,
	INPUT_LAYOUT_PDT2,
	INPUT_LAYOUT_SKINNED,
	INPUT_LAYOUT_TRANSFORMED,
	INPUT_LAYOUT_PN,           // Position + Normal only (terrain, no texcoords)
	INPUT_LAYOUT_TERRAIN_HTP,
	INPUT_LAYOUT_WATER,
	INPUT_LAYOUT_COUNT
};

namespace VertexStride
{
	constexpr UINT PDT = 24;          // float3 pos(12) + DWORD diffuse(4) + float2 tex(8)
	constexpr UINT PNT = 32;          // float3 pos(12) + float3 normal(12) + float2 tex(8)
	constexpr UINT PNT2 = 40;         // float3 pos(12) + float3 normal(12) + float2 tex(8) + float2 tex2(8)
	constexpr UINT PT = 20;           // float3 pos(12) + float2 tex(8)
	constexpr UINT PD = 16;           // float3 pos(12) + DWORD diffuse(4)
	constexpr UINT PDT2 = 32;         // float3 pos(12) + DWORD diffuse(4) + float2 tex(8) + float2 tex2(8)
	constexpr UINT PN = 24;           // float3 pos(12) + float3 normal(12)
	constexpr UINT TRANSFORMED = 24;  // float3 pos(12) + DWORD diffuse(4) + float2 tex(8)
	constexpr UINT SKINNED = 52;      // float3 pos(12) + float3 normal(12) + float2 tex(8) + float4 weights(16) + ubyte4 indices(4)
}

// Get vertex stride for a given input layout type
inline UINT GetVertexStride(EInputLayoutType type)
{
	switch (type)
	{
	case INPUT_LAYOUT_PDT:         return VertexStride::PDT;
	case INPUT_LAYOUT_PNT:         return VertexStride::PNT;
	case INPUT_LAYOUT_PNT2:        return VertexStride::PNT2;
	case INPUT_LAYOUT_PT:          return VertexStride::PT;
	case INPUT_LAYOUT_PD:          return VertexStride::PD;
	case INPUT_LAYOUT_PDT2:        return VertexStride::PDT2;
	case INPUT_LAYOUT_PN:          return VertexStride::PN;
	case INPUT_LAYOUT_TRANSFORMED: return VertexStride::TRANSFORMED;
	case INPUT_LAYOUT_SKINNED:     return VertexStride::SKINNED;
	case INPUT_LAYOUT_TERRAIN_HTP: return VertexStride::PN;
	case INPUT_LAYOUT_WATER:       return VertexStride::PD;
	default:                       return VertexStride::PDT;
	}
}

class CGraphicBase
{
	public:
		static DWORD GetAvailableTextureMemory();
		static const Matrix& GetViewMatrix();
		static const Matrix& GetIdentityMatrix();

		enum
		{
			DEFAULT_IB_LINE,
			DEFAULT_IB_LINE_TRI,
			DEFAULT_IB_LINE_RECT,
			DEFAULT_IB_LINE_CUBE,
			DEFAULT_IB_FILL_TRI,
			DEFAULT_IB_FILL_RECT,
			DEFAULT_IB_FILL_CUBE,
			DEFAULT_IB_FILL_RECT_MULTI,		// Tiled quad indices for batch rendering (up to 512 quads)
			DEFAULT_IB_NUM,
		};

	public:
		CGraphicBase();
		virtual	~CGraphicBase();

#ifdef ENABLE_FIX_MOBS_LAG
		static ID3D11Buffer* GetTinyPdtVertexBuffer() { return m_tinyPdtVertexBuffer; }
		static ID3D11Buffer* GetSmallPdtVertexBuffer() { return m_smallPdtVertexBuffer; }
		static ID3D11Buffer* GetMediumPdtVertexBuffer() { return m_mediumPdtVertexBuffer; }
		static ID3D11Buffer* GetLargePdtVertexBuffer() { return m_largePdtVertexBuffer; }
		static ID3D11Buffer* GetXLargePdtVertexBuffer() { return m_xlargePdtVertexBuffer; }
		static ID3D11Buffer* GetPdtVertexBufferForSize(UINT uVtxCount);
#endif

		void		SetSimpleCamera(float x, float y, float z, float pitch, float roll);
		void		SetEyeCamera(float xEye, float yEye, float zEye, float xCenter, float yCenter, float zCenter, float xUp, float yUp, float zUp);
		void		SetAroundCamera(float distance, float pitch, float roll, float lookAtZ = 0.0f);
		void		SetPositionCamera(float fx, float fy, float fz, float fDistance, float fPitch, float fRotation);
		void		MoveCamera(float fdeltax, float fdeltay, float fdeltaz);

		void		GetTargetPosition(float * px, float * py, float * pz);
		void		GetCameraPosition(float * px, float * py, float * pz);
		void		SetOrtho2D(float hres, float vres, float zres);
		void		SetOrtho3D(float hres, float vres, float zmin, float zmax);
		void		SetPerspective(float fov, float aspect, float nearz, float farz);
		float		GetFOV();
		void		GetClipPlane(float * fNearY, float * fFarY)
		{
			*fNearY = ms_fNearY;
			*fFarY = ms_fFarY;
		}

		////////////////////////////////////////////////////////////////////////
		void		PushMatrix();

		void		MultMatrix(const Matrix* pMat);
		void		MultMatrixLocal(const Matrix* pMat);

		void		Translate(float x, float y, float z);
		void		Rotate(float degree, float x, float y, float z);
		void		RotateLocal(float degree, float x, float y, float z);
		void		RotateYawPitchRollLocal(float fYaw, float fPitch, float fRoll);
		void		Scale(float x, float y, float z);
		void		PopMatrix();
		void		LoadMatrix(const Matrix& c_rSrcMatrix);
		void		GetMatrix(Matrix* pRetMatrix) const;
		const		Matrix* GetMatrixPointer() const;

		// Special Routine
		void		GetSphereMatrix(Matrix* pMatrix, float fValue = 0.1f);

		////////////////////////////////////////////////////////////////////////
		void		InitScreenEffect();
		void		SetScreenEffectWaving(float fDuringTime, int iPower);
		void		SetScreenEffectFlashing(float fDuringTime, const Color& c_rColor);

		////////////////////////////////////////////////////////////////////////
		DWORD		GetColor(float r, float g, float b, float a = 1.0f);

		DWORD		GetFaceCount();
		void		ResetFaceCount();
		HRESULT		GetLastResult();

		void		UpdateProjMatrix();
		void		UpdateViewMatrix();

		void		SetViewport(DWORD dwX, DWORD dwY, DWORD dwWidth, DWORD dwHeight, float fMinZ, float fMaxZ);
		static void		GetBackBufferSize(UINT* puWidth, UINT* puHeight);
		static bool		IsTLVertexClipping();
		static bool		IsLowTextureMemory();
		static bool		IsHighTextureMemory();

		// Device accessors for DX11
		static ID3D11Device* GetDevice() { return ms_pDevice; }
		static ID3D11DeviceContext* GetContext() { return ms_pContext; }
		static IDXGISwapChain* GetSwapChain() { return ms_pSwapChain; }
		static const D3D11_VIEWPORT& GetViewport() { return ms_Viewport; }
		static const Matrix& GetProjectionMatrix() { return ms_matProj; }

#ifdef ENABLE_SSAO
		static bool GetSSAOEnabled() { return ms_bSSAOEnabled; }
		static void SetSSAOEnabled(bool b) { ms_bSSAOEnabled = b; }
#endif

		static ID3D11RenderTargetView* GetRenderTargetView()
		{
#ifdef ENABLE_MSAA
			return ms_pMSAARenderTargetView ? ms_pMSAARenderTargetView : ms_pRenderTargetView;
#else
			return ms_pRenderTargetView;
#endif
		}
		static ID3D11DepthStencilView* GetDepthStencilView()
		{
#ifdef ENABLE_MSAA
			return ms_pMSAADepthStencilView ? ms_pMSAADepthStencilView : ms_pDepthStencilView;
#else
			return ms_pDepthStencilView;
#endif
		}

		// Direct swap chain back buffer accessors (bypass MSAA)
		static ID3D11RenderTargetView* GetSwapChainRenderTargetView() { return ms_pRenderTargetView; }
		static ID3D11DepthStencilView* GetSwapChainDepthStencilView() { return ms_pDepthStencilView; }

#ifdef ENABLE_MSAA
		static ID3D11RenderTargetView* GetMSAARenderTargetView() { return ms_pMSAARenderTargetView; }
		static ID3D11DepthStencilView* GetMSAADepthStencilView() { return ms_pMSAADepthStencilView; }
		static UINT GetMSAASampleCount() { return ms_uMSAASampleCount; }

		// Resolve MSAA RT to swap chain back buffer
		static void ResolveMSAA()
		{
			if (ms_uMSAASampleCount > 1 && ms_pMSAARenderTarget && ms_pSwapChain)
			{
				ID3D11Texture2D* pBackBuffer = nullptr;
				ms_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
				if (pBackBuffer)
				{
					ms_pContext->ResolveSubresource(pBackBuffer, 0, ms_pMSAARenderTarget, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
					pBackBuffer->Release();
				}
			}
		}

#ifdef ENABLE_BLOOM
		// Resolve MSAA RT to scene texture for bloom processing
		static void ResolveMSAAToSceneTexture()
		{
			if (ms_uMSAASampleCount > 1 && ms_pMSAARenderTarget && ms_pSceneTexture)
			{
				ms_pContext->ResolveSubresource(ms_pSceneTexture, 0, ms_pMSAARenderTarget, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
			}
		}
#endif
#endif

		static ID3D11DeviceContext* GetActiveContext();

		static CMatrixStack* GetActiveMatrixStack();

		static void SetDefaultIndexBuffer(UINT eDefIB);
		static bool SetPDTStream(SPDTVertexRaw* pVertices, UINT uVtxCount);
		static bool SetPDTStream(SPDTVertex* pVertices, UINT uVtxCount);


	protected:
		static Matrix					ms_matIdentity;

		static Matrix					ms_matView;
		static Matrix					ms_matProj;
		static Matrix					ms_matInverseView;
		static Matrix					ms_matInverseViewYAxis;

		static Matrix					ms_matWorld;
		static Matrix					ms_matWorldView;

	protected:
		void		UpdatePipeLineMatrix();

	protected:
		static HRESULT					ms_hLastResult;

		static int						ms_iWidth;
		static int						ms_iHeight;

		static UINT						ms_iD3DAdapterInfo;
		static UINT						ms_iD3DDevInfo;
		static UINT						ms_iD3DModeInfo;
		static D3D_CDisplayModeAutoDetector				ms_kD3DDetector;

		static HWND						ms_hWnd;
		static HDC						ms_hDC;

		// DX11 Core Objects
		static ID3D11Device*			ms_pDevice;
		static ID3D11DeviceContext*		ms_pContext;
		static IDXGISwapChain*			ms_pSwapChain;
		static IDXGIFactory*			ms_pDXGIFactory;
		static ID3D11RenderTargetView*	ms_pRenderTargetView;
		static ID3D11DepthStencilView*	ms_pDepthStencilView;
		static ID3D11Texture2D*			ms_pDepthStencilBuffer;
		static D3D_FEATURE_LEVEL		ms_FeatureLevel;

#ifdef ENABLE_MSAA
		static ID3D11Texture2D*			ms_pMSAARenderTarget;
		static ID3D11RenderTargetView*	ms_pMSAARenderTargetView;
		static ID3D11Texture2D*			ms_pMSAADepthStencilBuffer;
		static ID3D11DepthStencilView*	ms_pMSAADepthStencilView;
		static UINT						ms_uMSAASampleCount;
#endif

#ifdef ENABLE_SSAO
		static ID3D11ShaderResourceView* ms_pDepthStencilSRV;
		static ID3D11DepthStencilView*   ms_pDepthStencilViewReadOnly;
#ifdef ENABLE_MSAA
		static ID3D11ShaderResourceView* ms_pMSAADepthStencilSRV;
#endif

		// SSAO resources
		static ID3D11Texture2D*			ms_pResolvedDepthTex;     // Full-res R32_FLOAT (MSAA depth resolve target)
		static ID3D11RenderTargetView*	ms_pResolvedDepthRTV;
		static ID3D11ShaderResourceView* ms_pResolvedDepthSRV;

		static ID3D11Texture2D*			ms_pSSAO_RT;              // Half-res R8_UNORM
		static ID3D11RenderTargetView*	ms_pSSAO_RTV;
		static ID3D11ShaderResourceView* ms_pSSAO_SRV;

		static ID3D11Texture2D*			ms_pSSAOBlur_RT;          // Half-res R8_UNORM
		static ID3D11RenderTargetView*	ms_pSSAOBlur_RTV;
		static ID3D11ShaderResourceView* ms_pSSAOBlur_SRV;

		static bool						ms_bSSAOEnabled;

		static ID3D11ShaderResourceView* GetDepthSRV()
		{
#ifdef ENABLE_MSAA
			return (ms_uMSAASampleCount > 1) ? ms_pResolvedDepthSRV : ms_pDepthStencilSRV;
#else
			return ms_pDepthStencilSRV;
#endif
		}
#endif // ENABLE_SSAO

#ifdef ENABLE_BLOOM
		// Scene texture (full-res, non-MSAA, for bloom input)
		static ID3D11Texture2D*			ms_pSceneTexture;
		static ID3D11RenderTargetView*	ms_pSceneTextureRTV;
		static ID3D11ShaderResourceView* ms_pSceneTextureSRV;

		// Bloom render targets (1/4 resolution)
		static ID3D11Texture2D*			ms_pBloomRTA;
		static ID3D11RenderTargetView*	ms_pBloomRTA_RTV;
		static ID3D11ShaderResourceView* ms_pBloomRTA_SRV;

		static ID3D11Texture2D*			ms_pBloomRTB;
		static ID3D11RenderTargetView*	ms_pBloomRTB_RTV;
		static ID3D11ShaderResourceView* ms_pBloomRTB_SRV;

		static bool						ms_bBloomEnabled;

#ifdef ENABLE_GODRAYS
		static ID3D11Texture2D*			ms_pGodRaysRT;
		static ID3D11RenderTargetView*	ms_pGodRaysRTV;
		static ID3D11ShaderResourceView* ms_pGodRaysSRV;
#endif

#endif

		// Matrix stack
		static CMatrixStack*			ms_pMatrixStack;

		// Viewport
		static D3D11_VIEWPORT			ms_Viewport;

		static DWORD					ms_faceCount;

		static Matrix					ms_matScreen0;
		static Matrix					ms_matScreen1;
		static Matrix					ms_matScreen2;

		static Vector3					ms_vtPickRayOrig;
		static Vector3					ms_vtPickRayDir;

		static float					ms_fFieldOfView;
		static float					ms_fAspect;
		static float					ms_fNearY;
		static float					ms_fFarY;

		// Screen Effect - Waving, Flashing and so on..
		static DWORD					ms_dwWavingEndTime;
		static int						ms_iWavingPower;
		static DWORD					ms_dwFlashingEndTime;
		static Color					ms_FlashingColor;

 		static CRay						ms_Ray;

		static bool						ms_bSupportDXT;
		static bool						ms_isLowTextureMemory;
		static bool						ms_isHighTextureMemory;

		enum
		{
			PDT_VERTEX_NUM = 16,
			PDT_VERTEXBUFFER_NUM = 100,
		};

#ifdef ENABLE_FIX_MOBS_LAG
		static ID3D11Buffer* m_tinyPdtVertexBuffer;
		static ID3D11Buffer* m_smallPdtVertexBuffer;
		static ID3D11Buffer* m_mediumPdtVertexBuffer;
		static ID3D11Buffer* m_largePdtVertexBuffer;
		static ID3D11Buffer* m_xlargePdtVertexBuffer;
#else
		static ID3D11Buffer*	ms_alpd3dPDTVB[PDT_VERTEXBUFFER_NUM];
#endif
		static ID3D11Buffer*	ms_alpd3dDefIB[DEFAULT_IB_NUM];
};
