#include "StdAfx.h"
#include "GrpScreen.h"
#include "Camera.h"
#include "ShaderManager.h"

DWORD		CScreen::ms_diffuseColor = 0xffffffff;
float		CScreen::ms_clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
DWORD		CScreen::ms_clearStencil = 0L;
float		CScreen::ms_clearDepth = 1.0f;
Frustum		CScreen::ms_frustum;

extern bool GRAPHICS_CAPS_CAN_NOT_DRAW_LINE;

void CScreen::RenderLine3d(float sx, float sy, float sz, float ex, float ey, float ez)
{
	if (GRAPHICS_CAPS_CAN_NOT_DRAW_LINE)
		return;

	SPDTVertexRaw vertices[2] =
	{
		{ sx, sy, sz, ms_diffuseColor, 0.0f, 0.0f },
		{ ex, ey, ez, ms_diffuseColor, 0.0f, 0.0f }
	};

	if (SetPDTStream(vertices, 2))
	{
		SHADERMANAGER.SetDefaultTexture(0);
		SHADERMANAGER.SetShaderResource(1, NULL);
		SHADERMANAGER.Draw(TOPOLOGY_LINELIST, 0, 1);
	}
}

void CScreen::RenderBox3d(float sx, float sy, float sz, float ex, float ey, float ez)
{
	if (GRAPHICS_CAPS_CAN_NOT_DRAW_LINE)
		return;

	SPDTVertexRaw vertices[8] =
	{
		{ sx, sy, sz, ms_diffuseColor, 0.0f, 0.0f },
		{ ex, sy, sz, ms_diffuseColor, 0.0f, 0.0f },
		{ sx, sy, sz, ms_diffuseColor, 0.0f, 0.0f },
		{ sx, ey, ez, ms_diffuseColor, 0.0f, 0.0f },
		{ ex, sy, sz, ms_diffuseColor, 0.0f, 0.0f },
		{ ex, ey, ez, ms_diffuseColor, 0.0f, 0.0f },
		{ sx, ey, ez, ms_diffuseColor, 0.0f, 0.0f },
		{ ex + 1.0f, ey, ez, ms_diffuseColor, 0.0f, 0.0f }
	};

	if (SetPDTStream(vertices, 8))
	{
		SHADERMANAGER.SetDefaultTexture(0);
		SHADERMANAGER.SetShaderResource(1, NULL);
		SHADERMANAGER.Draw(TOPOLOGY_LINELIST, 0, 4);
	}
}

void CScreen::RenderBar3d(float sx, float sy, float sz, float ex, float ey, float ez)
{
	SPDTVertexRaw vertices[4] =
	{
		{ sx, sy, sz, ms_diffuseColor, 0.0f, 0.0f },
		{ sx, ey, ez, ms_diffuseColor, 0.0f, 0.0f },
		{ ex, sy, sz, ms_diffuseColor, 0.0f, 0.0f },
		{ ex, ey, ez, ms_diffuseColor, 0.0f, 0.0f },
	};

	if (SetPDTStream(vertices, 4))
	{
		SHADERMANAGER.SetDefaultTexture(0);
		SHADERMANAGER.SetShaderResource(1, NULL);
		SHADERMANAGER.Draw(TOPOLOGY_TRIANGLESTRIP, 0, 2);
	}
}

void CScreen::RenderBar3d(const Vector3* c_pv3Positions)
{
	SPDTVertexRaw vertices[4] =
	{
		{ c_pv3Positions[0].x, c_pv3Positions[0].y, c_pv3Positions[0].z, ms_diffuseColor, 0.0f, 0.0f },
		{ c_pv3Positions[2].x, c_pv3Positions[2].y, c_pv3Positions[2].z, ms_diffuseColor, 0.0f, 0.0f },
		{ c_pv3Positions[1].x, c_pv3Positions[1].y, c_pv3Positions[1].z, ms_diffuseColor, 0.0f, 0.0f },
		{ c_pv3Positions[3].x, c_pv3Positions[3].y, c_pv3Positions[3].z, ms_diffuseColor, 0.0f, 0.0f },
	};

	if (SetPDTStream(vertices, 4))
	{
		SHADERMANAGER.SetDefaultTexture(0);
		SHADERMANAGER.SetShaderResource(1, NULL);
		SHADERMANAGER.Draw(TOPOLOGY_TRIANGLESTRIP, 0, 2);
	}
}

void CScreen::RenderGradationBar3d(float sx, float sy, float sz, float ex, float ey, float ez, DWORD dwStartColor, DWORD dwEndColor)
{
	if (sx == ex) return;
	if (sy == ey) return;

	SPDTVertexRaw vertices[4] =
	{
		{ sx, sy, sz, dwStartColor, 0.0f, 0.0f },
		{ sx, ey, ez, dwEndColor, 0.0f, 0.0f },
		{ ex, sy, sz, dwStartColor, 0.0f, 0.0f },
		{ ex, ey, ez, dwEndColor, 0.0f, 0.0f },
	};

	if (SetPDTStream(vertices, 4))
	{
		SHADERMANAGER.SetDefaultTexture(0);
		SHADERMANAGER.SetShaderResource(1, NULL);
		SHADERMANAGER.Draw(TOPOLOGY_TRIANGLESTRIP, 0, 2);
	}
}

