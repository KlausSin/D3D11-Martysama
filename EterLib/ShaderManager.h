#pragma once

/*
 * ShaderManager.h
 * DX11 Unified Shader System (Shader Model 5.0)
 *
 * Dedicated shaders
 * for each rendering type. HLSL is loaded from FoxFS/CEterPackManager and CSO is cached on disk.
 *
 * Features:
 * - Texture stage blending (MODULATE, SELECTARG1, etc.)
 * - Texture factor color modulation
 * - Multi-texture support (2 texture stages)
 * - Per-vertex lighting toggle
 * - Material properties (diffuse, specular, emissive)
 * - Alpha testing
 * - Fog
 */

#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <unordered_map>
#include "GrpBase.h"
#include "GrpLightType.h"
#include "GrpStateEnum.h"
#include "StateObjectCache.h"
#include "../eterBase/Singleton.h"

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;
#include <wrl/client.h>
using namespace Microsoft::WRL;

//////////////////////////////////////////////////////////////////////////
// Shader Type Enumeration
//////////////////////////////////////////////////////////////////////////
enum EShaderType
{
	SHADER_NONE = -1,
	SHADER_UI,           // UI/2D rendering (TRANSFORMED/screen-space vertices)
	SHADER_MESH,         // 3D meshes with lighting (PNT vertices, single texture)
	SHADER_MESH_2TEX,    // 3D meshes with lighting (PNT2 vertices, two textures)
	SHADER_TERRAIN,      // Terrain rendering (PN vertices, generates texcoords)
	SHADER_WATER,        // Water surfaces (PD vertices)
	SHADER_SKY,          // Skybox rendering (PDT vertices, no lighting)
	SHADER_PARTICLE,     // Particle effects (PT/PDT vertices)
	SHADER_SHADOW,       // Shadow map rendering (depth-only pass)
	SHADER_SHADOW_SKINNED, // Shadow map rendering for GPU skinned meshes
	SHADER_SPEEDTREE,    // SpeedTree branches/fronds with wind animation
	SHADER_SPEEDTREE_LEAF, // SpeedTree leaves with GPU placement
	SHADER_MESH_NORMAL,  // Normal mapped mesh with tangent space lighting
	SHADER_MESH_SKINNED, // GPU skinned mesh (bone transforms on GPU)
	SHADER_GODRAYS,      // Volumetric light rays (god rays) post-process
	SHADER_MESH_VTF,     // VTF batched mesh rendering (instanced with vertex texture fetch)
	SHADER_SHADOW_VTF,   // VTF batched shadow rendering (instanced depth-only)
	SHADER_SPEEDTREE_VTF,// VTF batched SpeedTree rendering (instanced vegetation)
	SHADER_MESH_2TEX_VTF,// VTF batched two-texture mesh rendering (instanced dungeon blocks)
	SHADER_PARTICLE_PCT, // Particle effects with per-vertex color (PCT format, for CS output)
#ifdef ENABLE_BLOOM
	SHADER_BLOOM_BRIGHT,    // Bloom bright pass (luminance threshold)
	SHADER_BLOOM_BLUR,      // Bloom separable Gaussian blur
	SHADER_BLOOM_COMPOSITE, // Bloom scene + bloom additive composite
#endif
#ifdef ENABLE_SSAO
	SHADER_SSAO,            // SSAO hemisphere sampling
	SHADER_SSAO_BLUR,       // SSAO bilateral blur
	SHADER_DEPTH_RESOLVE,   // MSAA depth resolve (sample 0)
#endif
	SHADER_COUNT
};

//////////////////////////////////////////////////////////////////////////
// Compute Shader Type Enumeration
//////////////////////////////////////////////////////////////////////////
// Particle colour operation (values come straight from the effect data)
enum EParticleColorOp
{
	PARTICLE_COLOROP_DISABLE = 1,
	PARTICLE_COLOROP_SELECTARG1 = 2,
	PARTICLE_COLOROP_SELECTARG2 = 3,
	PARTICLE_COLOROP_MODULATE = 4,
	PARTICLE_COLOROP_MODULATE2X = 5,
	PARTICLE_COLOROP_MODULATE4X = 6,
	PARTICLE_COLOROP_ADD = 7,
};

enum EComputeShader
{
	CS_PARTICLE_BILLBOARD,  // Generate particle billboard quads on GPU
	CS_SNOW_BILLBOARD,      // Generate snow billboard quads on GPU
	CS_FLYTRACE,            // Generate fly trace billboard segments on GPU
	CS_WEAPONTRACE,         // Evaluate weapon trace splines on GPU
	CS_COUNT
};

//////////////////////////////////////////////////////////////////////////
// GPU Buffer wrapper (structured/raw buffer with SRV/UAV)
//////////////////////////////////////////////////////////////////////////
struct GpuBuffer
{
	ID3D11Buffer* pBuffer;
	ID3D11ShaderResourceView* pSRV;
	ID3D11UnorderedAccessView* pUAV;
	UINT elementCount;
	UINT elementSize;
	GpuBuffer() : pBuffer(nullptr), pSRV(nullptr), pUAV(nullptr), elementCount(0), elementSize(0) {}
	bool IsValid() const { return pBuffer != nullptr; }
};


static const int MAX_SHADER_LIGHTS = 16;

// Light types: Use ELightType enum from GrpStateEnum.h
// LIGHT_POINT = 1, LIGHT_SPOT = 2, LIGHT_DIRECTIONAL = 3

// Native DX11 light for shader constant buffers
struct DX11Light
{
	XMFLOAT4 Position;      // xyz = position, w = type (uses ELightType: 1=point, 2=spot, 3=dir)
	XMFLOAT4 Direction;     // xyz = direction, w = enabled (0 or 1)
	XMFLOAT4 Color;         // rgb = diffuse color, a = intensity
	XMFLOAT4 Attenuation;   // x = constant, y = linear, z = quadratic, w = range
};

// Native DX11 material for shader constant buffers
struct DX11Material
{
	XMFLOAT4 Diffuse;
	XMFLOAT4 Specular;
	XMFLOAT4 Emissive;
	float SpecularPower;
	float _pad[3];
};

//////////////////////////////////////////////////////////////////////////
// Constant Buffer Structures (16-byte aligned for HLSL)
//////////////////////////////////////////////////////////////////////////

// Lighting constant buffer - supports multiple lights
__declspec(align(16)) struct CBLighting
{
	DX11Light lights[MAX_SHADER_LIGHTS];
	XMFLOAT4 globalAmbient;     // rgb = ambient color, a = unused
	int numActiveLights;
	int _pad[3];
};

