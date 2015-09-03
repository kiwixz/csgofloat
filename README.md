# csgofloat

[Feel free to contact me by email.](mailto:kiwixz@users.noreply.github.com)

## Description

This is a simple software to check wear float of CS:GO items.

![](https://raw.githubusercontent.com/kiwixz/csgofloat/master/screenshot.png "Screenshot with options -af")

## What do I need ?

You need an usual development environnement (a C compiler, GNU Make, etc). The Makefile use `c99` as an alias for the compiler.

This program need some libraries:
- libcurl
- json-c

Finally, you also need a [Steam WebAPI key](http://steamcommunity.com/dev/apikey). Write it in a file named _STEAMKEY_ along other required files (like *items_game.txt*). Those files need to be in the current directory, when executing this program.

## How to test ?

### Compile

There is a Makefile so you can compile it with only one command:

```
make all
```

_NOTE: If you have a CS:GO installation under hand, you might want to set **CSGOPATH** variable in the Makefile._

### Run it

*Usage: csgofloat [options] _SteamID_*

As 'SteamID', you should use:
- SteamID64
- Custom name as in profile URL
- Any url containing _steamcommunity.com/_ and _/id/..._

This program need two files from CS:GO, you will need a copy of the game if you want to update them before I recommit.

#### Columns
- Item name
- Skin name
- Date when item will be tradable (local time)
- Name of item
- Stickers, denoted by a pipe when present
- Wear float value
- Condition
- Condition in the range of the exterior quality

#### Colors
If you enable ANSI escape codes with _a_ option and it is supported, output will be colored:
- Only grey if the item has no float
- With red if the item is not tradable
- More blue means better condition
- More green means better condition in the range of the exterior quality

#### Options
Letter     | Description
-----------|-----------
a          | Enable ANSI escape codes (mainly to colorize)
f          | Hide items without float
k key      | Use a specific Steam WebAPI key
s _string_ | Search for items whose names contain the string (case insensitive)
u          | Update downloadable files (_schema.txt_)
