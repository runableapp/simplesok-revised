/*
 * gz.c - tiny zlib wrapper for reading in-memory GZ archives.
 *
 * Copyright (C) 2014-2024 Mateusz Viste
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>   /* calloc(), free() */
#include <string.h>   /* memset() */
#include <zlib.h>

#include "gz.h" /* include self for control */


/* tests a memory chunk to see if it contains valid GZ or not. returns 1 if the GZ seems legit. 0 otherwise. */
int isGz(const void *memgz, size_t memgzlen) {
  const unsigned char *memc = memgz;
  if ((memgz == NULL) || (memgzlen < 16) || (memc[0] != 0x1F) || (memc[1] != 0x8B)) return(0);

  /* compression method must be 'store' (0) or 'deflate' (8) */
  if ((memc[2] != 0) && (memc[2] != 8)) return(0);

  /* seems legit */
  return(1);
}


/* decompress a gz file in memory. returns a pointer to a newly allocated memory chunk (holding uncompressed data), or NULL on error. */
void *ungz(const void *memgzsrc, size_t memgzlen, size_t *resultlen) {
  const unsigned char *memgz = memgzsrc;
  int extract_res;
  void *result;
  size_t filelen;
  z_stream zlibstream;

  /* validate arguments */
  if ((resultlen == NULL) || (memgzsrc == NULL) || (memgzlen < 16)) return(NULL);

  *resultlen = 0;

  /* Check the magic bytes of the gz stream before starting anything */
  if (memgz[0] != 0x1F) return(NULL);
  if (memgz[1] != 0x8B) return(NULL);

  /* read the uncompressed file length */
  filelen = memgz[memgzlen - 1];
  filelen <<= 8;
  filelen |= memgz[memgzlen - 2];
  filelen <<= 8;
  filelen |= memgz[memgzlen - 3];
  filelen <<= 8;
  filelen |= memgz[memgzlen - 4];

  /* abort if file appears to be over 1 GB - this is certainly an anomaly */
  if (filelen > 1024*1024*1024) return(NULL);

  /* allocate memory for uncompressed content */
  result = calloc(1, filelen + 1); /* +1 so it is guaranteed to end with a zero byte */
  if (result == NULL) return(NULL);  /* failed to alloc memory for the result */

  memset(&zlibstream, 0, sizeof(zlibstream));
  zlibstream.zalloc = Z_NULL;
  zlibstream.zfree = Z_NULL;
  zlibstream.opaque = Z_NULL;
  zlibstream.avail_in = 0;
  zlibstream.next_in = Z_NULL;
  if (inflateInit2(&zlibstream, 31) != Z_OK) { /* 31 means "this is gzip data" (as opposed to a zlib stream or raw deflate) */
    free(result);
    return(NULL);
  }

  zlibstream.next_in = (Bytef *)memgzsrc; /* ugly cast because the zlib API sadly does not declare input as CONST... it is a known issue caused by retro-compatibility concerns, but still it is safe to assume zlib is NOT changing input buffer in any way:
    "zlib does not touch the input data. It is treated as if it were const. next_in is not const by default since many applications use next_in outside of zlib to read in their data. Making it const would break those applications. - Mark Adler, Jul 7 '17 at 6:52"
    src: https://stackoverflow.com/questions/44958875/c-using-zlib-with-const-data */
  zlibstream.avail_in = (uInt)memgzlen;
  zlibstream.next_out = result;
  zlibstream.avail_out = (uInt)filelen;

  extract_res = inflate(&zlibstream, Z_FINISH); /* Z_FINISH informs zlib that it may use shortcuts because everything is expected to be done within this one single call */
  inflateEnd(&zlibstream);

  if (extract_res != Z_STREAM_END) {
    free(result);
    return(NULL);
  }

  *resultlen = filelen;
  return(result);
}
