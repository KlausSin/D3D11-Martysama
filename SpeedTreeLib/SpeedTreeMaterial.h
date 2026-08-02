///////////////////////////////////////////////////////////////////////
//	CSpeedTreeMaterial Class
//
//	(c) 2003 IDV, Inc.
//
//	This class is provided to illustrate one way to incorporate
//	SpeedTreeRT into an OpenGL application.  All of the SpeedTreeRT
//	calls that must be made on a per tree basis are done by this class.
//	Calls that apply to all trees (i.e. static SpeedTreeRT functions)
//	are made in the functions in main.cpp.
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

#pragma once

///////////////////////////////////////////////////////////////////////
//	Include Files

#include <d3d11.h>
#include <DirectXMath.h>
#include "../eterLib/GrpMathType.h"
#include "../eterLib/GrpMathFunc.h"

// SpeedTree Material struct
struct SSpeedTreeMaterial
{
	Color Diffuse;
	Color Ambient;
	Color Specular;
	Color Emissive;
	float Power;
	float _pad[3];
};

///////////////////////////////////////////////////////////////////////
//	class CSpeedTreeMaterial declaration/definiton

class CSpeedTreeMaterial
{
	public:
		CSpeedTreeMaterial()
		{
			m_cMaterial.Ambient.r = m_cMaterial.Diffuse.r = m_cMaterial.Specular.r = m_cMaterial.Emissive.r = 1.0f;
			m_cMaterial.Ambient.g = m_cMaterial.Diffuse.g = m_cMaterial.Specular.g = m_cMaterial.Emissive.g = 1.0f;
			m_cMaterial.Ambient.b = m_cMaterial.Diffuse.b = m_cMaterial.Specular.b = m_cMaterial.Emissive.b = 1.0f;
			m_cMaterial.Ambient.a = m_cMaterial.Diffuse.a = m_cMaterial.Specular.a = m_cMaterial.Emissive.a = 1.0f;
			m_cMaterial.Power = 5.0f;
		}

		void Set(const float * pMaterialArray)
		{
			m_cMaterial.Diffuse.r = pMaterialArray[0];
			m_cMaterial.Diffuse.g = pMaterialArray[1];
			m_cMaterial.Diffuse.b = pMaterialArray[2];
			m_cMaterial.Diffuse.a = 1.0f;

			m_cMaterial.Ambient.r = pMaterialArray[3];
			m_cMaterial.Ambient.g = pMaterialArray[4];
			m_cMaterial.Ambient.b = pMaterialArray[5];
			m_cMaterial.Ambient.a = 1.0f;

			m_cMaterial.Specular.r = pMaterialArray[6];
			m_cMaterial.Specular.g = pMaterialArray[7];
			m_cMaterial.Specular.b = pMaterialArray[8];
			m_cMaterial.Specular.a = 1.0f;

			m_cMaterial.Emissive.r = pMaterialArray[9];
			m_cMaterial.Emissive.g = pMaterialArray[10];
			m_cMaterial.Emissive.b = pMaterialArray[11];
			m_cMaterial.Emissive.a = 1.0f;

			m_cMaterial.Power = pMaterialArray[12];
		}

		SSpeedTreeMaterial* Get()
		{
			return &m_cMaterial;
		}

	private:
		SSpeedTreeMaterial m_cMaterial;	// the material object
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
