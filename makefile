targets := example-server-c example-server-cpp

all:: $(targets)

clean::
	rm -f $(targets)

example-server-c: example-server.c websock.h buffer.h base64.h
	clang -I. -s -O3 -std=gnu11 -D_GNU_SOURCE $< -lcrypto -o $@

example-server-cpp: example-server.cpp websock.hpp buffer.h base64.h
	clang++ -I. -s -O3 -std=c++14 -fno-exceptions -fno-rtti -fno-threadsafe-statics -D_GNU_SOURCE $< -lcrypto -o $@
