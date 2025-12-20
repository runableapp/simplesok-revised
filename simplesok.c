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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>             /* malloc() */
#include <string.h>             /* memcpy() */
#include <time.h>
#include <inttypes.h>		/* PRIx64 */
#include "compat-sdl.h"         /* SDL       */

#include "gra.h"
#include "sok_core.h"
#include "save.h"
#include "data.h"           /* embedded assets (font, levels...) */
#include "gz.h"
#include "net.h"
#include "skin.h"

#include "dbg.h"


#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION __DATE__ " DEV"
#endif

#ifndef PKGDATADIR
#define PKGDATADIR "/usr/share/simplesok"
#endif

#ifndef SDL_RENDERER_FLAGS
#define SDL_RENDERER_FLAGS 0
#endif

#define TEXTIFY(text) #text
#define TEXTIFY_VALUE(value) TEXTIFY(value)
#define PDATE "2014-" TEXTIFY_VALUE(PACKAGE_YEAR)

#define INET_HOST "mateusz.fr"
#define INET_PORT 80
#define INET_PATH "/simplesok/netlevels/"

#define DEFAULT_SKIN "antique3"

#define MAXLEVELS 4096
#define SCREEN_DEFAULT_WIDTH 800
#define SCREEN_DEFAULT_HEIGHT 600

#define DISPLAYCENTERED 1
#define NOREFRESH 2

#define DRAWSCREEN_REFRESH 1
#define DRAWSCREEN_PLAYBACK 2
#define DRAWSCREEN_PUSH 4
#define DRAWSCREEN_NOBG 8
#define DRAWSCREEN_NOTXT 16

#define DRAWSTRING_CENTER -1
#define DRAWSTRING_RIGHT -2
#define DRAWSTRING_BOTTOM -3

#define DRAWPLAYFIELDTILE_DRAWATOM 1
#define DRAWPLAYFIELDTILE_PUSH 2

#define BLIT_LEVELMAP_BACKGROUND 1

#define FONT_SPACE_WIDTH 12
#define FONT_KERNING -3

#define SELECTLEVEL_BACK -1
#define SELECTLEVEL_QUIT -2
#define SELECTLEVEL_LOADFILE -3
#define SELECTLEVEL_OK -4

enum normalizedkeys {
  KEY_UP,
  KEY_DOWN,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_CTRL_UP,
  KEY_CTRL_DOWN,
  KEY_ENTER,
  KEY_BACKSPACE,
  KEY_PAGEUP,
  KEY_PAGEDOWN,
  KEY_HOME,
  KEY_END,
  KEY_ESCAPE,
  KEY_F1,
  KEY_F2,
  KEY_F3,
  KEY_F4,
  KEY_F5,
  KEY_F6,
  KEY_F7,
  KEY_F8,
  KEY_F9,
  KEY_F10,
  KEY_FULLSCREEN,
  KEY_F12,
  KEY_S,
  KEY_R,
  KEY_CTRL_C,
  KEY_CTRL_V,
  KEY_UNKNOWN
};

enum leveltype {
  LEVEL_INTERNAL,
  LEVEL_INTERNET,
  LEVEL_FILE
};


struct videosettings {
  unsigned short tilesize;
  int rotspeed; /* player's rotation speed (1..100) */
  int movspeed; /* player's moving [horizontal/vertical] speed (1..100) */
  const char *customskinfile;
};

/* returns the absolute value of the 'i' integer. */
static int absval(int i) {
  if (i < 0) return(-i);
  return(i);
}

/* uncompress a sokoban XSB string and returns a new malloced string with the decompressed version */
static char *unRLE(const char *xsb) {
  char *res = NULL;
  size_t resalloc = 16;
  unsigned int reslen = 0;
  long x;
  int rlecnt = -1;

  res = malloc(resalloc);
  if (res == NULL) return(NULL);

  for (x = 0; xsb[x] != 0; x++) {
    if ((xsb[x] >= '0') && (xsb[x] <= '9')) {
      if (rlecnt == -1) rlecnt = 0;
      rlecnt *= 10;
      rlecnt += xsb[x] - '0';
      continue;
    }

    if (rlecnt == -1) rlecnt = 1; /* no rle counter means '1 move' */

    for (; rlecnt > 0; rlecnt--) {
      if (reslen + 4 > resalloc) {
        resalloc *= 2;
        res = realloc(res, resalloc);
        if (res == NULL) return(NULL);
      }
      res[reslen++] = xsb[x];
    }
    rlecnt = -1;

  }
  res[reslen] = 0;
  return(res);
}

/* normalize SDL keys to values easier to handle */
static int normalizekeys(SDL_Keycode key) {
  switch (key) {
    case SDLK_UP:
    case SDLK_KP_8:
      if (SDL_GetModState() & CSDL_KMOD_CTRL) return(KEY_CTRL_UP);
      return(KEY_UP);
    case SDLK_DOWN:
    case SDLK_KP_2:
      if (SDL_GetModState() & CSDL_KMOD_CTRL) return(KEY_CTRL_DOWN);
      return(KEY_DOWN);
    case SDLK_LEFT:
    case SDLK_KP_4:
      return(KEY_LEFT);
    case SDLK_RIGHT:
    case SDLK_KP_6:
      return(KEY_RIGHT);
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
      if (SDL_GetModState() & CSDL_KMOD_ALT) return(KEY_FULLSCREEN);
      return(KEY_ENTER);
    case SDLK_BACKSPACE:
      return(KEY_BACKSPACE);
    case SDLK_PAGEUP:
    case SDLK_KP_9:
      return(KEY_PAGEUP);
    case SDLK_PAGEDOWN:
    case SDLK_KP_3:
      return(KEY_PAGEDOWN);
    case SDLK_HOME:
    case SDLK_KP_7:
      return(KEY_HOME);
    case SDLK_END:
    case SDLK_KP_1:
      return(KEY_END);
    case SDLK_ESCAPE:
      return(KEY_ESCAPE);
    case SDLK_F1:
      return(KEY_F1);
    case SDLK_F2:
      return(KEY_F2);
    case SDLK_F3:
      return(KEY_F3);
    case SDLK_F4:
      return(KEY_F4);
    case SDLK_F5:
      return(KEY_F5);
    case SDLK_F6:
      return(KEY_F6);
    case SDLK_F7:
      return(KEY_F7);
    case SDLK_F8:
      return(KEY_F8);
    case SDLK_F9:
      return(KEY_F9);
    case SDLK_F10:
      return(KEY_F10);
    case SDLK_F11:
      return(KEY_FULLSCREEN);
    case SDLK_F12:
      return(KEY_F12);
    case CSDL_K_S:
      return(KEY_S);
    case CSDL_K_R:
      return(KEY_R);
    case CSDL_K_C:
      if (SDL_GetModState() & CSDL_KMOD_CTRL) return(KEY_CTRL_C);
      break;
    case CSDL_K_V:
      if (SDL_GetModState() & CSDL_KMOD_CTRL) return(KEY_CTRL_V);
      break;
  }
  return(KEY_UNKNOWN);
}


/* trims a trailing newline, if any, from a string */
static void trimstr(char *str) {
  int x, lastrealchar = -1;
  if (str == NULL) return;
  for (x = 0; str[x] != 0; x++) {
    switch (str[x]) {
      case ' ':
      case '\t':
      case '\r':
      case '\n':
        break;
      default:
        lastrealchar = x;
        break;
    }
  }
  str[lastrealchar + 1] = 0;
}

/* returns 0 if string is not a legal solution. non-zero otherwise. */
static int isLegalSokoSolution(const char *solstr) {
  if (solstr == NULL) return(0);
  if (strlen(solstr) < 1) return(0);
  if (debugmode) puts("got a CTRL+C solution, let's parse it:");
  for (;;) {
    if (debugmode) printf("%c", *solstr);
    switch (*solstr) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        if (solstr[1] == 0) return(0); /* numbers are tolerated, but only if followed by something */
        break;
      case 'u':
      case 'U':
      case 'r':
      case 'R':
      case 'd':
      case 'D':
      case 'l':
      case 'L':
        break;
      case 0:
        if (debugmode) puts("\nend of string, all good!");
        return(1);
      default:
        return(0);
    }
    solstr += 1;
  }
}


/* a crude timer that "ticks" roughly every 30ms, providing simplesok with information that it is a good time to update the screen (which translates to a refresh rate of about 33 Hz) */
static int sok_isitrefreshtime(void) {
  static Uint32 nextrefresh;
  Uint32 curtime = SDL_GetTicks();
  if (curtime < nextrefresh) return(0);
  /* schedule next refresh time and return a tick */
  nextrefresh = curtime + 30; /* miliseconds */
  return(1);
}


static int flush_events(void) {
  SDL_Event event;
  int exitflag = 0;
  while (SDL_PollEvent(&event) != 0)
    if (event.type == CSDL_EVENT_QUIT)
      exitflag = 1;
  return(exitflag);
}


static void switchfullscreen(SDL_Window *window) {
  static int fullscreenflag = 0;
  fullscreenflag ^= 1;
  CSDL_SetWindowFullscreen(window, fullscreenflag);
  SDL_Delay(50); /* wait for 50ms - the video thread needs some time to set things up */
  flush_events(); /* going fullscreen fires some garbage events that I don't want to hear about */
}


static int getoffseth(const struct sokgame *game, int winw, unsigned short tilesize) {
  /* if playfield is smaller than the screen */
  if (game->field_width * tilesize <= winw) return((winw / 2) - (game->field_width * tilesize / 2));
  /* if playfield is larger than the screen */
  if (game->positionx * tilesize + (tilesize / 2) > (winw / 2)) {
    int res = (winw / 2) - (game->positionx * tilesize + (tilesize / 2));
    if ((game->field_width * tilesize) + res < winw) res = winw - (game->field_width * tilesize);
    return(res);
  }
  return(0);
}

static int getoffsetv(const struct sokgame *game, int winh, int tilesize) {
  /* if playfield is smaller than the screen */
  if (game->field_height * tilesize <= winh) return((winh / 2) - (game->field_height * tilesize / 2));
  /* if playfield is larger than the screen */
  if (game->positiony * tilesize + (tilesize / 2) > winh / 2) {
    int res = (winh / 2) - (game->positiony * tilesize + (tilesize / 2));
    if ((game->field_height * tilesize) + res < winh) res = winh - (game->field_height * tilesize);
    return(res);
  }
  return(0);
}

