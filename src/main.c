/*
 * Copyright (c) 2015 kiwixz
 *
 * This file is part of csgofloat.
 *
 * interpolation is free software : you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * interpolation is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with csgofloat. If not, see <http://www.gnu.org/licenses/>.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <json-c/json.h>

#define INFO(s, ...) \
  fprintf(stderr, " \x1b[32m"s "\x1b[0m\n", ## __VA_ARGS__)

#define ERROR(s, ...)                                      \
  fprintf(stderr, __FILE__ ":%d: \x1b[31;1m"s "\x1b[0m\n", \
          __LINE__, ## __VA_ARGS__)

typedef struct
{
  char *ptr;
  int  len;
} String;

#define MAXTRYPERSEC 4 // API limit is 100000 per day

static const int URLBUF = 1024,
                 MINLEN = 5,
                 FLOATATTRIB = 8;
static const char INVURL[] =
  "http://api.steampowered.com/IEconItems_730/GetPlayerItems/v0001/?key="
#include "../STEAMKEY"
         "&steamid=%s",
                  IDURL[] =
  "http://api.steampowered.com/ISteamUser/ResolveVanityURL/v0001/?key="
#include "../STEAMKEY"
         "&vanityurl=%s",
                  *QUALITIES[] = {
  "Battle-Scarred",
  "Well-Worn",
  "Field-Tested",
  "Minimal Wear",
  "Factory New"
};
static const struct timespec DELAY = {.tv_nsec = 1000000000L / MAXTRYPERSEC};

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

static int get_json_try(CURL *curl, String *s)
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

static char *get_json(const char *id, const char *url)
{
  int    ret;
  char   buf[URLBUF];
  String s = {0};
  CURL   *curl;

  s.ptr = malloc(1);
  if (!s.ptr)
    {
      ERROR("Failed to malloc JSON string");
      return NULL;
    }

  snprintf(buf, URLBUF, url, id);

  curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
  curl_easy_setopt(curl, CURLOPT_URL, buf);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

  do
    {
      ret = get_json_try(curl, &s);
      if (!ret)
        return NULL;
    }
  while (ret < MINLEN);

  curl_easy_cleanup(curl);

  return s.ptr;
}

static char *parse_id(const char *json)
{
  json_object *jobj, *jrep, *jval;

  jobj = json_tokener_parse(json);

  if (!json_object_object_get_ex(jobj, "response", &jrep))
    {
      ERROR("Failed to decode object 'response'");
      return NULL;
    }

  if (!json_object_object_get_ex(jrep, "success", &jval))
    {
      ERROR("Failed to decode object 'success'");
      return NULL;
    }

  if (json_object_get_int(jval) != 1)
    return NULL;

  if (!json_object_object_get_ex(jrep, "steamid", &jval))
    {
      ERROR("Failed to decode object 'steamid'");
      return NULL;
    }

  return strdup(json_object_get_string(jval));
}

static double parse_float(json_object *jobj)
{
  int i, len;

  len = json_object_array_length(jobj);
  for (i = 0; i < len; ++i)
    {
      json_object *jattrib, *jval;

      jattrib = json_object_array_get_idx(jobj, i);

      if (!json_object_object_get_ex(jattrib, "defindex", &jval))
        {
          ERROR("Failed to decode object 'defindex'");
          return 0;
        }

      if (json_object_get_int(jval) == FLOATATTRIB)
        {
          if (!json_object_object_get_ex(jattrib, "float_value", &jval))
            {
              ERROR("Failed to decode object 'float_value'");
              return 0;
            }

          return json_object_get_double(jval);
        }
    }

  return 0;
}

static int parse_item(json_object *jobj)
{
  json_object *jval;

  if (!json_object_object_get_ex(jobj, "defindex", &jval))
    {
      ERROR("Failed to decode object 'defindex'");
      return 0;
    }

  printf("%d: ", json_object_get_int(jval));

  if (!json_object_object_get_ex(jobj, "attributes", &jval))
    {
      ERROR("Failed to decode object 'attributes'");
      return 0;
    }

  printf("%.16f\n", parse_float(jval));

  return 1;
}

static int parse_inv(const char *json)
{
  int         i, len, status;
  json_object *jobj, *jrep, *jval;

  jobj = json_tokener_parse(json);

  if (!json_object_object_get_ex(jobj, "result", &jrep))
    {
      ERROR("Failed to decode object 'result'");
      return 0;
    }

  if (!json_object_object_get_ex(jrep, "status", &jval))
    {
      ERROR("Failed to decode object 'status'");
      return 0;
    }

  status = json_object_get_int(jval);

  if (status == 15)
    {
      ERROR("The inventory of this user is private");
      return 0;
    }
  else if (status != 1)
    {
      ERROR("Failed to get the inventory");
      return 0;
    }

  if (!json_object_object_get_ex(jrep, "items", &jval))
    {
      ERROR("Failed to decode object 'items'");
      return 0;
    }

  len = json_object_array_length(jval);
  for (i = 0; i < len; ++i)
    if (!parse_item(json_object_array_get_idx(jval, i)))
      return 0;


  return i;
}

int main(int argc, char *argv[])
{
  char *id, *json;

  if (argc != 2)
    {
      ERROR("Usage: csgofloat SteamID");
      return EXIT_FAILURE;
    }

  json = get_json(argv[1], IDURL);
  if (!json)
    return EXIT_FAILURE;

  id = parse_id(json);
  if (!id)
    {
      id = strdup(argv[1]);
      if (!id)
        {
          ERROR("Failed to decode object 'response'");
          return EXIT_FAILURE;
        }
    }

  free(json);
  json = get_json(id, INVURL);
  if (!json)
    return EXIT_FAILURE;

  parse_inv(json);

  free(json);
  free(id);

  return EXIT_SUCCESS;
}
