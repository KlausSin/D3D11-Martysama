#include "StdAfx.h"
#include "ImGuiManager.h"
#include "ShaderManager.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#ifdef IMGUI_ENABLE_FREETYPE
#include "misc/freetype/imgui_freetype.h"
#endif

#include <vector>
#include <sstream>
#include <charconv>

#include "../EterPack/EterPackManager.h"
#include "../EterBase/MappedFile.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

CImGuiManager::CImGuiManager()
	: m_hWnd(nullptr)
	, m_pDevice(nullptr)
	, m_pContext(nullptr)
	, m_bInitialized(false)
	, m_bFontPixelate(true)
	, m_fFontSizeOffset(0.0f)
	, m_fUIScale(1.0f)
{
}

CImGuiManager::~CImGuiManager()
{
	Shutdown();
}

CImGuiManager& CImGuiManager::Instance()
{
	static std::unique_ptr<CImGuiManager> instance = std::make_unique<CImGuiManager>();
	return *instance;
}

bool CImGuiManager::Initialize(HWND hWnd, ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	if (m_bInitialized)
		return true;

	m_hWnd = hWnd;
	m_pDevice = pDevice;
	m_pContext = pContext;

	// Read settings from config file
	{
		FILE* fp = fopen("config/metin2.cfg", "r");
		if (fp)
		{
			char line[256];
			while (fgets(line, sizeof(line), fp))
			{
				line[strcspn(line, "\r\n")] = 0;

				if (strncmp(line, "FONT_PIXELATE", 13) == 0)
				{
					char* value = line + 13;
					while (*value == ' ' || *value == '\t') value++;
					if (*value)
						m_bFontPixelate = (atoi(value) != 0);
				}
				else if (strncmp(line, "FONT_SIZE_OFFSET", 16) == 0)
				{
					char* value = line + 16;
					while (*value == ' ' || *value == '\t') value++;
					if (*value)
					{
						m_fFontSizeOffset = (float)atof(value);
						if (m_fFontSizeOffset < -6.0f) m_fFontSizeOffset = -6.0f;
						if (m_fFontSizeOffset > 8.0f) m_fFontSizeOffset = 8.0f;
					}
				}
				else if (strncmp(line, "IMGUI_UI_SCALE", 14) == 0)
				{
					char* value = line + 14;
					while (*value == ' ' || *value == '\t') value++;
					if (*value)
					{
						m_fUIScale = (float)atof(value);
						if (m_fUIScale < 0.5f) m_fUIScale = 0.5f;
						if (m_fUIScale > 2.0f) m_fUIScale = 2.0f;
					}
				}
			}
			fclose(fp);
		}
	}

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	// Disable ini file
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;

	// Setup Platform/Renderer backends (DX11)
	if (!ImGui_ImplWin32_Init(hWnd))
		return false;

	if (!ImGui_ImplDX11_Init(pDevice, pContext))
	{
		ImGui_ImplWin32_Shutdown();
		return false;
	}

	// Setup style
	ImGui::StyleColorsDark();

	m_bInitialized = true;

	// Load Arial from Windows system fonts
	if (!__LoadSystemFont())
	{
		TraceError("ImGuiManager: Failed to load system font, using ImGui default");
		io.Fonts->AddFontDefault();
		io.Fonts->Build();
		ImGui_ImplDX11_InvalidateDeviceObjects();
		ImGui_ImplDX11_CreateDeviceObjects();
	}

	Trace("ImGuiManager: Successfully initialized");
	return true;
}

void CImGuiManager::Shutdown()
{
	if (!m_bInitialized)
		return;

	m_mapFonts.clear();
	m_strActiveFontName.clear();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	m_bInitialized = false;
}

