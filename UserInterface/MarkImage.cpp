#include "stdafx.h"
#include "MarkImage.h"

#include <stb_image.h>
#include <stb_image_write.h>

#if !defined(_MSC_VER)
#include "crc32.h"
#include "lzo_manager.h"
#include "minilzo.h"
#define CLZO LZOManager
#else
#define sys_err TraceError
#define sys_log(...)
#define thecore_memcpy memcpy
#define itertype(cont) typeof(cont.begin())
#endif

CGuildMarkImage * NewMarkImage()
{
	return new CGuildMarkImage;
}

void DeleteMarkImage(CGuildMarkImage * pkImage)
{
	delete pkImage;
}

CGuildMarkImage::CGuildMarkImage()
{
	m_bValid = false;
	memset(m_apxImage, 0, sizeof(m_apxImage));
}

CGuildMarkImage::~CGuildMarkImage()
{
	Destroy();
}

void CGuildMarkImage::Destroy()
{
	m_bValid = false;
}

void CGuildMarkImage::Create()
{
	if (!m_bValid)
	{
		memset(m_apxImage, 0, sizeof(m_apxImage));
		m_bValid = true;
	}
}

void CGuildMarkImage::ConvertRGBAtoBGRA(Pixel* pixels, int count)
{
	for (int i = 0; i < count; ++i)
	{
		BYTE* p = (BYTE*)&pixels[i];
		BYTE temp = p[0];
		p[0] = p[2];
		p[2] = temp;
	}
}

// Convert BGRA to RGBA (for saving with stb_image_write)
void CGuildMarkImage::ConvertBGRAtoRGBA(Pixel* pixels, int count)
{
	ConvertRGBAtoBGRA(pixels, count); // Same operation
}

bool CGuildMarkImage::Save(const char* c_szFileName)
{
	if (!m_bValid)
		return false;

	// Create a copy for saving (convert BGRA to RGBA)
	Pixel* tempImage = new Pixel[WIDTH * HEIGHT];
	memcpy(tempImage, m_apxImage, sizeof(m_apxImage));
	ConvertBGRAtoRGBA(tempImage, WIDTH * HEIGHT);

	// stb_image_write expects RGBA, save as TGA
	int result = stbi_write_tga(c_szFileName, WIDTH, HEIGHT, 4, tempImage);

	delete[] tempImage;
	return result != 0;
}

bool CGuildMarkImage::Build(const char * c_szFileName)
{
	Destroy();
	Create();

	// Initialize with black/transparent pixels
	memset(m_apxImage, 0, sizeof(m_apxImage));

	return Save(c_szFileName);
}

bool CGuildMarkImage::Load(const char * c_szFileName)
{
	Destroy();

	int width, height, channels;
	unsigned char* data = stbi_load(c_szFileName, &width, &height, &channels, 4);

	if (!data)
	{
		// File doesn't exist, build it
		if (!Build(c_szFileName))
		{
			sys_err("CGuildMarkImage: cannot create file %s", c_szFileName);
			return false;
		}
		return Load(c_szFileName);
	}

	if (width != WIDTH)
	{
		sys_err("CGuildMarkImage: %s width must be %u (got %d)", c_szFileName, WIDTH, width);
		stbi_image_free(data);
		return false;
	}

	if (height != HEIGHT)
	{
		sys_err("CGuildMarkImage: %s height must be %u (got %d)", c_szFileName, HEIGHT, height);
		stbi_image_free(data);
		return false;
	}

	// Copy and convert RGBA to BGRA
	memcpy(m_apxImage, data, WIDTH * HEIGHT * sizeof(Pixel));
	ConvertRGBAtoBGRA(m_apxImage, WIDTH * HEIGHT);

	stbi_image_free(data);

	m_bValid = true;
	BuildAllBlocks();
	return true;
}

