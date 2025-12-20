/*
 * This file is part of the 'Simple Sokoban' project.
 *
 * MIT LICENSE
 *
 * Copyright (C) 2014-2025 Mateusz Viste
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <errno.h>
#include <stdio.h>    /* fopen() */
#include <stdlib.h>   /* malloc(), realloc() */
#include <string.h>   /* strcpy(), strcat() */
#include <inttypes.h> /* PRIx64 */
#include <sys/stat.h> /* mkdir() */

#include "compat-sdl.h" /* SDL_GetPrefPath(), SDL_free() */

#include "save.h"

enum solmoves {
  solmove_u = 0,
  solmove_l = 1,
  solmove_d = 2,
  solmove_r = 3,
  solmove_U = 4,
  solmove_L = 5,
  solmove_D = 6,
  solmove_R = 7,
  solmove_ERR = 8
};

static int xsb2byte(char c) {
  switch (c) {
    case 'u':
      return(solmove_u);
    case 'l':
      return(solmove_l);
    case 'd':
      return(solmove_d);
    case 'r':
      return(solmove_r);
    case 'U':
      return(solmove_U);
    case 'L':
      return(solmove_L);
    case 'D':
      return(solmove_D);
    case 'R':
      return(solmove_R);
    default:
      return(solmove_ERR);
  }
}

static char byte2xsb(int b) {
  switch (b) {
    case solmove_u:
      return('u');
    case solmove_l:
      return('l');
    case solmove_d:
      return('d');
    case solmove_r:
      return('r');
    case solmove_U:
      return('U');
    case solmove_L:
      return('L');
    case solmove_D:
      return('D');
    case solmove_R:
      return('R');
    default:
      return('!');
  }
}

#ifdef _WIN32
#define MKDIR(d) mkdir(d)
#else
#define MKDIR(d) mkdir(d, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
#endif

static void getfname(char *result, size_t ressz, const char *fname) {
  char *prefpath;
  if (ressz > 0) result[0] = 0;
  prefpath = SDL_GetPrefPath("", "simplesok");
  if (prefpath == NULL) return;
  MKDIR(prefpath);
  if (strlen(prefpath) + strlen(fname) + 1 > ressz) return;
  strcat(result, prefpath);
  strcat(result, fname);
  SDL_free(prefpath);
}


/* fills *savedir with the directory path where simplesok is supposed to keep its save files */
static void getsavedir(char *savedir, size_t maxlen) {
  getfname(savedir, maxlen, "solved/");
  if (savedir[0] != 0) MKDIR(savedir);
}


/* same as getsavedir(), but looks into the old directory (as used by v1.0
 * and v1.0.1) */
static void getsavedir_legacy(char *savedir, int maxlen) {
  char *prefpath;
  if (maxlen > 0) savedir[0] = 0;
  maxlen -= 16; /* to be sure we will always have enough room for the app's suffix and extra NULL terminator */
  if (maxlen < 1) return;
  prefpath = SDL_GetPrefPath("Mateusz Viste", "Simple Sokoban");
  if (prefpath == NULL) return;
  if (strlen(prefpath) > (unsigned) maxlen) return;
  strcpy(savedir, prefpath);
  SDL_free(prefpath);
}


const char *loadconf_skin(void) {
  static char rootdir[512];
  FILE *fd;
  size_t len, i;
  getfname(rootdir, sizeof(rootdir), "skin.cfg");
  if (rootdir[0] == 0) return(NULL);

  fd = fopen(rootdir, "rb");
  if (fd == NULL) return(NULL);

  len = fread(rootdir, 1, sizeof(rootdir) - 2, fd);
  fclose(fd);

  if (len < 1) return(NULL);

  /* trim string at nearest \r or \n */
  for (i = 0; i < len; i++) {
    if ((rootdir[i] == '\r') || (rootdir[i] == '\n')) break;
  }
  rootdir[i] = 0;

  return(rootdir);
}


void setconf_skin(const char *skin) {
  char rootdir[512];
  FILE *fd;

  getfname(rootdir, sizeof(rootdir), "skin.cfg");
  if (rootdir[0] == 0) return;

  fd = fopen(rootdir, "wb");
  if (fd == NULL) {
    fprintf(stderr, "failed to open config file '%s' (%s)\n", rootdir, strerror(errno));
    return;
  }

  fwrite(skin, 1, strlen(skin), fd);
  fclose(fd);
}


