#
# Makefile
# Yuzhong Wen, 2015-10-02 10:47
#
#

LDFLAGS = -lpthread
TARGETS = racey-tcp-client racey-tcp-server

all: $(TARGETS)

racey-tcp-client:
	gcc -o $@ racey-tcp-client.c $(LDFLAGS)

racey-tcp-server:
	gcc -o $@ racey-tcp-server.c $(LDFLAGS)

clean:
	rm -rf $(TARGETS)

# vim:ft=make
#
