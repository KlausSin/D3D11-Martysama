#include "StdAfx.h"
#include "Material.h"
#include "Mesh.h"
#include "../eterbase/Filename.h"
#include "../eterbase/Timer.h"
#include "../eterlib/ResourceManager.h"
#include "../eterlib/ShaderManager.h"
#include "../eterlib/GrpScreen.h"
#include "../eterPack/EterPackManager.h"
#include <map>

CGraphicImageInstance CGrannyMaterial::ms_akSphereMapInstance[SPHEREMAP_NUM];

Vector3	CGrannyMaterial::ms_v3SpecularTrans(0.0f, 0.0f, 0.0f);
Matrix	CGrannyMaterial::ms_matSpecular;

thread_local ID3D11ShaderResourceView* CGrannyMaterial::ms_pLastTexture[2] = { nullptr, nullptr };
thread_local CGrannyMaterial* CGrannyMaterial::ms_pLastMaterial = nullptr;
thread_local bool CGrannyMaterial::ms_bSpecularStateApplied = false;
thread_local bool CGrannyMaterial::ms_bTwoSideStateApplied = false;

Color g_fSpecularColor = Color(1.0f, 1.0f, 1.0f, 1.0f);

void CGrannyMaterial::ResetRenderStateCache()
{
	ms_pLastTexture[0] = nullptr;
	ms_pLastTexture[1] = nullptr;
	ms_pLastMaterial = nullptr;
	ms_bSpecularStateApplied = false;
	ms_bTwoSideStateApplied = false;
}

void CGrannyMaterial::RestoreRenderStateCache()
{
	if (ms_bSpecularStateApplied)
	{
		SHADERMANAGER.RestoreSamplerState(1, SAMPLER_ADDRESSU);
		SHADERMANAGER.RestoreSamplerState(1, SAMPLER_ADDRESSV);

		ms_bSpecularStateApplied = false;
	}

	// Restore two-sided state if it was applied
	if (ms_bTwoSideStateApplied)
	{
		SHADERMANAGER.SetPipelineState(PSTATE_CULLMODE, CULL_FRONT);
		ms_bTwoSideStateApplied = false;
	}

	// Clear texture cache
	ms_pLastTexture[0] = nullptr;
	ms_pLastTexture[1] = nullptr;
}

void CGrannyMaterial::TranslateSpecularMatrix(float fAddX, float fAddY, float fAddZ)
{
	static float SPECULAR_TRANSLATE_MAX = 1000000.0f;

	ms_v3SpecularTrans.x+=fAddX;
	ms_v3SpecularTrans.y+=fAddY;
	ms_v3SpecularTrans.z+=fAddZ;

	if (ms_v3SpecularTrans.x>=SPECULAR_TRANSLATE_MAX)
		ms_v3SpecularTrans.x=0.0f;

	if (ms_v3SpecularTrans.y>=SPECULAR_TRANSLATE_MAX)
		ms_v3SpecularTrans.y=0.0f;

	if (ms_v3SpecularTrans.z>=SPECULAR_TRANSLATE_MAX)
		ms_v3SpecularTrans.z=0.0f;

	MatrixTranslation(&ms_matSpecular,
		ms_v3SpecularTrans.x,
		ms_v3SpecularTrans.y,
		ms_v3SpecularTrans.z
	);
}

void CGrannyMaterial::ApplyRenderState()
{
	assert(m_pfnApplyRenderState!=NULL && "CGrannyMaterial::SavePipelineState");
	(this->*m_pfnApplyRenderState)();
}

void CGrannyMaterial::RestorePipelineState()
{
	assert(m_pfnRestoreRenderState!=NULL && "CGrannyMaterial::RestorePipelineState");
	(this->*m_pfnRestoreRenderState)();
}

void CGrannyMaterial::Copy(CGrannyMaterial& rkMtrl)
{
	m_pgrnMaterial = rkMtrl.m_pgrnMaterial;
	m_roImage[0] =  rkMtrl.m_roImage[0];
	m_roImage[1] =  rkMtrl.m_roImage[1];
    m_eType = rkMtrl.m_eType;
}

CGrannyMaterial::CGrannyMaterial()
{
	m_bTwoSideRender = false;
	m_dwLastCullRenderStateForTwoSideRendering = CULL_FRONT;

	Initialize();
}

CGrannyMaterial::~CGrannyMaterial()
{
}

CGrannyMaterial::EType CGrannyMaterial::GetType() const
{
	return m_eType;
}