/* wait for a key up to timeout seconds (-1 = indefinitely), while redrawing the renderer screen, if not null */
static int wait_for_a_key(int timeout, SDL_Renderer *renderer) {
  SDL_Event event;
  Uint32 timeouttime = SDL_GetTicks();
  if (timeout > 0) timeouttime += (Uint32)timeout * 1000;
  for (;;) {
    SDL_Delay(50);
    if (SDL_PollEvent(&event) != 0) {
      if (renderer != NULL) SDL_RenderPresent(renderer);
      if (event.type == CSDL_EVENT_QUIT) {
        return(1);
      } else if (event.type == CSDL_EVENT_KEY_DOWN) {
        return(0);
      }
    }
    if ((timeout > 0) && (SDL_GetTicks() >= timeouttime)) return(0);
  }
}

/* display a bitmap onscreen */
static int displaytexture(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Window *window, int timeout, int flags, unsigned char alpha) {
  int winw, winh;
  SDL_Rect rect, *rectptr;
  CSDL_QueryTexture(texture, NULL, NULL, &rect.w, &rect.h);
  SDL_GetWindowSize(window, &winw, &winh);
  if (flags & DISPLAYCENTERED) {
    rectptr = &rect;
    rect.x = (winw - rect.w) / 2;
    rect.y = (winh - rect.h) / 2;
  } else {
    rectptr = NULL;
  }
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureAlphaMod(texture, alpha);
  if (CSDL_RenderTexture(renderer, texture, NULL, rectptr) != 0) printf("CSDL_RenderTexture() failed: %s\n", SDL_GetError());
  if ((flags & NOREFRESH) == 0) SDL_RenderPresent(renderer);
  if (timeout != 0) return(wait_for_a_key(timeout, renderer));
  return(0);
}

/* provides width and height of a string (in pixels) */
static void get_string_size(const char *string, int fontsize, const struct spritesstruct *sprites, int *w, int *h) {
  int glyphw, glyphh;
  *w = 0;
  *h = 0;
  while (*string != 0) {
    if (*string == ' ') {
      *w += FONT_SPACE_WIDTH * fontsize / 100;
    } else {
      CSDL_QueryTexture(sprites->font[(unsigned char)(*string)], NULL, NULL, &glyphw, &glyphh);
      *w += glyphw * fontsize / 100 + FONT_KERNING * fontsize / 100;
      if (glyphh * fontsize / 100 > *h) *h = glyphh * fontsize / 100;
    }
    string += 1;
  }
}

/* explode a string into wordwrapped substrings */
static void wordwrap(const char *string, char **multiline, int maxlines, int maxwidth, int fontsize, const struct spritesstruct *sprites) {
  int lastspace, multilineid;
  int x, stringw, stringh;
  char *tmpstring;
  /* set all multiline entries to NULL */
  for (x = 0; x < maxlines; x++) multiline[x] = NULL;

  /* find the next word boundary */
  lastspace = -1;
  multilineid = 0;
  for (;;) { /* loop on every word, and check if we reached the end */
    for (x = lastspace + 1; ; x++) {
      if ((string[x] == ' ') || (string[x] == '\t') || (string[x] == '\n') || (string[x] == 0)) {
        lastspace = x;
        break;
      }
    }
    /* is this word boundary fitting on screen? */
    tmpstring = strdup(string);
    tmpstring[lastspace] = 0;
    get_string_size(tmpstring, fontsize, sprites, &stringw, &stringh);
    if (stringw < maxwidth) {
      if (multiline[multilineid] != NULL) free(multiline[multilineid]);
      multiline[multilineid] = tmpstring;
    } else {
      free(tmpstring);
      if (multiline[multilineid] == NULL) break;
      lastspace = -1;
      string += strlen(multiline[multilineid]) + 1;
      multilineid += 1;
      if (multilineid >= maxlines) {
        size_t lastlinelen = strlen(multiline[multilineid - 1]);
        /* if text have been truncated, print '...' at the end of the last line */
        if (lastlinelen >= 3) {
          multiline[multilineid - 1][lastlinelen - 3] = '.';
          multiline[multilineid - 1][lastlinelen - 2] = '.';
          multiline[multilineid - 1][lastlinelen - 1] = '.';
        }
        break;
      }
    }
    if ((lastspace >= 0) && (string[lastspace] == 0)) break;
  }
}

/* blits a string onscreen, scaling the font at fontsize percents. The string is placed at starting position x/y */
static void draw_string(const char *orgstring, int fontsize, unsigned char alpha, struct spritesstruct *sprites, SDL_Renderer *renderer, int x, int y, SDL_Window *window, int maxlines, int pheight) {
  int i, winw, winh;
  char *string;
  SDL_Texture *glyph;
  SDL_Rect rectsrc, rectdst;
  char *multiline[16];
  int multilineid = 0;
  if (maxlines > 16) maxlines = 16;
  /* get size of the window */
  SDL_GetWindowSize(window, &winw, &winh);
  wordwrap(orgstring, multiline, maxlines, winw - x, fontsize, sprites);
  /* loop on every line */
  for (multilineid = 0; (multilineid < maxlines) && (multiline[multilineid] != NULL); multilineid += 1) {
    string = multiline[multilineid];
    if (multilineid > 0) y += pheight;
    /* if centering is requested, get size of the string */
    if ((x < 0) || (y < 0)) {
      int stringw, stringh;
      /* get pixel length of the string */
      get_string_size(string, fontsize, sprites, &stringw, &stringh);
      if (x == DRAWSTRING_CENTER) x = (winw - stringw) >> 1;
      if (x == DRAWSTRING_RIGHT) x = winw - stringw - 10;
      if (y == DRAWSTRING_BOTTOM) y = winh - stringh;
      if (y == DRAWSTRING_CENTER) y = (winh - stringh) / 2;
    }
    rectdst.x = x;
    rectdst.y = y;
    for (i = 0; string[i] != 0; i++) {
      if (string[i] == ' ') {
        rectdst.x += FONT_SPACE_WIDTH * fontsize / 100;
        continue;
      }
      glyph = sprites->font[(unsigned char)(string[i])];
      CSDL_QueryTexture(glyph, NULL, NULL, &rectsrc.w, &rectsrc.h);
      rectdst.w = rectsrc.w * fontsize / 100;
      rectdst.h = rectsrc.h * fontsize / 100;
      SDL_SetTextureAlphaMod(glyph, alpha);
      CSDL_RenderTexture(renderer, glyph, NULL, &rectdst);
      rectdst.x += (rectsrc.w * fontsize / 100) + (FONT_KERNING * fontsize / 100);
    }
    /* free the multiline memory */
    free(string);
  }
}


/* get wall neighbors, returns an 8-bit value where each bit indicates the
 * presence of a neigbor:
 *
 * 128 064 032
 * 016     008
 * 004 002 001
 *
 * Example: if bits 0, 1 and 2 are set (ie. value 7) then walls are present on
 * both bottom corners and bottom center. */
static unsigned short getwallneighb(const struct sokgame *game, int x, int y) {
  unsigned short res = 0;
  if (y > 0) { /* check top row */
    if ((x > 0) && (game->field[x - 1][y - 1] & field_wall)) res |= 128;
    if (game->field[x][y - 1] & field_wall) res |= 64;
    if ((x < 63) && (game->field[x + 1][y - 1] & field_wall)) res |= 32;
  }

  /* middle row */
  if ((x > 0) && (game->field[x - 1][y] & field_wall)) res |= 16;
  if ((x < 63) && (game->field[x + 1][y] & field_wall)) res |= 8;

  /* bottom row */
  if (y < 63) {
    if ((x > 0) && (game->field[x - 1][y + 1] & field_wall)) res |= 4;
    if (game->field[x][y + 1] & field_wall) res |= 2;
    if ((x < 63) && (game->field[x + 1][y + 1] & field_wall)) res |= 1;
  }

  return(res);
}


static void draw_wall(SDL_Renderer *renderer, const struct spritesstruct *sprites, const struct sokgame *game, int x, int y, unsigned short tilesize, int xoffset, int yoffset) {
  unsigned short wallid, neighbors;

  /* draw the wall elements (4 quarters) */
  neighbors = getwallneighb(game, x, y);

  /* top-left quarter */
  switch (neighbors & 0xD0) { /* 11010000 */
    case 0xC0: /* 11000000 */
    case 0x40: /* 01000000 */
      wallid = SPRITE_WALL_VERTIC;
      break;
    case 0x90: /* 10010000 */
    case 0x10: /* 00010000 */
      wallid = SPRITE_WALL_HORIZ;
      break;
    case 0x50: /* 01010000 (inner corner) */
      wallid = SPRITE_WALL_CORNER;
      break;
    case 0xD0: /* 11010000 (full block) */
      wallid = SPRITE_WALL_PLAIN;
      break;
    default:
      wallid = SPRITE_WALL_ISLAND;
      break;
  }
  gra_rendertilequarter(renderer, sprites, wallid, xoffset, yoffset, tilesize / 2, 0);

  /* top-right quarter */
  switch (neighbors & 0x68) { /* 011 01 000 */
    case 0x60: /* 011 00 000 */
    case 0x40: /* 010 00 000 */
      wallid = SPRITE_WALL_VERTIC;
      break;
    case 0x28: /* 001 01 000 */
    case 0x08: /* 000 01 000 */
      wallid = SPRITE_WALL_HORIZ;
      break;
    case 0x48: /* 010 01 000 (inner corner) */
      wallid = SPRITE_WALL_CORNER;
      break;
    case 0x68: /* 011 01 000 (full block) */
      wallid = SPRITE_WALL_PLAIN;
      break;
    default:
      wallid = SPRITE_WALL_ISLAND;
      break;
  }
  gra_rendertilequarter(renderer, sprites, wallid, xoffset + (tilesize / 2), yoffset, tilesize / 2, 1);

  /* bottom-left quarter */
  switch (neighbors & 0x16) { /* 000 10 110 */
    case 0x06: /* 000 00 110 */
    case 0x02: /* 000 00 010 */
      wallid = SPRITE_WALL_VERTIC;
      break;
    case 0x14: /* 000 10 100 */
    case 0x10: /* 000 10 000 */
      wallid = SPRITE_WALL_HORIZ;
      break;
    case 0x12: /* 000 10 010 (inner corner) */
      wallid = SPRITE_WALL_CORNER;
      break;
    case 0x16: /* 000 10 110 (full block) */
      wallid = SPRITE_WALL_PLAIN;
      break;
    default:
      wallid = SPRITE_WALL_ISLAND;
      break;
  }
  gra_rendertilequarter(renderer, sprites, wallid, xoffset, yoffset + (tilesize / 2), tilesize / 2, 2);

  /* bottom-right quarter */
  switch (neighbors & 0x0B) { /* 000 01 011 */
    case 0x03: /* 000 00 011 */
    case 0x02: /* 000 00 010 */
      wallid = SPRITE_WALL_VERTIC;
      break;
    case 0x09: /* 000 01 001 */
    case 0x08: /* 000 01 000 */
      wallid = SPRITE_WALL_HORIZ;
      break;
    case 0x0A: /* 000 01 010 (inner corner) */
      wallid = SPRITE_WALL_CORNER;
      break;
    case 0x0B: /* 000 01 011 (full block) */
      wallid = SPRITE_WALL_PLAIN;
      break;
    default:
      wallid = SPRITE_WALL_ISLAND;
      break;
  }
  gra_rendertilequarter(renderer, sprites, wallid, xoffset + (tilesize / 2), yoffset + (tilesize / 2), tilesize / 2, 3);

}


