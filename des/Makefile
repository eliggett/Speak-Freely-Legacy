all:	des.a

# all: des descert certify

des.a:	des.o
	rm -f des.a
	ar cr des.a des.o

des: main.o des.o getpass.o misc.o
	$(CC) $(CFLAGS) des main.o des.o getpass.o misc.o

descert: descert.o des.o
	$(CC) $(CFLAGS) descert descert.o des.o

certify: descert testdata
	descert < testdata
	touch certify

clean:
	rm -f *.bak *.a *.o *.out core des descert certify

depend:
	makedepend -- $(CFLAGS) -- *.c

# DO NOT DELETE
