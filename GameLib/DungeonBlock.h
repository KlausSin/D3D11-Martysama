#pragma once

#include "../EterLib/ResourceManager.h"
#include "../EterLib/GrpObjectInstance.h"
#include "../EterGrnLib/ModelInstance.h"
#include "../EterGrnLib/Thing.h"

class CDungeonModelInstance : public CGrannyModelInstance
{
	public:
		CDungeonModelInstance() {}
		virtual ~CDungeonModelInstance() {}

		void RenderDungeonBlock();
		void RenderDungeonBlockShadow();
};

class CDungeonBlock : public CGraphicObjectInstance
{
	public:
		typedef std::vector<CDungeonModelInstance *> TModelInstanceContainer;
		enum
		{
			ID = THING_OBJECT
		};
		int GetType() const { return ID; }

	public:
		CDungeonBlock();
		virtual ~CDungeonBlock();

		void Destroy();

		void BuildBoundingSphere();
		bool Load(const char * c_szFileName);

		bool Intersect(float * pfu, float * pfv, float * pft);
		void GetBoundBox(Vector3 * pv3Min, Vector3 * pv3Max);

		void Update();
		void Render();

		const TModelInstanceContainer& GetModelInstances() const { return m_ModelInstanceContainer; }

		bool GetBoundingSphere(Vector3 & v3Center, float & fRadius);
		void OnUpdateCollisionData(const CStaticCollisionDataVector * pscdVector);
		void OnUpdateHeighInstance(CAttributeInstance * pAttributeInstance);
		bool OnGetObjectHeight(float fX, float fY, float * pfHeight);

		void OnRender() {}
		void OnBlendRender() {}
		void OnRenderToShadowMap() {}
		void OnRenderShadow();
		void OnRenderPCBlocker() {}

	protected:
		void __Initialize();

	protected:
		Vector3 m_v3Center;
		float m_fRadius;

		CGraphicThing * m_pThing;
		TModelInstanceContainer m_ModelInstanceContainer;
		CGraphicVertexBuffer	m_kDeformableVertexBuffer;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
