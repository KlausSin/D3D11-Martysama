#include "stdafx.h"
#include "SkyBox.h"
#include "Camera.h"
#include "ShaderManager.h"
#include "ResourceManager.h"
#include "ShaderInit.h"

#include "../eterBase/Timer.h"

#include "../EterBase/StepTimer.h"

//////////////////////////////////////////////////////////////////////////
// CSkyObjectQuad
//////////////////////////////////////////////////////////////////////////

CSkyObjectQuad::CSkyObjectQuad()
{
	// Index buffer
	m_Indices[0] = 0;
	m_Indices[1] = 2;
	m_Indices[2] = 1;
	m_Indices[3] = 3;

	for (unsigned char uci = 0; uci < 4; ++uci)
	{
		memset(&m_Vertex[uci], 0, sizeof(TPDTVertex));
	}
}

CSkyObjectQuad::~CSkyObjectQuad()
{
}

void CSkyObjectQuad::Clear(const unsigned char & c_rucNumVertex,
						   const float & c_rfRed,
						   const float & c_rfGreen,
						   const float & c_rfBlue,
						   const float & c_rfAlpha)
{
	if (c_rucNumVertex > 3)
		return;
	m_Helper[c_rucNumVertex].Clear(c_rfRed, c_rfGreen, c_rfBlue, c_rfAlpha);
}

void CSkyObjectQuad::SetSrcColor(const unsigned char & c_rucNumVertex,
								 const float & c_rfRed,
								 const float & c_rfGreen,
								 const float & c_rfBlue,
								 const float & c_rfAlpha)
{
	if (c_rucNumVertex > 3)
		return;
	m_Helper[c_rucNumVertex].SetSrcColor(c_rfRed, c_rfGreen, c_rfBlue, c_rfAlpha);
}

void CSkyObjectQuad::SetTransition(const unsigned char & c_rucNumVertex,
								   const float & c_rfRed,
								   const float & c_rfGreen,
								   const float & c_rfBlue,
								   const float & c_rfAlpha,
								   DWORD dwDuration)
{
	if (c_rucNumVertex > 3)
		return;
	m_Helper[c_rucNumVertex].SetTransition(c_rfRed, c_rfGreen, c_rfBlue, c_rfAlpha, dwDuration);
}

void CSkyObjectQuad::SetVertex(const unsigned char & c_rucNumVertex, const TPDTVertex & c_rPDTVertex)
{
	if (c_rucNumVertex > 3)
		return;
	memcpy (&m_Vertex[m_Indices[c_rucNumVertex]], &c_rPDTVertex, sizeof(TPDTVertex));
}

void CSkyObjectQuad::StartTransition()
{
	for (unsigned char uci = 0; uci < 4; ++uci)
	{
		m_Helper[uci].StartTransition();
	}
}

bool CSkyObjectQuad::Update()
{
	bool bResult = false;
	for (unsigned char uci = 0; uci < 4; ++uci)
	{
		bResult = m_Helper[uci].Update() || bResult;
		m_Vertex[m_Indices[uci]].diffuse = m_Helper[uci].GetCurColor();
	}
 	return bResult;
}

void CSkyObjectQuad::Render()
{
	if (CGraphicBase::SetPDTStream(m_Vertex, 4))
		SHADERMANAGER.Draw(TOPOLOGY_TRIANGLESTRIP, 0, 2);
}

//////////////////////////////////////////////////////////////////////////
// CSkyObject
/////////////////////////////////////////////////////////////////////////
CSkyObject::CSkyObject() :
	m_v3Position(0.0f, 0.0f, 0.0f),
	m_fScaleX(1.0f),
	m_fScaleY(1.0f),
	m_fScaleZ(1.0f)
{
	MatrixIdentity(&m_matWorld);
	MatrixIdentity(&m_matTranslation);
	MatrixIdentity(&m_matTextureCloud);

	m_dwlastTime = DX::StepTimer::instance().GetTotalMillieSeconds();

	m_fCloudPositionU = 0.0f;
	m_fCloudPositionV = 0.0f;

	m_bTransitionStarted = false;
	m_bSkyMatrixUpdated = false;
}

CSkyObject::~CSkyObject()
{
	Destroy();
}

void CSkyObject::Destroy()
{
}

void CSkyObject::Update()
{
	CCamera* pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return;

	Vector3 v3Eye = pCamera->GetEye();

	if (m_v3Position == v3Eye)
		if (m_bSkyMatrixUpdated == false)
			return;

	m_v3Position = v3Eye;

	m_matWorld._41 = m_v3Position.x;
	m_matWorld._42 = m_v3Position.y;
	m_matWorld._43 = m_v3Position.z;

	m_matWorldCloud._41 = m_v3Position.x;
	m_matWorldCloud._42 = m_v3Position.y;
	m_matWorldCloud._43 = m_v3Position.z + m_fCloudHeight;

	if (m_bSkyMatrixUpdated)
		m_bSkyMatrixUpdated = false;
}

void CSkyObject::ForcePosition(const Vector3& v3Pos)
{
	m_v3Position = v3Pos;
	m_matWorld._41 = v3Pos.x;
	m_matWorld._42 = v3Pos.y;
	m_matWorld._43 = v3Pos.z;

	m_matWorldCloud._41 = v3Pos.x;
	m_matWorldCloud._42 = v3Pos.y;
	m_matWorldCloud._43 = v3Pos.z + m_fCloudHeight;

	m_bSkyMatrixUpdated = true;
}

void CSkyObject::Render()
{
}

CGraphicImageInstance * CSkyObject::GenerateTexture(const char * szfilename)
{
	assert(szfilename != NULL);

	if (strlen(szfilename) <= 0)
		assert(false);

	CResource * pResource = CResourceManager::Instance().GetResourcePointer(szfilename);

	if (!pResource->IsType(CGraphicImage::Type()))
	{
		assert(false);
		return NULL;
	}

	CGraphicImageInstance * pImageInstance = CGraphicImageInstance::New();
	pImageInstance->SetImagePointer(static_cast<CGraphicImage *>(pResource));
	return (pImageInstance);
}

