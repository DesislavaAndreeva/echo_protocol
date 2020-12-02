CDIR=$(CURDIR)
IDIR =$(CDIR)/src/h
CC=gcc
CFLAGS=-I$(IDIR)

ODIR=$(CDIR)/src/obj
SDIR=$(CDIR)/src

LIBS=-lm -lpthread

_DEPS = echo_main.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = echo_main.o echo_client.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(SDIR)/echo: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(IDIR)/*~ 
