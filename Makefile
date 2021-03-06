COMMIT=$(shell git rev-parse --short HEAD)
BRANCH=$(shell git rev-parse --abbrev-ref HEAD)

PROJECT_NAME=lox
THIS_OS=windows
THIS_ARCH=x86

ifeq ($(OS),Windows_NT)
	THIS_OS=windows
else
	UNAME_S := $(shell uname -s)
	THIS_ARCH=$(shell uname -m)
	ifeq ($(UNAME_S),Linux)
		THIS_OS=linux
	endif
	ifeq ($(UNAME_S),Darwin)
		THIS_OS=darwin
	endif
endif

ifeq (${OS},win)
	THIS_OS=windows
endif
ifeq (${OS},mac)
	THIS_OS=darwin
endif
ifeq (${OS},linux)
	THIS_OS=linux
endif

details:
	@echo "Detected OS: ${THIS_OS}"
	@echo "Detected Arch: ${THIS_ARCH}"
	@echo "Commit: ${COMMIT}"
	@echo "Branch: ${BRANCH}"
	@echo "-----"

install-deps:
	./install.sh

run:
	./build/${PROJECT_NAME}

run-test:
	./build/test

gen: details
# Updating the commit info in version.h
# Generating build projects
ifeq (${THIS_OS},windows)
	./tools/windows/x86/ssed -i 's/<commit>/${COMMIT}/g' src/version.h
	./tools/windows/x86/ssed -i 's/<branch>/${BRANCH}/g' src/version.h
	tools/windows/x86/premake5 vs2019 platform=windows
endif
ifeq (${THIS_OS},darwin)
	./tools/osx/x86/premake5 xcode4 platform=macosx
endif
ifeq (${THIS_OS},linux)
	sed -i 's/<commit>/${COMMIT}/g' src/version.h
	sed -i 's/<branch>/${BRANCH}/g' src/version.h
ifeq (${THIS_ARCH},aarch64)
	./tools/unix/arm/premake5 gmake platform=linuxARM64
else
	./tools/unix/x86/premake5 gmake platform=linux64
endif

endif

post-build:
	git checkout src/version.h

open:
	start .\projects\${PROJECT_NAME}.sln

binary-debug:
ifeq (${THIS_OS},windows)
	msbuild.exe ./projects/${PROJECT_NAME}.sln -p:Platform="windows";Configuration=Debug -target:${PROJECT_NAME}
endif
ifeq (${THIS_OS},darwin)
	xcodebuild -configuration "Debug" ARCHS="x86_64" -destination 'platform=macOS' -project "projects/${PROJECT_NAME}.xcodeproj" -target ${PROJECT_NAME}
endif
ifeq (${THIS_OS},linux)
	make -C projects ${PROJECT_NAME} config=debug_linux64
endif

binary-release: gen
ifeq (${THIS_OS},windows)
	msbuild.exe ./projects/${PROJECT_NAME}.sln -p:Platform="windows";Configuration=Release -target:${PROJECT_NAME}
endif
ifeq (${THIS_OS},darwin)
	xcodebuild -configuration "Release" ARCHS="x86_64" -destination 'platform=macOS' -project "projects/${PROJECT_NAME}.xcodeproj" -target ${PROJECT_NAME}
endif
ifeq (${THIS_OS},linux)
	make -C projects ${PROJECT_NAME} config=release_linux64
endif

build-test: gen
ifeq (${THIS_OS},windows)
	msbuild.exe ./projects/${PROJECT_NAME}.sln -p:Platform="windows";Configuration=Debug -target:test
endif
ifeq (${THIS_OS},darwin)
	xcodebuild -configuration "Debug" ARCHS="x86_64" -destination 'platform=macOS' -project "projects/test.xcodeproj" -target test
endif
ifeq (${THIS_OS},linux)
	make -C projects test config=debug_linux64
endif

build-release: gen binary-release post-build
build-debug: gen binary-debug post-build

# by default build a debug binary
build: build-debug

test: build-test
ifeq (${THIS_OS},windows)
	.\build\test.exe
endif
ifeq (${THIS_OS},darwin)
	./build/test
endif
ifeq (${THIS_OS},linux)
	./build/test
endif

clean-${PROJECT_NAME}:
ifeq (${THIS_OS},windows)
	rm -r build/${PROJECT_NAME}.exe
	rm -r projects/obj/Windows/Debug/${PROJECT_NAME}
	rm -r projects/obj/Windows/Release/${PROJECT_NAME}
else
	rm -f ./build/${PROJECT_NAME}
ifeq (${THIS_OS},linux)
	rm -Rf ./projects/obj/linux64/Debug/${PROJECT_NAME}
	rm -Rf ./projects/obj/linux64/Release/${PROJECT_NAME}
endif
ifeq (${THIS_OS},darwin)
	rm -f ./build/${PROJECT_NAME}
	rm -Rf ./projects/obj/macosx/Debug/${PROJECT_NAME}
	rm -Rf ./projects/obj/macosx/Release/${PROJECT_NAME}
endif
endif

clean:
ifeq (${THIS_OS},windows)
	rm -r build
	rm -r projects
else
	rm -Rf ./build/
	rm -Rf ./projects
endif
