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
#include "schema.h"
#include "ezcurl.h"
#include "inventory.h"
#include "shared.h"

const char *QUALITIES[] = {
  "Battle-Scarred",
  "Well-Worn",
  "Field-Tested",
  "Minimal Wear",
  "Factory New"
};

typedef struct
{
  int  index;
  char *name;
} MapItem;

static const int DOFFSET = 415,
                 DPHASESLEN = 7,
                 NAMEBUF = 256;
static const char URL[] =
  "http://api.steampowered.com/IEconItems_730/GetSchema/v0002/?key=%s&language=en",
                  *DPHASES[] = {
  "Ruby",
  "Sapphire",
  "Black Pearl",
  "Phase 1",
  "Phase 2",
  "Phase 3",
  "Phase 4"
};

static MapItem *map, *skinmap;
static int     maplen, skinmaplen;
static char    *skinnamesfile, *skinnames;

static char *find_limits(char *strfile) // not verbose !
{
  char *str, *ptr;

  str = strstr(strfile, "\n\t\"paint_kits\"");
  if (!str)
    return NULL;

  str = strstr(str, "\n\t\t\"2\"") - 1; // skip defaults
  if (!str)
    return NULL;

  ptr = strstr(str, "\n\t\"paint_kits_rarity\"");
  if (!ptr)
    return NULL;

  *ptr = '\0';

  return str;
}

static int parse_skins()
{
  int  i;
  char *strfile, *str, *ptr;

  strfile = read_file("items_game.txt");
  if (!strfile)
    return 0;

  str = find_limits(strfile);
  if (!str)
    {
      ERROR("Failed to parse skins");
      return 0;
    }

  ptr = str;
  for (skinmaplen = -1; ptr; ++skinmaplen)
    ptr = strstr(ptr + 1, "\n\t\t\"");

  SMALLOC(skinmap, skinmaplen * sizeof(MapItem), 0);

  ptr = str;
  for (i = 0; i < skinmaplen; ++i)
    {
      int  len;
      char *c;

      ptr = strstr(ptr + 1, "\n\t\t\"") + 4;
      skinmap[i].index = atoi(ptr);

      ptr = strstr(ptr, "\n\t\t\t\"description_tag\"") + 21;
      if (!ptr)
        {
          ERROR("Failed to parse skins names");
          return 0;
        }

      while (*ptr != 'P')
        ++ptr;

      c = ptr + 1;
      for (len = 1; *c != '"'; ++len)
        ++c;

      SMALLOC(skinmap[i].name, len + 1, 0);

      memcpy(skinmap[i].name, ptr, len);
      skinmap[i].name[len] = '\0';
    }

  free(strfile);
  return 1;
}

int schema_parse()
{
  int         i;
  json_object *jobj, *jval;
  char        *json;

  json = read_file("schema.txt");
  if (!json)
    return 0;

  jobj = json_tokener_parse(json);
  free(json);

  if (!json_object_object_get_ex(jobj, "result", &jval))
    {
      ERROR("Failed to decode object 'result'");
      return 0;
    }

  if (!json_object_object_get_ex(jval, "items", &jobj))
    {
      ERROR("Failed to decode object 'items'");
      return 0;
    }

  maplen = json_object_array_length(jobj);
  SMALLOC(map, maplen * sizeof(MapItem), 0);

  for (i = 0; i < maplen; ++i)
    {
      json_object *jitem;

      jitem = json_object_array_get_idx(jobj, i);

      if (!json_object_object_get_ex(jitem, "defindex", &jval))
        {
          ERROR("Failed to decode object 'defindex'");
          return 0;
        }
      map[i].index = json_object_get_int(jval);

      if (!json_object_object_get_ex(jitem, "item_name", &jval))
        {
          ERROR("Failed to decode object 'item_name'");
          return 0;
        }

      map[i].name = strdup(json_object_get_string(jval));
      if (!map[i].name)
        {
          ERROR("Failed to duplicate string");
          return 0;
        }
    }

  if (!parse_skins())
    return 0;

  skinnamesfile = read_file("csgo_english.txt");
  if (!skinnamesfile)
    return 0;

  skinnames = strstr(skinnamesfile, "PaintKit_Default_Tag");
  if (!skinnames)
    {
      ERROR("Failed to parse skins names");
      return 0;
    }

  return maplen;
}

void schema_clean()
{
  int i;

  for (i = 0; i < maplen; ++i)
    free(map[i].name);

  for (i = 0; i < skinmaplen; ++i)
    free(skinmap[i].name);

  free(map);
  free(skinmap);
  free(skinnamesfile);
}

int schema_update(const char *key)
{
  const char *str;
  int        len;
  FILE       *fp;

  str = ezcurl_get(URL, key);
  if (!str)
    return 0;

  fp = fopen("schema.txt", "w");
  if (!fp)
    {
      ERROR("Failed to open file 'schema.txt'");
      return 0;
    }

  len = strlen(str);
  if (fwrite(str, 1, len, fp) < (size_t)len)
    {
      ERROR("Failed to update schema");
      return 0;
    }

  fclose(fp);

  return 1;
}

char *schema_name(const Item *item)
{
  int  i, len;
  char *name, *ptr;

  SMALLOC(name, NAMEBUF, NULL);

  for (i = 0; i < maplen; ++i)
    if (map[i].index == item->defindex)
      break;


  name[0] = '\0';

  if (item->unusual)
    strcat(name, "\xE2\x98\x85 ");

  if (item->stattrak)
    strcat(name, "StatTrak\xE2\x84\xA2 ");
  else if (item->souvenir && item->skin)
    strcat(name, "Souvenir ");

  strcat(name, map[i].name);
  strcat(name, " | ");

  for (i = 0; i < skinmaplen; ++i)
    if (skinmap[i].index == item->skin)
      break;


  len = strlen(name);
  if (!item->skin || (i == skinmaplen))
    {
      if (item->f >= 0.0)
        strcat(name, "Vanilla");
      else
        name[len - 3] = '\0';

      return name;
    }

  ptr = strstr(skinnames, skinmap[i].name);
  if (ptr)
    {
      for (i = 0; i < 2; ++i)
        do
          ++ptr;
        while (*ptr != '"');

      for ( ; ptr[1] != '"'; ++len)
        name[len] = *(++ptr);

      name[len] = '\0';
    }
  else
    {
      strcat(name, "#");
      strcat(name, skinmap[i].name);
    }

  if ((item->skin >= DOFFSET) && (item->skin < DOFFSET + DPHASESLEN))
    {
      strcat(name, " ");
      strcat(name, DPHASES[item->skin - DOFFSET]);
    }

  strcat(name, " (");
  strcat(name, QUALITIES[item->quality]);
  strcat(name, ")");

  return name;
}
