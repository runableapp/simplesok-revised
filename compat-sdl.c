/*
 * SDL compatibility wrappers.
 *
 * Contributed by Patrick Monnerat in 2025.
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
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "compat-sdl.h"

#if WITH_SDL == 2

/* SDL2 wrappers. */

#else

/* SDL3 wrappers. */

static SDL_FRect *
Rect2FRect(SDL_FRect *frect, const SDL_Rect *rect)
{
  /* Convert a (possibly NULL) integer rectangle to its floating representation
   * and return its address. */

  if (!rect)
    frect = NULL;
  else {
    frect->x = rect->x;
    frect->y = rect->y;
    frect->w = rect->w;
    frect->h = rect->h;
  }

  return frect;
}

static SDL_FPoint *
Point2FPoint(SDL_FPoint *fpoint, const SDL_Point *point)
{
  /* Convert a (possibly NULL) integer point to its floating representation
   * and return its address. */

  if (!point)
    fpoint = NULL;
  else {
    fpoint->x = point->x;
    fpoint->y = point->y;
  }

  return fpoint;
}

int
CSDL_QueryTexture(SDL_Texture *texture, Uint32 *format, int *access,
		  int *w, int *h)
{
  SDL_PropertiesID props = SDL_GetTextureProperties(texture);

  /* Emulate SDL2 SDL_QueryTexture(). */

  if (!props)
    return -1;

  if (format)
    *format = SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_FORMAT_NUMBER, 0);

  if (access)
    *access = SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_ACCESS_NUMBER, 0);

  if (w)
    *w = SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_WIDTH_NUMBER, 0);

  if (h)
    *h = SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_HEIGHT_NUMBER, 0);

  return 0;
}

int
CSDL_RenderFillRect(SDL_Renderer *renderer, const SDL_Rect *rect)
{
  SDL_FRect frect;

  /* Implement integer version of SDL_RenderFillRect(). */

  return SDL_RenderFillRect(renderer, Rect2FRect(&frect, rect))? 0: -1;
}

int
CSDL_RenderRect(SDL_Renderer *renderer, const SDL_Rect *rect)
{
  SDL_FRect frect;

  /* Implement integer version of SDL_RenderRect(). */

  return SDL_RenderRect(renderer, Rect2FRect(&frect, rect))? 0: -1;
}

int
CSDL_RenderTexture(SDL_Renderer *renderer, SDL_Texture *texture,
		   const SDL_Rect *srcrect, const SDL_Rect *dstrect)
{
  SDL_FRect fsrcrect;
  SDL_FRect fdstrect;

  /* Implement integer version of SDL_RenderTexture(). */

  return SDL_RenderTexture(renderer, texture,
			   Rect2FRect(&fsrcrect, srcrect),
			   Rect2FRect(&fdstrect, dstrect))? 0: -1;
}

int
CSDL_RenderTextureRotated(SDL_Renderer *renderer, SDL_Texture *texture,
			  const SDL_Rect *srcrect, const SDL_Rect *dstrect,
			  double angle, const SDL_Point *center,
			  SDL_FlipMode flip)
{
  SDL_FRect fsrcrect;
  SDL_FRect fdstrect;
  SDL_FPoint fcenter;

  /* Implement integer version of SDL_RenderTextureRotated(). */

  return SDL_RenderTextureRotated(renderer, texture,
				  Rect2FRect(&fsrcrect, srcrect),
				  Rect2FRect(&fdstrect, dstrect), angle,
				  Point2FPoint(&fcenter, center), flip)? 0: -1;
}

int
CSDL_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
		      SDL_PixelFormat format, void *pixels, int pitch)
{
  int result = -1;
  SDL_Surface *sfc = SDL_RenderReadPixels(renderer, rect);

  /* Emulate SDL2 SDL_RenderReadPixels(). */

  if (sfc) {
    if (SDL_ConvertPixels(sfc->w, sfc->h, sfc->format, sfc->pixels, sfc->pitch,
			  format, pixels, pitch))
      result = 0;
    SDL_DestroySurface(sfc);
  }

  return result;
}

#endif
