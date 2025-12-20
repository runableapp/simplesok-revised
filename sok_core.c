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

#include <inttypes.h>   /* PRIx64 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "crc32.h"
#include "crc64.h"
#include "gz.h"
#include "save.h"
#include "sok_core.h"

#include "dbg.h"


enum errorslist {
  ERR_UNDEFINED = -1,
  ERR_LEVEL_TOO_HIGH = -2,
  ERR_LEVEL_TOO_LARGE = -3,
  ERR_LEVEL_TOO_SMALL = -4,
  ERR_MEM_ALLOC_FAILED = -5,
  ERR_NO_LEVEL_DATA_FOUND = -6,
  ERR_TOO_MANY_LEVELS_IN_SET = -7,
  ERR_UNABLE_TO_OPEN_FILE = -8,
  ERR_PLAYER_POS_UNDEFINED = -9
};

char *sok_strerr(int errid) {
  switch ((enum errorslist) errid) {
    case ERR_LEVEL_TOO_HIGH: return("Level height too high");
    case ERR_LEVEL_TOO_LARGE: return("Level width too large");
    case ERR_LEVEL_TOO_SMALL: return("Level dimensions too small");
    case ERR_MEM_ALLOC_FAILED: return("Memory allocation failed - out of memory?");
    case ERR_NO_LEVEL_DATA_FOUND: return("No level data found in file");
    case ERR_TOO_MANY_LEVELS_IN_SET: return("Too many levels in set");
    case ERR_UNABLE_TO_OPEN_FILE: return("Failed to open file");
    case ERR_UNDEFINED: return("Undefined error");
    case ERR_PLAYER_POS_UNDEFINED: return("Player position not defined");
  }
  return("Unknown error");
}

static void trim(char *comment) {
  int x, firstrealchar = -1, lastrealchar = 0;
  if (comment == NULL) return;
  for (x = 0; comment[x] != 0; x++) {
    if ((firstrealchar < 0) && (comment[x] != ' ')) firstrealchar = x;
    if (comment[x] != ' ') lastrealchar = x;
  }
  /* RTRIM */
  comment[lastrealchar + 1] = 0;
  /* LTRIM */
  if (firstrealchar > 0) {
    for (x = 0; x < 1 + lastrealchar - firstrealchar; x++) comment[x] = comment[x + firstrealchar];
    comment[1 + lastrealchar - firstrealchar] = 0;
  }
}

size_t sok_history_getlen(const char *history) {
  size_t res = 0;
  if (history == NULL) return(0);
  while (*history != 0) {
    res += 1;
    history += 1;
  }
  return(res);
}

size_t sok_history_getpushes(const char *history) {
  size_t res = 0;
  if (history == NULL) return(0);
  while (*history != 0) {
    if ((*history >= 'A') && (*history <= 'Z')) res += 1;
    history += 1;
  }
  return(res);
}

static struct sokgame *sok_allocgame(void) {
  struct sokgame *result;
  result = malloc(sizeof(struct sokgame));
  return(result);
}

static void sok_freegame(struct sokgame *game) {
  if (game == NULL) return;
  if (game->solution != NULL) free(game->solution);
  free(game);
}

/* free a list of allocated games */
void sok_freefile(struct sokgame **gamelist, int gamescount) {
  int x;
  for (x = 0; x < gamescount; x++) {
    sok_freegame(gamelist[x]);
  }
}

/* reads a byte from memory of from a file, whichever is passed as a parameter */
static int readbytefrommem(unsigned char **memptr) {
  int result = -1;
  if (memptr != NULL) {
    result = **memptr;
    *memptr += 1;
    if (result == 0) result = -1;
  }
  return(result);
}

/* reads a single RLE chunk from file fd, fills bytebuff with the actual data byte and returns the amount of times it should be repeated. returns -1 on error (like end of file). */
static int readRLEbyte(unsigned char **memptr, int *bytebuff) {
  int rleprefix = -1;
  for (;;) { /* RLE support */
      *bytebuff = readbytefrommem(memptr);
      if (*bytebuff < 0) return(-1);
      if ((*bytebuff >= '0') && (*bytebuff <= '9')) {
        if (rleprefix > 0) {
            rleprefix *= 10;
          } else {
            rleprefix = 0;
        }
        rleprefix += (*bytebuff - '0');
      } else { /* not a RLE prefix */
        break;
    }
  } /* RLE parsing done */
  if (rleprefix < 0) rleprefix = 1;
  return(rleprefix);
}

