# add files here
FILES+=ringbuffer_test 


CC=gcc
CFLAGS=-I. -Wall
OBJECTS = $(FILES:=.o)

define \n


endef

%.o: %.c minunit.h
	$(CC) $(CFLAGS) -o $@ $<

build: $(OBJECTS)
	$(echo "")

test: build
	$(foreach f, $(OBJECTS), ./$(f)${\n})

clean:
	rm *.o