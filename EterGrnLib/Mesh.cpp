#include "StdAfx.h"
#include "Mesh.h"
#include "Model.h"
#include "Material.h"

granny_data_type_definition GrannyPNT3322VertexType[5] =
{
	{GrannyReal32Member, GrannyVertexPositionName, 0, 3},
	{GrannyReal32Member, GrannyVertexNormalName, 0, 3},
	{GrannyReal32Member, GrannyVertexTextureCoordinatesName"0", 0, 2},
	{GrannyReal32Member, GrannyVertexTextureCoordinatesName"1", 0, 2},
	{GrannyEndMember}
};

void CGrannyMesh::LoadIndices(void * dstBaseIndices)
{
	const granny_mesh * pgrnMesh = GetGrannyMeshPointer();

	TIndex * dstIndices = ((TIndex *)dstBaseIndices) + m_idxBasePos;
	GrannyCopyMeshIndices(pgrnMesh, sizeof(TIndex), dstIndices);
}

void CGrannyMesh::LoadPNTVertices(void * dstBaseVertices)
{
	const granny_mesh * pgrnMesh = GetGrannyMeshPointer();

	if (!GrannyMeshIsRigid(pgrnMesh))
		return;

	TPNTVertex * dstVertices = ((TPNTVertex *)dstBaseVertices) + m_vtxBasePos;
	GrannyCopyMeshVertices(pgrnMesh, m_pgrnMeshType, dstVertices);
}

void CGrannyMesh::NEW_LoadVertices(void * dstBaseVertices)
{
	const granny_mesh * pgrnMesh = GetGrannyMeshPointer();

	if (!GrannyMeshIsRigid(pgrnMesh))
		return;

	TPNTVertex * dstVertices = ((TPNTVertex *)dstBaseVertices) + m_vtxBasePos;
	GrannyCopyMeshVertices(pgrnMesh, m_pgrnMeshType, dstVertices);
}

void CGrannyMesh::LoadSkinnedVertices(void * dstBaseVertices)
{
	const granny_mesh * pgrnMesh = GetGrannyMeshPointer();

	// This function is only for deformable (skinned) meshes
	if (GrannyMeshIsRigid(pgrnMesh))
		return;

	if (!m_pgrnMeshBindingTemp)
	{
		TraceError("CGrannyMesh::LoadSkinnedVertices - No mesh binding available for bone remapping");
		return;
	}

#if GrannyProductMinorVersion==4
	int * boneIndexMapping = GrannyGetMeshBindingToBoneIndices(m_pgrnMeshBindingTemp);
#elif GrannyProductMinorVersion==11 || GrannyProductMinorVersion==9 || GrannyProductMinorVersion==8 || GrannyProductMinorVersion==7
	const granny_int32x * boneIndexMapping = GrannyGetMeshBindingToBoneIndices(m_pgrnMeshBindingTemp);
#else
#error "unknown granny version"
#endif

	if (!boneIndexMapping)
	{
		TraceError("CGrannyMesh::LoadSkinnedVertices - Failed to get bone index mapping");
		return;
	}

	const granny_pwnt3432_vertex* srcVertices = (const granny_pwnt3432_vertex*)GrannyGetMeshVertices(pgrnMesh);
	TSkinnedVertex* dstVertices = ((TSkinnedVertex*)dstBaseVertices) + m_vtxBasePos;

	int vtxCount = GrannyGetMeshVertexCount(pgrnMesh);
	const float inv255 = 1.0f / 255.0f;
	const int MAX_BONE_INDEX = 511;  // MAX_BONES - 1

	// Track max bone index for selective upload optimization
	int maxBoneIdx = 0;

	for (int i = 0; i < vtxCount; ++i)
	{
		const granny_pwnt3432_vertex& src = srcVertices[i];
		TSkinnedVertex& dst = dstVertices[i];

		// Copy position
		dst.position.x = src.Position[0];
		dst.position.y = src.Position[1];
		dst.position.z = src.Position[2];

		// Copy normal
		dst.normal.x = src.Normal[0];
		dst.normal.y = src.Normal[1];
		dst.normal.z = src.Normal[2];

		// Copy texture coordinates
		dst.texCoord.x = src.UV[0];
		dst.texCoord.y = src.UV[1];

		dst.blendWeights[0] = (float)src.BoneWeights[0] * inv255;
		dst.blendWeights[1] = (float)src.BoneWeights[1] * inv255;
		dst.blendWeights[2] = (float)src.BoneWeights[2] * inv255;
		dst.blendWeights[3] = (float)src.BoneWeights[3] * inv255;

		float weightSum = dst.blendWeights[0] + dst.blendWeights[1] + dst.blendWeights[2] + dst.blendWeights[3];
		if (weightSum > 0.001f && (weightSum < 0.999f || weightSum > 1.001f))
		{
			float invSum = 1.0f / weightSum;
			dst.blendWeights[0] *= invSum;
			dst.blendWeights[1] *= invSum;
			dst.blendWeights[2] *= invSum;
			dst.blendWeights[3] *= invSum;
		}

		int boneIdx0 = boneIndexMapping[src.BoneIndices[0]];
		int boneIdx1 = boneIndexMapping[src.BoneIndices[1]];
		int boneIdx2 = boneIndexMapping[src.BoneIndices[2]];
		int boneIdx3 = boneIndexMapping[src.BoneIndices[3]];

		// Track max bone index for selective upload optimization
		if (dst.blendWeights[0] > 0.001f && boneIdx0 > maxBoneIdx) maxBoneIdx = boneIdx0;
		if (dst.blendWeights[1] > 0.001f && boneIdx1 > maxBoneIdx) maxBoneIdx = boneIdx1;
		if (dst.blendWeights[2] > 0.001f && boneIdx2 > maxBoneIdx) maxBoneIdx = boneIdx2;
		if (dst.blendWeights[3] > 0.001f && boneIdx3 > maxBoneIdx) maxBoneIdx = boneIdx3;

		dst.blendIndices[0] = (BYTE)((boneIdx0 >= 0 && boneIdx0 <= MAX_BONE_INDEX) ? boneIdx0 : 0);
		dst.blendIndices[1] = (BYTE)((boneIdx1 >= 0 && boneIdx1 <= MAX_BONE_INDEX) ? boneIdx1 : 0);
		dst.blendIndices[2] = (BYTE)((boneIdx2 >= 0 && boneIdx2 <= MAX_BONE_INDEX) ? boneIdx2 : 0);
		dst.blendIndices[3] = (BYTE)((boneIdx3 >= 0 && boneIdx3 <= MAX_BONE_INDEX) ? boneIdx3 : 0);
	}

	// Store max bone index for this mesh (add 1 to get count)
	m_maxBoneIndex = maxBoneIdx;
}