/* floodfill algorithm to fill areas of a playfield that are not contained in walls */
static void floodFillField(struct sokgame *game, int x, int y) {
  if ((x >= 0) && (x < 64) && (y >= 0) && (y < 64) && (game->field[x][y] == field_floor)) {
    game->field[x][y] = 0; /* set the 'pixel' before starting recursion */
    floodFillField(game, x + 1, y);
    floodFillField(game, x - 1, y);
    floodFillField(game, x, y + 1);
    floodFillField(game, x, y - 1);
  }
}


/* loads the next level from open file fd. returns 0 on success, 1 on success with end of file reached, or -1 on error. */
static int loadlevelfromfile(struct sokgame *game, unsigned char **memptr, char *precomment, size_t precommentsz, char *postcomment, size_t postcommentsz) {
  int leveldatastarted = 0, endoffile = 0;
  unsigned short x, y;
  int bytebuff;
  char commentbuf[128];
  size_t commentbuflen = 0;
  game->positionx = -1;
  game->positiony = -1;
  game->field_width = 0;
  game->field_height = 0;
  game->solution = NULL;
  if ((precomment != NULL) && (precommentsz > 0)) *precomment = 0;
  if ((postcomment != NULL) && (postcommentsz > 0)) *postcomment = 0;

  /* Fill the area with floor */
  for (y = 0; y < 64; y++) {
    for (x = 0; x < 64; x++) {
      game->field[x][y] = field_floor;
    }
  }

  x = 0;
  y = 0;

  for (;;) {
    int rleprefix;
    rleprefix = readRLEbyte(memptr, &bytebuff);
    if (rleprefix < 0) endoffile = 1;
    if (endoffile != 0) break;
    for (; rleprefix > 0; rleprefix--) {
      switch (bytebuff) {
        case ' ': /* empty space */
        case '-': /* dash (-) and underscore (_) are sometimes used to denote empty spaces */
        case '_':
          game->field[x + 1][y + 1] |= field_floor;
          x += 1;
          break;
        case '#': /* wall */
          game->field[x + 1][y + 1] |= field_wall;
          x += 1;
          break;
        case '@': /* player */
          game->field[x + 1][y + 1] |= field_floor;
          game->positionx = x;
          game->positiony = y;
          x += 1;
          break;
        case '*': /* atom on goal */
          game->field[x + 1][y + 1] |= field_goal;
          /* FALLTHRU */
        case '$': /* atom */
          game->field[x + 1][y + 1] |= field_atom;
          x += 1;
          break;
        case '+': /* player on goal */
          game->positionx = x;
          game->positiony = y;
          /* FALLTHRU */
        case '.': /* goal */
          game->field[x + 1][y + 1] |= field_goal;
          x += 1;
          break;
        case '\n': /* next row */
        case '|':  /* some variants of the xsb format use | as the 'new row' separator (mostly when used with RLE) */
          if (leveldatastarted != 0) y += 1;
          x = 0;
          break;
        case '\r': /* CR - ignore those */
          break;
        default: /* anything else is a comment -> skip until end of line or end of file */
          /* read the comment into buf */
          commentbuflen = 0;
          for (;;) {
            bytebuff = readbytefrommem(memptr);
            if (bytebuff == '\r') continue;
            if (bytebuff == '\n') break;
            if (bytebuff < 0) {
              endoffile = 1;
              break;
            }
            if (commentbuflen < sizeof(commentbuf) - 1) {
              commentbuf[commentbuflen++] = (char)bytebuff;
              commentbuf[commentbuflen] = 0;
            }
          }

          trim(commentbuf);

          /* copy the comment to pre or post comment */
          if (leveldatastarted) {
            leveldatastarted = -1;
            if ((postcomment) && (postcomment[0] == 0)) {
              snprintf(postcomment, postcommentsz, "%s", commentbuf);
            }
          } else {
            if ((precomment) && (precomment[0] == 0)) {
              snprintf(precomment, precommentsz, "%s", commentbuf);
            }
          }
          break;
      }
      if ((leveldatastarted < 0) || (endoffile != 0)) break;
      if (x > 0) leveldatastarted = 1;
      if (x >= 62) return(ERR_LEVEL_TOO_LARGE);
      if (y >= 62) return(ERR_LEVEL_TOO_HIGH);
      if (x > game->field_width) game->field_width = x;
      if ((y >= game->field_height) && (x > 0)) game->field_height = y + 1;
    }
    if ((leveldatastarted < 0) || (endoffile != 0)) break;
  }

  /* check if the loaded game looks sane */
  if (game->positionx < 0) return(ERR_PLAYER_POS_UNDEFINED);
  if (game->field_height < 1) return(ERR_LEVEL_TOO_SMALL);
  if (game->field_width < 1) return(ERR_LEVEL_TOO_SMALL);
  if (leveldatastarted == 0) return(ERR_NO_LEVEL_DATA_FOUND);

  /* remove floors around the level */
  floodFillField(game, 63, 63);

  /* move the field by -1 vertically and horizontally to remove the additional row and column added for the fill function to be able to get around the field. */
  for (y = 0; y < 63; y++) {
    for (x = 0; x < 63; x++) {
      game->field[x][y] = game->field[x + 1][y + 1];
    }
  }
  /* compute the CRC32 of the field as it was done in v1.0.6 and earlier. This
   * is buggy since it only looks at a part of the field due to the inversion
   * of x and y axis. Also, it does not take into account the initial position
   * of the player. This buggy CRC32 is only used as a fallback to look for
   * solutions written by earlier versions of the game. */
  game->crc32_106 = crc32_init();
  for (y = 0; y < game->field_width; y++) {
    for (x = 0; x < game->field_height; x++) {
      crc32_feed(&(game->crc32_106), &(game->field[x][y]), 1);
    }
  }
  crc32_finish(&(game->crc32_106));

  /* compute the CRC64 of the playfield */
  game->crc64 = 0;
  { /* do not forget to include the player's initial position in the CRC */
    unsigned char playerpos[2];
    playerpos[0] = (unsigned char)(game->positionx);
    playerpos[1] = (unsigned char)(game->positiony);
    game->crc64 = crc64(game->crc64, playerpos, 2);
  }

  if (debugmode) puts("---");
  for (y = 0; y < game->field_height; y++) {
    for (x = 0; x < game->field_width; x++) {
#if debugmode != 0
      switch (game->field[x][y]) {
        case 0:
          printf(" ");
          break;
        case 1:
          printf(".");
          break;
        case field_wall:
        case (field_wall | field_floor):
          printf("X");
          break;
        default:
          printf("%c", '0' + game->field[x][y]);
      }
#endif
      game->crc64 = crc64(game->crc64, &(game->field[x][y]), 1);
    }
    if (debugmode) puts("");
  }
  if (debugmode) printf("CRC64 = %016" PRIx64 " (buggy pre-1.0.7 CRC32 = %08lX)\n", game->crc64, game->crc32_106);

  if (endoffile != 0) return(1);
  return(0);
}

