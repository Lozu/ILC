CC = gcc
LD = ld

CFLAGS = -Wall -g

# Program name
NAME = ilc

# Installation prefix
prefix = /usr

# Musl package file (full path). Comment out to do musl-less build.
# If present IL compiler will be linked with it, instead of system libc.
MUSL = `pwd`/pkgs/musl-1.2.2.tar.gz

# Number of jobs to build musl. Has meaning only when MUSL variable
# declared.
JOBS = 8

### Don't edit anything below, unless you understand ###

localroot = local

srcdir = src
name = $(srcdir)/$(NAME)

sources = $(addprefix $(srcdir)/,\
	main.c utils.c cmdargs.c process.c lparse.c)
modules = $(sources:.c=.o)

ifdef MUSL
	extragoals += musl

	rcflags = $(CFLAGS) -nostdinc -I $(localroot)/include
	
	rld = $(LD)
	ldstart = -nostdlib\
			-L $(localroot)/lib\
			$(localroot)/lib/crt1.o
	ldend = -lc
else
	rld = $(CC)
endif

rcflags += -Wno-discarded-qualifiers -DDEBUG

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
