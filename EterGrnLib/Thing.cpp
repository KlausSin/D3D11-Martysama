#include "StdAfx.h"
#include "../eterbase/Debug.h"
#include "Thing.h"
#include "ThingInstance.h"
#include <unordered_map>
#include <algorithm>

static std::string NormalizeCacheKey(const std::string& fileName)
{
	std::string normalized = fileName;
	std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
	return normalized;
}

struct SThingModelCache
{
	CGrannyModel* models;
	int modelCount;
	int refCount;
	bool deviceObjectsCreated;
};

static std::unordered_map<std::string, SThingModelCache> g_ThingModelCache;

// Returns: 0 = error, 1 = created new, 2 = returned cached
int LoadThingModelsCached(const std::string& fileName, granny_file_info* pInfo, CGrannyModel*& outModels)
{
	std::string cacheKey = NormalizeCacheKey(fileName);
	auto it = g_ThingModelCache.find(cacheKey);

	if (it != g_ThingModelCache.end())
	{
		it->second.refCount++;
		outModels = it->second.models;
		return 2; // Cached
	}

	int modelCount = pInfo->ModelCount;
	CGrannyModel* models = new CGrannyModel[modelCount];

	for (int m = 0; m < modelCount; ++m)
	{
		if (!models[m].CreateFromGrannyModelPointer(pInfo->Models[m]))
		{
			delete[] models;
			return 0; // Error
		}
	}

	g_ThingModelCache[cacheKey] = { models, modelCount, 1, false };
	outModels = models;
	return 1; // Created new
}

void ReleaseThingModelsCached(const std::string& fileName)
{
	std::string cacheKey = NormalizeCacheKey(fileName);
	auto it = g_ThingModelCache.find(cacheKey);
	if (it == g_ThingModelCache.end())
		return;

	it->second.refCount--;

	if (it->second.refCount <= 0)
	{
		if (it->second.deviceObjectsCreated)
		{
			for (int m = 0; m < it->second.modelCount; ++m)
				it->second.models[m].DestroyDeviceObjects();
		}

		delete[] it->second.models;
		g_ThingModelCache.erase(it);
	}
}

struct SThingMotionCache
{
	CGrannyMotion* motions;
	int motionCount;
	int refCount;
};

static std::unordered_map<std::string, SThingMotionCache> g_ThingMotionCache;

bool LoadThingMotionsCached(const std::string& fileName, granny_file_info* pInfo, CGrannyMotion*& outMotions)
{
	std::string cacheKey = NormalizeCacheKey(fileName);
	auto it = g_ThingMotionCache.find(cacheKey);

	if (it != g_ThingMotionCache.end())
	{
		it->second.refCount++;
		outMotions = it->second.motions;
		return true;
	}

	int motionCount = pInfo->AnimationCount;
	CGrannyMotion* motions = new CGrannyMotion[motionCount];

	for (int m = 0; m < motionCount; ++m)
	{
		if (!motions[m].BindGrannyAnimation(pInfo->Animations[m]))
		{
			delete[] motions;
			return false;
		}
	}

	g_ThingMotionCache[cacheKey] = { motions, motionCount, 1 };
	outMotions = motions;
	return true;
}

void ReleaseThingMotionsCached(const std::string& fileName)
{
	std::string cacheKey = NormalizeCacheKey(fileName);
	auto it = g_ThingMotionCache.find(cacheKey);
	if (it == g_ThingMotionCache.end())
		return;

	it->second.refCount--;

	if (it->second.refCount <= 0)
	{
		delete[] it->second.motions;
		g_ThingMotionCache.erase(it);
	}
}

CGraphicThing::CGraphicThing(const char* c_szFileName) : CResource(c_szFileName)
{
	Initialize();
}

CGraphicThing::~CGraphicThing()
{
	//OnClear();
	Clear();
}

void CGraphicThing::Initialize()
{
	m_pgrnFile = NULL;
	m_pgrnFileInfo = NULL;
	m_pgrnAni = NULL;

	m_models = NULL;
	m_motions = NULL;

#if defined(__BL_GR2_FILE_LOAD_IMPROVE__)
	m_bIsCached = false;
#endif
}

void CGraphicThing::OnClear()
{
	const std::string& fileName = GetFileNameString();

	if (m_motions)
	{
		ReleaseThingMotionsCached(fileName);
		m_motions = nullptr;
	}

	if (m_models)
	{
		ReleaseThingModelsCached(fileName);
		m_models = nullptr;
	}

#if defined(__BL_GR2_FILE_LOAD_IMPROVE__)
	if (m_pgrnFile && !m_bIsCached)
#else
	if (m_pgrnFile)
#endif
		GrannyFreeFile(m_pgrnFile);

	m_pgrnFileInfo = nullptr;
	m_pgrnAni = nullptr;
	m_pgrnFile = nullptr;

#if defined(__BL_GR2_FILE_LOAD_IMPROVE__)
	m_bIsCached = false;
#endif
}

CGraphicThing::TType CGraphicThing::Type()
{
	static TType s_type = StringToType("CGraphicThing");
	return s_type;
}

bool CGraphicThing::OnIsEmpty() const
{
	return m_pgrnFile ? false : true;
}

