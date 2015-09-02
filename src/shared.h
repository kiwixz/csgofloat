/*
 * Copyright (c) 2015 kiwixz
 *
 * This file is part of csgofloat.
 *
 * birdseeker is free software : you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * birdseeker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with csgofloat. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SHARED_H
#define SHARED_H

#ifndef __unix__
  #error Only unix OS are supported.
#endif

#include <unistd.h>
#if _POSIX_VERSION < _POSIX_C_SOURCE
  #error This POSIX version is not supported.
#endif

#define INFO(s, ...)                                           \
  if (ansiec)                                                  \
    fprintf(stderr, " \x1b[32m"s "\x1b[0m\r", ## __VA_ARGS__); \
  else                                                         \
    fprintf(stderr, " "s "\r", ## __VA_ARGS__);
#define WARNING(s, ...)                                      \
  if (ansiec)                                                \
    fprintf(stderr, __FILE__ ":%d: \x1b[33;1m"s "\x1b[0m\n", \
            __LINE__, ## __VA_ARGS__);                       \
  else                                                       \
    fprintf(stderr, __FILE__ ":%d: "s "\n",                  \
            __LINE__, ## __VA_ARGS__);
#define ERROR(s, ...)                                        \
  if (ansiec)                                                \
                                                             \
    fprintf(stderr, __FILE__ ":%d: \x1b[31;1m"s "\x1b[0m\n", \
            __LINE__, ## __VA_ARGS__);                       \
  else                                                       \
    fprintf(stderr, __FILE__ ":%d: "s "\n",                  \
            __LINE__, ## __VA_ARGS__);

#define STRINGIFY(s) #s
#define MSTRINGIFY(s) STRINGIFY(s)

#define SMALLOC(ptr, size, ret)  \
  ptr = malloc(size);            \
  if (!ptr)                      \
    {                            \
      ERROR("Failed to malloc"); \
      return ret;                \
    }                            \

#define URLBUF 1024

int ansiec;

#endif
