OPTCFLAGS = -g -O
CPPFLAGS = -I.
CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar
CFLAGS = -Wall -Werror -ansi -std=c99 -pedantic $(OPTCFLAGS) $(CPPFLAGS)

SRC = fragpool.c
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

TARGET = libfragpool.a

all: $(TARGET)

msp430:
	make CROSS_COMPILE=msp430- clean all

libfragpool.a: $(OBJ)
	$(AR) rv $@ $^

doc:
	doxygen

clean:
	-rm -f $(OBJ)
	-rm -f *.gcov

realclean: clean
	-rm -f $(DEP) $(TARGET)
	-rm -f *.gcda *.gcno
	-rm -rf html

coverage: realclean
	$(MAKE) OPTCFLAGS='-fprofile-arcs -ftest-coverage' 

%.d: %.c
	@set -e; rm -f $@; \
	 $(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
	 sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	 rm -f $@.$$$$

include $(DEP)