void CScreen::RenderLineCube(float sx, float sy, float sz, float ex, float ey, float ez)
{
	SPDTVertexRaw vertices[8] =
	{
		{ sx, sy, sz, ms_diffuseColor, 0.0f, 0.0f },
		{ ex, sy, sz, ms_diffuseColor, 0.0f, 0.0f },
		{ sx, ey, sz, ms_diffuseColor, 0.0f, 0.0f },
		{ ex, ey, sz, ms_diffuseColor, 0.0f, 0.0f },
		{ sx, sy, ez, ms_diffuseColor, 0.0f, 0.0f },
		{ ex, sy, ez, ms_diffuseColor, 0.0f, 0.0f },
		{ sx, ey, ez, ms_diffuseColor, 0.0f, 0.0f },
		{ ex, ey, ez, ms_diffuseColor, 0.0f, 0.0f },
	};

	if (SetPDTStream(vertices, 8))
	{
		SHADERMANAGER.SetDefaultTexture(0);
		SHADERMANAGER.SetShaderResource(1, NULL);
		SHADERMANAGER.SetMatrix(MATRIX_WORLD, GetActiveMatrixStack()->GetTop());
		SetDefaultIndexBuffer(DEFAULT_IB_LINE_CUBE);
		SHADERMANAGER.DrawIndexed(TOPOLOGY_LINELIST, 0, 8, 0, 4 * 3);
	}
}

void CScreen::RenderCube(float sx, float sy, float sz, float ex, float ey, float ez)
{
	SPDTVertexRaw vertices[8] =
	{
		{ sx, sy, sz, ms_diffuseColor, 0.0f, 0.0f  },
		{ ex, sy, sz, ms_diffuseColor, 0.0f, 0.0f  },
		{ sx, ey, sz, ms_diffuseColor, 0.0f, 0.0f  },
		{ ex, ey, sz, ms_diffuseColor, 0.0f, 0.0f  },
		{ sx, sy, ez, ms_diffuseColor, 0.0f, 0.0f  },
		{ ex, sy, ez, ms_diffuseColor, 0.0f, 0.0f  },
		{ sx, ey, ez, ms_diffuseColor, 0.0f, 0.0f  },
		{ ex, ey, ez, ms_diffuseColor, 0.0f, 0.0f  },
	};

	if (SetPDTStream(vertices, 8))
	{
		SHADERMANAGER.SetDefaultTexture(0);
		SHADERMANAGER.SetShaderResource(1, NULL);
		SHADERMANAGER.SetMatrix(MATRIX_WORLD, GetActiveMatrixStack()->GetTop());
		SetDefaultIndexBuffer(DEFAULT_IB_FILL_CUBE);
		SHADERMANAGER.DrawIndexed(TOPOLOGY_TRIANGLELIST, 0, 8, 0, 4 * 3);
	}
}

void CScreen::RenderCube(float sx, float sy, float sz, float ex, float ey, float ez, Matrix matRotation)
{
	Vector3 v3Center = Vector3((sx + ex) * 0.5f, (sy + ey) * 0.5f, (sz + ez) * 0.5f);
	Vector3 v3Vertex[8] =
	{
		Vector3(sx, sy, sz),
		Vector3(ex, sy, sz),
		Vector3(sx, ey, sz),
		Vector3(ex, ey, sz),
		Vector3(sx, sy, ez),
		Vector3(ex, sy, ez),
		Vector3(sx, ey, ez),
		Vector3(ex, ey, ez),
	};
	SPDTVertexRaw vertices[8];

	for (int i = 0; i < 8; i++)
	{
		v3Vertex[i] = v3Vertex[i] - v3Center;
		Vec3TransformCoord(&v3Vertex[i], &v3Vertex[i], &matRotation);
		v3Vertex[i] = v3Vertex[i] + v3Center;
		vertices[i].px = v3Vertex[i].x;
		vertices[i].py = v3Vertex[i].y;
		vertices[i].pz = v3Vertex[i].z;
		vertices[i].diffuse = ms_diffuseColor;
		vertices[i].u = 0.0f; vertices[i].v = 0.0f;
	}

	if (SetPDTStream(vertices, 8))
	{
		SHADERMANAGER.SetDefaultTexture(0);
		SHADERMANAGER.SetShaderResource(1, NULL);
		SHADERMANAGER.SetMatrix(MATRIX_WORLD, GetActiveMatrixStack()->GetTop());
		SetDefaultIndexBuffer(DEFAULT_IB_FILL_CUBE);
		SHADERMANAGER.DrawIndexed(TOPOLOGY_TRIANGLELIST, 0, 8, 0, 4 * 3);
	}
}

void CScreen::RenderLine2d(float sx, float sy, float ex, float ey, float z)
{
	RenderLine3d(sx, sy, z, ex, ey, z);
}

