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

#ifndef COMPAT_SDL_H
#define COMPAT_SDL_H

#include "config.h"

/* Wrappers returning a completion status preserve the SDL2 convention:
 * 0 --> OK, < 0 --> error.
 * Wrapper names begin with a "C" (for "Compatibility) to avoid possible clashes
 * with the wrapped procedures.
 */

#if WITH_SDL == 2

/* SDL2 definitions and wrappers. */

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define CSDL_Init(f)	SDL_Init(f)

#define CSDL_CreateWindow(t, w, h, f) SDL_CreateWindow((t),		\
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,	\
		(w), (h), (f) | SDL_WINDOW_SHOWN)

#define CSDL_CreateRenderer(window, soft)				\
	SDL_CreateRenderer((window), -1, (soft)? SDL_RENDERER_SOFTWARE: 0)
#define CSDL_RenderFillRect(renderer, rect)				\
	SDL_RenderFillRect((renderer), (rect))
#define CSDL_RenderRect(renderer, rect)					\
	SDL_RenderDrawRect((renderer), (rect))
#define CSDL_RenderLine(renderer, x1, y1, x2, y2)			\
	SDL_RenderDrawLine((renderer), (x1), (y1), (x2), (y2))

typedef SDL_RWops	CSDL_IOStream;
#define CSDL_IOFromMem(mem, size)	SDL_RWFromMem((mem), (size))
#define CIMG_LoadTexture_IO(renderer, stream, closeio)			\
	IMG_LoadTexture_RW((renderer), (stream), (closeio))
#define CSDL_LoadBMP_IO(src, closeio)	SDL_LoadBMP_RW((src), (closeio))
#define CSDL_CloseIO(stream)	SDL_FreeRW(stream)

#define CSDL_QueryTexture(t, f, a, w, h)				\
	SDL_QueryTexture((t), (f), (a), (w), (h))

#define CSDL_RenderTexture(renderer, texture, srcrect, dstrect)		\
	SDL_RenderCopy((renderer), (texture), (srcrect), (dstrect))
#define CSDL_RenderTextureRotated(renderer, texture, srcrect, dstrect,	\
				  angle, center, flip)			\
	SDL_RenderCopyEx((renderer), (texture), (srcrect), (dstrect),	\
			 (angle), (center), (flip))

#define CSDL_RenderReadPixels(renderer, rect, format, pixels, pitch)	\
	SDL_RenderReadPixels((renderer), (rect), (format), (pixels), (pitch))

#define CSDL_FreeBasePath(p)	SDL_free((char *) p)
#define CSDL_DestroySurface(s)	SDL_FreeSurface(s)

/* Non-desktop mode not supported. */
#define CSDL_SetWindowFullscreen(w, f)					\
	SDL_SetWindowFullscreen((w), (f)? SDL_WINDOW_FULLSCREEN_DESKTOP: 0)

#define CSDL_SetEventEnabled(e, f)					\
	((void) SDL_EventState((e), (f)? SDL_ENABLE: SDL_DISABLE))

/* SDL_QUERY not supported. */
#define CSDL_ShowCursor(f)						\
	((void) SDL_ShowCursor((f)? SDL_ENABLE: SDL_DISABLE))

#define CSDL_LinearScalingHint()					\
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")

#define CSDL_DROP_FILE(d)	((d).file)

#define CSDL_KEY_SYM(k)		((k).keysym.sym)
#define CSDL_KEY_MOD(k)		((k).keysym.mod)

#define CSDL_KMOD_CTRL		KMOD_CTRL
#define CSDL_KMOD_ALT		KMOD_ALT
#define CSDL_K_C		SDLK_c
#define CSDL_K_R		SDLK_r
#define CSDL_K_S		SDLK_s
#define CSDL_K_V		SDLK_v

#define CSDL_EVENT_QUIT		SDL_QUIT
#define CSDL_EVENT_KEY_UP	SDL_KEYUP
#define CSDL_EVENT_KEY_DOWN	SDL_KEYDOWN
#define CSDL_EVENT_DROP_FILE	SDL_DROPFILE
#define CSDL_EVENT_MOUSE_MOTION	SDL_MOUSEMOTION

#else

/* SDL3 definitions and wrappers. */

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#define CSDL_Init(f)	(SDL_Init(f)? 0: -1)

#define CSDL_CreateWindow(t, w, h, f) SDL_CreateWindow((t), (w), (h), (f))

#define CSDL_CreateRenderer(window, soft)				\
	SDL_CreateRenderer((window), (soft)? SDL_SOFTWARE_RENDERER: NULL)
extern int CSDL_RenderFillRect(SDL_Renderer *renderer, const SDL_Rect *rect);
extern int CSDL_RenderRect(SDL_Renderer *renderer, const SDL_Rect *rect);
#define CSDL_RenderLine(renderer, x1, y1, x2, y2)			\
	(SDL_RenderLine((renderer), (x1), (y1), (x2), (y2))? 0: -1)

typedef SDL_IOStream	CSDL_IOStream;
#define CSDL_IOFromMem(mem, size)	SDL_IOFromMem((mem), (size))
#define CSDL_LoadBMP_IO(src, closeio)	SDL_LoadBMP_IO((src), (closeio))
#define CIMG_LoadTexture_IO(renderer, stream, closeio)			\
	IMG_LoadTexture_IO((renderer), (stream), (closeio))
#define CSDL_CloseIO(stream)	SDL_CloseIO(stream)

extern int CSDL_QueryTexture(SDL_Texture *texture, Uint32 *format, int *access,
			     int *w, int *h);
extern int CSDL_RenderTexture(SDL_Renderer *renderer, SDL_Texture *texture, \
			      const SDL_Rect *srcrect, const SDL_Rect *dstrect);
extern int CSDL_RenderTextureRotated(SDL_Renderer *renderer,
				     SDL_Texture *texture,
				     const SDL_Rect *srcrect,
				     const SDL_Rect *dstrect,
				     double angle, const SDL_Point *center,
				     SDL_FlipMode flip);
extern int CSDL_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
				 SDL_PixelFormat format,
				 void *pixels, int pitch);

#define CSDL_FreeBasePath(p)
#define CSDL_DestroySurface(s)	SDL_DestroySurface(s)

#define CSDL_SetWindowFullscreen(w, f)					\
	(SDL_SetWindowFullscreen((w), (f))? 0: -1)

#define CSDL_SetEventEnabled(e, f)	SDL_SetEventEnabled((e), (f))

#define CSDL_ShowCursor(f)						\
	((void) ((f)? SDL_ShowCursor(): SDL_HideCursor()))

#define CSDL_LinearScalingHint()	true	/* Linear is default. */

#define CSDL_DROP_FILE(d)	((d).data)

#define CSDL_KEY_SYM(k)		((k).key)
#define CSDL_KEY_MOD(k)		((k).mod)

#define CSDL_KMOD_CTRL		SDL_KMOD_CTRL
#define CSDL_KMOD_ALT		SDL_KMOD_ALT
#define CSDL_K_C		SDLK_C
#define CSDL_K_R		SDLK_R
#define CSDL_K_S		SDLK_S
#define CSDL_K_V		SDLK_V

#define CSDL_EVENT_QUIT		SDL_EVENT_QUIT
#define CSDL_EVENT_KEY_UP	SDL_EVENT_KEY_UP
#define CSDL_EVENT_KEY_DOWN	SDL_EVENT_KEY_DOWN
#define CSDL_EVENT_DROP_FILE	SDL_EVENT_DROP_FILE
#define CSDL_EVENT_MOUSE_MOTION	SDL_EVENT_MOUSE_MOTION

#endif

#endif
