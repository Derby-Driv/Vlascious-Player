# =============================================================================
# Krona Player — Top-level Makefile
# =============================================================================
# Usage:
#   make setup      — (once) install QEMU user-static on the host
#   make build-env  — build the ARM64 Docker image
#   make build      — compile the project inside the container
#   make test       — run unit tests inside the container
#   make clean      — remove local build artefacts
# =============================================================================

IMAGE_NAME  := r36s-dev-env
CONTAINER   := r36s-build
BUILD_DIR   := build

# ---- Host-side (one-time) --------------------------------------------------
.PHONY: setup
setup:
	@echo ">>> Installing QEMU user-static for ARM64 emulation..."
	sudo apt-get update && sudo apt-get install -y qemu-user-static binfmt-support
	@echo ">>> Registering binfmt handlers..."
	docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
	@echo ">>> Done. You can now run 'make build-env'."

# ---- Docker image -----------------------------------------------------------
.PHONY: build-env
build-env:
	@echo ">>> Building ARM64 Docker image '$(IMAGE_NAME)'..."
	docker build --platform linux/arm64 -t $(IMAGE_NAME) .

# ---- Compile inside container -----------------------------------------------
.PHONY: build
build: build-env
	@echo ">>> Compiling project inside ARM64 container..."
	docker run --rm --platform linux/arm64 \
		-v "$(CURDIR)":/project \
		-w /project \
		$(IMAGE_NAME) \
		bash -c " \
			mkdir -p $(BUILD_DIR) && \
			cd $(BUILD_DIR) && \
			cmake .. -DCMAKE_BUILD_TYPE=Release && \
			make -j\$$(nproc) \
		"
	@mkdir -p output/kronaplayer/assets
	@cp -r assets/* output/kronaplayer/assets/
	@cp $(BUILD_DIR)/krona_player output/kronaplayer/krona_player
	@echo ">>> Build complete! Copy the contents of 'output/' to your R36S."

# ---- Tests inside container -------------------------------------------------
.PHONY: test
test: build-env
	@echo ">>> Running tests inside ARM64 container..."
	docker run --rm --platform linux/arm64 \
		-v "$(CURDIR)":/project \
		-w /project \
		$(IMAGE_NAME) \
		bash -c " \
			mkdir -p $(BUILD_DIR) && \
			cd $(BUILD_DIR) && \
			cmake .. -DCMAKE_BUILD_TYPE=Debug && \
			make -j\$$(nproc) && \
			ctest --output-on-failure \
		"

# ---- Cleanup ----------------------------------------------------------------
.PHONY: clean
clean:
	@echo ">>> Cleaning build directory..."
	rm -rf $(BUILD_DIR)
