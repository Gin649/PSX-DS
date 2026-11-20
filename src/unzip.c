/* A small zip reader (zlib): the entries of a zip file, stored or deflated.
 * For the Linux updater, which unpacks release zips. */
#include "unzip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

typedef struct {
	char name[512];
	uint32_t offset; /* local header */
	uint32_t csize, usize;
	uint16_t method;
	uint32_t crc;
} Entry;

struct Zip {
	FILE* f;
	Entry* e;
	int n;
};

static uint16_t u16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t u32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

Zip* zip_open(const char* path) {
	FILE* f = fopen(path, "rb");
	if (!f) return NULL;
	/* the end of central directory record, in the last 64 KB + 22 */
	if (fseek(f, 0, SEEK_END) != 0) goto fail;
	long size = ftell(f);
	long back = size < 65557 ? size : 65557;
	uint8_t* tail = malloc((size_t)back);
	if (!tail || fseek(f, size - back, SEEK_SET) != 0 || fread(tail, 1, (size_t)back, f) != (size_t)back) {
		free(tail);
		goto fail;
	}
	long at = -1;
	for (long i = back - 22; i >= 0; i--)
		if (u32(tail + i) == 0x06054b50) { at = i; break; }
	if (at < 0) { free(tail); goto fail; }
	int count = u16(tail + at + 10);
	uint32_t cdSize = u32(tail + at + 12), cdOff = u32(tail + at + 16);
	free(tail);

	uint8_t* cd = malloc(cdSize);
	if (!cd || fseek(f, cdOff, SEEK_SET) != 0 || fread(cd, 1, cdSize, f) != cdSize) {
		free(cd);
		goto fail;
	}
	Zip* z = calloc(1, sizeof *z);
	z->f = f;
	z->e = calloc((size_t)(count ? count : 1), sizeof(Entry));
	uint32_t p = 0;
	for (int i = 0; i < count && p + 46 <= cdSize; i++) {
		const uint8_t* h = cd + p;
		if (u32(h) != 0x02014b50) break;
		Entry* e = &z->e[z->n];
		e->method = u16(h + 10);
		e->crc = u32(h + 16);
		e->csize = u32(h + 20);
		e->usize = u32(h + 24);
		uint16_t nameLen = u16(h + 28), extraLen = u16(h + 30), commentLen = u16(h + 32);
		e->offset = u32(h + 42);
		if (p + 46 + nameLen > cdSize) break;
		size_t n = nameLen < sizeof e->name - 1 ? nameLen : sizeof e->name - 1;
		memcpy(e->name, h + 46, n);
		e->name[n] = 0;
		z->n++;
		p += 46 + nameLen + extraLen + commentLen;
	}
	free(cd);
	return z;
fail:
	fclose(f);
	return NULL;
}

void zip_close(Zip* z) {
	if (!z) return;
	fclose(z->f);
	free(z->e);
	free(z);
}

int zip_count(const Zip* z) { return z->n; }
const char* zip_name(const Zip* z, int i) { return z->e[i].name; }

bool zip_extract(Zip* z, int i, FILE* out) {
	Entry* e = &z->e[i];
	uint8_t lh[30];
	if (fseek(z->f, e->offset, SEEK_SET) != 0 || fread(lh, 1, 30, z->f) != 30 || u32(lh) != 0x04034b50) return false;
	if (fseek(z->f, u16(lh + 26) + u16(lh + 28), SEEK_CUR) != 0) return false;
	uint8_t in[65536], buf[65536];
	uint32_t left = e->csize, crc = 0;
	if (e->method == 0) {
		while (left) {
			size_t n = left < sizeof in ? left : sizeof in;
			if (fread(in, 1, n, z->f) != n || fwrite(in, 1, n, out) != n) return false;
			crc = (uint32_t)crc32(crc, in, (uInt)n);
			left -= (uint32_t)n;
		}
		return crc == e->crc;
	}
	if (e->method != 8) return false;
	z_stream s;
	memset(&s, 0, sizeof s);
	if (inflateInit2(&s, -MAX_WBITS) != Z_OK) return false;
	int r = Z_OK;
	while (r != Z_STREAM_END) {
		if (s.avail_in == 0 && left) {
			size_t n = left < sizeof in ? left : sizeof in;
			if (fread(in, 1, n, z->f) != n) break;
			left -= (uint32_t)n;
			s.next_in = in;
			s.avail_in = (uInt)n;
		}
		s.next_out = buf;
		s.avail_out = sizeof buf;
		r = inflate(&s, Z_NO_FLUSH);
		if (r != Z_OK && r != Z_STREAM_END) break;
		size_t got = sizeof buf - s.avail_out;
		if (got && fwrite(buf, 1, got, out) != got) { r = Z_ERRNO; break; }
		crc = (uint32_t)crc32(crc, buf, (uInt)got);
		if (r == Z_OK && got == 0 && s.avail_in == 0 && !left) break;
	}
	inflateEnd(&s);
	return r == Z_STREAM_END && crc == e->crc;
}
