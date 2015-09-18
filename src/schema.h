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

#ifndef SCHEMA_H
#define SCHEMA_H

#include "inventory.h"

typedef struct
{
  double min, max;
} Limits;

const char *const QUALITIES[5];

int  schema_parse();
void schema_clean();
int  schema_update(const char *key);

char *schema_name_sticker(int index);
char *schema_name(const Item *item, const Limits * *lim);

#endif
