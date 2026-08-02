/**
 * ImGuiNameRenderer.cpp - Batched Name Rendering Implementation (DX11)
 * High-performance text batching for player names, guild names, etc.
 */

#include "StdAfx.h"
#include "ImGuiNameRenderer.h"
#include "ImGuiManager.h"

#include "imgui.h"

CImGuiNameRenderer& CImGuiNameRenderer::Instance()
{
	static CImGuiNameRenderer instance;
	return instance;
}

CImGuiNameRenderer::CImGuiNameRenderer()
	: m_iDrawCallCount(0)
	, m_bBatchActive(false)
{
	m_vecTextElements.reserve(500);
}

CImGuiNameRenderer::~CImGuiNameRenderer()
{
	m_vecTextElements.clear();
}

void CImGuiNameRenderer::BeginBatch()
{
	m_vecTextElements.clear();
	m_iDrawCallCount = 0;
	m_bBatchActive = true;
}

void CImGuiNameRenderer::EndBatch()
{
	if (!m_bBatchActive)
		return;

	m_bBatchActive = false;

	if (m_vecTextElements.empty())
		return;

	// Use BackgroundDrawList so text renders BEHIND UI windows
	ImDrawList* pDrawList = ImGui::GetBackgroundDrawList();
	if (!pDrawList)
		return;

	for (const auto& elem : m_vecTextElements)
	{
		__RenderTextElement(pDrawList, elem);
	}

	m_iDrawCallCount = 1;

	CImGuiManager::Instance().FlushAndRestart();
}

// Convert from game's code page to UTF-8 for ImGui
static std::string ToUtf8(const char* szText)
{
	if (!szText || !szText[0])
		return {};

	// Use CP_1250 (Central European) for Romanian characters
	constexpr DWORD gameCodePage = 1250;

	// Convert from CP_1250 to wide char
	int wideLen = MultiByteToWideChar(gameCodePage, 0, szText, -1, nullptr, 0);
	if (wideLen <= 0)
		return szText;

	std::wstring wideStr(wideLen, L'\0');
	MultiByteToWideChar(gameCodePage, 0, szText, -1, &wideStr[0], wideLen);

	// Then convert wide char to UTF-8
	int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (utf8Len <= 0)
		return szText;

	std::string utf8Str(utf8Len, '\0');
	WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, &utf8Str[0], utf8Len, nullptr, nullptr);

	// Remove null terminator from string length
	if (!utf8Str.empty() && utf8Str.back() == '\0')
		utf8Str.pop_back();

	return utf8Str;
}

void CImGuiNameRenderer::AddText(
	ETextType type,
	const char* szText,
	float x, float y, float z,
	DWORD dwColorARGB,
	bool hasOutline,
	DWORD dwOutlineColorARGB,
	int hAlign,
	int vAlign,
	DWORD virtualID)
{
	if (!m_bBatchActive || !szText || !szText[0])
		return;

	STextElement elem;
	elem.type = type;
	elem.text = ToUtf8(szText);
	elem.x = x;
	elem.y = y;
	elem.z = z;
	elem.color = __ConvertARGBtoABGR(dwColorARGB);
	elem.outlineColor = __ConvertARGBtoABGR(dwOutlineColorARGB);
	elem.hasOutline = hasOutline;
	elem.hasShadow = false;
	elem.horizontalAlign = hAlign;
	elem.verticalAlign = vAlign;
	elem.virtualID = virtualID;
	elem.hasBox = false;

	m_vecTextElements.push_back(std::move(elem));
}

void CImGuiNameRenderer::AddTextWithShadow(
	ETextType type,
	const char* szText,
	float x, float y, float z,
	DWORD dwColorARGB,
	DWORD virtualID)
{
	if (!m_bBatchActive || !szText || !szText[0])
		return;

	STextElement elem;
	elem.type = type;
	elem.text = ToUtf8(szText);
	elem.x = x;
	elem.y = y;
	elem.z = z;
	elem.color = __ConvertARGBtoABGR(dwColorARGB);
	elem.outlineColor = SHADOW_COLOR;
	elem.hasOutline = false;
	elem.hasShadow = true;
	elem.horizontalAlign = 1;
	elem.verticalAlign = 2;
	elem.virtualID = virtualID;
	elem.hasBox = false;

	m_vecTextElements.push_back(std::move(elem));
}

