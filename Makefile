all: smallchat_server install

smallchat_server: smallchat_server.cpp

install:
	-mkdir output
	mv smallchat_server output/

.PHONY = clean all