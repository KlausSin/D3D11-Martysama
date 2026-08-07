#include "StdAfx.h"
#include "VolumetricFog.h"
#include "GrpBase.h"
#include <d3dcompiler.h>

// Slice mapping is exponential: near froxels are thin, far ones thick. A linear mapping
// spends almost the whole grid on distance nobody looks at and reads blocky up close.
//   depth(z) = near * pow(far/near, (z + 0.5) / gridZ)
static const char* g_szFogCommon = R"(
cbuffer CBFog : register(b0)
{
	matrix matInvViewProj;
	float4 vCameraPos;
	float4 vGridSize;
	float4 vFogNearFar;
	float4 vFogMedia;
	float4 vFogScatterColor;
};

float FogSliceToDepth(float slice)
{
	return vFogNearFar.x * pow(vFogNearFar.z, (slice + 0.5f) / vGridSize.z);
}

float FogDepthToSlice(float depth)
{
	return log(max(depth, vFogNearFar.x) / vFogNearFar.x) / vFogNearFar.w * vGridSize.z - 0.5f;
}

float3 FogFroxelWorldPos(uint3 id)
{
	float2 uv    = (float2(id.xy) + 0.5f) / vGridSize.xy;
	float  depth = FogSliceToDepth((float)id.z);

	float4 ndc  = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.5f, 1.0f);
	float4 wp   = mul(ndc, matInvViewProj);
	float3 dir  = normalize(wp.xyz / wp.w - vCameraPos.xyz);
	return vCameraPos.xyz + dir * depth;
}
)";

static const char* g_szFogInjectCS = R"(
RWTexture3D<float4> g_Media : register(u0);

[numthreads(8, 8, 4)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
	if (id.x >= (uint)vGridSize.x || id.y >= (uint)vGridSize.y || id.z >= (uint)vGridSize.z)
		return;

	float3 wp = FogFroxelWorldPos(id);

	float h       = max(wp.z - vFogMedia.z, 0.0f);
	float density = vFogMedia.x * exp(-vFogMedia.y * h);

	g_Media[id] = float4(vFogScatterColor.rgb * density, density);
}
)";

static const char* g_szFogIntegrateCS = R"(
Texture3D<float4>   g_Media      : register(t0);
RWTexture3D<float4> g_Integrated : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
	if (id.x >= (uint)vGridSize.x || id.y >= (uint)vGridSize.y)
		return;

	float3 accum        = float3(0.0f, 0.0f, 0.0f);
	float  transmittance = 1.0f;
	float  prevDepth     = 0.0f;

	uint slices = (uint)vGridSize.z;
	for (uint z = 0; z < slices; ++z)
	{
		uint3 coord = uint3(id.xy, z);

		float  depth = FogSliceToDepth((float)z);
		float  step  = max(depth - prevDepth, 0.0f);
		prevDepth    = depth;

		float4 media = g_Media[coord];

		// Start distance moves WHERE the fog begins without changing how thick it reads beyond
		// that point: nothing scatters before it, and the remaining span is renormalised so the
		// total optical depth from start to far stays constant as the dial moves.
		float fogStart = vFogScatterColor.a;
		if (depth < fogStart)
			media = float4(0.0f, 0.0f, 0.0f, 0.0f);
		else
			media *= vFogNearFar.y / max(vFogNearFar.y - fogStart, 1.0f);

		float  extinction = max(media.a, 1e-5f);
		float3 scattering = media.rgb;

		float sliceT = exp(-extinction * step);
		accum += transmittance * (scattering - scattering * sliceT) / extinction;
		transmittance *= sliceT;

		g_Integrated[coord] = float4(accum, transmittance);
	}
}
)";

// Fullscreen triangle from SV_VertexID — no vertex buffer, no input layout.
static const char* g_szFogCompositeVS = R"(
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

VSOut VSMain(uint vid : SV_VertexID)
{
	VSOut o;
	o.uv  = float2((vid << 1) & 2, vid & 2);
	o.pos = float4(o.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
	return o;
}
)";

// Output is (inscatter, transmittance) and the blend state is ONE / SRC_ALPHA, so the
// hardware evaluates scene*transmittance + inscatter. There is no scene-colour SRV
// available mid-frame in this engine, which is why the composite is done by blending.
static const char* g_szFogCompositePS = R"(
Texture3D<float4> g_Integrated : register(t0);
Texture2D<float>  g_Depth      : register(t1);
SamplerState      g_LinearClamp : register(s0);

struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

float4 PSMain(VSOut i) : SV_TARGET
{
	float zDevice = g_Depth.SampleLevel(g_LinearClamp, i.uv, 0).r;

	// Linear view depth straight from the projection, no matrix inverse involved:
	//   zDevice = _33 + _43 / z   =>   z = _43 / (zDevice - _33)
	// Two scalars are passed from C++ (vFogNearFar.z/.w), so there is no row/column-major
	// convention left to get wrong. Reconstructing world position was what kept failing.
	float projA = vFogNearFar.z;
	float projB = vFogNearFar.w;
	float denom = zDevice - projA;
	float viewZ = (abs(denom) > 1e-7f) ? (projB / denom) : vFogNearFar.y;

	// Sky / cleared depth: no geometry, so no fog.
	if (zDevice >= 0.999999f)
		return float4(0.0f, 0.0f, 0.0f, 1.0f);

	viewZ = clamp(viewZ, 0.0f, vFogNearFar.y);

	// TWO INDEPENDENT DIALS.
	//   start   (vFogNearFar.x) - fog begins here; nearer than this is untouched.
	//   density (vFogMedia.x)   - how fast it accumulates BEYOND the start.
	// Because start only shifts the origin of the accumulation and density only scales the
	// rate, moving one cannot change the other. That is the separation that was missing.
	float d       = max(viewZ - vFogNearFar.x, 0.0f);
	float density = 0.00010f * vFogMedia.x;
	float fogAmt  = saturate(1.0f - exp(-density * d));

	// vFogMedia.z = debug mode from vfogdbg.txt. Alpha 0 makes rgb replace the scene outright.
	//   1 = raw device depth   2 = linear view depth / far   3 = fog amount
	if (vFogMedia.z > 0.5f)
	{
		if (vFogMedia.z < 1.5f)
			return float4(zDevice, zDevice, zDevice, 0.0f);
		if (vFogMedia.z < 2.5f)
		{
			float t = saturate(viewZ / vFogNearFar.y);
			return float4(t, 1.0f - t, 0.0f, 0.0f);
		}
		return float4(fogAmt, fogAmt, fogAmt, 0.0f);
	}

	// Blend is ONE / SRC_ALPHA => scene * (1-fogAmt) + fogColour * fogAmt.
	return float4(vFogScatterColor.rgb * fogAmt, 1.0f - fogAmt);
}
)";

bool CVolumetricFog::ms_bActive = false;

CVolumetricFog::CVolumetricFog()
	: m_pDevice(nullptr), m_pContext(nullptr), m_bInitialized(false), m_bEnabled(false)
	, m_pMediaTex(nullptr), m_pMediaSRV(nullptr), m_pMediaUAV(nullptr)
	, m_pIntegratedTex(nullptr), m_pIntegratedSRV(nullptr), m_pIntegratedUAV(nullptr)
	, m_pCSInject(nullptr), m_pCSIntegrate(nullptr)
	, m_pVSComposite(nullptr), m_pPSComposite(nullptr)
	, m_pCB(nullptr), m_pBlendState(nullptr), m_pDepthState(nullptr)
	, m_pRasterState(nullptr), m_pSamplerLinear(nullptr)
{
	ZeroMemory(&m_cb, sizeof(m_cb));
	m_cb.vGridSize        = XMFLOAT4((float)VFOG_GRID_X, (float)VFOG_GRID_Y, (float)VFOG_GRID_Z, 0.0f);
	m_cb.vFogNearFar      = XMFLOAT4(0.0f, 12800.0f, 1.0f, -1.0f);   // x = start, y = far, zw = proj _33/_43
	m_cb.vFogMedia        = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);   // x = fNear dial, y = fDist dial
	m_cb.vFogScatterColor = XMFLOAT4(0.65f, 0.72f, 0.82f, 0.0f);   // a = start distance
}

CVolumetricFog::~CVolumetricFog()
{
	Shutdown();
}

