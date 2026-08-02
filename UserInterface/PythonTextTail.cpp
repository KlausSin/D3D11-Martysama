//
//
#include "stdafx.h"
#include "InstanceBase.h"
#include "resource.h"
#include "PythonTextTail.h"
#include "PythonCharacterManager.h"
#include "PythonGuild.h"
#include "Locale.h"
#include "MarkManager.h"

#include "../EterBase/StepTimer.h"

#ifdef ENABLE_IMGUI_MANAGER
#include "../EterLib/ImGuiNameRenderer.h"

// Helper macro to convert float RGBA to DWORD ARGB
#define MAKE_COLOR_ARGB(r, g, b, a) \
	((DWORD)((((DWORD)((a) * 255.0f) & 0xFF) << 24) | \
	         (((DWORD)((r) * 255.0f) & 0xFF) << 16) | \
	         (((DWORD)((g) * 255.0f) & 0xFF) << 8) | \
	         (((DWORD)((b) * 255.0f) & 0xFF))))
#endif

static const int MAX_SHOWING_CHARACTER_TEXTTAILS = 50;

static std::vector<std::pair<float, DWORD>> s_kCharCandidatesCache;
static bool s_bCharCandidatesCacheReady = false;

static const float TEXTTAIL_SHOW_DIST_SQ = 3500.0f * 3500.0f;
static const float TEXTTAIL_ZDEPTH_DIST_SQ = 1300.0f * 1300.0f;

const Color c_TextTail_Player_Color = Color(1.0f, 1.0f, 1.0f, 1.0f);
const Color c_TextTail_Monster_Color = Color(1.0f, 0.0f, 0.0f, 1.0f);
const Color c_TextTail_Item_Color = Color(1.0f, 1.0f, 1.0f, 1.0f);
const Color c_TextTail_Chat_Color = Color(1.0f, 1.0f, 1.0f, 1.0f);
const Color c_TextTail_Info_Color = Color(1.0f, 0.785f, 0.785f, 1.0f);
const Color c_TextTail_Guild_Name_Color = 0xFFEFD3FF;
const float c_TextTail_Name_Position = -10.0f;
const float c_fxMarkPosition = 1.5f;
const float c_fyGuildNamePosition = 15.0f;
const float c_fyMarkPosition = 15.0f + 11.0f;
BOOL bPKTitleEnable = TRUE;

// TEXTTAIL_LIVINGTIME_CONTROL
long gs_TextTail_LivingTime = 5000;

long TextTail_GetLivingTime()
{
	assert(gs_TextTail_LivingTime>1000);
	return gs_TextTail_LivingTime;
}

void TextTail_SetLivingTime(long livingTime)
{
	gs_TextTail_LivingTime = livingTime;
}
// END_OF_TEXTTAIL_LIVINGTIME_CONTROL

CGraphicText * ms_pFont = NULL;

void CPythonTextTail::GetInfo(std::string* pstInfo)
{
	char szInfo[256];
	sprintf(szInfo, "TextTail: ChatTail %d, ChrTail (Map %d, List %d), ItemTail (Map %d, List %d), Pool %d",
		(int)m_ChatTailMap.size(),
		(int)m_CharacterTextTailMap.size(), (int)m_CharacterTextTailList.size(),
		(int)m_ItemTextTailMap.size(), (int)m_ItemTextTailList.size(),
		(int)m_TextTailPool.GetCapacity());

	pstInfo->append(szInfo);
}

void CPythonTextTail::UpdateAllTextTail()
{
	s_bCharCandidatesCacheReady = false;
	s_kCharCandidatesCache.clear();

	CInstanceBase * pInstance = CPythonCharacterManager::Instance().GetMainInstancePtr();
	if (pInstance)
	{
		TPixelPosition pixelPos;
		pInstance->NEW_GetPixelPosition(&pixelPos);

		for (TTextTailMap::iterator itorMap = m_CharacterTextTailMap.begin(); itorMap != m_CharacterTextTailMap.end(); ++itorMap)
		{
			UpdateDistance(pixelPos, itorMap->second);
			if (itorMap->second->fDistanceFromPlayer < TEXTTAIL_SHOW_DIST_SQ)
				s_kCharCandidatesCache.push_back(std::make_pair(itorMap->second->fDistanceFromPlayer, itorMap->first));
		}

		for (TTextTailMap::iterator itorMap = m_ItemTextTailMap.begin(); itorMap != m_ItemTextTailMap.end(); ++itorMap)
		{
			UpdateDistance(pixelPos, itorMap->second);
		}

		for (TChatTailMap::iterator itorChat=m_ChatTailMap.begin(); itorChat!=m_ChatTailMap.end(); ++itorChat)
		{
			UpdateDistance(pixelPos, itorChat->second);

			if (itorChat->second->bNameFlag)
			{
				DWORD dwVID = itorChat->first;
				ShowCharacterTextTail(dwVID);
			}
		}
	}

	s_bCharCandidatesCacheReady = true;
}

static void __GetTailTextSize(CGraphicTextInstance * pInstance, int * piWidth, int * piHeight)
{
	if (!pInstance)
	{
		*piWidth = 0;
		*piHeight = 0;
		return;
	}

#ifdef ENABLE_IMGUI_MANAGER
	CImGuiNameRenderer::Instance().GetTextSize(pInstance->GetValueStringReference().c_str(), piWidth, piHeight);
	if (*piWidth > 0)
		return;   // 0 means ImGui has no font loaded yet - fall through to the engine's own measure
#endif

	pInstance->GetTextSize(piWidth, piHeight);
}