// Per-frame constants - updated once per frame
__declspec(align(16)) struct CBPerFrame
{
	XMMATRIX matView;
	XMMATRIX matProjection;
	XMFLOAT4 vCameraPos;      // xyz = camera position, w = viewport width
	XMFLOAT4 vFogParams;      // x = fogStart, y = fogEnd, z = viewport height, w = fogEnabled
	XMFLOAT4 vFogColor;       // rgb = fog color, a = unused
	XMFLOAT4 vTime;           // x = total time, y = delta time, z = cloud layer2 speed mult, w = unused
	XMFLOAT4 vSunDirection;   // xyz = normalized sun direction, w = sun intensity

	// CSM shadow parameters
	XMFLOAT4 vShadowParams;      // x = opacity (0-120), y = reflection clip Z (0 = off), z = unused, w = unused
	XMFLOAT4 vCascadeSplits;     // x,y,z,w = view-space depth thresholds for cascades 0-3
	XMMATRIX matShadowBig;       // Cascade 3 (far) — kept for backward compat naming in HLSL
	XMMATRIX matShadowLocal;     // Cascade 0 (near)
	XMMATRIX matShadowMid;       // Cascade 1
	XMMATRIX matShadowFar;       // Cascade 2
};

// Per-object constants - updated per draw call
__declspec(align(16)) struct CBPerObject
{
	XMMATRIX matWorld;
	XMMATRIX matWorldViewProj;
	XMMATRIX matTexture0;      // Texture transform matrix 0 (terrain color)
	XMMATRIX matTexture1;      // Texture transform matrix 1 (terrain splat/alpha)
	XMFLOAT4 vDiffuseColor;    // Material/vertex color multiplier (from SetDiffuseColor)
	XMFLOAT4 vSkyTint;         // sky gradient tint (SHADER_SKY)
	XMFLOAT4 vMaterialParams;  // x = alphaRef, y = alphaTestEnabled, z = specularPower, w = twoTextureBlend
	XMFLOAT4 vEmissiveColor;   // Material emissive color
	XMFLOAT4 vSpecularColor;   // Material specular color
	XMFLOAT4 vPBRParams;       // z = specular scale, w = specular power
	XMFLOAT4 vRenderFlags;     // x = character shadow depth pass, y = mesh texture alpha enable
	XMFLOAT4 vParticleColor;   // per-draw particle colour (SHADER_PARTICLE)
	XMFLOAT4 vParticleParams;  // x = EParticleColorOp, yzw unused
};

// SpeedTree constants - wind matrices and tree data
static const int SPEEDTREE_NUM_WIND_MATRICES = 4;
static const int SPEEDTREE_MAX_LEAF_TABLES = 48;

__declspec(align(16)) struct CBSpeedTree
{
	XMMATRIX matWindMatrices[SPEEDTREE_NUM_WIND_MATRICES];  // Wind rotation matrices (register 54-69)
	XMFLOAT4 vTreePos;           // Tree position (register 52)
	XMFLOAT4 vLeafTables[SPEEDTREE_MAX_LEAF_TABLES];  // Leaf billboard tables (register 4-51)
	XMFLOAT4 vLeafLightingAdj;   // Leaf lighting adjustment (register 70)
	XMFLOAT4 vLightDir;          // Light direction (register 71)
	XMFLOAT4 vLightDiffuse;      // Light diffuse (register 72)
	XMFLOAT4 vLightAmbient;      // Light ambient (register 73)
	XMFLOAT4 vMaterialDiffuse;   // Material diffuse (register 74)
	XMFLOAT4 vMaterialAmbient;   // Material ambient (register 75)
	XMFLOAT4 vFogParams;         // Fog parameters (register 85)
	int nNumLeafTables;          // Number of active leaf tables
	int _pad[3];
};

static const int MAX_BONES = 256;

__declspec(align(16)) struct CBSkinning
{
	XMMATRIX boneMatrices[MAX_BONES];  // Bone transformation matrices
};

// God Rays (Volumetric Light Scattering) constants
__declspec(align(16)) struct CBGodRays
{
	XMFLOAT4 vLightScreenPos;  // xy = light position in screen UV space, z = intensity, w = decay
	XMFLOAT4 vRayParams;       // x = density, y = weight, z = exposure, w = numSamples
	XMFLOAT4 vRayColor;        // rgb = light color, a = unused
};

// Bloom post-process constants
#ifdef ENABLE_BLOOM
__declspec(align(16)) struct CBBloom
{
	XMFLOAT4 vBloomParams;    // x=threshold, y=intensity
	XMFLOAT4 vTexelSize;      // x=1/bloomW, y=1/bloomH
	XMFLOAT4 vBlurDirection;  // x=H component, y=V component
};
#endif

// SSAO (Screen-Space Ambient Occlusion) constants
#ifdef ENABLE_SSAO
static const int SSAO_KERNEL_SIZE = 16;
__declspec(align(16)) struct CBSSAO
{
	XMMATRIX matProjection;               // For projecting samples to screen
	XMMATRIX matInvProjection;            // For unprojecting depth to view-space
	XMFLOAT4 vSSAOParams;                // x=radius, y=bias, z=intensity, w=unused
	XMFLOAT4 vTexelSize;                 // x=1/ssaoW, y=1/ssaoH, z=1/fullW, w=1/fullH
	XMFLOAT4 vSampleKernel[SSAO_KERNEL_SIZE]; // Hemisphere sample offsets
};
#endif

// Water reflection/refraction constants

static const int SKY_GRADIENT_MAX_POINTS = 8;
__declspec(align(16)) struct CBSkyGradient
{
	XMFLOAT4 colors[SKY_GRADIENT_MAX_POINTS]; // Current blended gradient control points (RGBA)
	int      colorCount;                       // Number of active control points
	int      upperSegments;                    // Number of segments in upper half (above horizon)
	float    _pad[2];
};

// Particle Compute Shader constants
__declspec(align(16)) struct CBParticleCS
{
	XMFLOAT3 camUp;       float _pad0;   // Camera up vector
	XMFLOAT3 camCross;    float _pad1;   // Camera right (cross) vector
	XMFLOAT3 camView;     float _pad2;   // Camera view (forward) vector
	XMMATRIX attachMatrix;                // Attach-to-parent matrix (64 bytes)
	UINT     particleCount;               // Number of particles in this dispatch
	UINT     facesPerParticle;            // 1, 2, or 3
	UINT     hasAttachMatrix;             // 0 or 1
	UINT     _pad3;
	XMFLOAT4 faceRotations;              // x,y,z = face rotation angles in radians
};

struct ParticleGPUInput
{
	float posX, posY, posZ;                // 12 bytes - current position
	float lastPosX, lastPosY, lastPosZ;    // 12 bytes - previous position (for stretch)
	float halfW, halfH;                    // 8 bytes  - half-size
	float scaleX, scaleY;                  // 8 bytes  - scale
	float rotation;                        // 4 bytes  - rotation in radians
	UINT  color;                           // 4 bytes  - DWORD ARGB color
	UINT  flags;                           // 4 bytes  - billboard type (bits 0-3), stretch (bit 4), attach (bit 5)
	float _pad[3];                         // 12 bytes - pad to 64-byte alignment
};

