#include "StdAfx.h"
#include "DX11Resource.h"
#include <string>
#include <vector>

DXGI_FORMAT CD3D11Shader::GetFloatFormat(UINT mask)
{
	if (mask == 1)
		return DXGI_FORMAT_R32_FLOAT;

	if (mask <= 3)
		return DXGI_FORMAT_R32G32_FLOAT;

	if (mask <= 7)
		return DXGI_FORMAT_R32G32B32_FLOAT;

	if (mask <= 15)
		return DXGI_FORMAT_R32G32B32A32_FLOAT;

	return DXGI_FORMAT_UNKNOWN;
}

DXGI_FORMAT CD3D11Shader::GetUIntFormat(UINT mask)
{
	if (mask == 1)
		return DXGI_FORMAT_R32_UINT;

	if (mask <= 3)
		return DXGI_FORMAT_R32G32_UINT;

	if (mask <= 7)
		return DXGI_FORMAT_R32G32B32_UINT;

	if (mask <= 15)
		return DXGI_FORMAT_R32G32B32A32_UINT;

	return DXGI_FORMAT_UNKNOWN;
}

DXGI_FORMAT CD3D11Shader::GetSIntFormat(UINT mask)
{
	if (mask == 1)
		return DXGI_FORMAT_R32_SINT;

	if (mask <= 3)
		return DXGI_FORMAT_R32G32_SINT;

	if (mask <= 7)
		return DXGI_FORMAT_R32G32B32_SINT;

	if (mask <= 15)
		return DXGI_FORMAT_R32G32B32A32_SINT;

	return DXGI_FORMAT_UNKNOWN;
}

DXGI_FORMAT CD3D11Shader::GetDXGIFormat(const D3D11_SIGNATURE_PARAMETER_DESC& desc)
{
	if (!desc.SemanticName)
		return DXGI_FORMAT_UNKNOWN;

	if (_stricmp(desc.SemanticName, "COLOR") == 0)
		return DXGI_FORMAT_B8G8R8A8_UNORM;

	if (_stricmp(desc.SemanticName, "BLENDINDICES") == 0)
		return DXGI_FORMAT_R8G8B8A8_UINT;

	switch (desc.ComponentType)
	{
	case D3D_REGISTER_COMPONENT_FLOAT32:
		return GetFloatFormat(desc.Mask);

	case D3D_REGISTER_COMPONENT_UINT32:
		return GetUIntFormat(desc.Mask);

	case D3D_REGISTER_COMPONENT_SINT32:
		return GetSIntFormat(desc.Mask);

	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

bool CD3D11Shader::CreateVertex(ID3D11Device* pDevice, ID3DBlob* pBlob)
{
	if (!pDevice || !pBlob)
		return false;

	m_vertexShader.Reset();

	HRESULT hr = pDevice->CreateVertexShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, m_vertexShader.GetAddressOf());

	return SUCCEEDED(hr);
}

bool CD3D11Shader::CreatePixel(ID3D11Device* pDevice, ID3DBlob* pBlob)
{
	if (!pDevice || !pBlob)
		return false;

	m_pixelShader.Reset();

	HRESULT hr = pDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, m_pixelShader.GetAddressOf());

	return SUCCEEDED(hr);
}

bool CD3D11Shader::CreateCompute(ID3D11Device* pDevice, ID3DBlob* pBlob)
{
	if (!pDevice || !pBlob)
		return false;

	m_computeShader.Reset();

	HRESULT hr = pDevice->CreateComputeShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, m_computeShader.GetAddressOf());

	return SUCCEEDED(hr);
}

bool CD3D11Shader::CreateInputLayout(ID3D11Device* pDevice, ID3DBlob* pVSBlob)
{
	if (!pDevice || !pVSBlob)
		return false;

	Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflection;

#if _MSC_VER >= 1910 && _MSC_VER <= 1916 //vs 2017 v141
	HRESULT hr = D3DReflect(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), IID_ID3D11ShaderReflection, reinterpret_cast<void**>(&reflection));
