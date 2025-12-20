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

#ifndef sok_core_h_sentinel
#define sok_core_h_sentinel

  #include <stdint.h>

  #define field_floor 1
  #define field_atom 2
  #define field_goal 4
  #define field_wall 8

  struct sokgame {
    unsigned short field_width;
    unsigned short field_height;
    unsigned char field[64][64];
    char comment[128];
    int positionx;
    int positiony;
    unsigned short level;
    uint64_t crc64;
    unsigned long crc32_106; /* CRC32 as it was (badly) computed by v1.0.6 and earlier */
    char *solution;
  };

  struct sokgamestates {
    int angle;
    char *history;
    size_t historyallocsize;
  };

  enum SOKMOVE {
    sokmoveNONE = 0,
    sokmoveUP = 1,
    sokmoveLEFT = 2,
    sokmoveDOWN = 3,
    sokmoveRIGHT = 4
  };

  #define sokmove_pushed 1
  #define sokmove_ongoal 2
  #define sokmove_solved 4

  /* loads a level file. returns the amount of levels loaded on success, a non-positive value otherwise. */
  int sok_loadfile(struct sokgame **game, int maxlevels, char *gamelevel, unsigned char *memptr, size_t filelen, char *comment, int maxcommentlen);

  void sok_freefile(struct sokgame **gamelist, int gamescount);

  /* checks if the game is solved. returns 0 if the game is not solved, non-zero otherwise. */
  int sok_checksolution(struct sokgame *game, struct sokgamestates *states);

  /* try to move the player in a direction. returns a negative value if move has been denied, or a sokmove bitfield otherwise. */
  int sok_move(struct sokgame *game, enum SOKMOVE dir, int validitycheck, struct sokgamestates *states);

  /* undo last move */
  void sok_undo(struct sokgame *game, struct sokgamestates *states);

  /* returns the number of moves in a history string */
  size_t sok_history_getlen(const char *history);

  /* returns the number of pushes in a history string */
  size_t sok_history_getpushes(const char *history);

  /* reset game's states */
  void sok_resetstates(struct sokgamestates *states);

  /* initialize a states structure, and return a pointer to it */
  struct sokgamestates *sok_newstates(void);

  /* free the memory occupied by a previously allocated states structure */
  void sok_freestates(struct sokgamestates *states);

  /* reloads solutions for all levels in a list */
  void sok_loadsolutions(struct sokgame **gamelist, int levelscount);

  /* returns a human string for error code */
  char *sok_strerr(int errid);

  /* plays a string of moves */
  void sok_play(struct sokgame *game, struct sokgamestates *states, char *playfile);

#endif