void CImGuiNameRenderer::AddItemBox(
	const char* szText,
	float x, float y, float z,
	DWORD dwColorARGB,
	float xStart, float yStart, float xEnd, float yEnd,
	DWORD virtualID)
{
	if (!m_bBatchActive || !szText || !szText[0])
		return;

	STextElement elem;
	elem.type = ETextType::Item;
	elem.text = ToUtf8(szText);
	elem.x = x;
	elem.y = y;
	elem.z = z;
	elem.color = __ConvertARGBtoABGR(dwColorARGB);
	elem.outlineColor = 0xFF000000;
	elem.hasOutline = true;
	elem.hasShadow = false;
	elem.horizontalAlign = 1;
	elem.verticalAlign = 1;
	elem.virtualID = virtualID;
	elem.hasBox = true;
	elem.xStart = xStart;
	elem.yStart = yStart;
	elem.xEnd = xEnd;
	elem.yEnd = yEnd;

	m_vecTextElements.push_back(std::move(elem));
}

void CImGuiNameRenderer::GetTextSize(const char* szText, int* outWidth, int* outHeight)
{
	CImGuiManager::Instance().GetTextExtent(szText, outWidth, outHeight);
}

void CImGuiNameRenderer::__RenderTextElement(ImDrawList* pDrawList, const STextElement& elem)
{
	CImGuiManager& imgui = CImGuiManager::Instance();
	ImFont* pFont = imgui.GetFont();
	if (!pFont)
		return;

	float fontSize = imgui.GetActiveFontSize();
	if (fontSize < 1.0f)
		fontSize = 14.0f;

	ImVec2 textSize = pFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f,
		elem.text.c_str(), elem.text.c_str() + elem.text.size());

	float drawX = elem.x;
	float drawY = elem.y;

	// Horizontal alignment
	switch (elem.horizontalAlign)
	{
		case 0: break;                          // Left
		case 1: drawX -= textSize.x * 0.5f; break;  // Center
		case 2: drawX -= textSize.x; break;     // Right
	}

	// Vertical alignment
	switch (elem.verticalAlign)
	{
		case 0: break;                          // Top
		case 1: drawY -= textSize.y * 0.5f; break;  // Middle
		case 2: drawY -= textSize.y; break;     // Bottom
	}

	// Snap to pixel grid for crisp text
	drawX = floorf(drawX);
	drawY = floorf(drawY);

	const char* textStart = elem.text.c_str();
	const char* textEnd = textStart + elem.text.size();

	// Render outline (8-directional)
	if (elem.hasOutline)
	{
		static const float offsets[8][2] = {
			{ 0.0f, -1.0f}, { 1.0f, -1.0f}, { 1.0f,  0.0f}, { 1.0f,  1.0f},
			{ 0.0f,  1.0f}, {-1.0f,  1.0f}, {-1.0f,  0.0f}, {-1.0f, -1.0f}
		};

		for (int i = 0; i < 8; ++i)
		{
			pDrawList->AddText(pFont, fontSize,
				ImVec2(drawX + offsets[i][0], drawY + offsets[i][1]),
				elem.outlineColor,
				textStart, textEnd);
		}
	}
	else if (elem.hasShadow)
	{
		pDrawList->AddText(pFont, fontSize,
			ImVec2(drawX + SHADOW_OFFSET_X, drawY + SHADOW_OFFSET_Y),
			SHADOW_COLOR,
			textStart, textEnd);
	}

	// Render main text
	pDrawList->AddText(pFont, fontSize,
		ImVec2(drawX, drawY),
		elem.color,
		textStart, textEnd);

	// Render item box if needed
	if (elem.hasBox)
	{
		__RenderItemBox(pDrawList, elem);
	}
}

void CImGuiNameRenderer::__RenderItemBox(ImDrawList* pDrawList, const STextElement& elem)
{
	ImU32 boxBgColor = IM_COL32(0, 0, 0, 77);
	ImU32 boxBorderColor = IM_COL32(0, 0, 0, 255);

	float x1 = elem.x + elem.xStart;
	float y1 = elem.y + elem.yStart;
	float x2 = elem.x + elem.xEnd;
	float y2 = elem.y + elem.yEnd;

	pDrawList->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), boxBgColor);
	pDrawList->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), boxBorderColor);
}
