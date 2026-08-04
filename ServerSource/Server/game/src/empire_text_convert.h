#ifndef __INC_EMPIRE_CHAT_CONVERT
#define __INC_EMPIRE_CHAT_CONVERT

bool LoadEmpireTextConvertTable(DWORD dwEmpireID, const char* c_szFileName);
void ConvertEmpireText(DWORD dwEmpireID, char* szText, size_t len, int iPct);

#endif
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
