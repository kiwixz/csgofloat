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
#include <curl/curl.h>
#include <json-c/json.h>
#include "market.h"
#include "ezcurl.h"
#include "inventory.h"
#include "shared.h"

static const char URL[] =
  "http://steamcommunity.com/market/priceoverview/?appid=730&market_hash_name=%s";

char *market_get(const char *id, const Item *item) // "" if not on market
{
  char        *escaped, *json;
  json_object *jobj, *jval;

  escaped = ezcurl_escape(id, 0);
  if (!escaped)
    {
      ERROR("Failed to escape item name");
      return NULL;
    }

  json = ezcurl_get(URL, escaped);
  curl_free(escaped);
  if (!json)
    return NULL;

  jobj = json_tokener_parse(json);
  free(json);

  if (!json_object_object_get_ex(jobj, "success", &jval))
    {
#if DEBUG
        WARNING("Failed to decode object 'success', could not find price");
        // for some reasons, curl doesn't receive {"success": false}
#endif
      return strdup("");
    }

  if (!json_object_get_boolean(jval))
    return strdup("");

  if (!json_object_object_get_ex(jobj, "lowest_price", &jval))
    {
      ERROR("Failed to decode object 'lowest_price'");
      return NULL;
    }

  return strdup(json_object_get_string(jval));
}
