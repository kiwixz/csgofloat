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
  fprintf(stderr, " \x1b[32m"s "\x1b[0m\r", ## __VA_ARGS__)

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
#define MAXTRYPERSEC 8 // API limit is 100000 per day
#define NAMEWIDTH "55" // string
#define DATEFORMAT "%d/%m %H:%M"

static const int URLBUF = 1024,
                 DATEBUF = 32,
                 MINLEN = 5,
                 COLOR_OFF[] = {137, 137, 137},
                 COLOR_ON[] = {87, 203, 222};
static const double FSTEPS[] = {
  1.0, 0.45, 0.38, 0.15, 0.07, 0.0
};

static const char BANSURL[] =
  "http://api.steampowered.com/ISteamUser/GetPlayerBans/v0001/?steamids=%s&key="
#include "../STEAMKEY"
,
                  IDURL[] =
  "http://api.steampowered.com/ISteamUser/ResolveVanityURL/v0001/?vanityurl=%s&key="
#include "../STEAMKEY"
,
                  INVURL[] =
  "http://api.steampowered.com/IEconItems_730/GetPlayerItems/v0001/?steamid=%s&key="
#include "../STEAMKEY"
,
                  PLAYERURL[] =
  "http://api.steampowered.com/ISteamUser/GetPlayerSummaries/v0002/?steamids=%s&key="
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
},
                  *STATES[] = {
  "Offline",
  "Online",
  "Busy",
  "Away",
  "Snooze",
  "Looking to Trade",
  "Looking to Play"
};
static const struct timespec DELAY = {.tv_nsec = 1000000000LL / MAXTRYPERSEC};

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

static json_object *extract_first_player(json_object *jobj)
{
  json_object *jlist;

  if (!json_object_object_get_ex(jobj, "players", &jlist))
    {
      ERROR("Failed to decode object 'players'");
      return NULL;
    }

  if (!json_object_array_length(jlist))
    {
      ERROR("Invalid SteamID");
      return NULL;
    }

  return json_object_array_get_idx(jlist, 0);
}

static void display_player(int state, int offtime, const char *name, int age)
{
  const int *col;

  if (state)
    col = COLOR_ON;
  else
    col = COLOR_OFF;

  printf("\x1b[38;2;%d;%d;%dm%s (%s",
         col[0], col[1], col[2], name, STATES[state]);

  if (!state)
    {
      int offtimeh, offtimem;

      offtimeh = offtime / 60 / 60;
      offtime -= offtimeh * 60 * 60;
      offtimem = offtime / 60;
      offtime -= offtimem * 60;

      printf(" for %dh %dmin %ds", offtimeh, offtimem, offtime);
    }

  printf(") \x1b[38;2;%d;%d;%dmis on Steam for \x1b[0m%d days\n",
         COLOR_OFF[0], COLOR_OFF[1], COLOR_OFF[2], age / 60 / 60 / 24);
}

static int parse_player(const char *json)
{
  int         state, offtime, age;
  const char  *name;
  json_object *jobj, *jval;

  jobj = json_tokener_parse(json);

  if (!json_object_object_get_ex(jobj, "response", &jval))
    {
      ERROR("Failed to decode object 'response'");
      return 0;
    }

  jobj = extract_first_player(jval);
  if (!jobj)
    return 0;

  if (!json_object_object_get_ex(jobj, "communityvisibilitystate", &jval))
    {
      ERROR("Failed to decode object 'communityvisibilitystate'");
      return 0;
    }
  if (json_object_get_int(jval) == 1) // 1 - private / 3 - public
    {
      ERROR("Private profile");
      return 0;
    }

  if (!json_object_object_get_ex(jobj, "personastate", &jval))
    {
      ERROR("Failed to decode object 'personastate'");
      return 0;
    }
  state = json_object_get_int(jval);

  if (!state)
    {
      if (!json_object_object_get_ex(jobj, "lastlogoff", &jval))
        {
          ERROR("Failed to decode object 'lastlogoff'");
          return 0;
        }
      offtime = time(NULL) - json_object_get_int(jval);
    }
  else
    offtime = 0;

  if (!json_object_object_get_ex(jobj, "personaname", &jval))
    {
      ERROR("Failed to decode object 'personaname'");
      return 0;
    }
  name = json_object_get_string(jval);

  if (!json_object_object_get_ex(jobj, "timecreated", &jval))
    {
      ERROR("Failed to decode object 'timecreated'");
      return 0;
    }
  age = time(NULL) - json_object_get_int(jval);

  display_player(state, offtime, name, age);
  return 1;
}