void CSkyObject::DeleteTexture(CGraphicImageInstance * pImageInstance)
{
	if (pImageInstance)
		CGraphicImageInstance::Delete(pImageInstance);
}

void CSkyObject::StartTransition()
{
}

//////////////////////////////////////////////////////////////////////////
// CSkyObject::TSkyObjectFace
//////////////////////////////////////////////////////////////////////////

void CSkyObject::TSkyObjectFace::StartTransition()
{
	for (unsigned char uci = 0; uci < m_SkyObjectQuadVector.size(); ++uci)
	{
		m_SkyObjectQuadVector[uci].StartTransition();
	}
}

bool CSkyObject::TSkyObjectFace::Update()
{
	bool bResult = false;
	for (DWORD dwi = 0; dwi < m_SkyObjectQuadVector.size(); ++dwi)
 		bResult = m_SkyObjectQuadVector[dwi].Update() || bResult;
 	return bResult;
}

void CSkyObject::TSkyObjectFace::Render()
{
	for (unsigned char uci = 0; uci < m_SkyObjectQuadVector.size(); ++uci)
	{
		m_SkyObjectQuadVector[uci].Render();
	}
}

//////////////////////////////////////////////////////////////////////////
// CSkyBox
//////////////////////////////////////////////////////////////////////////

CSkyBox::CSkyBox()
{
	m_ucVirticalGradientLevelUpper = 0;
	m_ucVirticalGradientLevelLower = 0;
	m_ucOriginalGradientLevelUpper = 0;
	m_ucOriginalGradientLevelLower = 0;
	m_lGradientTransitionDuration = 0;
	m_dwGradientTransitionStart = 0;
	m_bGradientTransitionActive = false;

#ifdef ENABLE_CELESTIAL_BODY
	m_pSunTextureInstance = nullptr;
	m_pMoonTextureInstance = nullptr;
	m_vLightDirection = Vector3(0.0f, 0.0f, -1.0f);
	m_fSunSize = 0.07f;
	m_fMoonSize = 0.055f;
	m_bCelestialReady = false;
	m_bIsNight = false;
#endif
}

CSkyBox::~CSkyBox()
{
	Destroy();
}

void CSkyBox::Destroy()
{
	Unload();
}

void CSkyBox::SetSkyBoxScale(const Vector3 & c_rv3Scale)
{
	m_fScaleX = c_rv3Scale.x;
	m_fScaleY = c_rv3Scale.y;
	m_fScaleZ = c_rv3Scale.z;

	m_bSkyMatrixUpdated = true;
	MatrixScaling(&m_matWorld, m_fScaleX, m_fScaleY, m_fScaleZ);
}

static const int SKY_GRADIENT_SUBDIV = 4;

void CSkyBox::SetGradientLevel(BYTE byUpper, BYTE byLower)
{
	m_ucOriginalGradientLevelUpper = byUpper;
	m_ucOriginalGradientLevelLower = byLower;
	m_ucVirticalGradientLevelUpper = byUpper * SKY_GRADIENT_SUBDIV;
	m_ucVirticalGradientLevelLower = byLower * SKY_GRADIENT_SUBDIV;
}

void CSkyBox::SetFaceTexture( const char* c_szFileName, int iFaceIndex )
{
	if( iFaceIndex < 0 || iFaceIndex > 5 )
		return;

	TGraphicImageInstanceMap::iterator itor = m_GraphicImageInstanceMap.find(c_szFileName);
	if (m_GraphicImageInstanceMap.end() != itor)
		return;

	m_Faces[iFaceIndex].m_strFaceTextureFileName = c_szFileName;

	CGraphicImageInstance * pGraphicImageInstance = GenerateTexture(c_szFileName);
	m_GraphicImageInstanceMap.insert(TGraphicImageInstanceMap::value_type(c_szFileName, pGraphicImageInstance));
}

void CSkyBox::SetCloudTexture(const char * c_szFileName)
{
	TGraphicImageInstanceMap::iterator itor = m_GraphicImageInstanceMap.find(c_szFileName);
	if (m_GraphicImageInstanceMap.end() != itor)
		return;

	m_FaceCloud.m_strfacename = c_szFileName;
	CGraphicImageInstance * pGraphicImageInstance = GenerateTexture(c_szFileName);
	if (pGraphicImageInstance)
		m_GraphicImageInstanceMap.insert(TGraphicImageInstanceMap::value_type(m_FaceCloud.m_strfacename, pGraphicImageInstance));

//	CGraphicImage * pImage = (CGraphicImage *) CResourceManager::Instance().GetResourcePointer("D:\\Ymir Work\\special/cloudalpha.tga");
//	m_CloudAlphaImageInstance.SetImagePointer(pImage);
}

void CSkyBox::SetCloudScale(const Vector2 & c_rv2CloudScale)
{
	m_fCloudScaleX = c_rv2CloudScale.x;
	m_fCloudScaleY = c_rv2CloudScale.y;

	MatrixScaling(&m_matWorldCloud, m_fCloudScaleX, m_fCloudScaleY, 1.0f);
}

void CSkyBox::SetCloudHeight(float fHeight)
{
	m_fCloudHeight = fHeight;
}

void CSkyBox::SetCloudTextureScale(const Vector2 & c_rv2CloudTextureScale)
{
	m_fCloudTextureScaleX = c_rv2CloudTextureScale.x;
	m_fCloudTextureScaleY = c_rv2CloudTextureScale.y;

	m_matTextureCloud._11 = m_fCloudTextureScaleX;
	m_matTextureCloud._22 = m_fCloudTextureScaleY;
}