void CPythonTextTail::UpdateShowingTextTail()
{
	TTextTailList::iterator itor;

	for (itor = m_ItemTextTailList.begin(); itor != m_ItemTextTailList.end(); ++itor)
	{
		UpdateTextTail(*itor);
	}

	for (TChatTailMap::iterator itorChat=m_ChatTailMap.begin(); itorChat!=m_ChatTailMap.end(); ++itorChat)
	{
		// Skip ProjectPosition for chat tails beyond show distance
		if (itorChat->second->fDistanceFromPlayer < TEXTTAIL_SHOW_DIST_SQ)
			UpdateTextTail(itorChat->second);
	}

	for (itor = m_CharacterTextTailList.begin(); itor != m_CharacterTextTailList.end(); ++itor)
	{
		TTextTail * pTextTail = *itor;
		UpdateTextTail(pTextTail);

		TChatTailMap::iterator itor = m_ChatTailMap.find(pTextTail->dwVirtualID);
		if (m_ChatTailMap.end() != itor)
		{
			TTextTail * pChatTail = itor->second;
			if (pChatTail->bNameFlag)
			{
				pTextTail->y = pChatTail->y - 17.0f;
			}
		}
	}
}

void CPythonTextTail::UpdateTextTail(TTextTail * pTextTail)
{
	if (!pTextTail->pOwner)
		return;

	/////

	CPythonGraphic & rpyGraphic = CPythonGraphic::Instance();
	rpyGraphic.Identity();

	const Vector3 & c_rv3Position = pTextTail->pOwner->GetPosition();
	rpyGraphic.ProjectPosition(c_rv3Position.x,
							   c_rv3Position.y,
							   c_rv3Position.z + pTextTail->fHeight,
							   &pTextTail->x,
							   &pTextTail->y,
							   &pTextTail->z);

	pTextTail->x = floorf(pTextTail->x);
	pTextTail->y = floorf(pTextTail->y);

	if (pTextTail->fDistanceFromPlayer < TEXTTAIL_ZDEPTH_DIST_SQ)
	{
		pTextTail->z = 0.0f;
	}
	else
	{
		pTextTail->z = pTextTail->z * CPythonGraphic::Instance().GetOrthoDepth() * -1.0f;
		pTextTail->z += 10.0f;
	}
}

void CPythonTextTail::ArrangeTextTail()
{
	TTextTailList::iterator itor;
	TTextTailList::iterator itorCompare;

	DWORD dwTime = DX::StepTimer::instance().GetTotalMillieSeconds();

	for (itor = m_ItemTextTailList.begin(); itor != m_ItemTextTailList.end(); ++itor)
	{
		TTextTail * pInsertTextTail = *itor;

		int yTemp = 5;
		int LimitCount = 0;

		for (itorCompare = m_ItemTextTailList.begin(); itorCompare != m_ItemTextTailList.end();)
		{
			TTextTail * pCompareTextTail = *itorCompare;

			if (*itorCompare == *itor)
			{
				++itorCompare;
				continue;
			}

			if (LimitCount >= 20)
				break;

			if (isIn(pInsertTextTail, pCompareTextTail))
			{
				pInsertTextTail->y = (pCompareTextTail->y + pCompareTextTail->yEnd + yTemp);

				itorCompare = m_ItemTextTailList.begin();
				++LimitCount;
				continue;
			}

			++itorCompare;
		}

		if (pInsertTextTail->pOwnerTextInstance)
		{
			pInsertTextTail->pOwnerTextInstance->SetPosition(pInsertTextTail->x, pInsertTextTail->y, pInsertTextTail->z);
			pInsertTextTail->pOwnerTextInstance->Update();

			pInsertTextTail->pTextInstance->SetColor(pInsertTextTail->Color.r, pInsertTextTail->Color.g, pInsertTextTail->Color.b);
			pInsertTextTail->pTextInstance->SetPosition(pInsertTextTail->x, pInsertTextTail->y + 15.0f, pInsertTextTail->z);
			pInsertTextTail->pTextInstance->Update();

		}
		else
		{
			pInsertTextTail->pTextInstance->SetColor(pInsertTextTail->Color.r, pInsertTextTail->Color.g, pInsertTextTail->Color.b);
			pInsertTextTail->pTextInstance->SetPosition(pInsertTextTail->x, pInsertTextTail->y, pInsertTextTail->z);
			pInsertTextTail->pTextInstance->Update();

		}
	}

	for (itor = m_CharacterTextTailList.begin(); itor != m_CharacterTextTailList.end(); ++itor)
	{
		TTextTail * pTextTail = *itor;

		float fxAdd = 0.0f;

		CGraphicMarkInstance * pMarkInstance = pTextTail->pMarkInstance;
		CGraphicTextInstance * pGuildNameInstance = pTextTail->pGuildNameTextInstance;
		if (pMarkInstance && pGuildNameInstance)
		{
			int iWidth, iHeight;
			int iImageHalfSize = pMarkInstance->GetWidth()/2 + c_fxMarkPosition;
			pGuildNameInstance->GetTextSize(&iWidth, &iHeight);

			pMarkInstance->SetPosition(pTextTail->x - iWidth/2 - iImageHalfSize, pTextTail->y - c_fyMarkPosition);
			pGuildNameInstance->SetPosition(pTextTail->x + iImageHalfSize, pTextTail->y - c_fyGuildNamePosition, pTextTail->z);
			pGuildNameInstance->Update();
		}

		int iNameWidth, iNameHeight;
		__GetTailTextSize(pTextTail->pTextInstance, &iNameWidth, &iNameHeight);

		CGraphicTextInstance * pTitle = pTextTail->pTitleTextInstance;
		if (pTitle)
		{
			int iTitleWidth, iTitleHeight;
			__GetTailTextSize(pTitle, &iTitleWidth, &iTitleHeight);

			fxAdd = 8.0f;

			if (GetDefaultCodePage() == CP_ARABIC)
				pTitle->SetPosition(pTextTail->x - (iNameWidth / 2) - iTitleWidth - 4.0f, pTextTail->y, pTextTail->z);
			else
				pTitle->SetPosition(pTextTail->x - (iNameWidth / 2) + 4.0f, pTextTail->y, pTextTail->z); // @fixme036

			pTitle->Update();

			CGraphicTextInstance * pLevel = pTextTail->pLevelTextInstance;
			if (pLevel)
			{
				int iLevelWidth, iLevelHeight;
				__GetTailTextSize(pLevel, &iLevelWidth, &iLevelHeight);
				const float fGap = (iLevelHeight > 0) ? (float)(iLevelHeight / 2) : 8.0f;

				if( GetDefaultCodePage() == CP_ARABIC )
					pLevel->SetPosition(pTextTail->x - (iNameWidth / 2) - iLevelWidth - iTitleWidth - fGap, pTextTail->y, pTextTail->z);
				else
					pLevel->SetPosition(pTextTail->x - (iNameWidth / 2) - iTitleWidth - fGap, pTextTail->y, pTextTail->z);

				pLevel->Update();
			}
		}
		else
		{
			fxAdd = 4.0f;

			CGraphicTextInstance * pLevel = pTextTail->pLevelTextInstance;
			if (pLevel)
			{
				int iLevelWidth, iLevelHeight;
				__GetTailTextSize(pLevel, &iLevelWidth, &iLevelHeight);
				const float fGap = (iLevelHeight > 0) ? (float)iLevelHeight : 12.0f;

				if (GetDefaultCodePage() == CP_ARABIC)
					pLevel->SetPosition(pTextTail->x - (iNameWidth / 2) - iLevelWidth - fGap, pTextTail->y, pTextTail->z);
				else
					pLevel->SetPosition(pTextTail->x - (iNameWidth / 2) - fGap, pTextTail->y, pTextTail->z);

				pLevel->Update();
			}
		}

		pTextTail->pTextInstance->SetColor(pTextTail->Color.r, pTextTail->Color.g, pTextTail->Color.b);
		pTextTail->pTextInstance->SetPosition(pTextTail->x + fxAdd, pTextTail->y, pTextTail->z);
		pTextTail->pTextInstance->Update();
	}

	for (TChatTailMap::iterator itorChat=m_ChatTailMap.begin(); itorChat!=m_ChatTailMap.end();)
	{
		TTextTail * pTextTail = itorChat->second;

		if (pTextTail->LivingTime < dwTime)
		{
			DeleteTextTail(pTextTail);
			itorChat = m_ChatTailMap.erase(itorChat);
			continue;
		}
		else
			++itorChat;

		// Skip text instance update for distant chat tails
		if (pTextTail->fDistanceFromPlayer >= TEXTTAIL_SHOW_DIST_SQ)
			continue;

		pTextTail->pTextInstance->SetColor(pTextTail->Color);
		pTextTail->pTextInstance->SetPosition(pTextTail->x, pTextTail->y, pTextTail->z);
		pTextTail->pTextInstance->Update();
	}
}

