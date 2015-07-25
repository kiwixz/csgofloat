# csgofloat

[Feel free to contact me by email.](mailto:kiwixz@users.noreply.github.com)

## Description

This is a simple software to check wear float of CS:GO items.

## What do I need ?

You need an usual development environnement (a C compiler, GNU Make, etc). The Makefile use `c99` as an alias for the compiler.

This program need some libraries:
- libcurl
- json-c

Finally, you also need a [Steam Web API key](http://steamcommunity.com/dev/apikey). Write it, surrounded by quotation marks _"_, in a file named _STEAMKEY_ in root folder of this repository once cloned.

## How to test ?

### Compile

There is a Makefile so you can compile it with only one command:

```
make all
```

### Run it

*Usage: csgofloat _SteamID_*

As 'SteamID', you should use:
- SteamID64
- Custom name as in profile URL
- Profile URL

The program will display the inventory of the SteamID, one item by line.
Columns are:
- Item name (without skin name, yet)
- Presence of stickers
- Wear float value
- Condition
- Condition in the range of the exterior quality

The program gives the date at which items will become tradable; it should be in local timezone.
Output is colored:
- Only grey mean that the item has no float
- With red means not tradable
- More blue means better condition
- More green means better condition in the range of the exterior quality