#elif _MSC_VER >= 1930 && _MSC_VER < 1950 //vs 2022 v143
	HRESULT hr = D3DReflect(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), IID_PPV_ARGS(&reflection));
#elif _MSC_VER >= 1950 //vs 2026 v145
	HRESULT hr = D3DReflect(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), IID_PPV_ARGS(&reflection));
#endif

	if (FAILED(hr))
		return false;

	D3D11_SHADER_DESC shaderDesc{};
	hr = reflection->GetDesc(&shaderDesc);

	if (FAILED(hr))
		return false;

	std::vector<std::string> semanticNames;
	std::vector<D3D11_INPUT_ELEMENT_DESC> layout;

	semanticNames.reserve(shaderDesc.InputParameters);
	layout.reserve(shaderDesc.InputParameters);

	for (UINT i = 0; i < shaderDesc.InputParameters; ++i)
	{
		D3D11_SIGNATURE_PARAMETER_DESC param{};
		hr = reflection->GetInputParameterDesc(i, &param);

		if (FAILED(hr) || !param.SemanticName)
			return false;

		semanticNames.emplace_back(param.SemanticName);
	}

	for (UINT i = 0; i < shaderDesc.InputParameters; ++i)
	{
		D3D11_SIGNATURE_PARAMETER_DESC param{};
		hr = reflection->GetInputParameterDesc(i, &param);

		if (FAILED(hr))
			return false;

		D3D11_INPUT_ELEMENT_DESC element{};
		element.SemanticName = semanticNames[i].c_str();
		element.SemanticIndex = param.SemanticIndex;
		element.Format = GetDXGIFormat(param);
		element.InputSlot = 0;
		element.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
		element.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		element.InstanceDataStepRate = 0;

		if (element.Format == DXGI_FORMAT_UNKNOWN)
			return false;

		layout.push_back(element);
	}

	m_inputLayout.Reset();

	hr = pDevice->CreateInputLayout(layout.data(), static_cast<UINT>(layout.size()), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), m_inputLayout.GetAddressOf());

	return SUCCEEDED(hr);
}

bool CD3D11Shader::CreateGraphics(ID3D11Device* pDevice, ID3DBlob* pVSBlob, ID3DBlob* pPSBlob)
{
	if (!pDevice || !pVSBlob || !pPSBlob)
		return false;

	Reset();

	if (!CreateVertex(pDevice, pVSBlob))
	{
		Reset();
		return false;
	}

	if (!CreatePixel(pDevice, pPSBlob))
	{
		Reset();
		return false;
	}

	if (!CreateInputLayout(pDevice, pVSBlob))
	{
		Reset();
		return false;
	}

	return true;
}

void CD3D11Shader::Reset()
{
	m_vertexShader.Reset();
	m_pixelShader.Reset();
	m_computeShader.Reset();
	m_inputLayout.Reset();
}

bool CD3D11Shader::IsGraphicsValid() const noexcept
{
	return m_vertexShader && m_pixelShader && m_inputLayout;
}

bool CD3D11Shader::IsComputeValid() const noexcept
{
	return m_computeShader != nullptr;
}

bool CD3D11Shader::HasVertexShader() const noexcept
{
	return m_vertexShader != nullptr;
}

bool CD3D11Shader::HasPixelShader() const noexcept
{
	return m_pixelShader != nullptr;
}

bool CD3D11Shader::HasComputeShader() const noexcept
{
	return m_computeShader != nullptr;
}

bool CD3D11Shader::HasInputLayout() const noexcept
{
	return m_inputLayout != nullptr;
}

ID3D11VertexShader* CD3D11Shader::GetVertexShader() const noexcept
{
	return m_vertexShader.Get();
}

ID3D11PixelShader* CD3D11Shader::GetPixelShader() const noexcept
{
	return m_pixelShader.Get();
}

ID3D11ComputeShader* CD3D11Shader::GetComputeShader() const noexcept
{
	return m_computeShader.Get();
}

ID3D11InputLayout* CD3D11Shader::GetInputLayout() const noexcept
{
	return m_inputLayout.Get();
}