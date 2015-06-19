#
# Makefile for Tranporter Mechanism
#
# This file is generated through an automated process.
#

SHELL = /bin/sh

SRC_PATH = src
GWLIB_PATH = gwlib/build
CC = gcc
MAKEINFO = @MAKEINFO@
PACKAGE = @PACKAGE@
ARCTOOL = libtool -static -o
RANLIB = ranlib
SHELL = /bin/sh
VERSION = 1.0
SUFFIX = 
LEX = flex
PERL = /usr/bin/perl
YACC = bison -y

LIBS=-lmysqlclient_r -lssl -lresolv -lm  -lpthread -lxml2 -L/usr/lib -lcrypto -lssl -L/usr/local/mysql/lib  -lmysqlclient -liconv -L$(GWLIB_PATH) -lgwlib
CFLAGS=-D_REENTRANT=1 -I. -Igw -g -O0 -DDARWIN=1 -D_LARGE_FILES= -I/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.10.sdk/usr/include/libxml2 -I/usr/include/openssl -I/usr/local/mysql/include -I$(GWLIB_PATH)/gwlib  -I$(GWLIB_PATH)
LDFLAGS=

# LIBS=-lmysqlclient_r -lssl -lresolv -lm -lpthread  -L/usr/include/libxml2 -lxml2 -lz -lpthread -liconv -lm -L/usr/lib -lcrypto -lssl -L/usr/local/mysql/lib -lmysqlclient_r -liconv -L$(GWLIB_PATH) -lgwlib
# CFLAGS=-D_REENTRANT=1 -I. -Igw -g -DDARWIN=1 -D_LARGE_FILES= -I/usr/include/libxml2 -I/usr/include/openssl -I/usr/local/mysql/include -I$(GWLIB_PATH)/gwlib -I$(GWLIB_PATH)
# LDFLAGS=

binsrcs =
sbinsrcs = \
	$(SRC_PATH)/main.c
progsrcs = $(binsrcs) $(sbinsrcs)
progobjs = $(progsrcs:.c=.o)
progs = $(progsrcs:.c=)

# include libgwlib static library
gwlib = $(GWLIB_PATH)/libgwlib.a

enginesrcs = $(wildcard $(SRC_PATH)/server/*.c)
engineobjs = $(filter-out $(progobjs),$(enginesrcs:.c=.o))

libs = $(gwlib)

all: progs 

progs: $(progs)

clean:
	find $(SRC_PATH) -name "*.o" -o -name "*.i" | xargs rm -f
	rm -f $(progs)

$(progs): $(libs) $(progobjs)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(@:=).o $(libs) $(LIBS)
