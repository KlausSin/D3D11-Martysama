#include "StdAfx.h"
#include "JpegFile.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include <stb_image.h>
#include <stb_image_write.h>

int jpeg_save(unsigned char*data, int width, int height, int quality, const char*filename)
{
	if (!filename || !data)
		return 0;

	// stb_image_write expects RGB data with 3 components
	return stbi_write_jpg(filename, width, height, 3, data, quality);
}

int jpeg_save_to_file(unsigned char*data, int width, int height, int quality, FILE*fi)
{
	if (!fi || !data)
		return 0;

	auto write_func = [](void* context, void* data, int size) {
		fwrite(data, 1, size, (FILE*)context);
	};

	return stbi_write_jpg_to_func(write_func, fi, width, height, 3, data, quality);
}

// Callback context for memory writing
struct MemWriteContext {
	unsigned char* dest;
	int destlen;
	int written;
};

static void mem_write_func(void* context, void* data, int size)
{
	MemWriteContext* ctx = (MemWriteContext*)context;
	if (ctx->written + size <= ctx->destlen) {
		memcpy(ctx->dest + ctx->written, data, size);
		ctx->written += size;
	}
}

int jpeg_save_to_mem(unsigned char*data, int width, int height, int quality, unsigned char*dest, int destlen)
{
	if (!data || !dest || destlen <= 0)
		return 0;

	MemWriteContext ctx;
	ctx.dest = dest;
	ctx.destlen = destlen;
	ctx.written = 0;

	stbi_write_jpg_to_func(mem_write_func, &ctx, width, height, 3, data, quality);

	return ctx.written;
}

int jpeg_load_from_mem(unsigned char*_data, int _size, unsigned char*dest, int width, int height)
{
	if (!_data || !dest)
		return 0;

	int w, h, channels;
	unsigned char* loaded = stbi_load_from_memory(_data, _size, &w, &h, &channels, 3);

	if (!loaded)
		return 0;

	int copySize = width * height * 3;
	if (w * h * 3 < copySize)
		copySize = w * h * 3;

	memcpy(dest, loaded, copySize);
	stbi_image_free(loaded);

	return 1;
}

typedef struct _RGBA {
	unsigned char a,r,g,b;
} RGBA;

int jpeg_load(const char*filename, unsigned char**dest, int*_width, int*_height)
{
	if (!filename || !dest || !_width || !_height)
		return 0;

	int width, height, channels;
	unsigned char* loaded = stbi_load(filename, &width, &height, &channels, 4); // Load as RGBA

	if (!loaded) {
		fprintf(stderr, "Couldn't open file %s\n", filename);
		return 0;
	}

	*_width = width;
	*_height = height;
	*dest = (unsigned char*)malloc(width * height * 4);

	if (!*dest) {
		stbi_image_free(loaded);
		return 0;
	}

	RGBA* destRGBA = (RGBA*)(*dest);
	for (int i = 0; i < width * height; i++) {
		destRGBA[i].a = loaded[i * 4 + 3];
		destRGBA[i].r = loaded[i * 4 + 0];
		destRGBA[i].g = loaded[i * 4 + 1];
		destRGBA[i].b = loaded[i * 4 + 2];
	}

	stbi_image_free(loaded);
	return 1;
}
