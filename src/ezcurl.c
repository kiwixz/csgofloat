/*
 * Copyright (c) 2015 kiwixz
 *
 * This file is part of csgofloat.
 *
 * csgofloat is free software : you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * csgofloat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with csgofloat. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include "ezcurl.h"
#include "shared.h"

#define MAXTRYPERSEC 8 // API limit is 100000 per day
#define URLBUF 1024

typedef struct
{
  char *ptr;
  int  len;
} String;

static const int             MINLEN = 8;
static const struct timespec DELAY = {.tv_nsec = 1000000000LL / MAXTRYPERSEC};

static struct timespec lasttime = {0};
static CURL            *curl;

static size_t curl_callback(void *ptr, size_t size, size_t nmemb, void *svoid)
{
  int    new, len;
  String *s;

  s = (String *)svoid;
  new = size * nmemb;
  len = s->len + new;

  s->ptr = realloc(s->ptr, len + 1);
  if (!s->ptr)
    {
      ERROR("Failed to realloc download string");
      return 0;
    }

  memcpy(s->ptr + s->len, ptr, new);
  s->ptr[len] = '\0';
  s->len = len;

  return new;
}

void ezcurl_init()
{
  curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);

#if DEBUG
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
#endif
}

void ezcurl_clean()
{
  curl_easy_cleanup(curl);
}

static void timespec_subtract(struct timespec *x, struct timespec *y,
                              struct timespec *result)
{
  if (x->tv_nsec < y->tv_nsec)
    {
      int sec;

      sec = (y->tv_nsec - x->tv_nsec) / 1000000000L + 1;
      y->tv_nsec -= 1000000000L * sec;
      y->tv_sec += sec;
    }

  if (x->tv_nsec - y->tv_nsec > 1000000000L)
    {
      int sec;

      sec = (x->tv_nsec - y->tv_nsec) / 1000000000L;
      y->tv_nsec += 1000000000L * sec;
      y->tv_sec -= sec;
    }

  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_nsec = x->tv_nsec - y->tv_nsec;
}

char *ezcurl_escape(const char *str, int len)
{
  return curl_easy_escape(curl, str, len);
}

char *ezcurl_get(const char *url, ...)
{
  va_list args;
  char    buf[URLBUF];
  String  s = {0};

  va_start(args, url);
  vsnprintf(buf, URLBUF, url, args);
  va_end(args);

  SMALLOC(s.ptr, 1, NULL);

  curl_easy_setopt(curl, CURLOPT_URL, buf);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

  do
    {
      int             ret;
      struct timespec now, diff;

      if (clock_gettime(CLOCK_MONOTONIC, &now))
        {
          ERROR("Failed to get time");
          return 0;
        }

      timespec_subtract(&now, &lasttime, &diff);

      if ((((diff.tv_sec == DELAY.tv_sec) && (diff.tv_nsec < DELAY.tv_nsec))
           || (diff.tv_sec < DELAY.tv_sec))
          && nanosleep(&DELAY, NULL))
        {
          ERROR("Failed to sleep");
          return 0;
        }

      lasttime.tv_sec = now.tv_sec;
      lasttime.tv_nsec = now.tv_nsec;
      s.len = 0;

      ret = curl_easy_perform(curl);
      if (ret != CURLE_OK)
        {
          ERROR("cURL failed: %s", curl_easy_strerror(ret));
          return 0;
        }
    }
  while (s.len < MINLEN);

  return s.ptr;
}
