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
#include "account.h"
#include "display.h"
#include "ezcurl.h"
#include "inventory.h"
#include "schema.h"
#include "shared.h"

static int usage()
{
  printf("\x1b[31mUsage: csgofloat [-fu] \x1b[3mSteamID\x1b[0m\n");
  return EXIT_FAILURE;
}

int main(int argc, char *argv[])
{
  char    c;
  int     invlen, onlyfloat, update;
  Item    *inv;
  Account acc = {0};

  onlyfloat = update = 0;
  while ((c = getopt(argc, argv, "fu")) != -1)
    switch (c)
      {
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

        default:
          return usage();
      }


  if (argc == optind) // all arguments were consumed by getopt()
    return usage();

  ezcurl_init();

  if (update)
    schema_update();

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

  if (!display_inventory(inv, invlen, onlyfloat))
    return EXIT_FAILURE;

  ezcurl_clean();
  schema_clean();
  free(inv);
  free(acc.id);
  free(acc.name);

  return EXIT_SUCCESS;
}
