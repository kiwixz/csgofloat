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
#include <json-c/json.h>
#include "account.h"
#include "ezcurl.h"
#include "shared.h"

const char *const STATES[] = {
  "Offline",
  "Online",
  "Busy",
  "Away",
  "Snooze",
  "Looking to Trade",
  "Looking to Play"
};

static const char BANSURL[] =
  "http://api.steampowered.com/ISteamUser/GetPlayerBans/v0001/?key=%s&steamids=%s",
                  IDURL[] =
  "http://api.steampowered.com/ISteamUser/ResolveVanityURL/v0001/?key=%s&vanityurl=%s",
                  PROFILEURL[] =
  "http://api.steampowered.com/ISteamUser/GetPlayerSummaries/v0002/?key=%s&steamids=%s";

static char *extract_id(const char *oldid, const char *mark)
{
  int  len;
  char *id, *start, *end;

  start = strstr(oldid, mark);
  if (!start)
    return NULL;

  start += strlen(mark);
  end = strchr(start, '/');
  if (end)
    len = end - start;
  else
    len = strlen(start);

  SMALLOC(id, len + 1, NULL);

  memcpy(id, start, len);
  id[len] = '\0';

  return id;
}

static char *get_id(const char *key, const char *oldid)
{
  const char  *json;
  char        *id;
  json_object *jobj, *jrep, *jval;

  if (strstr(oldid, "steamcommunity.com/"))
    {
      id = extract_id(oldid, "/profiles/");
      if (id)
        return id;

      id = extract_id(oldid, "/id/");
      if (!id)
        {
          ERROR("Failed to extract SteamID from URL");
          return NULL;
        }
    }
  else
    id = strdup(oldid);

  json = ezcurl_get(IDURL, key, id);
  if (!json)
    return NULL;

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
    return id;

  if (!json_object_object_get_ex(jrep, "steamid", &jval))
    {
      ERROR("Failed to decode object 'steamid'");
      return NULL;
    }

  free(id);
  return strdup(json_object_get_string(jval));
}

static int get_profile(const char *key, Account *acc)
{
  const char  *json;
  json_object *jobj, *jval, *jrep, *jlist;

  json = ezcurl_get(PROFILEURL, key, acc->id);
  if (!json)
    return 0;

  jobj = json_tokener_parse(json);

  if (!json_object_object_get_ex(jobj, "response", &jrep))
    {
      ERROR("Failed to decode object 'response'");
      return 0;
    }

  if (!json_object_object_get_ex(jrep, "players", &jlist))
    {
      ERROR("Failed to decode object 'players'");
      return 0;
    }

  if (!json_object_array_length(jlist))
    {
      ERROR("Invalid SteamID");
      return 0;
    }

  jobj = json_object_array_get_idx(jlist, 0);

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
  acc->state = json_object_get_int(jval);

  if (!acc->state)
    {
      if (!json_object_object_get_ex(jobj, "lastlogoff", &jval))
        {
          ERROR("Failed to decode object 'lastlogoff'");
          return 0;
        }
      acc->offtime = time(NULL) - json_object_get_int(jval);
    }
  else
    acc->offtime = 0;

  if (!json_object_object_get_ex(jobj, "personaname", &jval))
    {
      ERROR("Failed to decode object 'personaname'");
      return 0;
    }
  acc->name = strdup(json_object_get_string(jval));

  if (!json_object_object_get_ex(jobj, "timecreated", &jval))
    {
      ERROR("Failed to decode object 'timecreated'");
      return 0;
    }
  acc->age = time(NULL) - json_object_get_int(jval);

  return 1;
}

static int get_bans(const char *key, Account *acc)
{
  const char  *json;
  json_object *jobj, *jval, *jlist;

  json = ezcurl_get(BANSURL, key, acc->id);
  if (!json)
    return 0;

  jobj = json_tokener_parse(json);

  if (!json_object_object_get_ex(jobj, "players", &jlist))
    {
      ERROR("Failed to decode object 'players'");
      return 0;
    }

  if (!json_object_array_length(jlist))
    {
      ERROR("Invalid SteamID");
      return 0;
    }

  jobj = json_object_array_get_idx(jlist, 0);

  if (!json_object_object_get_ex(jobj, "CommunityBanned", &jval))
    {
      ERROR("Failed to decode object 'CommunityBanned'");
      return 0;
    }
  acc->communityban = json_object_get_boolean(jval);

  if (!json_object_object_get_ex(jobj, "NumberOfVACBans", &jval))
    {
      ERROR("Failed to decode object 'NumberOfVACBans'");
      return 0;
    }
  acc->vacbans = json_object_get_int(jval);

  if (acc->vacbans)
    {
      if (!json_object_object_get_ex(jobj, "DaysSinceLastBan", &jval))
        {
          ERROR("Failed to decode object 'DaysSinceLastBan'");
          return 0;
        }
      acc->lastvacban = json_object_get_int(jval);
    }
  else
    acc->lastvacban = 0;

  if (!json_object_object_get_ex(jobj, "NumberOfGameBans", &jval))
    {
      ERROR("Failed to decode object 'NumberOfGameBans'");
      return 0;
    }
  acc->gamebans = json_object_get_int(jval);

  if (!json_object_object_get_ex(jobj, "EconomyBan", &jval))
    {
      ERROR("Failed to decode object 'EconomyBan'");
      return 0;
    }
  acc->economyban = strcmp(json_object_get_string(jval), "none");

  return 1;
}

int account_get(const char *key, const char *id, Account *acc)
{
  (acc)->id = get_id(key, id);
  if (!(acc)->id)
    return 0;

  if (!get_profile(key, acc))
    return 0;

  if (!get_bans(key, acc))
    return 0;

  return 1;
}
