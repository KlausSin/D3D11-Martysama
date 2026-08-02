///////////////////////////////////////////////////////////////////////
//	SpeedTreeRT DirectX Example
//
//	(c) 2003 IDV, Inc.
//
//	This example demonstrates how to render trees using SpeedTreeRT
//	and DirectX.  Techniques illustrated include ".spt" file parsing,
//	static lighting, dynamic lighting, LOD implementation, cloning,
//	instancing, and dynamic wind effects.
//
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization and may
//	not be copied or disclosed except in accordance with the terms of
//	that agreement.
//
//      Copyright (c) 2001-2003 IDV, Inc.
//      All Rights Reserved.
//
//		IDV, Inc.
//		1233 Washington St. Suite 610
//		Columbia, SC 29201
//		Voice: (803) 799-1699
//		Fax:   (803) 931-0320
//		Web:   http://www.idvinc.com

///////////////////////////////////////////////////////////////////////
//	Includes
#pragma once
#include "../UserInterface/Locale_inc.h"

#include "SpeedTreeConfig.h"
#include <map>
#include <string>


///////////////////////////////////////////////////////////////////////
// Branch Vertex Structure

struct SBranchVertex
{
	Vector3		m_vPosition;			// Always Used
#ifdef WRAPPER_USE_DYNAMIC_LIGHTING
	Vector3		m_vNormal;				// Dynamic Lighting Only
#else
	DWORD			m_dwDiffuseColor;		// Static Lighting Only
#endif
	FLOAT			m_fTexCoords[2];		// Always Used
#ifdef WRAPPER_RENDER_SELF_SHADOWS
	FLOAT			m_fShadowCoords[2];		// Texture coordinates for the shadows
#endif
#ifdef WRAPPER_USE_GPU_WIND
	FLOAT			m_fWindIndex;			// GPU Only
	FLOAT			m_fWindWeight;
#endif
};

///////////////////////////////////////////////////////////////////////
//	Branch/Frond Vertex Program

static const char g_achSimpleVertexProgram[] =
{
		"vs.1.1\n"												// identity shader version

		"mov		oT0.xy,		v7\n"							// always pass texcoord0 through

	#ifdef WRAPPER_RENDER_SELF_SHADOWS
		"mov		oT1.xy,		v8\n"							// pass shadow texcoords through if enabled
	#endif

	#ifdef WRAPPER_USE_GPU_WIND
		// retrieve and convert wind matrix index
		"mov		a0.x,	v9.x\n"

		// perform wind interpolation
		"m4x4		r1,			v0,			c[54+a0.x]\n"		// compute full wind effect
		"sub		r2,			r1,			v0\n"				// compute difference between full wind and none
		"mov		r3.x,		v9.y\n"							// mad can't access two v's at once, use r3.x as tmp
		"mad		r1,			r2,			r3.x,		v0\n"	// perform interpolation

		"add		r2,			c[52],		r1\n"				// translate to tree's position
	#else
		"add		r2,			c[52],		v0\n"				// no wind - just translate to tree's position
	#endif
		"m4x4		oPos,		r2,			c[0]\n"				// project to screen

	#ifdef WRAPPER_USE_FOG
		"dp4		r1,			r2,			c[2]\n"				// find distance to vertex
		"sub		r2.x,		c[85].y,	r1.z\n"				// linear fogging
		"mul		oFog,		r2.x,		c[85].z\n"			// write to fog register
	#endif

	#ifdef WRAPPER_USE_STATIC_LIGHTING
		"mov		oD0,		v5\n"							// pass color through
	#else
		"mov		r1,			c[74]\n"						// can only use one const register per instruction
		"mul		r5,			c[73],		r1\n"				// diffuse values

		"mov		r1,			c[75]\n"						// can only use one const register per instruction
		"mul		r4,			c[72],		r1\n"				// ambient values

		"dp3		r2,		v3,			c[71]\n"				// dot light direction with normal
//		"max		r2.x,		r2.x,		c[70].x\n"			// limit it
		"mad		oD0,		r2.x,		r5,			r4\n"	// compute the final color
	#endif
};


