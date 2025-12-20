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

#include "config.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "data.h"

#include "gra.h"
#include "gz.h"

#include "skin.h"

#ifndef PKGDATADIR
#define PKGDATADIR "/usr/share/simplesok"
#endif


/* loads a bmp.gz graphic and returns it as a texture, NULL on error */
static SDL_Texture *loadGraphic(SDL_Renderer *renderer, const void *memptr, size_t memlen) {
  SDL_Texture *texture;

  texture = loadgzbmp(memptr, memlen, renderer);

  if (texture == NULL) {
    printf("SDL_CreateTextureFromSurface() failed: %s\n", SDL_GetError());
    return(NULL);
  }

  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  return(texture);
}


/* looks out for a skin file and opens it, if found */
static FILE *skin_lookup(const char *name) {
  FILE *fd = NULL;
  struct skinlist *list, *node;

  /* load list of available skins and look for a match with name */
  list = skin_list();
  for (node = list; node != NULL; node = node->next) {
    if (strcmp(node->name, name) == 0) break;
  }

  /* if found a match, open it */
  if (node != NULL) {
    fd = fopen(node->path, "rb");
    printf("found skin file at %s\n", node->path);
    if (fd == NULL) fprintf(stderr, "failed to open skin file: %s\n", strerror(errno));
  }

  /* free skin list */
  skin_list_free(list);

  return(fd);
}


void skin_list_free(struct skinlist *l) {
  struct skinlist *next;
  while (l != NULL) {
    next = l->next;
    free(l->name);
    free(l->path);
    free(l);
    l = next;
  }
}


/* filter out skin names to match *.bmp and *.png files only */
static int skin_filter(const struct dirent *d) {
  int slen = strlen(d->d_name);
  if ((slen > 4) && (strcmp(d->d_name + slen - 4, ".bmp") == 0)) return(1);
  if ((slen > 4) && (strcmp(d->d_name + slen - 4, ".png") == 0)) return(1);
  return(0);
}


/* Load skin names and path from directory. */
static void skin_list_from_dir(struct skinlist **head, const char *dir) {
  DIR *dirfd = opendir(dir);

  if (dirfd != NULL) {
    struct dirent *dentry;

    while ((dentry = readdir(dirfd)) != NULL) {
      char fname[512];

      snprintf(fname, sizeof(fname), "%s%s", dir, dentry->d_name);
      if (skin_filter(dentry) && !access(fname, R_OK)) {
        char *name = strdup(dentry->d_name);
        int i = 1;
        size_t len;
        struct skinlist *node;
        struct skinlist **p;

        /* trim file extension from skin name: can be either .bmp, or .png */
        len = strlen(name);
        if ((len > 4) && (strcmp(name + len - 4, ".bmp") == 0)) {
          len -= 4;
          name[len] = 0;
        } else if ((len > 4) && (strcmp(name + len - 4, ".png") == 0)) {
          len -= 4;
          name[len] = 0;
        } else {
          printf("invalid skin filename: '%s'\n", name);
          continue;
        }

        /* Prepare to insert in alphanumerical order. */
        for (p = head; (node = *p) && (i = strcmp(name, node->name)) > 0;) {
          p = &node->next;
        }

        if (i != 0) {  /* Ignore duplicates. */
          /* prep new node */
          node = malloc(sizeof(struct skinlist));
          node->name = name;
          node->path = strdup(fname);

          /* insert new node in list of results */
          node->next = *p;
          *p = node;
        } else {
          free(name);
        }
      }
    }
    closedir(dirfd);
  }
}


struct skinlist *skin_list(void) {
  struct skinlist *head = NULL;
  char *ptr;
  const char *cptr;
  char buff[512];

  /* local user preferences directory */
  ptr = SDL_GetPrefPath("", "simplesok");
  if (ptr != NULL) {
    snprintf(buff, sizeof(buff), "%sskins/", ptr);
    skin_list_from_dir(&head, buff);
    SDL_free(ptr);
  }

