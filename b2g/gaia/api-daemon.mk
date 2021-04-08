define build-sidl-daemon
  echo "run run-sidl-build-daemon";
  cd $(B2G_DIR)/sidl/daemon; RUST_BACKTRACE=1 BUILD_TYPE=debug RUST_LOG=debug WS_RUNTIME_TOKEN=secrettoken ${CARGO_HOME}/bin/cargo build --features=virtual-host; cd ../../;
endef

define run-sidl-release
  echo "run run-sidl-release";
  cd $(B2G_DIR)/sidl; BUILD_TYPE=build ./release_libs.sh; cd ..;
endef