static void draw_playfield_tile(const struct sokgame *game, int x, int y, struct spritesstruct *sprites, SDL_Renderer *renderer, int winw, int winh, const struct videosettings *settings, int flags, int moveoffsetx, int moveoffsety) {
  int xpix, ypix;
  /* compute the pixel coordinates of the destination field */
  xpix = getoffseth(game, winw, settings->tilesize) + (x * settings->tilesize) + moveoffsetx;
  ypix = getoffsetv(game, winh, settings->tilesize) + (y * settings->tilesize) + moveoffsety;

  if ((flags & DRAWPLAYFIELDTILE_DRAWATOM) == 0) {
    if (game->field[x][y] & field_floor) gra_rendertile(renderer, sprites, SPRITE_FLOOR, xpix, ypix, settings->tilesize, 0);
    if (game->field[x][y] & field_goal) gra_rendertile(renderer, sprites, SPRITE_GOAL, xpix, ypix, settings->tilesize, 0);
    if (game->field[x][y] & field_wall) draw_wall(renderer, sprites, game, x, y, settings->tilesize, xpix, ypix);
  } else if (game->field[x][y] & field_atom) {
    unsigned short boxsprite = SPRITE_BOX;
    if (game->field[x][y] & field_goal) {
      boxsprite = SPRITE_BOXOK;
      if (flags & DRAWPLAYFIELDTILE_PUSH) {
        if ((game->positionx == x - 1) && (game->positiony == y) && (moveoffsetx > 0) && ((game->field[x + 1][y] & field_goal) == 0)) boxsprite = SPRITE_BOX;
        if ((game->positionx == x + 1) && (game->positiony == y) && (moveoffsetx < 0) && ((game->field[x - 1][y] & field_goal) == 0)) boxsprite = SPRITE_BOX;
        if ((game->positionx == x) && (game->positiony == y - 1) && (moveoffsety > 0) && ((game->field[x][y + 1] & field_goal) == 0)) boxsprite = SPRITE_BOX;
        if ((game->positionx == x) && (game->positiony == y + 1) && (moveoffsety < 0) && ((game->field[x][y - 1] & field_goal) == 0)) boxsprite = SPRITE_BOX;
      }
    }
    gra_rendertile(renderer, sprites, boxsprite, xpix, ypix, settings->tilesize, 0);
  }
}

static void draw_player(const struct sokgame *game, const struct sokgamestates *states, struct spritesstruct *sprites, SDL_Renderer *renderer, int winw, int winh, const struct videosettings *settings, int offsetx, int offsety) {
  SDL_Rect rect;
  unsigned short playersprite = SPRITE_PLAYERUP;
  int angle = states->angle;

  /* compute the dst rect */
  rect.x = getoffseth(game, winw, settings->tilesize) + (game->positionx * settings->tilesize) + offsetx;
  rect.y = getoffsetv(game, winh, settings->tilesize) + (game->positiony * settings->tilesize) + offsety;
  rect.w = settings->tilesize;
  rect.h = settings->tilesize;

  if ((sprites->flags & SPRITES_FLAG_PLAYERROTATE) == 0) {
    switch (states->angle) {
      case 0:
        playersprite = SPRITE_PLAYERUP;
        break;
      case 90:
        playersprite = SPRITE_PLAYERRIGHT;
        break;
      case 180:
        playersprite = SPRITE_PLAYERDOWN;
        break;
      case 270:
        playersprite = SPRITE_PLAYERLEFT;
        break;
    }
    angle = 0;
  }

  /* if player happens to be on a goal, then use the special "player on goal" version of the sprite */
  if (game->field[game->positionx][game->positiony] & field_goal) playersprite += 4;

  gra_rendertile(renderer, sprites, playersprite, rect.x, rect.y, settings->tilesize, angle);
}


static void draw_screen(const struct sokgame *game, const struct sokgamestates *states, struct spritesstruct *sprites, SDL_Renderer *renderer, SDL_Window *window, const struct videosettings *settings, int moveoffsetx, int moveoffsety, int scrolling, int flags, const char *levelname) {
  int x, y, winw, winh, offx, offy;
  /* int partialoffsetx = 0, partialoffsety = 0; */
  char stringbuff[256];
  int scrollingadjx = 0, scrollingadjy = 0; /* this is used when scrolling + movement of player is needed */
  int drawtile_flags = 0;

  SDL_GetWindowSize(window, &winw, &winh);
  SDL_RenderClear(renderer);

  if ((flags & DRAWSCREEN_NOBG) == 0) {
    gra_renderbg(renderer, sprites, SPRITE_BG, winw, winh);
  }

  if (flags & DRAWSCREEN_PUSH) drawtile_flags = DRAWPLAYFIELDTILE_PUSH;

  if (scrolling > 0) {
    if (moveoffsetx > scrolling) {
      scrollingadjx = moveoffsetx - scrolling;
      moveoffsetx = scrolling;
    }
    if (moveoffsetx < -scrolling) {
      scrollingadjx = moveoffsetx + scrolling;
      moveoffsetx = -scrolling;
    }
    if (moveoffsety > scrolling) {
      scrollingadjy = moveoffsety - scrolling;
      moveoffsety = scrolling;
    }
    if (moveoffsety < -scrolling) {
      scrollingadjy = moveoffsety + scrolling;
      moveoffsety = -scrolling;
    }
  }
  /* draw non-moveable tiles (floors, walls, goals) */
  for (y = 0; y < game->field_height; y++) {
    for (x = 0; x < game->field_width; x++) {
      if (scrolling != 0) {
        draw_playfield_tile(game, x, y, sprites, renderer, winw, winh, settings, drawtile_flags, -moveoffsetx, -moveoffsety);
      } else {
        draw_playfield_tile(game, x, y, sprites, renderer, winw, winh, settings, drawtile_flags, 0, 0);
      }
    }
  }
  /* draw moveable elements (atoms) */
  for (y = 0; y < game->field_height; y++) {
    for (x = 0; x < game->field_width; x++) {
      offx = 0;
      offy = 0;
      if (scrolling == 0) {
        if ((moveoffsetx > 0) && (x == game->positionx + 1) && (y == game->positiony)) offx = moveoffsetx;
        if ((moveoffsetx < 0) && (x == game->positionx - 1) && (y == game->positiony)) offx = moveoffsetx;
        if ((moveoffsety > 0) && (y == game->positiony + 1) && (x == game->positionx)) offy = moveoffsety;
        if ((moveoffsety < 0) && (y == game->positiony - 1) && (x == game->positionx)) offy = moveoffsety;
      } else {
        offx = -moveoffsetx;
        offy = -moveoffsety;
        if ((moveoffsetx > 0) && (x == game->positionx + 1) && (y == game->positiony)) offx = scrollingadjx;
        if ((moveoffsetx < 0) && (x == game->positionx - 1) && (y == game->positiony)) offx = scrollingadjx;
        if ((moveoffsety > 0) && (y == game->positiony + 1) && (x == game->positionx)) offy = scrollingadjy;
        if ((moveoffsety < 0) && (y == game->positiony - 1) && (x == game->positionx)) offy = scrollingadjy;
      }
      draw_playfield_tile(game, x, y, sprites, renderer, winw, winh, settings, DRAWPLAYFIELDTILE_DRAWATOM, offx, offy);
    }
  }
  /* draw where the player is */
  if (scrolling != 0) {
    draw_player(game, states, sprites, renderer, winw, winh, settings, scrollingadjx, scrollingadjy);
  } else {
    draw_player(game, states, sprites, renderer, winw, winh, settings, moveoffsetx, moveoffsety);
  }
  /* draw text */
  if ((flags & DRAWSCREEN_NOTXT) == 0) {
    sprintf(stringbuff, "%s, level %d", levelname, game->level);
    draw_string(stringbuff, 100, 255, sprites, renderer, 10, DRAWSTRING_BOTTOM, window, 1, 0);
    if (game->solution != NULL) {
      sprintf(stringbuff, "best score: %lu/%lu", (unsigned long)sok_history_getlen(game->solution), (unsigned long)sok_history_getpushes(game->solution));
    } else {
      sprintf(stringbuff, "best score: -");
    }
    draw_string(stringbuff, 100, 255, sprites, renderer, DRAWSTRING_RIGHT, 0, window, 1, 0);
    sprintf(stringbuff, "moves: %lu / pushes: %lu", (unsigned long)sok_history_getlen(states->history), (unsigned long)sok_history_getpushes(states->history));
    draw_string(stringbuff, 100, 255, sprites, renderer, 10, 0, window, 1, 0);
  }
  if ((flags & DRAWSCREEN_PLAYBACK) && (time(NULL) % 2 == 0)) draw_string("*** PLAYBACK ***", 100, 255, sprites, renderer, DRAWSTRING_CENTER, 32, window, 1, 0);
  /* Update the screen */
  if (flags & DRAWSCREEN_REFRESH) SDL_RenderPresent(renderer);
}


