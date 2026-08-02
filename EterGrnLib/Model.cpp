#include "StdAfx.h"
#include "Model.h"
#include "Mesh.h"

const CGrannyMaterialPalette& CGrannyModel::GetMaterialPalette() const
{
	return m_kMtrlPal;
}

const CGrannyModel::TMeshNode* CGrannyModel::GetMeshNodeList(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType) const
{
	return m_meshNodeLists[eMeshType][eMtrlType];
}

CGrannyMesh * CGrannyModel::GetMeshPointer(int iMesh)
{
	assert(CheckMeshIndex(iMesh));
	assert(m_meshs != NULL);

	return m_meshs + iMesh;
}

const CGrannyMesh* CGrannyModel::GetMeshPointer(int iMesh) const
{
	assert(CheckMeshIndex(iMesh));
	assert(m_meshs != NULL);

	return m_meshs + iMesh;
}

int CGrannyModel::GetRigidVertexCount() const
{
	return m_rigidVtxCount;
}

int CGrannyModel::GetDeformVertexCount() const
{
	return m_deformVtxCount;
}

int CGrannyModel::GetVertexCount() const
{
	return m_vtxCount;
}

int CGrannyModel::GetMeshCount() const
{
	return m_pgrnModel ? m_pgrnModel->MeshBindingCount : 0;
}

granny_model* CGrannyModel::GetGrannyModelPointer()
{
	return m_pgrnModel;
}

ID3D11Buffer* CGrannyModel::GetD3DIndexBuffer() const
{
	return m_idxBuf.GetBuffer();
}

ID3D11Buffer* CGrannyModel::GetPNTD3DVertexBuffer() const
{
	return m_pntVtxBuf.GetBuffer();
}

ID3D11Buffer* CGrannyModel::GetSkinnedD3DVertexBuffer() const
{
	return m_skinnedVtxBuf.GetBuffer();
}

bool CGrannyModel::LockVertices(void** indicies, void** vertices) const
{
	if (!m_idxBuf.Map(indicies))
		return false;

	if (!m_pntVtxBuf.Map(vertices))
	{
		m_idxBuf.Unmap();
		return false;
	}

	return true;
}

void CGrannyModel::UnlockVertices() const
{
	m_idxBuf.Unmap();
	m_pntVtxBuf.Unmap();
}

bool CGrannyModel::LoadPNTVertices()
{
	if (m_rigidVtxCount <= 0)
		return true;

	assert(m_meshs != NULL);

	if (!m_pntVtxBuf.Create(m_rigidVtxCount, m_LayoutType, D3D11_USAGE_DYNAMIC))
		return false;

	void* vertices;
	if (!m_pntVtxBuf.Map(&vertices))
		return false;

	for (int m = 0; m < m_pgrnModel->MeshBindingCount; ++m)
	{
		CGrannyMesh& rMesh = m_meshs[m];
		rMesh.LoadPNTVertices(vertices);
	}

	m_pntVtxBuf.Unmap();
	return true;
}

bool CGrannyModel::LoadSkinnedVertices()
{
	if (m_deformVtxCount <= 0)
		return true;

	assert(m_meshs != NULL);

	// Allocate temporary buffer to fill vertex data
	size_t bufferSize = m_deformVtxCount * sizeof(TSkinnedVertex);
	void* tempVertices = malloc(bufferSize);
	if (!tempVertices)
	{
		TraceError("CGrannyModel::LoadSkinnedVertices - Failed to allocate temp buffer");
		return false;
	}

	// Zero the buffer to ensure clean data
	memset(tempVertices, 0, bufferSize);

	// Fill vertex data from each mesh and track max bone index
	m_maxBoneIndex = 0;
	for (int m = 0; m < m_pgrnModel->MeshBindingCount; ++m)
	{
		CGrannyMesh& rMesh = m_meshs[m];
		rMesh.LoadSkinnedVertices(tempVertices);

		int meshMaxBone = rMesh.GetMaxBoneIndex();
		if (meshMaxBone > m_maxBoneIndex)
			m_maxBoneIndex = meshMaxBone;
	}

	bool success = m_skinnedVtxBuf.Create(m_deformVtxCount, INPUT_LAYOUT_SKINNED, D3D11_USAGE_IMMUTABLE, tempVertices);

	free(tempVertices);

	if (!success)
	{
		TraceError("CGrannyModel::LoadSkinnedVertices - Failed to create skinned vertex buffer");
		return false;
	}

	m_bHasGPUSkinning = true;
	return true;
}