bool CImGuiManager::__LoadSystemFont()
{
	// Get Windows fonts directory
	char szWindowsDir[MAX_PATH];
	if (!GetWindowsDirectoryA(szWindowsDir, MAX_PATH))
		return false;

	std::string arialPath = std::string(szWindowsDir) + "\\Fonts\\arial.ttf";

	// Load Arial font file directly from filesystem
	FILE* fp = fopen(arialPath.c_str(), "rb");
	if (!fp)
	{
		TraceError("ImGuiManager: Cannot open Arial font at '%s'", arialPath.c_str());
		return false;
	}

	fseek(fp, 0, SEEK_END);
	long fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (fileSize <= 0)
	{
		fclose(fp);
		return false;
	}

	// ImGui takes ownership with FontDataOwnedByAtlas = true
	void* fontData = IM_ALLOC(fileSize);
	if (!fontData)
	{
		fclose(fp);
		return false;
	}

	fread(fontData, 1, fileSize, fp);
	fclose(fp);

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Clear();

	// Font config
	ImFontConfig config;
	config.FontDataOwnedByAtlas = true;
	config.OversampleH = 2;
	config.OversampleV = 1;
	config.PixelSnapH = true;

#ifdef IMGUI_ENABLE_FREETYPE
	if (m_bFontPixelate)
		config.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_MonoHinting | ImGuiFreeTypeLoaderFlags_Monochrome;
#endif

	// Unicode ranges for international text
	static const ImWchar ranges[] = {
		0x0020, 0x00FF, // Basic Latin + Latin Supplement
		0x0100, 0x017F, // Latin Extended-A (Central European)
		0x0180, 0x024F, // Latin Extended-B
		0x0400, 0x052F, // Cyrillic + Supplement
		0x2000, 0x206F, // General Punctuation
		0,
	};

	float fontSize = 12.0f + m_fFontSizeOffset;
	if (fontSize < 8.0f) fontSize = 8.0f;

	ImFont* pFont = io.Fonts->AddFontFromMemoryTTF(fontData, fileSize, fontSize, &config, ranges);
	if (!pFont)
	{
		IM_FREE(fontData);
		return false;
	}

	// Store font entry
	SFontEntry entry{ pFont, fontSize, 1 };
	m_mapFonts["DEFAULT"] = entry;
	m_strActiveFontName = "DEFAULT";

#ifdef IMGUI_ENABLE_FREETYPE
	if (m_bFontPixelate)
		io.Fonts->FontLoaderFlags = ImGuiFreeTypeLoaderFlags_MonoHinting | ImGuiFreeTypeLoaderFlags_Monochrome;
#endif

	if (!io.Fonts->Build())
	{
		TraceError("ImGuiManager: Failed to build font atlas");
		return false;
	}

	ImGui_ImplDX11_InvalidateDeviceObjects();
	ImGui_ImplDX11_CreateDeviceObjects();

	Trace("ImGuiManager: Loaded Arial font from system");
	return true;
}