void CScreen::RenderBox2d(float sx, float sy, float ex, float ey, float z)
{
	RenderBox3d(sx, sy, z, ex, ey, z);
}

void CScreen::RenderBar2d(float sx, float sy, float ex, float ey, float z)
{
	RenderBar3d(sx, sy, z, ex, ey, z);
}

void CScreen::RenderGradationBar2d(float sx, float sy, float ex, float ey, DWORD dwStartColor, DWORD dwEndColor, float ez)
{
	RenderGradationBar3d(sx, sy, ez, ex, ey, ez, dwStartColor, dwEndColor);
}

void CScreen::RenderCircle2d(float fx, float fy, float fz, float fRadius, int iStep)
{
	int count;
	float theta, delta;
	float x, y, z;
	std::vector<Vector3> pts;

	pts.clear();
	pts.resize(iStep);

	theta = 0.0;
	delta = 2 * MATH_PI / float(iStep);

	for (count = 0; count < iStep; count++)
	{
		x = fx + fRadius * cosf(theta);
		y = fy + fRadius * sinf(theta);
		z = fz;

		pts[count] = Vector3(x, y, z);

		theta += delta;
	}
	for (count = 0; count < iStep - 1; count++)
	{
		RenderLine3d(pts[count].x, pts[count].y, pts[count].z, pts[count + 1].x, pts[count + 1].y, pts[count + 1].z);
	}
	RenderLine3d(pts[iStep - 1].x, pts[iStep - 1].y, pts[iStep - 1].z, pts[0].x, pts[0].y, pts[0].z);
}

void CScreen::RenderCircle3d(float fx, float fy, float fz, float fRadius, int iStep)
{
	CCamera* pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return;

	int count;
	float theta, delta;
	std::vector<Vector3> pts;

	pts.clear();
	pts.resize(iStep);

	theta = 0.0;
	delta = 2 * MATH_PI / float(iStep);

	const Matrix& c_rmatInvView = pCamera->GetBillboardMatrix();

	for (count = 0; count < iStep; count++)
	{
		pts[count] = Vector3(fRadius * cosf(theta), fRadius * sinf(theta), 0.0f);
		Vec3TransformCoord(&pts[count], &pts[count], &c_rmatInvView);

		theta += delta;
	}
	for (count = 0; count < iStep - 1; count++)
	{
		RenderLine3d(fx + pts[count].x, fy + pts[count].y, fz + pts[count].z,
			fx + pts[count + 1].x, fy + pts[count + 1].y, fz + pts[count + 1].z);
	}
	RenderLine3d(fx + pts[iStep - 1].x, fy + pts[iStep - 1].y, fz + pts[iStep - 1].z,
		fx + pts[0].x, fy + pts[0].y, fz + pts[0].z);
}

void CScreen::RenderSphere(const Matrix* c_pmatWorld, float fx, float fy, float fz, float fRadius, EFillMode fillMode)
{
	const int STACKS = 12;  // Vertical divisions
	const int SLICES = 16;  // Horizontal divisions

	// Save current fill mode and set requested mode
	DWORD savedFillMode = SHADERMANAGER.GetPipelineState(PSTATE_FILLMODE);
	SHADERMANAGER.SetPipelineState(PSTATE_FILLMODE, (fillMode == FILL_SOLID) ? FILL_SOLID : FILL_WIREFRAME);

	// Set world transform if provided
	if (c_pmatWorld)
	{
		Matrix matTranslate;
		MatrixTranslation(&matTranslate, fx, fy, fz);
		Matrix matCombined;
		MatrixMultiply(&matCombined, c_pmatWorld, &matTranslate);
		SHADERMANAGER.SetMatrix(MATRIX_WORLD, &matCombined);
	}

	// Generate sphere vertices
	std::vector<SPDTVertexRaw> vertices;
	vertices.reserve((STACKS + 1) * (SLICES + 1));

	for (int stack = 0; stack <= STACKS; ++stack)
	{
		float phi = MATH_PI * stack / STACKS;  // 0 to PI
		float sinPhi = sinf(phi);
		float cosPhi = cosf(phi);

		for (int slice = 0; slice <= SLICES; ++slice)
		{
			float theta = 2.0f * MATH_PI * slice / SLICES;  // 0 to 2*PI
			float sinTheta = sinf(theta);
			float cosTheta = cosf(theta);

			SPDTVertexRaw v;
			v.px = fRadius * sinPhi * cosTheta;
			v.py = fRadius * sinPhi * sinTheta;
			v.pz = fRadius * cosPhi;
			v.diffuse = ms_diffuseColor;
			v.u = (float)slice / SLICES;
			v.v = (float)stack / STACKS;
			vertices.push_back(v);
		}
	}

	// Generate indices for triangle strip rendering
	std::vector<WORD> indices;
	for (int stack = 0; stack < STACKS; ++stack)
	{
		for (int slice = 0; slice <= SLICES; ++slice)
		{
			indices.push_back((WORD)(stack * (SLICES + 1) + slice));
			indices.push_back((WORD)((stack + 1) * (SLICES + 1) + slice));
		}
		if (stack < STACKS - 1)
		{
			indices.push_back((WORD)((stack + 1) * (SLICES + 1) + SLICES));
			indices.push_back((WORD)((stack + 1) * (SLICES + 1)));
		}
	}

	// Render
	SHADERMANAGER.SetDefaultTexture(0);
	SHADERMANAGER.SetShaderResource(1, NULL);
	SHADERMANAGER.BeginMesh();
	SHADERMANAGER.DrawIndexedDynamic(TOPOLOGY_TRIANGLESTRIP, 0, (UINT)vertices.size(),
		(UINT)(indices.size() - 2), indices.data(), DXGI_FORMAT_R16_UINT,
		vertices.data(), sizeof(SPDTVertexRaw));

	// Restore fill mode
	SHADERMANAGER.SetPipelineState(PSTATE_FILLMODE, savedFillMode);
}

