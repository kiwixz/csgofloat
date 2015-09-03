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
#include "shared.h"

char *read_file(const char *name)
{
  long len;
  FILE *fp;
  char *str;

  fp = fopen(name, "r");
  if (!fp)
    {
      ERROR("Failed to open file '%s'", name);
      return NULL;
    }

  fseek(fp, 0, SEEK_END);
  len = ftell(fp);
  rewind(fp);

  SMALLOC(str, len + 1, NULL);

  if (fread(str, 1, len, fp) < (size_t)len)
    {
      ERROR("Failed to read file '%s'", name);
      return NULL;
    }

  fclose(fp);
  str[len] = '\0';

  return str;
}
