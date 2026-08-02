#pragma once

/*
 * GrpLightType.h
 * Light and Material structure definitions for DX11
 */

#include "GrpMathType.h"
#include "GrpStateEnum.h"

// DX11 Light structure
struct TLight
{
	ELightType  Type;
	Color       Diffuse;
	Color       Specular;
	Color       Ambient;
	Vector3     Position;
	float       _pad0;
	Vector3     Direction;
	float       Range;
	float       Falloff;
	float       Attenuation0;
	float       Attenuation1;
	float       Attenuation2;
	float       Theta;
	float       Phi;
	float       _pad1[2];
};

// DX11 Material structure
struct TMaterial
{
	Color       Diffuse;
	Color       Ambient;
	Color       Specular;
	Color       Emissive;
	float       Power;
	float       _pad[3];
};

