#include "StdAfx.h"
#include "DX11Cache.h"

void CDX11Cache::Initialize(ID3D11DeviceContext* pContext)
{
	m_pContext = pContext;
	InvalidateAll();
}

void CDX11Cache::Shutdown()
{
	InvalidateAll();
	m_pContext = nullptr;
}

void CDX11Cache::SetVertexBuffer(UINT stream, ID3D11Buffer* pBuffer, UINT stride, UINT offset)
{
	if (!m_pContext || stream >= MAX_VERTEX_STREAMS)
		return;

	SVertexBufferState state;
	state.Buffer = pBuffer;
	state.Stride = stride;
	state.Offset = offset;

	if (m_vertexBuffers[stream] == state)
		return;

	m_vertexBuffers[stream] = state;
	m_pContext->IASetVertexBuffers(stream, 1, &pBuffer, &stride, &offset);
}

void CDX11Cache::GetVertexBuffer(UINT stream, ID3D11Buffer** ppBuffer, UINT* pStride, UINT* pOffset) const
{
	if (ppBuffer)
		*ppBuffer = nullptr;

	if (pStride)
		*pStride = 0;

	if (pOffset)
		*pOffset = 0;

	if (stream >= MAX_VERTEX_STREAMS)
		return;

	const SVertexBufferState& state = m_vertexBuffers[stream];

	if (ppBuffer)
		*ppBuffer = state.Buffer;

	if (pStride)
		*pStride = state.Stride;

	if (pOffset)
		*pOffset = state.Offset;
}

void CDX11Cache::SetIndexBuffer(ID3D11Buffer* pBuffer, DXGI_FORMAT format, UINT offset)
{
	if (!m_pContext)
		return;

	SIndexBufferState state;
	state.Buffer = pBuffer;
	state.Format = format;
	state.Offset = offset;

	if (m_indexBuffer == state)
		return;

	m_indexBuffer = state;
	m_pContext->IASetIndexBuffer(pBuffer, format, offset);
}

void CDX11Cache::GetIndexBuffer(ID3D11Buffer** ppBuffer, DXGI_FORMAT* pFormat, UINT* pOffset) const
{
	if (ppBuffer)
		*ppBuffer = m_indexBuffer.Buffer;

	if (pFormat)
		*pFormat = m_indexBuffer.Format;

	if (pOffset)
		*pOffset = m_indexBuffer.Offset;
}