static ID3D11InputLayout* LoadBranchShader(ID3D11Device* pDx, ID3DBlob* pVSBlob = nullptr)
{
	// branch shader declaration (DX11)
	D3D11_INPUT_ELEMENT_DESC pBranchShaderDecl[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
#ifdef WRAPPER_RENDER_SELF_SHADOWS
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
#ifdef WRAPPER_USE_GPU_WIND
		{ "TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
#endif
#else
#ifdef WRAPPER_USE_GPU_WIND
		{ "TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
#endif
#endif
	};

	if (!pVSBlob)
	{
		return nullptr;
	}

	// create input layout
	ID3D11InputLayout* pInputLayout = NULL;

	HRESULT hr = pDx->CreateInputLayout(
		pBranchShaderDecl,
		ARRAYSIZE(pBranchShaderDecl),
		pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(),
		&pInputLayout);

	if (FAILED(hr))
	{
		char szError[1024];
		sprintf_s(szError, "Failed to create branch input layout. HRESULT: 0x%08X", hr);
		MessageBox(NULL, szError, "Input Layout Error", MB_ICONSTOP);
	}

	return pInputLayout;
}


///////////////////////////////////////////////////////////////////////
// Leaf Vertex Structure

struct SLeafVertex
{
		Vector3		m_vPosition;			// Always Used
	#ifdef WRAPPER_USE_DYNAMIC_LIGHTING
		Vector3		m_vNormal;				// Dynamic Lighting Only
	#else
		DWORD			m_dwDiffuseColor;		// Static Lighting Only
	#endif
		FLOAT			m_fTexCoords[2];		// Always Used
	#if defined WRAPPER_USE_GPU_WIND || defined WRAPPER_USE_GPU_LEAF_PLACEMENT
		FLOAT			m_fWindIndex;			// Only used when GPU is involved
		FLOAT			m_fWindWeight;
		FLOAT			m_fLeafPlacementIndex;
		FLOAT			m_fLeafScalarValue;
	#endif
};

///////////////////////////////////////////////////////////////////////
//	Leaf Vertex Program

static const char g_achLeafVertexProgram[] =
{
		"vs.1.1\n"											// identity shader version

		"mov		oT0.xy,	v7\n"							// always pass texcoord0 through

	#ifdef WRAPPER_USE_GPU_WIND
		// retrieve and convert wind matrix index
		"mov		a0.x,	v9.x\n"

		// perform wind interpolation
		"m4x4		r1,		v0,			c[54+a0.x]\n"		// compute full wind effect
		"sub		r2,		r1,			v0\n"				// compute difference between full wind and none
		"mov		r3.x,	v9.y\n"							// mad can't access two v's at once, use r3.x as tmp
		"mad		r0,		r2,			r3.x,		v0\n"	// perform interpolation
	#else
		"mov		r0,		v0\n"							// wind already handled, pass the vertex through
	#endif

	#ifdef WRAPPER_USE_GPU_LEAF_PLACEMENT
		"mov		a0.x,	v9.z\n"							// place the leaves
		"mul		r1,		c[a0.x],	v9.w\n"
		"add		r0,		r1,			r0\n"
	#endif

		"add		r0,		c[52],		r0\n"				// translate to tree's position
		"m4x4		oPos,	r0,			c[0]\n"				// project to screen

	#ifdef WRAPPER_USE_FOG
		"dp4		r1,			r0,			c[2]\n"			// find distance to vertex
		"sub		r2.x,		c[85].y,	r1.z\n"
		"mul		oFog,		r2.x,		c[85].z\n"
	#endif

	#ifdef WRAPPER_USE_STATIC_LIGHTING
		"mov		oD0,	v5\n"							// pass color through
	#else
		"mov		r1,		c[74]\n"						// can only use one const register per instruction
		"mul		r5,		c[73],		r1\n"				// diffuse values

		"mov		r1,		c[75]\n"						// can only use one const register per instruction
		"mul		r4,		c[72],		r1\n"				// ambient values

		"dp3		r2.x,   v3,			c[71]\n"			// dot light direction with normal
		"max		r2.x,	r2.x,		c[70].x\n"			// limit it
		"mad		oD0,	r2.x,		r5,			r4\n"	// compute the final color
	#endif
};


static ID3D11InputLayout* LoadLeafShader(ID3D11Device* pDx, ID3DBlob* pVSBlob = nullptr)
{
	// leaf shader declaration (DX11)
	D3D11_INPUT_ELEMENT_DESC pLeafShaderDecl[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	if (!pVSBlob)
	{
		return nullptr;
	}

	// create input layout
	ID3D11InputLayout* pInputLayout = NULL;

	HRESULT hr = pDx->CreateInputLayout(
		pLeafShaderDecl,
		ARRAYSIZE(pLeafShaderDecl),
		pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(),
		&pInputLayout);

	if (FAILED(hr))
	{
		char szError[1024];
		sprintf_s(szError, "Failed to create leaf input layout. HRESULT: 0x%08X", hr);
		MessageBox(NULL, szError, "Input Layout Error", MB_ICONSTOP);
	}

	return pInputLayout;
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
