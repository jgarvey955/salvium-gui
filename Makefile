ANDROID_STANDALONE_TOOLCHAIN_PATH ?= /usr/local/toolchain
MANUAL_SUBMODULES ?= OFF

host_system := $(shell uname -s)
windows_host := $(filter MINGW% MSYS% CYGWIN%,$(host_system))
native_prefix := $(shell cd $$(dirname $$(command -v $(CC)))/.. 2>/dev/null && pwd)
release_static_generator :=
release_static_platform_args :=
release_builddir := $(builddir)
release_topdir := $(topdir)
release_generator :=
release_platform_args :=

dotgit=$(shell ls -d .git/config)
ifeq ($(dotgit), .git/config)
  ifeq ($(shell git --version > /dev/null 2>&1 ; echo $$?), 0)
	git = yes
  else
    $(warning git command not found)
  endif
endif

builddir := build
topdir := ../..
BOOST_SYSTEM_LIBRARY ?= $(firstword $(wildcard /usr/lib/x86_64-linux-gnu/libboost_system.so /usr/lib/x86_64-linux-gnu/libboost_system.so.*))
ifneq ($(BOOST_SYSTEM_LIBRARY),)
  BOOST_SYSTEM_CMAKE_FLAGS := -DBoost_SYSTEM_LIBRARY_RELEASE=$(BOOST_SYSTEM_LIBRARY) -DBoost_SYSTEM_LIBRARY_DEBUG=$(BOOST_SYSTEM_LIBRARY)
endif
ifeq ($(USE_SINGLE_BUILDDIR), OFF)
  os := $(shell echo  `uname | sed -e 's|[:/\\ \(\)]|_|g'`)
  builddir := $(builddir)/$(os)
  topdir := $(topdir)/..

  ifdef git
    branch := $(shell git branch | grep '\* ' | cut -f2- -d' '| sed -e 's|[:/\\ \(\)]|_|g')
    builddir := $(builddir)/$(branch)
    topdir := $(topdir)/..
  endif

  deldirs := $(builddir)
else
  deldirs := $(builddir)/debug $(builddir)/release $(builddir)/fuzz
endif

release_static_builddir := $(builddir)
release_static_topdir := $(topdir)
release_static_environment :=
ifneq ($(windows_host),)
  release_builddir := build/$(if $(MSYSTEM),$(MSYSTEM),windows)-qt5-shared
  release_topdir := ../../..
  release_generator := -G "MSYS Makefiles"
  release_platform_args := -D ARCH="x86-64" -D BUILD_TAG="win-x64"
  release_static_builddir := build/$(if $(MSYSTEM),$(MSYSTEM),windows)-qt5-static
  release_static_topdir := ../../..
  release_static_environment := CMAKE_PREFIX_PATH="$(native_prefix)/qt5-static" PKG_CONFIG_PATH="$(native_prefix)/qt5-static/lib/pkgconfig"
  release_static_generator := -G "MSYS Makefiles"
  release_static_platform_args := -D ARCH="x86-64" -D BUILD_TAG="win-x64"
endif

default:
	mkdir -p build && cd build && cmake -D DEV_MODE=$(or ${DEV_MODE},OFF) -DMANUAL_SUBMODULES=${MANUAL_SUBMODULES} -D BUILD_64=ON -D CMAKE_BUILD_TYPE=Release $(BOOST_SYSTEM_CMAKE_FLAGS) .. && $(MAKE)
debug:
	mkdir -p build && cd build && cmake -D DEV_MODE=$(or ${DEV_MODE},ON) -DMANUAL_SUBMODULES=${MANUAL_SUBMODULES} -D CMAKE_BUILD_TYPE=Debug $(BOOST_SYSTEM_CMAKE_FLAGS) .. && $(MAKE) VERBOSE=1
debug-static:
	mkdir -p build && cd build && cmake -D STATIC=ON -D DEV_MODE=$(or ${DEV_MODE},ON) -DMANUAL_SUBMODULES=${MANUAL_SUBMODULES} -D CMAKE_BUILD_TYPE=Debug .. && $(MAKE) VERBOSE=1

depends:
	mkdir -p build/$(target)/release
	cd build/$(target)/release && cmake -D STATIC=ON -D DEV_MODE=$(or ${DEV_MODE},OFF) -DMANUAL_SUBMODULES=${MANUAL_SUBMODULES} -D BUILD_TAG=$(tag) -D CMAKE_BUILD_TYPE=Release -D CMAKE_TOOLCHAIN_FILE=/depends/$(target)/share/toolchain.cmake ../../.. && $(MAKE)

devmode:
	mkdir -p build && cd build && cmake -D DEV_MODE=$(or ${DEV_MODE},ON) -DMANUAL_SUBMODULES=${MANUAL_SUBMODULES} -D BUILD_64=ON -D CMAKE_BUILD_TYPE=Release .. && $(MAKE)
clean:
	mkdir -p build && cd build && rm -rf *
