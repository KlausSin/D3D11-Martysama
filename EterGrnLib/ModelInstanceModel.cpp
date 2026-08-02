#include "StdAfx.h"
#include "ModelInstance.h"
#include "Model.h"

void CGrannyModelInstance::Clear()
{
	m_kMtrlPal.Clear();

	DestroyDeviceObjects();
	// WORK
	__DestroyLocalSkinnedVertexBuffer();
	__DestroyMeshBindingVector();
	// END_OF_WORK
	__DestroyMeshMatrices();
	__DestroyModelInstance();
	__DestroyWorldPose();

	__Initialize();
}

void CGrannyModelInstance::SetMainModelPointer(CGrannyModel* pModel, CGraphicVertexBuffer* pkSharedDeformableVertexBuffer)
{
	SetLinkedModelPointer(pModel, pkSharedDeformableVertexBuffer, NULL);
}

void CGrannyModelInstance::SetLinkedModelPointer(CGrannyModel* pkModel, CGraphicVertexBuffer* pkSharedDeformableVertexBuffer, CGrannyModelInstance** ppkSkeletonInst)
{
	Clear();

	if (m_pModel)
		m_pModel->Release();

	m_pModel = pkModel;

	m_pModel->AddReference();

	if (pkSharedDeformableVertexBuffer)
		__SetSharedDeformableVertexBuffer(pkSharedDeformableVertexBuffer);
	else
		__CreateDynamicVertexBuffer();

	__CreateModelInstance();

	// WORK
	if (ppkSkeletonInst && *ppkSkeletonInst)
	{
		m_ppkSkeletonInst = ppkSkeletonInst;
		__CreateWorldPose(*ppkSkeletonInst);
		__CreateMeshBindingVector(*ppkSkeletonInst);
		__CreateLocalSkinnedVertexBuffer();
	}
	else
	{
		__CreateWorldPose(NULL);
		__CreateMeshBindingVector(NULL);
	}
	// END_OF_WORK

	__CreateMeshMatrices();

	ResetLocalTime();

	m_kMtrlPal.Copy(pkModel->GetMaterialPalette());
}

// WORK
granny_world_pose* CGrannyModelInstance::__GetWorldPosePtr() const
{
	if (m_pgrnWorldPoseReal)
		return m_pgrnWorldPoseReal;

	if (m_ppkSkeletonInst && *m_ppkSkeletonInst)
		return (*m_ppkSkeletonInst)->m_pgrnWorldPoseReal;

	assert(m_ppkSkeletonInst!=NULL && "__GetWorldPosePtr - NO HAVE SKELETON");
	return NULL;
}

#if GrannyProductMinorVersion==4
int* CGrannyModelInstance::__GetMeshBoneIndices(unsigned int iMeshBinding) const
#elif GrannyProductMinorVersion==11 || GrannyProductMinorVersion==9 || GrannyProductMinorVersion==8 || GrannyProductMinorVersion==7
const granny_int32x* CGrannyModelInstance::__GetMeshBoneIndices(unsigned int iMeshBinding) const
#else
#error "unknown granny version"
#endif
{
	assert(iMeshBinding<m_vct_pgrnMeshBinding.size());
	return GrannyGetMeshBindingToBoneIndices(m_vct_pgrnMeshBinding[iMeshBinding]);
}

bool CGrannyModelInstance::__CreateMeshBindingVector(CGrannyModelInstance* pkDstModelInst)
{
	assert(m_vct_pgrnMeshBinding.empty());

	if (!m_pModel)
		return false;

	granny_model* pgrnModel = m_pModel->GetGrannyModelPointer();
	if (!pgrnModel)
		return false;

	granny_skeleton* pgrnDstSkeleton = pgrnModel->Skeleton;
	if (pkDstModelInst && pkDstModelInst->m_pModel && pkDstModelInst->m_pModel->GetGrannyModelPointer())
		pgrnDstSkeleton = pkDstModelInst->m_pModel->GetGrannyModelPointer()->Skeleton;

	m_vct_pgrnMeshBinding.reserve(pgrnModel->MeshBindingCount);

	granny_int32 iMeshBinding;
	for (iMeshBinding = 0; iMeshBinding != pgrnModel->MeshBindingCount; ++iMeshBinding)
		m_vct_pgrnMeshBinding.push_back(GrannyNewMeshBinding(pgrnModel->MeshBindings[iMeshBinding].Mesh, pgrnModel->Skeleton, pgrnDstSkeleton));

	return true;
}

