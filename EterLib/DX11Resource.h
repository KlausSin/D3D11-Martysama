#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <type_traits>

template<typename T>
class CD3D11ConstantBuffer
{
public:
	static_assert(sizeof(T) % 16 == 0, "Constant buffer size must be 16-byte aligned.");
	static_assert(std::is_trivially_copyable_v<T>, "Constant buffer type must be trivially copyable.");

	bool Create(ID3D11Device* pDevice, D3D11_USAGE usage = D3D11_USAGE_DYNAMIC)
	{
		if (!pDevice) 
			return false;

		m_buffer.Reset();
		m_usage = usage;

		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = sizeof(T);
		desc.Usage = usage;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = usage == D3D11_USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0;

		return SUCCEEDED(pDevice->CreateBuffer(&desc, nullptr, m_buffer.GetAddressOf()));
	}

	bool Update(ID3D11DeviceContext* pContext, const T& data)
	{
		if (!pContext || !m_buffer)
			return false;

		if (m_usage == D3D11_USAGE_DEFAULT)
		{
			pContext->UpdateSubresource(m_buffer.Get(), 0, nullptr, &data, 0, 0);
			return true;
		}

		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (FAILED(pContext->Map(m_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) 
			return false;
		memcpy(mapped.pData, &data, sizeof(T));
		pContext->Unmap(m_buffer.Get(), 0);
		return true;
	}

	void BindVS(ID3D11DeviceContext* pContext, UINT slot) const
	{
		if (!pContext || !m_buffer)
			return;

		ID3D11Buffer* p = m_buffer.Get(); pContext->VSSetConstantBuffers(slot, 1, &p);
	}

	void BindPS(ID3D11DeviceContext* pContext, UINT slot) const
	{
		if (!pContext || !m_buffer)
			return;

		ID3D11Buffer* p = m_buffer.Get(); pContext->PSSetConstantBuffers(slot, 1, &p);
	}

	void BindCS(ID3D11DeviceContext* pContext, UINT slot) const
	{
		if (!pContext || !m_buffer) 
			return;

		ID3D11Buffer* p = m_buffer.Get(); pContext->CSSetConstantBuffers(slot, 1, &p);
	}

	void BindVSPS(ID3D11DeviceContext* pContext, UINT slot) const { BindVS(pContext, slot); BindPS(pContext, slot); }
	void Reset() { m_buffer.Reset(); }

	[[nodiscard]] bool IsValid() const noexcept { return m_buffer != nullptr; }
	[[nodiscard]] ID3D11Buffer* Get() const noexcept { return m_buffer.Get(); }

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_buffer;
	D3D11_USAGE m_usage = D3D11_USAGE_DYNAMIC;
};


class CD3D11Shader
{
public:
	CD3D11Shader() {};
	~CD3D11Shader() {};

	bool CreateGraphics(ID3D11Device* pDevice, ID3DBlob* pVSBlob, ID3DBlob* pPSBlob);
	bool CreateVertex(ID3D11Device* pDevice, ID3DBlob* pBlob);
	bool CreatePixel(ID3D11Device* pDevice, ID3DBlob* pBlob);
	bool CreateCompute(ID3D11Device* pDevice, ID3DBlob* pBlob);
	bool CreateInputLayout(ID3D11Device* pDevice, ID3DBlob* pVSBlob);

	void Reset();

	[[nodiscard]] bool IsGraphicsValid() const noexcept;
	[[nodiscard]] bool IsComputeValid() const noexcept;
	[[nodiscard]] bool HasVertexShader() const noexcept;
	[[nodiscard]] bool HasPixelShader() const noexcept;
	[[nodiscard]] bool HasComputeShader() const noexcept;
	[[nodiscard]] bool HasInputLayout() const noexcept;

	[[nodiscard]] ID3D11VertexShader* GetVertexShader() const noexcept;
	[[nodiscard]] ID3D11PixelShader* GetPixelShader() const noexcept;
	[[nodiscard]] ID3D11ComputeShader* GetComputeShader() const noexcept;
	[[nodiscard]] ID3D11InputLayout* GetInputLayout() const noexcept;

private:
	static DXGI_FORMAT GetFloatFormat(UINT mask);
	static DXGI_FORMAT GetUIntFormat(UINT mask);
	static DXGI_FORMAT GetSIntFormat(UINT mask);
	static DXGI_FORMAT GetDXGIFormat(const D3D11_SIGNATURE_PARAMETER_DESC& desc);

private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_computeShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
};
