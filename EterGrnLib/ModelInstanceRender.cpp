#include "StdAfx.h"
#include "../eterlib/ShaderManager.h"
#include "../eterlib/ShaderInit.h"
#include "ModelInstance.h"
#include "Model.h"
#include <map>

static void UploadBoneMatricesToGPU(granny_model_instance* pModelInstance, granny_world_pose* pWorldPose, int maxBoneIndexHint = -1)
{
	if (!pModelInstance || !pWorldPose)
		return;

	granny_matrix_4x4* pgrnCompositeMatrices = GrannyGetWorldPoseComposite4x4Array(pWorldPose);
	if (!pgrnCompositeMatrices)
		return;

	int boneCount;
	if (maxBoneIndexHint >= 0)
	{
		boneCount = maxBoneIndexHint + 1;
	}
	else
	{
		granny_skeleton* pgrnSkeleton = GrannyGetSourceSkeleton(pModelInstance);
		if (!pgrnSkeleton)
			return;
		boneCount = pgrnSkeleton->BoneCount;
	}

	if (boneCount <= 0)
		return;
	if (boneCount > MAX_BONES)
		boneCount = MAX_BONES;

	SHADERMANAGER.SetBoneMatrices((const Matrix*)pgrnCompositeMatrices, boneCount);
}

#ifdef _TEST

#include "../eterlib/GrpScreen.h"

void Granny_RenderBoxBones(const granny_skeleton* pkGrnSkeleton, const granny_world_pose* pkGrnWorldPose, const Matrix& matBase)
{
	Matrix matWorld;
	CScreen screen;
	for (int iBone = 0; iBone != pkGrnSkeleton->BoneCount; ++iBone)
	{
		const granny_bone& rkGrnBone = pkGrnSkeleton->Bones[iBone];
		const Matrix* c_matBone=(const Matrix*)GrannyGetWorldPose4x4(pkGrnWorldPose, iBone);

		MatrixMultiply(&matWorld, c_matBone, &matBase);

		SHADERMANAGER.SetMatrix(MATRIX_WORLD, &matWorld);
		screen.RenderBox3d(-5.0f, -5.0f, -5.0f, 5.0f, 5.0f, 5.0f);
	}
}

#endif

void CGrannyModelInstance::DeformNoSkin(const Matrix * c_pWorldMatrix)
{
	if (IsEmpty())
		return;

	UpdateWorldPose();
	UpdateWorldMatrices(c_pWorldMatrix);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//// Render - GPU Skinning Only (DX11)
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// With One Texture
void CGrannyModelInstance::RenderWithOneTexture()
{
	if (IsEmpty())
		return;

	SHADERMANAGER.SetMeshTextureAlphaEnabled(false);

#ifdef _TEST
	Granny_RenderBoxBones(GrannyGetSourceSkeleton(m_pgrnModelInstance), m_pgrnWorldPose, TEST_matWorld);
	if (GetAsyncKeyState('P'))
		Tracef("render %x", m_pgrnModelInstance);
	return;
#endif

	ID3D11Buffer* lpd3dRigidPNTVtxBuf = m_pModel->GetPNTD3DVertexBuffer();
	ID3D11Buffer* lpd3dSkinnedVtxBuf = __GetSkinnedD3DVertexBuffer();

	if (lpd3dSkinnedVtxBuf)
	{
		SHADERMANAGER.BeginMeshSkinned();
		SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
		SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_SKINNED);

		UploadBoneMatricesToGPU(m_pgrnModelInstance, __GetWorldPosePtr(), __GetMaxBoneIndex());
		SHADERMANAGER.CommitChanges();

		SHADERMANAGER.SetVertexBuffer(0, lpd3dSkinnedVtxBuf, sizeof(TSkinnedVertex));
		RenderMeshNodeListWithOneTexture(CGrannyMesh::TYPE_DEFORM, CGrannyMaterial::TYPE_DIFFUSE_PNT);

		SHADERMANAGER.End();
	}

	if (lpd3dRigidPNTVtxBuf)
	{
		BeginShaderMeshRender(false);
		SHADERMANAGER.SetVertexBuffer(0, lpd3dRigidPNTVtxBuf, sizeof(TPNTVertex));
		RenderMeshNodeListWithOneTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_DIFFUSE_PNT);
		EndShaderMeshRender();
	}
}

