#pragma once

#include <d3d11.h>
#include <string>
#include <memory>
#include <unordered_map>
#include <string_view>
#include <span>

// Forward declarations
struct ImFont;
struct ImDrawList;

// FreeType rendering quality flags
enum class EFreeTypeQuality : unsigned int
{
	Default = 0,
	NoHinting = 1 << 0,
	NoAutoHint = 1 << 1,
	ForceAutoHint = 1 << 2,
	LightHinting = 1 << 3,
	MonoHinting = 1 << 4,
	Bold = 1 << 5,
	Oblique = 1 << 6,
	Monochrome = 1 << 7,
	LoadColor = 1 << 8,

	// Recommended presets
	HighQuality = NoHinting | LightHinting,
	Sharp = NoHinting | NoAutoHint,
	Smooth = LightHinting,
};

// Font configuration structure
struct SFontConfig
{
	std::string name;
	std::string path;
	float size;
	int oversampleH;
	int oversampleV;
	float rasterizerMultiply;
	bool enableOutline;
	int outlineThickness;
	bool pixelSnapH;
	EFreeTypeQuality quality;

	SFontConfig()
		: size(14.0f)
		, oversampleH(4)
		, oversampleV(4)
		, rasterizerMultiply(1.2f)
		, enableOutline(false)
		, outlineThickness(1)
		, pixelSnapH(false)
		, quality(EFreeTypeQuality::HighQuality)
	{
	}
};

// Font entry
struct SFontEntry
{
	ImFont* pFont = nullptr;
	float fontSize = 14.0f;
	int outlineThickness = 1;
};

// Transparent hash for heterogeneous lookup
struct StringHash
{
	using is_transparent = void;
	using hash_type = std::hash<std::string_view>;

	size_t operator()(std::string_view sv) const noexcept { return hash_type{}(sv); }
	size_t operator()(const std::string& s) const noexcept { return hash_type{}(s); }
	size_t operator()(const char* s) const noexcept { return hash_type{}(s); }
};

class CImGuiManager
{
public:
	CImGuiManager();
	~CImGuiManager();

	// Initialization and shutdown (DX11)
	bool Initialize(HWND hWnd, ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	void Shutdown();

	// Font management
	[[nodiscard]] bool LoadFontsFromConfig(std::string_view configPath);
	[[nodiscard]] bool LoadFont(std::string_view fontName, std::string_view fontPath, float fontSize, bool enableOutline = false);
	[[nodiscard]] bool LoadFontFromMemory(std::string_view fontName, std::span<const std::byte> fontData, float fontSize, bool enableOutline = false);
	[[nodiscard]] bool SetActiveFont(std::string_view fontName);

	// Legacy compatibility
	[[nodiscard]] ImFont* GetFont(std::string_view fontName = "") const;
	[[nodiscard]] float GetActiveFontSize() const;

	// Frame lifecycle
	void BeginFrame() noexcept;
	void EndFrame() noexcept;
	void Render() noexcept;

	bool HandleWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	// Flush and restart frame
	void FlushAndRestart() noexcept;

	void BeginFlushBatch() noexcept;
	void EndFlushBatch() noexcept;

	struct SFlushBatchGuard
	{
		SFlushBatchGuard();
		~SFlushBatchGuard();
	};

	// Render layer control for z-ordering
	enum class ERenderLayer : int
	{
		Background = 0,
		Foreground = 1,
	};

	// Text rendering functions
	void RenderText(std::string_view text, float x, float y, unsigned long color, bool shadow = false, ERenderLayer layer = ERenderLayer::Background);
	void RenderTextW(std::wstring_view text, float x, float y, unsigned long color, bool shadow = false, ERenderLayer layer = ERenderLayer::Background);
	void RenderTextWithOutline(std::string_view text, float x, float y, unsigned long textColor, unsigned long outlineColor, ERenderLayer layer = ERenderLayer::Background);
	void RenderTextWithOutlineW(std::wstring_view text, float x, float y, unsigned long textColor, unsigned long outlineColor, ERenderLayer layer = ERenderLayer::Background);

	// Advanced rendering with specific font
	void RenderTextEx(std::string_view fontName, std::string_view text, float x, float y, unsigned long color, bool shadow = false);
	void RenderTextWithOutlineEx(std::string_view fontName, std::string_view text, float x, float y, unsigned long textColor, unsigned long outlineColor);

	// Text measurement
	void GetTextExtent(std::string_view text, int* width, int* height, std::string_view fontName = "") const;
	void GetTextExtentW(std::wstring_view text, int* width, int* height, std::string_view fontName = "") const;

	// UTF-8 conversion helper
	[[nodiscard]] static std::string WideToUtf8(std::wstring_view text);

	// Glyph info for DirectX rendering
	struct SGlyphInfo
	{
		float x0, y0, x1, y1;
		float u0, v0, u1, v1;
		float advanceX;
	};

	[[nodiscard]] bool GetGlyphInfo(wchar_t character, SGlyphInfo& outInfo, std::string_view fontName = "") const;
	[[nodiscard]] ID3D11ShaderResourceView* GetFontTexture(std::string_view fontName = "") const;

	// Accessors
	[[nodiscard]] bool IsInitialized() const noexcept { return m_bInitialized; }

	// Font pixelation control
	void SetFontPixelate(bool enable);
	[[nodiscard]] bool IsFontPixelated() const noexcept { return m_bFontPixelate; }

	// Font size control
	void SetFontSizeOffset(float offset);
	[[nodiscard]] float GetFontSizeOffset() const noexcept { return m_fFontSizeOffset; }

	// UI scale control
	void SetUIScale(float scale);
	[[nodiscard]] float GetUIScale() const noexcept { return m_fUIScale; }

	// Singleton access
	static CImGuiManager& Instance();

private:
	bool __LoadSystemFont();
	bool __LoadFont(const SFontConfig& config);
	bool __ParseConfigFile(std::string_view configPath, std::vector<SFontConfig>& outConfigs);
	[[nodiscard]] static constexpr unsigned int __ConvertArgbToImGuiColor(unsigned long argbColor) noexcept;

#ifdef IMGUI_ENABLE_FREETYPE
	[[nodiscard]] static constexpr unsigned int __ConvertToFreeTypeFlags(EFreeTypeQuality quality) noexcept;
#endif

	HWND m_hWnd;
	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pContext;
	bool m_bInitialized;
	int  m_iFlushSuppressDepth = 0;
	bool m_bFlushPending = false;
	bool m_bFontPixelate;
	float m_fFontSizeOffset;
	float m_fUIScale;
	std::string m_strFontConfigPath;

	std::unordered_map<std::string, SFontEntry, StringHash, std::equal_to<>> m_mapFonts;
	std::string m_strActiveFontName;
};

// Global accessor macro
#define IMGUI_MANAGER CImGuiManager::Instance()
