WITH_COVERAGE ?= 0
ifeq ($(WITH_COVERAGE),1)
LDFLAGS += -fprofile-arcs
endif # WITH_COVERAGE

CPPFLAGS += -I..
FRAGPOOL_LIB = ../libfragpool.a
LIBS = $(FRAGPOOL_LIB) -lcunit
CFLAGS = -Wall -Werror -ansi -std=c99 -pedantic $(CPPFLAGS) $(OPTCFLAGS)

SRC = test-basic.c
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

TESTS = $(SRC:.c=)

test: $(TESTS)
	@for f in $(TESTS); do ./$$f ; done

test-basic: test-basic.o $(FRAGPOOL_LIB)
	$(CC) $(LDFLAGS) -o $@ $< $(LIBS)

clean:
	-rm -f $(OBJ)
	-rm -f *.gcov

realclean: clean
	-rm -f $(DEP) $(TESTS)
	-rm -f *.gcda *.gcno

coverage: realclean
	$(MAKE) OPTCFLAGS='-fprofile-arcs -ftest-coverage' LDFLAGS=-fprofile-arcs

%.d: %.c
	@set -e; rm -f $@; \
	 $(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
	 sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	 rm -f $@.$$$$

-include $(DEP)
