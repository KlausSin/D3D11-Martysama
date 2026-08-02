#include "StdAfx.h"
#include "GrpVertexShader.h"
#include <d3dcompiler.h>
#include <fstream>

CVertexShader::CVertexShader()
{
	Initialize();
}

CVertexShader::~CVertexShader()
{
	Destroy();
}

void CVertexShader::Initialize()
{
	m_pShader = nullptr;
	m_Bytecode.clear();
}

void CVertexShader::Destroy()
{
	if (m_pShader)
	{
		m_pShader->Release();
		m_pShader = nullptr;
	}
	m_Bytecode.clear();
}

bool CVertexShader::CreateFromDiskFile(const char* c_szFileName, const char* szEntryPoint)
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
		"vs_5_0",
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

	// Store bytecode
	m_Bytecode.resize(pShaderBlob->GetBufferSize());
	memcpy(m_Bytecode.data(), pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize());

	// Create shader
	hr = ms_pDevice->CreateVertexShader(
		pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(),
		nullptr,
		&m_pShader
	);

	pShaderBlob->Release();

	return SUCCEEDED(hr);
}

bool CVertexShader::CreateFromBytecodeFile(const char* c_szFileName)
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

	m_Bytecode.resize(static_cast<size_t>(size));
	if (!file.read(reinterpret_cast<char*>(m_Bytecode.data()), size))
	{
		m_Bytecode.clear();
		return false;
	}

	return CreateFromBytecode(m_Bytecode.data(), m_Bytecode.size());
}

bool CVertexShader::CreateFromMemory(const char* szCode, SIZE_T codeLength, const char* szEntryPoint, const char* szName)
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
		"vs_5_0",
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

	// Store bytecode
	m_Bytecode.resize(pShaderBlob->GetBufferSize());
	memcpy(m_Bytecode.data(), pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize());

	// Create shader
	hr = ms_pDevice->CreateVertexShader(
		pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(),
		nullptr,
		&m_pShader
	);

	pShaderBlob->Release();

	return SUCCEEDED(hr);
}

bool CVertexShader::CreateFromBytecode(const void* pBytecode, SIZE_T bytecodeLength)
{
	if (!ms_pDevice || !pBytecode || bytecodeLength == 0)
		return false;

	// Store bytecode if not already stored
	if (m_Bytecode.empty())
	{
		m_Bytecode.resize(bytecodeLength);
		memcpy(m_Bytecode.data(), pBytecode, bytecodeLength);
	}

	HRESULT hr = ms_pDevice->CreateVertexShader(
		pBytecode,
		bytecodeLength,
		nullptr,
		&m_pShader
	);

	return SUCCEEDED(hr);
}

void CVertexShader::Set()
{
	if (ms_pContext && m_pShader)
	{
		ms_pContext->VSSetShader(m_pShader, nullptr, 0);
	}
}

void CVertexShader::Unbind()
{
	if (ms_pContext)
	{
		ms_pContext->VSSetShader(nullptr, nullptr, 0);
	}
}