void CPythonTextTail::Render()
{
#ifdef ENABLE_IMGUI_MANAGER
	auto ConvertHAlign = [](BYTE hAlign) -> int { return (hAlign & 0x0F) - 1; };
	auto ConvertVAlign = [](BYTE vAlign) -> int { return ((vAlign >> 4) & 0x0F) - 1; };

	CImGuiManager::SFlushBatchGuard imguiFlushBatch;

	CImGuiNameRenderer& nameRenderer = CImGuiNameRenderer::Instance();
	nameRenderer.BeginBatch();

	// Character names
	for (TTextTailList::iterator itor = m_CharacterTextTailList.begin(); itor != m_CharacterTextTailList.end(); ++itor)
	{
		TTextTail* pTextTail = *itor;

		// Main name
		DWORD dwNameColor = MAKE_COLOR_ARGB(
			pTextTail->Color.r,
			pTextTail->Color.g,
			pTextTail->Color.b,
			pTextTail->Color.a);

		const Vector3& namePos = pTextTail->pTextInstance->GetPositionRef();
		nameRenderer.AddText(
			CImGuiNameRenderer::ETextType::Name,
			pTextTail->pTextInstance->GetValueStringReference().c_str(),
			namePos.x,
			namePos.y,
			namePos.z,
			dwNameColor,
			true,
			0xFF000000,
			ConvertHAlign(pTextTail->pTextInstance->GetHAlign()),
			ConvertVAlign(pTextTail->pTextInstance->GetVAlign()),
			pTextTail->dwVirtualID);

		// Guild mark + name
		if (pTextTail->pMarkInstance && pTextTail->pGuildNameTextInstance)
		{
			pTextTail->pMarkInstance->Render();

			const Vector3& guildPos = pTextTail->pGuildNameTextInstance->GetPositionRef();
			nameRenderer.AddText(
				CImGuiNameRenderer::ETextType::Guild,
				pTextTail->pGuildNameTextInstance->GetValueStringReference().c_str(),
				guildPos.x,
				guildPos.y,
				guildPos.z,
				pTextTail->pGuildNameTextInstance->GetColor(),
				true,
				0xFF000000,
				ConvertHAlign(pTextTail->pGuildNameTextInstance->GetHAlign()),
				ConvertVAlign(pTextTail->pGuildNameTextInstance->GetVAlign()),
				pTextTail->dwVirtualID);
		}

		// Title
		if (pTextTail->pTitleTextInstance)
		{
			const Vector3& titlePos = pTextTail->pTitleTextInstance->GetPositionRef();
			nameRenderer.AddText(
				CImGuiNameRenderer::ETextType::Title,
				pTextTail->pTitleTextInstance->GetValueStringReference().c_str(),
				titlePos.x,
				titlePos.y,
				titlePos.z,
				pTextTail->pTitleTextInstance->GetColor(),
				true,
				0xFF000000,
				ConvertHAlign(pTextTail->pTitleTextInstance->GetHAlign()),
				ConvertVAlign(pTextTail->pTitleTextInstance->GetVAlign()),
				pTextTail->dwVirtualID);
		}

		// Level
		if (pTextTail->pLevelTextInstance)
		{
			const Vector3& levelPos = pTextTail->pLevelTextInstance->GetPositionRef();
			nameRenderer.AddText(
				CImGuiNameRenderer::ETextType::Level,
				pTextTail->pLevelTextInstance->GetValueStringReference().c_str(),
				levelPos.x,
				levelPos.y,
				levelPos.z,
				pTextTail->pLevelTextInstance->GetColor(),
				true,
				0xFF000000,
				ConvertHAlign(pTextTail->pLevelTextInstance->GetHAlign()),
				ConvertVAlign(pTextTail->pLevelTextInstance->GetVAlign()),
				pTextTail->dwVirtualID);
		}
	}

	// Item names
	for (TTextTailList::iterator itor = m_ItemTextTailList.begin(); itor != m_ItemTextTailList.end(); ++itor)
	{
		TTextTail* pTextTail = *itor;

		DWORD dwItemColor = MAKE_COLOR_ARGB(
			pTextTail->Color.r,
			pTextTail->Color.g,
			pTextTail->Color.b,
			pTextTail->Color.a);

		nameRenderer.AddItemBox(
			pTextTail->pTextInstance->GetValueStringReference().c_str(),
			pTextTail->x,
			pTextTail->y,
			pTextTail->z,
			dwItemColor,
			pTextTail->xStart,
			pTextTail->yStart,
			pTextTail->xEnd,
			pTextTail->yEnd,
			pTextTail->dwVirtualID);

		if (pTextTail->pOwnerTextInstance)
		{
			const Vector3& ownerPos = pTextTail->pOwnerTextInstance->GetPositionRef();
			nameRenderer.AddText(
				CImGuiNameRenderer::ETextType::Info,
				pTextTail->pOwnerTextInstance->GetValueStringReference().c_str(),
				ownerPos.x,
				ownerPos.y,
				ownerPos.z,
				pTextTail->pOwnerTextInstance->GetColor(),
				true,
				0xFF000000,
				ConvertHAlign(pTextTail->pOwnerTextInstance->GetHAlign()),
				ConvertVAlign(pTextTail->pOwnerTextInstance->GetVAlign()),
				pTextTail->dwVirtualID);
		}
	}

	nameRenderer.EndBatch();

	for (TChatTailMap::iterator itorChat = m_ChatTailMap.begin(); itorChat != m_ChatTailMap.end(); ++itorChat)
	{
		TTextTail* pTextTail = itorChat->second;
		if (pTextTail->fDistanceFromPlayer < TEXTTAIL_SHOW_DIST_SQ && pTextTail->pOwner->isShow())
			pTextTail->pTextInstance->Render();
	}

#else
	// Original rendering when ImGui not enabled
	TTextTailList::iterator itor;

	for (itor = m_CharacterTextTailList.begin(); itor != m_CharacterTextTailList.end(); ++itor)
	{
		TTextTail * pTextTail = *itor;
		pTextTail->pTextInstance->Render();
		if (pTextTail->pMarkInstance && pTextTail->pGuildNameTextInstance)
		{
			pTextTail->pMarkInstance->Render();
			pTextTail->pGuildNameTextInstance->Render();
		}
		if (pTextTail->pTitleTextInstance)
		{
			pTextTail->pTitleTextInstance->Render();
		}
		if (pTextTail->pLevelTextInstance)
		{
			pTextTail->pLevelTextInstance->Render();
		}
	}

	for (itor = m_ItemTextTailList.begin(); itor != m_ItemTextTailList.end(); ++itor)
	{
		TTextTail * pTextTail = *itor;

		RenderTextTailBox(pTextTail);
		pTextTail->pTextInstance->Render();
		if (pTextTail->pOwnerTextInstance)
			pTextTail->pOwnerTextInstance->Render();
	}

	for (TChatTailMap::iterator itorChat = m_ChatTailMap.begin(); itorChat!=m_ChatTailMap.end(); ++itorChat)
	{
		TTextTail * pTextTail = itorChat->second;
		if (pTextTail->fDistanceFromPlayer < TEXTTAIL_SHOW_DIST_SQ && pTextTail->pOwner->isShow())
			RenderTextTailName(pTextTail);
	}
#endif
}

