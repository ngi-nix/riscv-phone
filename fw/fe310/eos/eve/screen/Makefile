include ../../../common.mk
CFLAGS += -I..

obj = window.o view.o page.o form.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o
