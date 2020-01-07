CFLAGS += -Wall -Wextra -Wpedantic -std=gnu11 -Iinclude -pthread -O2 -flto
CFLAGS += -Werror
#CFLAGS += -g3
#CFLAGS += -O0
#CFLAGS += -fno-inline -fno-lto
#CFLAGS += -fsanitize=address
#CFLAGS += -fsanitize=thread
#CFLAGS += -fsanitize=undefined
#CFLAGS += -fPIC
ifneq ($(findstring GCC, $(shell $(CC) --version)), )
	CFLAGS += -ffat-lto-objects
	LDFLAGS += -fuse-ld=gold
endif
PREFIX ?= /usr/local

.PHONY: lib test
LIB = obj/lib/libcee.a
lib: obj $(LIB)

TEST = $(patsubst test/%.c, obj/test/%, $(wildcard test/*/*.c))
test: obj $(TEST)

obj:
	mkdir -p obj/lib
	mkdir -p obj/test
	for d in test/*; do mkdir obj/$$d; done

LIB_DEPS = $(wildcard include/cee/*.h)
LIB_OBJS = $(patsubst %.c, obj/%.o, $(wildcard lib/*.c))
obj/lib/%.o: lib/%.c $(LIB_DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(LIB): $(LIB_OBJS)
	$(AR) rcs $(LIB) $(LIB_OBJS)

$(TEST): obj/test/%: test/%.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< obj/lib/libcee.a $(LDFLAGS)

.PHONY: clean install uninstall
clean:
	rm -f obj/lib/*.o
	rm -f obj/lib/libcee.a
	for d in test/*; do rm -f obj/$$d/*; done

install: $(LIB)
	install -Dt $(PREFIX)/lib/ $(LIB)
	install -Dt $(PREFIX)/include/cee/ include/cee/*.h

uninstall:
	rm -f $(PREFIX)/lib/libcee.a
	rm -rf $(PREFIX)/include/cee
