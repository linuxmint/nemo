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

# =============================================================================
