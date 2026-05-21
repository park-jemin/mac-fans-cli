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
AGENT_WAKE_LABEL := com.mac-fans-cli.wake
AGENT_BOOT_LABEL := com.mac-fans-cli.boot
LEGACY_AGENT_LABEL := com.mac-fans-cli.fans

CPPFLAGS := -Iinclude
CFLAGS ?= -Wall -Wextra -Werror -O2
LDFLAGS := -framework IOKit -framework CoreFoundation

SRC := src/main.c src/fans.c src/fans_logic.c src/fans_saved.c
TEST_SRC := tests/test_fans_logic.c src/fans_logic.c
SAVED_TEST_SRC := tests/test_fans_saved.c src/fans_logic.c
SMC_TEST_SRC := tests/test_smc_format.c src/fans.c src/fans_logic.c src/fans_saved.c

.PHONY: all clean test install install-wake-agent uninstall sudo-refresh hardware-test info auto-all

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
	$(BINDIR)/fans info
	@$(MAKE) install-wake-agent FANS_BIN=$(BINDIR)/fans

define INSTALL_LAUNCH_AGENT
	@u="$$SUDO_USER"; \
	if [ -z "$$u" ] || [ "$$u" = "root" ]; then u="$$USER"; fi; \
	h=$$(eval echo ~$$u); \
	uid=$$(id -u $$u); \
	agent="$$h/Library/LaunchAgents/$(1).plist"; \
	mkdir -p "$$h/Library/LaunchAgents"; \
	printf '%s\n' \
		'<?xml version="1.0" encoding="UTF-8"?>' \
		'<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' \
		'<plist version="1.0"><dict>' \
		'<key>Label</key><string>$(1)</string>' \
		'<key>ProgramArguments</key><array>' \
		"<string>$(FANS_BIN)</string><string>restore</string><string>$(2)</string></array>" \
		'$(3)' \
		'</dict></plist>' > "$$agent"; \
	launchctl bootout "gui/$$uid" "$$agent" 2>/dev/null || true; \
	launchctl bootstrap "gui/$$uid" "$$agent"
endef

install-wake-agent:
	$(call INSTALL_LAUNCH_AGENT,$(AGENT_WAKE_LABEL),wake,<key>StartOnWake</key><true/><key>ThrottleInterval</key><integer>30</integer>)
	$(call INSTALL_LAUNCH_AGENT,$(AGENT_BOOT_LABEL),boot,<key>RunAtLoad</key><true/>)
	@u="$$SUDO_USER"; \
	if [ -z "$$u" ] || [ "$$u" = "root" ]; then u="$$USER"; fi; \
	h=$$(eval echo ~$$u); \
	uid=$$(id -u $$u); \
	legacy="$$h/Library/LaunchAgents/$(LEGACY_AGENT_LABEL).plist"; \
	launchctl bootout "gui/$$uid" "$$legacy" 2>/dev/null || true; \
	rm -f "$$legacy"; \
	echo "Installed wake and boot restore agents for $$u"

uninstall:
	@u="$$SUDO_USER"; \
	if [ -z "$$u" ] || [ "$$u" = "root" ]; then u="$$USER"; fi; \
	h=$$(eval echo ~$$u); \
	uid=$$(id -u $$u); \
	for label in $(AGENT_WAKE_LABEL) $(AGENT_BOOT_LABEL) $(LEGACY_AGENT_LABEL); do \
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
