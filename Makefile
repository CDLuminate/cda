CFLAGS  := -g -O2 -Wall -rdynamic -std=gnu11 \
  -pie -fPIC -fstack-protector-strong -Wformat -Werror=format-security \
  -Wno-date-time -Wno-format-extra-args -D_FORTIFY_SOURCE=2
INSTALL := install
DESTDIR :=
BIN     := /usr/bin/
LIBS    := -larchive

main: cda
%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<
cda: cda.o
	$(CC) $(CFLAGS) $< -o cda $(LIBS)
	
clean:
	$(RM) *.o cda
install: main
	$(INSTALL) -m 0755 cda $(DESTDIR)/$(BIN)
