
#include "mimalloc-new-delete.h"
#include <mimalloc.h>

#include <cstddef>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

extern "C" {

// ---------- Standard C ----------
void*  malloc(size_t size)                         { return mi_malloc(size); }
void   free(void* p)                               { mi_free(p); }
void*  calloc(size_t count, size_t size)           { return mi_calloc(count, size); }
void*  realloc(void* p, size_t size)               { return mi_realloc(p, size); }

// ---------- MSVC public extensions ----------
void*  _recalloc(void* p, size_t count, size_t size) { return mi_recalloc(p, count, size); }
void*  _expand(void* p, size_t size)                 { return mi_expand(p, size); }
size_t _msize(void* p)                               { return mi_usable_size(p); }

// ---------- MSVC aligned ----------
void*  _aligned_malloc(size_t size, size_t alignment)
	{ return mi_malloc_aligned(size, alignment); }
void   _aligned_free(void* p)
	{ mi_free(p); }
void*  _aligned_realloc(void* p, size_t size, size_t alignment)
	{ return mi_realloc_aligned(p, size, alignment); }
void*  _aligned_recalloc(void* p, size_t count, size_t size, size_t alignment)
	{ return mi_recalloc_aligned(p, count, size, alignment); }
size_t _aligned_msize(void* p, size_t /*alignment*/, size_t /*offset*/)
	{ return mi_usable_size(p); }

void*  _aligned_offset_malloc(size_t size, size_t alignment, size_t offset)
	{ return mi_malloc_aligned_at(size, alignment, offset); }
void*  _aligned_offset_realloc(void* p, size_t size, size_t alignment, size_t offset)
	{ return mi_realloc_aligned_at(p, size, alignment, offset); }
void*  _aligned_offset_recalloc(void* p, size_t count, size_t size, size_t alignment, size_t offset)
	{ return mi_recalloc_aligned_at(p, count, size, alignment, offset); }

void*  _malloc_base(size_t size)                          { return mi_malloc(size); }
void   _free_base(void* p)                                { mi_free(p); }
void*  _calloc_base(size_t count, size_t size)            { return mi_calloc(count, size); }
void*  _realloc_base(void* p, size_t size)                { return mi_realloc(p, size); }
void*  _recalloc_base(void* p, size_t count, size_t size) { return mi_recalloc(p, count, size); }
void*  _expand_base(void* p, size_t size)                 { return mi_expand(p, size); }
size_t _msize_base(void* p) noexcept                      { return mi_usable_size(p); }

} // extern "C"
