CC=gcc
CPPFLAGS=-g -Wall
USERID=123456789
CLASSES=

all: server client

server: $(CLASSES)
	$(CC) -o $@ $^ $(CPPFLAGS) $@.c

client: $(CLASSES)
	$(CC) -o $@ $^ $(CPPFLAGS) $@.c

clean:
	rm -rf *.o *~ *.gch *.swp *.dSYM server client *.tar.gz

dist: tarball

tarball: clean
	tar -cvzf /tmp/$(USERID).tar.gz --exclude=./.vagrant . && mv /tmp/$(USERID).tar.gz .