static int rotatePlayer(struct spritesstruct *sprites, const struct sokgame *game, struct sokgamestates *states, enum SOKMOVE dir, SDL_Renderer *renderer, SDL_Window *window, const struct videosettings *settings, const char *levelname, int drawscreenflags) {
  /* shortest-path vectors to get from one angle to another */
  const int arr[4][4] = {{ 0,  1,  0, -1}, /* angle 0->0, 0->90, 0->180, 0->270 */
                         {-1,  0,  1,  0}, /* angle 90->0, 90->90, 90->180, etc */
                         { 0, -1,  0,  1},
                         { 1,  0, -1,  0}};
  int dstangle;
  int dirmotion;

  switch (dir) {
    default:    /* sokmoveNONE, sokmoveUP */
      dstangle = 0;
      break;
    case sokmoveRIGHT:
      dstangle = 90;
      break;
    case sokmoveDOWN:
      dstangle = 180;
      break;
    case sokmoveLEFT:
      dstangle = 270;
      break;
  }

  /* if rotation speed is set to max value (100) or skin is primitive (no
   * transparency) then jump right away to the destination angle */
  if ((settings->rotspeed == 100) || (sprites->flags & SPRITES_FLAG_PRIMITIVE)) states->angle = dstangle;

  /* if no change in angle, stop here */
  if (states->angle == dstangle) return(0);

  /* figure out how to compute the shortest way to rotate the player */
  dirmotion = arr[states->angle / 90][dstangle / 90];
  if (dirmotion == 0) dirmotion = (time(NULL) & 1) * 2 - 1; /* add some pseudo random behavior for extra fun */

  /* perform the animated rotation */
  for (;;) {
    SDL_Delay(1);
    if (sok_isitrefreshtime()) {
      int i;

      /* make sure not to rotate past the destination angle */
      for (i = 0; i < settings->rotspeed; i++) {
        states->angle += dirmotion;
        if (states->angle >= 360) states->angle = 0;
        if (states->angle < 0) states->angle = 359;
        if (states->angle == dstangle) break;
      }

      draw_screen(game, states, sprites, renderer, window, settings, 0, 0, 0, DRAWSCREEN_REFRESH | drawscreenflags, levelname);

      if (dstangle == states->angle) break;
    }
  }

  return(1);
}


static int scrollneeded(struct sokgame *game, SDL_Window *window, unsigned short tilesize, int offx, int offy) {
  int winw, winh, offsetx, offsety, result = 0;
  SDL_GetWindowSize(window, &winw, &winh);
  offsetx = absval(getoffseth(game, winw, tilesize));
  offsety = absval(getoffsetv(game, winh, tilesize));
  game->positionx += offx;
  game->positiony += offy;
  result = offsetx - absval(getoffseth(game, winw, tilesize));
  if (result == 0) result = offsety - absval(getoffsetv(game, winh, tilesize));
  if (result < 0) result = -result; /* convert to abs() value */
  game->positionx -= offx;
  game->positiony -= offy;
  return(result);
}

static void loadlevel(struct sokgame *togame, struct sokgame *fromgame, struct sokgamestates *states) {
  memcpy(togame, fromgame, sizeof(struct sokgame));
  sok_resetstates(states);
}

static char *processDropFileEvent(SDL_Event *event, char **levelfile) {
  if (event->type != CSDL_EVENT_DROP_FILE) return(NULL);
  if (CSDL_DROP_FILE(event->drop) == NULL) return(NULL);
  if (*levelfile != NULL) free(*levelfile);
  *levelfile = strdup(CSDL_DROP_FILE(event->drop));
  return(*levelfile);
}


/* generic menu: display some choices and wait for the user to make a choice.
 * returns the item number of selected position,
 *  or -1 if user pressed ESC
 *  or -2 if a file has been dropped on window (filedrop is then filled). */
static int menu(SDL_Renderer *renderer, struct spritesstruct *sprites, SDL_Window *window, const struct videosettings *settings, const char **positions, int fontsize, int preselect, char **filedrop) {
  int longeststringw, higheststringh;
  int selection = preselect;
  int oldpusherposy = 0, newpusherposy, selectionchangeflag = 0;
  int textvadj = 12;
  int selectionpos[256];
  int poscount;
  int posoffset = 0;
  SDL_Event event;
  SDL_Rect rect;

  /* count the number of positions in menu and compute the pixel width of the
   * longest position */
  longeststringw = 0;
  higheststringh = 0;
  for (poscount = 0; positions[poscount] != NULL; poscount++) {
    int stringw, stringh;
    get_string_size(positions[poscount], fontsize, sprites, &stringw, &stringh);
    if (stringw > longeststringw) longeststringw = stringw;
    if (stringh > higheststringh) higheststringh = stringh;
  }

  for (;;) {
    int winw, winh, x;
    int step;

    /* by how many pixels the cursor should be moved at each screen refresh */
    step = (settings->movspeed * settings->tilesize) / 100;
    if (step == 0) step = 1;

    /* get window's width and height */
    SDL_GetWindowSize(window, &winw, &winh);

    /* precompute the y-axis position of every line */
    RECOMPUTE_POSITIONS:
    {
    int voffset = 0; /* accumulated offset to account for empty gaps (if present) */
    for (x = posoffset; x < poscount; x++) {
      selectionpos[x] = (int)((winh * 0.51) + ((x - posoffset) * higheststringh * 1.15)) + voffset;
      /* empty space smaller for empty gaps */
      if (positions[x][0] == 0) voffset -= (higheststringh * 0.6);
    }
    }

    /* compute the menu scroll offset (in case menu is too long to fit on screen) */
    if ((selection <= posoffset) && (posoffset > 0)) {
      posoffset--;
      goto RECOMPUTE_POSITIONS;
    }
    if (selectionpos[selection] + (higheststringh * 2) > winh) {
      posoffset++;
      goto RECOMPUTE_POSITIONS;
    }

    /* compute the dst rect of the pusher */
    rect.w = settings->tilesize;
    rect.h = settings->tilesize;
    rect.x = ((winw - longeststringw) >> 1) - 54;
    newpusherposy = selectionpos[selection] + 25 - (rect.h / 2);
    if (selectionchangeflag == 0) oldpusherposy = newpusherposy;

    /* draw the screen */
    rect.y = oldpusherposy;
    if ((settings->movspeed == 100) || (sprites->flags & SPRITES_FLAG_PRIMITIVE)) rect.y = newpusherposy; /* movspeed=100 means 'instant move' */

    for (;;) {
      if (sok_isitrefreshtime()) {
        SDL_RenderClear(renderer);
        gra_renderbg(renderer, sprites, SPRITE_BG, winw, winh);
        { /* render title and version strings */
          int sokow, sokoh, simpw, simph, verw, verh;
          int tity;
          const char *simpstr = "simple";
          const char *sokostr = "SOKOBAN";
          const char *verstr = "ver " PACKAGE_VERSION;

          get_string_size(simpstr, 100, sprites, &simpw, &simph);
          get_string_size(sokostr, 300, sprites, &sokow, &sokoh);
          get_string_size(verstr, 100, sprites, &verw, &verh);

          tity = (selectionpos[0] - (sokoh * 8 / 10)) / 2 - (simph * 8 / 10);

          draw_string(simpstr, 100, 200, sprites, renderer, 10 + (winw - sokow) / 2, tity, window, 1, 0);
          tity += simph * 8 / 10;
          draw_string(sokostr, 300, 255, sprites, renderer, (winw - sokow) / 2, tity, window, 1, 0);
          tity += sokoh * 8 / 10;
          draw_string(verstr, 100, 180, sprites, renderer, (sokow + (winw - sokow) / 2) - verw, tity, window, 1, 0);

        }

        {
          int player_cursor = SPRITE_PLAYERRIGHT, angle = 0;
          if (sprites->flags & SPRITES_FLAG_PLAYERROTATE) {
            player_cursor = SPRITE_PLAYERUP;
            angle = 90;
          }
          gra_rendertile(renderer, sprites, player_cursor, rect.x, rect.y, settings->tilesize, angle);
        }

        for (x = posoffset; x < poscount; x++) {
          draw_string(positions[x], fontsize, 255, sprites, renderer, rect.x + 54, textvadj + selectionpos[x], window, 1, 0);
        }
        SDL_RenderPresent(renderer);
        if (rect.y == newpusherposy) break;

        if (newpusherposy < oldpusherposy) {
          rect.y -= step;
          if (rect.y < newpusherposy) rect.y = newpusherposy;
        } else {
          rect.y += step;
          if (rect.y > newpusherposy) rect.y = newpusherposy;
        }

      }
      SDL_Delay(1);
    }
    oldpusherposy = newpusherposy;
    selectionchangeflag = 0;

    /* Wait for an event - but ignore 'KEYUP' and 'MOUSEMOTION' events, since they are worthless in this game */
    for (;;)
      if (SDL_WaitEvent(&event) != 0 && event.type != CSDL_EVENT_KEY_UP &&
	  event.type != CSDL_EVENT_MOUSE_MOTION)
	break;

    /* check what event we got */
    if (event.type == CSDL_EVENT_QUIT) {
      return(-10);
    } else if (event.type == CSDL_EVENT_DROP_FILE && filedrop != NULL) {
      if (processDropFileEvent(&event, filedrop) != NULL) return(-2);
    } else if (event.type == CSDL_EVENT_KEY_DOWN) {
      switch (normalizekeys(CSDL_KEY_SYM(event.key))) {
        case KEY_UP:
          selection--;
          selectionchangeflag = 1;
          /* skip also gap if present */
          if ((selection > 0) && (positions[selection][0] == 0)) selection--;
          break;
        case KEY_DOWN:
          selection++;
          selectionchangeflag = 1;
          /* skip also gap if present */
          if ((selection < poscount) && (positions[selection][0] == 0)) selection++;
          break;
        case KEY_ENTER:
          return(selection);
        case KEY_FULLSCREEN:
          switchfullscreen(window);
          break;
        case KEY_ESCAPE:
          return(-1);
      }
      if (selection < 0) selection = 0;
      if (selection >= poscount) selection = poscount - 1;
    }
  }
}


/* waits for the user to choose a game type or to load an external xsb file and returns either a pointer to a memory chunk with xsb data or to fill levelfile with a filename */
static unsigned char *selectgametype(SDL_Renderer *renderer, struct spritesstruct *sprites, SDL_Window *window, const struct videosettings *settings, char **levelfile, size_t *levelfilelen) {
  static int selection = 0;
  int choice;
  const char *levname[] = {"Easy (Microban)",
                           "Normal (Sasquatch)",
                           "Hard (Sasquatch III)",
                           "",
                           "Internet levels",
                           "Skin configuration",
                           "",
                           "Quit",
                           NULL};

  *levelfilelen = 0;

  choice = menu(renderer, sprites, window, settings, levname, 100, selection, levelfile);
  if (choice >= 0) selection = choice;

  switch (choice) {
    case 0: /* easy (Microban) */
      *levelfilelen = assets_levels_microban_xsb_gz_len;
      return(assets_levels_microban_xsb_gz);
    case 1: /* normal (Sasquatch) */
      *levelfilelen = assets_levels_sasquatch_xsb_gz_len;
      return(assets_levels_sasquatch_xsb_gz);
    case 2: /* hard (Sasquatch III) */
      *levelfilelen = assets_levels_sasquatch3_xsb_gz_len;
      return(assets_levels_sasquatch3_xsb_gz);
    case 4: /* internet levels */
      return((unsigned char *)"@");
    case 5: /* configuration */
      return((unsigned char *)"#");
    default: /* quit, ESC or file drop */
      return(NULL);
  }
}


