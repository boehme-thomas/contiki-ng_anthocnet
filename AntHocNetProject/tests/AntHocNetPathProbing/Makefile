# Makefile for AntHocNet Algo testing

CONTIKI_PROJECT = anthocnetpathprobingtest

all: $(CONTIKI_PROJECT)
CONTIKI = ../../../../contiki-ng

# that the new version of tsch-types.h is used
CFLAGS := -I../../../includes $(CFLAGS)
CFLAGS += -g #debug info

# include that so that multicast messages are available
MODULES += os/net/ipv6/multicast

# exclude files from /multicast, which use rpl, which is not with the new routing method
MODULES_SOURCES_EXCLUDES += esmrf.c esmrf.h smrf.c smrf.h
MODULES_REL += ../../../AntHocNet ../../../modules


MAKE_MAC = MAKE_MAC_TSCH
MAKE_NET = MAKE_NET_IPV6
MAKE_ROUTING = MAKE_ROUTING_OTHER

LDLIBS += -lm

include $(CONTIKI)/Makefile.include