void CGrannyMesh::LoadSkinnedVerticesWithBinding(void* dstBaseVertices, granny_mesh_binding* pBinding, int& maxBoneIndexOut)
{
	maxBoneIndexOut = 0;

	const granny_mesh* pgrnMesh = GetGrannyMeshPointer();

	if (GrannyMeshIsRigid(pgrnMesh))
		return;

	if (!pBinding)
	{
		TraceError("CGrannyMesh::LoadSkinnedVerticesWithBinding - binding is NULL");
		return;
	}

#if GrannyProductMinorVersion==4
	int* boneIndexMapping = GrannyGetMeshBindingToBoneIndices(pBinding);
#elif GrannyProductMinorVersion==11 || GrannyProductMinorVersion==9 || GrannyProductMinorVersion==8 || GrannyProductMinorVersion==7
	const granny_int32x* boneIndexMapping = GrannyGetMeshBindingToBoneIndices(pBinding);
#else
#error "unknown granny version"
#endif

	if (!boneIndexMapping)
	{
		TraceError("CGrannyMesh::LoadSkinnedVerticesWithBinding - null bone index mapping");
		return;
	}

	const granny_pwnt3432_vertex* srcVertices = (const granny_pwnt3432_vertex*)GrannyGetMeshVertices(pgrnMesh);
	TSkinnedVertex* dstVertices = ((TSkinnedVertex*)dstBaseVertices) + m_vtxBasePos;

	int vtxCount = GrannyGetMeshVertexCount(pgrnMesh);
	const float inv255 = 1.0f / 255.0f;
	const int MAX_BONE_INDEX = 511;

	int maxBoneIdx = 0;

	for (int i = 0; i < vtxCount; ++i)
	{
		const granny_pwnt3432_vertex& src = srcVertices[i];
		TSkinnedVertex& dst = dstVertices[i];

		dst.position.x = src.Position[0];
		dst.position.y = src.Position[1];
		dst.position.z = src.Position[2];

		dst.normal.x = src.Normal[0];
		dst.normal.y = src.Normal[1];
		dst.normal.z = src.Normal[2];

		dst.texCoord.x = src.UV[0];
		dst.texCoord.y = src.UV[1];

		dst.blendWeights[0] = (float)src.BoneWeights[0] * inv255;
		dst.blendWeights[1] = (float)src.BoneWeights[1] * inv255;
		dst.blendWeights[2] = (float)src.BoneWeights[2] * inv255;
		dst.blendWeights[3] = (float)src.BoneWeights[3] * inv255;

		float weightSum = dst.blendWeights[0] + dst.blendWeights[1] + dst.blendWeights[2] + dst.blendWeights[3];
		if (weightSum > 0.001f && (weightSum < 0.999f || weightSum > 1.001f))
		{
			float invSum = 1.0f / weightSum;
			dst.blendWeights[0] *= invSum;
			dst.blendWeights[1] *= invSum;
			dst.blendWeights[2] *= invSum;
			dst.blendWeights[3] *= invSum;
		}

		int boneIdx0 = boneIndexMapping[src.BoneIndices[0]];
		int boneIdx1 = boneIndexMapping[src.BoneIndices[1]];
		int boneIdx2 = boneIndexMapping[src.BoneIndices[2]];
		int boneIdx3 = boneIndexMapping[src.BoneIndices[3]];

		if (boneIdx0 > maxBoneIdx) maxBoneIdx = boneIdx0;
		if (boneIdx1 > maxBoneIdx) maxBoneIdx = boneIdx1;
		if (boneIdx2 > maxBoneIdx) maxBoneIdx = boneIdx2;
		if (boneIdx3 > maxBoneIdx) maxBoneIdx = boneIdx3;

		dst.blendIndices[0] = (BYTE)((boneIdx0 >= 0 && boneIdx0 <= MAX_BONE_INDEX) ? boneIdx0 : 0);
		dst.blendIndices[1] = (BYTE)((boneIdx1 >= 0 && boneIdx1 <= MAX_BONE_INDEX) ? boneIdx1 : 0);
		dst.blendIndices[2] = (BYTE)((boneIdx2 >= 0 && boneIdx2 <= MAX_BONE_INDEX) ? boneIdx2 : 0);
		dst.blendIndices[3] = (BYTE)((boneIdx3 >= 0 && boneIdx3 <= MAX_BONE_INDEX) ? boneIdx3 : 0);
	}

	maxBoneIndexOut = maxBoneIdx;
}