void CSkyBox::SetCloudScrollSpeed(const Vector2 & c_rv2CloudScrollSpeed)
{
	m_fCloudScrollSpeedU = c_rv2CloudScrollSpeed.x;
	m_fCloudScrollSpeedV = c_rv2CloudScrollSpeed.y;
}

void CSkyBox::Unload()
{
	TGraphicImageInstanceMap::iterator itor = m_GraphicImageInstanceMap.begin();

	while (itor != m_GraphicImageInstanceMap.end())
	{
		DeleteTexture(itor->second);
		++itor;
	}

	m_GraphicImageInstanceMap.clear();

#ifdef ENABLE_CELESTIAL_BODY
	if (m_pSunTextureInstance) { DeleteTexture(m_pSunTextureInstance); m_pSunTextureInstance = nullptr; }
	if (m_pMoonTextureInstance) { DeleteTexture(m_pMoonTextureInstance); m_pMoonTextureInstance = nullptr; }
	m_bCelestialReady = false;
#endif
}

void CSkyBox::SetSkyObjectQuadVertical(TSkyObjectQuadVector * pSkyObjectQuadVector, const Vector2 * c_pv2QuadPoints)
{
	TPDTVertex aPDTVertex;

	DWORD dwTotalQuads = (DWORD)m_ucVirticalGradientLevelUpper + (DWORD)m_ucVirticalGradientLevelLower;
	if (dwTotalQuads == 0)
		return;

	pSkyObjectQuadVector->clear();
	pSkyObjectQuadVector->resize(dwTotalQuads);

	for (DWORD ucY = 0; ucY < dwTotalQuads; ++ucY)
	{
		CSkyObjectQuad & rSkyObjectQuad = pSkyObjectQuadVector->at(ucY);

		float zTop = 1.0f - 2.0f * (float)ucY / (float)dwTotalQuads;
		float zBot = 1.0f - 2.0f * (float)(ucY + 1) / (float)dwTotalQuads;
		float texTop = (float)ucY / (float)dwTotalQuads;
		float texBot = (float)(ucY + 1) / (float)dwTotalQuads;

		aPDTVertex.position.x = c_pv2QuadPoints[0].x;
		aPDTVertex.position.y = c_pv2QuadPoints[0].y;
		aPDTVertex.position.z = zBot;
		aPDTVertex.texCoord.x = 0.0f;
		aPDTVertex.texCoord.y = texBot;
		rSkyObjectQuad.SetVertex(0, aPDTVertex);

		aPDTVertex.position.x = c_pv2QuadPoints[0].x;
		aPDTVertex.position.y = c_pv2QuadPoints[0].y;
		aPDTVertex.position.z = zTop;
		aPDTVertex.texCoord.x = 0.0f;
		aPDTVertex.texCoord.y = texTop;
		rSkyObjectQuad.SetVertex(1, aPDTVertex);

		aPDTVertex.position.x = c_pv2QuadPoints[1].x;
		aPDTVertex.position.y = c_pv2QuadPoints[1].y;
		aPDTVertex.position.z = zBot;
		aPDTVertex.texCoord.x = 1.0f;
		aPDTVertex.texCoord.y = texBot;
		rSkyObjectQuad.SetVertex(2, aPDTVertex);

		aPDTVertex.position.x = c_pv2QuadPoints[1].x;
		aPDTVertex.position.y = c_pv2QuadPoints[1].y;
		aPDTVertex.position.z = zTop;
		aPDTVertex.texCoord.x = 1.0f;
		aPDTVertex.texCoord.y = texTop;
		rSkyObjectQuad.SetVertex(3, aPDTVertex);
	}
}


void CSkyBox::SetSkyObjectQuadHorizon(TSkyObjectQuadVector * pSkyObjectQuadVector, const Vector3 * c_pv3QuadPoints)
{
	pSkyObjectQuadVector->clear();
	pSkyObjectQuadVector->resize(1);
	CSkyObjectQuad & rSkyObjectQuad = pSkyObjectQuadVector->at(0);

	TPDTVertex aPDTVertex;
	aPDTVertex.position		= c_pv3QuadPoints[0];
	aPDTVertex.texCoord.x	= 0.0f;
	aPDTVertex.texCoord.y	= 1.0f;
	rSkyObjectQuad.SetVertex(0, aPDTVertex);

	aPDTVertex.position		= c_pv3QuadPoints[1];
	aPDTVertex.texCoord.x	= 0.0f;
	aPDTVertex.texCoord.y	= 0.0f;
	rSkyObjectQuad.SetVertex(1, aPDTVertex);

	aPDTVertex.position		= c_pv3QuadPoints[2];
	aPDTVertex.texCoord.x	= 1.0f;
	aPDTVertex.texCoord.y	= 1.0f;
	rSkyObjectQuad.SetVertex(2, aPDTVertex);

	aPDTVertex.position		= c_pv3QuadPoints[3];
	aPDTVertex.texCoord.x	= 1.0f;
	aPDTVertex.texCoord.y	= 0.0f;
	rSkyObjectQuad.SetVertex(3, aPDTVertex);
}