bool CImGuiManager::__ParseConfigFile(std::string_view configPath, std::vector<SFontConfig>& outConfigs)
{
	// Load config file from pack system
	CMappedFile file;
	LPCVOID pData = nullptr;

	if (!CEterPackManager::Instance().Get(file, std::string(configPath).c_str(), &pData))
		return false;

	DWORD fileSize = file.Size();
	if (fileSize == 0 || !pData)
		return false;

	// Copy to string for parsing
	std::string content(reinterpret_cast<const char*>(pData), fileSize);

	outConfigs.clear();
	SFontConfig currentConfig;
	bool hasName = false;

	std::istringstream iss(content);
	std::string line;

	while (std::getline(iss, line))
	{
		// Remove comments
		size_t commentPos = line.find("//");
		if (commentPos == std::string::npos)
			commentPos = line.find('#');
		if (commentPos != std::string::npos)
			line = line.substr(0, commentPos);

		// Trim whitespace
		size_t start = line.find_first_not_of(" \t\r\n");
		size_t end = line.find_last_not_of(" \t\r\n");
		if (start == std::string::npos || end == std::string::npos)
			continue;

		line = line.substr(start, end - start + 1);

		// Parse key-value pairs
		size_t delimPos = line.find_first_of(" \t");
		if (delimPos == std::string::npos)
			continue;

		std::string key = line.substr(0, delimPos);
		std::string value = line.substr(delimPos + 1);

		start = value.find_first_not_of(" \t");
		if (start != std::string::npos)
			value = value.substr(start);

		// Process directives
		if (key == "FONT")
		{
			if (hasName && !currentConfig.name.empty())
				outConfigs.push_back(currentConfig);

			currentConfig = SFontConfig();
			currentConfig.name = value;
			hasName = true;
		}
		else if (key == "PATH")
		{
			currentConfig.path = value;
		}
		else if (key == "SIZE")
		{
			float tempFloat;
			auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), tempFloat);
			if (ec == std::errc())
				currentConfig.size = tempFloat;
		}
		else if (key == "OVERSAMPLE_H")
		{
			int tempInt;
			auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), tempInt);
			if (ec == std::errc())
				currentConfig.oversampleH = tempInt;
		}
		else if (key == "OVERSAMPLE_V")
		{
			int tempInt;
			auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), tempInt);
			if (ec == std::errc())
				currentConfig.oversampleV = tempInt;
		}
		else if (key == "RASTERIZER_MULTIPLY")
		{
			float tempFloat;
			auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), tempFloat);
			if (ec == std::errc())
				currentConfig.rasterizerMultiply = tempFloat;
		}
		else if (key == "ENABLE_OUTLINE")
		{
			int tempInt;
			auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), tempInt);
			if (ec == std::errc())
				currentConfig.enableOutline = (tempInt != 0);
		}
		else if (key == "OUTLINE_THICKNESS")
		{
			int tempInt;
			auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), tempInt);
			if (ec == std::errc())
				currentConfig.outlineThickness = tempInt;
		}
		else if (key == "PIXEL_SNAP_H")
		{
			int tempInt;
			auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), tempInt);
			if (ec == std::errc())
				currentConfig.pixelSnapH = (tempInt != 0);
		}
	}

	// Save last font config
	if (hasName && !currentConfig.name.empty())
		outConfigs.push_back(currentConfig);

	return !outConfigs.empty();
}

bool CImGuiManager::__LoadFont(const SFontConfig& config)
{
	if (!m_bInitialized)
		return false;

	ImGuiIO& io = ImGui::GetIO();

	// Load font file from pack system
	CMappedFile file;
	LPCVOID pData = nullptr;

	if (!CEterPackManager::Instance().Get(file, config.path.c_str(), &pData))
	{
		TraceError("ImGuiManager: Failed to load font '%s' from path '%s'", config.name.c_str(), config.path.c_str());
		return false;
	}

	DWORD fileSize = file.Size();
	if (fileSize == 0 || !pData)
		return false;

	void* fontDataCopy = IM_ALLOC(fileSize);
	if (!fontDataCopy)
		return false;
	memcpy(fontDataCopy, pData, fileSize);

	// Configure font
	ImFontConfig imConfig;
	imConfig.FontDataOwnedByAtlas = true;
	imConfig.OversampleH = config.oversampleH;
	imConfig.OversampleV = config.oversampleV;
	imConfig.PixelSnapH = config.pixelSnapH;
	imConfig.RasterizerMultiply = config.rasterizerMultiply;

#ifdef IMGUI_ENABLE_FREETYPE
	if (m_bFontPixelate)
		imConfig.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_MonoHinting | ImGuiFreeTypeLoaderFlags_Monochrome;
	else
		imConfig.FontLoaderFlags = 0;
#endif

	// Unicode ranges
	static const ImWchar ranges[] = {
		0x0020, 0x00FF, // Basic Latin + Latin Supplement
		0x0100, 0x017F, // Latin Extended-A
		0x0180, 0x024F, // Latin Extended-B
		0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
		0x2000, 0x206F, // General Punctuation
		0x3000, 0x30FF, // CJK Symbols, Hiragana, Katakana
		0x4E00, 0x9FAF, // CJK Ideograms
		0xAC00, 0xD7A3, // Hangul Syllables
		0,
	};

	float finalSize = config.size + m_fFontSizeOffset;
	if (finalSize < 6.0f) finalSize = 6.0f;

	ImFont* pFont = io.Fonts->AddFontFromMemoryTTF(
		fontDataCopy,
		static_cast<int>(fileSize),
		finalSize,
		&imConfig,
		ranges
	);

	if (!pFont)
	{
		IM_FREE(fontDataCopy);
		return false;
	}

	SFontEntry entry{
		.pFont = pFont,
		.fontSize = finalSize,
		.outlineThickness = config.outlineThickness
	};

	m_mapFonts[config.name] = entry;

	if (m_strActiveFontName.empty())
		m_strActiveFontName = config.name;

	return true;
}