const granny_mesh * CGrannyMesh::GetGrannyMeshPointer() const
{
	return m_pgrnMesh;
}

const CGrannyMesh::TTriGroupNode * CGrannyMesh::GetTriGroupNodeList(CGrannyMaterial::EType eMtrlType) const
{
	return m_triGroupNodeLists[eMtrlType];
}

int CGrannyMesh::GetVertexCount() const
{
	assert(m_pgrnMesh!=NULL);
	return GrannyGetMeshVertexCount(m_pgrnMesh);
}

int CGrannyMesh::GetVertexBasePosition() const
{
	return m_vtxBasePos;
}

int CGrannyMesh::GetIndexBasePosition() const
{
	return m_idxBasePos;
}

// WORK
#if GrannyProductMinorVersion==4
int * CGrannyMesh::GetDefaultBoneIndices() const
#elif GrannyProductMinorVersion==11 || GrannyProductMinorVersion==9 || GrannyProductMinorVersion==8 || GrannyProductMinorVersion==7
const granny_int32x * CGrannyMesh::GetDefaultBoneIndices() const
#else
#error "unknown granny version"
#endif
{
	return GrannyGetMeshBindingToBoneIndices(m_pgrnMeshBindingTemp);
}
// END_OF_WORK

bool CGrannyMesh::IsEmpty() const
{
	if (m_pgrnMesh)
		return false;

	return true;
}

bool CGrannyMesh::CreateFromGrannyMeshPointer(granny_skeleton * pgrnSkeleton, granny_mesh * pgrnMesh, int vtxBasePos, int idxBasePos, CGrannyMaterialPalette& rkMtrlPal)
{
	assert(IsEmpty());

	m_pgrnMesh = pgrnMesh;
	m_vtxBasePos = vtxBasePos;
	m_idxBasePos = idxBasePos;

	if (m_pgrnMesh->BoneBindingCount < 0)
		return true;

	// WORK
	m_pgrnMeshBindingTemp = GrannyNewMeshBinding(m_pgrnMesh, pgrnSkeleton, pgrnSkeleton);
	// END_OF_WORK

	// Two Side Mesh
	if (!strncmp(m_pgrnMesh->Name, "2x", 2))
		m_isTwoSide = true;

	if (!LoadMaterials(rkMtrlPal))
		return false;

	if (!LoadTriGroupNodeList(rkMtrlPal))
		return false;

	return true;
}

