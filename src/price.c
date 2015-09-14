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
#include "price.h"
#include "ezcurl.h"
#include "inventory.h"
#include "shared.h"

static const char URL[] =
  "http://steamcommunity.com/market/priceoverview/?appid=730&market_hash_name=%s";

static char *prices;

int price_read()
{
  if (access("steamanalyst", F_OK) == -1) // if doesn't exist
    return 1;

  prices = read_file("steamanalyst");

  return prices != NULL;
}

void price_clean()
{
  if (prices)
    free(prices);
}

static float get_market(const char *id)
{
  const char  *json;
  char        *escaped;
  json_object *jobj, *jval;

  escaped = ezcurl_escape(id, 0);
  if (!escaped)
    {
      ERROR("Failed to escape item name");
      return -1.0f;
    }

  json = ezcurl_get(URL, escaped);
  curl_free(escaped);
  if (!json)
    return -1.0f;

  jobj = json_tokener_parse(json);

  if (!json_object_object_get_ex(jobj, "success", &jval))
    {
#if DEBUG
        WARNING("Failed to decode object 'success', item not sellable ?");
        // for some reasons, curl doesn't receive {"success": false}
#endif
      return -1.0f;
    }

  if (!json_object_get_boolean(jval))
    return -1.0f;

  if (!json_object_object_get_ex(jobj, "lowest_price", &jval))
    {
#if DEBUG
        WARNING("Failed to decode object 'lowest_price', item not on market ?");
        // maximum market price is $400, some items are not listed on it
#endif
      return -1.0f;
    }

  return atof(json_object_get_string(jval) + 1);
}

static float get_steamanalyst(const char *id) // get_market if not found
{
  const char *ptr;

  if (id[0] == '\xE2') // remove knife "black star" unicode
    ptr = strstr(prices, id + 4);
  else
    ptr = strstr(prices, id);

  if (!ptr)
    return get_market(id);

  ptr = strchr(ptr, ';');
  if (!ptr)
    return get_market(id);

  return atof(ptr + 1);
}

float price_get(const char *id) // -1 if not found
{
  if (prices)
    return get_steamanalyst(id);
  else
    return get_market(id);
}
