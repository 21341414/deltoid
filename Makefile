CC = gcc
CFLAGS = `pkg-config --cflags gtk4 glib-2.0` -fPIC
LDFLAGS = `pkg-config --libs gtk4 glib-2.0`

all: deltoidUI injector libdeltoid.so

libdeltoid.so: injected_lib.c
	$(CC) -shared -fPIC -o libdeltoid.so injected_lib.c -ldl -lpthread -lrt

deltoidUI: deltoidUI.o
	$(CC) -o deltoidUI deltoidUI.o $(LDFLAGS)

injector: Injector.o
	$(CC) -o injector Injector.o $(LDFLAGS)

%.o: %.c
	$(CC) -c $< $(CFLAGS)

clean:
	rm -f *.o deltoidUI injector libdeltoid.so

.PHONY: all clean
