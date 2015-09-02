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
#include <string.h>
#include <time.h>
#include "display.h"
#include "account.h"
#include "inventory.h"
#include "schema.h"
#include "shared.h"

#define DATEFORMAT " [%d/%m %H:%M]"
#define FLOATDEC 8
#define PERCENTDEC 2
#define TERMINALWIDTH 120

static const int DATEBUF = 32,
                 NAMELEN = TERMINALWIDTH
  - 1 - 6 - 1 - FLOATDEC - 2 - 1 - (PERCENTDEC + 5) * 2,
                 COLOR_NOF[] = {125, 125, 125},
                 COLOR_OFF[] = {137, 137, 137},
                 COLOR_ONLINE[] = {87, 203, 222};

void display_account(const Account *acc)
{
  const int *col;

  if (acc->state)
    col = COLOR_ONLINE;
  else
    col = COLOR_OFF;

  if (ansiec)
    printf("\x1b[38;2;%d;%d;%dm",
           col[0], col[1], col[2]);

  printf("%s (%s", acc->name, STATES[acc->state]);

  if (!acc->state)
    {
      int offtimes, offtimem, offtimeh, offtimed;

      offtimes = acc->offtime % 60;
      offtimed = acc->offtime / 60;
      offtimem = offtimed % 60;
      offtimed /= 60;
      offtimeh = offtimed % 24;
      offtimed /= 24;

      printf(" for %d%s%d%s%d%s%ds", offtimed, offtimed ? "d " : "",
             offtimeh, offtimeh ? "h " : "",
             offtimem, offtimem ? "min " : "", offtimes);
    }

  if (ansiec)
    printf(
      ") \x1b[38;2;%d;%d;%dmis on Steam for \x1b[0m%d days\n\x1b[38;2;255;0;0m",
      COLOR_OFF[0], COLOR_OFF[1], COLOR_OFF[2], acc->age / 60 / 60 / 24);
  else
    printf(") is on Steam for %d days\n", acc->age / 60 / 60 / 24);

  if (acc->communityban)
    printf("Community banned\n");

  if (acc->vacbans)
    printf("VAC banned %d time%s%d days ago)\n", acc->vacbans,
           acc->vacbans > 1 ? "s (last was " : " (", acc->lastvacban);

  if (acc->gamebans)
    printf("Game banned %d time%s\n",
           acc->gamebans, acc->gamebans > 1 ? "s" : "");

  if (acc->economyban)
    printf("Trade banned\n");

  if (ansiec)
    printf("\x1b[0m");
}

int display_inventory(const Item *inv, int len,
                      int onlyfloat, const char *filter)
{
  int i;

  for (i = 0; i < len; ++i)
    {
      char *name;

      if (onlyfloat && (inv[i].f < 0.0))
        continue;

      name = schema_name(inv + i);
      if (!name)
        return 0;

      if (filter)
        {
          int  j, namelen;
          char *lname;

          namelen = strlen(name);
          SMALLOC(lname, namelen + 1, 0);

          for (j = 0; j < namelen; ++j)
            lname[j] = tolower(name[j]);

          lname[namelen] = '\0';
          if (!strstr(lname, filter))
            {
              free(name);
              free(lname);

              continue;
            }

          free(lname);
        }

      if (inv[i].tdate)
        {
          char date[DATEBUF];

          strftime(date, DATEBUF, DATEFORMAT, localtime(&inv[i].tdate));
          name = realloc(name, strlen(name) + DATEBUF + 1);
          if (!name)
            {
              ERROR("Failed to realloc");
              return 0;
            }

          strcat(name, date);
        }

      if (inv[i].skin && (inv[i].f >= 0.0))
        {
          int    j;
          double pf, qpf;

          pf = 100 * (1 - inv[i].f);
          qpf = 100 * (1 - (inv[i].f - FSTEPS[inv[i].quality + 1])
                       / (FSTEPS[inv[i].quality] - FSTEPS[inv[i].quality + 1]));

          if (ansiec)
            printf("\x1b[38;2;%d;%d;%dm", inv[i].tdate ? 255 : 55,
                   55 + (int)(2 * qpf), 55 + (int)(2 * pf));

          printf("%s", name);

          j = NAMELEN - strlen(name);
          if (j < 0)
            {
              printf("\n");
              j = NAMELEN;
            }

          for ( ; j > 0; --j)
            printf(" ");

#define STICK(s) inv[i].stickers[s] ? '|' : '_'

          printf("%c%c%c%c%c%c %." MSTRINGIFY(FLOATDEC) "f %6.2f%% %6.2f%%\n",
                 STICK(0), STICK(1), STICK(2), STICK(3), STICK(4), STICK(5),
                 inv[i].f, pf, qpf);

#undef STICK
        }
      else
        {
          if (ansiec)
            printf("\x1b[38;2;%d;%d;%dm", COLOR_NOF[0],
                   COLOR_NOF[1], COLOR_NOF[2]);

          printf("%s\n", name);
        }

      free(name);
    }

  if (ansiec)
    printf("\x1b[0m");

  return i;
}
