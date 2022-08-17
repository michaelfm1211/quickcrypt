PREFIX = /usr/local

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

CFLAGS += -Wall -Wextra -std=c99
LDFLAGS += -lsodium

all: CFLAGS += -c
all: qc

%.o: %.c
	@$(CC) $(CFLAGS) $< -o $@
	@echo "[CC] $@"

qc: $(OBJS)
	@$(CC) $(LDFLAGS) $^ -o $@
	@echo "[LD] $@"

debug: CFLAGS += -g -fsanitize=address
debug: $(SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

.PHONY: install
install: qc
	mkdir -p "${PREFIX}/bin"
	cp qc "${PREFIX}/bin"

.PHONY: clean
clean:
	rm -rf $(OBJS) qc debug