void CScreen::RenderCylinder(const Matrix* c_pmatWorld, float fx, float fy, float fz, float fRadius, float fLength, EFillMode fillMode)
{
	// Procedural cylinder rendering
	const int SLICES = 16;  // Number of sides

	// Save current fill mode and set requested mode
	DWORD savedFillMode = SHADERMANAGER.GetPipelineState(PSTATE_FILLMODE);
	SHADERMANAGER.SetPipelineState(PSTATE_FILLMODE, (fillMode == FILL_SOLID) ? FILL_SOLID : FILL_WIREFRAME);

	// Set world transform if provided
	if (c_pmatWorld)
	{
		Matrix matTranslate;
		MatrixTranslation(&matTranslate, fx, fy, fz);
		Matrix matCombined;
		MatrixMultiply(&matCombined, c_pmatWorld, &matTranslate);
		SHADERMANAGER.SetMatrix(MATRIX_WORLD, &matCombined);
	}

	// Generate cylinder vertices (2 rings: bottom and top)
	std::vector<SPDTVertexRaw> vertices;
	vertices.reserve((SLICES + 1) * 2 + 2);  // Side vertices + center vertices for caps

	float halfLength = fLength * 0.5f;

	// Bottom ring
	for (int i = 0; i <= SLICES; ++i)
	{
		float theta = 2.0f * MATH_PI * i / SLICES;
		SPDTVertexRaw v;
		v.px = fRadius * cosf(theta);
		v.py = fRadius * sinf(theta);
		v.pz = -halfLength;
		v.diffuse = ms_diffuseColor;
		v.u = (float)i / SLICES;
		v.v = 0.0f;
		vertices.push_back(v);
	}

	// Top ring
	for (int i = 0; i <= SLICES; ++i)
	{
		float theta = 2.0f * MATH_PI * i / SLICES;
		SPDTVertexRaw v;
		v.px = fRadius * cosf(theta);
		v.py = fRadius * sinf(theta);
		v.pz = halfLength;
		v.diffuse = ms_diffuseColor;
		v.u = (float)i / SLICES;
		v.v = 1.0f;
		vertices.push_back(v);
	}

	// Generate indices for cylinder body (triangle strip)
	std::vector<WORD> indices;
	for (int i = 0; i <= SLICES; ++i)
	{
		indices.push_back((WORD)i);                    // Bottom ring
		indices.push_back((WORD)(i + SLICES + 1));     // Top ring
	}

	// Render cylinder body
	SHADERMANAGER.SetDefaultTexture(0);
	SHADERMANAGER.SetShaderResource(1, NULL);
	SHADERMANAGER.BeginMesh();
	SHADERMANAGER.DrawIndexedDynamic(TOPOLOGY_TRIANGLESTRIP, 0, (UINT)vertices.size(),
		(UINT)(indices.size() - 2), indices.data(), DXGI_FORMAT_R16_UINT,
		vertices.data(), sizeof(SPDTVertexRaw));

	// Render caps using triangle fans (converted to triangles)
	// Bottom cap center
	SPDTVertexRaw centerBottom = { 0.0f, 0.0f, -halfLength, ms_diffuseColor, 0.5f, 0.5f };
	SPDTVertexRaw centerTop = { 0.0f, 0.0f, halfLength, ms_diffuseColor, 0.5f, 0.5f };

	// Bottom cap triangles
	std::vector<SPDTVertexRaw> capVerts;
	for (int i = 0; i < SLICES; ++i)
	{
		capVerts.push_back(centerBottom);
		capVerts.push_back(vertices[i + 1]);  // Next vertex (reversed winding)
		capVerts.push_back(vertices[i]);      // Current vertex
	}

	// Top cap triangles
	int topRingStart = SLICES + 1;
	for (int i = 0; i < SLICES; ++i)
	{
		capVerts.push_back(centerTop);
		capVerts.push_back(vertices[topRingStart + i]);      // Current vertex
		capVerts.push_back(vertices[topRingStart + i + 1]);  // Next vertex
	}

	// Render caps
	SHADERMANAGER.DrawDynamic(TOPOLOGY_TRIANGLELIST, (UINT)(capVerts.size() / 3),
		capVerts.data(), sizeof(SPDTVertexRaw));

	// Restore fill mode
	SHADERMANAGER.SetPipelineState(PSTATE_FILLMODE, savedFillMode);
}

