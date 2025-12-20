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


#ifndef save_h_sentinel
#define save_h_sentinel

/* saves the solution for levcrc64 */
void solution_save(uint64_t levcrc64, char *solution, char *ext);

/* returns a malloc()'ed, null-terminated string with the solution to level levcrc64. if no solution available, returns NULL. */
char *solution_load(uint64_t levcrc64, char *ext);

/* returns a pointer to a string with the currently configured skin, or NULL if
 * no skin configuration found. the returned pointer MUST NOT be freed. */
const char *loadconf_skin(void);

/* set skin as the new configured skin */
void setconf_skin(const char *skin);

#endif
