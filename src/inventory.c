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
#include <json-c/json.h>
#include <time.h>
#include "inventory.h"
#include "ezcurl.h"
#include "shared.h"

const double FSTEPS[] = {
  1.0, 0.45, 0.38, 0.15, 0.07, 0.0
};

enum
{
  ATTRIB_SKIN = 6,
  ATTRIB_F = 8,
  ATTRIB_TDATE = 75,
  ATTRIB_STATTRACK = 80,
  ATTRIB_NAME = 111,
  ATTRIB_STICK0 = 113,
  ATTRIB_STICK1 = 117,
  ATTRIB_STICK2 = 121,
  ATTRIB_STICK3 = 125,
  ATTRIB_STICK4 = 129,
  ATTRIB_STICK5 = 133,
  ATTRIB_SOUVENIR = 137
};

static const char URL[] =
  "http://api.steampowered.com/IEconItems_730/GetPlayerItems/v0001/?steamid=%s&key="
#include "../STEAMKEY"
;

static int parse_attributes(json_object *jobj, Item *item, int tradable)
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
          case ATTRIB_SKIN:
            {
              if (!json_object_object_get_ex(jattrib, "float_value", &jval))
                {
                  ERROR("Failed to decode object 'float_value'");
                  return 0;
                }

              item->skin = json_object_get_int(jval);
              break;
            }

          case ATTRIB_F:
            {
              if (!json_object_object_get_ex(jattrib, "float_value", &jval))
                {
                  ERROR("Failed to decode object 'float_value'");
                  return 0;
                }

              item->f = json_object_get_double(jval);
              break;
            }

          case ATTRIB_TDATE:
            {
              if (tradable) // old dates are still here
                break;

              if (!json_object_object_get_ex(jattrib, "value", &jval))
                {
                  ERROR("Failed to decode object 'value'");
                  return 0;
                }

              item->tdate = json_object_get_int(jval);

              break;
            }

          case ATTRIB_NAME:
            {
              if (!json_object_object_get_ex(jattrib, "value", &jval))
                {
                  ERROR("Failed to decode object 'value'");
                  return 0;
                }

              item->name = strdup(json_object_get_string(jval));

              break;
            }

          case ATTRIB_STATTRACK:
            {
              item->stattrack = 1;
              break;
            }

          case ATTRIB_STICK0:
            {
              item->stickers[0] = 1;
              break;
            }

          case ATTRIB_STICK1:
            {
              item->stickers[1] = 1;
              break;
            }

          case ATTRIB_STICK2:
            {
              item->stickers[2] = 1;
              break;
            }

          case ATTRIB_STICK3:
            {
              item->stickers[3] = 1;
              break;
            }

          case ATTRIB_STICK4:
            {
              item->stickers[4] = 1;
              break;
            }

          case ATTRIB_STICK5:
            {
              item->stickers[5] = 1;
              break;
            }

          case ATTRIB_SOUVENIR:
            {
              item->souvenir = 1;
              break;
            }
        }
    }

  return 1;
}

int inventory_get(const char *id, Item * *items)
{
  int         i, j, len, status;
  json_object *jobj, *jrep, *jval;
  char        buf[URLBUF], *json;

  snprintf(buf, URLBUF, URL, id);

  json = ezcurl_get(buf);
  if (!json)
    return 0;

  jobj = json_tokener_parse(json);
  free(json);

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

  if (!json_object_object_get_ex(jrep, "items", &jobj))
    {
      ERROR("Failed to decode object 'items'");
      return 0;
    }

  len = json_object_array_length(jobj);
  *items = calloc(len, sizeof(Item));
  if (!*items)
    {
      ERROR("Failed to calloc");
      return 0;
    }

  for (i = 0, j = 0; i < len; ++i)
    {
      int         tradable;
      json_object *jitem;

      (*items)[j].f = -1.0;

      jitem = json_object_array_get_idx(jobj, i);

      if (!json_object_object_get_ex(jitem, "defindex", &jval))
        {
          ERROR("Failed to decode object 'defindex'");
          return 0;
        }

      (*items)[j].defindex = json_object_get_int(jval);

      if (json_object_object_get_ex(jobj, "flag_cannot_trade", &jval)
          && json_object_get_boolean(jval))
        tradable = 0;
      else
        tradable = 1;

      if (json_object_object_get_ex(jitem, "attributes", &jval))
        {
          if (!parse_attributes(jval, *items + j, tradable))
            return 0;

          (*items)[j].quality = 0;
          if ((*items)[j].f >= 0.0)
            while ((*items)[j].f < FSTEPS[(*items)[j].quality + 1])
              ++(*items)[j].quality;

        }

      ++j;
    }

  return j;
}