void CPythonTextTail::RenderTextTailBox(TTextTail * pTextTail)
{
	CPythonGraphic::Instance().SetDiffuseColor(0.0f, 0.0f, 0.0f, 1.0f);
	CPythonGraphic::Instance().RenderBox2d(pTextTail->x + pTextTail->xStart,
										   pTextTail->y + pTextTail->yStart,
										   pTextTail->x + pTextTail->xEnd,
										   pTextTail->y + pTextTail->yEnd,
										   pTextTail->z);

	CPythonGraphic::Instance().SetDiffuseColor(0.0f, 0.0f, 0.0f, 0.3f);
	CPythonGraphic::Instance().RenderBar2d(pTextTail->x + pTextTail->xStart,
										   pTextTail->y + pTextTail->yStart,
										   pTextTail->x + pTextTail->xEnd,
										   pTextTail->y + pTextTail->yEnd,
										   pTextTail->z);
}

void CPythonTextTail::RenderTextTailName(TTextTail * pTextTail)
{
	pTextTail->pTextInstance->Render();
}

void CPythonTextTail::HideAllTextTail()
{
	for (TTextTailList::iterator it = m_CharacterTextTailList.begin(); it != m_CharacterTextTailList.end(); ++it)
		(*it)->bIsShowing = false;
	for (TTextTailList::iterator it = m_ItemTextTailList.begin(); it != m_ItemTextTailList.end(); ++it)
		(*it)->bIsShowing = false;
	m_CharacterTextTailList.clear();
	m_ItemTextTailList.clear();
}

void CPythonTextTail::UpdateDistance(const TPixelPosition & c_rCenterPosition, TTextTail * pTextTail)
{
	const Vector3 & c_rv3Position = pTextTail->pOwner->GetPosition();
	float dx = c_rv3Position.x - c_rCenterPosition.x;
	float dy = -c_rv3Position.y - c_rCenterPosition.y;
	pTextTail->fDistanceFromPlayer = dx * dx + dy * dy; // squared distance — avoids sqrt for 999 mobs
}

