#pragma once

/**
 * ImGuiNameRenderer.h - Batched Name Rendering System (DX11)
 * High-performance text batching for player names, guild names, etc.
 */

#include <string>
#include <vector>

struct ImFont;
struct ImDrawList;

class CImGuiNameRenderer
{
public:
	static constexpr float NAME_MAX_DISTANCE = 3500.0f;
	static constexpr float SHADOW_OFFSET_X = 1.0f;
	static constexpr float SHADOW_OFFSET_Y = 1.0f;
	static constexpr unsigned int SHADOW_COLOR = 0xB0000000;

	enum class ETextType : int
	{
		Name = 0,
		Guild,
		Title,
		Level,
		CustomTitle,
		CustomAlign,
		Chat,
		Info,
		Item,
	};

	struct STextElement
	{
		ETextType type;
		std::string text;
		float x, y, z;
		unsigned int color;
		unsigned int outlineColor;
		bool hasOutline;
		bool hasShadow;
		int horizontalAlign;
		int verticalAlign;
		DWORD virtualID;
		float xStart, yStart, xEnd, yEnd;
		bool hasBox;
	};

public:
	CImGuiNameRenderer();
	~CImGuiNameRenderer();

	static CImGuiNameRenderer& Instance();

	void BeginBatch();
	void EndBatch();

	void AddText(
		ETextType type,
		const char* szText,
		float x, float y, float z,
		DWORD dwColorARGB,
		bool hasOutline = true,
		DWORD dwOutlineColorARGB = 0xFF000000,
		int hAlign = 1,
		int vAlign = 2,
		DWORD virtualID = 0
	);

	void AddTextWithShadow(
		ETextType type,
		const char* szText,
		float x, float y, float z,
		DWORD dwColorARGB,
		DWORD virtualID = 0
	);

	void AddItemBox(
		const char* szText,
		float x, float y, float z,
		DWORD dwColorARGB,
		float xStart, float yStart, float xEnd, float yEnd,
		DWORD virtualID = 0
	);

	void GetTextSize(const char* szText, int* outWidth, int* outHeight);

	int GetDrawCallCount() const { return m_iDrawCallCount; }
	int GetTextCount() const { return static_cast<int>(m_vecTextElements.size()); }

private:
	void __RenderTextElement(ImDrawList* pDrawList, const STextElement& elem);
	void __RenderItemBox(ImDrawList* pDrawList, const STextElement& elem);

	static constexpr unsigned int __ConvertARGBtoABGR(DWORD argb) noexcept
	{
		const unsigned char a = static_cast<unsigned char>((argb >> 24) & 0xFF);
		const unsigned char r = static_cast<unsigned char>((argb >> 16) & 0xFF);
		const unsigned char g = static_cast<unsigned char>((argb >> 8) & 0xFF);
		const unsigned char b = static_cast<unsigned char>((argb >> 0) & 0xFF);

		return (static_cast<unsigned int>(a) << 24) |
			   (static_cast<unsigned int>(b) << 16) |
			   (static_cast<unsigned int>(g) << 8) |
			   (static_cast<unsigned int>(r) << 0);
	}

private:
	std::vector<STextElement> m_vecTextElements;
	int m_iDrawCallCount;
	bool m_bBatchActive;
};

// Global accessor macro
#define IMGUI_NAME_RENDERER CImGuiNameRenderer::Instance()