/* returns a malloc()'ed, null-terminated string with the solution to level levcrc32. if no solution available, returns NULL. */
char *solution_load(uint64_t levcrc64, char *ext) {
  char rootdir[4096], crcstr[32], *solution, *solutionfinal;
  int bytebuff, rlecounter;
  size_t solutionpos = 0, solution_alloc = 16;
  FILE *fd = NULL;
  unsigned short i;

  /* try loading the solution from two sources: first from current preferred
   * directory, then from the legacy (1.0 and 1.0.1) directory */
  for (i = 0; i < 2; i++) {
    if (i == 0) {
      getsavedir(rootdir, sizeof(rootdir));
    } else {
      getsavedir_legacy(rootdir, sizeof(rootdir));
    }
    if (rootdir[0] == 0) continue;
    if ((ext[0] | 0x20) == 'd') { /* legacy *.DAT loading (pre-1.0.7) */
      sprintf(crcstr, "%08lX.%s", (unsigned long)levcrc64, ext);
    } else {
      sprintf(crcstr, "%016" PRIx64 ".%s", levcrc64, ext);
    }
    strcat(rootdir, crcstr);
    fd = fopen(rootdir, "rb");
    if (fd != NULL) break;
  }
  if (fd == NULL) return(NULL);

  solution = malloc(solution_alloc);
  if (solution == NULL) return(NULL);
  solution[0] = 0;

  for (;;) {
    bytebuff = getc(fd);
    if (bytebuff < 0) break; /* if end of file -> stop here */
    rlecounter = bytebuff >> 4; /* fetch the RLE counter */
    bytebuff &= 15; /* strip the rle counter, so we have only the actual value */
    while (rlecounter > 0) {
      /* check first if we are in need of reallocation */
      if (solutionpos + 1 >= solution_alloc) {
        solution_alloc *= 2;
        solution = realloc(solution, solution_alloc);
        if (solution == NULL) {
          printf("realloc() failed for %lu bytes: %s\n", (unsigned long)solution_alloc, strerror(errno));
          solution = NULL;
          break;
        }
      }
      /* add one position to the solution and decrement the rle counter */
      solution[solutionpos] = byte2xsb(bytebuff);
      if (solution[solutionpos] == '!') { /* if corrupted solution, free it and return nothing */
        free(solution);
        solution = NULL;
        break;
      }
      solutionpos += 1;
      solution[solutionpos] = 0;
      rlecounter -= 1;
    }
    if (solution == NULL) break; /* break out on error */
  }

  /* close the file descriptor */
  fclose(fd);

  /* strdup() the solution to make sure we use the least possible amount of memory */
  solutionfinal = NULL;
  if (solution != NULL) {
    solutionfinal = strdup(solution);
    free(solution);
  }
  /* return the solution string */
  return(solutionfinal);
}

/* saves the solution for levcrc64 */
void solution_save(uint64_t levcrc64, char *solution, char *ext) {
  char rootdir[4096], crcstr[32];
  int curbyte, lastbyte = -1, lastbytecount = 0;
  FILE *fd;
  getsavedir(rootdir, sizeof(rootdir));
  if ((rootdir[0] == 0) || (solution == NULL)) return;
  sprintf(crcstr, "%016" PRIx64 ".%s", levcrc64, ext);
  strcat(rootdir, crcstr);
  fd = fopen(rootdir, "wb");
  if (fd == NULL) return;
  for (;;) {
    curbyte = xsb2byte(*solution);
    if ((curbyte == lastbyte) && (lastbytecount < 15)) {
        lastbytecount += 1; /* same pattern -> increment the RLE counter */
      } else {
        int bytebuff;
        /* dump the lastbyte chunk */
        if (lastbytecount > 0) {
          bytebuff = lastbytecount;
          bytebuff <<= 4;
          bytebuff |= lastbyte;
          fputc(bytebuff, fd);
        }
        /* save the new RLE counter */
        lastbyte = curbyte;
        lastbytecount = 1;
    }
    if (curbyte == solmove_ERR) break;
    solution += 1;
  }
  fclose(fd);
  return;
}