struct FlyTraceSegmentInput
{
	float pos1X, pos1Y, pos1Z;  // 12 bytes - segment start
	float size1;                 // 4 bytes  - cross-section size at start
	float pos2X, pos2Y, pos2Z;  // 12 bytes - segment end
	float size2;                 // 4 bytes  - cross-section size at end
	UINT  color;                 // 4 bytes  - ARGB
	UINT  _pad[3];               // 12 bytes - pad to 48
};

// Fly Trace Compute Shader constants
__declspec(align(16)) struct CBFlyTraceCS
{
	XMFLOAT3 camEye;     float _pad0;    // 16 bytes
	XMFLOAT3 camFwd;     float _pad1;    // 16 bytes
	UINT segmentCount;   UINT _pad2[3];  // 16 bytes
};

struct WeaponTraceSplineSegment
{
	XMFLOAT3 a; float timeStart;    // 16 bytes — cubic coefficient a + segment start time
	XMFLOAT3 b; float timeEnd;      // 16 bytes — cubic coefficient b + segment end time
	XMFLOAT3 c; float _pad0;        // 16 bytes — cubic coefficient c
	XMFLOAT3 d; float _pad1;        // 16 bytes — cubic coefficient d
};  // 64 bytes total

// Weapon Trace Compute Shader constants
__declspec(align(16)) struct CBWeaponTraceCS
{
	UINT numSegments;       // Number of spline segments per spline
	UINT numSamples;        // Number of output sample points
	float lifetime;         // Trace lifetime for alpha fade
	float samplingTime;     // Time step between samples
	float firstPointTime;   // Input[0].first for alpha calculation
	float totalLength;      // Sampling range = min(lifetime, back().first)
	UINT _pad[2];           // Pad to 32 bytes (16-byte aligned)
};

//////////////////////////////////////////////////////////////////////////
// Pending Render State
//////////////////////////////////////////////////////////////////////////
struct PendingRenderState
{
	// Blend state
	bool bAlphaBlendEnable;
	D3D11_BLEND srcBlend, destBlend;
	D3D11_BLEND_OP blendOp;
	D3D11_BLEND srcBlendAlpha, destBlendAlpha;
	D3D11_BLEND_OP blendOpAlpha;
	UINT8 colorWriteMask;

	// Rasterizer state
	D3D11_FILL_MODE fillMode;
	D3D11_CULL_MODE cullMode;
	bool bScissorEnable, bMultisampleEnable, bAntialiasedLineEnable;
	INT depthBias;
	float depthBiasClamp, slopeScaledDepthBias;
	bool bDepthClipEnable;

	// Depth stencil state
	bool bDepthEnable, bDepthWriteEnable;
	D3D11_COMPARISON_FUNC depthFunc;
	bool bStencilEnable;
	UINT8 stencilReadMask, stencilWriteMask;

	void SetDefaults();
};

//////////////////////////////////////////////////////////////////////////
// Sampler Slot State
//////////////////////////////////////////////////////////////////////////
struct SamplerSlotState
{
	DWORD minFilter, magFilter, mipFilter;
	DWORD addressU, addressV, addressW;
	DWORD borderColor;
	float mipLodBias;
	DWORD maxAnisotropy;
	bool dirty;
};

static const UINT CB_RING_SLOTS_PEROBJECT = 256;
static const UINT CB_RING_SLOTS_SKINNING = 64;
inline UINT CBRingAlign256(UINT n) { return (n + 255u) & ~255u; }

//////////////////////////////////////////////////////////////////////////
// Shader Manager Class
//////////////////////////////////////////////////////////////////////////

class CShaderManager : public CSingleton<CShaderManager>
{
public:
	CShaderManager();
	~CShaderManager();

	// Initialization
	bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	void Shutdown();
	void SetDefaultState();  // Reset render states to defaults (after resize, etc.)
	bool IsInitialized() const { return m_bInitialized; }

	// Dynamic buffer size constants
	static const DWORD        DYNAMIC_VB_SIZE = 131072;  // 128KB for vertex data
	static const DWORD        DYNAMIC_IB_SIZE = 65536;   // 64KB for index data

	void BeginUI();           // For UI elements (screen-space)
	void BeginMesh();         // For 3D models with lighting (single texture)
	void BeginMesh2Tex();     // For 3D models with lighting (two textures)
	void BeginTerrain();      // For terrain patches
	void BeginWater();        // For water surfaces
	void BeginSky();          // For skybox
	void BeginParticle();     // For particle effects
	void BeginShadow();       // For shadow map rendering
	void BeginShadowSkinned(); // For GPU skinned shadow map rendering
	void BeginSpeedTree();    // For SpeedTree branches/fronds
	void BeginSpeedTreeLeaf(); // For SpeedTree leaves with GPU placement
	void BeginMeshNormal();   // For normal mapped meshes
	void BeginMeshSkinned();  // For GPU skinned meshes
	void BeginGodRays();      // For volumetric light rays post-process
	void BeginMeshVTF();      // For VTF batched mesh rendering (instanced)
	void BeginShadowVTF();    // For VTF batched shadow rendering (instanced)
	void BeginSpeedTreeVTF(); // For VTF batched SpeedTree rendering (instanced)
	void BeginMesh2TexVTF();  // For VTF batched two-texture mesh rendering (instanced dungeon blocks)
	void BeginParticlePCT();  // For particle effects with per-vertex color (CS output)
	void End();               // Unbind current shader


	//--------------------------------------------------------------------
	// Sky Gradient - Per-pixel gradient via constant buffer
	//--------------------------------------------------------------------
	void SetSkyGradient(const float* pColors, int count, int upperSegments); // pColors = count * 4 floats (RGBA)

	//--------------------------------------------------------------------
	// GPU Skinning - Bone Matrix Upload
	//--------------------------------------------------------------------
	void SetBoneMatrices(const Matrix* pMatrices, int count);

	EShaderType GetCurrentShader() const { return m_eCurrentShader; }

	//--------------------------------------------------------------------
	// Multithreaded Rendering Support (Deferred Contexts)
	//--------------------------------------------------------------------
	// Thread-local context override for worker threads
	ID3D11DeviceContext* GetActiveContext() const {
		return m_pContext;
	}

	void SyncPerFrameToContext(ID3D11DeviceContext* pDeferredCtx, ID3D11Buffer* pCBPerFrame);

	void SyncLightingToContext(ID3D11DeviceContext* pDeferredCtx, ID3D11Buffer* pCBLighting);

	void SyncAllConstantBuffers(ID3D11DeviceContext* pDeferredCtx,
		ID3D11Buffer* pCBPerFrame, ID3D11Buffer* pCBPerObject,
		ID3D11Buffer* pCBLighting, ID3D11Buffer* pCBSkinning);

	void BindShaderToContext(ID3D11DeviceContext* pDeferredCtx, EShaderType type);

