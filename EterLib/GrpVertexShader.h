#pragma once

/*
 * GrpVertexShader.h
 * DX11 Vertex Shader Wrapper
 */

#include "GrpBase.h"
#include <string>
#include <vector>

class CVertexShader : public CGraphicBase
{
public:
	CVertexShader();
	virtual ~CVertexShader();

	void Destroy();

	// Create from HLSL source file
	bool CreateFromDiskFile(const char* c_szFileName, const char* szEntryPoint = "VS_Main");

	// Create from precompiled shader file (.cso)
	bool CreateFromBytecodeFile(const char* c_szFileName);

	// Create from source code in memory
	bool CreateFromMemory(const char* szCode, SIZE_T codeLength, const char* szEntryPoint = "VS_Main", const char* szName = nullptr);

	// Create from bytecode in memory
	bool CreateFromBytecode(const void* pBytecode, SIZE_T bytecodeLength);

	// Bind shader to pipeline
	void Set();
	void Bind() { Set(); }

	// Unbind shader
	static void Unbind();

	// Accessors
	ID3D11VertexShader* GetShader() const { return m_pShader; }
	const void* GetBytecode() const { return m_Bytecode.data(); }
	SIZE_T GetBytecodeSize() const { return m_Bytecode.size(); }
	bool IsValid() const { return m_pShader != nullptr; }

protected:
	void Initialize();

protected:
	ID3D11VertexShader* m_pShader;
	std::vector<BYTE> m_Bytecode;  // Store bytecode for input layout creation
};
