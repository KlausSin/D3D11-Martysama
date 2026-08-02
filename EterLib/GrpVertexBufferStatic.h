#pragma once

#include "GrpVertexBuffer.h"

class CStaticVertexBuffer : public CGraphicVertexBuffer
{
	public:
		CStaticVertexBuffer();
		virtual ~CStaticVertexBuffer();

		bool Create(int vtxCount, EInputLayoutType layoutType);
		bool Create(int vtxCount, UINT stride);
};