void CVolumetricFog::SetMedia(float density, float heightFalloff, float baseHeight,
                              float r, float g, float b)
{
	m_cb.vFogMedia.x        = density;
	m_cb.vFogMedia.y        = heightFalloff;
	m_cb.vFogMedia.z        = baseHeight;
	m_cb.vFogScatterColor.x = r;
	m_cb.vFogScatterColor.y = g;
	m_cb.vFogScatterColor.z = b;
}

void CVolumetricFog::SetRange(float nearDist, float farDist)
{
	if (nearDist < 1.0f)        nearDist = 1.0f;
	if (farDist < nearDist * 2) farDist  = nearDist * 2;
	m_cb.vFogNearFar.x = nearDist;
	m_cb.vFogNearFar.y = farDist;
	m_cb.vFogNearFar.z = farDist / nearDist;
	m_cb.vFogNearFar.w = logf(farDist / nearDist);
}

bool CVolumetricFog::__CreateVolume(ID3D11Texture3D** ppTex, ID3D11ShaderResourceView** ppSRV,
                                    ID3D11UnorderedAccessView** ppUAV)
{
	D3D11_TEXTURE3D_DESC desc = {};
	desc.Width     = VFOG_GRID_X;
	desc.Height    = VFOG_GRID_Y;
	desc.Depth     = VFOG_GRID_Z;
	desc.MipLevels = 1;
	desc.Format    = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.Usage     = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	if (FAILED(m_pDevice->CreateTexture3D(&desc, nullptr, ppTex)))
	{
		TraceError("CVolumetricFog: CreateTexture3D failed");
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
	srv.Format              = desc.Format;
	srv.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE3D;
	srv.Texture3D.MipLevels = 1;
	if (FAILED(m_pDevice->CreateShaderResourceView(*ppTex, &srv, ppSRV)))
	{
		TraceError("CVolumetricFog: CreateShaderResourceView failed");
		return false;
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC uav = {};
	uav.Format          = desc.Format;
	uav.ViewDimension   = D3D11_UAV_DIMENSION_TEXTURE3D;
	uav.Texture3D.WSize = VFOG_GRID_Z;
	if (FAILED(m_pDevice->CreateUnorderedAccessView(*ppTex, &uav, ppUAV)))
	{
		TraceError("CVolumetricFog: CreateUnorderedAccessView failed");
		return false;
	}
	return true;
}

void CVolumetricFog::__DestroyVolume(ID3D11Texture3D** ppTex, ID3D11ShaderResourceView** ppSRV,
                                     ID3D11UnorderedAccessView** ppUAV)
{
	if (ppUAV && *ppUAV) { (*ppUAV)->Release(); *ppUAV = nullptr; }
	if (ppSRV && *ppSRV) { (*ppSRV)->Release(); *ppSRV = nullptr; }
	if (ppTex && *ppTex) { (*ppTex)->Release(); *ppTex = nullptr; }
}

static ID3DBlob* __CompileFogShader(const std::string& src, const char* entry, const char* profile)
{
	ID3DBlob* pBlob = nullptr;
	ID3DBlob* pErr  = nullptr;
	HRESULT hr = D3DCompile(src.c_str(), src.size(), nullptr, nullptr, nullptr,
	                        entry, profile, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pBlob, &pErr);
	if (FAILED(hr))
	{
		if (pErr)
		{
			TraceError("CVolumetricFog: %s (%s) failed: %s", entry, profile,
			           (const char*)pErr->GetBufferPointer());
			pErr->Release();
		}
		else
		{
			TraceError("CVolumetricFog: %s (%s) failed hr=0x%08X", entry, profile, hr);
		}
		return nullptr;
	}
	if (pErr) pErr->Release();
	return pBlob;
}

bool CVolumetricFog::__CreateShaders()
{
	const std::string common(g_szFogCommon);

	ID3DBlob* pInject = __CompileFogShader(common + g_szFogInjectCS, "CSMain", "cs_5_0");
	if (!pInject) return false;
	HRESULT hr = m_pDevice->CreateComputeShader(pInject->GetBufferPointer(), pInject->GetBufferSize(),
	                                            nullptr, &m_pCSInject);
	pInject->Release();
	if (FAILED(hr)) return false;

	ID3DBlob* pInteg = __CompileFogShader(common + g_szFogIntegrateCS, "CSMain", "cs_5_0");
	if (!pInteg) return false;
	hr = m_pDevice->CreateComputeShader(pInteg->GetBufferPointer(), pInteg->GetBufferSize(),
	                                    nullptr, &m_pCSIntegrate);
	pInteg->Release();
	if (FAILED(hr)) return false;

	ID3DBlob* pVS = __CompileFogShader(std::string(g_szFogCompositeVS), "VSMain", "vs_5_0");
	if (!pVS) return false;
	hr = m_pDevice->CreateVertexShader(pVS->GetBufferPointer(), pVS->GetBufferSize(),
	                                   nullptr, &m_pVSComposite);
	pVS->Release();
	if (FAILED(hr)) return false;

	ID3DBlob* pPS = __CompileFogShader(common + g_szFogCompositePS, "PSMain", "ps_5_0");
	if (!pPS) return false;
	hr = m_pDevice->CreatePixelShader(pPS->GetBufferPointer(), pPS->GetBufferSize(),
	                                  nullptr, &m_pPSComposite);
	pPS->Release();
	return SUCCEEDED(hr);
}

bool CVolumetricFog::__CreateStates()
{
	// dst = src.rgb * ONE + dst * src.a  =>  inscatter + scene * transmittance
	D3D11_BLEND_DESC bd = {};
	bd.RenderTarget[0].BlendEnable           = TRUE;
	bd.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
	bd.RenderTarget[0].DestBlend             = D3D11_BLEND_SRC_ALPHA;
	bd.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ZERO;
	bd.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ONE;
	bd.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	if (FAILED(m_pDevice->CreateBlendState(&bd, &m_pBlendState)))
		return false;

	D3D11_DEPTH_STENCIL_DESC dd = {};
	dd.DepthEnable    = FALSE;
	dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	dd.DepthFunc      = D3D11_COMPARISON_ALWAYS;
	if (FAILED(m_pDevice->CreateDepthStencilState(&dd, &m_pDepthState)))
		return false;

	D3D11_RASTERIZER_DESC rd = {};
	rd.FillMode        = D3D11_FILL_SOLID;
	rd.CullMode        = D3D11_CULL_NONE;
	rd.DepthClipEnable = TRUE;
	if (FAILED(m_pDevice->CreateRasterizerState(&rd, &m_pRasterState)))
		return false;

	D3D11_SAMPLER_DESC sd = {};
	sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.MaxLOD   = D3D11_FLOAT32_MAX;
	return SUCCEEDED(m_pDevice->CreateSamplerState(&sd, &m_pSamplerLinear));
}

bool CVolumetricFog::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	if (m_bInitialized)
		return true;
	if (!pDevice || !pContext)
		return false;

	m_pDevice  = pDevice;
	m_pContext = pContext;

	if (!__CreateVolume(&m_pMediaTex, &m_pMediaSRV, &m_pMediaUAV) ||
	    !__CreateVolume(&m_pIntegratedTex, &m_pIntegratedSRV, &m_pIntegratedUAV))
	{
		Shutdown();
		return false;
	}

	D3D11_BUFFER_DESC cb = {};
	cb.ByteWidth      = sizeof(CBVolumetricFog);
	cb.Usage          = D3D11_USAGE_DYNAMIC;
	cb.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
	cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(m_pDevice->CreateBuffer(&cb, nullptr, &m_pCB)))
	{
		TraceError("CVolumetricFog: constant buffer creation failed");
		Shutdown();
		return false;
	}

	if (!__CreateShaders() || !__CreateStates())
	{
		TraceError("CVolumetricFog: shader/state creation failed, fog disabled");
		Shutdown();
		return false;
	}

	SetRange(200.0f, 12800.0f);

	m_bInitialized = true;
	Tracef("CVolumetricFog: ready (%ux%ux%u RGBA16F)\n", VFOG_GRID_X, VFOG_GRID_Y, VFOG_GRID_Z);
	return true;
}

