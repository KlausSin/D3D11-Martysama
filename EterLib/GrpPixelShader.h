#pragma once

/*
 * GrpPixelShader.h
 * DX11 Pixel Shader Wrapper
 */

#include "GrpBase.h"
#include <string>
#include <vector>

class CPixelShader : public CGraphicBase
{
public:
	CPixelShader();
	virtual ~CPixelShader();

	void Destroy();

	// Create from HLSL source file
	bool CreateFromDiskFile(const char* c_szFileName, const char* szEntryPoint = "PS_Main");

	// Create from precompiled shader file (.cso)
	bool CreateFromBytecodeFile(const char* c_szFileName);

	// Create from source code in memory
	bool CreateFromMemory(const char* szCode, SIZE_T codeLength, const char* szEntryPoint = "PS_Main", const char* szName = nullptr);

	// Create from bytecode in memory
	bool CreateFromBytecode(const void* pBytecode, SIZE_T bytecodeLength);

	// Bind shader to pipeline
	void Set();
	void Bind() { Set(); }

	// Unbind shader
	static void Unbind();

	// Accessors
	ID3D11PixelShader* GetShader() const { return m_pShader; }
	bool IsValid() const { return m_pShader != nullptr; }

protected:
	void Initialize();

protected:
	ID3D11PixelShader* m_pShader;
};
