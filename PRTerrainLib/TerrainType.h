#ifndef __TERRAIn_TYPES__
#define __TERRAIn_TYPES__

#include "../eterlib/GrpVertexBuffer.h"
#include "../eterlib/GrpIndexBuffer.h"

#define TERRAIN_PATCHSIZE	16
#define TERRAIN_SIZE		128
#define TERRAIN_PATCHCOUNT	TERRAIN_SIZE/TERRAIN_PATCHSIZE
#define MAXTERRAINTEXTURES	256

typedef struct STerainSplat
{
	long					Active;
	long					NeedsUpdate;
	ID3D11ShaderResourceView*		pd3dTexture;
} TTerainSplat;

typedef struct STerrainSplatPatch
{
 	DWORD			TileCount[MAXTERRAINTEXTURES];
	DWORD			PatchTileCount[TERRAIN_PATCHCOUNT*TERRAIN_PATCHCOUNT][MAXTERRAINTEXTURES];
	TTerainSplat 	Splats[MAXTERRAINTEXTURES];
	bool			m_bNeedsUpdate;
} TTerrainSplatPatch;

typedef struct STERRAIN_VBUFFER
{
	char					used;
	short					mat;

	CGraphicVertexBuffer	vb;
	CGraphicIndexBuffer		ib;
	long					VertexSize;

	short					NumIndices;

	float					minx, maxx;
	float					miny, maxy;
	float					minz, maxz;
} TERRAIN_VBUFFER;

typedef struct SPR_MATERIAL
{
	char name[19];
	float ambi_r, ambi_g, ambi_b, ambi_a;		/* Ambient Color */
	float diff_r, diff_g, diff_b, diff_a;		/* Diffuse Color */
	float spec_r, spec_g, spec_b, spec_a;		/* Specular Color */
	float spec_power;							/* Specular power */
} PR_MATERIAL;

typedef struct STerrainGlobals
{
	/* Public Settings */
	float			PageUVLength;
	long			SquaresPerTexture;              /* Heightfield squares per texture (128 texels) */
	long			SplatTilesX;					/* Number of splat textures across map */
	long			SplatTilesY;					/* Number of splat textures down map */
	long			DisableWrapping;
	long			DisableShadow;
	long			ShadowMode;
	long			OutsideVisible;
	Vector3		SunLocation;
} TTerrainGlobals;

/* Converts a floating point number to an integer by truncation, using
   the FISTP instruction */
#if defined(_M_X64) || defined(_M_ARM64)
#  include <math.h>
#  define PR_FLOAT_TO_INTASM   do { PR_ICNV = (long)lrintf(PR_FCNV); } while (0)
#else
#  define PR_FLOAT_TO_INTASM __asm	\
{									\
	__asm fld PR_FCNV				\
	__asm fistp PR_ICNV				\
}
#endif

#define PR_FLOAT_TO_FIXED(inreal, outint)	\
{											\
	PR_FCNV = (inreal) * 65536.0f;			\
    PR_FLOAT_TO_INTASM;						\
	(outint) = PR_ICNV;						\
}

#define PR_FLOAT_TO_INT(inreal, outint)							\
{																\
	PR_FCNV = (inreal);											\
    PR_FLOAT_TO_INTASM;											\
	(outint) = PR_ICNV > PR_FCNV ? PR_ICNV - 1 : PR_ICNV;		\
}

#define PR_FLOAT_ADD_TO_INT(inreal, outint)	\
{											\
	PR_FCNV = (inreal);						\
    PR_FLOAT_TO_INTASM;						\
	(outint) += PR_ICNV;					\
}

#endif //__TERRAIn_TYPES__
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
