# csgofloat

[Feel free to contact me by email.](mailto:kiwixz@users.noreply.github.com)

## Description

This is a simple software to check float of CSGO items.

## What do I need ?

You need an usual development environnement (a C compiler, GNU Make, etc). The Makefile use `c99` as an alias for the compiler.

This program need some libraries:
- libcurl
- json-c

You also need a [Steam Web API key](http://steamcommunity.com/dev/apikey). Write it in a file named _STEAMKEY_ in root folder of this repository once cloned.

## How to test ?

### Compile

There is a Makefile so you can also compile it with only one command:

```
make all
```

### Run it

*Usage: csgofloat _SteamID_*

The program will display the inventory of the SteamID, one item by line.
The fist column is the item name, the second column is the float, the third column is the condition and the fourth column is the condition in the range of the exterior quality.
The output is colored:
- Only grey mean that the item has no float
- With red means not tradable
- More blue means better condition
- More green means better condition in the range of the exterior quality