/* blit a level preview */
static void blit_levelmap(const struct sokgame *game, struct spritesstruct *sprites, int xpos, int ypos, SDL_Renderer *renderer, unsigned short tilesize, unsigned char alpha, int flags) {
  int x, y, bgpadding = tilesize * 3;
  SDL_Rect rect, bgrect;

  bgrect.x = xpos - (game->field_width * tilesize + bgpadding) / 2;
  bgrect.y = ypos - (game->field_height * tilesize + bgpadding) / 2;
  bgrect.w = game->field_width * tilesize + bgpadding;
  bgrect.h = game->field_height * tilesize + bgpadding;
  /* if background enabled, compute coordinates of the background and draw it */
  if (flags & BLIT_LEVELMAP_BACKGROUND) {
    SDL_SetRenderDrawColor(renderer, 0x12, 0x12, 0x12, 255);
    CSDL_RenderFillRect(renderer, &bgrect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  }
  for (y = 0; y < game->field_height; y++) {
    for (x = 0; x < game->field_width; x++) {
      /* compute coordinates of the tile on screen */
      rect.x = xpos + (tilesize * x) - (game->field_width * tilesize) / 2;
      rect.y = ypos + (tilesize * y) - (game->field_height * tilesize) / 2;
      /* draw the tile */
      if (game->field[x][y] & field_floor) gra_rendertile(renderer, sprites, SPRITE_FLOOR, rect.x, rect.y, tilesize, 0);
      if (game->field[x][y] & field_wall) draw_wall(renderer, sprites, game, x, y, tilesize, rect.x, rect.y);
      if ((game->field[x][y] & field_goal) && (game->field[x][y] & field_atom)) { /* atom on goal */
        gra_rendertile(renderer, sprites, SPRITE_BOXOK, rect.x, rect.y, tilesize, 0);
      } else if (game->field[x][y] & field_goal) { /* goal */
        gra_rendertile(renderer, sprites, SPRITE_GOAL, rect.x, rect.y, tilesize, 0);
      } else if (game->field[x][y] & field_atom) { /* atom */
        gra_rendertile(renderer, sprites, SPRITE_BOX, rect.x, rect.y, tilesize, 0);
      }
    }
  }
  /* apply alpha filter */
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255 - alpha);
  CSDL_RenderFillRect(renderer, &bgrect);
  /* if background enabled, then draw the border */
  if (flags & BLIT_LEVELMAP_BACKGROUND) {
    unsigned char fadealpha;
    SDL_SetRenderDrawColor(renderer, 0x28, 0x28, 0x28, 255);
    CSDL_RenderRect(renderer, &bgrect);
    /* draw a nice fade-out effect around the selected level */
    for (fadealpha = 1; fadealpha < 20; fadealpha++) {
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255 - fadealpha * (255 / 20));
      bgrect.x -= 1;
      bgrect.y -= 1;
      bgrect.w += 2;
      bgrect.h += 2;
      CSDL_RenderRect(renderer, &bgrect);
    }
    /* set the drawing color to its default, plain black color */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  }
  /* if level is solved, draw a 'complete' tag */
  if (game->solution != NULL) {
    /* SDL_Rect rect; */
    CSDL_QueryTexture(sprites->solved, NULL, NULL, &rect.w, &rect.h);
    rect.w = rect.w * sprites->em / 60;
    rect.h = rect.h * sprites->em / 60;
    rect.x = xpos - (rect.w / 2);
    rect.y = ypos - (rect.h * 3 / 4);
    CSDL_RenderTexture(renderer, sprites->solved, NULL, &rect);
  }
}

static int fade2texture(SDL_Renderer *renderer, SDL_Window *window, SDL_Texture *texture) {
  int exitflag = 0;
  unsigned char alphaval;

  for (alphaval = 0; alphaval < 64; alphaval += 4) {
    exitflag = displaytexture(renderer, texture, window, 0, 0, alphaval);
    if (exitflag != 0) break;
    SDL_Delay(15);  /* wait for 15ms */
  }

  if (exitflag == 0) exitflag = displaytexture(renderer, texture, window, 0, 0, 255);
  return(exitflag);
}

static int selectlevel(struct sokgame **gameslist, struct spritesstruct *sprites, SDL_Renderer *renderer, SDL_Window *window, struct videosettings *settings, const char *levcomment, int levelscount, int selection, char **levelfile) {
  int i, winw, winh, maxallowedlevel;
  char levelnum[64];
  SDL_Event event;
  /* reload all solutions for levels, in case they changed (for ex. because we just solved a level..) */
  sok_loadsolutions(gameslist, levelscount);

  /* if no current level is selected, then preselect the first unsolved level */
  if (selection < 0) {
    for (i = 0; i < levelscount; i++) {
      if (gameslist[i]->solution != NULL) {
        if (debugmode != 0) printf("Level %d [%016" PRIx64 "] has solution: %s\n", i + 1, gameslist[i]->crc64, gameslist[i]->solution);
      } else {
        if (debugmode != 0) printf("Level %d [%16" PRIx64 "] has NO solution\n", i + 1, gameslist[i]->crc64);
        selection = i;
        break;
      }
    }
  }

  /* if no unsolved level found, then select the first one */
  if (selection < 0) selection = 0;

  /* compute the last allowed level */
  i = 0; /* i will temporarily store the number of unsolved levels */
  for (maxallowedlevel = 0; maxallowedlevel < levelscount; maxallowedlevel++) {
    if (gameslist[maxallowedlevel]->solution == NULL) i++;
    if (i > 3) break; /* user can see up to 3 unsolved levels */
  }

  /* loop */
  for (;;) {
    SDL_GetWindowSize(window, &winw, &winh);

    /* draw the screen
     * when drawing levelmaps make sure that the tilesize is an even number,
     * otherwise glitches will appear between tile boundaries */
    SDL_RenderClear(renderer);

    /* draw the level before */
    if (selection > 0) blit_levelmap(gameslist[selection - 1], sprites, winw / 5, winh / 2, renderer, (settings->tilesize / 4) & 254, 96, 0);

    /* draw the level after */
    if (selection + 1 < maxallowedlevel) blit_levelmap(gameslist[selection + 1], sprites, winw * 4 / 5,  winh / 2, renderer, (settings->tilesize / 4) & 254, 96, 0);

    /* draw the selected level */
    blit_levelmap(gameslist[selection], sprites,  winw / 2,  winh / 2, renderer, (settings->tilesize / 3) & 254, 210, BLIT_LEVELMAP_BACKGROUND);

    /* draw strings, etc */
    draw_string(levcomment, 100, 255, sprites, renderer, DRAWSTRING_CENTER, winh / 8, window, 1, 0);
    draw_string("(choose a level)", 100, 255, sprites, renderer, DRAWSTRING_CENTER, winh / 8 + 40, window, 1, 0);
    sprintf(levelnum, "Level %d of %d", selection + 1, levelscount);
    draw_string(levelnum, 100, 255, sprites, renderer, DRAWSTRING_CENTER, winh * 3 / 4, window, 1, 0);

    /* if level has a comment then display it, too (between quotes) */
    if (strlen(gameslist[selection]->comment) > 4) {
      char buf[sizeof(gameslist[0]->comment) + 2];
      sprintf(buf, "\"%s\"", gameslist[selection]->comment);
      draw_string(buf, 80, 255, sprites, renderer, DRAWSTRING_CENTER, winh * 3 / 4 + 50, window, 1, 0);
    }

    SDL_RenderPresent(renderer);

    /* Wait for an event - but ignore 'KEYUP' and 'MOUSEMOTION' events, since they are worthless in this game */
    for (;;)
      if (SDL_WaitEvent(&event) != 0 && event.type != CSDL_EVENT_KEY_UP &&
	  event.type != CSDL_EVENT_MOUSE_MOTION)
	break;

    /* check what event we got */
    if (event.type == CSDL_EVENT_QUIT) {
      return(SELECTLEVEL_QUIT);
    } else if (event.type == CSDL_EVENT_DROP_FILE) {
      if (processDropFileEvent(&event, levelfile) != NULL) {
        fade2texture(renderer, window, sprites->black);
        return(SELECTLEVEL_LOADFILE);
      }
    } else if (event.type == CSDL_EVENT_KEY_DOWN) {
      switch (normalizekeys(CSDL_KEY_SYM(event.key))) {
        case KEY_LEFT:
          if (selection > 0) selection--;
          break;
        case KEY_RIGHT:
          if (selection + 1 < maxallowedlevel) selection++;
          break;
        case KEY_HOME:
          selection = 0;
          break;
        case KEY_END:
          selection = maxallowedlevel - 1;
          break;
        case KEY_PAGEUP:
          if (selection < 3) {
            selection = 0;
          } else {
            selection -= 3;
          }
          break;
        case KEY_PAGEDOWN:
          if (selection + 3 >= maxallowedlevel) {
            selection = maxallowedlevel - 1;
          } else {
            selection += 3;
          }
          break;
        case KEY_CTRL_UP:
          if (settings->tilesize < 255) settings->tilesize += 8;
          break;
        case KEY_CTRL_DOWN:
          if (settings->tilesize > 10) settings->tilesize -= 8;
          break;
        case KEY_ENTER:
          return(selection);
        case KEY_FULLSCREEN:
          switchfullscreen(window);
          break;
        case KEY_ESCAPE:
          fade2texture(renderer, window, sprites->black);
          return(SELECTLEVEL_BACK);
      }
    }
  }
}

/* sets the icon in the aplication's title bar */
static void setsokicon(SDL_Window *window) {
  SDL_Surface *surface;
  void *bmp;
  size_t bmplen;
  CSDL_IOStream *stream;

  bmp = ungz(assets_icon_bmp_gz, assets_icon_bmp_gz_len, &bmplen);
  if (bmp == NULL) return;
  stream = CSDL_IOFromMem(bmp, (int)bmplen);

  surface = CSDL_LoadBMP_IO(stream, 0);
  free(bmp);
  CSDL_CloseIO(stream);
  if (surface == NULL) return;
  SDL_SetWindowIcon(window, surface);
  CSDL_DestroySurface(surface); /* once the icon is loaded, the surface is not needed anymore */
}

/* returns 1 if curlevel is the last level to solve in the set. returns 0 otherwise. */
static int islevelthelastleft(struct sokgame **gamelist, int curlevel, int levelscount) {
  int x;
  if (curlevel < 0) return(0);
  if (gamelist[curlevel]->solution != NULL) return(0);
  for (x = 0; x < levelscount; x++) {
    if ((gamelist[x]->solution == NULL) && (x != curlevel)) return(0);
  }
  return(1);
}

