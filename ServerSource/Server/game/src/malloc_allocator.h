#ifndef _MALLOC_ALLOCATOR_H_
#define _MALLOC_ALLOCATOR_H_

// Allocator implementation detail based on default CRT malloc/free.
class MallocAllocator {
public:
	MallocAllocator() {}
	~MallocAllocator() {}

	void SetUp() {}
	void TearDown() {}

	void* Alloc(size_t size) {
		return ::malloc(size);
	}
	void Free(void* p) {
		::free(p);
	}
};

#endif // _MALLOC_ALLOCATOR_H_
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