void CVolumetricFog::__UpdateConstants(const XMMATRIX& matView, const XMMATRIX& matProj,
                                       const XMFLOAT3& camPos)
{
	// CBPerFrame matrices arrive ALREADY TRANSPOSED (ShaderManager uploads
	// XMMatrixTranspose(...) because HLSL packs column-major). Undo that, build the product in
	// row-vector order, invert, then transpose again for upload like every other matrix here.
	XMMATRIX view = XMMatrixTranspose(matView);
	XMMATRIX proj = XMMatrixTranspose(matProj);
	XMMATRIX vp   = XMMatrixMultiply(view, proj);
	XMVECTOR det;
	m_cb.matInvViewProj = XMMatrixTranspose(XMMatrixInverse(&det, vp));

	// _33 / _43 of the row-major projection drive the linear-depth reconstruction in the PS.
	XMFLOAT4X4 p4;
	XMStoreFloat4x4(&p4, proj);
	m_cb.vFogNearFar.z = p4._33;
	m_cb.vFogNearFar.w = p4._43;
	m_cb.vCameraPos     = XMFLOAT4(camPos.x, camPos.y, camPos.z, 1.0f);

	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(m_pContext->Map(m_pCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &m_cb, sizeof(CBVolumetricFog));
		m_pContext->Unmap(m_pCB, 0);
	}
}