void CGrannyMaterial::SetImagePointer(int iStage, CGraphicImage* pImage)
{
	assert(iStage<2 && "CGrannyMaterial::SetImagePointer");
	m_roImage[iStage]=pImage;
}

bool CGrannyMaterial::IsIn(const char* c_szImageName, int* piStage)
{
	std::string strImageName = c_szImageName;
	CFileNameHelper::StringPath(strImageName);

	granny_texture * pgrnDiffuseTexture = GrannyGetMaterialTextureByType(m_pgrnMaterial, GrannyDiffuseColorTexture);
	if (pgrnDiffuseTexture)
	{
		std::string strDiffuseFileName = pgrnDiffuseTexture->FromFileName;
		CFileNameHelper::StringPath(strDiffuseFileName);
		if (strDiffuseFileName == strImageName)
		{
			*piStage=0;
			return true;
		}
	}

    granny_texture * pgrnOpacityTexture = GrannyGetMaterialTextureByType(m_pgrnMaterial, GrannyOpacityTexture);
	if (pgrnOpacityTexture)
	{
		std::string strOpacityFileName = pgrnOpacityTexture->FromFileName;
		CFileNameHelper::StringPath(strOpacityFileName);
		if (strOpacityFileName == strImageName)
		{
			*piStage=1;
			return true;
		}
	}

	return false;
}

void CGrannyMaterial::SetSpecularInfo(BOOL bFlag, float fPower, BYTE uSphereMapIndex)
{
	m_fSpecularPower = fPower;
	m_bSphereMapIndex = uSphereMapIndex;
	m_bSpecularEnable = bFlag;

	if (bFlag)
	{
		m_pfnApplyRenderState = &CGrannyMaterial::__ApplySpecularRenderState;
		m_pfnRestoreRenderState = &CGrannyMaterial::__RestoreSpecularRenderState;
	}
	else
	{
		m_pfnApplyRenderState = &CGrannyMaterial::__ApplyDiffuseRenderState;
		m_pfnRestoreRenderState = &CGrannyMaterial::__RestoreDiffuseRenderState;
	}
}

bool CGrannyMaterial::IsEqual(granny_material* pgrnMaterial) const
{
	if (m_pgrnMaterial==pgrnMaterial)
		return true;

	return false;
}

ID3D11ShaderResourceView* CGrannyMaterial::GetD3DTexture(int iStage) const
{
	const CGraphicImage::TRef & ratImage = m_roImage[iStage];

	if (ratImage.IsNull())
		return nullptr;

	CGraphicImage * pImage = ratImage.GetPointer();
	const CGraphicTexture * pTexture = pImage->GetTexturePointer();
	return pTexture->GetD3DTexture();
}

bool CGrannyMaterial::IsTextureLoaded() const
{
	const CGraphicImage::TRef & ratImage = m_roImage[0];

	if (ratImage.IsNull())
		return false;

	CGraphicImage * pImage = ratImage.GetPointer();
	if (!pImage)
		return false;

	if (pImage->IsEmpty())
		return false;

	// Double-check the D3D texture pointer exists
	const CGraphicTexture * pTexture = pImage->GetTexturePointer();
	if (!pTexture || pTexture->IsEmpty())
		return false;

	return (pTexture->GetD3DTexture() != NULL);
}

CGraphicImage * CGrannyMaterial::GetImagePointer(int iStage) const
{
	const CGraphicImage::TRef & ratImage = m_roImage[iStage];

	if (ratImage.IsNull())
		return NULL;

	CGraphicImage * pImage = ratImage.GetPointer();
	return pImage;
}

const CGraphicTexture* CGrannyMaterial::GetDiffuseTexture() const
{
	if (m_roImage[0].IsNull())
		return NULL;

	return m_roImage[0].GetPointer()->GetTexturePointer();
}

const CGraphicTexture* CGrannyMaterial::GetOpacityTexture() const
{
	if (m_roImage[1].IsNull())
		return NULL;

	return m_roImage[1].GetPointer()->GetTexturePointer();
}

BOOL CGrannyMaterial::__IsSpecularEnable() const
{
	return m_bSpecularEnable;
}

float CGrannyMaterial::__GetSpecularPower() const
{
	return m_fSpecularPower;
}

extern const std::string& GetModelLocalPath();