void CSkyBox::Refresh()
{
	Vector3 v3QuadPoints[4];

	if( m_ucRenderMode == CSkyObject::SKY_RENDER_MODE_DEFAULT ||  m_ucRenderMode == CSkyObject::SKY_RENDER_MODE_DIFFUSE )
	{
		if (m_ucVirticalGradientLevelUpper + m_ucVirticalGradientLevelLower <= 0)
			return;

		Vector2 v2QuadPoints[2];

		//// Face 0: FRONT
		v2QuadPoints[0] = Vector2(1.0f, -1.0f);
		v2QuadPoints[1] = Vector2(-1.0f, -1.0f);
		SetSkyObjectQuadVertical(&m_Faces[0].m_SkyObjectQuadVector, v2QuadPoints);
		m_Faces[0].m_strfacename = "front";

		//// Face 1: BACK
		v2QuadPoints[0] = Vector2(-1.0f, 1.0f);
		v2QuadPoints[1] = Vector2(1.0f, 1.0f);
		SetSkyObjectQuadVertical(&m_Faces[1].m_SkyObjectQuadVector, v2QuadPoints);
		m_Faces[1].m_strfacename = "back";

		//// Face 2: LEFT
		v2QuadPoints[0] = Vector2(-1.0f, -1.0f);
		v2QuadPoints[1] = Vector2(-1.0f, 1.0f);
		SetSkyObjectQuadVertical(&m_Faces[2].m_SkyObjectQuadVector, v2QuadPoints);
		m_Faces[2].m_strfacename = "left";

		//// Face 3: RIGHT
		v2QuadPoints[0] = Vector2(1.0f, 1.0f);
		v2QuadPoints[1] = Vector2(1.0f, -1.0f);
		SetSkyObjectQuadVertical(&m_Faces[3].m_SkyObjectQuadVector, v2QuadPoints);
		m_Faces[3].m_strfacename = "right";

		//// Face 4: TOP
		v3QuadPoints[0] = Vector3(1.0f, 1.0f, 1.0f);
		v3QuadPoints[1] = Vector3(-1.0f, 1.0f, 1.0f);
		v3QuadPoints[2] = Vector3(1.0f, -1.0f, 1.0f);
		v3QuadPoints[3] = Vector3(-1.0f, -1.0f, 1.0f);
		SetSkyObjectQuadHorizon(&m_Faces[4].m_SkyObjectQuadVector, v3QuadPoints);
		m_Faces[4].m_strfacename = "top";

		//// Face 5: BOTTOM
		v3QuadPoints[0] = Vector3(-1.0f, 1.0f, -1.0f);
		v3QuadPoints[1] = Vector3(1.0f, 1.0f, -1.0f);
		v3QuadPoints[2] = Vector3(-1.0f, -1.0f, -1.0f);
		v3QuadPoints[3] = Vector3(1.0f, -1.0f, -1.0f);
		SetSkyObjectQuadHorizon(&m_Faces[5].m_SkyObjectQuadVector, v3QuadPoints);
		m_Faces[5].m_strfacename = "bottom";

	}
	else if( m_ucRenderMode == CSkyObject::SKY_RENDER_MODE_TEXTURE )
	{
		// Face 0: FRONT
		v3QuadPoints[0] = Vector3(1.0f, -1.0f, -1.0f);
		v3QuadPoints[1] = Vector3(1.0f, -1.0f, 1.0f);
		v3QuadPoints[2] = Vector3(-1.0f, -1.0f, -1.0f);
		v3QuadPoints[3] = Vector3(-1.0f, -1.0f, 1.0f);

		//UpdateSkyFaceQuadTransform(v3QuadPoints);

		SetSkyObjectQuadHorizon(&m_Faces[0].m_SkyObjectQuadVector, v3QuadPoints);
		m_Faces[0].m_strfacename = "front";

		//// Face 1: BACK
		v3QuadPoints[0] = Vector3(-1.0f, 1.0f, -1.0f);
		v3QuadPoints[1] = Vector3(-1.0f, 1.0f, 1.0f);
		v3QuadPoints[2] = Vector3(1.0f, 1.0f, -1.0f);
		v3QuadPoints[3] = Vector3(1.0f, 1.0f, 1.0f);

		//UpdateSkyFaceQuadTransform(v3QuadPoints);

		SetSkyObjectQuadHorizon(&m_Faces[1].m_SkyObjectQuadVector, v3QuadPoints);
		m_Faces[1].m_strfacename = "back";

		// Face 2: LEFT
		v3QuadPoints[0] = Vector3(1.0f, 1.0f, -1.0f);
		v3QuadPoints[1] = Vector3(1.0f, 1.0f, 1.0f);
		v3QuadPoints[2] = Vector3(1.0f, -1.0f, -1.0f);
		v3QuadPoints[3] = Vector3(1.0f, -1.0f, 1.0f);

		//UpdateSkyFaceQuadTransform(v3QuadPoints);

		SetSkyObjectQuadHorizon(&m_Faces[2].m_SkyObjectQuadVector, v3QuadPoints);
		m_Faces[2].m_strfacename = "left";

		// Face 3: RIGHT
		v3QuadPoints[0] = Vector3(-1.0f, -1.0f, -1.0f);
		v3QuadPoints[1] = Vector3(-1.0f, -1.0f, 1.0f);
		v3QuadPoints[2] = Vector3(-1.0f, 1.0f, -1.0f);
		v3QuadPoints[3] = Vector3(-1.0f, 1.0f, 1.0f);

		//UpdateSkyFaceQuadTransform(v3QuadPoints);

		SetSkyObjectQuadHorizon(&m_Faces[3].m_SkyObjectQuadVector, v3QuadPoints);
		m_Faces[3].m_strfacename = "right";

		// Face 4: TOP
		v3QuadPoints[0] = Vector3(1.0f, -1.0f, 1.0f);
		v3QuadPoints[1] = Vector3(1.0f, 1.0f, 1.0f);
		v3QuadPoints[2] = Vector3(-1.0f, -1.0f, 1.0f);
		v3QuadPoints[3] = Vector3(-1.0f, 1.0f, 1.0f);

		//UpdateSkyFaceQuadTransform(v3QuadPoints);

		SetSkyObjectQuadHorizon(&m_Faces[4].m_SkyObjectQuadVector, v3QuadPoints);
		m_Faces[4].m_strfacename = "top";

		////// Face 5: BOTTOM
		// @fixme005
		v3QuadPoints[0] = Vector3(1.0f, 1.0f, -1.0f);
		v3QuadPoints[1] = Vector3(1.0f, -1.0f, -1.0f);
		v3QuadPoints[2] = Vector3(-1.0f, 1.0f, -1.0f);
		v3QuadPoints[3] = Vector3(-1.0f, -1.0f, -1.0f);

		//UpdateSkyFaceQuadTransform(v3QuadPoints);

		SetSkyObjectQuadHorizon(&m_Faces[5].m_SkyObjectQuadVector, v3QuadPoints);
		m_Faces[5].m_strfacename = "bottom";
	}

	//// Clouds..
	v3QuadPoints[0] = Vector3(1.0f, 1.0f, 0.0f);
	v3QuadPoints[1] = Vector3(-1.0f, 1.0f, 0.0f);
	v3QuadPoints[2] = Vector3(1.0f, -1.0f, 0.0f);
	v3QuadPoints[3] = Vector3(-1.0f, -1.0f, 0.0f);
	SetSkyObjectQuadHorizon(&m_FaceCloud.m_SkyObjectQuadVector, v3QuadPoints);
}