bool CImGuiManager::LoadFontsFromConfig(std::string_view configPath)
{
	if (!m_bInitialized)
		return false;

	std::vector<SFontConfig> configs;
	if (!__ParseConfigFile(configPath, configs))
	{
		TraceError("ImGuiManager: Failed to parse font config '%s'", std::string(configPath).c_str());
		return false;
	}

	Trace("ImGuiManager: Parsed font configs successfully");
	m_strFontConfigPath = configPath;

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Clear();
	m_mapFonts.clear();

	bool anySuccess = false;
	for (const SFontConfig& config : configs)
	{
		if (__LoadFont(config))
			anySuccess = true;
	}

	if (!anySuccess)
		return false;

#ifdef IMGUI_ENABLE_FREETYPE
	if (m_bFontPixelate)
		io.Fonts->FontLoaderFlags = ImGuiFreeTypeLoaderFlags_MonoHinting | ImGuiFreeTypeLoaderFlags_Monochrome;
	else
		io.Fonts->FontLoaderFlags = 0;
#endif

	if (!io.Fonts->Build())
	{
		TraceError("ImGuiManager: Failed to build font atlas");
		return false;
	}

	ImGui_ImplDX11_InvalidateDeviceObjects();
	ImGui_ImplDX11_CreateDeviceObjects();

	Trace("ImGuiManager: Loaded fonts from config");
	return true;
}

bool CImGuiManager::LoadFont(std::string_view fontName, std::string_view fontPath, float fontSize, bool enableOutline)
{
	if (!m_bInitialized)
		return false;

	SFontConfig config;
	config.name = std::string(fontName);
	config.path = std::string(fontPath);
	config.size = fontSize;
	config.enableOutline = enableOutline;

	ImGuiIO& io = ImGui::GetIO();

	if (m_mapFonts.empty())
		io.Fonts->Clear();

	bool result = __LoadFont(config);

	if (result)
	{
#ifdef IMGUI_ENABLE_FREETYPE
		if (m_bFontPixelate)
			io.Fonts->FontLoaderFlags = ImGuiFreeTypeLoaderFlags_MonoHinting | ImGuiFreeTypeLoaderFlags_Monochrome;
		else
			io.Fonts->FontLoaderFlags = 0;
#endif
		io.Fonts->Build();
		ImGui_ImplDX11_InvalidateDeviceObjects();
		ImGui_ImplDX11_CreateDeviceObjects();
	}

	return result;
}