void CGrannyModelInstance::BlendRenderWithOneTexture()
{
	if (IsEmpty())
		return;

	SHADERMANAGER.SetMeshTextureAlphaEnabled(true);

	ID3D11Buffer* lpd3dRigidPNTVtxBuf = m_pModel->GetPNTD3DVertexBuffer();
	ID3D11Buffer* lpd3dSkinnedVtxBuf = __GetSkinnedD3DVertexBuffer();

	if (lpd3dSkinnedVtxBuf)
	{
		SHADERMANAGER.BeginMeshSkinned();
		SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
		SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_SKINNED);
		UploadBoneMatricesToGPU(m_pgrnModelInstance, __GetWorldPosePtr(), __GetMaxBoneIndex());
		SHADERMANAGER.CommitChanges();

		SHADERMANAGER.SetVertexBuffer(0, lpd3dSkinnedVtxBuf, sizeof(TSkinnedVertex));
		RenderMeshNodeListWithOneTexture(CGrannyMesh::TYPE_DEFORM, CGrannyMaterial::TYPE_BLEND_PNT);
		SHADERMANAGER.End();
	}

	// Rigid meshes use regular mesh shader
	if (lpd3dRigidPNTVtxBuf)
	{
		BeginShaderMeshRender(false);
		SHADERMANAGER.SetVertexBuffer(0, lpd3dRigidPNTVtxBuf, sizeof(TPNTVertex));
		RenderMeshNodeListWithOneTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_BLEND_PNT);
		EndShaderMeshRender();
	}

	SHADERMANAGER.SetMeshTextureAlphaEnabled(false);
}

// With Two Texture (Shadow Receiving)
void CGrannyModelInstance::RenderWithTwoTexture()
{
	if (IsEmpty())
		return;

	ID3D11Buffer* lpd3dRigidPNTVtxBuf = m_pModel->GetPNTD3DVertexBuffer();
	ID3D11Buffer* lpd3dSkinnedVtxBuf = __GetSkinnedD3DVertexBuffer();

	if (lpd3dSkinnedVtxBuf)
	{
		SHADERMANAGER.BeginMeshSkinned();
		SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
		SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_SKINNED);
		SetShaderTwoTextureBlend(true);
		UploadBoneMatricesToGPU(m_pgrnModelInstance, __GetWorldPosePtr(), __GetMaxBoneIndex());
		SHADERMANAGER.CommitChanges();

		SHADERMANAGER.SetVertexBuffer(0, lpd3dSkinnedVtxBuf, sizeof(TSkinnedVertex));
		RenderMeshNodeListWithTwoTexture(CGrannyMesh::TYPE_DEFORM, CGrannyMaterial::TYPE_DIFFUSE_PNT);

		SetShaderTwoTextureBlend(false);
		SHADERMANAGER.End();
	}

	// Rigid meshes use regular mesh shader
	if (lpd3dRigidPNTVtxBuf)
	{
		BeginShaderMeshRender(false);
		SetShaderTwoTextureBlend(true);
		SHADERMANAGER.SetVertexBuffer(0, lpd3dRigidPNTVtxBuf, sizeof(TPNTVertex));
		RenderMeshNodeListWithTwoTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_DIFFUSE_PNT);
		SetShaderTwoTextureBlend(false);
		EndShaderMeshRender();
	}
}

void CGrannyModelInstance::BlendRenderWithTwoTexture()
{
	if (IsEmpty())
		return;

	ID3D11Buffer* lpd3dRigidPNTVtxBuf = m_pModel->GetPNTD3DVertexBuffer();
	ID3D11Buffer* lpd3dSkinnedVtxBuf = __GetSkinnedD3DVertexBuffer();

	if (lpd3dSkinnedVtxBuf)
	{
		SHADERMANAGER.BeginMeshSkinned();
		SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
		SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_SKINNED);
		SetShaderTwoTextureBlend(true);
		UploadBoneMatricesToGPU(m_pgrnModelInstance, __GetWorldPosePtr(), __GetMaxBoneIndex());
		SHADERMANAGER.CommitChanges();

		SHADERMANAGER.SetVertexBuffer(0, lpd3dSkinnedVtxBuf, sizeof(TSkinnedVertex));
		RenderMeshNodeListWithTwoTexture(CGrannyMesh::TYPE_DEFORM, CGrannyMaterial::TYPE_BLEND_PNT);

		SetShaderTwoTextureBlend(false);
		SHADERMANAGER.End();
	}

	// Rigid meshes use regular mesh shader
	if (lpd3dRigidPNTVtxBuf)
	{
		BeginShaderMeshRender(false);
		SetShaderTwoTextureBlend(true);
		SHADERMANAGER.SetVertexBuffer(0, lpd3dRigidPNTVtxBuf, sizeof(TPNTVertex));
		RenderMeshNodeListWithTwoTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_BLEND_PNT);
		SetShaderTwoTextureBlend(false);
		EndShaderMeshRender();
	}
}