void CSkyBox::SetCloudColor(const TGradientColor & c_rColor, const TGradientColor & c_rNextColor, const DWORD & dwTransitionTime)
{
	bool bColorIsBlack = (c_rColor.m_FirstColor.r < 0.01f && c_rColor.m_FirstColor.g < 0.01f &&
	                      c_rColor.m_FirstColor.b < 0.01f && c_rColor.m_FirstColor.a < 0.01f);
	bool bNextColorIsBlack = (c_rNextColor.m_FirstColor.r < 0.01f && c_rNextColor.m_FirstColor.g < 0.01f &&
	                          c_rNextColor.m_FirstColor.b < 0.01f && c_rNextColor.m_FirstColor.a < 0.01f);

	float fSrcR = bColorIsBlack ? 1.0f : c_rColor.m_FirstColor.r;
	float fSrcG = bColorIsBlack ? 1.0f : c_rColor.m_FirstColor.g;
	float fSrcB = bColorIsBlack ? 1.0f : c_rColor.m_FirstColor.b;
	float fSrcA = bColorIsBlack ? 1.0f : c_rColor.m_FirstColor.a;
	float fDstR = bNextColorIsBlack ? 1.0f : c_rNextColor.m_FirstColor.r;
	float fDstG = bNextColorIsBlack ? 1.0f : c_rNextColor.m_FirstColor.g;
	float fDstB = bNextColorIsBlack ? 1.0f : c_rNextColor.m_FirstColor.b;
	float fDstA = bNextColorIsBlack ? 1.0f : c_rNextColor.m_FirstColor.a;

	TSkyObjectFace & aFaceCloud = m_FaceCloud;
	for (DWORD dwk = 0; dwk < aFaceCloud.m_SkyObjectQuadVector.size(); ++dwk)
	{
		CSkyObjectQuad & aSkyObjectQuad = aFaceCloud.m_SkyObjectQuadVector[dwk];

		aSkyObjectQuad.SetSrcColor(0, fSrcR, fSrcG, fSrcB, fSrcA);
		aSkyObjectQuad.SetTransition(0, fDstR, fDstG, fDstB, fDstA, dwTransitionTime);
		aSkyObjectQuad.SetSrcColor(1, fSrcR, fSrcG, fSrcB, fSrcA);
		aSkyObjectQuad.SetTransition(1, fDstR, fDstG, fDstB, fDstA, dwTransitionTime);
		aSkyObjectQuad.SetSrcColor(2, fSrcR, fSrcG, fSrcB, fSrcA);
		aSkyObjectQuad.SetTransition(2, fDstR, fDstG, fDstB, fDstA, dwTransitionTime);
		aSkyObjectQuad.SetSrcColor(3, fSrcR, fSrcG, fSrcB, fSrcA);
		aSkyObjectQuad.SetTransition(3, fDstR, fDstG, fDstB, fDstA, dwTransitionTime);
	}
}

static TColor SampleGradient(const std::vector<TColor>& points, float t)
{
	int N = (int)points.size() - 1;
	if (N <= 0) return points[0];

	t = max(0.0f, min((float)N, t));
	int seg = min((int)t, N - 1);
	float f = t - (float)seg;

	f = f * f * (3.0f - 2.0f * f);

	const TColor& a = points[seg];
	const TColor& b = points[seg + 1];

	TColor result;
	result.r = a.r + (b.r - a.r) * f;
	result.g = a.g + (b.g - a.g) * f;
	result.b = a.b + (b.b - a.b) * f;
	result.a = a.a + (b.a - a.a) * f;
	return result;
}