bool CImGuiManager::LoadFontFromMemory(std::string_view fontName, std::span<const std::byte> fontData, float fontSize, bool enableOutline)
{
	if (!m_bInitialized)
		return false;

	ImGuiIO& io = ImGui::GetIO();

	ImFontConfig config;
	config.FontDataOwnedByAtlas = true;
	config.OversampleH = 4;
	config.OversampleV = 4;
	config.PixelSnapH = false;
	config.RasterizerMultiply = 1.2f;

	ImFont* pFont = io.Fonts->AddFontFromMemoryTTF(
		const_cast<void*>(static_cast<const void*>(fontData.data())),
		static_cast<int>(fontData.size()),
		fontSize,
		&config);

	if (!pFont)
		return false;

	SFontEntry entry;
	entry.pFont = pFont;
	entry.fontSize = fontSize;
	entry.outlineThickness = enableOutline ? 1 : 0;

	m_mapFonts[std::string(fontName)] = entry;

	if (m_strActiveFontName.empty())
		m_strActiveFontName = std::string(fontName);

#ifdef IMGUI_ENABLE_FREETYPE
	if (m_bFontPixelate)
		io.Fonts->FontLoaderFlags = ImGuiFreeTypeLoaderFlags_MonoHinting | ImGuiFreeTypeLoaderFlags_Monochrome;
	else
		io.Fonts->FontLoaderFlags = 0;
#endif
	io.Fonts->Build();
	ImGui_ImplDX11_InvalidateDeviceObjects();
	ImGui_ImplDX11_CreateDeviceObjects();

	return true;
}

bool CImGuiManager::SetActiveFont(std::string_view fontName)
{
	auto it = m_mapFonts.find(fontName);
	if (it == m_mapFonts.end())
		return false;

	m_strActiveFontName = fontName;
	return true;
}

ImFont* CImGuiManager::GetFont(std::string_view fontName) const
{
	if (fontName.empty())
		fontName = m_strActiveFontName;

	auto it = m_mapFonts.find(fontName);
	if (it != m_mapFonts.end())
		return it->second.pFont;

	return nullptr;
}

float CImGuiManager::GetActiveFontSize() const
{
	if (!m_bInitialized || m_strActiveFontName.empty())
		return 14.0f;

	auto it = m_mapFonts.find(m_strActiveFontName);
	if (it == m_mapFonts.end())
		return 14.0f;

	return it->second.fontSize;
}

void CImGuiManager::BeginFrame() noexcept
{
	if (!m_bInitialized)
		return;

	ImGuiStyle& style = ImGui::GetStyle();
	static ImGuiStyle baseStyle;
	static bool savedBase = false;

	if (!savedBase)
	{
		baseStyle = style;
		savedBase = true;
	}

	style = baseStyle;
	style.ScaleAllSizes(m_fUIScale);

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void CImGuiManager::EndFrame() noexcept
{
	if (!m_bInitialized)
		return;

	ImGui::EndFrame();
}

void CImGuiManager::Render() noexcept
{
	if (!m_bInitialized)
		return;

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	SHADERMANAGER.InvalidateIACache();
}

bool CImGuiManager::HandleWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (!m_bInitialized)
		return false;

	// Let ImGui process the message
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	return false;
}

void CImGuiManager::FlushAndRestart() noexcept
{
	if (!m_bInitialized)
		return;


	if (m_iFlushSuppressDepth > 0)
	{
		m_bFlushPending = true;
		return;
	}

	{
		static DWORD s_lastLog = 0;
		const DWORD now = GetTickCount();
		if (now - s_lastLog > 1000)
		{
			s_lastLog = now;
		}
	}

	const float fRealDelta = ImGui::GetIO().DeltaTime;
	EndFrame();
	Render();
	BeginFrame();
	ImGui::GetIO().DeltaTime = fRealDelta;
}

void CImGuiManager::BeginFlushBatch() noexcept
{
	++m_iFlushSuppressDepth;
}

void CImGuiManager::EndFlushBatch() noexcept
{
	if (m_iFlushSuppressDepth > 0)
		--m_iFlushSuppressDepth;

	if (m_iFlushSuppressDepth == 0 && m_bFlushPending)
	{
		m_bFlushPending = false;
		FlushAndRestart();     // the ONE submission that replaces the per-instance storm
	}
}

CImGuiManager::SFlushBatchGuard::SFlushBatchGuard()
{
	CImGuiManager::Instance().BeginFlushBatch();
}

