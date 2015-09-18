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

const char *const QUALITIES[] = {
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

typedef struct
{
  int    index;
  Limits lim;
  char   *name;
} SkinMapItem;

static const int NAMEBUF = 512,
                 STICKERDEF = 1209;
static const char URL[] =
  "http://api.steampowered.com/IEconItems_730/GetSchema/v0002/?key=%s&language=en";

static MapItem     *defmap, *stickermap;
static SkinMapItem *skinmap;
static int         defmaplen, skinmaplen, stickermaplen;
static char        *namesfile, *skinnames, *stickernames;

static const char *find_limits(char *strfile, const char *start,
                               const char *first)
// no error output
{
  char *str, *ptr;

  str = strstr(strfile, start);
  if (!str)
    return NULL;

  str = strstr(str, first) - 1; // skip defaults
  if (!str)
    return NULL;

  ptr = strstr(str, "\n\t}");
  if (!ptr)
    return NULL;

  *ptr = '\0';

  return str;
}

static char *read_property(const char * *ptr, const char *prop, char first)
{
  const char *c;
  int        len;
  char       *str;

  *ptr = strstr(*ptr, prop);
  if (!*ptr)
    {
      ERROR("Failed to parse property '%s' name", prop);
      return NULL;
    }

  *ptr = strchr(*ptr + strlen(prop), first);
  if (!*ptr)
    {
      ERROR("Failed to parse property '%s' value", prop);
      return NULL;
    }

  c = *ptr + 1;
  for (len = 1; *c != '"'; ++len)
    ++c;

  SMALLOC(str, len + 1, 0);
  memcpy(str, *ptr, len);
  str[len] = '\0';

  return str;
}

static double read_property_f(const char * *ptr, const char *nextptr,
                              const char *prop, double ifnot)
{
  *ptr = strstr(*ptr, prop);
  if ((*ptr > nextptr) || !*ptr)
    return ifnot;

  *ptr = strchr(*ptr + strlen(prop), '.');
  if (!*ptr)
    {
      ERROR("Failed to parse property '%s' value", prop);
      return ifnot;
    }

  return atof(*ptr - 1);
}

static int parse_skinmap()
{
  const char *str, *ptr, *nextptr;
  int        i;
  char       *strfile;

  strfile = read_file("items_game.txt");
  if (!strfile)
    return 0;

  str = find_limits(strfile, "\n\t\"paint_kits\"", "\n\t\t\"2\"");
  if (!str)
    {
      ERROR("Failed to parse names of skins");
      return 0;
    }

  ptr = str;
  for (skinmaplen = -1; ptr; ++skinmaplen)
    ptr = strstr(ptr + 1, "\n\t\t\"");

  SMALLOC(skinmap, skinmaplen * sizeof(SkinMapItem), 0);

  nextptr = strstr(str + 1, "\n\t\t\"") + 4;
  for (i = 0; i < skinmaplen; ++i)
    {
      ptr = nextptr;
      skinmap[i].index = atoi(ptr);

      skinmap[i].name = read_property(&ptr, "\"description_tag\"", 'P');
      if (!skinmap[i].name)
        return 0;

      nextptr = strstr(ptr + 1, "\n\t\t\"") + 4;

      skinmap[i].lim.min = read_property_f(&ptr, nextptr,
                                           "\"wear_remap_min\"", 0.0);
      skinmap[i].lim.max = read_property_f(&ptr, nextptr,
                                           "\"wear_remap_max\"", 1.0);
    }

  free(strfile);

  return 1;
}

static int parse_stickermap()
{
  const char *str, *ptr;
  int        i;
  char       *strfile;

  strfile = read_file("items_game.txt");
  if (!strfile)
    return 0;

  str = find_limits(strfile, "\n\t\"sticker_kits\"", "\n\t\t\"1\"");
  if (!str)
    {
      ERROR("Failed to parse names of stickers");
      return 0;
    }

  ptr = str;
  for (stickermaplen = -1; ptr; ++stickermaplen)
    ptr = strstr(ptr + 1, "\n\t\t\"");

  SMALLOC(stickermap, stickermaplen * sizeof(MapItem), 0);

  ptr = str;
  for (i = 0; i < stickermaplen; ++i)
    {
      ptr = strstr(ptr + 1, "\n\t\t\"") + 4;
      stickermap[i].index = atoi(ptr);

      stickermap[i].name = read_property(&ptr, "\"item_name\"", 'S');
      if (!stickermap[i].name)
        return 0;
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

  if (!parse_skinmap() || !parse_stickermap())
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

static int extract_name(const char *namec, const char *list,
                        char *name, int *len)
{
  char *ptr;

  ptr = strstr(list, namec);
  if (ptr)
    {
      int i;

      for (i = 0; i < 2; ++i)
        {
          ptr = strchr(ptr + 1, '"');
          if (!ptr)
            {
              ERROR("Failed to extract name");
              return 0;
            }
        }

      for ( ; ptr[1] != '"'; ++*len)
        name[*len] = *(++ptr);

      name[*len] = '\0';
    }
  else
    {
      strcat(name, "#");
      strcat(name, namec);
    }

  return 1;
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

  if (!extract_name(stickermap[i].name, stickernames, name, &len))
    return NULL;

  return name;
}

char *schema_name(const Item *item, const Limits * *lim)
// if lim not NULL, it'll point to wear limits of the skin (if found)
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

  if (lim)
    *lim = &skinmap[i].lim;

  if (!extract_name(skinmap[i].name, skinnames, name, &len))
    return NULL;

  strcat(name, " (");
  strcat(name, QUALITIES[item->quality]);
  strcat(name, ")");

  return name;
}
