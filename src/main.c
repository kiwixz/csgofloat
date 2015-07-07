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

typedef struct
{
  int    tradable, stattrack, stickers[6];
  double f;
  time_t tdate;
} Attributes;

enum
{
  ATTRIB_F = 8,
  ATTRIB_TDATE = 75,
  ATTRIB_ST = 80,
  ATTRIB_STICK0 = 113,
  ATTRIB_STICK1 = 117,
  ATTRIB_STICK2 = 121,
  ATTRIB_STICK3 = 125,
  ATTRIB_STICK4 = 129,
  ATTRIB_STICK5 = 133
};

#define HIDE_WHEN_NO_FLOAT 1
#define MAXTRYPERSEC 16 // API limit is 100000 per day
#define NAMEWIDTH "50" // string
#define DATEFORMAT "%d/%m %H:%M"

static const int URLBUF = 1024,
                 DATEBUF = 32,
                 MINLEN = 5;
static const double FSTEPS[] = {
  1.0, 0.45, 0.38, 0.15, 0.07, 0.0
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

static int extract_id(char * *id)
{
  int  len;
  char *nid, *start, *end;

  start = strstr(*id, "/id/");
  if (!start)
    {
      ERROR("Failed to find ID in custom URL");
      return 0;
    }

  start += 4;
  end = strchr(start, '/');
  if (end)
    len = end - start;
  else
    len = strlen(start);

  nid = malloc(len + 1);
  memcpy(nid, start, len);
  nid[len] = '\0';

  free(*id);
  *id = nid;

  return 1;
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

static int parse_attributes(json_object *jobj, Attributes *a)
{
  int i, len;

  len = json_object_array_length(jobj);
  for (i = 0; i < len; ++i)
    {
      int         attrib;
      json_object *jattrib, *jval;

      jattrib = json_object_array_get_idx(jobj, i);

      if (!json_object_object_get_ex(jattrib, "defindex", &jval))
        {
          ERROR("Failed to decode object 'defindex'");
          return 0;
        }

      attrib = json_object_get_int(jval);
      switch (attrib)
        {
          case ATTRIB_F:
            {
              if (!json_object_object_get_ex(jattrib, "float_value", &jval))
                {
                  ERROR("Failed to decode object 'float_value'");
                  return 0;
                }

              a->f = json_object_get_double(jval);
              break;
            }

          case ATTRIB_TDATE:
            {
              if (a->tradable) // old dates are still here
                break;

              if (!json_object_object_get_ex(jattrib, "value", &jval))
                {
                  ERROR("Failed to decode object 'value'");
                  return 0;
                }

              a->tdate = json_object_get_int(jval);
              break;
            }

          case ATTRIB_ST:
            {
              a->stattrack = 1;
              break;
            }

          case ATTRIB_STICK0:
            {
              a->stickers[0] = 1;
              break;
            }

          case ATTRIB_STICK1:
            {
              a->stickers[1] = 1;
              break;
            }

          case ATTRIB_STICK2:
            {
              a->stickers[2] = 1;
              break;
            }

          case ATTRIB_STICK3:
            {
              a->stickers[3] = 1;
              break;
            }

          case ATTRIB_STICK4:
            {
              a->stickers[4] = 1;
              break;
            }

          case ATTRIB_STICK5:
            {
              a->stickers[5] = 1;
              break;
            }
        }
    }

  return 1;
}

static void display_item(char *rawname, Attributes a)
{
  char *name;

#if HIDE_WHEN_NO_FLOAT
    if (!a.f)
      return;
#endif

  if (a.stattrack)
    {
      name = malloc(strlen(rawname) + 1 + 10);
      sprintf(name, "StatTrack %s", rawname);
    }
  else
    name = rawname;

  if (a.tdate)
    {
      char date[DATEBUF], *nname;

      strftime(date, DATEBUF, DATEFORMAT, localtime(&a.tdate));
      nname = malloc(strlen(rawname) + 1 + 3 + DATEBUF);
      sprintf(nname, "%s (%s)", name, date);

      free(name);
      name = nname;
    }

  if (a.f)
    {
      int    q;
      double pf, qpf;

      q = 1;
      while (a.f < FSTEPS[q])
        ++q;

      --q;
      pf = 100 * (1 - a.f);
      qpf = 100 * (1 - (a.f - FSTEPS[q + 1]) / (FSTEPS[q] - FSTEPS[q + 1]));

#define STICK(i) a.stickers[i] ? '|' : '_'
      printf("\x1b[38;2;%d;%d;%dm%-"NAMEWIDTH
             "s    %c%c%c%c%c%c    %.16f    \t%.2f%%    \t%.2f%% \t(%s)",
             a.tradable ? 55 : 255,
             55 + (int)(2 * qpf),
             55 + (int)(2 * pf),
             name,
             STICK(0), STICK(1), STICK(2), STICK(3), STICK(4), STICK(5),
             a.f, pf, qpf, QUALITIES[q]);
#undef STICK
    }
  else
    printf("\x1b[38;2;125;125;125m%-"NAMEWIDTH "s    \t (no float)", name);

  printf("\x1b[0m\n");
}

static int parse_item(json_object *jobj, const Item *items, int itemslen)
{
  int         i, defindex;
  json_object *jval;
  Attributes  a = {0};

  if (!json_object_object_get_ex(jobj, "defindex", &jval))
    {
      ERROR("Failed to decode object 'defindex'");
      return 0;
    }

  defindex = json_object_get_int(jval);

  if (json_object_object_get_ex(jobj, "flag_cannot_trade", &jval)
      && json_object_get_boolean(jval))
    a.tradable = 0;
  else
    a.tradable = 1;

  if (!json_object_object_get_ex(jobj, "attributes", &jval))
    return 1;

  if (!parse_attributes(jval, &a))
    return 0;

  for (i = 0; i < itemslen; ++i)
    if (items[i].defindex == defindex)
      {
        display_item(items[i].name, a);
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
  char *nid, *id, *json;
  Item *items;

  if (argc != 2)
    {
      ERROR("Usage: csgofloat SteamID");
      return EXIT_FAILURE;
    }

  id = strdup(argv[1]);

  if (!strncmp(argv[1], "http://", 7) || !strncmp(argv[1], "https://", 8))
    if (!extract_id(&id))
      return EXIT_FAILURE;


  json = get_json(id, IDURL);
  if (!json)
    return EXIT_FAILURE;

  nid = parse_id(json);
  if (nid)
    id = nid;

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
