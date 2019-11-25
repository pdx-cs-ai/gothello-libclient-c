gthclient.o: gthclient.c gthclient.h
	$(CC) $(CFLAGS) -c gthclient.c

clean:
	-rm -f gthclient.o