void CScreen::RenderTextureBox(float sx, float sy, float ex, float ey, float z, float su, float sv, float eu, float ev)
{
	TPDTVertex vertices[4];

	vertices[0].position = TPosition(sx, sy, z);
	vertices[0].diffuse = ms_diffuseColor;
	vertices[0].texCoord = TTextureCoordinate(su, sv);

	vertices[1].position = TPosition(ex, sy, z);
	vertices[1].diffuse = ms_diffuseColor;
	vertices[1].texCoord = TTextureCoordinate(eu, sv);

	vertices[2].position = TPosition(sx, ey, z);
	vertices[2].diffuse = ms_diffuseColor;
	vertices[2].texCoord = TTextureCoordinate(su, ev);

	vertices[3].position = TPosition(ex, ey, z);
	vertices[3].diffuse = ms_diffuseColor;
	vertices[3].texCoord = TTextureCoordinate(eu, ev);

	SetDefaultIndexBuffer(DEFAULT_IB_FILL_RECT);
	if (SetPDTStream(vertices, 4))
		SHADERMANAGER.DrawIndexed(TOPOLOGY_TRIANGLELIST, 0, 4, 0, 2);
}

void CScreen::RenderBillboard(Vector3* Position, Color& Color)
{
	TPDTVertex vertices[4];
	vertices[0].position = TPosition(Position[0].x, Position[0].y, Position[0].z);
	vertices[0].diffuse = Color;
	vertices[0].texCoord = TTextureCoordinate(0, 0);

	vertices[1].position = TPosition(Position[1].x, Position[1].y, Position[1].z);
	vertices[1].diffuse = Color;
	vertices[1].texCoord = TTextureCoordinate(1, 0);

	vertices[2].position = TPosition(Position[2].x, Position[2].y, Position[2].z);
	vertices[2].diffuse = Color;
	vertices[2].texCoord = TTextureCoordinate(0, 1);

	vertices[3].position = TPosition(Position[3].x, Position[3].y, Position[3].z);
	vertices[3].diffuse = Color;
	vertices[3].texCoord = TTextureCoordinate(1, 1);

	SetDefaultIndexBuffer(DEFAULT_IB_FILL_RECT);
	if (SetPDTStream(vertices, 4))
		SHADERMANAGER.DrawIndexed(TOPOLOGY_TRIANGLELIST, 0, 4, 0, 2);
}

void CScreen::DrawMinorGrid(float xMin, float yMin, float xMax, float yMax, float xminorStep, float yminorStep, float zPos)
{
	float x, y;

	for (y = yMin; y <= yMax; y += yminorStep)
		RenderLine2d(xMin, y, xMax, y, zPos);

	for (x = xMin; x <= xMax; x += xminorStep)
		RenderLine2d(x, yMin, x, yMax, zPos);
}

void CScreen::DrawGrid(float xMin, float yMin, float xMax, float yMax, float xmajorStep, float ymajorStep, float xminorStep, float yminorStep, float zPos)
{
	xMin *= xminorStep;
	xMax *= xminorStep;
	yMin *= yminorStep;
	yMax *= yminorStep;
	xmajorStep *= xminorStep;
	ymajorStep *= yminorStep;

	float x, y;

	SetDiffuseColor(0.5f, 0.5f, 0.5f);
	DrawMinorGrid(xMin, yMin, xMax, yMax, xminorStep, yminorStep, zPos);

	SetDiffuseColor(0.7f, 0.7f, 0.7f);
	for (y = 0.0f; y >= yMin; y -= ymajorStep)
		RenderLine2d(xMin, y, xMax, y, zPos);

	for (y = 0.0f; y <= yMax; y += ymajorStep)
		RenderLine2d(xMin, y, xMax, y, zPos);

	for (x = 0.0f; x >= xMin; x -= xmajorStep)
		RenderLine2d(x, yMin, x, yMax, zPos);

	for (x = 0.0f; x <= yMax; x += xmajorStep)
		RenderLine2d(x, yMin, x, yMax, zPos);

	SetDiffuseColor(1.0f, 1.0f, 1.0f);
	RenderLine2d(xMin, 0.0f, xMax, 0.0f, zPos);
	RenderLine2d(0.0f, yMin, 0.0f, yMax, zPos);
}