/* loads a file to memory. returns the file size on success, 0 otherwise. */
static size_t loadfile2mem(char *file, unsigned char **memptr) {
  size_t filesize;
  long ftelloff;
  FILE *fd = NULL;
  *memptr = NULL;
  fd = fopen(file, "rb");
  if (fd == NULL) goto ERR;
  /* get file size */
  fseek(fd, 0, SEEK_END);
  ftelloff = ftell(fd);
  if (ftelloff <= 0) goto ERR;
  filesize = (size_t)ftelloff;
  if (filesize > 1024 * 1024 * 1024) goto ERR; /* don't even try loading huge files */
  rewind(fd);
  /* allocate mem */
  *memptr = calloc(1, filesize + 1); /* +1 because I always want to have a NULL terminator at the end */
  if (*memptr == NULL) goto ERR;
  /* load file to mem */
  if (fread(*memptr, 1, filesize, fd) != filesize) goto ERR;
  fclose(fd);
  return(filesize);

  ERR:
  if (fd != NULL) fclose(fd);
  if (*memptr != NULL) {
    free(*memptr);
    *memptr = NULL;
  }
  return(0);
}

/* load levels from a file, and put them into an array of up to maxlevels levels */
int sok_loadfile(struct sokgame **gamelist, int maxlevels, char *gamelevel, unsigned char *memptr, size_t filelen, char *comment, int maxcommentlen) {
  int errflag = 0;
  unsigned short level;
  unsigned char *allocptr = NULL;
  struct sokgame *game = NULL;
  if (gamelevel != NULL) {
    filelen = loadfile2mem(gamelevel, &allocptr);
    memptr = allocptr;
  }
  if ((filelen == 0) || (memptr == NULL)) return(ERR_UNABLE_TO_OPEN_FILE);

  /* if the level is gziped, uncompress it now */
  if (isGz(memptr, filelen)) {
    char unsigned *ungzptr;
    size_t uncompressedlen;
    ungzptr = ungz(memptr, filelen, &uncompressedlen);
    filelen = uncompressedlen;
    if (allocptr != NULL) free(allocptr);
    allocptr = ungzptr;
    memptr = ungzptr;
    if (ungzptr == NULL) return(ERR_UNABLE_TO_OPEN_FILE);
  }

  for (level = 0; !errflag; level++) { /* iterate to load games sequentially from the file */
    if (debugmode) puts("loading level..");
    game = sok_allocgame();
    if(!game) {
      errflag = ERR_MEM_ALLOC_FAILED;
      break;
    }

    /* call loadlevelfromfile */
    errflag = loadlevelfromfile(game, &memptr, (level == 0) ? comment : NULL, maxcommentlen, game->comment, sizeof(game->comment));
    if (errflag < 0) {
      if (level) errflag = 0;
      break;
    }
    if (level >= maxlevels) {
      errflag = ERR_TOO_MANY_LEVELS_IN_SET;
      break;
    }

    /* write the level num and load the solution (if any) */
    game->level = level + 1;
    game->solution = solution_load(game->crc64, "sol");
    gamelist[level] = game;
    game = NULL;
  }

  sok_freegame(game);
  if (allocptr != NULL) free(allocptr);

  if (errflag < 0) {
    sok_freefile(gamelist, level);
    return(errflag);
  }

  return(level);
}

