UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
	CXX_FLAGS=--std=c++17 -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -mmacosx-version-min=12.0 -I /usr/local/include/QtCore/ -F/usr/local/lib -F/usr/local/lib -L/usr/local/lib/ -framework QtCore
else
	CXX_FLAGS=$(shell pkg-config --libs --cflags Qt5Core) --std=c++11
endif

CXX=clang++

SOURCES := $(wildcard *.cpp)
BINS := $(SOURCES:.cpp=)

.PHONY: all 
all: clean $(BINS)

%: 
	$(CXX) -Wall -Wextra -O2 -fPIC $(CXX_FLAGS) $@.cpp -o $@

clean:
	ls -1 *.cpp | sed 's#[.]cpp##g' | xargs rm -f
