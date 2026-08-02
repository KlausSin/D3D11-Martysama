#pragma once

#include "GrpVertexBuffer.h"

class CDynamicVertexBuffer : public CGraphicVertexBuffer
{
	public:
		CDynamicVertexBuffer();
		virtual ~CDynamicVertexBuffer();

		bool Create(int vtxCount, EInputLayoutType layoutType);
		bool Create(int vtxCount, UINT stride);

	protected:
		int m_cachedVtxCount;
		EInputLayoutType m_cachedLayoutType;
		UINT m_cachedStride;
};
