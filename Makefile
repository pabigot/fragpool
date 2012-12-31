ifdef EXPOSE_INTERNALS
CPPFLAGS += -DFRAGPOOL_EXPOSE_INTERNALS=$(EXPOSE_INTERNALS)
endif # EXPOSE_INTERNALS

ifdef WITH_COVERAGE
OPTCFLAGS ?= -g
AUX_CFLAGS += -fprofile-arcs -ftest-coverage
AUX_LDFLAGS += -fprofile-arcs
endif # WITH_COVERAGE

ifeq ($(CROSS_COMPILE),msp430-)
OPTCFLAGS ?= -g -Os -ffunction-sections -fdata-sections
OPTLDFLAGS ?= -Wl,-gc-sections
endif # CROSS_COMPILE

OPTCFLAGS ?= -g -O
CPPFLAGS += -I.
CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar
GCOV = $(CROSS_COMPILE)gcov

CFLAGS = -Wall -Werror -ansi -std=c99 -pedantic $(OPTCFLAGS) $(CPPFLAGS) $(AUX_CFLAGS)
LDFLAGS = $(OPTLDFLAGS) $(AUX_LDFLAGS)

SRC = fragpool.c
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

TARGET = libfragpool.a

.PHONY: all
all: $(TARGET)

.PHONY: msp430
msp430:
	make CROSS_COMPILE=msp430- clean all

libfragpool.a: $(OBJ)
	$(AR) rv $@ $^

.PHONY: doc
doc:
	doxygen

.PHONY: astyle
ASTYLE_ARGS=--options=none --style=1tbs --indent=spaces=2 --indent-switches --pad-header
astyle:
	astyle $(ASTYLE_ARGS) -r '*.c' '*.h'

.PHONY: clean
clean:
	-rm -f $(OBJ)
	-rm -f *.gcov

.PHONY: realclean
realclean: clean
	-rm -f $(DEP) $(TARGET)
	-rm -f *.gcda *.gcno
	-rm -rf html

.PHONY: unittest
unittest:
	$(MAKE) realclean \
	&& $(MAKE) -C tests realclean \
	&& $(MAKE) EXPOSE_INTERNALS=1 all \
	&& $(MAKE) -C tests
	if [ -f fragpool.gcda ] ; then $(GCOV) -a $(SRC) ; fi

.PHONY: coverage
coverage:
	$(MAKE) realclean \
	&& $(MAKE) -C tests realclean \
	&& $(MAKE) WITH_COVERAGE=1 EXPOSE_INTERNALS=1 all \
	&& $(MAKE) -C tests coverage
	if [ -f fragpool.gcda ] ; then $(GCOV) -a $(SRC) ; fi

%.d: %.c
	@set -e; rm -f $@; \
	 $(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
	 sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	 rm -f $@.$$$$

-include $(DEP)
