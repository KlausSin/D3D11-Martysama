#pragma once

#include "../eterlib/GrpVertexBuffer.h"
#include "../eterlib/GrpIndexBuffer.h"

#include "Mesh.h"

class CGrannyModel : public CReferenceObject
{
	public:
		typedef struct SMeshNode
		{
			int					iMesh;
			const CGrannyMesh * pMesh;
			SMeshNode *			pNextMeshNode;
		} TMeshNode;

	public:
		CGrannyModel();
		virtual ~CGrannyModel();

		bool IsEmpty() const;
		bool CreateFromGrannyModelPointer(granny_model* pgrnModel);
		bool CreateDeviceObjects();
		void DestroyDeviceObjects();
		void Destroy();

		int GetRigidVertexCount() const;
		int GetDeformVertexCount() const;
		int GetVertexCount() const;

		int GetIdxCount();
		int GetMeshCount() const;
		CGrannyMesh * GetMeshPointer(int iMesh);
		granny_model * GetGrannyModelPointer();
		const CGrannyMesh* GetMeshPointer(int iMesh) const;

		ID3D11Buffer* GetPNTD3DVertexBuffer() const;
		ID3D11Buffer* GetSkinnedD3DVertexBuffer() const;  // GPU skinning vertex buffer
		ID3D11Buffer* GetD3DIndexBuffer() const;

		// GPU Skinning support
		bool HasGPUSkinning() const { return m_bHasGPUSkinning; }
		int GetMaxBoneIndex() const { return m_maxBoneIndex; }

		const CGrannyModel::TMeshNode*  GetMeshNodeList(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType) const;

		bool LockVertices(void** indicies, void** vertices) const;
		void UnlockVertices() const;

		const CGrannyMaterialPalette& GetMaterialPalette() const;

	protected:
		bool LoadMeshs();
		bool LoadPNTVertices();
		bool LoadSkinnedVertices();  // Load skinned vertices for GPU skinning
		bool LoadIndices();
		void Initialize();

		BOOL CheckMeshIndex(int iIndex) const;
		void AppendMeshNode(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType, int iMesh);

	protected:
		// Granny Data
		granny_model *			m_pgrnModel;

		// Static Data
		CGrannyMesh *			m_meshs;

		CGraphicVertexBuffer	m_pntVtxBuf;	// for rigid mesh
		CGraphicVertexBuffer	m_skinnedVtxBuf; // for GPU skinned mesh (static buffer with blend weights)
		CGraphicIndexBuffer		m_idxBuf;
		bool					m_bHasGPUSkinning;
		int						m_maxBoneIndex;  // Max bone index used across all meshes (for selective upload)

		TMeshNode *				m_meshNodes;
		TMeshNode *				m_meshNodeLists[CGrannyMesh::TYPE_MAX_NUM][CGrannyMaterial::TYPE_MAX_NUM];

		int						m_deformVtxCount;
		int						m_rigidVtxCount;
		int						m_vtxCount;
		int						m_idxCount;

		int						m_meshNodeSize;
		int						m_meshNodeCapacity;

		CGrannyMaterialPalette	m_kMtrlPal;
	private:
		bool					m_bHaveBlendThing;
	public:
		bool					HaveBlendThing() { return m_bHaveBlendThing; }

	//////////////////////////////////////////////////////////////////////////
	// Vertex layout type (PNT or PNT2)
	protected:
		bool __LoadVertices();
	protected:
		EInputLayoutType m_LayoutType;
	//////////////////////////////////////////////////////////////////////////
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