void CSkyBox::SetSkyColor(const TVectorGradientColor & c_rColorVector, const TVectorGradientColor & c_rNextColorVector, long lTransitionTime)
{
	int numOriginal = (int)c_rColorVector.size();
	std::vector<TColor> srcPoints, nextPoints;
	srcPoints.reserve(numOriginal + 1);
	nextPoints.reserve(numOriginal + 1);

	if (numOriginal > 0)
	{
		srcPoints.push_back(c_rColorVector[0].m_FirstColor);
		nextPoints.push_back(c_rNextColorVector[0].m_FirstColor);
		for (int i = 0; i < numOriginal; ++i)
		{
			srcPoints.push_back(c_rColorVector[i].m_SecondColor);
			nextPoints.push_back(c_rNextColorVector[i].m_SecondColor);
		}
	}

	m_gradientSrc = srcPoints;
	m_gradientDst = nextPoints;
	m_lGradientTransitionDuration = lTransitionTime;
	m_bGradientTransitionActive = false; // Will be activated by StartTransition()

	int numSegments = numOriginal; // total gradient segments
	unsigned long uck;
	for (unsigned char ucj = 0; ucj < 4; ++ucj)
	{
		TSkyObjectFace & aFace = m_Faces[ucj];
		DWORD dwTotalQuads = (DWORD)aFace.m_SkyObjectQuadVector.size();
		if (dwTotalQuads == 0)
			continue;

		for (uck = 0; uck < dwTotalQuads; ++uck)
		{
			CSkyObjectQuad & aSkyObjectQuad = aFace.m_SkyObjectQuadVector[uck];

			float tTop = (float)uck * (float)numSegments / (float)dwTotalQuads;
			float tBot = (float)(uck + 1) * (float)numSegments / (float)dwTotalQuads;

			TColor srcTop = SampleGradient(srcPoints, tTop);
			TColor srcBot = SampleGradient(srcPoints, tBot);
			TColor nextTop = SampleGradient(nextPoints, tTop);
			TColor nextBot = SampleGradient(nextPoints, tBot);

			// Vertices 0,2 = bottom edge, Vertices 1,3 = top edge
			aSkyObjectQuad.SetSrcColor(0, srcBot.r, srcBot.g, srcBot.b, srcBot.a);
			aSkyObjectQuad.SetTransition(0, nextBot.r, nextBot.g, nextBot.b, nextBot.a, lTransitionTime);
			aSkyObjectQuad.SetSrcColor(1, srcTop.r, srcTop.g, srcTop.b, srcTop.a);
			aSkyObjectQuad.SetTransition(1, nextTop.r, nextTop.g, nextTop.b, nextTop.a, lTransitionTime);
			aSkyObjectQuad.SetSrcColor(2, srcBot.r, srcBot.g, srcBot.b, srcBot.a);
			aSkyObjectQuad.SetTransition(2, nextBot.r, nextBot.g, nextBot.b, nextBot.a, lTransitionTime);
			aSkyObjectQuad.SetSrcColor(3, srcTop.r, srcTop.g, srcTop.b, srcTop.a);
			aSkyObjectQuad.SetTransition(3, nextTop.r, nextTop.g, nextTop.b, nextTop.a, lTransitionTime);
		}
	}

	/////

	TSkyObjectFace & aFaceTop = m_Faces[4];
	const TColor & srcZenith = srcPoints[0];
	const TColor & nextZenith = nextPoints[0];
	for (unsigned long uckTop = 0; uckTop < aFaceTop.m_SkyObjectQuadVector.size(); ++uckTop)
	{
		CSkyObjectQuad & aSkyObjectQuad = aFaceTop.m_SkyObjectQuadVector[uckTop];
		for (unsigned char v = 0; v < 4; ++v)
		{
			aSkyObjectQuad.SetSrcColor(v, srcZenith.r, srcZenith.g, srcZenith.b, srcZenith.a);
			aSkyObjectQuad.SetTransition(v, nextZenith.r, nextZenith.g, nextZenith.b, nextZenith.a, lTransitionTime);
		}
	}

	TSkyObjectFace & aFaceBottom = m_Faces[5];
	const TColor & srcGround = srcPoints.back();
	const TColor & nextGround = nextPoints.back();
	for (unsigned long uckBot = 0; uckBot < aFaceBottom.m_SkyObjectQuadVector.size(); ++uckBot)
	{
		CSkyObjectQuad & aSkyObjectQuad = aFaceBottom.m_SkyObjectQuadVector[uckBot];
		for (unsigned char v = 0; v < 4; ++v)
		{
			aSkyObjectQuad.SetSrcColor(v, srcGround.r, srcGround.g, srcGround.b, srcGround.a);
			aSkyObjectQuad.SetTransition(v, nextGround.r, nextGround.g, nextGround.b, nextGround.a, lTransitionTime);
		}
	}
}

void CSkyBox::StartTransition()
{
	m_bTransitionStarted = true;
	m_bGradientTransitionActive = true;
	m_dwGradientTransitionStart = DX::StepTimer::instance().GetTotalMillieSeconds();
	for (unsigned char ucj = 0; ucj < 6; ++ucj)
		m_Faces[ucj].StartTransition();
	m_FaceCloud.StartTransition();
}

void CSkyBox::Update()
{
	CSkyObject::Update();

	if (!m_gradientSrc.empty())
	{
		int count = min((int)m_gradientSrc.size(), 8);
		float blendFactor = 0.0f;

		if (m_bGradientTransitionActive && m_lGradientTransitionDuration > 0)
		{
			DWORD dwElapsed = DX::StepTimer::instance().GetTotalMillieSeconds() - m_dwGradientTransitionStart;
			blendFactor = min(1.0f, (float)dwElapsed / (float)m_lGradientTransitionDuration);
			if (blendFactor >= 1.0f)
				m_bGradientTransitionActive = false;
		}

		// Compute current blended gradient colors
		float colors[8 * 4];
		for (int i = 0; i < count; ++i)
		{
			const TColor& src = m_gradientSrc[i];
			const TColor& dst = (i < (int)m_gradientDst.size()) ? m_gradientDst[i] : src;
			colors[i * 4 + 0] = src.r + (dst.r - src.r) * blendFactor;
			colors[i * 4 + 1] = src.g + (dst.g - src.g) * blendFactor;
			colors[i * 4 + 2] = src.b + (dst.b - src.b) * blendFactor;
			colors[i * 4 + 3] = src.a + (dst.a - src.a) * blendFactor;
		}

		SHADERMANAGER.SetSkyGradient(colors, count, (int)m_ucOriginalGradientLevelUpper);
	}

	if (!m_bTransitionStarted)
		return;

	bool bResult = false;
	for (unsigned char uci = 0; uci < 6; ++uci)
 		bResult = m_Faces[uci].Update() || bResult;
 	bResult = m_FaceCloud.Update() || bResult;

	m_bTransitionStarted = bResult;
}

