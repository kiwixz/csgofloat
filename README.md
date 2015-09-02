# csgofloat

[Feel free to contact me by email.](mailto:kiwixz@users.noreply.github.com)

## Description

This is a simple software to check wear float of CS:GO items.

## What do I need ?

You need an usual development environnement (a C compiler, GNU Make, etc). The Makefile use `c99` as an alias for the compiler.

This program need some libraries:
- libcurl
- json-c

Finally, you also need a [Steam Web API key](http://steamcommunity.com/dev/apikey). Write it, surrounded by quotation marks ("), in a file named _STEAMKEY_ in root folder of this repository once cloned.

## How to test ?

### Compile

There is a Makefile so you can compile it with only one command:

```
make all
```

_NOTE: If you have a CS:GO installation under hand, you might want to set **CSGOPATH** variable in the Makefile._

### Run it

*Usage: csgofloat [-fu] _SteamID_*

As 'SteamID', you should use:
- SteamID64
- Custom name as in profile URL
- Any url containing _steamcommunity.com/_ and _/id/..._

This program need two files

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
- Only grey if the item has no float
- With red if the item is not tradable
- More blue means better condition
- More green means better condition in the range of the exterior quality

#### Options
- _f_ will hide items without float
- _u_ will update _schema.txt_
