/*
 * HTTP client to fetch simplesok internet levels
 *
 * Copyright (C) 2014-2024 Mateusz Viste
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

#ifndef http_h_sentinel
#define http_h_sentinel

  /* HTTP download data size limit (# bytes). */
  #define DATA_SIZE_LIMIT (4 * 1024 * 1024 - 1)

  void init_net(void);
  void cleanup_net(void);

/* fetch a resource from host/path on defined port using http and return a
 * pointer to the allocated chunk of memory
 * Note: do not forget to free the memory afterwards! */
  size_t http_get(const char *host, unsigned short port, const char *path, unsigned char **resptr);

#endif
