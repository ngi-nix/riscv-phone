include ../../common.mk

CFLAGS += -I..

obj = app_screen.o app_status.o app_form.o


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<

all: $(obj)

clean:
	rm -f *.o
