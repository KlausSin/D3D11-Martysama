#ifndef FOXFS_ARCHIVE_H
#define FOXFS_ARCHIVE_H

#include <algorithm>
#include <map>
#include <cstdlib>
#include <cctype>

#include <cryptopp/crc.h>

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)
#pragma warning(disable : 4996)
#include <windows.h>
#else
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#include "Config.h"

namespace FoxFS
{

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)
	inline std::string AnsiToUtf8(const char* ansi)
	{
		if (!ansi || !*ansi) return std::string();
		int len = static_cast<int>(strlen(ansi));
		// First convert ANSI to wide string
		int wideSize = MultiByteToWideChar(CP_ACP, 0, ansi, len, NULL, 0);
		if (wideSize == 0) return std::string(ansi);
		std::wstring wide(wideSize, 0);
		MultiByteToWideChar(CP_ACP, 0, ansi, len, &wide[0], wideSize);
		// Then convert wide string to UTF-8
		int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wideSize, NULL, 0, NULL, NULL);
		if (utf8Size == 0) return std::string(ansi);
		std::string utf8(utf8Size, 0);
		WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wideSize, &utf8[0], utf8Size, NULL, NULL);
		return utf8;
	}
#endif

	class Archive
	{
	public:
		enum
		{
			ERROR_OK = 0,
			ERROR_BASE_CODE = 0,
			ERROR_FILE_WAS_NOT_FOUND = ERROR_BASE_CODE + 1,
			ERROR_CORRUPTED_FILE = ERROR_BASE_CODE + 2,
			ERROR_MISSING_KEY = ERROR_BASE_CODE + 3,
			ERROR_MISSING_IV = ERROR_BASE_CODE + 4,
			ERROR_DECRYPTION_HAS_FAILED = ERROR_BASE_CODE + 5,
			ERROR_DECOMPRESSION_FAILED = ERROR_BASE_CODE + 6,
			ERROR_ARCHIVE_NOT_FOUND = ERROR_BASE_CODE + 7,
			ERROR_ARCHIVE_NOT_READABLE = ERROR_BASE_CODE + 8,
			ERROR_ARCHIVE_INVALID = ERROR_BASE_CODE + 9,
			ERROR_ARCHIVE_ACCESS_DENIED = ERROR_BASE_CODE + 10,
			ERROR_KEYSERVER_SOCKET = ERROR_BASE_CODE + 11,
			ERROR_KEYSERVER_CONNECTION = ERROR_BASE_CODE + 12,
			ERROR_KEYSERVER_RESPONSE = ERROR_BASE_CODE + 13,
			ERROR_KEYSERVER_TIMEOUT = ERROR_BASE_CODE + 14,
			ERROR_UNKNOWN = ERROR_BASE_CODE + 15
		};

		struct FileListEntry
		{
			unsigned long long offset;
			unsigned int size;
			unsigned int decompressed;
			unsigned int hash;
			unsigned int name;
		};

	public:
		Archive();
		~Archive();

		const wchar_t* getFilename() const;

		int exists(const char* filename) const;
		unsigned int size(const char* filename) const;
		int get(const char* filename, void* buffer, unsigned int maxsize, unsigned int* outsize) const;

		int load(const wchar_t* filename, const void* key, const void* iv);
		void unload();

		static inline unsigned int generateFilenameIndex(const char* filename)
		{
#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)
			// Convert from ANSI to UTF-8 to match archiver encoding
			std::string fn = AnsiToUtf8(filename);
#else
			std::string fn = filename;
#endif
			transform(fn.begin(), fn.end(), fn.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
			for (size_t i = 0; i < fn.length(); ++i)
			{
				if (fn[i] == '\\')
				{
					fn[i] = '/';
				}
			}
			unsigned int index = 0;
			CryptoPP::CRC32 crc;
			crc.CalculateDigest(reinterpret_cast<unsigned char*>(&index), reinterpret_cast<const unsigned char*>(fn.c_str()), fn.length());
			return index;
		}

	private:
		std::map<unsigned int, FileListEntry> files;

		unsigned char key[32];
		unsigned char iv[32];

		TArchiveHeader header;

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)
		HANDLE file;
#else
		int file;
#endif
		wchar_t filename[
#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)
			MAX_PATH + 1
#else
			PATH_MAX + 1
#endif
		];
	};

}

#endif