scanner:
	mkdir -p build && cd build && cmake -D DEV_MODE=$(or ${DEV_MODE},ON) -DMANUAL_SUBMODULES=${MANUAL_SUBMODULES} -D WITH_SCANNER=ON -D BUILD_64=ON -D CMAKE_BUILD_TYPE=Release .. && $(MAKE)

release:
	mkdir -p $(release_builddir)/release && cd $(release_builddir)/release && cmake $(release_generator) -D STATIC=OFF -D DEV_MODE=$(or ${DEV_MODE},OFF) -DMANUAL_SUBMODULES=${MANUAL_SUBMODULES} -D CMAKE_BUILD_TYPE=Release $(release_platform_args) $(release_topdir) && $(MAKE)

release-linux-armv8:
	mkdir -p $(builddir)/release && cd $(builddir)/release && cmake -D DEV_MODE=$(or ${DEV_MODE},OFF) -D ARCH="armv8-a" -D BUILD_64=ON -D CMAKE_BUILD_TYPE=Release -D BUILD_TAG="linux-armv8" $(topdir) && $(MAKE)

release-linux-ppc64le:
	mkdir -p $(builddir)/release && cd $(builddir)/release && cmake -D DEV_MODE=$(or ${DEV_MODE},OFF) -DMANUAL_SUBMODULES=${MANUAL_SUBMODULES} -D ARCH="ppc64le" -D CMAKE_BUILD_TYPE=Release $(topdir) && $(MAKE)

release-static:
	mkdir -p $(release_static_builddir)/release && cd $(release_static_builddir)/release && $(release_static_environment) cmake $(release_static_generator) -D STATIC=ON -D DEV_MODE=$(or ${DEV_MODE},OFF) -DMANUAL_SUBMODULES=${MANUAL_SUBMODULES} -D BUILD_64=ON -D CMAKE_BUILD_TYPE=Release $(release_static_platform_args) $(release_static_topdir) && $(MAKE)

release-static-mac-x86_64:
	mkdir -p $(builddir)/release &&	cd $(builddir)/release && cmake -D STATIC=ON -D DEV_MODE=$(or ${DEV_MODE},OFF) -DMANUAL_SUBMODULES=${MANUAL_SUBMODULES} -D ARCH="x86-64" -D BUILD_64=ON -D CMAKE_BUILD_TYPE=Release $(topdir) && $(MAKE)

debug-static-win64:
	mkdir -p $(builddir)/debug && cd $(builddir)/debug && cmake -D STATIC=ON -G "MSYS Makefiles" -D DEV_MODE=$(or ${DEV_MODE},ON) -DMANUAL_SUBMODULES=${MANUAL_SUBMODULES} -D ARCH="x86-64" -D BUILD_64=ON -D CMAKE_BUILD_TYPE=Debug -D BUILD_TAG="win-x64" -D CMAKE_TOOLCHAIN_FILE=$(topdir)/cmake/64-bit-toolchain.cmake -D MSYS2_FOLDER=$(shell cd ${MINGW_PREFIX}/.. && pwd -W) -D MINGW=ON $(topdir) && $(MAKE)

debug-static-mac64:
	mkdir -p $(builddir)/debug
	cd $(builddir)/debug && cmake -D STATIC=ON -D DEV_MODE=$(or ${DEV_MODE},ON) -DMANUAL_SUBMODULES=${MANUAL_SUBMODULES} -D ARCH="x86-64" -D BUILD_64=ON -D CMAKE_BUILD_TYPE=Debug -D BUILD_TAG="mac-x64" $(topdir) && $(MAKE)

release-static-win64:
	mkdir -p $(builddir)/release && cd $(builddir)/release && cmake -D STATIC=ON -G "MSYS Makefiles" -D DEV_MODE=$(or ${DEV_MODE},OFF) -DMANUAL_SUBMODULES=${MANUAL_SUBMODULES} -D ARCH="x86-64" -D BUILD_64=ON -D CMAKE_BUILD_TYPE=Release -D BUILD_TAG="win-x64" -D CMAKE_TOOLCHAIN_FILE=$(topdir)/cmake/64-bit-toolchain.cmake -D MSYS2_FOLDER=$(shell cd ${MINGW_PREFIX}/.. && pwd -W) -D MINGW=ON $(topdir) && $(MAKE)

release-win64:
	mkdir -p $(builddir)/release && cd $(builddir)/release && cmake -D STATIC=OFF -G "MSYS Makefiles" -D DEV_MODE=$(or ${DEV_MODE},OFF) -DMANUAL_SUBMODULES=${MANUAL_SUBMODULES} -D ARCH="x86-64" -D BUILD_64=ON -D CMAKE_BUILD_TYPE=Release -D BUILD_TAG="win-x64" -D CMAKE_TOOLCHAIN_FILE=$(topdir)/cmake/64-bit-toolchain.cmake -D MSYS2_FOLDER=$(shell cd ${MINGW_PREFIX}/.. && pwd -W) -D MINGW=ON $(topdir) && $(MAKE)
