#pragma once

//////////////////////////////////////////////////////////////////////////
// DX11 Native State Enumerations
// Pure DirectX 11 - state objects only
//////////////////////////////////////////////////////////////////////////

enum EPipelineState
{
	// Depth-Stencil State (D3D11_DEPTH_STENCIL_DESC)
	PSTATE_DEPTHENABLE = 0,
	PSTATE_DEPTHWRITEMASK,
	PSTATE_DEPTHFUNC,
	PSTATE_STENCILENABLE,
	PSTATE_STENCILFAILOP,
	PSTATE_STENCILDEPTHFAILOP,
	PSTATE_STENCILPASSOP,
	PSTATE_STENCILFUNC,
	PSTATE_STENCILREF,
	PSTATE_STENCILREADMASK,
	PSTATE_STENCILWRITEMASK,
	PSTATE_TWOSIDEDSTENCIL,
	PSTATE_BACKSTENCILFAILOP,
	PSTATE_BACKSTENCILDEPTHFAILOP,
	PSTATE_BACKSTENCILPASSOP,
	PSTATE_BACKSTENCILFUNC,

	// Blend State (D3D11_BLEND_DESC)
	PSTATE_BLENDENABLE,
	PSTATE_SRCBLEND,
	PSTATE_DESTBLEND,
	PSTATE_BLENDOP,
	PSTATE_SEPARATEALPHABLEND,
	PSTATE_SRCBLENDALPHA,
	PSTATE_DESTBLENDALPHA,
	PSTATE_BLENDOPALPHA,
	PSTATE_RTWRITEMASK,
	PSTATE_RTWRITEMASK1,
	PSTATE_RTWRITEMASK2,
	PSTATE_RTWRITEMASK3,
	PSTATE_BLENDFACTOR,

	// Rasterizer State (D3D11_RASTERIZER_DESC)
	PSTATE_FILLMODE,
	PSTATE_CULLMODE,
	PSTATE_SCISSORENABLE,
	PSTATE_MULTISAMPLEENABLE,
	PSTATE_ANTIALIASEDLINEENABLE,
	PSTATE_DEPTHBIAS,
	PSTATE_SLOPESCALEDDEPTHBIAS,

	PSTATE_MAX
};

//////////////////////////////////////////////////////////////////////////
// Sampler State Types - Maps to D3D11_SAMPLER_DESC
//////////////////////////////////////////////////////////////////////////
enum ESamplerState
{
	SAMPLER_ADDRESSU = 0,
	SAMPLER_ADDRESSV,
	SAMPLER_ADDRESSW,
	SAMPLER_BORDERCOLOR,
	SAMPLER_MAGFILTER,
	SAMPLER_MINFILTER,
	SAMPLER_MIPFILTER,
	SAMPLER_MIPLODBIAS,
	SAMPLER_MAXMIPLEVEL,
	SAMPLER_MAXANISOTROPY,
	SAMPLER_MAX
};

enum EMatrixSlot
{
	MATRIX_VIEW = 0,
	MATRIX_PROJECTION,
	MATRIX_WORLD,
	MATRIX_WORLD1,
	MATRIX_WORLD2,
	MATRIX_WORLD3,
	MATRIX_TEXTURE0,
	MATRIX_TEXTURE1,
	MATRIX_TEXTURE2,
	MATRIX_TEXTURE3,
	MATRIX_TEXTURE4,
	MATRIX_TEXTURE5,
	MATRIX_TEXTURE6,
	MATRIX_TEXTURE7,
	MATRIX_MAX = 300  // Support bone matrices
};

//////////////////////////////////////////////////////////////////////////
// Primitive Types - Maps to D3D11_PRIMITIVE_TOPOLOGY
//////////////////////////////////////////////////////////////////////////
enum EPrimitiveTopology
{
	TOPOLOGY_NONE = 0,       // Special: topology already set (e.g., tessellation patches)
	TOPOLOGY_POINTLIST = 1,
	TOPOLOGY_LINELIST,
	TOPOLOGY_LINESTRIP,
	TOPOLOGY_TRIANGLELIST,
	TOPOLOGY_TRIANGLESTRIP,
};

//////////////////////////////////////////////////////////////////////////
// Fill Modes - Maps to D3D11_FILL_MODE
//////////////////////////////////////////////////////////////////////////
enum EFillMode
{
	FILL_POINT = 1,
	FILL_WIREFRAME,
	FILL_SOLID,
};

//////////////////////////////////////////////////////////////////////////
// Cull Modes - Maps to D3D11_CULL_MODE
//////////////////////////////////////////////////////////////////////////
enum ECullMode
{
	CULL_NONE = 1,
	CULL_FRONT,
	CULL_BACK,
};

//////////////////////////////////////////////////////////////////////////
// Blend Types - Maps to D3D11_BLEND
//////////////////////////////////////////////////////////////////////////
enum EBlend
{
	BLEND_ZERO = 1,
	BLEND_ONE,
	BLEND_SRCCOLOR,
	BLEND_INVSRCCOLOR,
	BLEND_SRCALPHA,
	BLEND_INVSRCALPHA,
	BLEND_DESTALPHA,
	BLEND_INVDESTALPHA,
	BLEND_DESTCOLOR,
	BLEND_INVDESTCOLOR,
	BLEND_SRCALPHASAT,
	BLEND_BOTHSRCALPHA,
	BLEND_BOTHINVSRCALPHA,
	BLEND_BLENDFACTOR,
	BLEND_INVBLENDFACTOR,
};