void CPythonTextTail::ShowAllTextTail()
{
	std::vector<std::pair<float, DWORD>>* pCandidates;
	static std::vector<std::pair<float, DWORD>> s_kVct_kCharCandidates;

	if (s_bCharCandidatesCacheReady)
	{
		pCandidates = &s_kCharCandidatesCache;
	}
	else
	{
		// Fallback if UpdateAllTextTail wasn't called first
		s_kVct_kCharCandidates.clear();
		for (TTextTailMap::iterator itor = m_CharacterTextTailMap.begin(); itor != m_CharacterTextTailMap.end(); ++itor)
		{
			if (itor->second->fDistanceFromPlayer < TEXTTAIL_SHOW_DIST_SQ)
				s_kVct_kCharCandidates.push_back(std::make_pair(itor->second->fDistanceFromPlayer, itor->first));
		}
		pCandidates = &s_kVct_kCharCandidates;
	}

	if ((int)pCandidates->size() > MAX_SHOWING_CHARACTER_TEXTTAILS)
	{
		std::partial_sort(pCandidates->begin(),
			pCandidates->begin() + MAX_SHOWING_CHARACTER_TEXTTAILS,
			pCandidates->end());
	}

	int iLimit = min((int)pCandidates->size(), MAX_SHOWING_CHARACTER_TEXTTAILS);
	for (int i = 0; i < iLimit; ++i)
		ShowCharacterTextTail((*pCandidates)[i].second);

	for (TTextTailMap::iterator itor = m_ItemTextTailMap.begin(); itor != m_ItemTextTailMap.end(); ++itor)
	{
		if (itor->second->fDistanceFromPlayer < TEXTTAIL_SHOW_DIST_SQ)
			ShowItemTextTail(itor->first);
	}
}

void CPythonTextTail::ShowCharacterTextTail(DWORD VirtualID)
{
	TTextTailMap::iterator itor = m_CharacterTextTailMap.find(VirtualID);

	if (m_CharacterTextTailMap.end() == itor)
		return;

	TTextTail * pTextTail = itor->second;

	if (pTextTail->bIsShowing)
		return;

	if (!pTextTail->pOwner->isShow())
		return;

	CInstanceBase * pInstance = CPythonCharacterManager::Instance().GetInstancePtr(pTextTail->dwVirtualID);
	if (!pInstance)
		return;

	if (pInstance->IsGuildWall())
		return;

	if (pInstance->CanPickInstance())
	{
		pTextTail->bIsShowing = true;
		m_CharacterTextTailList.push_back(pTextTail);
	}
}

void CPythonTextTail::ShowItemTextTail(DWORD VirtualID)
{
	TTextTailMap::iterator itor = m_ItemTextTailMap.find(VirtualID);

	if (m_ItemTextTailMap.end() == itor)
		return;

	TTextTail * pTextTail = itor->second;

	if (pTextTail->bIsShowing)
		return;

	pTextTail->bIsShowing = true;
	m_ItemTextTailList.push_back(pTextTail);
}

bool CPythonTextTail::isIn(CPythonTextTail::TTextTail * pSource, CPythonTextTail::TTextTail * pTarget)
{
	float x1Source = pSource->x + pSource->xStart;
	float y1Source = pSource->y + pSource->yStart;
	float x2Source = pSource->x + pSource->xEnd;
	float y2Source = pSource->y + pSource->yEnd;
	float x1Target = pTarget->x + pTarget->xStart;
	float y1Target = pTarget->y + pTarget->yStart;
	float x2Target = pTarget->x + pTarget->xEnd;
	float y2Target = pTarget->y + pTarget->yEnd;

	if (x1Source <= x2Target && x2Source >= x1Target &&
	    y1Source <= y2Target && y2Source >= y1Target)
	{
		return true;
	}

	return false;
}

void CPythonTextTail::RegisterCharacterTextTail(DWORD dwGuildID, DWORD dwVirtualID, const Color & c_rColor, float fAddHeight)
{
	CInstanceBase * pCharacterInstance = CPythonCharacterManager::Instance().GetInstancePtr(dwVirtualID);

	if (!pCharacterInstance)
		return;

	TTextTail * pTextTail = RegisterTextTail(dwVirtualID,
											 pCharacterInstance->GetNameString(),
											 pCharacterInstance->GetGraphicThingInstancePtr(),
											 pCharacterInstance->GetGraphicThingInstanceRef().GetHeight() + fAddHeight,
											 c_rColor);

	CGraphicTextInstance * pTextInstance = pTextTail->pTextInstance;
	pTextInstance->SetOutline(true);
	pTextInstance->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);

	pTextTail->pMarkInstance=NULL;
	pTextTail->pGuildNameTextInstance=NULL;
	pTextTail->pTitleTextInstance=NULL;
	pTextTail->pLevelTextInstance=NULL;

	if (0 != dwGuildID)
	{
		pTextTail->pMarkInstance = CGraphicMarkInstance::New();

		DWORD dwMarkID = CGuildMarkManager::Instance().GetMarkID(dwGuildID);

		if (dwMarkID != CGuildMarkManager::INVALID_MARK_ID)
		{
			std::string markImagePath;

			if (CGuildMarkManager::Instance().GetMarkImageFilename(dwMarkID / CGuildMarkImage::MARK_TOTAL_COUNT, markImagePath))
			{
				pTextTail->pMarkInstance->SetImageFileName(markImagePath.c_str());
				pTextTail->pMarkInstance->Load();
				pTextTail->pMarkInstance->SetIndex(dwMarkID % CGuildMarkImage::MARK_TOTAL_COUNT);
			}
		}

		std::string strGuildName;
		if (!CPythonGuild::Instance().GetGuildName(dwGuildID, &strGuildName))
			strGuildName = "Noname";

		CGraphicTextInstance *& prGuildNameInstance = pTextTail->pGuildNameTextInstance;
		prGuildNameInstance = CGraphicTextInstance::New();
		prGuildNameInstance->SetTextPointer(ms_pFont);
		prGuildNameInstance->SetOutline(true);
		prGuildNameInstance->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
		prGuildNameInstance->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);
		prGuildNameInstance->SetValue(strGuildName.c_str());
		prGuildNameInstance->SetColor(c_TextTail_Guild_Name_Color.r, c_TextTail_Guild_Name_Color.g, c_TextTail_Guild_Name_Color.b);
		prGuildNameInstance->Update();
	}

	m_CharacterTextTailMap.insert(TTextTailMap::value_type(dwVirtualID, pTextTail));
}

