default: all

all: trace2cmt cmtreplay

trace2cmt: trace2cmt.cpp

cmtreplay: cmtreplay.cpp

clean:
	rm -f trace2cmt cmtreplay