CImGuiManager::SFlushBatchGuard::~SFlushBatchGuard()
{
	CImGuiManager::Instance().EndFlushBatch();
}

void CImGuiManager::RenderText(std::string_view text, float x, float y, unsigned long color, bool shadow, ERenderLayer layer)
{
	if (!m_bInitialized || m_mapFonts.empty() || text.empty())
		return;

	auto it = m_mapFonts.find(m_strActiveFontName);
	if (it == m_mapFonts.end())
		return;

	ImFont* pFont = it->second.pFont;
	float fontSize = it->second.fontSize;

	ImU32 imColor = __ConvertArgbToImGuiColor(color);

	ImDrawList* drawList = (layer == ERenderLayer::Foreground)
		? ImGui::GetForegroundDrawList()
		: ImGui::GetBackgroundDrawList();

	if (shadow)
		drawList->AddText(pFont, fontSize, ImVec2(x + 1.0f, y + 1.0f), IM_COL32(0, 0, 0, 128), text.data(), text.data() + text.size());

	drawList->AddText(pFont, fontSize, ImVec2(x, y), imColor, text.data(), text.data() + text.size());
}

std::string CImGuiManager::WideToUtf8(std::wstring_view text)
{
	if (text.empty())
		return {};

	const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
	if (size <= 0)
		return {};

	std::string result(size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);

	return result;
}

void CImGuiManager::RenderTextW(std::wstring_view text, float x, float y, unsigned long color, bool shadow, ERenderLayer layer)
{
	if (text.empty())
		return;

	const std::string utf8Text = WideToUtf8(text);
	if (!utf8Text.empty())
		RenderText(utf8Text, x, y, color, shadow, layer);
}

void CImGuiManager::RenderTextWithOutline(std::string_view text, float x, float y, unsigned long textColor, unsigned long outlineColor, ERenderLayer layer)
{
	if (!m_bInitialized || m_mapFonts.empty() || text.empty())
		return;

	auto it = m_mapFonts.find(m_strActiveFontName);
	if (it == m_mapFonts.end())
		return;

	ImFont* pFont = it->second.pFont;
	float fontSize = it->second.fontSize;
	int thickness = it->second.outlineThickness;

	ImU32 imOutlineColor = __ConvertArgbToImGuiColor(outlineColor);
	ImU32 imTextColor = __ConvertArgbToImGuiColor(textColor);

	ImDrawList* drawList = (layer == ERenderLayer::Foreground)
		? ImGui::GetForegroundDrawList()
		: ImGui::GetBackgroundDrawList();

	const char* textStart = text.data();
	const char* textEnd = text.data() + text.size();

	// 8-directional outline
	if (thickness > 0)
	{
		static constexpr float offsetsX[] = { 0.0f,  1.0f,  1.0f,  1.0f,  0.0f, -1.0f, -1.0f, -1.0f };
		static constexpr float offsetsY[] = { -1.0f, -1.0f,  0.0f,  1.0f,  1.0f,  1.0f,  0.0f, -1.0f };

		const float offset = static_cast<float>(thickness);

		for (size_t i = 0; i < 8; ++i)
		{
			const float ox = offsetsX[i] * offset;
			const float oy = offsetsY[i] * offset;
			drawList->AddText(pFont, fontSize, ImVec2(x + ox, y + oy), imOutlineColor, textStart, textEnd);
		}
	}

	drawList->AddText(pFont, fontSize, ImVec2(x, y), imTextColor, textStart, textEnd);
}

void CImGuiManager::RenderTextWithOutlineW(std::wstring_view text, float x, float y, unsigned long textColor, unsigned long outlineColor, ERenderLayer layer)
{
	if (text.empty())
		return;

	const std::string utf8Text = WideToUtf8(text);
	if (!utf8Text.empty())
		RenderTextWithOutline(utf8Text, x, y, textColor, outlineColor, layer);
}

