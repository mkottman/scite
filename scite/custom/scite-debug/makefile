CONFIGFLAGS=$(shell pkg-config --cflags gtk+-2.0)

all: unix-spawner-ex.so

unix-spawner-ex.so: unix-spawner-ex.c
	gcc -I/usr/include/lua5.1 -g $(CONFIGFLAGS) -shared -fPIC -o unix-spawner-ex.so unix-spawner-ex.c -lutil
    

