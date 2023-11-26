ifeq ($(CROSS_COMPILE),aarch64-none-linux-gnu-) # space around "aarch64-none-linux-gnu-" would mess up string comparison
	CC:=aarch64-none-linux-gnu-gcc
else
	CC:=gcc
endif

$(info CROSS_COMPILE is $(CROSS_COMPILE))
$(info CC is $(CC))

# This is the default target, which will be built when 
# you invoke make
.PHONY: all
all: writer

# This rule tells make how to build hello from hello.cpp
writer: writer.c
	$(CC) -o writer writer.c

# This rule tells make to delete hello and hello.o
.PHONY: clean 
clean:
	rm -f writer *.o