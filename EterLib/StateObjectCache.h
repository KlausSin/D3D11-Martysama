#pragma once

#include <d3d11.h>
#include <unordered_map>
#include <functional>
#include <mutex>

// Hash functions for state descriptions
namespace StateHashers
{
	inline size_t HashBlendDesc(const D3D11_BLEND_DESC& desc)
	{
		size_t hash = 0;
		hash ^= std::hash<BOOL>()(desc.AlphaToCoverageEnable) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<BOOL>()(desc.IndependentBlendEnable) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		for (int i = 0; i < 8; ++i)
		{
			hash ^= std::hash<BOOL>()(desc.RenderTarget[i].BlendEnable) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			hash ^= std::hash<UINT8>()(desc.RenderTarget[i].SrcBlend) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			hash ^= std::hash<UINT8>()(desc.RenderTarget[i].DestBlend) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			hash ^= std::hash<UINT8>()(desc.RenderTarget[i].BlendOp) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			hash ^= std::hash<UINT8>()(desc.RenderTarget[i].SrcBlendAlpha) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			hash ^= std::hash<UINT8>()(desc.RenderTarget[i].DestBlendAlpha) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			hash ^= std::hash<UINT8>()(desc.RenderTarget[i].BlendOpAlpha) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			hash ^= std::hash<UINT8>()(desc.RenderTarget[i].RenderTargetWriteMask) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		}
		return hash;
	}

	inline size_t HashRasterizerDesc(const D3D11_RASTERIZER_DESC& desc)
	{
		size_t hash = 0;
		hash ^= std::hash<UINT>()(desc.FillMode) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT>()(desc.CullMode) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<BOOL>()(desc.FrontCounterClockwise) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<INT>()(desc.DepthBias) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<float>()(desc.DepthBiasClamp) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<float>()(desc.SlopeScaledDepthBias) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<BOOL>()(desc.DepthClipEnable) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<BOOL>()(desc.ScissorEnable) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<BOOL>()(desc.MultisampleEnable) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<BOOL>()(desc.AntialiasedLineEnable) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		return hash;
	}

	inline size_t HashDepthStencilDesc(const D3D11_DEPTH_STENCIL_DESC& desc)
	{
		size_t hash = 0;
		hash ^= std::hash<BOOL>()(desc.DepthEnable) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT>()(desc.DepthWriteMask) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT>()(desc.DepthFunc) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<BOOL>()(desc.StencilEnable) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT8>()(desc.StencilReadMask) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT8>()(desc.StencilWriteMask) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT>()(desc.FrontFace.StencilFailOp) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT>()(desc.FrontFace.StencilDepthFailOp) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT>()(desc.FrontFace.StencilPassOp) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT>()(desc.FrontFace.StencilFunc) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT>()(desc.BackFace.StencilFailOp) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT>()(desc.BackFace.StencilDepthFailOp) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT>()(desc.BackFace.StencilPassOp) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT>()(desc.BackFace.StencilFunc) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		return hash;
	}

	inline size_t HashSamplerDesc(const D3D11_SAMPLER_DESC& desc)
	{
		size_t hash = 0;
		hash ^= std::hash<UINT>()(desc.Filter) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT>()(desc.AddressU) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT>()(desc.AddressV) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT>()(desc.AddressW) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<float>()(desc.MipLODBias) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT>()(desc.MaxAnisotropy) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<UINT>()(desc.ComparisonFunc) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<float>()(desc.MinLOD) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<float>()(desc.MaxLOD) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		return hash;
	}
}

class CStateObjectCache
{
public:
	CStateObjectCache(ID3D11Device* pDevice)
		: m_pDevice(pDevice)
	{
	}

	~CStateObjectCache()
	{
		Clear();
	}

	void Clear()
	{
		for (auto& pair : m_BlendStates)
			if (pair.second) pair.second->Release();
		m_BlendStates.clear();

		for (auto& pair : m_RasterizerStates)
			if (pair.second) pair.second->Release();
		m_RasterizerStates.clear();

		for (auto& pair : m_DepthStencilStates)
			if (pair.second) pair.second->Release();
		m_DepthStencilStates.clear();

		for (auto& pair : m_SamplerStates)
			if (pair.second) pair.second->Release();
		m_SamplerStates.clear();
	}

	ID3D11BlendState* GetBlendState(const D3D11_BLEND_DESC& desc)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		size_t hash = StateHashers::HashBlendDesc(desc);
		auto it = m_BlendStates.find(hash);
		if (it != m_BlendStates.end())
			return it->second;

		ID3D11BlendState* pState = nullptr;
		if (SUCCEEDED(m_pDevice->CreateBlendState(&desc, &pState)))
		{
			m_BlendStates[hash] = pState;
			return pState;
		}
		return nullptr;
	}

	ID3D11RasterizerState* GetRasterizerState(const D3D11_RASTERIZER_DESC& desc)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		size_t hash = StateHashers::HashRasterizerDesc(desc);
		auto it = m_RasterizerStates.find(hash);
		if (it != m_RasterizerStates.end())
			return it->second;

		ID3D11RasterizerState* pState = nullptr;
		if (SUCCEEDED(m_pDevice->CreateRasterizerState(&desc, &pState)))
		{
			m_RasterizerStates[hash] = pState;
			return pState;
		}
		return nullptr;
	}

	ID3D11DepthStencilState* GetDepthStencilState(const D3D11_DEPTH_STENCIL_DESC& desc)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		size_t hash = StateHashers::HashDepthStencilDesc(desc);
		auto it = m_DepthStencilStates.find(hash);
		if (it != m_DepthStencilStates.end())
			return it->second;

		ID3D11DepthStencilState* pState = nullptr;
		if (SUCCEEDED(m_pDevice->CreateDepthStencilState(&desc, &pState)))
		{
			m_DepthStencilStates[hash] = pState;
			return pState;
		}
		return nullptr;
	}

	ID3D11SamplerState* GetSamplerState(const D3D11_SAMPLER_DESC& desc)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		size_t hash = StateHashers::HashSamplerDesc(desc);
		auto it = m_SamplerStates.find(hash);
		if (it != m_SamplerStates.end())
			return it->second;

		ID3D11SamplerState* pState = nullptr;
		if (SUCCEEDED(m_pDevice->CreateSamplerState(&desc, &pState)))
		{
			m_SamplerStates[hash] = pState;
			return pState;
		}
		return nullptr;
	}

private:
	ID3D11Device* m_pDevice;
	std::mutex m_mutex;
	std::unordered_map<size_t, ID3D11BlendState*> m_BlendStates;
	std::unordered_map<size_t, ID3D11RasterizerState*> m_RasterizerStates;
	std::unordered_map<size_t, ID3D11DepthStencilState*> m_DepthStencilStates;
	std::unordered_map<size_t, ID3D11SamplerState*> m_SamplerStates;
};
