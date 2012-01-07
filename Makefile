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

clean:
	-rm -f $(OBJ)

realclean: clean
	-rm -f $(DEP) $(TARGET)
	-rm -rf html

%.d: %.c
	@set -e; rm -f $@; \
	 $(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
	 sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	 rm -f $@.$$$$

include $(DEP)