	// Update per-object constant buffer on a deferred context
	void UpdatePerObjectOnContext(ID3D11DeviceContext* pDeferredCtx, ID3D11Buffer* pCBPerObject,
		const Matrix* pWorld, const XMFLOAT4* pDiffuseColor = nullptr);

	// Update skinning constant buffer on a deferred context
	void UpdateSkinningOnContext(ID3D11DeviceContext* pDeferredCtx, ID3D11Buffer* pCBSkinning,
		const Matrix* pBoneMatrices, int boneCount);

	const CBPerFrame& GetPerFrameData() const { return m_cbPerFrame; }
	const CBLighting& GetLightingData() const { return m_cbLighting; }
	const CBPerObject& GetPerObjectData() const { return m_cbPerObject; }

	// Get shadow SRVs for re-binding on deferred contexts
	ID3D11ShaderResourceView* GetShadowTextureBig() const { return m_pTextures[2]; }
	ID3D11ShaderResourceView* GetShadowTextureLocal() const { return m_pTextures[3]; }
	ID3D11ShaderResourceView* GetShadowTextureMid() const { return m_pTextures[4]; }
	ID3D11ShaderResourceView* GetShadowTextureFar() const { return m_pTextures[5]; }

	// Get render state copy for thread-local initialization
	const PendingRenderState& GetRenderStateCopy() const { return m_RenderState; }

	void CopyTransforms(Matrix* pDest, int maxCount) const {
		int count = (maxCount < (int)MAX_TRANSFORMS) ? maxCount : (int)MAX_TRANSFORMS;
		memcpy(pDest, m_Matrices, count * sizeof(Matrix));
	}

	// Copy sampler states for thread-local use
	void CopySamplerStates(SamplerSlotState* pDest, int maxSlots) const {
		int count = (maxSlots < (int)MAX_SAMPLER_SLOTS) ? maxSlots : (int)MAX_SAMPLER_SLOTS;
		memcpy(pDest, m_SamplerStates, count * sizeof(SamplerSlotState));
	}

	// Get current input layout type
	EInputLayoutType GetCurrentInputLayout() const { return m_CurrentInputLayout; }

	//--------------------------------------------------------------------
	// Input Layout Binding (Native DX11)
	//--------------------------------------------------------------------
	void BindForInputLayout(EInputLayoutType type);

	//--------------------------------------------------------------------
	// Per-Frame Constant Updates (call once per frame)
	//--------------------------------------------------------------------
	void SetViewProjection(const Matrix* pView, const Matrix* pProj);
	void GetProjectionMatrix(Matrix* pProj) const;
	void SetCameraPosition(const Vector3* pCameraPos);
	void SetViewportSize(float width, float height);
	void SetFog(bool bEnabled, float fStart, float fEnd, DWORD dwColor);
	void SetLight(const Vector3* pDirection, const Color* pColor, float fIntensity);
	void SetAmbient(const Color* pColor);
	void SetTime(float fTotalTime, float fDeltaTime);
	void SetSunDirection(float x, float y, float z, float intensity = 1.0f);
	void SetLightingEnabled(bool bEnabled);

	// Shadow system (4-cascade CSM)
	void SetShadowOpacity(float fOpacity);  // 0-120 range
	void SetShadowTexelSize(float fTexelSize);   // 1.0 / shadow map resolution

	void SetShadowCullPlanes(const float* pafPlanes4x4);   // NULL disables culling
	bool IsInShadowCull(float x, float y, float z, float fRadius) const;
	bool IsShadowCullActive() const { return m_bShadowCullActive; }
	void SetReflectionClipZ(float fWaterZ);
	void SetShadowMatrices(const Matrix* pBig, const Matrix* pLocal);
	void SetShadowMidFarMatrices(const Matrix* pMid, const Matrix* pFar);
	void SetCascadeSplits(float s0, float s1, float s2, float s3);
	void SetShadowTextures(ID3D11ShaderResourceView* pBig, ID3D11ShaderResourceView* pLocal);
	void SetShadowMidFarTextures(ID3D11ShaderResourceView* pMid, ID3D11ShaderResourceView* pFar);

	// God Rays (Volumetric Light Scattering)
	void SetGodRaysParams(float fScreenX, float fScreenY, float fIntensity, float fDecay);
	void SetGodRaysRayParams(float fDensity, float fWeight, float fExposure, int nSamples);
	void SetGodRaysColor(float r, float g, float b);
	bool IsGodRaysEnabled() const { return m_bGodRaysEnabled; }
	void SetGodRaysEnabled(bool bEnabled) { m_bGodRaysEnabled = bEnabled; }
#ifdef ENABLE_GODRAYS
	void RenderGodRaysPass(
		ID3D11ShaderResourceView* pSceneSRV,
		ID3D11RenderTargetView* pGodRaysRTV,
		UINT w, UINT h);
#endif

#ifdef ENABLE_BLOOM
	// Bloom Post-Process
	void SetBloomEnabled(bool bEnabled);
	bool IsBloomEnabled() const { return m_bBloomEnabled; }
	void SetBloomParams(float threshold, float intensity);
	void BeginBloomBright();
	void BeginBloomBlur();
	void BeginBloomComposite();
	void RenderBloom(
		ID3D11ShaderResourceView* pSceneSRV,
		ID3D11RenderTargetView* pBloomRTA_RTV, ID3D11ShaderResourceView* pBloomRTA_SRV,
		ID3D11RenderTargetView* pBloomRTB_RTV, ID3D11ShaderResourceView* pBloomRTB_SRV,
		UINT bloomW, UINT bloomH,
		ID3D11ShaderResourceView* pGodRaysSRV,
		ID3D11ShaderResourceView* pSSAO_SRV,
		ID3D11RenderTargetView* pOutputRTV, UINT outputW, UINT outputH);
#endif

#ifdef ENABLE_SSAO
	// SSAO Post-Process
	void SetSSAOEnabled(bool bEnabled);
	bool IsSSAOEnabled() const { return CGraphicBase::GetSSAOEnabled(); }
	void RenderDepthResolve(
		ID3D11ShaderResourceView* pMSAADepthSRV,
		ID3D11RenderTargetView* pResolvedRTV,
		UINT w, UINT h);
	void RenderSSAOPass(
		ID3D11ShaderResourceView* pDepthSRV,
		ID3D11RenderTargetView* pSSAO_RTV,
		UINT ssaoW, UINT ssaoH, UINT fullW, UINT fullH);
	void RenderSSAOBlur(
		ID3D11ShaderResourceView* pSSAO_SRV,
		ID3D11ShaderResourceView* pDepthSRV,
		ID3D11RenderTargetView* pBlurRTV,
		UINT w, UINT h);
#endif

	//--------------------------------------------------------------------
	// Multi-Light Support (Native DX11)
	//--------------------------------------------------------------------
	void SetLight(UINT index, const DX11Light& light);
	void GetLight(UINT index, DX11Light* pLight) const;
	void GetLight(UINT index, TLight* pLight) const;  // Conversion overload for legacy code
	bool IsLightEnabled(UINT index) const;
	void EnableLight(UINT index, bool bEnable);
	void SetGlobalAmbient(const XMFLOAT4& color);
	void SetGlobalAmbient(float r, float g, float b, float a = 1.0f);