void CImGuiManager::RenderTextEx(std::string_view fontName, std::string_view text, float x, float y, unsigned long color, bool shadow)
{
	if (!m_bInitialized || text.empty() || fontName.empty())
		return;

	auto it = m_mapFonts.find(fontName);
	if (it == m_mapFonts.end())
		return;

	ImFont* pFont = it->second.pFont;
	float fontSize = it->second.fontSize;

	ImU32 imColor = __ConvertArgbToImGuiColor(color);
	ImDrawList* drawList = ImGui::GetForegroundDrawList();

	const char* textStart = text.data();
	const char* textEnd = text.data() + text.size();

	if (shadow)
		drawList->AddText(pFont, fontSize, ImVec2(x + 1.0f, y + 1.0f), IM_COL32(0, 0, 0, 128), textStart, textEnd);

	drawList->AddText(pFont, fontSize, ImVec2(x, y), imColor, textStart, textEnd);
}

void CImGuiManager::RenderTextWithOutlineEx(std::string_view fontName, std::string_view text, float x, float y, unsigned long textColor, unsigned long outlineColor)
{
	if (!m_bInitialized || text.empty() || fontName.empty())
		return;

	auto it = m_mapFonts.find(fontName);
	if (it == m_mapFonts.end())
		return;

	ImFont* pFont = it->second.pFont;
	float fontSize = it->second.fontSize;
	int thickness = it->second.outlineThickness;

	ImU32 imOutlineColor = __ConvertArgbToImGuiColor(outlineColor);
	ImU32 imTextColor = __ConvertArgbToImGuiColor(textColor);
	ImDrawList* drawList = ImGui::GetForegroundDrawList();

	const char* textStart = text.data();
	const char* textEnd = text.data() + text.size();

	if (thickness > 0)
	{
		static constexpr float offsetsX[] = { 0.0f,  1.0f,  1.0f,  1.0f,  0.0f, -1.0f, -1.0f, -1.0f };
		static constexpr float offsetsY[] = { -1.0f, -1.0f,  0.0f,  1.0f,  1.0f,  1.0f,  0.0f, -1.0f };

		const float offset = static_cast<float>(thickness);

		for (size_t i = 0; i < 8; ++i)
		{
			const float ox = offsetsX[i] * offset;
			const float oy = offsetsY[i] * offset;
			drawList->AddText(pFont, fontSize, ImVec2(x + ox, y + oy), imOutlineColor, textStart, textEnd);
		}
	}

	drawList->AddText(pFont, fontSize, ImVec2(x, y), imTextColor, textStart, textEnd);
}

void CImGuiManager::GetTextExtent(std::string_view text, int* width, int* height, std::string_view fontName) const
{
	if (!m_bInitialized || text.empty())
	{
		if (width) *width = 0;
		if (height) *height = 0;
		return;
	}

	if (fontName.empty())
		fontName = m_strActiveFontName;

	auto it = m_mapFonts.find(fontName);
	if (it == m_mapFonts.end())
	{
		if (width) *width = 0;
		if (height) *height = 0;
		return;
	}

	ImFont* pFont = it->second.pFont;
	float fontSize = it->second.fontSize;

	ImVec2 size = pFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.data(), text.data() + text.size());

	if (width) *width = static_cast<int>(size.x);
	if (height) *height = static_cast<int>(size.y);
}

void CImGuiManager::GetTextExtentW(std::wstring_view text, int* width, int* height, std::string_view fontName) const
{
	if (text.empty())
	{
		if (width) *width = 0;
		if (height) *height = 0;
		return;
	}

	const std::string utf8Text = WideToUtf8(text);
	GetTextExtent(utf8Text, width, height, fontName);
}

