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
#include "market.h"
#include "schema.h"
#include "shared.h"

#define CONDITIONS (FLOATDEC + 2 + 1 + (PERCENTDEC + 5) * 2)
#define DATEFORMAT "%d/%m %H:%M"
#define FLOATDEC 8
#define PERCENTDEC 2
#define TERMINALWIDTH 120

static const int DATEBUF = 32,
                 MAXTDATE = 7 * 24 * 60 * 60,
                 NAMELEN = TERMINALWIDTH - 1 - 7 - 1 - 6 - 1 - CONDITIONS,
                 SWAPDEF = 1324,
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

static void print_color_raw(float f) // linear from 0.0f to 100.0f
{
  int i;

  i = (int)(f * 5.11);

  if (i < 256)
    printf("\x1b[38;2;255;%d;0m", i);
  else
    printf("\x1b[38;2;%d;255;0m", 511 - i);
}

static void print_color(float f) // from 0.0f to 100.0f
{
  f -= 50;
  print_color_raw(f * f * f / 2500 + 50);
}

static void print_base(const char *name, const char *price, const Item *item)
{
  int len;

  printf("%s", name);

  if (item->stattrak || (item->defindex == SWAPDEF)) // StatTrak Swap Tool
    len = NAMELEN + 2 - strlen(name); // +2 because of UTF-8 (TM)
  else
    len = NAMELEN - strlen(name);

  if (item->unusual)
    len += 2;

  if (item->name)
    {
      int l;

      if (ansiec)
        printf("\x1b[39m");

      printf(" \"%s\"%n", item->name, &l);
      len -= l;
    }

  if (item->tdate)
    {
      int  l, col;
      char buf[DATEBUF];

      if (ansiec)
        {
          col = (item->tdate - time(NULL)) * 255 / MAXTDATE;
          if (col > 255)
            col = 255;

          printf("\x1b[38;2;255;%d;0m", 255 - col);
        }

      strftime(buf, DATEBUF, DATEFORMAT, localtime(&item->tdate));
      printf(" [%s]%n", buf, &l);
      len -= l;
    }

  if (len < 0)
    {
      printf("\n");
      len = NAMELEN;
    }

  for ( ; len > 0; --len)
    printf(" ");

  if (price && price[0])
    {
      print_color_raw(-4000 / (atof(price + 1) + 37) + 109); // not linear
      printf("%7s", price);
    }
  else
    printf("       ");

  if (ansiec)
    {
#define STICK(s) \
  item->stickers[s] ? "\x1b[38;2;0;0;255m|" : "\x1b[38;2;55;0;0m_"

      printf(" %s%s%s%s%s%s", STICK(0), STICK(1),
             STICK(2), STICK(3), STICK(4), STICK(5));

#undef STICK
    }
  else
    {
#define STICK(s) item->stickers[s] ? '|' : '_'

      printf(" %c%c%c%c%c%c", STICK(0), STICK(1),
             STICK(2), STICK(3), STICK(4), STICK(5));

#undef STICK
    }
}

int display_inventory(const Item *inv, int len, int detailed,
                      int onlyfloat, int dispprice, const char *filter)
{
  int alt;
  int i;

  alt = 0;
  for (i = 0; i < len; ++i)
    {
      char *name, *price;

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

      if (dispprice)
        {
          price = market_get(name, inv + i);

          if (!price)
            return 0;
        }
      else
        price = NULL;

      if (ansiec)
        {
          if (alt)
            printf("\x1b[48;2;8;8;8m");
          else
            printf("\x1b[49m");
        }

      if (inv[i].skin && (inv[i].f >= 0.0))
        {
          double pf, qpf;

          pf = 100 * (1 - inv[i].f);
          qpf = 100 * (1 - (inv[i].f - FSTEPS[inv[i].quality + 1])
                       / (FSTEPS[inv[i].quality] - FSTEPS[inv[i].quality + 1]));

          if (ansiec)
            printf("\x1b[38;2;0;%d;%dm",
                   55 + (int)(2 * qpf), 55 + (int)(2 * pf));

          print_base(name, price, inv + i);

          if (ansiec)
            print_color(pf);

          printf(" %." MSTRINGIFY(FLOATDEC) "f %6.2f%%", inv[i].f, pf);

          if (ansiec)
            print_color(qpf);

          printf(" %6.2f%%\x1b[0m\n", qpf);

          if (detailed)
            {
              int j, follow;

              follow = 0;
              for (j = 0; j < 6; ++j)
                if (inv[i].stickers[j])
                  {
                    char *sname;

                    if (follow)
                      printf(", ");
                    else if (ansiec)
                      printf("\x1b[38;2;255;0;255m");

                    sname = schema_name_sticker(inv[i].stickers[j]);
                    if (!sname)
                      return 0;

                    printf("%d: %s", j, sname);

                    free(sname);
                    follow = 1;
                  }


              if (follow)
                printf("\x1b[0m\n");
            }
        }
      else
        {
          int j;

          if (ansiec)
            printf("\x1b[38;2;%d;%d;%dm", COLOR_NOF[0],
                   COLOR_NOF[1], COLOR_NOF[2]);

          print_base(name, price, inv + i);

          for (j = 0; j < CONDITIONS; ++j)
            printf(" ");

          printf("  \x1b[0m\n");
        }

      free(name);

      if (dispprice)
        free(price);

      alt = !alt;
    }

  // if (ansiec)
  // printf("\x1b[0m");

  return i;
}