// Without Texture (used for shadow map rendering)
void CGrannyModelInstance::RenderWithoutTexture()
{
	if (IsEmpty())
		return;

	SHADERMANAGER.SetDefaultTexture(0);
	SHADERMANAGER.SetShaderResource(1, nullptr);

	ID3D11Buffer* lpd3dRigidPNTVtxBuf = m_pModel->GetPNTD3DVertexBuffer();
	ID3D11Buffer* lpd3dSkinnedVtxBuf = __GetSkinnedD3DVertexBuffer();

	EShaderType currentShader = SHADERMANAGER.GetCurrentShader();
	bool bShadowMode = (currentShader == SHADER_SHADOW || currentShader == SHADER_SHADOW_SKINNED || currentShader == SHADER_SHADOW_VTF);

	if (bShadowMode)
	{
		if (lpd3dSkinnedVtxBuf)
		{
			SHADERMANAGER.BeginShadowSkinned();
			SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_SKINNED);
			UploadBoneMatricesToGPU(m_pgrnModelInstance, __GetWorldPosePtr(), __GetMaxBoneIndex());
			SHADERMANAGER.CommitChanges();

			SHADERMANAGER.SetVertexBuffer(0, lpd3dSkinnedVtxBuf, sizeof(TSkinnedVertex));
			RenderMeshNodeListWithoutTexture(CGrannyMesh::TYPE_DEFORM, CGrannyMaterial::TYPE_DIFFUSE_PNT);
			RenderMeshNodeListWithoutTexture(CGrannyMesh::TYPE_DEFORM, CGrannyMaterial::TYPE_BLEND_PNT);
		}

		if (lpd3dRigidPNTVtxBuf && !ms_bShadowSkipRigid)
		{
			SHADERMANAGER.BeginShadow();
			SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_PNT);
			SHADERMANAGER.SetVertexBuffer(0, lpd3dRigidPNTVtxBuf, sizeof(TPNTVertex));
			RenderMeshNodeListWithoutTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_DIFFUSE_PNT);
			RenderMeshNodeListWithoutTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_BLEND_PNT);
		}
	}
	else
	{
		// Normal rendering (not shadow map)
		if (lpd3dSkinnedVtxBuf)
		{
			SHADERMANAGER.BeginMeshSkinned();
			SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
			SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_SKINNED);
			UploadBoneMatricesToGPU(m_pgrnModelInstance, __GetWorldPosePtr(), __GetMaxBoneIndex());
			SHADERMANAGER.CommitChanges();

			SHADERMANAGER.SetVertexBuffer(0, lpd3dSkinnedVtxBuf, sizeof(TSkinnedVertex));
			RenderMeshNodeListWithoutTexture(CGrannyMesh::TYPE_DEFORM, CGrannyMaterial::TYPE_DIFFUSE_PNT);
			RenderMeshNodeListWithoutTexture(CGrannyMesh::TYPE_DEFORM, CGrannyMaterial::TYPE_BLEND_PNT);
			SHADERMANAGER.End();
		}

		if (lpd3dRigidPNTVtxBuf)
		{
			BeginShaderMeshRender(false);
			SHADERMANAGER.SetVertexBuffer(0, lpd3dRigidPNTVtxBuf, sizeof(TPNTVertex));
			RenderMeshNodeListWithoutTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_DIFFUSE_PNT);
			RenderMeshNodeListWithoutTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_BLEND_PNT);
			EndShaderMeshRender();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//// Render Mesh List
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// With One Texture
void CGrannyModelInstance::RenderMeshNodeListWithOneTexture(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType)
{
	assert(m_pModel != NULL);

	ID3D11Buffer* lpd3dIdxBuf = m_pModel->GetD3DIndexBuffer();
	assert(lpd3dIdxBuf != NULL);

	const CGrannyModel::TMeshNode * pMeshNode = m_pModel->GetMeshNodeList(eMeshType, eMtrlType);

	SHADERMANAGER.SetIndexBuffer(lpd3dIdxBuf, DXGI_FORMAT_R16_UINT, 0);

	while (pMeshNode)
	{
		const CGrannyMesh * pMesh = pMeshNode->pMesh;
		int vtxMeshBasePos = pMesh->GetVertexBasePosition();

		SetShaderWorldMatrix(&m_meshMatrices[pMeshNode->iMesh]);

		const CGrannyMesh::TTriGroupNode* pTriGroupNode = pMesh->GetTriGroupNodeList(eMtrlType);
		int vtxCount = pMesh->GetVertexCount();
		while (pTriGroupNode)
		{
			CGrannyMaterial& rkMtrl = m_kMtrlPal.GetMaterialRef(pTriGroupNode->mtrlIndex);

			ms_faceCount += pTriGroupNode->triCount;

			rkMtrl.ApplyRenderState();
			SHADERMANAGER.DrawIndexed(TOPOLOGY_TRIANGLELIST, 0, vtxCount, pTriGroupNode->idxPos, pTriGroupNode->triCount, vtxMeshBasePos);
			rkMtrl.RestorePipelineState();


			pTriGroupNode = pTriGroupNode->pNextTriGroupNode;
		}

		pMeshNode = pMeshNode->pNextMeshNode;
	}
}