static void dumplevel2clipboard(struct sokgame *game, char *history) {
  char *txt;
  unsigned long solutionlen = 0, playfieldsize;
  int x, y;
  if (game->solution != NULL) solutionlen = strlen(game->solution);
  playfieldsize = (game->field_width + 1) * game->field_height;
  txt = malloc(solutionlen + playfieldsize + 4096);
  if (txt == NULL) return;
  sprintf(txt, "; Level id: %016" PRIx64 "\n\n", game->crc64);
  for (y = 0; y < game->field_height; y++) {
    for (x = 0; x < game->field_width; x++) {
      switch (game->field[x][y] & ~field_floor) {
        case field_wall:
          strcat(txt, "#");
          break;
        case (field_atom | field_goal):
          strcat(txt, "*");
          break;
        case field_atom:
          strcat(txt, "$");
          break;
        case field_goal:
          if ((game->positionx == x) && (game->positiony == y)) {
            strcat(txt, "+");
          } else {
            strcat(txt, ".");
          }
          break;
        default:
          if ((game->positionx == x) && (game->positiony == y)) {
            strcat(txt, "@");
          } else {
            strcat(txt, " ");
          }
          break;
      }
    }
    strcat(txt, "\n");
  }
  strcat(txt, "\n");
  if ((history != NULL) && (history[0] != 0)) { /* only allow if there actually is a solution */
    strcat(txt, "; Solution\n; ");
    strcat(txt, history);
    strcat(txt, "\n");
  } else {
    strcat(txt, "; No solution available\n");
  }
  SDL_SetClipboardText(txt);
  free(txt);
}

/* reads a chunk of text from memory. returns the line in a chunk of memory that needs to be freed afterwards */
static char *readmemline(char **memptrptr) {
  unsigned long linelen;
  char *res;
  char *memptr;
  memptr = *memptrptr;
  if (*memptr == 0) return(NULL);
  /* check how long the line is */
  for (linelen = 0; ; linelen += 1) {
    if (memptr[linelen] == 0) break;
    if (memptr[linelen] == '\n') break;
  }
  /* allocate memory for the line, and copy its content */
  res = malloc(linelen + 1);
  memcpy(res, memptr, linelen);
  /* move the original pointer forward */
  *memptrptr += linelen;
  if (**memptrptr == '\n') *memptrptr += 1;
  /* trim out trailing \r, if any */
  if ((linelen > 0) && (res[linelen - 1] == '\r')) linelen -= 1;
  /* terminate the line with a null terminator */
  res[linelen] = 0;
  return(res);
}

static void fetchtoken(char *res, char *buf, int pos) {
  int tokpos = 0, x;
  /* forward to the position where the token starts */
  while (tokpos != pos) {
    if (*buf == 0) {
      break;
    } else if (*buf == '\t') {
      tokpos += 1;
    }
    buf += 1;
  }
  /* copy the token to buf, until \t or \0 */
  for (x = 0;; x++) {
    if (buf[x] == '\0') break;
    if (buf[x] == '\t') break;
    res[x] = buf[x];
  }
  res[x] = 0;
}


/* select skin menu, returns -1 if application should quit immediately, zero otherwise */
static int selectskinmenu(SDL_Renderer *renderer, SDL_Window *window, struct spritesstruct *sprites, struct videosettings *settings) {
  struct skinlist *list, *node;
  int choice = 0, i;
  const char *skinlist[64];

  list = skin_list();

  i = 0;
  for (node = list; node != NULL; node = node->next) {
    skinlist[i] = node->name;
    if ((settings->customskinfile != NULL) && (strcmp(settings->customskinfile, node->name) == 0)) choice = i;
    i++;
  }
  skinlist[i] = NULL; /* list terminator */

  choice = menu(renderer, sprites, window, settings, skinlist, 100, choice, NULL);
  if (choice >= 0) {
    printf("selected skin: %s\n", skinlist[choice]);
    setconf_skin(skinlist[choice]);
    settings->customskinfile = NULL;
  }
  skin_list_free(list);

  if (choice == -10) return(-1);
  return(0);
}


static int selectinternetlevel(SDL_Renderer *renderer, SDL_Window *window, struct spritesstruct *sprites, const char *host, unsigned short port, const char *path, char *levelslist, unsigned char **xsbptr, size_t *reslen) {
  unsigned char *res = NULL;
  char url[2048], buff[1200], buff2[1024];
  char *inetlist[1024];
  int inetlistlen = 0, i, selected = 0, windowrows, fontheight = 24, winw, winh;
  static int selection = 0, seloffset = 0;
  SDL_Event event;
  *xsbptr = NULL;
  *reslen = 0;
  /* load levelslist into an array */
  for (;;) {
    inetlist[inetlistlen] = readmemline(&levelslist);
    if (inetlist[inetlistlen] == NULL) break;
    inetlistlen += 1;
    if (inetlistlen >= 1024) break;
  }
  if (inetlistlen < 1) return(SELECTLEVEL_BACK); /* if failed to load any level, quit here */
  /* selection loop */
  for (;;) {
    SDL_Rect rect;
    /* compute the amount of rows we can fit onscreen */
    SDL_GetWindowSize(window, &winw, &winh);
    windowrows = (winh / fontheight) - 7;
    /* display the list of levels */
    SDL_RenderClear(renderer);
    for (i = 0; i < windowrows; i++) {
      if (i + seloffset >= inetlistlen) break;
      fetchtoken(buff, inetlist[i + seloffset], 1);
      draw_string(buff, 100, 255, sprites, renderer, 30, i * fontheight, window, 1, 0);
      if (i + seloffset == selection) {
        int angle = 0;
        int player_cursor = SPRITE_PLAYERRIGHT;
        if (sprites->flags & SPRITES_FLAG_PLAYERROTATE) {
          player_cursor = SPRITE_PLAYERUP;
          angle = 90;
        }
        gra_rendertile(renderer, sprites, player_cursor, 0, i * fontheight, 30, angle);
      }
    }
    /* render background of level description */
    rect.x = 0;
    rect.y = (windowrows * fontheight) + (fontheight * 4 / 10);
    rect.w = winw;
    rect.h = winh;
    SDL_SetRenderDrawColor(renderer, 0x30, 0x30, 0x30, 255);
    CSDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 0xC0, 0xC0, 0xC0, 255);
    CSDL_RenderLine(renderer, 0, rect.y, winw, rect.y);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    /* draw level description */
    rect.y += fontheight / 2;
    fetchtoken(buff2, inetlist[selection], 1);
    draw_string(buff2, 100, 250, sprites, renderer, DRAWSTRING_CENTER, rect.y, window, 1, 0);
    fetchtoken(buff2, inetlist[selection], 2);
    sprintf(buff, "Copyright (C) %s", buff2);
    draw_string(buff, 65, 200, sprites, renderer, DRAWSTRING_CENTER, rect.y + (fontheight * 12 / 10), window, 1, 0);
    fetchtoken(buff, inetlist[selection], 3);
    draw_string(buff, 100, 210, sprites, renderer, 0, rect.y + (fontheight * 26 / 10), window, 3, fontheight);
    /* refresh screen */
    SDL_RenderPresent(renderer);
    /* Wait for an event - but ignore 'KEYUP' and 'MOUSEMOTION' events, since they are worthless in this game */
    for (;;) {
      SDL_WaitEvent(&event);
      if (event.type != CSDL_EVENT_KEY_UP &&
	  event.type != CSDL_EVENT_MOUSE_MOTION)
	break;
    }
    /* check what event we got */
    if (event.type == CSDL_EVENT_QUIT) {
        selected = SELECTLEVEL_QUIT;
      /* } else if (event.type == CSDL_EVENT_DROP_FILE) {
        if (processDropFileEvent(&event, &levelfile) != NULL) {
          fade2texture(renderer, window, sprites->black);
          goto GametypeSelectMenu;
        } */
      } else if (event.type == CSDL_EVENT_KEY_DOWN) {
        switch (normalizekeys(CSDL_KEY_SYM(event.key))) {
          case KEY_UP:
            if (selection > 0) selection -= 1;
            if ((seloffset > 0) && (selection < seloffset + 2)) seloffset -= 1;
            break;
          case KEY_DOWN:
            if (selection + 1 < inetlistlen) selection += 1;
            if ((seloffset < inetlistlen - windowrows) && (selection >= seloffset + windowrows - 2)) seloffset += 1;
            break;
          case KEY_ENTER:
            selected = SELECTLEVEL_OK;
            break;
          case KEY_ESCAPE:
            selected = SELECTLEVEL_BACK;
            break;
          case KEY_FULLSCREEN:
            switchfullscreen(window);
            break;
          case KEY_HOME:
            selection = 0;
            seloffset = 0;
            break;
          case KEY_END:
            selection = inetlistlen - 1;
            seloffset = inetlistlen - windowrows;
            break;
        }
    }
    if (selected != 0) break;
  }
  /* fetch the selected level */
  if (selected == SELECTLEVEL_OK) {
    fetchtoken(buff, inetlist[selection], 0);
    sprintf(url, "%s%s", path, buff);
    *reslen = http_get(host, port, url, &res);
    *xsbptr = res;
  } else {
    *xsbptr = NULL;
  }
  /* free the list */
  while (inetlistlen > 0) {
    inetlistlen -= 1;
    free(inetlist[inetlistlen]);
  }
  fade2texture(renderer, window, sprites->black);
  return(selected);
}


/* returns a tilesize that is more or less consistent with the in-game fonts */
static unsigned short auto_tilesize(const struct spritesstruct *spr) {
  unsigned short tilesize;

  tilesize = (spr->em + 1) * 3 / 2;

  /* drop the lowest bit so tilesize is guaranteed to be even */
  tilesize >>= 1;
  tilesize <<= 1;
  /* add a bit if em native tilesize is odd, this to make sure that the user
   * is able to zoom in/out to the native tile resolution (zooming is
   * performed by increments of 2) */
  tilesize |= (spr->tilesize & 1);

  return(tilesize);
}


static void list_installed_skins(void) {
  struct skinlist *list, *node;
  puts("List of installed skins:");
  list = skin_list();
  if (list == NULL) puts("no skins found");
  for (node = list; node != NULL; node = node->next) {
    printf("%-16s (%s)\n", node->name, node->path);
  }
  skin_list_free(list);
}


