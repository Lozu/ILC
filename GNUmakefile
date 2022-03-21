CC = gcc
LD = ld

CFLAGS = -Wall -g -O0

# Program name
NAME = ilc

# Installation prefix
prefix = /usr

# Musl package file (full path). Comment out to do musl-less build.
# If present IL compiler will be linked with it, instead of system libc.
#MUSL = `pwd`/pkgs/musl-1.2.2.tar.gz

# Number of jobs to build musl. Has meaning only when MUSL variable
# declared.
#JOBS = 8

### Don't edit anything below, unless you understand ###

localroot = local

srcdir = src
name = $(srcdir)/$(NAME)

sources = $(addprefix $(srcdir)/,\
	main.c global.c cmdargs.c process.c lparse.c remap.c func.c)
modules = $(sources:.c=.o)

rcflags += $(CFLAGS)

ifdef MUSL
	extragoals += musl
	rcflags = -nostdinc -I $(localroot)/include
	rld = $(LD)
	ldstart = -static -nostdlib\
			-L $(localroot)/lib\
			$(localroot)/lib/crt1.o
	ldend = -lc
else
	rld = $(CC)
endif

rcflags += -Wno-discarded-qualifiers

def: cmp


%.o: %.c
	$(CC) $(rcflags) -c -o $@ $<

cmp: $(extragoals) $(modules)
	$(rld) -o $(name) $(ldstart) $(filter %.o, $^) $(ldend)

musl: pkgs/musl-install.sh
	pkgs/musl-install.sh $(MUSL) `pwd`/$(localroot) $(JOBS)

install: cmp
	install -d $(DESTDIR)$(prefix)/bin
	install $(name) $(DESTDIR)$(prefix)/bin

clean:
	rm -f $(name) $(modules)

distclean: clean
	rm -rf $(localroot) pkgs/musl-1.2.2
