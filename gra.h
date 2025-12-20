/*
 * wrappers and helper functions around graphic operations.
 *
 * Copyright (C) 2014-2025 Mateusz Viste
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

#ifndef GRA_H
#define GRA_H

#include "compat-sdl.h"

#define SPRITES_FLAG_PRIMITIVE 1 /* indicates that the sprites map is "primitive", ie. it comes with no transparency support, making it impossible to animate properly */

#define SPRITES_FLAG_PLAYERROTATE 2 /* indicates that player's sprite should be rotated (as opposed of using pre-drawn right/left/down player sprites) */

struct spritesstruct {
  SDL_Texture *bg;
  SDL_Texture *black;
  SDL_Texture *cleared;
  SDL_Texture *nosolution;
  SDL_Texture *congrats;
  SDL_Texture *copiedtoclipboard;
  SDL_Texture *playfromclipboard;
  SDL_Texture *snapshottoclipboard;
  SDL_Texture *help;
  SDL_Texture *map[4*8]; /* 4 tiles per row, 8 rows */
  SDL_Texture *saved;
  SDL_Texture *loaded;
  SDL_Texture *nosave;
  SDL_Texture *solved;
  SDL_Texture *font[256];
  unsigned short tilesize; /* width (and height) of tiles present in the sprite map */
  unsigned short em;       /* a font-related unit used to scale tiles and possibly other elements */
  unsigned short flags;
};


/* loads a gziped bmp image from memory and returns a texture */
SDL_Texture *loadgzbmp(const unsigned char *memgz, size_t memgzlen, SDL_Renderer *renderer);

/* render a tiled background over the entire screen */
void gra_renderbg(SDL_Renderer *renderer, const struct spritesstruct *spr, unsigned short id, int winw, int winh);

void gra_rendertile(SDL_Renderer *renderer, const struct spritesstruct *spr, unsigned short id, int x, int y, unsigned short tilesize, int angle);

/* same as gra_rendertile() but for one quarter of the tile (qid 0 = topleft) */
void gra_rendertilequarter(SDL_Renderer *renderer, const struct spritesstruct *spr, unsigned short id, int x, int y, unsigned short tilesize, int qid);

#endif