void CGrannyModelInstance::__DestroyMeshBindingVector()
{
	std::for_each(m_vct_pgrnMeshBinding.begin(), m_vct_pgrnMeshBinding.end(), GrannyFreeMeshBinding);
	m_vct_pgrnMeshBinding.clear();
}

// END_OF_WORK

void CGrannyModelInstance::__CreateWorldPose(CGrannyModelInstance* pkSkeletonInst)
{
	assert(m_pgrnModelInstance != NULL);
	assert(m_pgrnWorldPoseReal == NULL);

	// WORK
	if (pkSkeletonInst)
		return;
	// END_OF_WORK

	granny_skeleton * pgrnSkeleton = GrannyGetSourceSkeleton(m_pgrnModelInstance);

	// WORK
	m_pgrnWorldPoseReal = GrannyNewWorldPose(pgrnSkeleton->BoneCount);
	// END_OF_WORK
}

void CGrannyModelInstance::__DestroyWorldPose()
{
	if (!m_pgrnWorldPoseReal)
		return;

	GrannyFreeWorldPose(m_pgrnWorldPoseReal);
	m_pgrnWorldPoseReal = NULL;
}

void CGrannyModelInstance::__CreateModelInstance()
{
	assert(m_pModel != NULL);
	assert(m_pgrnModelInstance == NULL);

	const granny_model * pgrnModel = m_pModel->GetGrannyModelPointer();
	m_pgrnModelInstance = GrannyInstantiateModel(pgrnModel);
}

void CGrannyModelInstance::__DestroyModelInstance()
{
	if (!m_pgrnModelInstance)
		return;

	GrannyFreeModelInstance(m_pgrnModelInstance);
	m_pgrnModelInstance = NULL;
}

void CGrannyModelInstance::__CreateMeshMatrices()
{
	assert(m_pModel != NULL);

	if (m_pModel->GetMeshCount() <= 0)
		return;

	int meshCount = m_pModel->GetMeshCount();
	m_meshMatrices = new Matrix[meshCount];
}

void CGrannyModelInstance::__DestroyMeshMatrices()
{
	if (!m_meshMatrices)
		return;

	delete [] m_meshMatrices;
	m_meshMatrices = NULL;
}

DWORD CGrannyModelInstance::GetDeformableVertexCount()
{
	if (!m_pModel)
		return 0;

	return m_pModel->GetDeformVertexCount();
}

DWORD CGrannyModelInstance::GetVertexCount()
{
	if (!m_pModel)
		return 0;

	return m_pModel->GetVertexCount();
}

// WORK

void CGrannyModelInstance::__SetSharedDeformableVertexBuffer(CGraphicVertexBuffer* pkSharedDeformableVertexBuffer)
{
	m_pkSharedDeformableVertexBuffer = pkSharedDeformableVertexBuffer;
}

bool CGrannyModelInstance::__IsDeformableVertexBuffer()
{
	if (m_pkSharedDeformableVertexBuffer)
		return true;

	return m_kLocalDeformableVertexBuffer.IsEmpty();
}

ID3D11Buffer* CGrannyModelInstance::__GetDeformableD3DVertexBufferPtr()
{
	return __GetDeformableVertexBufferRef().GetBuffer();
}

CGraphicVertexBuffer& CGrannyModelInstance::__GetDeformableVertexBufferRef()
{
	if (m_pkSharedDeformableVertexBuffer)
		return *m_pkSharedDeformableVertexBuffer;

	return m_kLocalDeformableVertexBuffer;
}

bool CGrannyModelInstance::__CreateLocalSkinnedVertexBuffer()
{
	assert(!m_bHasLocalSkinnedVB);

	if (!m_pModel)
		return false;

	int deformVtxCount = m_pModel->GetDeformVertexCount();
	if (deformVtxCount <= 0)
		return true;

	if (m_vct_pgrnMeshBinding.empty())
		return false;

	size_t bufferSize = (size_t)deformVtxCount * sizeof(TSkinnedVertex);
	void* tempVertices = malloc(bufferSize);
	if (!tempVertices)
	{
		TraceError("CGrannyModelInstance::__CreateLocalSkinnedVertexBuffer - alloc failed (%u bytes)", (unsigned)bufferSize);
		return false;
	}
	memset(tempVertices, 0, bufferSize);

	int maxBone = 0;
	int meshCount = m_pModel->GetMeshCount();
	for (int m = 0; m < meshCount; ++m)
	{
		if ((size_t)m >= m_vct_pgrnMeshBinding.size())
			break;

		CGrannyMesh* pMesh = m_pModel->GetMeshPointer(m);
		granny_mesh_binding* pBinding = m_vct_pgrnMeshBinding[m];
		if (!pBinding)
			continue;

		int meshMax = 0;
		pMesh->LoadSkinnedVerticesWithBinding(tempVertices, pBinding, meshMax);
		if (meshMax > maxBone)
			maxBone = meshMax;
	}

	bool success = m_kLocalSkinnedVertexBuffer.Create(deformVtxCount, INPUT_LAYOUT_SKINNED, D3D11_USAGE_IMMUTABLE, tempVertices);
	free(tempVertices);

	if (!success)
	{
		TraceError("CGrannyModelInstance::__CreateLocalSkinnedVertexBuffer - VB create failed");
		return false;
	}

	m_nLocalMaxBoneIndex = maxBone;
	m_bHasLocalSkinnedVB = true;
	return true;
}

