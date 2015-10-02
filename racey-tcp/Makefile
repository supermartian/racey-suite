#
# Makefile
# Yuzhong Wen, 2015-10-02 10:47
#
#

LDFLAGS = -lpthread
CFLAGS = -static
TARGETS = racey-tcp-client racey-tcp-server

all: $(TARGETS)

racey-tcp-client:
	gcc $(CFLAGS) -o $@ racey-tcp-client.c $(LDFLAGS)

racey-tcp-server:
	gcc $(CFLAGS) -o $@ racey-tcp-server.c $(LDFLAGS)

clean:
	rm -rf $(TARGETS)

# vim:ft=make
#