bool CGrannyModel::LoadIndices()
{
	//assert(m_idxCount > 0);
	if (m_idxCount <= 0)
		return true;

	if (!m_idxBuf.Create(m_idxCount, DXGI_FORMAT_R16_UINT, D3D11_USAGE_DYNAMIC))
		return false;

	void * indices;

	if (!m_idxBuf.Map((void**)&indices))
		return false;

	for (int m = 0; m < m_pgrnModel->MeshBindingCount; ++m)
	{
		CGrannyMesh& rMesh = m_meshs[m];
		rMesh.LoadIndices(indices);
	}

	m_idxBuf.Unmap();
	return true;
}

bool CGrannyModel::LoadMeshs()
{
	assert(m_meshs == NULL);
	assert(m_pgrnModel != NULL);

	if (m_pgrnModel->MeshBindingCount <= 0)
		return true;

	granny_skeleton * pgrnSkeleton = m_pgrnModel->Skeleton;

	int vtxRigidPos = 0;
	int vtxDeformPos = 0;
	int vtxPos = 0;
	int idxPos = 0;

	int diffusePNTMeshNodeCount = 0;
	int blendPNTMeshNodeCount = 0;
	int blendPNT2MeshNodeCount = 0;

	int meshCount = GetMeshCount();
	m_meshs = new CGrannyMesh[meshCount];

	// Detect vertex layout type from mesh data
	bool hasTex2 = false;

	for (int m = 0; m < meshCount; ++m)
	{
		CGrannyMesh& rMesh = m_meshs[m];
		granny_mesh* pgrnMesh = m_pgrnModel->MeshBindings[m].Mesh;

		if (GrannyMeshIsRigid(pgrnMesh))
		{
			if (!rMesh.CreateFromGrannyMeshPointer(pgrnSkeleton, pgrnMesh, vtxRigidPos, idxPos, m_kMtrlPal))
				return false;

			vtxRigidPos += GrannyGetMeshVertexCount(pgrnMesh);
		}
		else
		{
			if (!rMesh.CreateFromGrannyMeshPointer(pgrnSkeleton, pgrnMesh, vtxDeformPos, idxPos, m_kMtrlPal))
				return false;

			vtxDeformPos += GrannyGetMeshVertexCount(pgrnMesh);
		}
		m_bHaveBlendThing |= rMesh.HaveBlendThing();

		granny_int32x grni32xTypeCount = GrannyGetTotalTypeSize(pgrnMesh->PrimaryVertexData->VertexType) / sizeof(granny_data_type_definition);
		int i = 0;
		while (i < grni32xTypeCount)
		{
			if (NULL == pgrnMesh->PrimaryVertexData->VertexType[i].Name || 0 == strlen(pgrnMesh->PrimaryVertexData->VertexType[i].Name))
			{
				++i;
				continue;
			}
			if ( 0 == strcmp(pgrnMesh->PrimaryVertexData->VertexType[i].Name, GrannyVertexTextureCoordinatesName"1") )
				hasTex2 = true;
			++i;
		}

		vtxPos += GrannyGetMeshVertexCount(pgrnMesh);
		idxPos += GrannyGetMeshIndexCount(pgrnMesh);

		if (rMesh.GetTriGroupNodeList(CGrannyMaterial::TYPE_DIFFUSE_PNT))
			++diffusePNTMeshNodeCount;

		if (rMesh.GetTriGroupNodeList(CGrannyMaterial::TYPE_BLEND_PNT))
			++blendPNTMeshNodeCount;
	}

	m_meshNodeCapacity = diffusePNTMeshNodeCount + blendPNTMeshNodeCount + blendPNT2MeshNodeCount;
	m_meshNodes = new TMeshNode[m_meshNodeCapacity];

	for (int n = 0; n < meshCount; ++n)
	{
		CGrannyMesh& rMesh = m_meshs[n];
		granny_mesh* pgrnMesh = m_pgrnModel->MeshBindings[n].Mesh;

		CGrannyMesh::EType eMeshType = GrannyMeshIsRigid(pgrnMesh) ? CGrannyMesh::TYPE_RIGID : CGrannyMesh::TYPE_DEFORM;

		if (rMesh.GetTriGroupNodeList(CGrannyMaterial::TYPE_DIFFUSE_PNT))
			AppendMeshNode(eMeshType, CGrannyMaterial::TYPE_DIFFUSE_PNT, n);

		if (rMesh.GetTriGroupNodeList(CGrannyMaterial::TYPE_BLEND_PNT))
			AppendMeshNode(eMeshType, CGrannyMaterial::TYPE_BLEND_PNT, n);
	}

	// Set vertex layout type based on detected components
	if (hasTex2)
	{
		m_LayoutType = INPUT_LAYOUT_PNT2;
		// For Dungeon Block - mark meshes as PNT2
		for (int n = 0; n < meshCount; ++n)
		{
			CGrannyMesh& rMesh = m_meshs[n];
			rMesh.SetPNT2Mesh();
		}
	}
	else
	{
		m_LayoutType = INPUT_LAYOUT_PNT;
	}

	m_rigidVtxCount = vtxRigidPos;
	m_deformVtxCount = vtxDeformPos;

	m_vtxCount = vtxPos;
	m_idxCount = idxPos;

	return true;
}