	//--------------------------------------------------------------------
	// Per-Object Constant Updates (call before each draw)
	//--------------------------------------------------------------------
	void SetWorldMatrix(const Matrix* pWorld);
	void SetDiffuseColor(float r, float g, float b, float a);
	void SetAlphaTest(bool bEnabled, float fRef);

	// Material properties
	void SetMaterial(float fSpecularPower);
	void SetSpecularColor(float r, float g, float b);
	void SetSpecularTune(float fIntensity, float fPower);
	void SetSpecularPower(float power);
	void SetEmissiveColor(float r, float g, float b);
	void SetTextureColorSwap(bool bEnabled);  // Enable R/G channel swap for BC textures in particle shader

	void SetTwoTextureBlend(bool bEnabled);
	bool IsTwoTextureBlendEnabled() const;

	void SetParticleColorOp(BYTE byColorOp);

	void SetMaterialParams(float x, float y, float z, float w);

	// Texture transform matrices (for terrain splatting)
	void SetTextureMatrix(int slot, const Matrix* pMatrix);

	// Texture Factor (for color modulation)
	void SetSkyTint(DWORD dwColor);
	void SetParticleColor(DWORD dwColor);
	void SetMeshTextureAlphaEnabled(bool bEnabled);
	void SetCharacterShadowPass(bool bEnabled);

	// Commit pending constant buffer updates
	void CommitChanges();

	void __CommitCBRing(ID3D11DeviceContext* pCtx, ID3D11DeviceContext1* pCtx1,
		ID3D11Buffer* pBuf, UINT& rOffset, UINT& rBound, UINT ringBytes,
		const void* pSrc, UINT srcBytes, int vsSlot, int psSlot,
		bool* pForceDiscard = nullptr);

	void __BindCBRing(ID3D11DeviceContext* pCtx, ID3D11DeviceContext1* pCtx1,
		ID3D11Buffer* pBuf, UINT boundOffset, UINT srcBytes,
		int vsSlot, int psSlot, bool bRing);

	//--------------------------------------------------------------------
	// Texture Binding
	//--------------------------------------------------------------------
	void SetShaderResource(UINT slot, ID3D11ShaderResourceView* pSRV);
	void SetDefaultTexture(UINT slot = 0);  // Explicitly bind white default texture (for UI backgrounds)
	void OnFrameComplete();  // Call after each frame to track frame count
	int GetFrameCount() const { return m_iFrameCount; }
	ID3D11ShaderResourceView* GetDefaultTexture() const { return m_pActiveDefaultTextureSRV; }  // Returns active (transparent during init, then white)

	//--------------------------------------------------------------------
	// Input Layout Access
	//--------------------------------------------------------------------
	ID3D11InputLayout* GetInputLayout(EShaderType type) const;

	//--------------------------------------------------------------------
	// Render State Management (from StateManager)
	//--------------------------------------------------------------------
	void SetPipelineState(EPipelineState state, DWORD value);
	DWORD GetPipelineState(EPipelineState state);
	void SavePipelineState(EPipelineState state, DWORD value);
	void RestorePipelineState(EPipelineState state);

	// Stream/Buffer binding
	void SetVertexBuffer(UINT stream, ID3D11Buffer* pBuffer, UINT stride, UINT offset = 0);
	void SetIndexBuffer(ID3D11Buffer* pBuffer, DXGI_FORMAT format = DXGI_FORMAT_R16_UINT, UINT offset = 0);

	void SetPrimitiveTopologyIfChanged(D3D11_PRIMITIVE_TOPOLOGY topology);

	void InvalidateIACache();

	// Drawing
	void Draw(EPrimitiveTopology type, UINT startVertex, UINT primitiveCount);
	void DrawIndexed(EPrimitiveTopology type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primitiveCount, INT baseVertex);
	void DrawIndexed(EPrimitiveTopology type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primitiveCount);  // 5-arg overload (baseVertex=0)
	void DrawDynamic(EPrimitiveTopology type, UINT primitiveCount, const void* pVertexData, UINT stride);
	void DrawIndexedDynamic(EPrimitiveTopology type, UINT minIndex, UINT numVertices, UINT primitiveCount, const void* pIndexData, DXGI_FORMAT indexFormat, const void* pVertexData, UINT stride);
	void DrawIndexedInstanced(EPrimitiveTopology type, UINT indexCountPerInstance, UINT instanceCount, UINT startIndex = 0, INT baseVertex = 0, UINT startInstance = 0);
	void DrawInstanced(EPrimitiveTopology type, UINT vertexCountPerInstance, UINT instanceCount, UINT startVertex = 0, UINT startInstance = 0);
	void ResetDynamicBuffers();  // Call at frame start to reset ring buffer positions

	//--------------------------------------------------------------------
	// Batched Rendering Support (particles, etc.)
	//--------------------------------------------------------------------
	struct MappedDynamicVB
	{
		void* pData;          // Mapped pointer (at usable region start)
		UINT  byteOffset;     // Byte offset within ring buffer
		UINT  maxBytes;       // Maximum bytes available from pData
	};

	bool MapDynamicVB(UINT requiredBytes, MappedDynamicVB& outMapped);
	void UnmapDynamicVB();
	void AdvanceDynamicVBOffset(UINT bytesUsed);
	void DrawBatchedQuads(UINT stride, UINT vbByteOffset, UINT firstQuad, UINT quadCount);

	//--------------------------------------------------------------------
	// GPU Compute Shader Support
	//--------------------------------------------------------------------
	bool CompileComputeShader(EComputeShader type, const char* szCSFile, const char* szEntryPoint = "CSMain");
	void DispatchCompute(EComputeShader type, UINT groupsX, UINT groupsY = 1, UINT groupsZ = 1);
	bool CreateStructuredBuffer(UINT elementSize, UINT elementCount, bool bCpuWrite, GpuBuffer& outBuffer);
	bool CreateRawVertexUAVBuffer(UINT byteWidth, GpuBuffer& outBuffer);
	void ReleaseGpuBuffer(GpuBuffer& buffer);
	void CSSetSRV(UINT slot, ID3D11ShaderResourceView* pSRV);
	void CSSetUAV(UINT slot, ID3D11UnorderedAccessView* pUAV);
	void CSSetCB(UINT slot, ID3D11Buffer* pCB);
	void CSUnbindResources();
	ID3D11Device* GetDevice() const { return m_pDevice; }

	//--------------------------------------------------------------------
	// Particle Compute Shader Billboard System
	//--------------------------------------------------------------------
	static const UINT MAX_CS_PARTICLES = 4096;     // Max particles per dispatch
	static const UINT MAX_CS_QUADS = MAX_CS_PARTICLES * 3;  // Max quads (3FACE worst case)

