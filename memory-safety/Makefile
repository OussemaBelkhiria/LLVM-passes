FINAL_RTLIB = build/libMemorySafetyRuntime.a

all: cmake-build rtlib

rtlib: c

.PHONY clean:
	rm -rf build target

build_dir:
	mkdir -p build

c: cmake-build
	cp build/libMemorySafetyRuntimeC.a $(FINAL_RTLIB)

cmake-build: build_dir
	cd build; cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
	ninja -Cbuild