void CScreen::SetCursorPosition(int x, int y, int hres, int vres)
{
	Vector3 v;
	v.x = -(((2.0f * x) / hres) - 1) / ms_matProj._11;
	v.y = (((2.0f * y) / vres) - 1) / ms_matProj._22;
	v.z = 1.0f;

	Matrix matViewInverse = ms_matInverseView;

	ms_vtPickRayDir.x = v.x * matViewInverse._11 +
		v.y * matViewInverse._21 +
		v.z * matViewInverse._31;

	ms_vtPickRayDir.y = v.x * matViewInverse._12 +
		v.y * matViewInverse._22 +
		v.z * matViewInverse._32;

	ms_vtPickRayDir.z = v.x * matViewInverse._13 +
		v.y * matViewInverse._23 +
		v.z * matViewInverse._33;

	ms_vtPickRayOrig.x = matViewInverse._41;
	ms_vtPickRayOrig.y = matViewInverse._42;
	ms_vtPickRayOrig.z = matViewInverse._43;

	ms_Ray.SetStartPoint(ms_vtPickRayOrig);
	ms_Ray.SetDirection(-ms_vtPickRayDir, 51200.0f);
}

bool CScreen::GetCursorPosition(float* px, float* py, float* pz)
{
	if (!GetCursorXYPosition(px, py)) return false;
	if (!GetCursorZPosition(pz)) return false;
	return true;
}

bool CScreen::GetCursorXYPosition(float* px, float* py)
{
	CCamera* pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return false;

	Vector3 v3Eye = pCamera->GetEye();

	TPosition posVertices[4];
	posVertices[0] = TPosition(v3Eye.x - 90000000.0f, v3Eye.y + 90000000.0f, 0.0f);
	posVertices[1] = TPosition(v3Eye.x - 90000000.0f, v3Eye.y - 90000000.0f, 0.0f);
	posVertices[2] = TPosition(v3Eye.x + 90000000.0f, v3Eye.y + 90000000.0f, 0.0f);
	posVertices[3] = TPosition(v3Eye.x + 90000000.0f, v3Eye.y - 90000000.0f, 0.0f);

	static const WORD sc_awFillRectIndices[6] = { 0, 2, 1, 2, 3, 1, };

	float u, v, t;
	for (int i = 0; i < 2; ++i)
	{
		if (IntersectTriangle(ms_vtPickRayOrig, ms_vtPickRayDir,
			posVertices[sc_awFillRectIndices[i * 3 + 0]],
			posVertices[sc_awFillRectIndices[i * 3 + 1]],
			posVertices[sc_awFillRectIndices[i * 3 + 2]],
			&u, &v, &t))
		{
			*px = ms_vtPickRayOrig.x + ms_vtPickRayDir.x * t;
			*py = ms_vtPickRayOrig.y + ms_vtPickRayDir.y * t;
			return true;
		}
	}
	return false;
}

bool CScreen::GetCursorZPosition(float* pz)
{
	CCamera* pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return false;

	Vector3 v3Eye = pCamera->GetEye();

	TPosition posVertices[4];
	posVertices[0] = TPosition(v3Eye.x - 90000000.0f, 0.0f, v3Eye.z + 90000000.0f);
	posVertices[1] = TPosition(v3Eye.x - 90000000.0f, 0.0f, v3Eye.z - 90000000.0f);
	posVertices[2] = TPosition(v3Eye.x + 90000000.0f, 0.0f, v3Eye.z + 90000000.0f);
	posVertices[3] = TPosition(v3Eye.x + 90000000.0f, 0.0f, v3Eye.z - 90000000.0f);

	static const WORD sc_awFillRectIndices[6] = { 0, 2, 1, 2, 3, 1, };

	float u, v, t;
	for (int i = 0; i < 2; ++i)
	{
		if (IntersectTriangle(ms_vtPickRayOrig, ms_vtPickRayDir,
			posVertices[sc_awFillRectIndices[i * 3 + 0]],
			posVertices[sc_awFillRectIndices[i * 3 + 1]],
			posVertices[sc_awFillRectIndices[i * 3 + 2]],
			&u, &v, &t))
		{
			*pz = ms_vtPickRayOrig.z + ms_vtPickRayDir.z * t;
			return true;
		}
	}
	return false;
}

void CScreen::GetPickingPosition(float t, float* x, float* y, float* z)
{
	*x = ms_vtPickRayOrig.x + ms_vtPickRayDir.x * t;
	*y = ms_vtPickRayOrig.y + ms_vtPickRayDir.y * t;
	*z = ms_vtPickRayOrig.z + ms_vtPickRayDir.z * t;
}

void CScreen::SetDiffuseColor(DWORD diffuseColor)
{
	ms_diffuseColor = diffuseColor;
}

void CScreen::SetDiffuseColor(float r, float g, float b, float a)
{
	ms_diffuseColor = GetColor(r, g, b, a);
}

void CScreen::SetClearColor(float r, float g, float b, float a)
{
	ms_clearColor[0] = r;
	ms_clearColor[1] = g;
	ms_clearColor[2] = b;
	ms_clearColor[3] = a;
}

void CScreen::SetClearDepth(float depth)
{
	ms_clearDepth = depth;
}

void CScreen::SetClearStencil(DWORD stencil)
{
	ms_clearStencil = stencil;
}

void CScreen::ClearDepthBuffer()
{
	ID3D11DepthStencilView* pDSV = GetDepthStencilView();
	if (ms_pContext && pDSV)
	{
		ms_pContext->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH, ms_clearDepth, 0);
	}
}