  /* applications running path */
  cptr = SDL_GetBasePath();
  if (cptr != NULL) {
    snprintf(buff, sizeof(buff), "%sskins/", cptr);
    skin_list_from_dir(&head, buff);
    CSDL_FreeBasePath(cptr);
  }

  skin_list_from_dir(&head, PKGDATADIR "/skins/");
  skin_list_from_dir(&head, "/usr/share/simplesok/skins/");
  return(head);
}


/* analyzes a texture and returs:
 * 0 if no pixels are found
 * 1 if only transparent pixels were found
 * 2 if only non-transparent pixels were found
 * 3 if both transparent and non-transparent pixels were found */
static int texture_check_transparency(SDL_Texture *tex, SDL_Renderer *renderer)  {
  uint32_t *pixels;
  SDL_Texture *workscreen;
  int texw, texh;
  int i;
  int verdict = 0;
  SDL_Rect r;

  if (tex == NULL) return(0);

  CSDL_QueryTexture(tex, NULL, NULL, &texw, &texh);

  workscreen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, texw, texh);

  if (workscreen == NULL) printf("%d: Ooops! %s\n", __LINE__, SDL_GetError());

  SDL_SetTextureBlendMode(workscreen, SDL_BLENDMODE_BLEND); /* enable transparency support for this texture */

  /* fill the renderer with all-transparent pixels and blit tex to it */
  SDL_SetRenderTarget(renderer, workscreen);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); /* fill texture with transparency */
  SDL_RenderClear(renderer);
  CSDL_RenderTexture(renderer, tex, NULL, NULL);

  pixels = calloc(4, texw * texh);
  r.x = 0;
  r.y = 0;
  r.w = texw;
  r.h = texh;

  if (CSDL_RenderReadPixels(renderer, &r, SDL_PIXELFORMAT_RGBA8888, pixels, texw * 4) != 0) {
    printf("OOOPS: %s\n", SDL_GetError());
  }
  for (i = 0; i < texw * texh; i++) {
    if (pixels[i] & 0xff) { /* alpha channel is in lowest 8 bits (RGBA8888) */
      verdict |= 2;
    } else {
      verdict |= 1;
    }
  }

  free(pixels);

  /* reset the renderer (detach any texture) so I can draw to screen again */
  SDL_SetRenderTarget(renderer, NULL);

  return(verdict);
}


/* extracts a tile from a tilemap at coordinates pointed by rect r and return it as a texture */
static SDL_Texture *copy_tile_from_map(SDL_Renderer *renderer, SDL_Texture *map, const SDL_Rect *r) {
  SDL_Texture *tile;

  tile = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, r->w, r->h);
  SDL_SetTextureBlendMode(tile, SDL_BLENDMODE_BLEND); /* enable transparency support for this texture */

  /* cut the sprite out of the map and copy it into an isolated texture so it can be used later and rescaled without the risk of any texture bleed */
  SDL_SetRenderTarget(renderer, tile);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); /* fill texture with transparency */
  SDL_RenderClear(renderer);
  CSDL_RenderTexture(renderer, map, r, NULL);
  SDL_SetRenderTarget(renderer, NULL); /* reset the renderer (detach any texture) so I can draw to screen again */

  return(tile);
}


/* fills rect with coordinates of tile id withing sprite map */
static void locate_sprite(SDL_Rect *r, unsigned short id, unsigned short tilesize) {
  r->x = (id % 4) * tilesize;
  r->y = (id / 4) * tilesize;
  r->w = tilesize;
  r->h = tilesize;
}