void CGuildMarkImage::PutData(UINT x, UINT y, UINT width, UINT height, void * data)
{
	if (!m_bValid)
		return;

	Pixel* srcPixels = (Pixel*)data;

	for (UINT row = 0; row < height; ++row)
	{
		for (UINT col = 0; col < width; ++col)
		{
			UINT dstX = x + col;
			UINT dstY = y + row;

			if (dstX < WIDTH && dstY < HEIGHT)
			{
				m_apxImage[dstY * WIDTH + dstX] = srcPixels[row * width + col];
			}
		}
	}
}

void CGuildMarkImage::GetData(UINT x, UINT y, UINT width, UINT height, void * data)
{
	if (!m_bValid)
		return;

	Pixel* dstPixels = (Pixel*)data;

	for (UINT row = 0; row < height; ++row)
	{
		for (UINT col = 0; col < width; ++col)
		{
			UINT srcX = x + col;
			UINT srcY = y + row;

			if (srcX < WIDTH && srcY < HEIGHT)
			{
				dstPixels[row * width + col] = m_apxImage[srcY * WIDTH + srcX];
			}
		}
	}
}

// SERVER
bool CGuildMarkImage::SaveMark(DWORD posMark, BYTE * pbImage)
{
	if (posMark >= MARK_TOTAL_COUNT)
	{
		sys_err("CGuildMarkImage::CopyMarkFromData: Invalid mark position %u", posMark);
		return false;
	}

	DWORD colMark = posMark % MARK_COL_COUNT;
	DWORD rowMark = posMark / MARK_COL_COUNT;

	printf("PutMark pos %u %ux%u\n", posMark, colMark * SGuildMark::WIDTH, rowMark * SGuildMark::HEIGHT);
	PutData(colMark * SGuildMark::WIDTH, rowMark * SGuildMark::HEIGHT, SGuildMark::WIDTH, SGuildMark::HEIGHT, pbImage);

	DWORD rowBlock = rowMark / SGuildMarkBlock::MARK_PER_BLOCK_HEIGHT;
	DWORD colBlock = colMark / SGuildMarkBlock::MARK_PER_BLOCK_WIDTH;

	Pixel apxBuf[SGuildMarkBlock::SIZE];
	GetData(colBlock * SGuildMarkBlock::WIDTH, rowBlock * SGuildMarkBlock::HEIGHT, SGuildMarkBlock::WIDTH, SGuildMarkBlock::HEIGHT, apxBuf);
	m_aakBlock[rowBlock][colBlock].Compress(apxBuf);
	return true;
}

bool CGuildMarkImage::DeleteMark(DWORD posMark)
{
	Pixel image[SGuildMark::SIZE];
	memset(&image, 0, sizeof(image));
	return SaveMark(posMark, (BYTE *) &image);
}

// CLIENT
bool CGuildMarkImage::SaveBlockFromCompressedData(DWORD posBlock, const BYTE * pbComp, DWORD dwCompSize)
{
	if (posBlock >= BLOCK_TOTAL_COUNT)
		return false;

	Pixel apxBuf[SGuildMarkBlock::SIZE];
	size_t sizeBuf = sizeof(apxBuf);

	if (LZO_E_OK != lzo1x_decompress_safe(pbComp, dwCompSize, (BYTE *) apxBuf, (lzo_uint*) &sizeBuf, CLZO::Instance().GetWorkMemory()))
	{
		sys_err("CGuildMarkImage::CopyBlockFromCompressedData: cannot decompress, compressed size = %u", dwCompSize);
		return false;
	}

	if (sizeBuf != sizeof(apxBuf))
	{
		sys_err("CGuildMarkImage::CopyBlockFromCompressedData: image corrupted, decompressed size = %u", sizeBuf);
		return false;
	}

	DWORD rowBlock = posBlock / BLOCK_COL_COUNT;
	DWORD colBlock = posBlock % BLOCK_COL_COUNT;

	PutData(colBlock * SGuildMarkBlock::WIDTH, rowBlock * SGuildMarkBlock::HEIGHT, SGuildMarkBlock::WIDTH, SGuildMarkBlock::HEIGHT, apxBuf);

	m_aakBlock[rowBlock][colBlock].CopyFrom(pbComp, dwCompSize, GetCRC32((const char *) apxBuf, sizeof(Pixel) * SGuildMarkBlock::SIZE));
	return true;
}