//////////////////////////////////////////////////////////////////////////
// Blend Operations - Maps to D3D11_BLEND_OP
//////////////////////////////////////////////////////////////////////////
enum EBlendOp
{
	BLENDOP_ADD = 1,
	BLENDOP_SUBTRACT,
	BLENDOP_REVSUBTRACT,
	BLENDOP_MIN,
	BLENDOP_MAX,
};

//////////////////////////////////////////////////////////////////////////
// Compare Functions - Maps to D3D11_COMPARISON_FUNC
//////////////////////////////////////////////////////////////////////////
enum EComparisonFunc
{
	COMPARISON_NEVER = 1,
	COMPARISON_LESS,
	COMPARISON_EQUAL,
	COMPARISON_LESSEQUAL,
	COMPARISON_GREATER,
	COMPARISON_NOTEQUAL,
	COMPARISON_GREATEREQUAL,
	COMPARISON_ALWAYS,
};

enum ETextureAddressMode
{
	ADDRESS_WRAP = 1,
	ADDRESS_MIRROR,
	ADDRESS_CLAMP,
	ADDRESS_BORDER,
	ADDRESS_MIRRORONCE,
};

//////////////////////////////////////////////////////////////////////////
// Texture Filter Types - Maps to D3D11_FILTER components
//////////////////////////////////////////////////////////////////////////
enum ETextureFilter
{
	FILTER_NONE = 0,
	FILTER_POINT,
	FILTER_LINEAR,
	FILTER_ANISOTROPIC,
};

//////////////////////////////////////////////////////////////////////////
// Stencil Operations - Maps to D3D11_STENCIL_OP
//////////////////////////////////////////////////////////////////////////
enum EStencilOp
{
	STENCILOP_KEEP = 1,
	STENCILOP_ZERO,
	STENCILOP_REPLACE,
	STENCILOP_INCRSAT,
	STENCILOP_DECRSAT,
	STENCILOP_INVERT,
	STENCILOP_INCR,
	STENCILOP_DECR,
};

//////////////////////////////////////////////////////////////////////////
// Light Types - For shader light constant buffers
//////////////////////////////////////////////////////////////////////////
enum ELightType
{
	LIGHT_POINT = 1,
	LIGHT_SPOT = 2,
	LIGHT_DIRECTIONAL = 3,
};

//////////////////////////////////////////////////////////////////////////
// Texture Load Filter Flags
//////////////////////////////////////////////////////////////////////////
enum ETextureLoadFilter
{
	TEXFILTER_NONE = 0x00000001,
	TEXFILTER_POINT = 0x00000002,
	TEXFILTER_LINEAR = 0x00000003,
	TEXFILTER_TRIANGLE = 0x00000004,
	TEXFILTER_BOX = 0x00000005,
	TEXFILTER_DEFAULT = 0xFFFFFFFF,
};

enum ETextureOp
{
	TEXOP_DISABLE = 1,
	TEXOP_SELECTARG1 = 2,
	TEXOP_SELECTARG2 = 3,
	TEXOP_MODULATE = 4,
	TEXOP_MODULATE2X = 5,
	TEXOP_MODULATE4X = 6,
	TEXOP_ADD = 7,
	TEXOP_ADDSIGNED = 8,
	TEXOP_ADDSIGNED2X = 9,
	TEXOP_SUBTRACT = 10,
	TEXOP_ADDSMOOTH = 11,
	TEXOP_BLENDDIFFUSEALPHA = 12,
	TEXOP_BLENDTEXTUREALPHA = 13,
	TEXOP_BLENDFACTORALPHA = 14,
	TEXOP_BLENDTEXTUREALPHAPM = 15,
	TEXOP_BLENDCURRENTALPHA = 16,
	TEXOP_PREMODULATE = 17,
	TEXOP_MODULATEALPHA_ADDCOLOR = 18,
	TEXOP_MODULATECOLOR_ADDALPHA = 19,
	TEXOP_MODULATEINVALPHA_ADDCOLOR = 20,
	TEXOP_MODULATEINVCOLOR_ADDALPHA = 21,
	TEXOP_BUMPENVMAP = 22,
	TEXOP_BUMPENVMAPLUMINANCE = 23,
	TEXOP_DOTPRODUCT3 = 24,
	TEXOP_MULTIPLYADD = 25,
	TEXOP_LERP = 26,
};

//////////////////////////////////////////////////////////////////////////
// Math Constants
//////////////////////////////////////////////////////////////////////////
#ifndef MATH_PI
#define MATH_PI 3.14159265358979323846f
#endif

//////////////////////////////////////////////////////////////////////////
// Helper Functions
//////////////////////////////////////////////////////////////////////////
inline EMatrixSlot GetWorldMatrixState(int index)
{
	return static_cast<EMatrixSlot>(MATRIX_WORLD + index);
}