/* translates a simplesok-format tilemap into sprites */
static void load_spritemap(struct spritesstruct *sprites, SDL_Texture *map, SDL_Renderer *renderer) {
  int i;
  SDL_Rect r;

  /* explode the map into tiles */
  for (i = 0; i < 4*8; i++) {
    locate_sprite(&r, i, sprites->tilesize);
    sprites->map[i] = copy_tile_from_map(renderer, map, &r);
  }

  /* check the WALL_PLAIN tile - some skins have this transparent, in such
   * case rewire it to WALL_CORNER for a best effort rendering */
  if (texture_check_transparency(sprites->map[SPRITE_WALL_PLAIN], renderer) < 2) {
    if (sprites->map[SPRITE_WALL_PLAIN]) SDL_DestroyTexture(sprites->map[SPRITE_WALL_PLAIN]);
    locate_sprite(&r, SPRITE_WALL_CORNER, sprites->tilesize);
    sprites->map[SPRITE_WALL_PLAIN] = copy_tile_from_map(renderer, map, &r);
  }
}


/* try to load a skin named "name" */
static SDL_Texture *loadmap(const char *name, SDL_Renderer *renderer) {
  FILE *fd;
  SDL_Texture *map = NULL;

  /* look out for a skin file */
  if ((name != NULL) && ((fd = skin_lookup(name)) != NULL)) {
    unsigned char *memptr;
    size_t skinlen;

    /* figure skin's length */
    fseek(fd, 0, SEEK_END);
    skinlen = ftell(fd);
    rewind(fd);
    if (skinlen < 1) {
      fclose(fd);
      return(NULL);
    }

    memptr = malloc(skinlen);
    if (fread(memptr, 1, skinlen, fd) != skinlen) {
      fprintf(stderr, "warning: unexpectedly short skin (%s, expected len=%ld)\n", name, (long int)skinlen);
    }
    fclose(fd);
    map = loadGraphic(renderer, memptr, skinlen);
    free(memptr);
  } else { /* otherwise load the embedded skin */
    fprintf(stderr, "skin load failed ('%s'), falling back to embedded default\n", name);
    map = loadGraphic(renderer, skins_yoshi_png, skins_yoshi_png_len);
  }

  return(map);
}


struct spritesstruct *skin_load(const char *name, SDL_Renderer *renderer) {
  struct spritesstruct *sprites;
  SDL_Texture *map;
  int i;

  sprites = calloc(1, sizeof(struct spritesstruct));
  if (sprites == NULL) {
    printf("skin.c: out of memory @ %d\n", __LINE__);
    return(NULL);
  }

  /* load the tilemap */
  map = loadmap(name, renderer);
  if (map == NULL) return(NULL);

  /* figure out how big the tiles of this skin are */
  {
    int gwidth, gheight;
    CSDL_QueryTexture(map, NULL, NULL, &gwidth, &gheight);
    if ((gwidth == 0) || (gheight == 0)) {
      SDL_DestroyTexture(map);
      return(NULL);
    }

    sprites->tilesize = (unsigned short)(gwidth / 4);

    /* If width or tilesize seems odd, then it is probably an overly large
     * skin (with extra columns) as used by some YASC versions. Let's try to
     * figure out the real tilesize by matching common spritemap heights. */
    if ((gwidth % 4) || (sprites->tilesize & 1) || (gheight % sprites->tilesize)) {
      int i;
      int h[] = { 8, 10, 0 };
      puts("WARNING: skin does not seem to be in the usual 4-columns format! Trying to figure out its geometry...");

      for (i = 0; h[i] != 0; i++) {
        unsigned short tilesize_candidate = gheight / h[i];

        if (tilesize_candidate & 1) continue;
        if (gheight % h[i]) continue;
        if (gwidth % (tilesize_candidate)) continue;
        if (gwidth / tilesize_candidate < 5) continue;

        printf("Skin is likely a %dx%d sprite map\n", gwidth / tilesize_candidate, h[i]);
        sprites->tilesize = tilesize_candidate;
        break;
      }

      if (h[i] == 0) puts("ERROR: Unable to figure out the skin geometry. Will proceed with default assumptions, sorry.");
    }
  }

  /* explode the map into tiles (and free the big-ass map afterwards) */
  load_spritemap(sprites, map, renderer);

  SDL_DestroyTexture(map);
  map = NULL;