bool CGraphicThing::OnIsType(TType type)
{
	if (CGraphicThing::Type() == type)
		return true;

	return CResource::OnIsType(type);
}

bool CGraphicThing::CreateDeviceObjects()
{
	if (!m_pgrnFileInfo || !m_models)
		return true;

	std::string cacheKey = NormalizeCacheKey(GetFileNameString());
	auto it = g_ThingModelCache.find(cacheKey);
	if (it == g_ThingModelCache.end())
		return true;

	if (it->second.deviceObjectsCreated)
		return true;

	for (int m = 0; m < it->second.modelCount; ++m)
		it->second.models[m].CreateDeviceObjects();

	it->second.deviceObjectsCreated = true;
	return true;
}

void CGraphicThing::DestroyDeviceObjects()
{
}

bool CGraphicThing::CheckModelIndex(int iModel) const
{
	if (!m_pgrnFileInfo)
	{
		Tracef("m_pgrnFileInfo == NULL: %s\n", GetFileName());
		return false;
	}

	assert(m_pgrnFileInfo != NULL);

	if (iModel < 0)
		return false;

	if (iModel >= m_pgrnFileInfo->ModelCount)
		return false;

	return true;
}

bool CGraphicThing::CheckMotionIndex(int iMotion) const
{
	// Temporary
	if (!m_pgrnFileInfo)
		return false;
	// Temporary

	assert(m_pgrnFileInfo != NULL);

	if (iMotion < 0)
		return false;

	if (iMotion >= m_pgrnFileInfo->AnimationCount)
		return false;

	return true;
}

CGrannyModel * CGraphicThing::GetModelPointer(int iModel)
{
	assert(CheckModelIndex(iModel));
	assert(m_models != NULL);
	return m_models + iModel;
}

CGrannyMotion * CGraphicThing::GetMotionPointer(int iMotion)
{
	assert(CheckMotionIndex(iMotion));

	if (iMotion >= m_pgrnFileInfo->AnimationCount)
		return NULL;

	assert(m_motions != NULL);
	return (m_motions + iMotion);
}

int CGraphicThing::GetModelCount() const
{
	if (!m_pgrnFileInfo)
		return 0;

	return (m_pgrnFileInfo->ModelCount);
}

int CGraphicThing::GetMotionCount() const
{
	if (!m_pgrnFileInfo)
		return 0;

	return (m_pgrnFileInfo->AnimationCount);
}

bool CGraphicThing::OnLoad(int iSize, const void * c_pvBuf)
{
	if (!c_pvBuf)
		return false;

	m_pgrnFile = GrannyReadEntireFileFromMemory(iSize, (void *) c_pvBuf);

	if (!m_pgrnFile)
		return false;

    m_pgrnFileInfo = GrannyGetFileInfo(m_pgrnFile);

	if (!m_pgrnFileInfo)
		return false;

	LoadModels();
	LoadMotions();
	return true;
}

// SUPPORT_LOCAL_TEXTURE
static std::string gs_modelLocalPath;

const std::string& GetModelLocalPath()
{
	return gs_modelLocalPath;
}
// END_OF_SUPPORT_LOCAL_TEXTURE

bool CGraphicThing::LoadModels()
{
	assert(m_pgrnFile != NULL);
	assert(m_models == NULL);

	if (m_pgrnFileInfo->ModelCount <= 0)
		return false;

	// SUPPORT_LOCAL_TEXTURE
	const std::string& fileName = GetFileNameString();

	if (fileName.length() > 2 && fileName[1] != ':')
	{
		int sepPos = (int)(fileName.rfind('\\'));
		gs_modelLocalPath.assign(fileName, 0, sepPos+1);
	}
	// END_OF_SUPPORT_LOCAL_TEXTURE

	int result = LoadThingModelsCached(fileName, m_pgrnFileInfo, m_models);
	if (result == 0)
		return false;

#if defined(__BL_GR2_FILE_LOAD_IMPROVE__)
	if (result == 1 && !m_bIsCached)
#else
	if (result == 1)
#endif
	{
		GrannyFreeFileSection(m_pgrnFile, GrannyStandardRigidVertexSection);
		GrannyFreeFileSection(m_pgrnFile, GrannyStandardRigidIndexSection);
		GrannyFreeFileSection(m_pgrnFile, GrannyStandardDeformableIndexSection);
		GrannyFreeFileSection(m_pgrnFile, GrannyStandardTextureSection);
	}
	return true;
}

bool CGraphicThing::LoadMotions()
{
	assert(m_pgrnFile != NULL);
	assert(m_motions == NULL);

	if (m_pgrnFileInfo->AnimationCount <= 0)
		return false;

	const std::string& fileName = GetFileNameString();

	if (!LoadThingMotionsCached(fileName, m_pgrnFileInfo, m_motions))
		return false;

	return true;
}

#if defined(__BL_GR2_FILE_LOAD_IMPROVE__)
void CGraphicThing::LoadNew(granny_file* file, bool cached)
{
	if (!file)
		return;

	m_pgrnFile = file;
	m_pgrnFileInfo = GrannyGetFileInfo(file);
	m_bIsCached = cached;

	LoadModels();
	LoadMotions();
}
#endif