void CSkyBox::Render()
{
	SHADERMANAGER.PushState();

	SHADERMANAGER.SetPipelineState(PSTATE_DEPTHENABLE, TRUE);
	SHADERMANAGER.SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	SHADERMANAGER.SetLightingEnabled(false);
	SHADERMANAGER.SetFogEnabled(false);
	SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, FALSE);
	SHADERMANAGER.SetShaderResource(1, nullptr);
	SHADERMANAGER.SetMatrix(MATRIX_WORLD, &m_matWorld);

	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.BeginSky();
		SHADERMANAGER.SetWorldMatrix(&m_matWorld);
		SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
		SHADERMANAGER.SetSkyTint(0xFFFFFFFF);
		SHADERMANAGER.CommitChanges();
	}

	if (m_ucRenderMode == CSkyObject::SKY_RENDER_MODE_TEXTURE)
	{
		SHADERMANAGER.SetMaterialParams(0.0f, 0.0f, 0.0f, 1.0f);

		Matrix matTexIdentity;
		MatrixIdentity(&matTexIdentity);

		SHADERMANAGER.SetTextureMatrix(0, &matTexIdentity);
		SHADERMANAGER.CommitChanges();

		SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSU, ADDRESS_CLAMP);
		SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSV, ADDRESS_CLAMP);

		for (unsigned int i = 0; i < 6; ++i)
		{
			CGraphicImageInstance* pFaceImageInstance =
				m_GraphicImageInstanceMap[m_Faces[i].m_strFaceTextureFileName];

			if (!pFaceImageInstance)
				break;

			SHADERMANAGER.SetShaderResource(0, pFaceImageInstance->GetTextureReference().GetD3DTexture());
			m_Faces[i].Render();
		}
	}
	else
	{
		SHADERMANAGER.SetMaterialParams(0.0f, 0.0f, 0.0f, 0.0f);
		SHADERMANAGER.SetDefaultTexture(0);
		SHADERMANAGER.CommitChanges();

		for (unsigned int i = 0; i < 6; ++i)
			m_Faces[i].Render();
	}

	SHADERMANAGER.SetMaterialParams(0.0f, 0.0f, 0.0f, 0.0f);

#ifdef ENABLE_CELESTIAL_BODY
	{
		SHADERMANAGER.PushState();

		SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, TRUE);
		SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
		SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, BLEND_ONE);
		SHADERMANAGER.SetPipelineState(PSTATE_CULLMODE, CULL_NONE);

		Vector3 vLN = m_vLightDirection;
		float fLen = sqrtf(vLN.x * vLN.x + vLN.y * vLN.y + vLN.z * vLN.z);

		if (fLen >= 0.001f)
		{
			vLN.x /= fLen;
			vLN.y /= fLen;
			vLN.z /= fLen;

			float fBaseAngle = atan2f(-vLN.y, -vLN.x);

			DWORD dwTime = DX::StepTimer::instance().GetTotalMillieSeconds();
			float fTimeSec = (float)dwTime / 1000.0f;
			const float fOrbitSpeed = 6.2831853f / 1200.0f;
			float fTimeAngle = fTimeSec * fOrbitSpeed;

			float fAngle = fBaseAngle + fTimeAngle;

			if (m_bIsNight)
				fAngle += 3.14159265f;

			float fCosA = cosf(fAngle);
			float fSinA = sinf(fAngle);

			const float fElevBase = 0.10f;
			const float fElevAmp = 0.04f;
			const float fBodyDist = 0.88f;

			float fBodyZ = fElevBase + fElevAmp * sinf(fTimeAngle * 2.0f);
			Vector3 vBodyPos = { fCosA * fBodyDist, fSinA * fBodyDist, fBodyZ };

			Vector3 vToBody = vBodyPos;
			Vec3Normalize(&vToBody, &vToBody);

			Vector3 vWorldUp(0.0f, 0.0f, 1.0f);
			Vector3 vRight, vUp;

			Vec3Cross(&vRight, &vToBody, &vWorldUp);
			Vec3Normalize(&vRight, &vRight);
			Vec3Cross(&vUp, &vRight, &vToBody);
			Vec3Normalize(&vUp, &vUp);

			CGraphicImageInstance* pTexInst =
				m_bIsNight ? m_pMoonTextureInstance : m_pSunTextureInstance;

			bool bHasTex = false;

			if (pTexInst && !pTexInst->IsEmpty())
			{
				ID3D11ShaderResourceView* pSRV =
					pTexInst->GetTextureReference().GetD3DTexture();

				if (pSRV)
				{
					SHADERMANAGER.SetShaderResource(0, pSRV);
					SHADERMANAGER.SetMaterialParams(0.0f, 0.0f, 0.0f, 0.3f);
					bHasTex = true;
				}
			}

			if (!bHasTex)
			{
				SHADERMANAGER.SetDefaultTexture(0);
				SHADERMANAGER.SetMaterialParams(0.0f, 0.0f, 0.0f, 0.0f);
			}

			if (m_bIsNight)
			{
				SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
				SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, BLEND_INVSRCALPHA);
			}

			SHADERMANAGER.CommitChanges();

			DWORD dwBodyColor = m_bIsNight ? 0xFFC0C8D8 : 0xFFFFC850;
			float fSize = m_bIsNight ? m_fMoonSize : m_fSunSize;

			TPDTVertex bodyVerts[4];

			bodyVerts[0].position.x = vBodyPos.x + (-vRight.x + vUp.x) * fSize;
			bodyVerts[0].position.y = vBodyPos.y + (-vRight.y + vUp.y) * fSize;
			bodyVerts[0].position.z = vBodyPos.z + (-vRight.z + vUp.z) * fSize;
			bodyVerts[0].diffuse = dwBodyColor;
			bodyVerts[0].texCoord.x = 0.0f; bodyVerts[0].texCoord.y = 0.0f;

			bodyVerts[1].position.x = vBodyPos.x + (vRight.x + vUp.x) * fSize;
			bodyVerts[1].position.y = vBodyPos.y + (vRight.y + vUp.y) * fSize;
			bodyVerts[1].position.z = vBodyPos.z + (vRight.z + vUp.z) * fSize;
			bodyVerts[1].diffuse = dwBodyColor;
			bodyVerts[1].texCoord.x = 1.0f; bodyVerts[1].texCoord.y = 0.0f;

			bodyVerts[2].position.x = vBodyPos.x + (-vRight.x - vUp.x) * fSize;
			bodyVerts[2].position.y = vBodyPos.y + (-vRight.y - vUp.y) * fSize;
			bodyVerts[2].position.z = vBodyPos.z + (-vRight.z - vUp.z) * fSize;
			bodyVerts[2].diffuse = dwBodyColor;
			bodyVerts[2].texCoord.x = 0.0f; bodyVerts[2].texCoord.y = 1.0f;

			bodyVerts[3].position.x = vBodyPos.x + (vRight.x - vUp.x) * fSize;
			bodyVerts[3].position.y = vBodyPos.y + (vRight.y - vUp.y) * fSize;
			bodyVerts[3].position.z = vBodyPos.z + (vRight.z - vUp.z) * fSize;
			bodyVerts[3].diffuse = dwBodyColor;
			bodyVerts[3].texCoord.x = 1.0f; bodyVerts[3].texCoord.y = 1.0f;

			if (CGraphicBase::SetPDTStream(bodyVerts, 4))
				SHADERMANAGER.Draw(TOPOLOGY_TRIANGLESTRIP, 0, 2);
		}

		SHADERMANAGER.PopState();
	}