/* reloads solutions for all levels in a list */
void sok_loadsolutions(struct sokgame **gamelist, int levelscount) {
  int x = 0;
  for (x = 0; x < levelscount; x++) {
    gamelist[x]->solution = solution_load(gamelist[x]->crc64, "sol");
    /* no solution found: look for a solution under the pre-1.0.7 CRC32 */
    if (gamelist[x]->solution == NULL) {
      gamelist[x]->solution = solution_load(gamelist[x]->crc32_106, "dat");
    }
  }
}

/* checks if level is solved yet. returns 0 if not, non-zero otherwise. */
int sok_checksolution(struct sokgame *game, struct sokgamestates *states) {
  unsigned short x, y;
  size_t bestscorelen, bestscorepushes, myscorelen, myscorepushes, betterflag = 0;
  for (y = 0; y < game->field_height; y++) {
    for (x = 0; x < game->field_width; x++) {
      if (((game->field[x][y] & field_goal) != 0) && ((game->field[x][y] & field_atom) == 0)) return(0);
    }
  }

  /* no non-filled goal found = level completed! (but only if at least 1 push was made) */
  if (states == NULL) return(0);
  if (sok_history_getpushes(states->history) == 0) return(0);

  /* Check if the solution is better than the one we had so far */
  bestscorelen = sok_history_getlen(game->solution);
  bestscorepushes = sok_history_getpushes(game->solution);
  myscorelen = sok_history_getlen(states->history);
  myscorepushes = sok_history_getpushes(states->history);
  if (bestscorelen < 1) betterflag = 1;
  if (bestscorelen > myscorelen) betterflag = 1;
  if ((bestscorelen == myscorelen) && (bestscorepushes > myscorepushes)) betterflag = 1;
  /* if our solution is better, save it */
  if (betterflag != 0) solution_save(game->crc64, states->history, "sol");
  return(1);
}