CGraphicImage* CGrannyMaterial::__GetImagePointer(const char* fileName)
{
	assert(*fileName != '\0');

	CResourceManager& rkResMgr = CResourceManager::Instance();

	// SUPPORT_LOCAL_TEXTURE
	int fileName_len = (int)(strlen(fileName));
	if (fileName_len > 2 && fileName[1] != ':')
	{
		char localFileName[256];
		const std::string& modelLocalPath = GetModelLocalPath();

		int localFileName_len = (int)(modelLocalPath.length() + 1 + fileName_len);
		if (localFileName_len < sizeof(localFileName) - 1)
		{
			_snprintf(localFileName, sizeof(localFileName), "%s%s", GetModelLocalPath().c_str(), fileName);
			CResource* pResource = rkResMgr.GetResourcePointer(localFileName);
			return static_cast<CGraphicImage*>(pResource);
		}
	}
	// END_OF_SUPPORT_LOCAL_TEXTURE

	CResource* pResource = rkResMgr.GetResourcePointer(fileName);
	return static_cast<CGraphicImage*>(pResource);
}

bool CGrannyMaterial::CreateFromGrannyMaterialPointer(granny_material * pgrnMaterial)
{
	m_pgrnMaterial = pgrnMaterial;

	granny_texture * pgrnDiffuseTexture = NULL;
	granny_texture * pgrnOpacityTexture = NULL;

	if (pgrnMaterial)
	{
		if (pgrnMaterial->MapCount > 1 && !_strnicmp(pgrnMaterial->Name, "Blend", 5))
		{
			pgrnDiffuseTexture = GrannyGetMaterialTextureByType(pgrnMaterial->Maps[0].Material, GrannyDiffuseColorTexture);
			pgrnOpacityTexture = GrannyGetMaterialTextureByType(pgrnMaterial->Maps[1].Material, GrannyDiffuseColorTexture);
		}
		else
		{
			pgrnDiffuseTexture = GrannyGetMaterialTextureByType(m_pgrnMaterial, GrannyDiffuseColorTexture);
			pgrnOpacityTexture = GrannyGetMaterialTextureByType(m_pgrnMaterial, GrannyOpacityTexture);
		}

		{
			granny_int32 twoSided = 0;
			granny_data_type_definition TwoSidedFieldType[] =
			{
				{GrannyInt32Member, "Two-sided"},
				{GrannyEndMember},
			};
#if GrannyProductMinorVersion==4
			granny_variant twoSideResult = GrannyFindMatchingMember(pgrnMaterial->ExtendedData.Type, pgrnMaterial->ExtendedData.Object, "Two-sided");

			if (NULL != twoSideResult.Type)
				GrannyConvertSingleObject(twoSideResult.Type, twoSideResult.Object, TwoSidedFieldType, &twoSided);
#elif GrannyProductMinorVersion==7
			granny_variant twoSideResult;
			bool findMatchResult = GrannyFindMatchingMember(pgrnMaterial->ExtendedData.Type, pgrnMaterial->ExtendedData.Object, "Two-sided", &twoSideResult);
			if (NULL != twoSideResult.Type && findMatchResult)
				GrannyConvertSingleObject(twoSideResult.Type, twoSideResult.Object, TwoSidedFieldType, &twoSided);
#elif GrannyProductMinorVersion==11 || GrannyProductMinorVersion==9 || GrannyProductMinorVersion==8
			granny_variant twoSideResult;
			bool gfmm_bool = GrannyFindMatchingMember(pgrnMaterial->ExtendedData.Type, pgrnMaterial->ExtendedData.Object, "Two-sided", &twoSideResult);
			if (NULL != twoSideResult.Type)
				GrannyConvertSingleObject(twoSideResult.Type, twoSideResult.Object, TwoSidedFieldType, &twoSided, 0);
#else
#error "unknown granny version"
#endif

			m_bTwoSideRender = 1 == twoSided;
		}
	}

	if (pgrnDiffuseTexture)
		m_roImage[0].SetPointer(__GetImagePointer(pgrnDiffuseTexture->FromFileName));

	if (pgrnOpacityTexture)
		m_roImage[1].SetPointer(__GetImagePointer(pgrnOpacityTexture->FromFileName));

	bool bHasOpacityMap = false;
	if (!m_roImage[1].IsNull())
	{
		const char * c_szOpacityFileName = m_roImage[1].GetPointer()->GetFileName();
		if (c_szOpacityFileName && '\0' != *c_szOpacityFileName)
			bHasOpacityMap = CEterPackManager::Instance().isExist(c_szOpacityFileName);

		if (!bHasOpacityMap)
			m_roImage[1] = NULL;
	}

	m_eType = bHasOpacityMap ? TYPE_BLEND_PNT : TYPE_DIFFUSE_PNT;

	return true;
}