bool CGrannyMesh::LoadTriGroupNodeList(CGrannyMaterialPalette& rkMtrlPal)
{
	assert(m_pgrnMesh != NULL);
	assert(m_triGroupNodes == NULL);

	int mtrlCount		= m_pgrnMesh->MaterialBindingCount;
	if (mtrlCount <= 0)
		return true;

	int GroupNodeCount	= GrannyGetMeshTriangleGroupCount(m_pgrnMesh);
	if (GroupNodeCount <= 0)
		return true;

	m_triGroupNodes		= new TTriGroupNode[GroupNodeCount];

	const granny_tri_material_group * c_pgrnTriGroups = GrannyGetMeshTriangleGroups(m_pgrnMesh);

	for (int g = 0; g < GroupNodeCount; ++g)
	{
		const granny_tri_material_group & c_rgrnTriGroup = c_pgrnTriGroups[g];
		TTriGroupNode * pTriGroupNode = m_triGroupNodes + g;

		pTriGroupNode->idxPos = m_idxBasePos + c_rgrnTriGroup.TriFirst * 3;
		pTriGroupNode->triCount = c_rgrnTriGroup.TriCount;

		int iMtrl = c_rgrnTriGroup.MaterialIndex;
		if (iMtrl < 0 || iMtrl >= mtrlCount)
		{
			pTriGroupNode->mtrlIndex=0;//m_mtrlIndexVector[iMtrl];
		}
		else
		{
			pTriGroupNode->mtrlIndex=m_mtrlIndexVector[iMtrl];
		}

		const CGrannyMaterial& rkMtrl=rkMtrlPal.GetMaterialRef(pTriGroupNode->mtrlIndex);
		pTriGroupNode->pNextTriGroupNode		= m_triGroupNodeLists[rkMtrl.GetType()];
		m_triGroupNodeLists[rkMtrl.GetType()]	= pTriGroupNode;

	}

	return true;
}

void CGrannyMesh::RebuildTriGroupNodeList()
{
	assert(!"CGrannyMesh::RebuildTriGroupNodeList() - Why should you rebuild it -?");
	/*
	int mtrlCount = m_pgrnMesh->MaterialBindingCount;
	int GroupNodeCount = GrannyGetMeshTriangleGroupCount(m_pgrnMesh);

	if (GroupNodeCount <= 0)
		return;

	const granny_tri_material_group * c_pgrnTriGroups = GrannyGetMeshTriangleGroups(m_pgrnMesh);

	for (int g = 0; g < GroupNodeCount; ++g)
	{
		const granny_tri_material_group& c_rgrnTriGroup = c_pgrnTriGroups[g];
		TTriGroupNode * pTriGroupNode = m_triGroupNodes + g;

		int iMtrl = c_rgrnTriGroup.MaterialIndex;

		if (iMtrl >= 0 && iMtrl < mtrlCount)
		{
			CGrannyMaterial & rMtrl = m_mtrls[iMtrl];

			pTriGroupNode->lpd3dTextures[0] = rMtrl.GetD3DTexture(0);
			pTriGroupNode->lpd3dTextures[1] = rMtrl.GetD3DTexture(1);

		}
	}
	*/
}

bool CGrannyMesh::LoadMaterials(CGrannyMaterialPalette& rkMtrlPal)
{
	assert(m_pgrnMesh != NULL);

	if (m_pgrnMesh->MaterialBindingCount <= 0)
		return true;

	int mtrlCount = m_pgrnMesh->MaterialBindingCount;
	bool bHaveBlendThing = false;

	for (int m = 0; m < mtrlCount; ++m)
	{
		granny_material* pgrnMaterial = m_pgrnMesh->MaterialBindings[m].Material;
		DWORD mtrlIndex=rkMtrlPal.RegisterMaterial(pgrnMaterial);
		m_mtrlIndexVector.push_back(mtrlIndex);
		bHaveBlendThing |= rkMtrlPal.GetMaterialRef(mtrlIndex).GetType() == CGrannyMaterial::TYPE_BLEND_PNT;
	}
	m_bHaveBlendThing = bHaveBlendThing;

	return true;
}

bool CGrannyMesh::IsTwoSide() const
{
	return m_isTwoSide;
}

void CGrannyMesh::SetPNT2Mesh()
{
	m_pgrnMeshType = GrannyPNT3322VertexType;
}

void CGrannyMesh::Destroy()
{
	if (m_triGroupNodes)
		delete [] m_triGroupNodes;

	m_mtrlIndexVector.clear();

	// WORK
	if (m_pgrnMeshBindingTemp)
		GrannyFreeMeshBinding(m_pgrnMeshBindingTemp);
	// END_OF_WORK

	Initialize();
}

void CGrannyMesh::Initialize()
{
	for (int r = 0; r < CGrannyMaterial::TYPE_MAX_NUM; ++r)
		m_triGroupNodeLists[r] = NULL;

	m_pgrnMeshType = GrannyPNT332VertexType;
	m_pgrnMesh = NULL;
	// WORK
	m_pgrnMeshBindingTemp = NULL;
	// END_OF_WORK

	m_triGroupNodes = NULL;

	m_vtxBasePos = 0;
	m_idxBasePos = 0;

	m_isTwoSide = false;
	m_bHaveBlendThing = false;
	m_maxBoneIndex = 0;
}

CGrannyMesh::CGrannyMesh()
{
	Initialize();
}

CGrannyMesh::~CGrannyMesh()
{
	Destroy();
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
