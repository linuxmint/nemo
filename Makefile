.PHONY: build

# Variables
# =============================================================================
VAR_DOCKER_EXE   := docker compose
VAR_COMPOSE_FILE := docker-compose.yml
# =============================================================================


# Helpers
# =============================================================================
ARGS ?= $(filter-out $@,$(MAKECMDGOALS))
DOCKER := $(VAR_DOCKER_EXE) -f $(VAR_COMPOSE_FILE)
# =============================================================================


# Core Commands
# =============================================================================

build:
	$(DOCKER) run --rm build

fix_permissions:
	sudo chown -R $(USER) ./build
	sudo chown -R $(USER) ./build-output

install:
	sudo killall nemo-desktop nemo 2>/dev/null || true
	sleep 1
	sudo cp -rf build-output/nemo                    /usr/bin/nemo
	sudo cp -rf build-output/nemo-desktop            /usr/bin/nemo-desktop
	sudo cp -rf build-output/nemo-autorun-software   /usr/bin/nemo-autorun-software
	sudo cp -rf build-output/nemo-connect-server     /usr/bin/nemo-connect-server
	sudo cp -rf build-output/nemo-open-with          /usr/bin/nemo-open-with
	sudo cp -rf build-output/nemo-extensions-list    /usr/lib/x86_64-linux-gnu/nemo/nemo-extensions-list
	sudo cp -rf libnemo-private/org.nemo.gschema.xml /usr/share/glib-2.0/schemas/
	sudo glib-compile-schemas /usr/share/glib-2.0/schemas/

run_build:
	pkill nemo; sleep 1; build-output/nemo

# =============================================================================