#endif

	SHADERMANAGER.PopState();
}

void CSkyBox::RenderCloud()
{
	CGraphicImageInstance* pCloudGraphicImageInstance =
		m_GraphicImageInstanceMap[m_FaceCloud.m_strfacename];

	if (!pCloudGraphicImageInstance)
		return;

	SHADERMANAGER.PushState();

	SHADERMANAGER.SetPipelineState(PSTATE_DEPTHENABLE, TRUE);
	SHADERMANAGER.SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	SHADERMANAGER.SetLightingEnabled(false);
	SHADERMANAGER.SetFogEnabled(false);
	SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, TRUE);
	SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND, BLEND_ONE);
	SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, BLEND_INVSRCCOLOR);

	m_matTextureCloud._41 = m_fCloudPositionU;
	m_matTextureCloud._42 = m_fCloudPositionV;

	DWORD dwCurTime = DX::StepTimer::instance().GetTotalMillieSeconds();

	m_fCloudPositionU += m_fCloudScrollSpeedU * (float)(dwCurTime - m_dwlastTime) * 0.001f;
	if (m_fCloudPositionU >= 1.0f)
		m_fCloudPositionU = 0.0f;

	m_fCloudPositionV += m_fCloudScrollSpeedV * (float)(dwCurTime - m_dwlastTime) * 0.001f;
	if (m_fCloudPositionV >= 1.0f)
		m_fCloudPositionV = 0.0f;

	m_dwlastTime = dwCurTime;

	SHADERMANAGER.SetMatrix(MATRIX_TEXTURE0, &m_matTextureCloud);

	Matrix matProjCloud;
	const D3D11_VIEWPORT& viewport = CGraphicBase::GetViewport();
	float fAspect = viewport.Height > 0 ? viewport.Width / viewport.Height : 1.33333f;

	MatrixPerspectiveFovRH(&matProjCloud, MATH_PI * 0.25f, fAspect, 1.0f, 999999.0f);

	SHADERMANAGER.SetMatrix(MATRIX_WORLD, &m_matWorldCloud);
	SHADERMANAGER.SetMatrix(MATRIX_PROJECTION, &matProjCloud);

	CGraphicTexture* pCloudTexture = pCloudGraphicImageInstance->GetTexturePointer();

	if (!pCloudTexture)
	{
		SHADERMANAGER.PopState();
		return;
	}

	SHADERMANAGER.SetShaderResource(0, pCloudTexture->GetD3DTexture());

	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.BeginSky();
		SHADERMANAGER.SetWorldMatrix(&m_matWorldCloud);
		SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
		SHADERMANAGER.SetSkyTint(0xFFFFFFFF);
		SHADERMANAGER.SetMaterialParams(0.0f, 0.0f, 0.0f, 1.0f);
		SHADERMANAGER.CommitChanges();
	}

	m_FaceCloud.Render();

	SHADERMANAGER.PopState();
}

#ifdef ENABLE_CELESTIAL_BODY
void CSkyBox::SetCelestialBodyTexture(const char* szSunTexture, const char* szMoonTexture)
{
	if (m_pSunTextureInstance)
	{
		DeleteTexture(m_pSunTextureInstance);
		m_pSunTextureInstance = nullptr;
	}

	if (m_pMoonTextureInstance)
	{
		DeleteTexture(m_pMoonTextureInstance);
		m_pMoonTextureInstance = nullptr;
	}

	m_bCelestialReady = false;

	if (szSunTexture && strlen(szSunTexture) > 0)
		m_pSunTextureInstance = GenerateTexture(szSunTexture);

	if (szMoonTexture && strlen(szMoonTexture) > 0)
		m_pMoonTextureInstance = GenerateTexture(szMoonTexture);

	m_bCelestialReady =
		m_pSunTextureInstance != nullptr ||
		m_pMoonTextureInstance != nullptr;
}

void CSkyBox::SetLightDirection(const Vector3& vLightDir, bool bIsNight)
{
	m_vLightDirection = vLightDir;
	m_bIsNight = bIsNight;
}
#endif
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
