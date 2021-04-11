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
	uint8_t *compressedBuffer = (uint8_t *)strstr((char *)rawCompressed, "bvx2");
	if (!compressedBuffer)
		return NULL;

	*filesize -= (uintptr_t)compressedBuffer - (uintptr_t)rawCompressed;

	verbose("Header found at %p. %llu further than it's parent at %p\n", compressedBuffer, (uintptr_t)compressedBuffer - (uintptr_t)rawCompressed, rawCompressed);

	uint8_t *decompressedBuffer = malloc(4 * *filesize);
	if (!decompressedBuffer)
		return NULL;

	size_t result = lzfse_decode_buffer(decompressedBuffer, 4 * *filesize, compressedBuffer, *filesize, NULL);

	verbose("Decompressed %llu bytes inside a %llu byte buffer. Shrinking...\n", result, *filesize);

	*filesize = (int32_t)result;
	decompressedBuffer = (uint8_t *)realloc(decompressedBuffer, *filesize);

	verbose("Dumping decompressed kernelcache to `kernelcache.bin`n");
	FILE *fp = fopen("kernelcache.bin", "w");
	if (fp) {
		fwrite(decompressedBuffer, 1, *filesize, fp);
		fclose(fp);
	} else
		verbose("Couldn't create file. Check permissions\n");

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
	void *tryBVX_orig = (void *)0x407260;

	hook(tryBVX_orig, tryBVX_hooked);
}