void CPythonTextTail::RegisterItemTextTail(DWORD VirtualID, const char * c_szText, CGraphicObjectInstance * pOwner)
{
#ifdef __DEBUG
	char szName[256];
	spritnf(szName, "%s[%d]", c_szText, VirtualID);

	TTextTail * pTextTail = RegisterTextTail(VirtualID, c_szText, pOwner, c_TextTail_Name_Position, c_TextTail_Item_Color);
	m_ItemTextTailMap.insert(TTextTailMap::value_type(VirtualID, pTextTail));
#else
	TTextTail * pTextTail = RegisterTextTail(VirtualID, c_szText, pOwner, c_TextTail_Name_Position, c_TextTail_Item_Color);
	m_ItemTextTailMap.insert(TTextTailMap::value_type(VirtualID, pTextTail));
#endif
}

void CPythonTextTail::RegisterChatTail(DWORD VirtualID, const char * c_szChat)
{
	CInstanceBase * pCharacterInstance = CPythonCharacterManager::Instance().GetInstancePtr(VirtualID);

	if (!pCharacterInstance)
		return;

	TChatTailMap::iterator itor = m_ChatTailMap.find(VirtualID);

	if (m_ChatTailMap.end() != itor)
	{
		TTextTail * pTextTail = itor->second;

		pTextTail->pTextInstance->SetValue(c_szChat);
		pTextTail->pTextInstance->Update();
		pTextTail->Color = c_TextTail_Chat_Color;
		pTextTail->pTextInstance->SetColor(c_TextTail_Chat_Color);

		// TEXTTAIL_LIVINGTIME_CONTROL
		pTextTail->LivingTime = DX::StepTimer::instance().GetTotalMillieSeconds() + TextTail_GetLivingTime();
		// END_OF_TEXTTAIL_LIVINGTIME_CONTROL

		pTextTail->bNameFlag = TRUE;

		return;
	}

	auto baseHeight = pCharacterInstance->GetGraphicThingInstanceRef().GetHeight() + 10.0f;
#ifdef ENABLE_RACE_HEIGHT
	baseHeight += pCharacterInstance->GetBaseHeight();
#endif

	TTextTail * pTextTail = RegisterTextTail(VirtualID,
											 c_szChat,
											 pCharacterInstance->GetGraphicThingInstancePtr(),
											 baseHeight,
											 c_TextTail_Chat_Color);

	// TEXTTAIL_LIVINGTIME_CONTROL
	pTextTail->LivingTime = DX::StepTimer::instance().GetTotalMillieSeconds() + TextTail_GetLivingTime();
	// END_OF_TEXTTAIL_LIVINGTIME_CONTROL

	pTextTail->bNameFlag = TRUE;
	pTextTail->pTextInstance->SetOutline(true);
	pTextTail->pTextInstance->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);
	m_ChatTailMap.emplace(VirtualID, pTextTail);
}

void CPythonTextTail::RegisterInfoTail(DWORD VirtualID, const char * c_szChat)
{
	CInstanceBase * pCharacterInstance = CPythonCharacterManager::Instance().GetInstancePtr(VirtualID);

	if (!pCharacterInstance)
		return;

	TChatTailMap::iterator itor = m_ChatTailMap.find(VirtualID);

	if (m_ChatTailMap.end() != itor)
	{
		TTextTail * pTextTail = itor->second;

		pTextTail->pTextInstance->SetValue(c_szChat);
		pTextTail->pTextInstance->Update();
		pTextTail->Color = c_TextTail_Info_Color;
		pTextTail->pTextInstance->SetColor(c_TextTail_Info_Color);

		// TEXTTAIL_LIVINGTIME_CONTROL
		pTextTail->LivingTime = DX::StepTimer::instance().GetTotalMillieSeconds() + TextTail_GetLivingTime();
		// END_OF_TEXTTAIL_LIVINGTIME_CONTROL

		pTextTail->bNameFlag = FALSE;

		return;
	}

	TTextTail * pTextTail = RegisterTextTail(VirtualID,
											 c_szChat,
											 pCharacterInstance->GetGraphicThingInstancePtr(),
											 pCharacterInstance->GetGraphicThingInstanceRef().GetHeight() + 10.0f,
											 c_TextTail_Info_Color);

	// TEXTTAIL_LIVINGTIME_CONTROL
	pTextTail->LivingTime = DX::StepTimer::instance().GetTotalMillieSeconds() + TextTail_GetLivingTime();
	// END_OF_TEXTTAIL_LIVINGTIME_CONTROL

	pTextTail->bNameFlag = FALSE;
	pTextTail->pTextInstance->SetOutline(true);
	pTextTail->pTextInstance->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);
	m_ChatTailMap.insert(TTextTailMap::value_type(VirtualID, pTextTail));
}

bool CPythonTextTail::GetTextTailPosition(DWORD dwVID, float* px, float* py, float* pz)
{
	TTextTailMap::iterator itorCharacter = m_CharacterTextTailMap.find(dwVID);

	if (m_CharacterTextTailMap.end() == itorCharacter)
	{
		return false;
	}

	TTextTail * pTextTail = itorCharacter->second;
	*px=pTextTail->x;
	*py=pTextTail->y;
	*pz=pTextTail->z;

	return true;
}

bool CPythonTextTail::IsChatTextTail(DWORD dwVID)
{
	TChatTailMap::iterator itorChat = m_ChatTailMap.find(dwVID);

	if (m_ChatTailMap.end() == itorChat)
		return false;

	return true;
}

void CPythonTextTail::SetCharacterTextTailColor(DWORD VirtualID, const Color & c_rColor)
{
	TTextTailMap::iterator itorCharacter = m_CharacterTextTailMap.find(VirtualID);

	if (m_CharacterTextTailMap.end() == itorCharacter)
		return;

	TTextTail * pTextTail = itorCharacter->second;
	pTextTail->pTextInstance->SetColor(c_rColor);
	pTextTail->Color = c_rColor;
}

