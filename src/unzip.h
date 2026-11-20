#ifndef PSXDS_UNZIP_H
#define PSXDS_UNZIP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Zip Zip;

Zip* zip_open(const char* path);
void zip_close(Zip* z);
int zip_count(const Zip* z);
const char* zip_name(const Zip* z, int i); /* "dir/" for a folder */
/* Writes entry i to out; false if it can't be read or its CRC is wrong. */
bool zip_extract(Zip* z, int i, FILE* out);

#endif