void CGrannyMaterial::Initialize()
{
	m_roImage[0] = NULL;
	m_roImage[1] = NULL;

	SetSpecularInfo(FALSE, 0.0f, 0);
}

void CGrannyMaterial::__ApplyDiffuseRenderState()
{
	ID3D11ShaderResourceView* pTexture = GetD3DTexture(0);

	SHADERMANAGER.SetShaderResource(0, pTexture);

	if (SHADERMANAGER.IsInitialized())
		SHADERMANAGER.SetSpecularColor(0.0f, 0.0f, 0.0f);

	if (m_bTwoSideRender)
	{
		m_dwLastCullRenderStateForTwoSideRendering = SHADERMANAGER.GetPipelineState(PSTATE_CULLMODE);
		SHADERMANAGER.SetPipelineState(PSTATE_CULLMODE, CULL_NONE);
	}

}

void CGrannyMaterial::__RestoreDiffuseRenderState()
{
	if (m_bTwoSideRender)
	{
		SHADERMANAGER.SetPipelineState(PSTATE_CULLMODE, m_dwLastCullRenderStateForTwoSideRendering);
	}
}

void CGrannyMaterial::__ApplySpecularRenderState()
{
	if (TRUE == SHADERMANAGER.GetPipelineState(PSTATE_BLENDENABLE))
	{
		__ApplyDiffuseRenderState();
		return;
	}

	ID3D11ShaderResourceView* pTexture = GetD3DTexture(0);
	if (!pTexture && !m_roImage[0].IsNull())
	{
		CGraphicImage* pImage = m_roImage[0].GetPointer();
		if (pImage)
		{
			static std::map<CGraphicImage*, DWORD> s_mapLastReloadTimeSpec;
			DWORD dwCurrentTime = ELTimer_GetMSec();
			DWORD dwLastReload = 0;

			auto it = s_mapLastReloadTimeSpec.find(pImage);
			if (it != s_mapLastReloadTimeSpec.end())
				dwLastReload = it->second;

			if (dwLastReload == 0)
			{
				s_mapLastReloadTimeSpec[pImage] = dwCurrentTime;
				pImage->Reload();
				pTexture = GetD3DTexture(0);
			}
		}
	}
	SHADERMANAGER.SetShaderResource(0, pTexture);

	if (SHADERMANAGER.IsInitialized())
	{
		CGraphicTexture* pkSphere = ms_akSphereMapInstance[m_bSphereMapIndex].GetTexturePointer();
		SHADERMANAGER.SetShaderResource(6, pkSphere ? pkSphere->GetD3DTexture() : NULL);

		SHADERMANAGER.SetSpecularColor(g_fSpecularColor.r, g_fSpecularColor.g, g_fSpecularColor.b);
		SHADERMANAGER.SetSpecularPower(__GetSpecularPower());

		static const float s_fSpecIntensity = 1.5f;
		static const float s_fSpecPower = 64.0f;
		SHADERMANAGER.SetSpecularTune(s_fSpecIntensity, s_fSpecPower);
	}


	if (m_bTwoSideRender)
	{
		m_dwLastCullRenderStateForTwoSideRendering = SHADERMANAGER.GetPipelineState(PSTATE_CULLMODE);
		SHADERMANAGER.SetPipelineState(PSTATE_CULLMODE, CULL_NONE);
	}
}

void CGrannyMaterial::__RestoreSpecularRenderState()
{
	if (TRUE == SHADERMANAGER.GetPipelineState(PSTATE_BLENDENABLE))
	{
		__RestoreDiffuseRenderState();
		return;
	}


	if (m_bTwoSideRender)
	{
		SHADERMANAGER.SetPipelineState(PSTATE_CULLMODE, m_dwLastCullRenderStateForTwoSideRendering);
	}
}

void CGrannyMaterial::CreateSphereMap(UINT uMapIndex, const char* c_szSphereMapImageFileName)
{
	CResourceManager& rkResMgr = CResourceManager::Instance();
	CGraphicImage * pImage = (CGraphicImage *)rkResMgr.GetResourcePointer(c_szSphereMapImageFileName);
	ms_akSphereMapInstance[uMapIndex].SetImagePointer(pImage);
}