	bool InitParticleCSResources();
	bool DispatchParticleBillboardCS(const ParticleGPUInput* pParticles, UINT count,
		UINT facesPerParticle, const float fRotations[3], const Matrix* pAttachMatrix);
	void DrawParticleCSOutput(UINT quadCount);
	bool IsComputeParticlesAvailable() const { return m_bComputeParticlesAvailable; }

	struct ParticleBatchKey
	{
		ID3D11ShaderResourceView* pTexture;
		UINT                      srcBlend;
		UINT                      destBlend;
		BYTE                      colorOp;
		BYTE                      facesPerParticle;
		BYTE                      stretchFlag;
		BYTE                      _pad;
		float                     rot0, rot1, rot2;

		bool operator==(const ParticleBatchKey& o) const
		{
			return pTexture == o.pTexture && srcBlend == o.srcBlend && destBlend == o.destBlend
				&& colorOp == o.colorOp && facesPerParticle == o.facesPerParticle
				&& stretchFlag == o.stretchFlag
				&& rot0 == o.rot0 && rot1 == o.rot1 && rot2 == o.rot2;
		}
	};
	struct ParticleBatchKeyHash
	{
		size_t operator()(const ParticleBatchKey& k) const
		{
			size_t h = std::hash<void*>()(k.pTexture);
			h ^= (size_t)k.srcBlend + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= (size_t)k.destBlend + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= (size_t)k.colorOp + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= (size_t)k.facesPerParticle + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= (size_t)k.stretchFlag + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};

	void ResetParticleBatcher();
	bool IsParticleBatchingActive() const { return m_bParticleBatchingActive; }
	void SetParticleBatchingActive(bool b) { m_bParticleBatchingActive = b; }
	void AddParticleToBatch(const ParticleBatchKey& key, const ParticleGPUInput& input);
	void FlushParticleBatches();

	struct ParticleBatchStats
	{
		UINT psisContributed;
		UINT particlesBatched;
		UINT batchGroups;
		UINT dispatches;
		UINT drawsIssued;
		UINT meshElemsDrawn;  // effect mesh elements rendered (per-frame)
	};

	void StatsNoteMeshElem() { m_particleBatchStats.meshElemsDrawn++; }
	const ParticleBatchStats& GetParticleBatchStats() const { return m_particleBatchStats; }
	void ResetParticleBatchStats() { memset(&m_particleBatchStats, 0, sizeof(m_particleBatchStats)); }
	void StatsNotePSIContribution() { m_particleBatchStats.psisContributed++; }

	UINT GetGlobalDrawCount() const { return m_globalDrawCount; }
	void ResetGlobalDrawCount() { m_globalDrawCount = 0; }
	void IncrementGlobalDrawCount()
	{
		++m_globalDrawCount;
		++t_subsystemDrawCount;
	}

	static void ResetSubsystemDrawCount() { t_subsystemDrawCount = 0; }
	static UINT GetSubsystemDrawCount() { return t_subsystemDrawCount; }

	void SnapshotPhase0_AfterShadow() { m_drawPhase0 = m_globalDrawCount; }
	void SnapshotPhase1_AfterWorkers() { m_drawPhase1 = m_globalDrawCount; }
	void SnapshotPhase2_AfterWater() { m_drawPhase2 = m_globalDrawCount; }
	UINT GetPhaseDraws_Shadow()  const { return m_drawPhase0; }
	UINT GetPhaseDraws_Workers() const { return m_drawPhase1 >= m_drawPhase0 ? m_drawPhase1 - m_drawPhase0 : 0; }
	UINT GetPhaseDraws_Water()   const { return m_drawPhase2 >= m_drawPhase1 ? m_drawPhase2 - m_drawPhase1 : 0; }
	UINT GetPhaseDraws_AfterWater() const { return m_globalDrawCount >= m_drawPhase2 ? m_globalDrawCount - m_drawPhase2 : 0; }

	UINT terrainJobDraws = 0;
	UINT characterJobDraws = 0;
	UINT effectJobDraws = 0;
	void StoreTerrainJobDraws(UINT n) { terrainJobDraws = n; }
	void StoreCharacterJobDraws(UINT n) { characterJobDraws = n; }
	void StoreEffectJobDraws(UINT n) { effectJobDraws = n; }

	double cpuMs_Frame = 0.0;
	double cpuMs_Deform = 0.0;
	double cpuMs_Shadow = 0.0;
	double cpuMs_WorkerWait = 0.0;

	double cpuMs_Sky = 0.0;   // RenderSky .. RenderCloud
	double cpuMs_Terrain = 0.0;   // m_pyBackground.Render()
	double cpuMs_Char = 0.0;   // m_kChrMgr.Render()  (main pass only)
	double cpuMs_Water = 0.0;   // refraction copy + reflection pass + RenderWater
	double cpuMs_Effects = 0.0;   // RenderEffect + m_kEftMgr.Render()
	double cpuMs_Misc = 0.0;   // items, flying, PC blocker, lens flare, state resets

	//--------------------------------------------------------------------
	// Fly Trace Compute Shader Billboard System
	//--------------------------------------------------------------------
	static const UINT MAX_FLYTRACE_SEGMENTS = 1024;  // Max segments per dispatch

	bool InitFlyTraceCSResources();
	bool DispatchFlyTraceCS(const FlyTraceSegmentInput* pSegments, UINT count);
	void DrawFlyTraceCSOutput(UINT segmentCount);
	bool IsFlyTraceCSAvailable() const { return m_bFlyTraceCSAvailable; }

	//--------------------------------------------------------------------
	// Weapon Trace Compute Shader Spline System
	//--------------------------------------------------------------------
	static const UINT MAX_WEAPONTRACE_SEGMENTS = 300;  // Max spline segments per spline
	static const UINT MAX_WEAPONTRACE_SAMPLES = 1024;  // Max output samples

	bool InitWeaponTraceCSResources();
	bool DispatchWeaponTraceCS(const WeaponTraceSplineSegment* pSegments, UINT numSegments, const CBWeaponTraceCS& params);
	void DrawWeaponTraceCSOutput(UINT numSamples);
	bool IsWeaponTraceCSAvailable() const { return m_bWeaponTraceCSAvailable; }

	// Transform management
	void SetMatrix(EMatrixSlot state, const Matrix* pMatrix);
	void GetMatrix(EMatrixSlot state, Matrix* pMatrix);
	void SaveTransform(EMatrixSlot state, const Matrix* pMatrix);
	void RestoreTransform(EMatrixSlot state);

	// Input layout by type
	void SetInputLayout(EInputLayoutType type);
	void SetInputLayout(ID3D11InputLayout* pLayout);  // Direct binding overload
	void SaveInputLayout(EInputLayoutType type);
	void RestoreInputLayout();

	// Sampler state management
	void SetSamplerState(UINT slot, ESamplerState state, DWORD value);
	void SaveSamplerState(UINT slot, ESamplerState state, DWORD value);
	void RestoreSamplerState(UINT slot, ESamplerState state);

