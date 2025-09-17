CONTIKI_PROJECT = anthocnetproject_multiple_sender_link_failure project-conf

all: $(CONTIKI_PROJECT)
CONTIKI = ../../../../../contiki-ng

#MAKE_MAC = MAKE_MAC_TSCH
MAKE_MAC = MAKE_MAC_CSMA
MAKE_NET = MAKE_NET_IPV6
MAKE_ROUTING = MAKE_ROUTING_OTHER

# that the new version of tsch-types.h / csma-output.h is used
ifeq ($(MAKE_MAC), MAKE_MAC_TSCH)
CFLAGS := -I../../../../includes/net/mac/tsch $(CFLAGS)
else ifeq ($(MAKE_MAC),MAKE_MAC_CSMA)
CFLAGS := -I../../../../includes/net/mac/csma $(CFLAGS)
endif
CFLAGS += -g # for debugging

MODULES_REL += ../../../../AntHocNet ../../../../modules

ifeq ($(MAKE_MAC), MAKE_MAC_TSCH)
MODULES_SOURCES_EXCLUDES += csma-output.c
else ifeq ($(MAKE_MAC),MAKE_MAC_CSMA)
MODULES_SOURCES_EXCLUDES += tsch-queue.c
endif

# to include the math library
LDLIBS = -lm

include $(CONTIKI)/Makefile.include