void CGuildMarkImage::BuildAllBlocks()
{
	Pixel apxBuf[SGuildMarkBlock::SIZE];
	sys_log(0, "CGuildMarkImage::BuildAllBlocks");

	for (UINT row = 0; row < BLOCK_ROW_COUNT; ++row)
		for (UINT col = 0; col < BLOCK_COL_COUNT; ++col)
		{
			GetData(col * SGuildMarkBlock::WIDTH, row * SGuildMarkBlock::HEIGHT, SGuildMarkBlock::WIDTH, SGuildMarkBlock::HEIGHT, apxBuf);
			m_aakBlock[row][col].Compress(apxBuf);
		}
}

DWORD CGuildMarkImage::GetEmptyPosition()
{
	SGuildMark kMark;

	for (DWORD row = 0; row < MARK_ROW_COUNT; ++row)
	{
		for (DWORD col = 0; col < MARK_COL_COUNT; ++col)
		{
			GetData(col * SGuildMark::WIDTH, row * SGuildMark::HEIGHT, SGuildMark::WIDTH, SGuildMark::HEIGHT, kMark.m_apxBuf);

			if (kMark.IsEmpty())
				return (row * MARK_COL_COUNT + col);
		}
	}

	return INVALID_MARK_POSITION;
}

void CGuildMarkImage::GetDiffBlocks(const DWORD * crcList, std::map<BYTE, const SGuildMarkBlock *> & mapDiffBlocks)
{
	BYTE posBlock = 0;

	for (DWORD row = 0; row < BLOCK_ROW_COUNT; ++row)
		for (DWORD col = 0; col < BLOCK_COL_COUNT; ++col)
		{
			if (m_aakBlock[row][col].m_crc != *crcList)
			{
				mapDiffBlocks.insert(std::map<BYTE, const SGuildMarkBlock *>::value_type(posBlock, &m_aakBlock[row][col]));
			}
			++crcList;
			++posBlock;
		}
}

void CGuildMarkImage::GetBlockCRCList(DWORD * crcList)
{
	for (DWORD row = 0; row < BLOCK_ROW_COUNT; ++row)
		for (DWORD col = 0; col < BLOCK_COL_COUNT; ++col)
			*(crcList++) = m_aakBlock[row][col].GetCRC();
}

////////////////////////////////////////////////////////////////////////////////
void SGuildMark::Clear()
{
	for (DWORD iPixel = 0; iPixel < SIZE; ++iPixel)
		m_apxBuf[iPixel] = 0xff000000;
}

bool SGuildMark::IsEmpty()
{
	for (DWORD iPixel = 0; iPixel < SIZE; ++iPixel)
		if (m_apxBuf[iPixel] != 0x00000000)
			return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////
DWORD SGuildMarkBlock::GetCRC() const
{
	return m_crc;
}

void SGuildMarkBlock::CopyFrom(const BYTE * pbCompBuf, DWORD dwCompSize, DWORD crc)
{
	if (dwCompSize > MAX_COMP_SIZE)
		return;

	m_sizeCompBuf = dwCompSize;
	thecore_memcpy(m_abCompBuf, pbCompBuf, dwCompSize);
	m_crc = crc;
}

void SGuildMarkBlock::Compress(const Pixel * pxBuf)
{
	m_sizeCompBuf = MAX_COMP_SIZE;

	if (LZO_E_OK != lzo1x_999_compress((const BYTE *) pxBuf,
		sizeof(Pixel) * SGuildMarkBlock::SIZE, m_abCompBuf,
		(lzo_uint*) &m_sizeCompBuf,
		CLZO::Instance().GetWorkMemory()))
	{
		sys_err("SGuildMarkBlock::Compress: Error! %u > %u", sizeof(Pixel) * SGuildMarkBlock::SIZE, m_sizeCompBuf);
		return;
	}

	m_crc = GetCRC32((const char *) pxBuf, sizeof(Pixel) * SGuildMarkBlock::SIZE);
}
