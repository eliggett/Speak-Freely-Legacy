
#   Make file for IDEA package

#   Everything will probably work OK without any changes below
#   this line.

#   Compiler flags

all:	idea.a

IDEAOBJS = idea.o

idea.a: $(IDEAOBJS)
	rm -f idea.a
	ar cr idea.a $(IDEAOBJS)

idea:	$(IDEAOBJS)
	$(CC) $(IDEAOBJS) -o idea

idea.o: idea.c idea.h

clean:
	rm -f core *.out *.a *.o *.bak idea *.shar

depend:
	makedepend -- $(CFLAGS) -- *.c

# DO NOT DELETE


idea.o: idea.h
