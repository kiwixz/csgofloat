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
                 NAMEBUF = 512,
                 STICKERDEF = 1209;
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

static MapItem *defmap, *skinmap, *stickermap;
static int     defmaplen, skinmaplen, stickermaplen;
static char    *namesfile, *skinnames, *stickernames;

static char *find_limits(char *strfile, const char *start,
                         const char *first, const char *end) // not verbose !
{
  char *str, *ptr;

  str = strstr(strfile, start);
  if (!str)
    return NULL;

  str = strstr(str, first) - 1; // skip defaults
  if (!str)
    return NULL;

  ptr = strstr(str, end);
  if (!ptr)
    return NULL;

  *ptr = '\0';

  return str;
}

static int parse_names(MapItem * *map, int *maplen, char letter,
                       const char *start, const char *first,
                       const char *end, const char *namec)
{
  int  i, nameclen;
  char *strfile, *str, *ptr;

  nameclen = strlen(namec);

  strfile = read_file("items_game.txt");
  if (!strfile)
    return 0;

  str = find_limits(strfile, start, first, end);
  if (!str)
    {
      ERROR("Failed to parse names of skins");
      return 0;
    }

  ptr = str;
  for (*maplen = -1; ptr; ++*maplen)
    ptr = strstr(ptr + 1, "\n\t\t\"");

  SMALLOC(*map, *maplen * sizeof(MapItem), 0);

  ptr = str;
  for (i = 0; i < *maplen; ++i)
    {
      int  len;
      char *c;

      ptr = strstr(ptr + 1, "\n\t\t\"") + 4;
      (*map)[i].index = atoi(ptr);

      ptr = strstr(ptr, namec) + nameclen;
      if (!ptr)
        {
          ERROR("Failed to parse skin name");
          return 0;
        }

      while (*ptr != letter)
        ++ptr;

      c = ptr + 1;
      for (len = 1; *c != '"'; ++len)
        ++c;

      SMALLOC((*map)[i].name, len + 1, 0);

      memcpy((*map)[i].name, ptr, len);
      (*map)[i].name[len] = '\0';
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

  defmaplen = json_object_array_length(jobj);
  SMALLOC(defmap, defmaplen * sizeof(MapItem), 0);

  for (i = 0; i < defmaplen; ++i)
    {
      json_object *jitem;

      jitem = json_object_array_get_idx(jobj, i);

      if (!json_object_object_get_ex(jitem, "defindex", &jval))
        {
          ERROR("Failed to decode object 'defindex'");
          return 0;
        }
      defmap[i].index = json_object_get_int(jval);

      if (!json_object_object_get_ex(jitem, "item_name", &jval))
        {
          ERROR("Failed to decode object 'item_name'");
          return 0;
        }

      defmap[i].name = strdup(json_object_get_string(jval));
      if (!defmap[i].name)
        {
          ERROR("Failed to duplicate string");
          return 0;
        }
    }

  if (!parse_names(&skinmap, &skinmaplen, 'P', "\n\t\"paint_kits\"",
                   "\n\t\t\"2\"", "\n\t}", "\n\t\t\t\"description_tag\"")
      || !parse_names(&stickermap, &stickermaplen, 'S', "\n\t\"sticker_kits\"",
                      "\n\t\t\"1\"", "\n\t}", "\n\t\t\t\"item_name\""))
    return 0;

  namesfile = read_file("csgo_english.txt");
  if (!namesfile)
    return 0;

  skinnames = strstr(namesfile, "PaintKit_Default_Tag");
  stickernames = strstr(namesfile, "StickerKit_Default");
  if (!skinnames || !stickernames)
    {
      ERROR("Failed to parse skins names");
      return 0;
    }

  return defmaplen;
}

void schema_clean()
{
  int i;

  for (i = 0; i < defmaplen; ++i)
    free(defmap[i].name);

  for (i = 0; i < skinmaplen; ++i)
    free(skinmap[i].name);

  free(defmap);
  free(skinmap);
  free(namesfile);
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

static void extract_name(const char *namec, const char *list,
                         char *name, int *len)
{
  char *ptr;

  ptr = strstr(list, namec);
  if (ptr)
    {
      int i;

      for (i = 0; i < 2; ++i)
        do
          ++ptr;
        while (*ptr != '"');

      for ( ; ptr[1] != '"'; ++*len)
        name[*len] = *(++ptr);

      name[*len] = '\0';
    }
  else
    {
      strcat(name, "#");
      strcat(name, namec);
    }
}

char *schema_name_sticker(int index)
{
  int  i, len;
  char *name;

  if (!index)
    return strdup("");

  SMALLOC(name, NAMEBUF, NULL);
  len = 0;

  for (i = 0; i < stickermaplen; ++i)
    if (stickermap[i].index == index)
      break;


  if (i == stickermaplen)
    {
      ERROR("Failed to find sticker with index %d", index);
      return NULL;
    }

  extract_name(stickermap[i].name, stickernames, name, &len);

  return name;
}

char *schema_name(const Item *item)
{
  int  i, len;
  char *name;

  SMALLOC(name, NAMEBUF, NULL);
  name[0] = '\0';

  if (item->unusual)
    strcat(name, "\xE2\x98\x85 ");

  if (item->stattrak)
    strcat(name, "StatTrak\xE2\x84\xA2 ");
  else if (item->souvenir && item->skin)
    strcat(name, "Souvenir ");

  for (i = 0; i < defmaplen; ++i)
    if (defmap[i].index == item->defindex)
      break;


  strcat(name, defmap[i].name);
  strcat(name, " | ");

  len = strlen(name);
  if (!item->skin)
    {
      if (item->defindex == STICKERDEF)
        {
          char *sname;

          sname = schema_name_sticker(item->stickers[0]);
          if (!sname)
            return NULL;

          strcat(name, sname);
          free(sname);
        }
      else if (item->f >= 0.0)
        strcat(name, "Vanilla");
      else
        name[len - 3] = '\0';

      return name;
    }

  for (i = 0; i < skinmaplen; ++i)
    if (skinmap[i].index == item->skin)
      break;


  if (i == skinmaplen)
    {
      ERROR("Failed to find skin with index %d", item->skin);
      return NULL;
    }

  if(skinmap[i].name[5] == 'k') // "Paintkit" to "PaintKit"
    skinmap[i].name[5] = 'K';

  extract_name(skinmap[i].name, skinnames, name, &len);

  if ((item->skin >= DOFFSET) &&
      (item->skin < DOFFSET + DPHASESLEN))
    {
      strcat(name, " ");
      strcat(name, DPHASES[item->skin - DOFFSET]);
    }

  strcat(name, " (");
  strcat(name, QUALITIES[item->quality]);
  strcat(name, ")");

  return name;
}