	bool GetLightingEnabled() const { return m_bLightingEnabled; }

	// Fog state
	void SetFogEnabled(bool bEnabled);
	bool GetFogEnabled() const { return m_bFogEnabled; }
	void SetFogColor(DWORD dwColor);
	void SetFogParams(float fStart, float fEnd, DWORD dwColor);
	// Volumetric-fog dials, stored in spare CBPerFrame fields so no layout change is needed:
	//   vFogParams.x = start distance (world units)   vFogColor.a = density multiplier (0..4)
	// The dial VALUES live here, not only in the CB: MapManager calls SetFogParams()/SetFogColor()
	// every frame from the env data, which was overwriting both CB fields and made the sliders
	// look dead. ReapplyVolFogDials() restores them after any such write.
	void  SetVolFogDensity(float d) { m_volFogDensity = (d < 0.0f) ? 0.0f : (d > 4.0f ? 4.0f : d); ReapplyVolFogDials(); }
	void  SetVolFogStart(float v)   { m_volFogStart = (v < 0.0f) ? 0.0f : v; ReapplyVolFogDials(); }
	float GetVolFogDensity() const  { return m_volFogDensity; }
	float GetVolFogStart() const    { return m_volFogStart; }
	void  ReapplyVolFogDials()
	{
		m_cbPerFrame.vFogColor.w  = m_volFogDensity;
		m_cbPerFrame.vFogParams.x = m_volFogStart;
		m_bPerFrameDirty = true;
	}
	float m_volFogDensity = 1.0f;
	float m_volFogStart   = 0.0f;

	// Texture factor — must read thread-local on workers (setter writes thread-local)
	DWORD GetSkyTint() const { return m_dwSkyTint; }
	DWORD GetParticleColor() const { return m_dwParticleColor; }

	// Best filtering helper
	void SetBestFiltering(UINT slot);

	// Alpha test state — must read thread-local on workers (setter writes thread-local)
	void SetAlphaTestEnabled(bool bEnabled);
	bool GetAlphaTestEnabled() const {
		return m_bAlphaTestEnabled;
	}
	void SetAlphaTestRefByte(DWORD dwRef);
	DWORD GetAlphaTestRef() const {
		return m_dwAlphaTestRef;
	}

	// Legacy material API (TMaterial)
	void SetMaterial(const TMaterial* pMaterial);
	void GetMaterial(TMaterial* pMaterial) const;
	void SaveMaterial();
	void RestoreMaterial();

	// Legacy light API (TLight)
	void SetLight(UINT index, const TLight* pLight);
	void LightEnable(UINT index, BOOL bEnable);

	void SetSpeedTreeWindMatrix(int nIndex, const float* pMatrix16);
	void SetSpeedTreeTreePosition(const float* pPos4);
	void SetSpeedTreeLeafTables(int nFirstEntry, const float* pTables, UINT uiEntryCount);
	void SetSpeedTreeLeafLightingAdjustment(const float* pAdj4);
	void SetSpeedTreeLight(const float* pLight12);
	void SetSpeedTreeMaterial(const float* pMaterial8);
	void SetSpeedTreeFogParams(const float* pFog4);
	void SetSpeedTreeCompoundMatrix(const float* pMatrix16);

	// Commit all pending state changes
	void CommitRenderState();

private:
	// Shader compilation with caching
	bool CompileAllShaders();
	bool CompileShader(EShaderType type, const char* szVSFile, const char* szPSFile);

	// Shader cache helpers
	static UINT ComputeShaderHash(const void* pVSData, size_t vsSize, const void* pPSData, size_t psSize);
	bool LoadShaderFromCache(EShaderType type, UINT hash, ID3DBlob** ppVSBlob, ID3DBlob** ppPSBlob);
	bool SaveShaderToCache(EShaderType type, UINT hash, ID3DBlob* pVSBlob, ID3DBlob* pPSBlob);
	static const char* GetShaderCachePath();

	// Resource creation
	bool CreateConstantBuffers();
	bool CreateDefaultTexture();
	bool CreateSamplerStates();

	// Internal binding
	void BindShader(EShaderType type);

private:

	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pContext;
	ID3D11DeviceContext1* m_pContext1 = nullptr;
	bool                    m_bCBRingSupported = false;
	UINT                    m_cbPerObjectOffset = 0;
	UINT                    m_cbSkinningOffset = 0;
	UINT                    m_cbPerObjectBound = 0;
	UINT                    m_cbSkinningBound = 0;
	bool                    m_bInitialized;
	int                     m_iFrameCount;  // Frame counter for startup - use transparent texture for first few frames

	// Shader resources per type
	struct ShaderProgram
	{
		ID3D11VertexShader* pVertexShader;
		ID3D11PixelShader* pPixelShader;
		ID3D11InputLayout* pInputLayout;
		ID3DBlob* pVSBlob;

		ShaderProgram() : pVertexShader(nullptr), pPixelShader(nullptr),
			pInputLayout(nullptr), pVSBlob(nullptr) {
		}
	};

	ShaderProgram m_Shaders[SHADER_COUNT]{};
	EShaderType   m_eCurrentShader;

	// Constant buffers
	ComPtr<ID3D11Buffer> m_pCBPerFrame = nullptr;
	ComPtr<ID3D11Buffer> m_pCBPerObject = nullptr;
	ComPtr<ID3D11Buffer> m_pCBLighting = nullptr;
	ComPtr<ID3D11Buffer> m_pCBSpeedTree = nullptr;
	ComPtr<ID3D11Buffer> m_pCBSkinning = nullptr;
	ComPtr<ID3D11Buffer> m_pCBGodRays = nullptr;

	CBPerFrame    m_cbPerFrame{};
	CBPerObject   m_cbPerObject{};
	CBLighting    m_cbLighting{};
	CBSpeedTree   m_cbSpeedTree{};
	CBSkinning    m_cbSkinning{};
	CBGodRays     m_cbGodRays{};

	ComPtr<ID3D11Buffer> m_pCBSkyGradient = nullptr;
	CBSkyGradient m_cbSkyGradient{};
	bool          m_bSkyGradientDirty;
	bool          m_bPerFrameDirty;

	float         m_afShadowCullPlane[4][4];   // 4 SIDE planes of the cascade being rendered
	bool          m_bShadowCullActive;
	bool          m_bPerObjectDirty;
	bool          m_bLightingDirty;
	bool          m_bSpeedTreeDirty;
	bool          m_bSkinningDirty;
	int           m_iActiveBoneCount;
	bool          m_bGodRaysDirty;
	bool          m_bGodRaysEnabled;
#ifdef ENABLE_BLOOM
	ID3D11Buffer* m_pCBBloom;
	CBBloom       m_cbBloom{};
	bool          m_bBloomEnabled;
#endif
#ifdef ENABLE_SSAO
	ID3D11Buffer* m_pCBSSAO;
	CBSSAO        m_cbSSAO{};
	bool          m_bSSAODirty;
	ID3D11Texture2D* m_pSSAONoiseTex;
	ID3D11ShaderResourceView* m_pSSAONoiseSRV;
#endif