bool CImGuiManager::GetGlyphInfo(wchar_t character, SGlyphInfo& outInfo, std::string_view fontName) const
{
	if (!m_bInitialized || m_mapFonts.empty())
		return false;

	if (fontName.empty())
		fontName = m_strActiveFontName;

	auto it = m_mapFonts.find(fontName);
	if (it == m_mapFonts.end())
		return false;

	ImFont* pFont = it->second.pFont;
	float fontSize = it->second.fontSize;
	if (!pFont)
		return false;

	// In ImGui 1.92+, FindGlyph is on ImFontBaked, not ImFont
	ImFontBaked* pBaked = pFont->GetFontBaked(fontSize);
	if (!pBaked)
		return false;

	const ImFontGlyph* pGlyph = pBaked->FindGlyph(static_cast<ImWchar>(character));
	if (!pGlyph || !pGlyph->Visible)
		return false;

	outInfo.x0 = pGlyph->X0;
	outInfo.y0 = pGlyph->Y0;
	outInfo.x1 = pGlyph->X1;
	outInfo.y1 = pGlyph->Y1;
	outInfo.u0 = pGlyph->U0;
	outInfo.v0 = pGlyph->V0;
	outInfo.u1 = pGlyph->U1;
	outInfo.v1 = pGlyph->V1;
	outInfo.advanceX = pGlyph->AdvanceX;

	return true;
}

ID3D11ShaderResourceView* CImGuiManager::GetFontTexture(std::string_view fontName) const
{
	if (!m_bInitialized || m_mapFonts.empty())
		return nullptr;

	if (fontName.empty())
		fontName = m_strActiveFontName;

	auto it = m_mapFonts.find(fontName);
	if (it == m_mapFonts.end())
		return nullptr;

	ImFont* pFont = it->second.pFont;
	if (!pFont || !pFont->OwnerAtlas)
		return nullptr;

	// In ImGui 1.92+, use TexRef.GetTexID() instead of TexID
	ImTextureID texId = pFont->OwnerAtlas->TexRef.GetTexID();
	return reinterpret_cast<ID3D11ShaderResourceView*>(texId);
}

void CImGuiManager::SetFontPixelate(bool enable)
{
	if (!m_bInitialized)
		return;

	if (m_bFontPixelate == enable)
		return;

	m_bFontPixelate = enable;
}

constexpr unsigned int CImGuiManager::__ConvertArgbToImGuiColor(unsigned long argbColor) noexcept
{
	// Colour format: ARGB (0xAARRGGBB)
	// ImGui format: ABGR (0xAABBGGRR)
	const unsigned char a = static_cast<unsigned char>((argbColor >> 24) & 0xFF);
	const unsigned char r = static_cast<unsigned char>((argbColor >> 16) & 0xFF);
	const unsigned char g = static_cast<unsigned char>((argbColor >> 8) & 0xFF);
	const unsigned char b = static_cast<unsigned char>((argbColor >> 0) & 0xFF);

	return (static_cast<unsigned int>(a) << 24) |
		(static_cast<unsigned int>(b) << 16) |
		(static_cast<unsigned int>(g) << 8) |
		(static_cast<unsigned int>(r) << 0);
}

#ifdef IMGUI_ENABLE_FREETYPE
constexpr unsigned int CImGuiManager::__ConvertToFreeTypeFlags(EFreeTypeQuality quality) noexcept
{
	return static_cast<unsigned int>(quality);
}
#endif

void CImGuiManager::SetFontSizeOffset(float offset)
{
	if (!m_bInitialized)
		return;

	if (offset < -6.0f) offset = -6.0f;
	if (offset > 8.0f) offset = 8.0f;

	if (m_fFontSizeOffset == offset)
		return;

	m_fFontSizeOffset = offset;

	if (!m_strFontConfigPath.empty())
		(void)LoadFontsFromConfig(m_strFontConfigPath);
}

void CImGuiManager::SetUIScale(float scale)
{
	if (scale < 0.5f) scale = 0.5f;
	if (scale > 2.0f) scale = 2.0f;

	if (m_fUIScale == scale)
		return;

	m_fUIScale = scale;
}