static int parse_cmdline(struct videosettings *settings, int argc, char **argv, char **levelfile) {
  /* pre-set a few default settings */
  memset(settings, 0, sizeof(*settings));
  settings->rotspeed = -1;
  settings->movspeed = -1;

  /* parse the commandline */
  if (argc > 1) {
    int i;
    for (i = 1 ; i < argc ; i++) {
      if (strstr(argv[i], "--movspeed=") == argv[i]) {
        settings->movspeed = atoi(argv[i] + strlen("--movspeed="));
      } else if (strstr(argv[i], "--rotspeed=") == argv[i]) {
        settings->rotspeed = atoi(argv[i] + strlen("--rotspeed="));
      } else if (strstr(argv[i], "--skin=") == argv[i]) {
        settings->customskinfile = argv[i] + strlen("--skin=");
      } else if (strcmp(argv[i], "--skinlist") == 0) {
        list_installed_skins();
        return(1);
      } else if ((*levelfile == NULL) && (argv[i][0] != '-')) { /* else assume it is a level file */
        *levelfile = strdup(argv[i]);
      } else { /* invalid argument */
        puts("Simple Sokoban ver " PACKAGE_VERSION);
        puts("Copyright (C) " PDATE " Mateusz Viste");
        puts("");
        puts("usage: simplesok [options] [levelfile.xsb]");
        puts("");
        puts("options:");
        puts(" --movspeed=n   player's moving speed (1..100, 1=slowest 100=instant default=22)");
        puts(" --rotspeed=n   player's rotation speed (1..100, default=22)");
        puts(" --skin=name    skin name to be used (default: antique3)");
        puts(" --skinlist     display the list of installed skins");
        puts("");
        puts("Skin files can be located in the following directories:");
        puts(" * a skins/ subdirectory in SimpleSok's user directory");
        puts(" * a skins/ subdirectory in SimpleSok's application directory");
        puts(" * " PKGDATADIR "/skins/");
        puts("");
        puts("If skin loading fails, then a default (embedded) skin is used.");
        puts("");
        puts("homepage: http://simplesok.sourceforge.net");
        return(1);
      }
    }
  }
  return(0);
}


/* process playback */
static void process_autoplayback(enum SOKMOVE *movedir, int *playsolution, const char *playsource) {

  switch (playsource[*playsolution - 1]) {
    case 'u':
    case 'U':
      *movedir = sokmoveUP;
      break;
    case 'r':
    case 'R':
      *movedir = sokmoveRIGHT;
      break;
    case 'd':
    case 'D':
      *movedir = sokmoveDOWN;
      break;
    case 'l':
    case 'L':
      *movedir = sokmoveLEFT;
      break;
    default:
      *movedir = sokmoveNONE;
  }
  (*playsolution)++;
  if (playsource[*playsolution - 1] == 0) *playsolution = 0;
}