	// Default resources
	ID3D11Texture2D* m_pDefaultTexture;        // White texture for UI solid color rendering
	ID3D11ShaderResourceView* m_pDefaultTextureSRV;
	ID3D11Texture2D* m_pTransparentTexture;    // Transparent texture for null fallback
	ID3D11ShaderResourceView* m_pTransparentTextureSRV;
	ID3D11ShaderResourceView* m_pActiveDefaultTextureSRV;  // Active pointer that switches between transparent/white
	ID3D11SamplerState* m_pSamplerLinear;
	ID3D11SamplerState* m_pSamplerPoint;
	ID3D11SamplerState* m_pSamplerClamp;
	ID3D11SamplerState* m_pSamplerShadowCmp;   // hardware PCF comparison sampler (s2)

	//--------------------------------------------------------------------
	// Render State Management (moved from StateManager)
	//--------------------------------------------------------------------
	CStateObjectCache* m_pStateCache;

	PendingRenderState        m_RenderState;

	// Current state objects
	ID3D11BlendState* m_pCurrentBlendState;
	ID3D11RasterizerState* m_pCurrentRasterizerState;
	ID3D11DepthStencilState* m_pCurrentDepthStencilState;

	// Dirty flags
	bool                      m_bBlendStateDirty;
	bool                      m_bRasterizerStateDirty;
	bool                      m_bDepthStencilStateDirty;

	// Saved render states (for Save/Restore pattern)
	std::unordered_map<EPipelineState, DWORD> m_SavedRenderStates;

	// Transform matrices
	static const DWORD MAX_TRANSFORMS = 300;
	Matrix                    m_Matrices[MAX_TRANSFORMS]{};
	Matrix                    m_SavedMatrices[MAX_TRANSFORMS]{};

	// Stream data
	static const DWORD MAX_STREAMS = 16;
	struct StreamData {
		ID3D11Buffer* pBuffer;
		UINT stride;
		UINT offset;
		StreamData() : pBuffer(nullptr), stride(0), offset(0) {}
	};
	StreamData                m_Streams[MAX_STREAMS]{};
	ID3D11Buffer* m_pCurrentIndexBuffer;
	DXGI_FORMAT               m_IndexFormat;
	UINT                      m_IndexOffset;

	D3D11_PRIMITIVE_TOPOLOGY  m_CurrentTopology;

	ID3D11Buffer* m_pDynamicVertexBuffer;
	ID3D11Buffer* m_pDynamicIndexBuffer;
	DWORD                     m_dwDynamicVBOffset;       // Current write position
	DWORD                     m_dwDynamicIBOffset;       // Current write position
	bool                      m_bDynamicBufferNeedsDiscard;  // True at frame start

	static const UINT         SKINNING_CB_POOL_SIZE = 256;
	ID3D11Buffer* m_pSkinningCBPool[SKINNING_CB_POOL_SIZE]{};
	UINT                      m_dwSkinningPoolIndex;

	// Cross-PSI particle batcher storage
	bool                      m_bParticleBatchingActive;
	std::unordered_map<ParticleBatchKey, std::vector<ParticleGPUInput>, ParticleBatchKeyHash> m_particleBatches;
	ParticleBatchStats        m_particleBatchStats;
	UINT                      m_globalDrawCount;
	static thread_local UINT  t_subsystemDrawCount;
	UINT                      m_drawPhase0 = 0;
	UINT                      m_drawPhase1 = 0;
	UINT                      m_drawPhase2 = 0;

	// Compute shaders
	ID3D11ComputeShader* m_ComputeShaders[CS_COUNT]{};

	// Particle CS resources
	GpuBuffer                 m_particleCSInput;      // Structured buffer (CPU write, SRV)
	GpuBuffer                 m_particleCSOutput;     // Raw VB+UAV buffer
	ID3D11Buffer* m_pCBParticleCS;        // Particle CS constant buffer
	ID3D11Buffer* m_pParticleCSIB;        // Index buffer for CS output quads
	CBParticleCS              m_cbParticleCS{};          // CPU-side CB data
	bool                      m_bComputeParticlesAvailable;

	// FlyTrace CS resources
	GpuBuffer                 m_flyTraceCSInput;      // Structured buffer (CPU write, SRV)
	GpuBuffer                 m_flyTraceCSOutput;     // Raw VB+UAV buffer
	ID3D11Buffer* m_pCBFlyTraceCS;        // FlyTrace CS constant buffer
	ID3D11Buffer* m_pFlyTraceCSIB;        // Index buffer for CS output segments
	CBFlyTraceCS              m_cbFlyTraceCS{};          // CPU-side CB data
	bool                      m_bFlyTraceCSAvailable;

	// WeaponTrace CS resources
	GpuBuffer                 m_weaponTraceCSInput;   // Structured buffer (short+long segments, CPU write, SRV)
	GpuBuffer                 m_weaponTraceCSOutput;  // Raw VB+UAV buffer (triangle strip vertices)
	ID3D11Buffer* m_pCBWeaponTraceCS;     // WeaponTrace CS constant buffer
	CBWeaponTraceCS           m_cbWeaponTraceCS{};       // CPU-side CB data
	bool                      m_bWeaponTraceCSAvailable;

	// Input layout
	EInputLayoutType          m_CurrentInputLayout;
	EInputLayoutType          m_SavedInputLayout;

	// Lighting/Fog/AlphaTest state
	bool                      m_bLightingEnabled;
	bool                      m_bFogEnabled;
	bool                      m_bAlphaTestEnabled;
	DWORD                     m_dwAlphaTestRef;

	// Saved material for Save/Restore pattern
	TMaterial                 m_CurrentMaterial{};
	TMaterial                 m_SavedMaterial{};

	// Sampler state storage
	static const DWORD MAX_SAMPLER_SLOTS = 8;
	static const DWORD STATEMANAGER_MAX_STAGES = 8;  // Max texture stages
	SamplerSlotState        m_SamplerStates[MAX_SAMPLER_SLOTS]{};
	std::unordered_map<UINT, std::unordered_map<ESamplerState, DWORD>> m_SavedSamplerStates;

	// Texture slots
	ID3D11ShaderResourceView* m_pTextures[STATEMANAGER_MAX_STAGES]{};

	// Texture factor color
	bool                      m_bTwoTextureBlend;
	DWORD                     m_dwSkyTint;
	DWORD                     m_dwParticleColor;

	// Internal state update methods
	void UpdateBlendState();
	void UpdateRasterizerState();
	void UpdateDepthStencilState();
	bool CreateDynamicBuffers();
	void ApplyRenderStates();  // Commit all pending state changes to GPU
};

#define SHADERMANAGER CShaderManager::Instance()