void CScreen::ClearStencilBuffer()
{
	ID3D11DepthStencilView* pDSV = GetDepthStencilView();
	if (ms_pContext && pDSV)
	{
		ms_pContext->ClearDepthStencilView(pDSV, D3D11_CLEAR_STENCIL, 1.0f, static_cast<UINT8>(ms_clearStencil));
	}
}

void CScreen::ClearRenderTarget()
{
	ID3D11RenderTargetView* pRTV = GetRenderTargetView();
	if (ms_pContext && pRTV)
	{
		ms_pContext->ClearRenderTargetView(pRTV, ms_clearColor);
	}
}

void CScreen::Clear()
{
	if (ms_pContext)
	{
		ID3D11RenderTargetView* pRTV = GetRenderTargetView();
		ID3D11DepthStencilView* pDSV = GetDepthStencilView();

		if (pRTV)
			ms_pContext->ClearRenderTargetView(pRTV, ms_clearColor);

		if (pDSV)
			ms_pContext->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, ms_clearDepth, static_cast<UINT8>(ms_clearStencil));
	}
}

BOOL CScreen::IsDeviceRemoved()
{
	if (!ms_pDevice)
		return TRUE;

	HRESULT hr = ms_pDevice->GetDeviceRemovedReason();
	return FAILED(hr) ? TRUE : FALSE;
}

BOOL CScreen::CheckDeviceState()
{
	// Only device removed (driver crash/update)
	if (!ms_pDevice)
		return FALSE;

	HRESULT hr = ms_pDevice->GetDeviceRemovedReason();
	if (FAILED(hr))
	{
		// Device was removed - would need to recreate device
		return FALSE;
	}

	return TRUE;
}

bool CScreen::Begin()
{
	if (!ms_pContext)
		return false;

	ResetFaceCount();
	return true;
}

void CScreen::End()
{
}

void CScreen::Present(bool bVSync)
{
	if (ms_pSwapChain)
	{
#ifdef ENABLE_MSAA
		ResolveMSAA();  // Always resolve to swap chain (safety fallback)
#ifdef ENABLE_BLOOM
		if (ms_bBloomEnabled && ms_pSceneTextureSRV)
		{
			// Separate resolve to scene texture for bloom input
			ResolveMSAAToSceneTexture();

#ifdef ENABLE_SSAO
			ID3D11ShaderResourceView* pSSAO_SRV = nullptr;
			if (ms_bSSAOEnabled && ms_pSSAO_RTV && SHADERMANAGER.GetFrameCount() > 15)
			{
				UINT fullW = (UINT)ms_Viewport.Width;
				UINT fullH = (UINT)ms_Viewport.Height;
				UINT halfW = max(1u, fullW / 2);
				UINT halfH = max(1u, fullH / 2);

				// Resolve MSAA depth if needed
				if (ms_uMSAASampleCount > 1 && ms_pMSAADepthStencilSRV && ms_pResolvedDepthRTV)
				{
					SHADERMANAGER.RenderDepthResolve(ms_pMSAADepthStencilSRV, ms_pResolvedDepthRTV, fullW, fullH);
				}

				ID3D11ShaderResourceView* pDepthSRV = GetDepthSRV();
				if (pDepthSRV)
				{
					SHADERMANAGER.RenderSSAOPass(pDepthSRV, ms_pSSAO_RTV, halfW, halfH, fullW, fullH);
					SHADERMANAGER.RenderSSAOBlur(ms_pSSAO_SRV, pDepthSRV, ms_pSSAOBlur_RTV, halfW, halfH);
					pSSAO_SRV = ms_pSSAOBlur_SRV;
				}
			}
#endif

#ifdef ENABLE_GODRAYS
			if (SHADERMANAGER.IsGodRaysEnabled() && ms_pGodRaysRTV)
			{
				SHADERMANAGER.RenderGodRaysPass(
					ms_pSceneTextureSRV, ms_pGodRaysRTV,
					max(1u, (UINT)ms_Viewport.Width / 4),
					max(1u, (UINT)ms_Viewport.Height / 4));
			}
			ID3D11ShaderResourceView* pGodRaysSRV = ms_pGodRaysSRV;
#else
			ID3D11ShaderResourceView* pGodRaysSRV = nullptr;
#endif

#ifndef ENABLE_SSAO
			ID3D11ShaderResourceView* pSSAO_SRV = nullptr;
#endif

			ID3D11RenderTargetView* pSwapRTV = GetSwapChainRenderTargetView();
			SHADERMANAGER.RenderBloom(
				ms_pSceneTextureSRV,
				ms_pBloomRTA_RTV, ms_pBloomRTA_SRV,
				ms_pBloomRTB_RTV, ms_pBloomRTB_SRV,
				max(1u, (UINT)ms_Viewport.Width / 4), max(1u, (UINT)ms_Viewport.Height / 4),
				pGodRaysSRV,
				pSSAO_SRV,
				pSwapRTV, (UINT)ms_Viewport.Width, (UINT)ms_Viewport.Height);
		}
#endif
#endif
		UINT syncInterval = bVSync ? 1 : 0;
		HRESULT hr = ms_pSwapChain->Present(syncInterval, 0);

		if (FAILED(hr))
		{
			HRESULT hrReason = ms_pDevice->GetDeviceRemovedReason();
			TraceError("DX11 ERROR: Present failed hr=0x%08X, DeviceRemovedReason=0x%08X", hr, hrReason);
		}
	}

	SHADERMANAGER.OnFrameComplete();

	SHADERMANAGER.ResetDynamicBuffers();
}