int main(int argc, char **argv) {
  struct sokgame **gameslist, game;
  struct sokgamestates *states;
  struct spritesstruct *sprites;
  int levelscount, curlevel, exitflag = 0, showhelp = 0, lastlevelleft = 0;
  int playsolution, drawscreenflags;
  int autoplay = 0;
  char *levelfile = NULL;
  char *playsource = NULL;
  char *levelslist = NULL;
  #define LEVCOMMENTMAXLEN 32
  char levcomment[LEVCOMMENTMAXLEN];
  struct videosettings settings;
  unsigned char *xsblevelptr = NULL;
  size_t xsblevelptrlen = 0;
  enum leveltype levelsource = LEVEL_INTERNAL;

  SDL_Window* window = NULL;
  SDL_Renderer *renderer;
  SDL_Event event;

  exitflag = parse_cmdline(&settings, argc, argv, &levelfile);
  if (exitflag != 0) return(1);

  /* init networking stack (required on windows) */
  init_net();

  /* Init SDL and set the video mode */
  if (CSDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("SDL_Init() failed: %s\n", SDL_GetError());
    return(1);
  }
  /* set SDL scaling algorithm to "nearest neighbor" so there is no bleeding
   * of textures. */
  CSDL_LinearScalingHint();

  window = CSDL_CreateWindow("Simple Sokoban " PACKAGE_VERSION,
                            SCREEN_DEFAULT_WIDTH, SCREEN_DEFAULT_HEIGHT,
                            SDL_WINDOW_RESIZABLE);
  if (window == NULL) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return(1);
  }

  setsokicon(window);

  renderer = CSDL_CreateRenderer(window, WITH_SOFTWARE_RENDERER);
  if (renderer == NULL) {
    SDL_DestroyWindow(window);
    printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
    return(1);
  }

  SDL_SetWindowMinimumSize(window, 600, 400);

  LoadSprites:

  /* if no command-line skin provided, look for a default configuration */
  if (settings.customskinfile == NULL) {
    settings.customskinfile = loadconf_skin();
  }

  /* if still no skin, then use the default one */
  if (settings.customskinfile == NULL) {
    settings.customskinfile = DEFAULT_SKIN;
  }

  /* Load sprites */
  sprites = skin_load(settings.customskinfile, renderer);
  if (sprites == NULL) return(1);
  printf("loaded skin appears to have tiles %d pixels wide\n", sprites->tilesize);
  if (sprites->flags & SPRITES_FLAG_PRIMITIVE) {
    puts("\nNOTICE: this skin is primitive (no transparency found on player's sprite)\nthus it is unsuitable for animated movements. ALL ANIMATIONS DISABLED!\n");
  }

  /* Hide the mouse cursor, disable mouse events and make sure DropEvents are enabled (sometimes they are not) */
  CSDL_ShowCursor(0);
  CSDL_SetEventEnabled(CSDL_EVENT_MOUSE_MOTION, 0);
  CSDL_SetEventEnabled(CSDL_EVENT_DROP_FILE, 1);

  /* validate parameters */
  if ((settings.movspeed < 1) || (settings.movspeed > 100)) settings.movspeed = 22;
  if ((settings.rotspeed < 1) || (settings.rotspeed > 100)) settings.rotspeed = 22;

  gameslist = malloc(sizeof(struct sokgame *) * MAXLEVELS);
  if (gameslist == NULL) {
    puts("Memory allocation failed!");
    return(1);
  }

  states = sok_newstates();
  if (states == NULL) return(1);

  GametypeSelectMenu:
  if (levelslist != NULL) {
    free(levelslist);
    levelslist = NULL;
  }
  curlevel = -1;
  levelscount = -1;
  settings.tilesize = auto_tilesize(sprites);
  if (levelfile != NULL) goto LoadLevelFile;
  xsblevelptr = selectgametype(renderer, sprites, window, &settings, &levelfile, &xsblevelptrlen);
  levelsource = LEVEL_INTERNAL;
  if ((xsblevelptrlen == 0) && (xsblevelptr != NULL)) {
    if (*xsblevelptr == '@') {
      levelsource = LEVEL_INTERNET;
    } else if (*xsblevelptr == '#') { /* config */
      if (selectskinmenu(renderer, window, sprites, &settings) == -1) {
        /* quit application (window closed) */
        exitflag = 1;
      } else {
        skin_free(sprites);
        goto LoadSprites;
      }
    }
  }
  if (exitflag == 0) fade2texture(renderer, window, sprites->black);

  LoadInternetLevels:
  if (levelsource == LEVEL_INTERNET) { /* internet levels */
    int selectres;
    size_t httpres;
    httpres = http_get(INET_HOST, INET_PORT, INET_PATH, (unsigned char **) &levelslist);
    if ((httpres == 0) || (levelslist == NULL)) {
      SDL_RenderClear(renderer);
      draw_string("Failed to fetch internet levels!", 100, 255, sprites, renderer, DRAWSTRING_CENTER, DRAWSTRING_CENTER, window, 1, 0);
      wait_for_a_key(-1, renderer);
      goto GametypeSelectMenu;
    }
    selectres = selectinternetlevel(renderer, window, sprites, INET_HOST, INET_PORT, INET_PATH, levelslist, &xsblevelptr, &xsblevelptrlen);
    if (selectres == SELECTLEVEL_BACK) goto GametypeSelectMenu;
    if (selectres == SELECTLEVEL_QUIT) exitflag = 1;
    if (exitflag == 0) fade2texture(renderer, window, sprites->black);
  } else if ((xsblevelptr == NULL) && (levelfile == NULL)) { /* nothing */
    exitflag = 1;
  }

  LoadLevelFile:
  if ((levelfile != NULL) && (exitflag == 0)) {
    levelscount = sok_loadfile(gameslist, MAXLEVELS, levelfile, NULL, 0, levcomment, LEVCOMMENTMAXLEN);
  } else if (exitflag == 0) {
    levelscount = sok_loadfile(gameslist, MAXLEVELS, NULL, xsblevelptr, xsblevelptrlen, levcomment, LEVCOMMENTMAXLEN);
  }

  if ((levelscount < 1) && (exitflag == 0)) {
    SDL_RenderClear(renderer);
    printf("Failed to load the level file [%d]: %s\n", levelscount, sok_strerr(levelscount));
    draw_string("Failed to load the level file!", 100, 255, sprites, renderer, DRAWSTRING_CENTER, DRAWSTRING_CENTER, window, 1, 0);
    wait_for_a_key(-1, renderer);
    exitflag = 1;
  }

  /* printf("Loaded %d levels '%s'\n", levelscount, levcomment); */

  LevelSelectMenu:
  settings.tilesize = auto_tilesize(sprites);
  if (exitflag == 0) exitflag = flush_events();

  if (exitflag == 0) {
    curlevel = selectlevel(gameslist, sprites, renderer, window, &settings, levcomment, levelscount, curlevel, &levelfile);
    if (curlevel == SELECTLEVEL_BACK) {
      if (levelfile == NULL) {
        if (levelsource == LEVEL_INTERNET) goto LoadInternetLevels;
        goto GametypeSelectMenu;
      } else {
        exitflag = 1;
      }
    } else if (curlevel == SELECTLEVEL_QUIT) {
      exitflag = 1;
    } else if (curlevel == SELECTLEVEL_LOADFILE) {
      goto GametypeSelectMenu;
    }
  }
  if (exitflag == 0) fade2texture(renderer, window, sprites->black);
  if (exitflag == 0) loadlevel(&game, gameslist[curlevel], states);

  /* here we start the actual game */

  settings.tilesize = auto_tilesize(sprites);
  if ((curlevel == 0) && (game.solution == NULL)) showhelp = 1;
  playsolution = 0;
  drawscreenflags = 0;
  if (exitflag == 0) lastlevelleft = islevelthelastleft(gameslist, curlevel, levelscount);

  while (exitflag == 0) {
    if (playsolution > 0) {
      drawscreenflags |= DRAWSCREEN_PLAYBACK;
    } else {
      drawscreenflags &= ~DRAWSCREEN_PLAYBACK;
    }
    draw_screen(&game, states, sprites, renderer, window, &settings, 0, 0, 0, DRAWSCREEN_REFRESH | drawscreenflags, levcomment);
    if (showhelp != 0) {
      exitflag = displaytexture(renderer, sprites->help, window, -1, DISPLAYCENTERED, 255);
      draw_screen(&game, states, sprites, renderer, window, &settings, 0, 0, 0, DRAWSCREEN_REFRESH | drawscreenflags, levcomment);
      showhelp = 0;
    }
    if (debugmode != 0) printf("history: %s\n", states->history);

    /* Wait for an event - but ignore 'KEYUP' and 'MOUSEMOTION' events, since they are worthless in this game */
    for (;;) {
      if (SDL_WaitEventTimeout(&event, 80) == 0) {
        if (playsolution == 0) continue;
        event.type = CSDL_EVENT_KEY_DOWN;
        CSDL_KEY_SYM(event.key) = SDLK_F10;
      }
      if (event.type != CSDL_EVENT_KEY_UP &&
	  event.type != CSDL_EVENT_MOUSE_MOTION)
	break;
    }

    /* check what event we got */
    if (event.type == CSDL_EVENT_QUIT) {
      exitflag = 1;
    } else if (event.type == CSDL_EVENT_DROP_FILE) {
      if (processDropFileEvent(&event, &levelfile) != NULL) {
        fade2texture(renderer, window, sprites->black);
        goto GametypeSelectMenu;
      }
    } else if (event.type == CSDL_EVENT_KEY_DOWN) {
      int res = 0;
      enum SOKMOVE movedir = sokmoveNONE;
      switch (normalizekeys(CSDL_KEY_SYM(event.key))) {
        case KEY_LEFT:
          if (playsolution == 0) movedir = sokmoveLEFT;
          break;
        case KEY_RIGHT:
          if (playsolution == 0) movedir = sokmoveRIGHT;
          break;
        case KEY_UP:
          if (playsolution == 0) movedir = sokmoveUP;
          break;
        case KEY_CTRL_UP:
          if (settings.tilesize < 255) settings.tilesize += 2;
          break;
        case KEY_DOWN:
          if (playsolution == 0) movedir = sokmoveDOWN;
          break;
        case KEY_CTRL_DOWN:
          if (settings.tilesize > 4) settings.tilesize -= 2;
          break;
        case KEY_BACKSPACE:
          if (autoplay == 0) {
            sok_undo(&game, states);
            if (playsolution > 1) playsolution--;
          } else {
            autoplay = 0;
          }
          break;
        case KEY_R:
          playsolution = 0;
          loadlevel(&game, gameslist[curlevel], states);
          break;
        case KEY_F3: /* dump level & solution (if any) to clipboard */
          dumplevel2clipboard(gameslist[curlevel], gameslist[curlevel]->solution);
          exitflag = displaytexture(renderer, sprites->copiedtoclipboard, window, 2, DISPLAYCENTERED, 255);
          break;
        case KEY_CTRL_C:
          dumplevel2clipboard(&game, states->history);
          exitflag = displaytexture(renderer, sprites->snapshottoclipboard, window, 2, DISPLAYCENTERED, 255);
          break;
        case KEY_CTRL_V:
          {
          char *solFromClipboard;
          solFromClipboard = SDL_GetClipboardText();
          if (debugmode) {
            printf("CTRL+V: got %lu bytes from clipboard\n", (unsigned long)strlen(solFromClipboard));
            if (*solFromClipboard == 0) puts(SDL_GetError());
          }
          trimstr(solFromClipboard);
          if (isLegalSokoSolution(solFromClipboard) != 0) {
            loadlevel(&game, gameslist[curlevel], states);
            exitflag = displaytexture(renderer, sprites->playfromclipboard, window, 2, DISPLAYCENTERED, 255);
            playsolution = 1;
            autoplay = 1;
            if (playsource != NULL) free(playsource);
            playsource = unRLE(solFromClipboard);
          }
          if (solFromClipboard != NULL) free(solFromClipboard);
          }
          break;
        case KEY_S:
          if (playsolution == 0) {
            if (game.solution != NULL) { /* only allow if there actually is a solution */
              if (playsource != NULL) free(playsource);
              playsource = unRLE(game.solution); /* I duplicate the solution string, because I want to free it later, since it can originate both from the game's solution as well as from a clipboard string */
              if (playsource != NULL) {
                loadlevel(&game, gameslist[curlevel], states);
                playsolution = 1;
                autoplay = 1;
              }
            } else {
              exitflag = displaytexture(renderer, sprites->nosolution, window, 1, DISPLAYCENTERED, 255);
            }
          } else {
            autoplay = 1;
          }
          break;
        case KEY_F1:
          showhelp = 1;
          break;
        case KEY_F2:
          if ((drawscreenflags & DRAWSCREEN_NOBG) && (drawscreenflags & DRAWSCREEN_NOTXT)) {
            drawscreenflags &= ~(DRAWSCREEN_NOBG | DRAWSCREEN_NOTXT);
          } else if (drawscreenflags & DRAWSCREEN_NOBG) {
            drawscreenflags |= DRAWSCREEN_NOTXT;
          } else if (drawscreenflags & DRAWSCREEN_NOTXT) {
            drawscreenflags &= ~DRAWSCREEN_NOTXT;
            drawscreenflags |= DRAWSCREEN_NOBG;
          } else {
            drawscreenflags |= DRAWSCREEN_NOTXT;
          }
          break;
        case KEY_F5:
          if (playsolution == 0) {
            exitflag = displaytexture(renderer, sprites->saved, window, 1, DISPLAYCENTERED, 255);
            solution_save(game.crc64, states->history, "sav");
          }
          break;
        case KEY_F7:
          {
          char *loadsol;
          loadsol = solution_load(game.crc64, "sav");
          if (loadsol == NULL) {
            exitflag = displaytexture(renderer, sprites->nosave, window, 1, DISPLAYCENTERED, 255);
          } else {
            exitflag = displaytexture(renderer, sprites->loaded, window, 1, DISPLAYCENTERED, 255);
            playsolution = 0;
            loadlevel(&game, gameslist[curlevel], states);
            sok_play(&game, states, loadsol);
            free(loadsol);
          }
          }
          break;
        case KEY_FULLSCREEN:
          switchfullscreen(window);
          break;
        case KEY_ESCAPE:
          fade2texture(renderer, window, sprites->black);
          goto LevelSelectMenu;
      }

      /* if playback is ongoing then process it now (and overwrite movedir) */
      if ((playsolution > 0) && (autoplay != 0)) {
        SDL_Delay(300); /* make sure autoplay does not run too fast */
        process_autoplayback(&movedir, &playsolution, playsource);
      }

      if (movedir != sokmoveNONE) {
        if (sprites->flags & SPRITES_FLAG_PLAYERROTATE) rotatePlayer(sprites, &game, states, movedir, renderer, window, &settings, levcomment, drawscreenflags);
        res = sok_move(&game, movedir, 1, states);

        /* do animations (unless movspeed is set to instant speed or skin is primitive) */
        if ((res >= 0) && (settings.movspeed < 100) && ((sprites->flags & SPRITES_FLAG_PRIMITIVE) == 0)) {
          int offset, vectorx = 0, vectory = 0, scrollflag, step;
          if (res & sokmove_pushed) drawscreenflags |= DRAWSCREEN_PUSH;

          /* How do I need to move? */
          if (movedir == sokmoveUP) vectory = -1;
          if (movedir == sokmoveRIGHT) vectorx = 1;
          if (movedir == sokmoveDOWN) vectory = 1;
          if (movedir == sokmoveLEFT) vectorx = -1;

          /* what's my per-screen-refresh step? */
          step = (settings.tilesize * settings.movspeed) / 100;
          if (step == 0) step = 1;

          /* Do I need to move the player, or the entire field? */
          scrollflag = scrollneeded(&game, window, settings.tilesize, vectorx, vectory);

          /* moving */
          for (offset = 0; offset < settings.tilesize; offset += step) {
            draw_screen(&game, states, sprites, renderer, window, &settings, offset * vectorx, offset * vectory, scrollflag, DRAWSCREEN_REFRESH | drawscreenflags, levcomment);

            /* wait for screen refresh time */
            while (sok_isitrefreshtime() == 0) SDL_Delay(1);
          }
        }

        res = sok_move(&game, movedir, 0, states);
        if ((res >= 0) && (res & sokmove_solved)) {
          unsigned short alphaval;
          SDL_Texture *tmptex;
          /* display a congrats message */
          if (lastlevelleft != 0) {
            tmptex = sprites->congrats;
          } else {
            tmptex = sprites->cleared;
          }
          flush_events();
          for (alphaval = 0; alphaval < 255; alphaval += 30) {
            draw_screen(&game, states, sprites, renderer, window, &settings, 0, 0, 0, drawscreenflags, levcomment);
            exitflag = displaytexture(renderer, tmptex, window, 0, DISPLAYCENTERED, (unsigned char)alphaval);
            SDL_Delay(25);
            if (exitflag != 0) break;
          }
          if (exitflag == 0) {
            draw_screen(&game, states, sprites, renderer, window, &settings, 0, 0, 0, drawscreenflags, levcomment);
            /* if this was the last level left, display a congrats screen */
            if (lastlevelleft != 0) {
              exitflag = displaytexture(renderer, sprites->congrats, window, 10, DISPLAYCENTERED, 255);
            } else {
              exitflag = displaytexture(renderer, sprites->cleared, window, 3, DISPLAYCENTERED, 255);
            }
            /* fade out to black */
            if (exitflag == 0) {
              fade2texture(renderer, window, sprites->black);
              exitflag = flush_events();
            }
          }
          /* load the new level and reset states */
          curlevel++; /* select next level */
          if (curlevel >= levelscount) curlevel = -1; /* if no more levels available then let selectlevel() choose either the first unsolved one, or level 0 */
          goto LevelSelectMenu;
        }
      }
      drawscreenflags &= ~DRAWSCREEN_PUSH;
    }

    if (exitflag != 0) break;
  }

  /* free the states struct */
  sok_freestates(states);

  if (levelfile != NULL) free(levelfile);

  /* free all textures */
  skin_free(sprites);

  /* clean up SDL */
  flush_events();
  SDL_DestroyWindow(window);
  SDL_Quit();

  /* Clean-up networking. */
  cleanup_net();

  return(0);
}