void CPythonTextTail::SetItemTextTailOwner(DWORD dwVID, const char * c_szName)
{
	TTextTailMap::iterator itor = m_ItemTextTailMap.find(dwVID);
	if (m_ItemTextTailMap.end() == itor)
		return;

	TTextTail * pTextTail = itor->second;

	if (strlen(c_szName) > 0)
	{
		if (!pTextTail->pOwnerTextInstance)
		{
			pTextTail->pOwnerTextInstance = CGraphicTextInstance::New();
		}

		std::string strName = c_szName;
		static const string & strOwnership = ApplicationStringTable_GetString(IDS_POSSESSIVE_MORPHENE) == "" ? "'s" : ApplicationStringTable_GetString(IDS_POSSESSIVE_MORPHENE);
		strName += strOwnership;

		pTextTail->pOwnerTextInstance->SetTextPointer(ms_pFont);
		pTextTail->pOwnerTextInstance->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
		pTextTail->pOwnerTextInstance->SetValue(strName.c_str());
		pTextTail->pOwnerTextInstance->SetColor(1.0f, 1.0f, 0.0f);
		pTextTail->pOwnerTextInstance->Update();

		int xOwnerSize, yOwnerSize;
		pTextTail->pOwnerTextInstance->GetTextSize(&xOwnerSize, &yOwnerSize);
		pTextTail->yStart	= -2.0f;
		pTextTail->yEnd		+= float(yOwnerSize + 4);
		pTextTail->xStart	= fMIN(pTextTail->xStart, float(-xOwnerSize / 2 - 1));
		pTextTail->xEnd		= fMAX(pTextTail->xEnd, float(xOwnerSize / 2 + 1));
	}
	else
	{
		if (pTextTail->pOwnerTextInstance)
		{
			CGraphicTextInstance::Delete(pTextTail->pOwnerTextInstance);
			pTextTail->pOwnerTextInstance = NULL;
		}

		int xSize, ySize;
		pTextTail->pTextInstance->GetTextSize(&xSize, &ySize);
		pTextTail->xStart	= (float) (-xSize / 2 - 2);
		pTextTail->yStart	= -2.0f;
		pTextTail->xEnd		= (float) (xSize / 2 + 2);
		pTextTail->yEnd		= (float) ySize;
	}
}

void CPythonTextTail::DeleteCharacterTextTail(DWORD VirtualID)
{
	TTextTailMap::iterator itorCharacter = m_CharacterTextTailMap.find(VirtualID);
	TTextTailMap::iterator itorChat = m_ChatTailMap.find(VirtualID);

	if (m_CharacterTextTailMap.end() != itorCharacter)
	{
		TTextTail * pTextTail = itorCharacter->second;
		if (pTextTail->bIsShowing)
		{
			m_CharacterTextTailList.remove(pTextTail);
			pTextTail->bIsShowing = false;
		}
		DeleteTextTail(pTextTail);
		m_CharacterTextTailMap.erase(itorCharacter);
	}
	else
	{
		Tracenf("CPythonTextTail::DeleteCharacterTextTail - Find VID[%d] Error", VirtualID);
	}

	if (m_ChatTailMap.end() != itorChat)
	{
		DeleteTextTail(itorChat->second);
		m_ChatTailMap.erase(itorChat);
	}
}

void CPythonTextTail::DeleteItemTextTail(DWORD VirtualID)
{
	TTextTailMap::iterator itor = m_ItemTextTailMap.find(VirtualID);

	if (m_ItemTextTailMap.end() == itor)
	{
		Tracef(" CPythonTextTail::DeleteItemTextTail - None Item Text Tail\n");
		return;
	}

	TTextTail * pTextTail = itor->second;
	if (pTextTail->bIsShowing)
	{
		m_ItemTextTailList.remove(pTextTail);
		pTextTail->bIsShowing = false;
	}
	DeleteTextTail(pTextTail);
	m_ItemTextTailMap.erase(itor);
}

CPythonTextTail::TTextTail * CPythonTextTail::RegisterTextTail(DWORD dwVirtualID, const char * c_szText, CGraphicObjectInstance * pOwner, float fHeight, const Color & c_rColor)
{
	TTextTail * pTextTail = m_TextTailPool.Alloc();

	pTextTail->dwVirtualID = dwVirtualID;
	pTextTail->pOwner = pOwner;
	pTextTail->pTextInstance = CGraphicTextInstance::New();
	pTextTail->pOwnerTextInstance = NULL;
	pTextTail->fHeight = fHeight;

	pTextTail->pTextInstance->SetTextPointer(ms_pFont);
	pTextTail->pTextInstance->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
	pTextTail->pTextInstance->SetValue(c_szText);
	pTextTail->pTextInstance->SetColor(c_rColor.r, c_rColor.g, c_rColor.b);
	pTextTail->pTextInstance->Update();

	int xSize, ySize;
	pTextTail->pTextInstance->GetTextSize(&xSize, &ySize);
	pTextTail->xStart				= (float) (-xSize / 2 - 2);
	pTextTail->yStart				= -2.0f;
	pTextTail->xEnd					= (float) (xSize / 2 + 2);
	pTextTail->yEnd					= (float) ySize;
	pTextTail->Color				= c_rColor;
	pTextTail->fDistanceFromPlayer	= 0.0f;
	pTextTail->bIsShowing			= false;
	pTextTail->x = -100.0f;
	pTextTail->y = -100.0f;
	pTextTail->z = 0.0f;
	pTextTail->pMarkInstance = NULL;
	pTextTail->pGuildNameTextInstance = NULL;
	pTextTail->pTitleTextInstance = NULL;
	pTextTail->pLevelTextInstance = NULL;
	return pTextTail;
}