void CDX11Cache::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology)
{
	if (!m_pContext || topology == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
		return;

	if (m_topology == topology)
		return;

	m_topology = topology;
	m_pContext->IASetPrimitiveTopology(topology);
}

D3D11_PRIMITIVE_TOPOLOGY CDX11Cache::GetPrimitiveTopology() const
{
	return m_topology;
}

void CDX11Cache::SetInputLayout(ID3D11InputLayout* pLayout)
{
	if (!m_pContext)
		return;

	if (m_pInputLayout == pLayout)
		return;

	m_pInputLayout = pLayout;
	m_pContext->IASetInputLayout(pLayout);
}

ID3D11InputLayout* CDX11Cache::GetInputLayout() const
{
	return m_pInputLayout;
}

void CDX11Cache::SetVertexShader(ID3D11VertexShader* pShader)
{
	if (!m_pContext)
		return;

	if (m_pVertexShader == pShader)
		return;

	m_pVertexShader = pShader;
	m_pContext->VSSetShader(pShader, nullptr, 0);
}

void CDX11Cache::SetPixelShader(ID3D11PixelShader* pShader)
{
	if (!m_pContext)
		return;

	if (m_pPixelShader == pShader)
		return;

	m_pPixelShader = pShader;
	m_pContext->PSSetShader(pShader, nullptr, 0);
}

ID3D11VertexShader* CDX11Cache::GetVertexShader() const
{
	return m_pVertexShader;
}

ID3D11PixelShader* CDX11Cache::GetPixelShader() const
{
	return m_pPixelShader;
}

void CDX11Cache::SetShaderResource(UINT slot, ID3D11ShaderResourceView* pSRV)
{
	if (!m_pContext || slot >= MAX_SHADER_RESOURCES)
		return;

	if (m_shaderResources[slot] == pSRV)
		return;

	m_shaderResources[slot] = pSRV;
	m_pContext->PSSetShaderResources(slot, 1, &pSRV);
}

ID3D11ShaderResourceView* CDX11Cache::GetShaderResource(UINT slot) const
{
	if (slot >= MAX_SHADER_RESOURCES)
		return nullptr;

	return m_shaderResources[slot];
}

void CDX11Cache::SetSampler(UINT slot, ID3D11SamplerState* pSampler)
{
	if (!m_pContext || slot >= MAX_SAMPLERS)
		return;

	if (m_samplers[slot] == pSampler)
		return;

	m_samplers[slot] = pSampler;
	m_pContext->PSSetSamplers(slot, 1, &pSampler);
}

ID3D11SamplerState* CDX11Cache::GetSampler(UINT slot) const
{
	if (slot >= MAX_SAMPLERS)
		return nullptr;

	return m_samplers[slot];
}

void CDX11Cache::SetBlendState(ID3D11BlendState* pState, const float* pBlendFactor, UINT sampleMask)
{
	if (!m_pContext)
		return;

	float factor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	if (pBlendFactor)
	{
		factor[0] = pBlendFactor[0];
		factor[1] = pBlendFactor[1];
		factor[2] = pBlendFactor[2];
		factor[3] = pBlendFactor[3];
	}

	const bool sameFactor =
		m_blendFactor[0] == factor[0] &&
		m_blendFactor[1] == factor[1] &&
		m_blendFactor[2] == factor[2] &&
		m_blendFactor[3] == factor[3];

	if (m_pBlendState == pState && m_sampleMask == sampleMask && sameFactor)
		return;

	m_pBlendState = pState;
	m_sampleMask = sampleMask;
	m_blendFactor[0] = factor[0];
	m_blendFactor[1] = factor[1];
	m_blendFactor[2] = factor[2];
	m_blendFactor[3] = factor[3];

	m_pContext->OMSetBlendState(pState, factor, sampleMask);
}

void CDX11Cache::SetRasterizerState(ID3D11RasterizerState* pState)
{
	if (!m_pContext)
		return;

	if (m_pRasterizerState == pState)
		return;

	m_pRasterizerState = pState;
	m_pContext->RSSetState(pState);
}

void CDX11Cache::SetDepthStencilState(ID3D11DepthStencilState* pState, UINT stencilRef)
{
	if (!m_pContext)
		return;

	if (m_pDepthStencilState == pState && m_stencilRef == stencilRef)
		return;

	m_pDepthStencilState = pState;
	m_stencilRef = stencilRef;
	m_pContext->OMSetDepthStencilState(pState, stencilRef);
}

ID3D11BlendState* CDX11Cache::GetBlendState() const
{
	return m_pBlendState;
}

ID3D11RasterizerState* CDX11Cache::GetRasterizerState() const
{
	return m_pRasterizerState;
}

ID3D11DepthStencilState* CDX11Cache::GetDepthStencilState() const
{
	return m_pDepthStencilState;
}

void CDX11Cache::InvalidateIA()
{
	for (SVertexBufferState& state : m_vertexBuffers)
		state = {};

	m_indexBuffer = {};
	m_topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	m_pInputLayout = nullptr;
}

void CDX11Cache::InvalidateShaders()
{
	m_pVertexShader = nullptr;
	m_pPixelShader = nullptr;
}

void CDX11Cache::InvalidateResources()
{
	m_shaderResources.fill(nullptr);
	m_samplers.fill(nullptr);
}

void CDX11Cache::InvalidateStates()
{
	m_pBlendState = nullptr;
	m_pRasterizerState = nullptr;
	m_pDepthStencilState = nullptr;
	m_blendFactor[0] = 1.0f;
	m_blendFactor[1] = 1.0f;
	m_blendFactor[2] = 1.0f;
	m_blendFactor[3] = 1.0f;
	m_sampleMask = 0xFFFFFFFF;
	m_stencilRef = 0;
}

void CDX11Cache::InvalidateAll()
{
	InvalidateIA();
	InvalidateShaders();
	InvalidateResources();
	InvalidateStates();
}
