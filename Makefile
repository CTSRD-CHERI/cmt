CFLAGS+=	-Wall

default: all

all: trace2cmt cmtreplay bumpmalloc.so

trace2cmt: trace2cmt.cpp

cmtreplay: cmtreplay.cpp

bumpmalloc.so: bumpmalloc.c
	$(CC) $(CFLAGS) -fPIC -shared -o bumpmalloc.so bumpmalloc.c

clean:
	rm -f trace2cmt cmtreplay bumpmalloc.so