  /* playfield items */
  sprites->black = loadGraphic(renderer, assets_img_black_bmp_gz, assets_img_black_bmp_gz_len);

  /* strings */
  sprites->cleared = loadGraphic(renderer, assets_img_cleared_bmp_gz, assets_img_cleared_bmp_gz_len);
  sprites->help = loadGraphic(renderer, assets_img_help_bmp_gz, assets_img_help_bmp_gz_len);
  sprites->solved = loadGraphic(renderer, assets_img_solved_bmp_gz, assets_img_solved_bmp_gz_len);
  sprites->nosolution = loadGraphic(renderer, assets_img_nosol_bmp_gz, assets_img_nosol_bmp_gz_len);
  sprites->congrats = loadGraphic(renderer, assets_img_congrats_bmp_gz, assets_img_congrats_bmp_gz_len);
  sprites->copiedtoclipboard = loadGraphic(renderer, assets_img_copiedtoclipboard_bmp_gz, assets_img_copiedtoclipboard_bmp_gz_len);
  sprites->playfromclipboard = loadGraphic(renderer, assets_img_playfromclipboard_bmp_gz, assets_img_playfromclipboard_bmp_gz_len);
  sprites->snapshottoclipboard = loadGraphic(renderer, assets_img_snapshottoclipboard_bmp_gz, assets_img_snapshottoclipboard_bmp_gz_len);
  sprites->saved = loadGraphic(renderer, assets_img_saved_bmp_gz, assets_img_saved_bmp_gz_len);
  sprites->loaded = loadGraphic(renderer, assets_img_loaded_bmp_gz, assets_img_loaded_bmp_gz_len);
  sprites->nosave = loadGraphic(renderer, assets_img_nosave_bmp_gz, assets_img_nosave_bmp_gz_len);

