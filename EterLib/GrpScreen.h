#pragma once

/*
 * GrpScreen.h
 * DX11 Screen Management (Clear, Present, Rendering utilities)
 */

#include "GrpCollisionObject.h"
#include "GrpMathType.h"
#include "GrpStateEnum.h"
#include "../SphereLib/frustum.h"

class CScreen : public CGraphicCollisionObject
{
public:
	CScreen();
	virtual ~CScreen();

	// Clear operations
	void ClearDepthBuffer();
	void ClearStencilBuffer();
	void Clear();
	void ClearRenderTarget();

	// Frame begin/end
	bool Begin();
	void End();

	// Present to screen
	void Show(HWND hWnd = NULL);
	void Show(RECT* pSrcRect);
	void Show(RECT* pSrcRect, HWND hWnd);

	// Present with vsync control
	void Present(bool bVSync = true);

	// 2D Rendering
	void RenderLine2d(float sx, float sy, float ex, float ey, float z = 0.0f);
	void RenderBox2d(float sx, float sy, float ex, float ey, float z = 0.0f);
	void RenderBar2d(float sx, float sy, float ex, float ey, float z = 0.0f);
	void RenderGradationBar2d(float sx, float sy, float ex, float ey, DWORD dwStartColor, DWORD dwEndColor, float ez = 0.0f);
	void RenderCircle2d(float fx, float fy, float fz, float fRadius, int iStep = 50);

	// 3D Rendering
	void RenderLine3d(float sx, float sy, float sz, float ex, float ey, float ez);
	void RenderBox3d(float sx, float sy, float sz, float ex, float ey, float ez);
	void RenderBar3d(float sx, float sy, float sz, float ex, float ey, float ez);
	void RenderBar3d(const Vector3* c_pv3Positions);
	void RenderGradationBar3d(float sx, float sy, float sz, float ex, float ey, float ez, DWORD dwStartColor, DWORD dwEndColor);
	void RenderCircle3d(float fx, float fy, float fz, float fRadius, int iStep = 50);

	void RenderLineCube(float sx, float sy, float sz, float ex, float ey, float ez);
	void RenderCube(float sx, float sy, float sz, float ex, float ey, float ez);
	void RenderCube(float sx, float sy, float sz, float ex, float ey, float ez, Matrix matRotation);
	void RenderTextureBox(float sx, float sy, float ex, float ey, float z = 0.0f, float su = 0.0f, float sv = 0.0f, float eu = 1.0f, float ev = 1.0f);
	void RenderBillboard(Vector3* Position, Color& color);

	// Grid drawing
	void DrawMinorGrid(float xMin, float yMin, float xMax, float yMax, float xminorStep, float yminorStep, float zPos = 0);
	void DrawGrid(float xMin, float yMin, float xMax, float yMax, float xmajorStep, float ymajorStep, float xminorStep, float yminorStep, float zPos = 0);

	// Debug shape rendering
	void RenderSphere(const Matrix* c_pmatWorld, float fx, float fy, float fz, float fRadius, EFillMode fillMode = FILL_SOLID);
	void RenderCylinder(const Matrix* c_pmatWorld, float fx, float fy, float fz, float fRadius, float fLength, EFillMode fillMode = FILL_SOLID);

	// Render state operations
	void SetColorOperation();
	void SetDiffuseOperation();
	void SetBlendOperation();
	void SetOneColorOperation(Color& rColor);
	void SetAddColorOperation(Color& rColor);
	void SetDiffuseColor(DWORD diffuseColor);
	void SetDiffuseColor(float r, float g, float b, float a = 1.0f);
	void SetClearColor(float r, float g, float b, float a = 1.0f);
	void SetClearDepth(float depth);
	void SetClearStencil(DWORD stencil);

	// Picking/Projection
	void SetCursorPosition(int x, int y, int hres, int vres);
	bool GetCursorPosition(float* px, float* py, float* pz);
	bool GetCursorXYPosition(float* px, float* py);
	bool GetCursorZPosition(float* pz);
	void GetPickingPosition(float t, float* x, float* y, float* z);
	void ProjectPosition(float x, float y, float z, float* pfX, float* pfY);
	void ProjectPosition(float x, float y, float z, float* pfX, float* pfY, float* pfZ);
	void UnprojectPosition(float x, float y, float z, float* pfX, float* pfY, float* pfZ);

	// Device state (DX11: no device lost, only device removed)
	BOOL IsDeviceRemoved();
	BOOL CheckDeviceState();

	// Frustum
	void BuildViewFrustum();

	static void Identity();
	static Frustum& GetFrustum() { return ms_frustum; }

protected:
	static DWORD    ms_diffuseColor;
	static float    ms_clearColor[4];  // RGBA float array for DX11
	static DWORD    ms_clearStencil;
	static float    ms_clearDepth;

	static Frustum ms_frustum;
};
