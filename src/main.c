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

typedef struct
{
  int  defindex;
  char *name;
} Item;

#define MAXTRYPERSEC 4 // API limit is 100000 per day
#define NAMEWIDTH "36"

static const int URLBUF = 1024,
                 MINLEN = 5,
                 FLOATATTRIB = 8;
static const double FSTEPS[] = {
  1.0, 0.44, 0.37, 0.15, 0.07, 0.0
};

static const char INVURL[] =
  "http://api.steampowered.com/IEconItems_730/GetPlayerItems/v0001/?steamid=%s&key="
#include "../STEAMKEY"
,
                  IDURL[] =
  "http://api.steampowered.com/ISteamUser/ResolveVanityURL/v0001/?vanityurl=%s&key="
#include "../STEAMKEY"
,
                  SCHEMAURL[] =
  "http://api.steampowered.com/IEconItems_730/GetSchema/v0002/?language=en&key="
#include "../STEAMKEY"
,
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

static int parse_schema(const char *json, Item * *items)
{
  int         i, len;
  json_object *jobj, *jval;

  jobj = json_tokener_parse(json);

  if (!json_object_object_get_ex(jobj, "result", &jval))
    {
      ERROR("Failed to decode object 'result'");
      return NULL;
    }

  if (!json_object_object_get_ex(jval, "items", &jobj))
    {
      ERROR("Failed to decode object 'items'");
      return NULL;
    }

  len = json_object_array_length(jobj);
  *items = malloc(len * sizeof(Item));

  for (i = 0; i < len; ++i)
    {
      json_object *jitem;

      jitem = json_object_array_get_idx(jobj, i);

      if (!json_object_object_get_ex(jitem, "defindex", &jval))
        {
          ERROR("Failed to decode object 'defindex'");
          return NULL;
        }

      (*items)[i].defindex = json_object_get_int(jval);

      if (!json_object_object_get_ex(jitem, "item_name", &jval))
        {
          ERROR("Failed to decode object 'item_name'");
          return NULL;
        }

      (*items)[i].name = strdup(json_object_get_string(jval));
    }

  return i;
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

static void display_item(const char *name, double f)
{
  if (f)
    {
      int q;

      q = 1;
      while (f < FSTEPS[q])
        ++q;

      --q;
      printf("%-"NAMEWIDTH "s %.16f    \t%.2f%%    \t%.2f%% \t(%s)\n", name, f,
             100 * (1 - f),
             100 * (1 - (f - FSTEPS[q + 1]) / (FSTEPS[q] - FSTEPS[q + 1])),
             QUALITIES[q]);
    }
  else
    printf("%-"NAMEWIDTH "s (no float)\n", name);
}

static int parse_item(json_object *jobj, const Item *items, int itemslen)
{
  int         i, defindex;
  json_object *jval;

  if (!json_object_object_get_ex(jobj, "defindex", &jval))
    {
      ERROR("Failed to decode object 'defindex'");
      return 0;
    }

  defindex = json_object_get_int(jval);

  if (!json_object_object_get_ex(jobj, "attributes", &jval))
    {
      ERROR("Failed to decode object 'attributes'");
      return 0;
    }

  for (i = 0; i < itemslen; ++i)
    if (items[i].defindex == defindex)
      {
        display_item(items[i].name, parse_float(jval));
        break;
      }


  return 1;
}

static int parse_inv(const char *json, const Item *items, int itemslen)
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
    if (!parse_item(json_object_array_get_idx(jval, i), items, itemslen))
      return 0;


  return i;
}

int main(int argc, char *argv[])
{
  int  i, itemslen;
  char *id, *json;
  Item *items;

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
  json = get_json(id, SCHEMAURL);
  if (!json)
    return EXIT_FAILURE;

  itemslen = parse_schema(json, &items);
  if (!itemslen)
    return EXIT_FAILURE;

  free(json);
  json = get_json(id, INVURL);
  if (!json)
    return EXIT_FAILURE;

  parse_inv(json, items, itemslen);

  free(json);
  free(id);
  for (i = 0; i < itemslen; ++i)
    free(items[i].name);

  free(items);

  return EXIT_SUCCESS;
}
