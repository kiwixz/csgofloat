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
#include <ctype.h>
#include "account.h"
#include "display.h"
#include "ezcurl.h"
#include "inventory.h"
#include "schema.h"
#include "shared.h"

static int usage()
{
  printf("Usage: csgofloat [-fsu] SteamID\n");
  printf(" -a\t\tuse ANSI escape codes (mainly to colorize)\n");
  printf(" -f\t\thide items without float\n");
  printf(" -s string\tsearch for items (case insensitive)\n");
  printf(" -u\t\tupdate 'schema.txt'\n");

  return EXIT_FAILURE;
}

int main(int argc, char *argv[])
{
  char    c;
  int     i, invlen, onlyfloat, update;
  Item    *inv;
  char    *filter;
  Account acc = {0};

  ansiec = onlyfloat = update = 0;
  filter = NULL;
  while ((c = getopt(argc, argv, "afs:u")) != -1)
    switch (c)
      {
        case 'a':
          {
            ansiec = 1;
            break;
          }

        case 'f':
          {
            onlyfloat = 1;
            break;
          }

        case 'u':
          {
            update = 1;
            break;
          }

        case 's':
          {
            filter = optarg;
            break;
          }

        default:
          return usage();
      }


  if (filter)
    {
      i = 0;
      while (filter[i] != '\0')
        {
          filter[i] = tolower(filter[i]);
          ++i;
        }
    }

  if (argc == optind) // all arguments were consumed by getopt()
    return usage();

  ezcurl_init();

  if (update)
    {
      INFO("Updating...");
      schema_update();
    }

  INFO("Loading profile...");
  if (!account_get(argv[optind], &acc))
    return EXIT_FAILURE;

  display_account(&acc);
  printf("\n");

  INFO("Loading schema...");
  if (!schema_get())
    return EXIT_FAILURE;

  INFO("Loading inventory...");
  invlen = inventory_get(acc.id, &inv);
  if (!invlen)
    return EXIT_FAILURE;

  printf("                     \r");
  if (!display_inventory(inv, invlen, onlyfloat, filter))
    return EXIT_FAILURE;

  ezcurl_clean();
  schema_clean();

  for (i = 0; i < invlen; ++i)
    if (inv[i].name)
      free(inv[i].name);


  free(inv);
  free(acc.id);
  free(acc.name);

  return EXIT_SUCCESS;
}
