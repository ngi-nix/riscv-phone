#CFLAGS =
LDFLAGS = -pthread -lgpiod
TARGET = esp32spid
obj = msgq.o spi.o tun.o

all: $(TARGET)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

$(TARGET): $(obj)
	$(CC) $(obj) $(LDFLAGS) -o $@

clean:
	rm -f $(TARGET) *.o
