#define _GNU_SOURCE

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <lzfse.h>

#define VERBOSE
#define JOKER_VERSION 2

static inline int verbose(const char *format, ...) {
	#ifdef VERBOSE
	va_list args;
	va_start(args, format);
	int result = vfprintf(stderr, format, args);
	va_end(args);
	#else
	int result = 0;
	#endif

	return result;
}

uint8_t *tryBVX_hooked(uint8_t *rawCompressed, int32_t *filesize) {
	uint8_t *compressedBuffer = (uint8_t *)memmem(rawCompressed, *filesize, "bvx2", 4);
	if (!compressedBuffer)
		return NULL;

	size_t compressedSize = *filesize - ((uintptr_t)compressedBuffer - (uintptr_t)rawCompressed);

	verbose("Header found at %p. %llu further than it's parent at %p\n", compressedBuffer, (uintptr_t)compressedBuffer - (uintptr_t)rawCompressed, rawCompressed);

	size_t decompressedSize = 4 * compressedSize; // 4 times bigger because the format gurantees the max compression is 4 times smaller
	uint8_t *decompressedBuffer = malloc(decompressedSize);
	if (!decompressedBuffer) {
		verbose("Error while allocating %llu byte buffer\n", decompressedSize);
		return NULL;
	}

	decompressedSize = lzfse_decode_buffer(decompressedBuffer, decompressedSize, compressedBuffer, compressedSize, NULL);

	verbose("Decompressed %llu bytes inside a %llu byte buffer. Shrinking...\n", decompressedSize, 4 * compressedSize);

	decompressedBuffer = (uint8_t *)realloc(decompressedBuffer, decompressedSize);
	if (!decompressedBuffer) {
		verbose("Error while shrinking buffer\n");
		return NULL;
	}

	*filesize = (int32_t)decompressedSize; // Yuck

	return decompressedBuffer;
}

uint8_t hook(void *orig, void *hook) {
	verbose("[Hook] Detouring %p for %p\n", orig, hook);

	uintptr_t pageSize = sysconf(_SC_PAGESIZE);
	uintptr_t pagedOrigAddress = (uintptr_t)orig - ((uintptr_t)orig % pageSize);

	verbose("[Hook] Changing permissions of memory at %p (Paged: %p)\n", orig, (void *)pagedOrigAddress);
	if (mprotect((void *)pagedOrigAddress, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
		perror("mprotect");
		return 1;
	}

	uint8_t jmpPatch[] = {
		0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// mov rax, 0x0
		0xff, 0xe0													// jmp rax
	};

	*(uintptr_t *)((uintptr_t)&jmpPatch + 2) = (uintptr_t)hook;

	memcpy(orig, &jmpPatch, sizeof(jmpPatch));

	return 0;
}


__attribute__((constructor))
static void hook_ctor() {
	#if JOKER_VERSION == 1
	void *tryBVX_orig = (void *)0x407260;
	#elif JOKER_VERSION == 2
	void *tryBVX_orig = (void *)0x422c6f;
	#else
	#error "JOKER_VERSION must be either 1 or 2"
	#endif

	hook(tryBVX_orig, tryBVX_hooked);
}
