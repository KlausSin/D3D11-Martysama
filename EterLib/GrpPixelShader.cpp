#include "StdAfx.h"
#include "GrpPixelShader.h"
#include <d3dcompiler.h>
#include <fstream>

CPixelShader::CPixelShader()
{
	Initialize();
}

CPixelShader::~CPixelShader()
{
	Destroy();
}

void CPixelShader::Initialize()
{
	m_pShader = nullptr;
}

void CPixelShader::Destroy()
{
	if (m_pShader)
	{
		m_pShader->Release();
		m_pShader = nullptr;
	}
}

bool CPixelShader::CreateFromDiskFile(const char* c_szFileName, const char* szEntryPoint)
{
	Destroy();

	if (!ms_pDevice || !c_szFileName)
		return false;

	// Convert filename to wide string
	int len = MultiByteToWideChar(CP_ACP, 0, c_szFileName, -1, nullptr, 0);
	std::wstring wFileName(len, L'\0');
	MultiByteToWideChar(CP_ACP, 0, c_szFileName, -1, &wFileName[0], len);

	ID3DBlob* pShaderBlob = nullptr;
	ID3DBlob* pErrorBlob = nullptr;

	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	HRESULT hr = D3DCompileFromFile(
		wFileName.c_str(),
		nullptr,                          // No defines
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		szEntryPoint,
		"ps_5_0",
		flags,
		0,
		&pShaderBlob,
		&pErrorBlob
	);

	if (FAILED(hr))
	{
		if (pErrorBlob)
		{
			pErrorBlob->Release();
		}
		return false;
	}

	if (pErrorBlob)
		pErrorBlob->Release();

	// Create shader
	hr = ms_pDevice->CreatePixelShader(
		pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(),
		nullptr,
		&m_pShader
	);

	pShaderBlob->Release();

	return SUCCEEDED(hr);
}

bool CPixelShader::CreateFromBytecodeFile(const char* c_szFileName)
{
	Destroy();

	if (!ms_pDevice || !c_szFileName)
		return false;

	// Read file
	std::ifstream file(c_szFileName, std::ios::binary | std::ios::ate);
	if (!file.is_open())
		return false;

	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<BYTE> bytecode(static_cast<size_t>(size));
	if (!file.read(reinterpret_cast<char*>(bytecode.data()), size))
		return false;

	return CreateFromBytecode(bytecode.data(), bytecode.size());
}

bool CPixelShader::CreateFromMemory(const char* szCode, SIZE_T codeLength, const char* szEntryPoint, const char* szName)
{
	Destroy();

	if (!ms_pDevice || !szCode || codeLength == 0)
		return false;

	ID3DBlob* pShaderBlob = nullptr;
	ID3DBlob* pErrorBlob = nullptr;

	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	HRESULT hr = D3DCompile(
		szCode,
		codeLength,
		szName,
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		szEntryPoint,
		"ps_5_0",
		flags,
		0,
		&pShaderBlob,
		&pErrorBlob
	);

	if (FAILED(hr))
	{
		if (pErrorBlob)
			pErrorBlob->Release();
		return false;
	}

	if (pErrorBlob)
		pErrorBlob->Release();

	// Create shader
	hr = ms_pDevice->CreatePixelShader(
		pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(),
		nullptr,
		&m_pShader
	);

	pShaderBlob->Release();

	return SUCCEEDED(hr);
}

bool CPixelShader::CreateFromBytecode(const void* pBytecode, SIZE_T bytecodeLength)
{
	if (!ms_pDevice || !pBytecode || bytecodeLength == 0)
		return false;

	HRESULT hr = ms_pDevice->CreatePixelShader(
		pBytecode,
		bytecodeLength,
		nullptr,
		&m_pShader
	);

	return SUCCEEDED(hr);
}

void CPixelShader::Set()
{
	if (ms_pContext && m_pShader)
	{
		ms_pContext->PSSetShader(m_pShader, nullptr, 0);
	}
}

void CPixelShader::Unbind()
{
	if (ms_pContext)
	{
		ms_pContext->PSSetShader(nullptr, nullptr, 0);
	}
}