  /* load font */
  sprites->font['0'] = loadGraphic(renderer, assets_font_0_bmp_gz, assets_font_0_bmp_gz_len);
  sprites->font['1'] = loadGraphic(renderer, assets_font_1_bmp_gz, assets_font_1_bmp_gz_len);
  sprites->font['2'] = loadGraphic(renderer, assets_font_2_bmp_gz, assets_font_2_bmp_gz_len);
  sprites->font['3'] = loadGraphic(renderer, assets_font_3_bmp_gz, assets_font_3_bmp_gz_len);
  sprites->font['4'] = loadGraphic(renderer, assets_font_4_bmp_gz, assets_font_4_bmp_gz_len);
  sprites->font['5'] = loadGraphic(renderer, assets_font_5_bmp_gz, assets_font_5_bmp_gz_len);
  sprites->font['6'] = loadGraphic(renderer, assets_font_6_bmp_gz, assets_font_6_bmp_gz_len);
  sprites->font['7'] = loadGraphic(renderer, assets_font_7_bmp_gz, assets_font_7_bmp_gz_len);
  sprites->font['8'] = loadGraphic(renderer, assets_font_8_bmp_gz, assets_font_8_bmp_gz_len);
  sprites->font['9'] = loadGraphic(renderer, assets_font_9_bmp_gz, assets_font_9_bmp_gz_len);
  sprites->font['a'] = loadGraphic(renderer, assets_font_a_bmp_gz, assets_font_a_bmp_gz_len);
  sprites->font['b'] = loadGraphic(renderer, assets_font_b_bmp_gz, assets_font_b_bmp_gz_len);
  sprites->font['c'] = loadGraphic(renderer, assets_font_c_bmp_gz, assets_font_c_bmp_gz_len);
  sprites->font['d'] = loadGraphic(renderer, assets_font_d_bmp_gz, assets_font_d_bmp_gz_len);
  sprites->font['e'] = loadGraphic(renderer, assets_font_e_bmp_gz, assets_font_e_bmp_gz_len);
  sprites->font['f'] = loadGraphic(renderer, assets_font_f_bmp_gz, assets_font_f_bmp_gz_len);
  sprites->font['g'] = loadGraphic(renderer, assets_font_g_bmp_gz, assets_font_g_bmp_gz_len);
  sprites->font['h'] = loadGraphic(renderer, assets_font_h_bmp_gz, assets_font_h_bmp_gz_len);
  sprites->font['i'] = loadGraphic(renderer, assets_font_i_bmp_gz, assets_font_i_bmp_gz_len);
  sprites->font['j'] = loadGraphic(renderer, assets_font_j_bmp_gz, assets_font_j_bmp_gz_len);
  sprites->font['k'] = loadGraphic(renderer, assets_font_k_bmp_gz, assets_font_k_bmp_gz_len);
  sprites->font['l'] = loadGraphic(renderer, assets_font_l_bmp_gz, assets_font_l_bmp_gz_len);
  sprites->font['m'] = loadGraphic(renderer, assets_font_m_bmp_gz, assets_font_m_bmp_gz_len);
  sprites->font['n'] = loadGraphic(renderer, assets_font_n_bmp_gz, assets_font_n_bmp_gz_len);
  sprites->font['o'] = loadGraphic(renderer, assets_font_o_bmp_gz, assets_font_o_bmp_gz_len);
  sprites->font['p'] = loadGraphic(renderer, assets_font_p_bmp_gz, assets_font_p_bmp_gz_len);
  sprites->font['q'] = loadGraphic(renderer, assets_font_q_bmp_gz, assets_font_q_bmp_gz_len);
  sprites->font['r'] = loadGraphic(renderer, assets_font_r_bmp_gz, assets_font_r_bmp_gz_len);
  sprites->font['s'] = loadGraphic(renderer, assets_font_s_bmp_gz, assets_font_s_bmp_gz_len);
  sprites->font['t'] = loadGraphic(renderer, assets_font_t_bmp_gz, assets_font_t_bmp_gz_len);
  sprites->font['u'] = loadGraphic(renderer, assets_font_u_bmp_gz, assets_font_u_bmp_gz_len);
  sprites->font['v'] = loadGraphic(renderer, assets_font_v_bmp_gz, assets_font_v_bmp_gz_len);
  sprites->font['w'] = loadGraphic(renderer, assets_font_w_bmp_gz, assets_font_w_bmp_gz_len);
  sprites->font['x'] = loadGraphic(renderer, assets_font_x_bmp_gz, assets_font_x_bmp_gz_len);
  sprites->font['y'] = loadGraphic(renderer, assets_font_y_bmp_gz, assets_font_y_bmp_gz_len);
  sprites->font['z'] = loadGraphic(renderer, assets_font_z_bmp_gz, assets_font_z_bmp_gz_len);
  sprites->font['A'] = loadGraphic(renderer, assets_font_aa_bmp_gz, assets_font_aa_bmp_gz_len);
  sprites->font['B'] = loadGraphic(renderer, assets_font_bb_bmp_gz, assets_font_bb_bmp_gz_len);
  sprites->font['C'] = loadGraphic(renderer, assets_font_cc_bmp_gz, assets_font_cc_bmp_gz_len);
  sprites->font['D'] = loadGraphic(renderer, assets_font_dd_bmp_gz, assets_font_dd_bmp_gz_len);
  sprites->font['E'] = loadGraphic(renderer, assets_font_ee_bmp_gz, assets_font_ee_bmp_gz_len);
  sprites->font['F'] = loadGraphic(renderer, assets_font_ff_bmp_gz, assets_font_ff_bmp_gz_len);
  sprites->font['G'] = loadGraphic(renderer, assets_font_gg_bmp_gz, assets_font_gg_bmp_gz_len);
  sprites->font['H'] = loadGraphic(renderer, assets_font_hh_bmp_gz, assets_font_hh_bmp_gz_len);
  sprites->font['I'] = loadGraphic(renderer, assets_font_ii_bmp_gz, assets_font_ii_bmp_gz_len);
  sprites->font['J'] = loadGraphic(renderer, assets_font_jj_bmp_gz, assets_font_jj_bmp_gz_len);
  sprites->font['K'] = loadGraphic(renderer, assets_font_kk_bmp_gz, assets_font_kk_bmp_gz_len);
  sprites->font['L'] = loadGraphic(renderer, assets_font_ll_bmp_gz, assets_font_ll_bmp_gz_len);
  sprites->font['M'] = loadGraphic(renderer, assets_font_mm_bmp_gz, assets_font_mm_bmp_gz_len);
  sprites->font['N'] = loadGraphic(renderer, assets_font_nn_bmp_gz, assets_font_nn_bmp_gz_len);
  sprites->font['O'] = loadGraphic(renderer, assets_font_oo_bmp_gz, assets_font_oo_bmp_gz_len);
  sprites->font['P'] = loadGraphic(renderer, assets_font_pp_bmp_gz, assets_font_pp_bmp_gz_len);
  sprites->font['Q'] = loadGraphic(renderer, assets_font_qq_bmp_gz, assets_font_qq_bmp_gz_len);
  sprites->font['R'] = loadGraphic(renderer, assets_font_rr_bmp_gz, assets_font_rr_bmp_gz_len);
  sprites->font['S'] = loadGraphic(renderer, assets_font_ss_bmp_gz, assets_font_ss_bmp_gz_len);
  sprites->font['T'] = loadGraphic(renderer, assets_font_tt_bmp_gz, assets_font_tt_bmp_gz_len);
  sprites->font['U'] = loadGraphic(renderer, assets_font_uu_bmp_gz, assets_font_uu_bmp_gz_len);
  sprites->font['V'] = loadGraphic(renderer, assets_font_vv_bmp_gz, assets_font_vv_bmp_gz_len);
  sprites->font['W'] = loadGraphic(renderer, assets_font_ww_bmp_gz, assets_font_ww_bmp_gz_len);
  sprites->font['X'] = loadGraphic(renderer, assets_font_xx_bmp_gz, assets_font_xx_bmp_gz_len);
  sprites->font['Y'] = loadGraphic(renderer, assets_font_yy_bmp_gz, assets_font_yy_bmp_gz_len);
  sprites->font['Z'] = loadGraphic(renderer, assets_font_zz_bmp_gz, assets_font_zz_bmp_gz_len);
  sprites->font[':'] = loadGraphic(renderer, assets_font_sym_col_bmp_gz, assets_font_sym_col_bmp_gz_len);
  sprites->font[';'] = loadGraphic(renderer, assets_font_sym_scol_bmp_gz, assets_font_sym_scol_bmp_gz_len);
  sprites->font['!'] = loadGraphic(renderer, assets_font_sym_excl_bmp_gz, assets_font_sym_excl_bmp_gz_len);
  sprites->font['$'] = loadGraphic(renderer, assets_font_sym_doll_bmp_gz, assets_font_sym_doll_bmp_gz_len);
  sprites->font['.'] = loadGraphic(renderer, assets_font_sym_dot_bmp_gz, assets_font_sym_dot_bmp_gz_len);
  sprites->font['&'] = loadGraphic(renderer, assets_font_sym_ampe_bmp_gz, assets_font_sym_ampe_bmp_gz_len);
  sprites->font['*'] = loadGraphic(renderer, assets_font_sym_star_bmp_gz, assets_font_sym_star_bmp_gz_len);
  sprites->font[','] = loadGraphic(renderer, assets_font_sym_comm_bmp_gz, assets_font_sym_comm_bmp_gz_len);
  sprites->font['('] = loadGraphic(renderer, assets_font_sym_par1_bmp_gz, assets_font_sym_par1_bmp_gz_len);
  sprites->font[')'] = loadGraphic(renderer, assets_font_sym_par2_bmp_gz, assets_font_sym_par2_bmp_gz_len);
  sprites->font['['] = loadGraphic(renderer, assets_font_sym_bra1_bmp_gz, assets_font_sym_bra1_bmp_gz_len);
  sprites->font[']'] = loadGraphic(renderer, assets_font_sym_bra2_bmp_gz, assets_font_sym_bra2_bmp_gz_len);
  sprites->font['-'] = loadGraphic(renderer, assets_font_sym_minu_bmp_gz, assets_font_sym_minu_bmp_gz_len);
  sprites->font['_'] = loadGraphic(renderer, assets_font_sym_unde_bmp_gz, assets_font_sym_unde_bmp_gz_len);
  sprites->font['/'] = loadGraphic(renderer, assets_font_sym_slas_bmp_gz, assets_font_sym_slas_bmp_gz_len);
  sprites->font['"'] = loadGraphic(renderer, assets_font_sym_quot_bmp_gz, assets_font_sym_quot_bmp_gz_len);
  sprites->font['#'] = loadGraphic(renderer, assets_font_sym_hash_bmp_gz, assets_font_sym_hash_bmp_gz_len);
  sprites->font['@'] = loadGraphic(renderer, assets_font_sym_at_bmp_gz, assets_font_sym_at_bmp_gz_len);
  sprites->font['\''] = loadGraphic(renderer, assets_font_sym_apos_bmp_gz, assets_font_sym_apos_bmp_gz_len);