void CVolumetricFog::Render(const XMMATRIX& matView, const XMMATRIX& matProj, const XMFLOAT3& camPos)
{
	if (!m_bInitialized || !m_bEnabled)
		return;

	ID3D11ShaderResourceView* pDepthSRV = CGraphicBase::ResolveAndGetDepthSRV();
	if (!pDepthSRV)
		return;

	__UpdateConstants(matView, matProj, camPos);

	ID3D11ShaderResourceView* nullSRV[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* nullUAV   = nullptr;
	UINT noCount = (UINT)-1;

	// Composite. The scene RT stays bound; DSV is dropped because depth is read as an SRV.
	ID3D11RenderTargetView* pRTV = CGraphicBase::GetRenderTargetView();
	m_pContext->OMSetRenderTargets(1, &pRTV, nullptr);

	const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_pContext->OMSetBlendState(m_pBlendState, blendFactor, 0xFFFFFFFF);
	m_pContext->OMSetDepthStencilState(m_pDepthState, 0);
	m_pContext->RSSetState(m_pRasterState);

	m_pContext->IASetInputLayout(nullptr);
	m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pContext->VSSetShader(m_pVSComposite, nullptr, 0);
	m_pContext->PSSetShader(m_pPSComposite, nullptr, 0);
	m_pContext->PSSetConstantBuffers(0, 1, &m_pCB);

	ID3D11ShaderResourceView* srvs[2] = { m_pIntegratedSRV, pDepthSRV };
	m_pContext->PSSetShaderResources(0, 2, srvs);
	m_pContext->PSSetSamplers(0, 1, &m_pSamplerLinear);

	m_pContext->Draw(3, 0);

	m_pContext->PSSetShaderResources(0, 2, nullSRV);

	ID3D11DepthStencilView* pDSV = CGraphicBase::GetDepthStencilView();
	m_pContext->OMSetRenderTargets(1, &pRTV, pDSV);
}

void CVolumetricFog::Shutdown()
{
	__DestroyVolume(&m_pMediaTex, &m_pMediaSRV, &m_pMediaUAV);
	__DestroyVolume(&m_pIntegratedTex, &m_pIntegratedSRV, &m_pIntegratedUAV);

	if (m_pCSInject)      { m_pCSInject->Release();      m_pCSInject = nullptr; }
	if (m_pCSIntegrate)   { m_pCSIntegrate->Release();   m_pCSIntegrate = nullptr; }
	if (m_pVSComposite)   { m_pVSComposite->Release();   m_pVSComposite = nullptr; }
	if (m_pPSComposite)   { m_pPSComposite->Release();   m_pPSComposite = nullptr; }
	if (m_pCB)            { m_pCB->Release();            m_pCB = nullptr; }
	if (m_pBlendState)    { m_pBlendState->Release();    m_pBlendState = nullptr; }
	if (m_pDepthState)    { m_pDepthState->Release();    m_pDepthState = nullptr; }
	if (m_pRasterState)   { m_pRasterState->Release();   m_pRasterState = nullptr; }
	if (m_pSamplerLinear) { m_pSamplerLinear->Release(); m_pSamplerLinear = nullptr; }

	m_pDevice      = nullptr;
	m_pContext     = nullptr;
	m_bInitialized = false;
	m_bEnabled     = false;
}