void CGrannyMaterial::DestroySphereMap()
{
	for (UINT uMapIndex=0; uMapIndex<SPHEREMAP_NUM; ++uMapIndex)
		ms_akSphereMapInstance[uMapIndex].Destroy();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CGrannyMaterialPalette::CGrannyMaterialPalette()
{
}

CGrannyMaterialPalette::~CGrannyMaterialPalette()
{
	Clear();
}

void CGrannyMaterialPalette::Copy(const CGrannyMaterialPalette& rkMtrlPalSrc)
{
	m_mtrlVector=rkMtrlPalSrc.m_mtrlVector;
}

void CGrannyMaterialPalette::Clear()
{
	m_mtrlVector.clear();
}

CGrannyMaterial& CGrannyMaterialPalette::GetMaterialRef(DWORD mtrlIndex)
{
	assert(mtrlIndex<m_mtrlVector.size());
	return *m_mtrlVector[mtrlIndex].GetPointer();
}

void CGrannyMaterialPalette::SetMaterialImagePointer(const char* c_szImageName, CGraphicImage* pImage)
{
	DWORD size=(DWORD)(m_mtrlVector.size());
	DWORD i;
	for (i=0; i<size; ++i)
	{
		CGrannyMaterial::TRef& roMtrl=m_mtrlVector[i];

		int iStage;
		if (roMtrl->IsIn(c_szImageName, &iStage))
		{
			CGrannyMaterial* pkNewMtrl=new CGrannyMaterial;
			pkNewMtrl->Copy(*roMtrl.GetPointer());
			pkNewMtrl->SetImagePointer(iStage, pImage);
			roMtrl=pkNewMtrl;

			return;
		}
	}
}

void CGrannyMaterialPalette::SetMaterialData(const char* c_szMtrlName, const SMaterialData& c_rkMaterialData)
{
	if (c_szMtrlName)
	{
		std::vector<CGrannyMaterial::TRef>::iterator i;
		for (i=m_mtrlVector.begin(); i!=m_mtrlVector.end(); ++i)
		{
			CGrannyMaterial::TRef& roMtrl=*i;

			int iStage;
			if (roMtrl->IsIn(c_szMtrlName, &iStage))
			{
				CGrannyMaterial* pkNewMtrl=new CGrannyMaterial;
				pkNewMtrl->Copy(*roMtrl.GetPointer());
				pkNewMtrl->SetImagePointer(iStage, c_rkMaterialData.pImage);
				pkNewMtrl->SetSpecularInfo(c_rkMaterialData.isSpecularEnable, c_rkMaterialData.fSpecularPower, c_rkMaterialData.bSphereMapIndex);
				roMtrl=pkNewMtrl;

				return;
			}
		}
	}
	else
	{
		std::vector<CGrannyMaterial::TRef>::iterator i;
		for (i=m_mtrlVector.begin(); i!=m_mtrlVector.end(); ++i)
		{
			CGrannyMaterial::TRef& roMtrl=*i;
			roMtrl->SetSpecularInfo(c_rkMaterialData.isSpecularEnable, c_rkMaterialData.fSpecularPower, c_rkMaterialData.bSphereMapIndex);
		}
	}
}

void CGrannyMaterialPalette::SetSpecularInfo(const char* c_szMtrlName, BOOL bEnable, float fPower)
{
	DWORD size=(DWORD)(m_mtrlVector.size());
	DWORD i;
	if (c_szMtrlName)
	{
		for (i=0; i<size; ++i)
		{
			CGrannyMaterial::TRef& roMtrl=m_mtrlVector[i];

			int iStage;
			if (roMtrl->IsIn(c_szMtrlName, &iStage))
			{
				roMtrl->SetSpecularInfo(bEnable, fPower, 0);
				return;
			}
		}
	}
	else
	{
		for (i=0; i<size; ++i)
		{
			CGrannyMaterial::TRef& roMtrl=m_mtrlVector[i];
			roMtrl->SetSpecularInfo(bEnable, fPower, 0);
		}
	}
}

DWORD CGrannyMaterialPalette::RegisterMaterial(granny_material* pgrnMaterial)
{
	DWORD size=(DWORD)(m_mtrlVector.size());
	DWORD i;
	for (i=0; i<size; ++i)
	{
		CGrannyMaterial::TRef& roMtrl=m_mtrlVector[i];
		if (roMtrl->IsEqual(pgrnMaterial))
			return i;
	}

	CGrannyMaterial* pkNewMtrl=new CGrannyMaterial;
	pkNewMtrl->CreateFromGrannyMaterialPointer(pgrnMaterial);
	m_mtrlVector.push_back(pkNewMtrl);

	return size;
}

DWORD CGrannyMaterialPalette::GetMaterialCount() const
{
	return (DWORD)(m_mtrlVector.size());
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