void CScreen::Show(HWND hWnd)
{
	Present(true);
}

void CScreen::Show(RECT* pSrcRect)
{
	// Present the entire back buffer
	Present(true);
}

void CScreen::Show(RECT* pSrcRect, HWND hWnd)
{
	// DX11 doesn't support partial present to specific window
	Present(true);
}

void CScreen::ProjectPosition(float x, float y, float z, float* pfX, float* pfY)
{
	Vector3 Input(x, y, z);
	Vector3 Output;

	// Transform through world, view, projection
	Matrix matWVP;
	MatrixMultiply(&matWVP, &ms_matWorld, &ms_matView);
	MatrixMultiply(&matWVP, &matWVP, &ms_matProj);

	Vec3TransformCoord(&Output, &Input, &matWVP);

	// Map to viewport
	*pfX = (Output.x + 1.0f) * 0.5f * ms_Viewport.Width + ms_Viewport.TopLeftX;
	*pfY = (1.0f - Output.y) * 0.5f * ms_Viewport.Height + ms_Viewport.TopLeftY;
}

void CScreen::ProjectPosition(float x, float y, float z, float* pfX, float* pfY, float* pfZ)
{
	Vector3 Input(x, y, z);
	Vector3 Output;

	// Transform through world, view, projection
	Matrix matWVP;
	MatrixMultiply(&matWVP, &ms_matWorld, &ms_matView);
	MatrixMultiply(&matWVP, &matWVP, &ms_matProj);

	Vec3TransformCoord(&Output, &Input, &matWVP);

	// Map to viewport
	*pfX = (Output.x + 1.0f) * 0.5f * ms_Viewport.Width + ms_Viewport.TopLeftX;
	*pfY = (1.0f - Output.y) * 0.5f * ms_Viewport.Height + ms_Viewport.TopLeftY;
	*pfZ = Output.z * (ms_Viewport.MaxDepth - ms_Viewport.MinDepth) + ms_Viewport.MinDepth;
}

void CScreen::UnprojectPosition(float x, float y, float z, float* pfX, float* pfY, float* pfZ)
{
	// Unmap from viewport to normalized device coordinates
	Vector3 v;
	v.x = (x - ms_Viewport.TopLeftX) / ms_Viewport.Width * 2.0f - 1.0f;
	v.y = 1.0f - (y - ms_Viewport.TopLeftY) / ms_Viewport.Height * 2.0f;
	v.z = (z - ms_Viewport.MinDepth) / (ms_Viewport.MaxDepth - ms_Viewport.MinDepth);

	// Create inverse WVP matrix
	Matrix matWVP, matInvWVP;
	MatrixMultiply(&matWVP, &ms_matWorld, &ms_matView);
	MatrixMultiply(&matWVP, &matWVP, &ms_matProj);
	MatrixInverse(&matInvWVP, NULL, &matWVP);

	// Transform back to world space
	Vector3 Output;
	Vec3TransformCoord(&Output, &v, &matInvWVP);

	*pfX = Output.x;
	*pfY = Output.y;
	*pfZ = Output.z;
}

void CScreen::SetColorOperation()
{
	SHADERMANAGER.SetDefaultTexture(0);
}

void CScreen::SetDiffuseOperation()
{
	SHADERMANAGER.SetDefaultTexture(0);
}

void CScreen::SetBlendOperation()
{
	SHADERMANAGER.SetDefaultTexture(0);
}

void CScreen::SetOneColorOperation(Color& rColor)
{
	SHADERMANAGER.SetDefaultTexture(0);
	SHADERMANAGER.SetShaderResource(1, NULL);
	if (SHADERMANAGER.IsInitialized())
		SHADERMANAGER.SetParticleColor(rColor);
}

void CScreen::SetAddColorOperation(Color& rColor)
{
	if (SHADERMANAGER.IsInitialized())
		SHADERMANAGER.SetParticleColor(rColor);
}

void CScreen::Identity()
{
	SHADERMANAGER.SetMatrix(MATRIX_WORLD, &ms_matIdentity);
}

CScreen::CScreen()
{
}

CScreen::~CScreen()
{
}

void CScreen::BuildViewFrustum()
{
	CCamera* pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return;

	auto val = ms_matView * ms_matProj;
	const Vector3& c_rv3Eye = pCamera->GetEye();
	const Vector3& c_rv3View = pCamera->GetView();
	ms_frustum.BuildViewFrustum2(
		val,
		ms_fNearY,
		ms_fFarY,
		ms_fFieldOfView,
		ms_fAspect,
		c_rv3Eye, c_rv3View);
}
