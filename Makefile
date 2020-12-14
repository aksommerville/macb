all:
.SILENT:
PRECMD=echo "  $(@F)" ; mkdir -p $(@D) ;

# For 'make install':
SUDO:=sudo
INSTALLDST:=/usr/local/bin/macb

CC:=gcc -c -MMD -O2 -Isrc -Werror -Wimplicit
LD:=gcc
LDPOST:=

CFILES:=$(shell find src -name '*.c')
OFILES:=$(patsubst src/%.c,mid/%.o,$(CFILES))
-include $(OFILES:.o=.d)

mid/%.o:src/%.c;$(PRECMD) $(CC) -o $@ $<

EXE_MAIN:=out/macb
all:$(EXE_MAIN)
$(EXE_MAIN):$(OFILES);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)

clean:;rm -r mid out

install:$(EXE_MAIN);$(SUDO) cp $(EXE_MAIN) $(INSTALLDST) && echo "Installed '$(INSTALLDST)'"
uninstall:; \
  if ! [ -f "$(INSTALLDST)" ] ; then \
    echo "'$(INSTALLDST)' not found" ; \
    exit 1 ; \
  fi ; \
  $(SUDO) rm "$(INSTALLDST)" || exit 1 ; \
  echo "Deleted '$(INSTALLDST)'"
