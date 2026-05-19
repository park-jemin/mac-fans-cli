CC ?= clang
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
RPM ?= 3500
SUDO ?= sudo
FANS_BIN ?= $(BINDIR)/fans
KEEP_FORCED ?= 0

BUILD_DIR := build
TARGET := $(BUILD_DIR)/fans
TEST_TARGET := $(BUILD_DIR)/test_fans_logic
SMC_TEST_TARGET := $(BUILD_DIR)/test_smc_format

CPPFLAGS := -Iinclude
CFLAGS ?= -Wall -Wextra -Werror -O2
LDFLAGS := -framework IOKit -framework CoreFoundation

SRC := src/main.c src/fans.c src/fans_logic.c
TEST_SRC := tests/test_fans_logic.c src/fans_logic.c
SMC_TEST_SRC := tests/test_smc_format.c src/fans.c src/fans_logic.c

.PHONY: all clean test install uninstall sudo-refresh hardware-test info auto-all

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SRC) include/fans.h include/fans_logic.h include/smc.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

$(TEST_TARGET): $(TEST_SRC) include/fans_logic.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(TEST_SRC)

$(SMC_TEST_TARGET): $(SMC_TEST_SRC) include/fans.h include/fans_logic.h include/smc.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SMC_TEST_SRC) $(LDFLAGS)

test: $(TEST_TARGET) $(SMC_TEST_TARGET)
	$(TEST_TARGET)
	$(SMC_TEST_TARGET)

sudo-refresh:
	$(SUDO) -v

install: $(TARGET)
	$(SUDO) mkdir -p $(BINDIR)
	$(SUDO) cp $(TARGET) $(BINDIR)/fans
	$(SUDO) chown root:wheel $(BINDIR)/fans
	$(SUDO) chmod 4755 $(BINDIR)/fans
	$(BINDIR)/fans info

uninstall:
	$(SUDO) rm -f $(BINDIR)/fans

hardware-test: $(TARGET)
	@set -e; \
	cleanup() { \
		if [ "$(KEEP_FORCED)" != "1" ]; then \
			$(FANS_BIN) auto-all; \
		fi; \
	}; \
	trap cleanup EXIT INT TERM; \
	$(FANS_BIN) set-all $(RPM); \
	sleep 3; \
	$(FANS_BIN) info; \
	if [ "$(KEEP_FORCED)" = "1" ]; then \
		trap - EXIT INT TERM; \
		echo "Leaving fans forced at $(RPM) RPM because KEEP_FORCED=1"; \
	else \
		cleanup; \
		trap - EXIT INT TERM; \
	fi

info: $(TARGET)
	$(TARGET) info

auto-all: $(TARGET)
	$(FANS_BIN) auto-all

clean:
	rm -rf $(BUILD_DIR)