  /* set all NULL fonts to '_' */
  for (i = 0; i < 256; i++) {
    if (sprites->font[i] == NULL) sprites->font[i] = sprites->font['_'];
  }

  /* analyze the "player right" position - if completely transparent, then
   * player character is rotatable */
  if (texture_check_transparency(sprites->map[SPRITE_PLAYERRIGHT], renderer) < 2) {
    sprites->flags |= SPRITES_FLAG_PLAYERROTATE;
  }

  /* analyze the player's sprite - if it has no transparent pixels, then flag the sprite map as "primitive" to hint simplesok that animations may be unadvisable */
  if (texture_check_transparency(sprites->map[SPRITE_PLAYERUP], renderer) == 2) {
    sprites->flags |= SPRITES_FLAG_PRIMITIVE;
  }

  /* compute the em unit used to scale other things in the game */
  {
    int em;
    /* the reference is the height of the 'A' glyph */
    CSDL_QueryTexture(sprites->font['A'], NULL, NULL, NULL, &em);
    sprites->em = (unsigned short)em;
  }

  return(sprites);
}


void skin_free(struct spritesstruct *sprites) {
  int x;
  for (x = 0; x < 4*8; x++) if (sprites->map[x]) SDL_DestroyTexture(sprites->map[x]);
  if (sprites->black) SDL_DestroyTexture(sprites->black);
  if (sprites->nosolution) SDL_DestroyTexture(sprites->nosolution);
  if (sprites->cleared) SDL_DestroyTexture(sprites->cleared);
  if (sprites->help) SDL_DestroyTexture(sprites->help);
  if (sprites->congrats) SDL_DestroyTexture(sprites->congrats);
  if (sprites->copiedtoclipboard) SDL_DestroyTexture(sprites->copiedtoclipboard);
  if (sprites->playfromclipboard) SDL_DestroyTexture(sprites->playfromclipboard);
  if (sprites->snapshottoclipboard) SDL_DestroyTexture(sprites->snapshottoclipboard);
  if (sprites->saved) SDL_DestroyTexture(sprites->saved);
  if (sprites->loaded) SDL_DestroyTexture(sprites->loaded);
  if (sprites->nosave) SDL_DestroyTexture(sprites->nosave);
  for (x = 0; x < 256; x++) {
    /* skip font['_'] because it may be used as a placeholder pointer for many
     * glyphs, so I will explicitely free it afterwards */
    if (sprites->font[x] && sprites->font[x] != sprites->font['_']) {
      SDL_DestroyTexture(sprites->font[x]);
    }
  }
  if (sprites->font['_']) SDL_DestroyTexture(sprites->font['_']);
  free(sprites);
}