int sok_move(struct sokgame *game, enum SOKMOVE dir, int validitycheck, struct sokgamestates *states) {
  int res = 0;
  int x, y, vectorx = 0, vectory = 0, alreadysolved;
  char historychar = ' ';
  size_t movescount;
  movescount = sok_history_getlen(states->history);
  /* first of all let's check if we have enough place in history for a potential move - if not, realloc some place */
  if (movescount + 3 >= states->historyallocsize) {
    states->historyallocsize *= 2;
    states->history = realloc(states->history, states->historyallocsize);
    if (states->history == NULL) {
      printf("failed to allocate %lu bytes for history buffer!\n", (unsigned long)(states->historyallocsize));
      return(ERR_MEM_ALLOC_FAILED);
    }
  }
  /* now let's do our real stuff */
  alreadysolved = sok_checksolution(game, NULL);
  x = game->positionx;
  y = game->positiony;
  switch (dir) {
    case sokmoveNONE:
    case sokmoveUP:
      vectory = -1;
      states->angle = 0;
      historychar = 'u';
      break;
    case sokmoveRIGHT:
      vectorx = 1;
      states->angle = 90;
      historychar = 'r';
      break;
    case sokmoveDOWN:
      vectory = 1;
      states->angle = 180;
      historychar = 'd';
      break;
    case sokmoveLEFT:
      vectorx = -1;
      states->angle = 270;
      historychar = 'l';
      break;
  }

  if (y < 1) return(-1);
  if (game->field[x + vectorx][y + vectory] & field_wall) return(-1);
  /* is there an atom on our way? */
  if (game->field[x + vectorx][y + vectory] & field_atom) {
    if (alreadysolved != 0) return(-1);
    if ((y + vectory < 1) || (y + vectory > 62) || (x + vectorx < 1) || (x + vectorx > 62)) return(-1);
    if (game->field[x + vectorx * 2][y + vectory * 2] & (field_wall | field_atom)) return(-1);
    res |= sokmove_pushed;
    if (game->field[x + vectorx * 2][y + vectory * 2] & field_goal) res |= sokmove_ongoal;
    if (validitycheck == 0) {
      historychar -= 32; /* change historical move to uppercase to mark a push action */
      game->field[x + vectorx][y + vectory] &= ~field_atom;
      game->field[x + vectorx * 2][y + vectory * 2] |= field_atom;
    }
  }
  if (validitycheck == 0) {
    states->history[movescount] = historychar;
    states->history[movescount + 1] = 0; /* makes it a null-terminated string in case anyone would want to print it as-is */
    game->positiony += vectory;
    game->positionx += vectorx;
  }
  if ((alreadysolved == 0) && (sok_checksolution(game, states) != 0)) res |= sokmove_solved;
  return(res);
}

void sok_resetstates(struct sokgamestates *states) {
  if (states->history != NULL) free(states->history);
  memset(states, 0, sizeof(struct sokgamestates));
  states->historyallocsize = 64;
  states->history = malloc(states->historyallocsize);
  if (states->history != NULL) memset(states->history, 0, states->historyallocsize);
}

struct sokgamestates *sok_newstates(void) {
  struct sokgamestates *result;
  result = malloc(sizeof(struct sokgamestates));
  if (result == NULL) return(NULL);
  memset(result, 0, sizeof(struct sokgamestates));
  sok_resetstates(result);
  return(result);
}

void sok_freestates(struct sokgamestates *states) {
  if (states == NULL) return;
  if (states->history != NULL) free(states->history);
  free(states);
}

void sok_undo(struct sokgame *game, struct sokgamestates *states) {
  int movex = 0, movey = 0;
  size_t movescount;
  movescount = sok_history_getlen(states->history);
  if (movescount < 1) return;
  movescount -= 1;
  switch (states->history[movescount]) {
    case 'u':
    case 'U':
      movey = 1;
      states->angle = 0;
      break;
    case 'r':
    case 'R':
      movex = -1;
      states->angle = 90;
      break;
    case 'd':
    case 'D':
      movey = -1;
      states->angle = 180;
      break;
    case 'l':
    case 'L':
      movex = 1;
      states->angle = 270;
      break;
  }
  /* if it was a PUSH action, then move the atom back */
  if ((states->history[movescount] >= 'A') && ((states->history[movescount] <= 'Z'))) {
    game->field[game->positionx - movex][game->positiony - movey] &= ~field_atom;
    game->field[game->positionx][game->positiony] |= field_atom;
  }
  game->positionx += movex;
  game->positiony += movey;
  states->history[movescount] = 0;
}

void sok_play(struct sokgame *game, struct sokgamestates *states, char *playfile) {
  if (playfile == NULL) return;
  while (*playfile != 0) {
    enum SOKMOVE playmove;
    switch (*playfile) {
      case 'u':
      case 'U':
        playmove = sokmoveUP;
        break;
      case 'r':
      case 'R':
        playmove = sokmoveRIGHT;
        break;
      case 'd':
      case 'D':
        playmove = sokmoveDOWN;
        break;
      case 'l':
      case 'L':
        playmove = sokmoveLEFT;
        break;
      default:
        playmove = sokmoveLEFT;
        break;
    }
    sok_move(game, playmove, 0, states);
    playfile += 1;
  }
}
