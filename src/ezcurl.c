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
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include "ezcurl.h"
#include "shared.h"

#define MAXTRYPERSEC 8 // API limit is 100000 per day

typedef struct
{
  char *ptr;
  int  len;
} String;

static const int             MINLEN = 5;
static const struct timespec DELAY = {.tv_nsec = 1000000000LL / MAXTRYPERSEC};

static CURL *curl;

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
}

void ezcurl_clean()
{
  curl_easy_cleanup(curl);
}

static int get_try(String *s)
{
  int ret;

  s->len = 0;
  if (nanosleep(&DELAY, NULL))
    return 0;

  ret = curl_easy_perform(curl);
  if (ret != CURLE_OK)
    {
      ERROR("cURL failed: %s", curl_easy_strerror(ret));
      return 0;
    }

  return s->len;
}

char *ezcurl_get(const char *url)
{
  int    ret;
  String s = {0};

  SMALLOC(s.ptr, 1, NULL);

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

  do
    {
      ret = get_try(&s);
      if (!ret)
        return NULL;
    }
  while (ret < MINLEN);

  return s.ptr;
}
