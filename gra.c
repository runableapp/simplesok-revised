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

#include <stdlib.h>

#include "compat-sdl.h"

#include "gra.h"
#include "gz.h"
#include "skin.h"


/* loads a gziped bmp image from memory and returns a texture */
SDL_Texture *loadgzbmp(const unsigned char *memgz, size_t memgzlen, SDL_Renderer *renderer) {
  unsigned char *rawimage = (void *)memgz;
  size_t rawimagelen = memgzlen;
  CSDL_IOStream *stream;
  SDL_Texture *texture;

  /* if it's a gzip file then uncompress it first */
  if (isGz(memgz, memgzlen)) rawimage = ungz(memgz, memgzlen, &rawimagelen);

  stream = CSDL_IOFromMem(rawimage, (int)rawimagelen);
  texture = CIMG_LoadTexture_IO(renderer, stream, 0);
  CSDL_CloseIO(stream);
  if (rawimage != memgz) free(rawimage);

  return(texture);
}



/* render a tiled background over the entire screen */
void gra_renderbg(SDL_Renderer *renderer, const struct spritesstruct *spr, unsigned short id, int winw, int winh) {
  SDL_Rect dst;

  dst.w = spr->tilesize * 2;
  dst.h = spr->tilesize * 2;

  /* fill screen with tiles */
  for (dst.y = 0; dst.y < winh; dst.y += dst.h) {
    for (dst.x = 0; dst.x < winw; dst.x += dst.w) {
      CSDL_RenderTexture(renderer, spr->map[id], NULL, &dst);
    }
  }
}


void gra_rendertile(SDL_Renderer *renderer, const struct spritesstruct *spr, unsigned short id, int x, int y, unsigned short tilesize, int angle) {
  SDL_Rect dst;

  /* prep dst */
  dst.x = x;
  dst.y = y;
  dst.w = tilesize;
  dst.h = tilesize;

  /* copy the texture to screen (possibly scaled) */
  CSDL_RenderTextureRotated(renderer, spr->map[id], NULL, &dst,
			    angle, NULL, SDL_FLIP_NONE);

}


void gra_rendertilequarter(SDL_Renderer *renderer, const struct spritesstruct *spr, unsigned short id, int x, int y, unsigned short tilesize, int qid) {
  SDL_Rect src, dst;

  dst.x = x;
  dst.y = y;
  dst.w = tilesize;
  dst.h = tilesize;

  if (qid == 0) { /* top left */
    src.x = 0;
    src.y = 0;
  } else if (qid == 1) { /* top right */
    src.x = spr->tilesize / 2;
    src.y = 0;
  } else if (qid == 2) { /* bottom left */
    src.x = 0;
    src.y = spr->tilesize / 2;
  } else { /* bottom right */
    src.x = spr->tilesize / 2;
    src.y = spr->tilesize / 2;
  }

  src.w = spr->tilesize / 2;
  src.h = spr->tilesize / 2;

  /* copy the texture to screen (possibly scaled) */
  CSDL_RenderTexture(renderer, spr->map[id], &src, &dst);
}
