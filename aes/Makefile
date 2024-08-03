
all:	aes.a

aes.a:	aescrypt.o aeskey.o aestab.o
	rm -f aes.a
	ar cr aes.a aescrypt.o aeskey.o aestab.o

test:	aes_test
	./aes_test
	
aes_test:	aes.a
	cc -o aes_test aes_test.c aes.a
	
clean:
	rm -f *.o aes.a aes_test core core.* *.out
