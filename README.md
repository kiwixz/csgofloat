# csgofloat

Feel free to contact me by email.

## Description

At first, this is a simple software to check wear float of CS:GO items. This is now my Swiss army knife as a trader. It uses SteamPowered APIs.

![](https://raw.githubusercontent.com/kiwixz/csgofloat/master/screenshot.png "What it looks like")

## What do I need ?

You need an usual development environnement (a C compiler, GNU Make, etc). The Makefile use `c99` as an alias for the compiler.

This program need some libraries:
- libcurl
- json-c

Finally, you also need a [Steam WebAPI key](http://steamcommunity.com/dev/apikey). You can either write it in a file named _STEAMKEY_ along other required files (like *items_game.txt*) or pass it using _k_ option. The required files need to be in the current directory, when executing this program.

## How to test ?

### Update files

If you have a CS:GO installation under hand, you might want to set _CSGOPATH_ variable in the Makefile, so *csgo_english.txt* and *items_game.txt* will be updated automatically.

To update SteamAnalyst cache, you need to go on [Price List](http://csgo.steamanalyst.com/list) and download the source of the page **once the list loaded**, to _steamanalyst.html_ in root of this repository.

### Compile

There is a Makefile so you can compile it with only one command:

```
make all
```

### Run it

*Usage: csgofloat [options] _SteamID_*

As 'SteamID', you should use:
- SteamID64
- Custom name as in profile URL
- Any url containing _steamcommunity.com/_ and _/id/_ or _/profiles/_

### Options
Letter     | Description
-----------|-----------
a          | Enable ANSI escape codes (mainly to colorize)
d          | Show names of sticked stickers
f          | Hide items without float
k key      | Use a specific Steam WebAPI key
m          | Display current market prices instead of cached ones from SteamAnalyst (only makes sense with -p)
p          | Display price of item on market (if possible)
s _string_ | Search for items whose names contain the string (case insensitive)
u          | Update downloadable files (_schema.txt_)

## What is the output ?

### Columns
- Item name (StatTrack and Souvenir skins will be specified as such)
- Skin name (if Doppler, phase will be given)
- Custom name of item
- If not, date when item will be tradable (local time)
- Price (if option _p_ is given)
- Stickers, denoted by a pipe when present
- Wear float value
- Wear condition
- Wear condition against exterior quality

Conditions are calculated considering range of possible wear float.
If option _d_ is given, a second line will give name of stickers.

### Colors
If you enable ANSI escape codes with _a_ option and it is supported, output will be colored.

Almost all figures will be colored from red to green. The custom name of item will be white and the date when item will be tradable will be colored from yellow (if soon) to red.

The item's name color depend of condition:
- Only grey if the item has no float
- More blue means better condition
- More green means better condition against exterior quality

## License
This program is protected by the **GNU GENERAL PUBLIC LICENSE**. This repositery should contain a file named _LICENSE_.