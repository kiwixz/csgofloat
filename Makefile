ifneq (,$(findstring d,$(MAKECMDGOALS))) # debug
  $(info * Debugging!)
  DEBUG=1

  DIR=bin/debug
  CFLAGS=-DDEBUG -g -O0
else # release
  DIR=bin/release
  CFLAGS=-DRELEASE
endif
DEPEND=$(addprefix $(DIR)/,.depend)
$(shell mkdir -p $(DIR))
$(shell touch -a $(DEPEND))

CSGOPATH=$(HOME)/.steam/steam/steamapps/common/Counter-Strike Global Offensive/csgo

NAME=csgofloat
PKGLIBS=libcurl json-c
LIBS=
MACRO=-D_POSIX_C_SOURCE=200809L
OPT=-O3 -ffast-math
OTHER=

CC=c99
WARN=-Wall -Wextra -Wformat=2 -Winit-self -Wmissing-include-dirs -Wdeclaration-after-statement -Wshadow -Wno-aggressive-loop-optimizations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wstack-protector -Wno-missing-field-initializers -Wno-switch -Wno-unused-parameter -Wno-format-nonliteral
CFLAGS+=$(OTHER) $(MACRO) $(WARN) $(shell pkg-config --cflags $(PKGLIBS))
ifndef DEBUG
	CFLAGS+= $(OPT)
endif
LDFLAGS=-march=native -pipe -m64 -m128bit-long-double $(OTHER) $(shell pkg-config --libs $(PKGLIBS)) $(LIBS)
SRC=$(wildcard src/*.c)
OBJ=$(addprefix $(DIR)/,$(notdir $(SRC:.c=.o)))
PATHNAME=$(addprefix $(DIR)/,$(NAME))

all: copy $(NAME)

copy:
	@-[ -f "steamanalyst.html" ] && grep -o '<tr role="row" class="\w*">.*</tr>' steamanalyst.html | sed -nr 's/<tr role="row"[^<]*<td[^<]*<\/td><td[^>]*>([^<]*)<\/td><td[^>]*>([^<]*)<\/td><td[^>]*>([^<]*)<\/td><td[^<]*<\/td><td[^>]*>([^<]*)<\/td><td[^>]*>[^<]*<\/td><td[^>]*>([^<]*|<span[^<]*<\/span>)<\/td><td[^>]*>[^<]*<\/td><\/tr>/\1 | \2 (\3);\4\n/pg' > steamanalyst
	@-cp -f "$(CSGOPATH)/scripts/items/items_game.txt" ./ || echo -e "\x1b[31;1mFailed to update required CS:GO files\x1b[0m"
	@-[ -f "$(CSGOPATH)/resource/csgo_english.txt" ] && iconv -f UTF16LE -t UTF8 "$(CSGOPATH)/resource/csgo_english.txt" | sed s/Paintkit/PaintKit/g > csgo_english.txt

run: all
	@echo -e "\x1b[33;1mLaunching...\x1b[0m"
	@echo
ifdef DEBUG
	@gdb --args $(PATHNAME) $(ARGS)
else
	@$(PATHNAME) $(ARGS)
endif

include $(DEPEND)

$(NAME): $(OBJ)
	@echo -e "\x1b[36mLinkage of $@...\x1b[0m"
	@$(CC) -o $(addprefix $(DIR)/,$@) $^ $(LDFLAGS)

$(addprefix $(DIR)/,%.o): src/%.c
	@echo -e "\x1b[32mCompilation of $<...\x1b[0m"
	@$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean

clean:
	@echo -e "\x1b[35mCleaning...\x1b[0m"
	@rm -Rf bin
	@find . -name '*~' -delete