static void display_bans(int communityban, int vacbans, int lastvacban,
                         int gamebans, int economyban)
{
  printf("\x1b[38;2;255;0;0m");

  if (communityban)
    printf("Community banned\n");

  if (vacbans)
    printf("VAC banned %d time%s%d days ago)\n",
           vacbans, vacbans > 1 ? "s (last was " : " (", lastvacban);

  if (gamebans)
    printf("Game banned %d time%s\n",
           gamebans, gamebans > 1 ? "s" : "");

  if (economyban)
    printf("Trade banned\n");
}

static int parse_bans(const char *json)
{
  int         communityban, vacbans, lastvacban, gamebans, economyban;
  json_object *jobj, *jval;

  jobj = extract_first_player(json_tokener_parse(json));
  if (!jobj)
    return 0;

  if (!json_object_object_get_ex(jobj, "CommunityBanned", &jval))
    {
      ERROR("Failed to decode object 'CommunityBanned'");
      return 0;
    }
  communityban = json_object_get_boolean(jval);

  if (!json_object_object_get_ex(jobj, "NumberOfVACBans", &jval))
    {
      ERROR("Failed to decode object 'NumberOfVACBans'");
      return 0;
    }
  vacbans = json_object_get_int(jval);

  if (vacbans)
    {
      if (!json_object_object_get_ex(jobj, "DaysSinceLastBan", &jval))
        {
          ERROR("Failed to decode object 'DaysSinceLastBan'");
          return 0;
        }
      lastvacban = json_object_get_int(jval);
    }
  else
    lastvacban = 0;

  if (!json_object_object_get_ex(jobj, "NumberOfGameBans", &jval))
    {
      ERROR("Failed to decode object 'NumberOfGameBans'");
      return 0;
    }
  gamebans = json_object_get_int(jval);

  if (!json_object_object_get_ex(jobj, "EconomyBan", &jval))
    {
      ERROR("Failed to decode object 'EconomyBan'");
      return 0;
    }
  economyban = strcmp(json_object_get_string(jval), "none");

  display_bans(communityban, vacbans, lastvacban, gamebans, economyban);
  return 1;
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
    name = strdup(rawname);

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
             "s %c%c%c%c%c%c   %.16f\t%.2f%%\t%.2f%%\t(%s)",
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

  printf("\n");
  free(name);
}

static int parse_item(json_object *jobj, const Item *items,
                      int itemslen)
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

static int parse_inv(const char *json, const Item *items,
                     int itemslen)
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
      ERROR("Private inventory");
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

  INFO("Loading SteamID...");

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

#define JSON(url)           \
  free(json);               \
  json = get_json(id, url); \
  if (!json)                \
    return EXIT_FAILURE

  INFO("Loading profile...");
  JSON(PLAYERURL);
  if (!parse_player(json))
    return EXIT_FAILURE;

  INFO("Loading bans...");
  JSON(BANSURL);
  if (!parse_bans(json))
    return EXIT_FAILURE;

  INFO("Loading schema...");
  JSON(SCHEMAURL);
  itemslen = parse_schema(json, &items);
  if (!itemslen)
    return EXIT_FAILURE;

  INFO("Loading inventory...");
  JSON(INVURL);
  parse_inv(json, items, itemslen);

  printf("\x1b[0m");

  free(json);
  free(id);
  for (i = 0; i < itemslen; ++i)
    free(items[i].name);

  free(items);

  return EXIT_SUCCESS;
}
