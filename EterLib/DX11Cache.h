#pragma once

#include <d3d11.h>
#include <array>

class CDX11Cache
{
public:
	static constexpr UINT MAX_VERTEX_STREAMS = 16;
	static constexpr UINT MAX_SHADER_RESOURCES = 8;
	static constexpr UINT MAX_SAMPLERS = 8;

	struct SVertexBufferState
	{
		ID3D11Buffer* Buffer = nullptr;
		UINT Stride = 0;
		UINT Offset = 0;

		bool operator==(const SVertexBufferState& rhs) const
		{
			return Buffer == rhs.Buffer &&
				Stride == rhs.Stride &&
				Offset == rhs.Offset;
		}
	};

	struct SIndexBufferState
	{
		ID3D11Buffer* Buffer = nullptr;
		DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
		UINT Offset = 0;

		bool operator==(const SIndexBufferState& rhs) const
		{
			return Buffer == rhs.Buffer &&
				Format == rhs.Format &&
				Offset == rhs.Offset;
		}
	};

public:
	CDX11Cache() = default;

	void Initialize(ID3D11DeviceContext* pContext);
	void Shutdown();

	void SetVertexBuffer(UINT stream, ID3D11Buffer* pBuffer, UINT stride, UINT offset = 0);
	void GetVertexBuffer(UINT stream, ID3D11Buffer** ppBuffer, UINT* pStride, UINT* pOffset) const;

	void SetIndexBuffer(ID3D11Buffer* pBuffer, DXGI_FORMAT format = DXGI_FORMAT_R16_UINT, UINT offset = 0);
	void GetIndexBuffer(ID3D11Buffer** ppBuffer, DXGI_FORMAT* pFormat, UINT* pOffset) const;

	void SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology);
	D3D11_PRIMITIVE_TOPOLOGY GetPrimitiveTopology() const;

	void SetInputLayout(ID3D11InputLayout* pLayout);
	ID3D11InputLayout* GetInputLayout() const;

	void SetVertexShader(ID3D11VertexShader* pShader);
	void SetPixelShader(ID3D11PixelShader* pShader);
	ID3D11VertexShader* GetVertexShader() const;
	ID3D11PixelShader* GetPixelShader() const;

	void SetShaderResource(UINT slot, ID3D11ShaderResourceView* pSRV);
	ID3D11ShaderResourceView* GetShaderResource(UINT slot) const;

	void SetSampler(UINT slot, ID3D11SamplerState* pSampler);
	ID3D11SamplerState* GetSampler(UINT slot) const;

	void SetBlendState(ID3D11BlendState* pState, const float* pBlendFactor = nullptr, UINT sampleMask = 0xFFFFFFFF);
	void SetRasterizerState(ID3D11RasterizerState* pState);
	void SetDepthStencilState(ID3D11DepthStencilState* pState, UINT stencilRef = 0);

	ID3D11BlendState* GetBlendState() const;
	ID3D11RasterizerState* GetRasterizerState() const;
	ID3D11DepthStencilState* GetDepthStencilState() const;

	void InvalidateIA();
	void InvalidateShaders();
	void InvalidateResources();
	void InvalidateStates();
	void InvalidateAll();

private:
	ID3D11DeviceContext* m_pContext = nullptr;

	std::array<SVertexBufferState, MAX_VERTEX_STREAMS> m_vertexBuffers{};
	SIndexBufferState m_indexBuffer{};
	D3D11_PRIMITIVE_TOPOLOGY m_topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	ID3D11InputLayout* m_pInputLayout = nullptr;

	ID3D11VertexShader* m_pVertexShader = nullptr;
	ID3D11PixelShader* m_pPixelShader = nullptr;

	std::array<ID3D11ShaderResourceView*, MAX_SHADER_RESOURCES> m_shaderResources{};
	std::array<ID3D11SamplerState*, MAX_SAMPLERS> m_samplers{};

	ID3D11BlendState* m_pBlendState = nullptr;
	ID3D11RasterizerState* m_pRasterizerState = nullptr;
	ID3D11DepthStencilState* m_pDepthStencilState = nullptr;
	float m_blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	UINT m_sampleMask = 0xFFFFFFFF;
	UINT m_stencilRef = 0;
};