void CPythonTextTail::DeleteTextTail(TTextTail * pTextTail)
{
	if (pTextTail->pTextInstance)
	{
		CGraphicTextInstance::Delete(pTextTail->pTextInstance);
		pTextTail->pTextInstance = NULL;
	}
	if (pTextTail->pOwnerTextInstance)
	{
		CGraphicTextInstance::Delete(pTextTail->pOwnerTextInstance);
		pTextTail->pOwnerTextInstance = NULL;
	}
	if (pTextTail->pMarkInstance)
	{
		CGraphicMarkInstance::Delete(pTextTail->pMarkInstance);
		pTextTail->pMarkInstance = NULL;
	}
	if (pTextTail->pGuildNameTextInstance)
	{
		CGraphicTextInstance::Delete(pTextTail->pGuildNameTextInstance);
		pTextTail->pGuildNameTextInstance = NULL;
	}
	if (pTextTail->pTitleTextInstance)
	{
		CGraphicTextInstance::Delete(pTextTail->pTitleTextInstance);
		pTextTail->pTitleTextInstance = NULL;
	}
	if (pTextTail->pLevelTextInstance)
	{
		CGraphicTextInstance::Delete(pTextTail->pLevelTextInstance);
		pTextTail->pLevelTextInstance = NULL;
	}

	m_TextTailPool.Free(pTextTail);
}

int CPythonTextTail::Pick(int ixMouse, int iyMouse)
{
	for (TTextTailMap::iterator itor = m_ItemTextTailMap.begin(); itor != m_ItemTextTailMap.end(); ++itor)
	{
		TTextTail * pTextTail = itor->second;

		if (ixMouse >= pTextTail->x + pTextTail->xStart && ixMouse <= pTextTail->x + pTextTail->xEnd &&
			iyMouse >= pTextTail->y + pTextTail->yStart && iyMouse <= pTextTail->y + pTextTail->yEnd)
		{
			SelectItemName(itor->first);
			return (itor->first);
		}
	}

	return -1;
}

void CPythonTextTail::SelectItemName(DWORD dwVirtualID)
{
	TTextTailMap::iterator itor = m_ItemTextTailMap.find(dwVirtualID);

	if (m_ItemTextTailMap.end() == itor)
		return;

	TTextTail * pTextTail = itor->second;
	pTextTail->pTextInstance->SetColor(0.1f, 0.9f, 0.1f);
}

void CPythonTextTail::AttachTitle(DWORD dwVID, const char * c_szName, const Color & c_rColor)
{
	if (!bPKTitleEnable)
		return;

	TTextTailMap::iterator itor = m_CharacterTextTailMap.find(dwVID);
	if (m_CharacterTextTailMap.end() == itor)
		return;

	TTextTail * pTextTail = itor->second;

	CGraphicTextInstance *& prTitle = pTextTail->pTitleTextInstance;
	if (!prTitle)
	{
		prTitle = CGraphicTextInstance::New();
		prTitle->SetTextPointer(ms_pFont);
		prTitle->SetOutline(true);
		prTitle->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_RIGHT);
		prTitle->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);
	}

	prTitle->SetValue(c_szName);
	prTitle->SetColor(c_rColor.r, c_rColor.g, c_rColor.b);
	prTitle->Update();
}

void CPythonTextTail::DetachTitle(DWORD dwVID)
{
	if (!bPKTitleEnable)
		return;

	TTextTailMap::iterator itor = m_CharacterTextTailMap.find(dwVID);
	if (m_CharacterTextTailMap.end() == itor)
		return;

	TTextTail * pTextTail = itor->second;

	if (pTextTail->pTitleTextInstance)
	{
		CGraphicTextInstance::Delete(pTextTail->pTitleTextInstance);
		pTextTail->pTitleTextInstance = NULL;
	}
}

void CPythonTextTail::EnablePKTitle(BOOL bFlag)
{
	bPKTitleEnable = bFlag;
}

void CPythonTextTail::AttachLevel(DWORD dwVID, const char * c_szText, const Color & c_rColor)
{
	if (!bPKTitleEnable)
		return;

	TTextTailMap::iterator itor = m_CharacterTextTailMap.find(dwVID);
	if (m_CharacterTextTailMap.end() == itor)
		return;

	TTextTail * pTextTail = itor->second;

	CGraphicTextInstance *& prLevel = pTextTail->pLevelTextInstance;
	if (!prLevel)
	{
		prLevel = CGraphicTextInstance::New();
		prLevel->SetTextPointer(ms_pFont);
		prLevel->SetOutline(true);

		prLevel->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_RIGHT);
		prLevel->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);
	}

	prLevel->SetValue(c_szText);
	prLevel->SetColor(c_rColor.r, c_rColor.g, c_rColor.b);
	prLevel->Update();
}

void CPythonTextTail::DetachLevel(DWORD dwVID)
{
	if (!bPKTitleEnable)
		return;

	TTextTailMap::iterator itor = m_CharacterTextTailMap.find(dwVID);
	if (m_CharacterTextTailMap.end() == itor)
		return;

	TTextTail * pTextTail = itor->second;

	if (pTextTail->pLevelTextInstance)
	{
		CGraphicTextInstance::Delete(pTextTail->pLevelTextInstance);
		pTextTail->pLevelTextInstance = NULL;
	}
}

void CPythonTextTail::Initialize()
{
	// DEFAULT_FONT
	//ms_pFont = (CGraphicText *)CResourceManager::Instance().GetTypeResourcePointer(g_strDefaultFontName.c_str());

	CGraphicText* pkDefaultFont = static_cast<CGraphicText*>(DefaultFont_GetResource());
	if (!pkDefaultFont)
	{
		TraceError("CPythonTextTail::Initialize - CANNOT_FIND_DEFAULT_FONT");
		return;
	}

	ms_pFont = pkDefaultFont;
	// END_OF_DEFAULT_FONT
}

void CPythonTextTail::Destroy()
{
	m_TextTailPool.Clear();
}

void CPythonTextTail::Clear()
{
	m_CharacterTextTailMap.clear();
	m_CharacterTextTailList.clear();
	m_ItemTextTailMap.clear();
	m_ItemTextTailList.clear();
	m_ChatTailMap.clear();

	m_TextTailPool.Clear();
}

CPythonTextTail::CPythonTextTail()
{
	Clear();
}

CPythonTextTail::~CPythonTextTail()
{
	Destroy();
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
