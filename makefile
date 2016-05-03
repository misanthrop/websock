all:: example-server

clean::
	rm -f example-server

#example-server: example-server.c websocket.h buffer.h base64.h
#	clang -I. -g -std=gnu11 -D_GNU_SOURCE $< -lcrypto -o $@

example-server: example-server.cpp websock.hpp buffer.h base64.h
	clang++ -I. -g -std=c++14 -D_GNU_SOURCE $< -lcrypto -o $@
