/*
 * loading skins
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

#ifndef SKIN_H
#define SKIN_H

#include "compat-sdl.h"

#include "gra.h"

#define SPRITE_FLOOR  0
#define SPRITE_BOX    2
#define SPRITE_GOAL   4
#define SPRITE_BOXOK  6
#define SPRITE_WALL_CORNER 8
#define SPRITE_WALL_HORIZ 9
#define SPRITE_WALL_PLAIN 10
#define SPRITE_BG     11
#define SPRITE_WALL_VERTIC 12
#define SPRITE_WALL_ISLAND 13
#define SPRITE_PLAYERUP 16
#define SPRITE_PLAYERLEFT 17
#define SPRITE_PLAYERDOWN 18
#define SPRITE_PLAYERRIGHT 19

struct skinlist {
  struct skinlist *next;
  char *name;
  char *path;
};

struct skinlist *skin_list(void);
void skin_list_free(struct skinlist *l);

struct spritesstruct *skin_load(const char *name, SDL_Renderer *renderer);
void skin_free(struct spritesstruct *skin);

#endif