// With Two Texture
void CGrannyModelInstance::RenderMeshNodeListWithTwoTexture(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType)
{
	assert(m_pModel != NULL);

	ID3D11Buffer* lpd3dIdxBuf = m_pModel->GetD3DIndexBuffer();
	assert(lpd3dIdxBuf != NULL);

	const CGrannyModel::TMeshNode * pMeshNode = m_pModel->GetMeshNodeList(eMeshType, eMtrlType);

	bool bShadowMode = SHADERMANAGER.IsTwoTextureBlendEnabled();

	SHADERMANAGER.SetIndexBuffer(lpd3dIdxBuf, DXGI_FORMAT_R16_UINT, 0);

	while (pMeshNode)
	{
		const CGrannyMesh * pMesh = pMeshNode->pMesh;
		int vtxMeshBasePos = pMesh->GetVertexBasePosition();

		SetShaderWorldMatrix(&m_meshMatrices[pMeshNode->iMesh]);

		const CGrannyMesh::TTriGroupNode* pTriGroupNode = pMesh->GetTriGroupNodeList(eMtrlType);
		int vtxCount = pMesh->GetVertexCount();
		while (pTriGroupNode)
		{
			const CGrannyMaterial& rkMtrl = m_kMtrlPal.GetMaterialRef(pTriGroupNode->mtrlIndex);

			ms_faceCount += pTriGroupNode->triCount;

			SHADERMANAGER.SetShaderResource(0, rkMtrl.GetD3DTexture(0));
			if (!bShadowMode)
				SHADERMANAGER.SetShaderResource(1, rkMtrl.GetD3DTexture(1));
			SHADERMANAGER.DrawIndexed(TOPOLOGY_TRIANGLELIST, 0, vtxCount, pTriGroupNode->idxPos, pTriGroupNode->triCount, vtxMeshBasePos);
			pTriGroupNode = pTriGroupNode->pNextTriGroupNode;
		}

		pMeshNode = pMeshNode->pNextMeshNode;
	}
}

// Without Texture
void CGrannyModelInstance::RenderMeshNodeListWithoutTexture(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType)
{
	assert(m_pModel != NULL);

	ID3D11Buffer* lpd3dIdxBuf = m_pModel->GetD3DIndexBuffer();
	assert(lpd3dIdxBuf != NULL);

	const CGrannyModel::TMeshNode * pMeshNode = m_pModel->GetMeshNodeList(eMeshType, eMtrlType);

	SHADERMANAGER.SetIndexBuffer(lpd3dIdxBuf, DXGI_FORMAT_R16_UINT, 0);

	while (pMeshNode)
	{
		const CGrannyMesh * pMesh = pMeshNode->pMesh;
		int vtxMeshBasePos = pMesh->GetVertexBasePosition();

		SetShaderWorldMatrix(&m_meshMatrices[pMeshNode->iMesh]);

		const CGrannyMesh::TTriGroupNode* pTriGroupNode = pMesh->GetTriGroupNodeList(eMtrlType);
		int vtxCount = pMesh->GetVertexCount();

		while (pTriGroupNode)
		{
			ms_faceCount += pTriGroupNode->triCount;
			SHADERMANAGER.DrawIndexed(TOPOLOGY_TRIANGLELIST, 0, vtxCount, pTriGroupNode->idxPos, pTriGroupNode->triCount, vtxMeshBasePos);
			pTriGroupNode = pTriGroupNode->pNextTriGroupNode;
		}

		pMeshNode = pMeshNode->pNextMeshNode;
	}
}
