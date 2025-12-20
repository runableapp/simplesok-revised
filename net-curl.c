/*
 * curl-based minimalist HTTP client to fetch simplesok internet levels
 *
 * Contributed by Patrick Monnerat in 2023.
 *
 * This file is part of the Simple Sokoban project.
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

#include <errno.h>
#include <unistd.h> /* NULL */
#include <string.h> /* memcpy() */
#include <stdlib.h> /* realloc(), malloc() */
#include <stdio.h>  /* sprintf() */

#include <curl/curl.h>  /* cURL */

#include "net.h"

/* Network data loading control structure. */
struct netload {
  unsigned char *buffer;        /* Loaded data buffer. */
  size_t bufalloc;              /* Buffer size. */
  size_t datalen;               /* Data byte count in buffer. */
};


void init_net(void) {
  curl_global_init(CURL_GLOBAL_ALL);
}


void cleanup_net(void) {
  curl_global_cleanup();
}


/* Accumulate incoming data. */
static size_t loaddata(void *ptr, size_t size, size_t nmemb, void *userdata) {
  struct netload *p = userdata;

  (void) size;

  /* Check size limit. */
  if (p->datalen + nmemb > DATA_SIZE_LIMIT)
    return (size_t) -1; /* Error: too many data bytes. */

  /* Enlarge buffer if needed. */
  while (p->bufalloc < p->datalen + nmemb + 1) {
    unsigned char *newbuf = realloc(p->buffer, p->bufalloc *= 2);

    if (!newbuf)
      return (size_t) -1;       /* Error: can't allocate memory. */
    p->buffer = newbuf;
  }

  /* Append new data into buffer. */
  memcpy(p->buffer + p->datalen, ptr, nmemb);
  p->datalen += nmemb;
  p->buffer[p->datalen] = '\0';
  return nmemb;
}

/* fetch a resource from host/path on defined port using http and return a pointer to the allocated chunk of memory
 * Note: do not forget to free the memory afterwards! */
size_t http_get(const char *host, unsigned short port, const char *path, unsigned char **resptr) {
  CURLU *url = curl_url();
  CURL *easy = NULL;
  struct netload ctrl;
  char portstring[6];
  char *urlstring = NULL;

  ctrl.datalen = 0;
  ctrl.bufalloc = 1024;
  ctrl.buffer = NULL;
  if(url) {
    /* Build URL. */
    snprintf(portstring, sizeof(portstring), "%hu", port);
    if (!curl_url_set(url, CURLUPART_SCHEME, "http", 0) &&
        !curl_url_set(url, CURLUPART_HOST, host, 0) &&
        !curl_url_set(url, CURLUPART_PORT, portstring, 0) &&
        !curl_url_set(url, CURLUPART_PATH, path, 0)) {
      curl_url_get(url, CURLUPART_URL, &urlstring, CURLU_URLENCODE);

      /* Build curl handle. */
      if (urlstring && (easy = curl_easy_init())) {
        curl_easy_setopt(easy, CURLOPT_URL, urlstring);
        curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, loaddata);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, &ctrl);
#if defined(_WIN32) || defined(WIN32)
        /* The request may be redirected to https and Windows libcurl packages
           come without trusted CA bundle: if possible, use Windows trusted CA
           certificate store for peer verification, else do not verify peer. */
# ifdef CURLSSLOPT_NATIVE_CA
        /* Supported since 7.71.0. */
        if (curl_version_info(CURLVERSION_NOW)->version_num >= 0x074700)
          curl_easy_setopt(easy, CURLOPT_SSL_OPTIONS,
                           (long) CURLSSLOPT_NATIVE_CA);
        else
# endif
        curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 0L);
#endif

        /* Allocate initial buffer and transfer data. */
        if ((ctrl.buffer = malloc(ctrl.bufalloc))) {
          ctrl.buffer[0] = '\0';
          if (curl_easy_perform(easy)) {
            /* An error occurred. */
            free(ctrl.buffer);
            ctrl.buffer = NULL;
            ctrl.datalen = 0;
          }
        }
      }
    }
  }

  /* Clean up and return. */
  curl_easy_cleanup(easy);
  curl_free(urlstring);
  curl_url_cleanup(url);
  *resptr = ctrl.buffer;

  return(ctrl.datalen);
}