void CGrannyModelInstance::__DestroyLocalSkinnedVertexBuffer()
{
	m_kLocalSkinnedVertexBuffer.Destroy();
	m_bHasLocalSkinnedVB = false;
	m_nLocalMaxBoneIndex = 0;
}

ID3D11Buffer* CGrannyModelInstance::__GetSkinnedD3DVertexBuffer() const
{
	if (m_bHasLocalSkinnedVB)
		return m_kLocalSkinnedVertexBuffer.GetBuffer();
	return m_pModel ? m_pModel->GetSkinnedD3DVertexBuffer() : nullptr;
}

int CGrannyModelInstance::__GetMaxBoneIndex() const
{
	if (m_bHasLocalSkinnedVB)
		return m_nLocalMaxBoneIndex;
	return m_pModel ? m_pModel->GetMaxBoneIndex() : 0;
}

void CGrannyModelInstance::__CreateDynamicVertexBuffer()
{
	assert(m_pModel != NULL);
	assert(m_kLocalDeformableVertexBuffer.IsEmpty());

	int vtxCount = m_pModel->GetDeformVertexCount();

	if (0 != vtxCount)
	{
		if (!m_kLocalDeformableVertexBuffer.Create(vtxCount,
			INPUT_LAYOUT_PNT,
			D3D11_USAGE_DEFAULT))
			return;
	}
}

void CGrannyModelInstance::__DestroyDynamicVertexBuffer()
{
	m_kLocalDeformableVertexBuffer.Destroy();
	m_pkSharedDeformableVertexBuffer = NULL;
}

// END_OF_WORK

bool CGrannyModelInstance::GetBoneIndexByName(const char * c_szBoneName, int * pBoneIndex) const
{
	assert(m_pgrnModelInstance != NULL);

	granny_skeleton * pgrnSkeleton = GrannyGetSourceSkeleton(m_pgrnModelInstance);

	if (!GrannyFindBoneByName(pgrnSkeleton, c_szBoneName, pBoneIndex))
		return false;

	return true;
}

const float * CGrannyModelInstance::GetBoneMatrixPointer(int iBone) const
{
	const float* bones = GrannyGetWorldPose4x4(__GetWorldPosePtr(), iBone);
	if (!bones)
	{
		granny_model* pModel = m_pModel->GetGrannyModelPointer();
		//TraceError("GrannyModelInstance(%s).GetBoneMatrixPointer(boneIndex(%d)).NOT_FOUND_BONE", pModel->Name, iBone);
		return NULL;
	}
	return bones;
}

const float * CGrannyModelInstance::GetCompositeBoneMatrixPointer(int iBone) const
{
	return GrannyGetWorldPoseComposite4x4(__GetWorldPosePtr(), iBone);
}

void CGrannyModelInstance::ReloadTexture()
{
	assert("Not currently used - CGrannyModelInstance::ReloadTexture()");
/*
	assert(m_pModel != NULL);
	const CGrannyMaterialPalette & c_rGrannyMaterialPalette = m_pModel->GetMaterialPalette();
	DWORD dwMaterialCount = c_rGrannyMaterialPalette.GetMaterialCount();
	for (DWORD dwMtrIndex = 0; dwMtrIndex < dwMaterialCount; ++dwMtrIndex)
	{
		const CGrannyMaterial & c_rGrannyMaterial = c_rGrannyMaterialPalette.GetMaterialRef(dwMtrIndex);
		CGraphicImage * pImageStage0 = c_rGrannyMaterial.GetImagePointer(0);
		if (pImageStage0)
			pImageStage0->Reload();
		CGraphicImage * pImageStage1 = c_rGrannyMaterial.GetImagePointer(1);
		if (pImageStage1)
			pImageStage1->Reload();
	}
*/
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