BOOL CGrannyModel::CheckMeshIndex(int iIndex) const
{
	if (iIndex < 0)
		return FALSE;
	if (iIndex >= m_meshNodeSize)
		return FALSE;

	return TRUE;
}

void CGrannyModel::AppendMeshNode(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType, int iMesh)
{
	assert(m_meshNodeSize < m_meshNodeCapacity);

	TMeshNode& rMeshNode = m_meshNodes[m_meshNodeSize++];

	rMeshNode.iMesh = iMesh;
	rMeshNode.pMesh = m_meshs + iMesh;
	rMeshNode.pNextMeshNode = m_meshNodeLists[eMeshType][eMtrlType];
	m_meshNodeLists[eMeshType][eMtrlType] = &rMeshNode;
}

bool CGrannyModel::CreateFromGrannyModelPointer(granny_model* pgrnModel)
{
	assert(IsEmpty());

	m_pgrnModel = pgrnModel;

	if (!LoadMeshs())
		return false;

	if (!__LoadVertices())
		return false;

	if (!LoadSkinnedVertices())
	{
		TraceError("CGrannyModel::CreateFromGrannyModelPointer - Failed to load skinned vertices for GPU skinning");
		return false;
	}

	if (!LoadIndices())
		return false;

	AddReference();

	return true;
}

int CGrannyModel::GetIdxCount()
{
	return m_idxCount;
}

bool CGrannyModel::CreateDeviceObjects()
{
	if (m_rigidVtxCount > 0)
		if (!m_pntVtxBuf.CreateDeviceObjects())
			return false;

	// Create GPU skinning vertex buffer
	if (m_deformVtxCount > 0 && m_bHasGPUSkinning)
		if (!m_skinnedVtxBuf.CreateDeviceObjects())
			return false;

	if (m_idxCount > 0)
		if (!m_idxBuf.CreateDeviceObjects())
			return false;

	int meshCount = GetMeshCount();

	for (int i = 0; i < meshCount; ++i)
	{
		CGrannyMesh& rMesh = m_meshs[i];
		rMesh.RebuildTriGroupNodeList();
	}

	return true;
}

void CGrannyModel::DestroyDeviceObjects()
{
	m_pntVtxBuf.DestroyDeviceObjects();
	m_skinnedVtxBuf.DestroyDeviceObjects();
	m_idxBuf.DestroyDeviceObjects();
}

bool CGrannyModel::IsEmpty() const
{
	if (m_pgrnModel)
		return false;

	return true;
}

void CGrannyModel::Destroy()
{
	m_kMtrlPal.Clear();

	if (m_meshNodes)
		delete [] m_meshNodes;

	if (m_meshs)
		delete [] m_meshs;

	m_pntVtxBuf.Destroy();
	m_idxBuf.Destroy();

	Initialize();
}

bool CGrannyModel::__LoadVertices()
{
	if (m_rigidVtxCount <= 0)
		return true;

	assert(m_meshs != NULL);

	if (!m_pntVtxBuf.Create(m_rigidVtxCount, m_LayoutType, D3D11_USAGE_DYNAMIC))
		return false;

	void* vertices;
	if (!m_pntVtxBuf.Map(&vertices))
		return false;

	for (int m = 0; m < m_pgrnModel->MeshBindingCount; ++m)
	{
		CGrannyMesh& rMesh = m_meshs[m];
		rMesh.NEW_LoadVertices(vertices);
	}

	m_pntVtxBuf.Unmap();
	return true;
}

void CGrannyModel::Initialize()
{
	memset(m_meshNodeLists, 0, sizeof(m_meshNodeLists));

	m_pgrnModel = NULL;
	m_meshs = NULL;
	m_meshNodes = NULL;

	m_meshNodeSize = 0;
	m_meshNodeCapacity = 0;

	m_rigidVtxCount = 0;
	m_deformVtxCount = 0;
	m_vtxCount = 0;
	m_idxCount = 0;

	m_bHasGPUSkinning = false;
	m_maxBoneIndex = 0;

	m_LayoutType = INPUT_LAYOUT_PNT;
	m_bHaveBlendThing = false;
}

CGrannyModel::CGrannyModel()
{
	Initialize();
}

CGrannyModel::~CGrannyModel()
{
	Destroy();
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
