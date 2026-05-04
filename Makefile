PREFIX ?= $(HOME)
CARGO ?= cargo
CC ?= gcc

RUST_UI_DIR = rust-ui
RUST_BIN = $(RUST_UI_DIR)/target/release/deltoid-ui

INJECTOR_SRC = Injector.c
INJECTOR_BIN = deltoid-injector

LIB_SRC = injected_lib.c
LIB_SO = libdeltoid.so

.PHONY: all rust-ui injector lib clean install

all: lib rust-ui injector

lib:
	$(CC) -shared -fPIC -O2 -o $(LIB_SO) $(LIB_SRC) -ldl -lpthread

rust-ui:
	cd $(RUST_UI_DIR) && $(CARGO) build --release
	cp $(RUST_BIN) ./deltoid-ui
	chmod +x ./deltoid-ui

injector:
	$(CC) -O2 -o $(INJECTOR_BIN) $(INJECTOR_SRC)

clean:
	rm -f deltoid-ui deltoid-injector $(LIB_SO)
	cd $(RUST_UI_DIR) && $(CARGO) clean

install: all
	install -Dm755 deltoid-ui $(PREFIX)/.local/bin/deltoid-ui
	install -Dm755 deltoid-injector $(PREFIX)/.local/bin/deltoid-injector
	install -Dm755 $(LIB_SO) $(PREFIX)/$(LIB_SO)
