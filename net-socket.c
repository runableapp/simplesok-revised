/*
 * socket-based, minimalist and somewhat naive HTTP client used by simplesok
 * to fetch internet levels
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

#if defined(_WIN32) || defined(WIN32)
  #include <winsock2.h>  /* gethostbyname() on nonstandard, exotic platforms */
#else
  #include <netdb.h>     /* gethostbyname() on posix */
#endif

#include <errno.h>
#include <unistd.h> /* NULL */
#include <string.h> /* memcpy() */
#include <stdlib.h> /* realloc(), malloc() */
#include <stdio.h>  /* sprintf() */

#include "net.h"

void init_net(void) {
  #if defined(_WIN32) || defined(WIN32)
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2,2), &wsaData);
  #endif
}

void cleanup_net(void) {
#if defined(_WIN32) || defined(WIN32)
  WSACleanup();
#endif
}

/* a socket must be closed with closesocket() on Windows */
#ifdef _WIN32
#define CLOSESOCK(x) closesocket(x)
#else
#define CLOSESOCK(x) close(x)
#endif


/* open socket to remote host/port and return its socket descriptor */
static int makeSocket(const char *host, unsigned short portnum) {
  int sock;
  struct sockaddr_in sa; /* Socket address */
  struct hostent *hp;    /* Host entity */

  hp = gethostbyname(host);
  if (hp == NULL) return(-1);

  /* Copy host address from hostent to (server) socket address */
  memcpy((char *)&sa.sin_addr, (char *)hp->h_addr, hp->h_length);
  sa.sin_family = hp->h_addrtype;  /* Set service sin_family to PF_INET */
  sa.sin_port = htons(portnum);       /* Put portnum into sockaddr */

  /* open socket */
  sock = socket(hp->h_addrtype, SOCK_STREAM, 0);
  if (sock == -1) return(-1);

  /* connect to remote host */
  if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
    CLOSESOCK(sock);
    return(-1);
  }

  return(sock);
}

static long readline(int sock, char *buf, long maxlen) {
  long res, bufpos = 0;
  char bytebuff;
  for (;;) {
    res = recv(sock, &bytebuff, 1, 0);
    if (res < 1) break;
    if (bytebuff == '\r') continue;
    if (bytebuff == '\n') break;
    buf[bufpos++] = bytebuff;
    if (bufpos >= maxlen - 1) break;
  }
  buf[bufpos] = 0;
  return(bufpos);
}

/* fetch a resource from host/path on defined port using http and return a pointer to the allocated chunk of memory
 * Note: do not forget to free the memory afterwards! */
size_t http_get(const char *host, unsigned short port, const char *path, unsigned char **resptr) {
  #define BUFLEN 2048
  size_t resalloc = 1024;
  size_t reslen = 0;
  char linebuf[BUFLEN];
  long len;
  int sock;
  unsigned char *res;
  *resptr = NULL;
  res = NULL;
  sock = makeSocket(host, port);
  if (sock < 0) {
    printf("makeSocket() err: %s\n", strerror(errno));
    return(0);
  }
  snprintf(linebuf, BUFLEN - 1, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
  send(sock, linebuf, strlen(linebuf), 0);
  /* trash out all headers */
  for (;;) {
    len = readline(sock, linebuf, BUFLEN);
    if (len == 0) break;
  }
  /* preallocate initial buffer */
  res = malloc(resalloc);
  if (res == NULL) {
    CLOSESOCK(sock);
    return(0);
  }
  /* fetch data */
  for (;;) {
    len = recv(sock, linebuf, BUFLEN, 0);
    if (len == 0) break;
    if (len < 0) {
      printf("Err: %s\n", strerror(errno));
      free(res);
      CLOSESOCK(sock);
      return(0);
    }
    if (reslen + (size_t) len > DATA_SIZE_LIMIT) {
      free(res);
      CLOSESOCK(sock);
      return 0;
    }
    while (reslen + (size_t)len + 1 > resalloc) {
      unsigned char *newres = realloc(res, resalloc *= 2);

      if (newres == NULL) {
        free(res);
        CLOSESOCK(sock);
        return(0);
      }
      res = newres;
    }
    memcpy(&(res[reslen]), linebuf, (size_t)len);
    reslen += (size_t)len;
  }
  CLOSESOCK(sock);
  res[reslen] = 0; /* terminate data with a NULL, just in case (I don't know what the caller will want to do with the data..) */
  *resptr = res;

  return(reslen);
}
