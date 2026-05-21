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
SAVED_TEST_TARGET := $(BUILD_DIR)/test_fans_saved
SMC_TEST_TARGET := $(BUILD_DIR)/test_smc_format
AGENT_LABEL := com.mac-fans-cli.fans
LEGACY_WAKE_LABEL := com.mac-fans-cli.wake
LEGACY_BOOT_LABEL := com.mac-fans-cli.boot

CPPFLAGS := -Iinclude
CFLAGS ?= -Wall -Wextra -Werror -O2
LDFLAGS := -framework IOKit -framework CoreFoundation

SRC := src/main.c src/fans.c src/fans_logic.c src/fans_saved.c
TEST_SRC := tests/test_fans_logic.c src/fans_logic.c
SAVED_TEST_SRC := tests/test_fans_saved.c src/fans_logic.c
SMC_TEST_SRC := tests/test_smc_format.c src/fans.c src/fans_logic.c src/fans_saved.c

.PHONY: all clean test install install-verify install-wake-agent uninstall sudo-refresh hardware-test info auto-all

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SRC) include/fans.h include/fans_logic.h include/smc.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

$(TEST_TARGET): $(TEST_SRC) include/fans_logic.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(TEST_SRC)

$(SAVED_TEST_TARGET): $(SAVED_TEST_SRC) include/fans_logic.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SAVED_TEST_SRC)

$(SMC_TEST_TARGET): $(SMC_TEST_SRC) include/fans.h include/fans_logic.h include/smc.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SMC_TEST_SRC) $(LDFLAGS)

test: $(TEST_TARGET) $(SAVED_TEST_TARGET) $(SMC_TEST_TARGET)
	$(TEST_TARGET)
	$(SAVED_TEST_TARGET)
	$(SMC_TEST_TARGET)

sudo-refresh:
	$(SUDO) -v

install: $(TARGET)
	$(SUDO) mkdir -p $(BINDIR)
	$(SUDO) cp $(TARGET) $(BINDIR)/fans
	$(SUDO) chown root:wheel $(BINDIR)/fans
	$(SUDO) chmod 4755 $(BINDIR)/fans
	@$(MAKE) install-wake-agent FANS_BIN=$(BINDIR)/fans
	@$(MAKE) install-verify FANS_BIN=$(BINDIR)/fans

install-verify:
	@u="$$SUDO_USER"; \
	if [ -z "$$u" ] || [ "$$u" = "root" ]; then u="$$USER"; fi; \
	echo "Verifying $(FANS_BIN) as $$u..."; \
	$(SUDO) -u "$$u" $(FANS_BIN) info || \
		echo "Warning: fans info verify failed; install finished anyway."

install-wake-agent:
	@u="$$SUDO_USER"; \
	if [ -z "$$u" ] || [ "$$u" = "root" ]; then u="$$USER"; fi; \
	h=$$(eval echo ~$$u); \
	uid=$$(id -u $$u); \
	agent="$$h/Library/LaunchAgents/$(AGENT_LABEL).plist"; \
	mkdir -p "$$h/Library/LaunchAgents"; \
	for label in $(LEGACY_WAKE_LABEL) $(LEGACY_BOOT_LABEL); do \
		old="$$h/Library/LaunchAgents/$$label.plist"; \
		launchctl bootout "gui/$$uid" "$$old" 2>/dev/null || true; \
		rm -f "$$old"; \
	done; \
	printf '%s\n' \
		'<?xml version="1.0" encoding="UTF-8"?>' \
		'<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' \
		'<plist version="1.0"><dict>' \
		'<key>Label</key><string>$(AGENT_LABEL)</string>' \
		'<key>ProgramArguments</key><array>' \
		"<string>$(FANS_BIN)</string><string>restore</string></array>" \
		'<key>RunAtLoad</key><true/>' \
		'<key>StartOnWake</key><true/>' \
		'<key>ThrottleInterval</key><integer>120</integer>' \
		'<key>StandardErrorPath</key><string>'"$$h"'/.config/mac-fans-cli/restore.err</string>' \
		'</dict></plist>' > "$$agent"; \
	launchctl bootout "gui/$$uid" "$$agent" 2>/dev/null || true; \
	launchctl bootstrap "gui/$$uid" "$$agent"; \
	if [ -d "$$h/.config/mac-fans-cli" ]; then chown -R "$$u" "$$h/.config/mac-fans-cli"; fi; \
	echo "Installed restore agent for $$u"

uninstall:
	@u="$$SUDO_USER"; \
	if [ -z "$$u" ] || [ "$$u" = "root" ]; then u="$$USER"; fi; \
	h=$$(eval echo ~$$u); \
	uid=$$(id -u $$u); \
	for label in $(AGENT_LABEL) $(LEGACY_WAKE_LABEL) $(LEGACY_BOOT_LABEL); do \
		agent="$$h/Library/LaunchAgents/$$label.plist"; \
		launchctl bootout "gui/$$uid" "$$agent" 2>/dev/null || true; \
		rm -f "$$agent"; \
	done; \
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
