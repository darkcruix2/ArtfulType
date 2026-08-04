##############################################################################
# ArtfulType Pro — Carbon / OS X 10.4 build
#
# TOOLCHAIN OPTIONS
# ─────────────────
# A) i386 (Intel Tiger / Rosetta) via osxcross + clang [default]:
#      make
#    Produces ArtfulTypePro (Mach-O i386) — runs on Intel Tiger natively,
#    and on PPC Tiger only via Rosetta (10.4.4+).
#
# B) powerpc native via GCC cross-compiler [RECOMMENDED for real PPC Macs]:
#      make ppc
#    Requires /opt/ppc-darwin/bin/powerpc-apple-darwin8-gcc
#    Build the toolchain first:
#      sudo bash /mnt/volume1/MyCode/workspace/build-ppc-darwin-toolchain.sh
#
# C) Native on PPC Mac running OS X 10.4 with Xcode 2.x:
#      CC= ARCH=ppc make   (empty CC = system gcc)
##############################################################################

PPC_PREFIX   ?= /opt/ppc-darwin
OSXCROSS_BIN ?= /mnt/volume1/MyCode/workspace/osxcross/target/bin
SDK          ?= /mnt/volume1/MyCode/workspace/osxcross/target/SDK/MacOSX10.4u.sdk
ARCH         ?= i386
MINVER       ?= 10.4

# Make sure the osxcross linker (i386-apple-darwin8-ld) is found before /usr/bin/ld
export PATH := $(OSXCROSS_BIN):$(PPC_PREFIX)/bin:$(PATH)

# Toolchain selection: i386 uses osxcross/clang; ppc uses GCC cross-compiler
ifeq ($(ARCH),ppc)
  PPC_TARGET = powerpc-apple-darwin8
  CC    = $(PPC_PREFIX)/bin/$(PPC_TARGET)-gcc
  CXX   = $(PPC_PREFIX)/bin/$(PPC_TARGET)-g++
  STRIP = $(PPC_PREFIX)/bin/$(PPC_TARGET)-strip
  AR    = $(PPC_PREFIX)/bin/$(PPC_TARGET)-ar
else
  CC    = $(OSXCROSS_BIN)/$(ARCH)-apple-darwin8-clang
  CXX   = $(OSXCROSS_BIN)/$(ARCH)-apple-darwin8-clang++
  STRIP = $(OSXCROSS_BIN)/$(ARCH)-apple-darwin8-strip
  AR    = $(OSXCROSS_BIN)/$(ARCH)-apple-darwin8-ar
endif

APP_NAME  = ArtfulTypePro
BUNDLE    = $(APP_NAME).app

SRCS = main.c file.c markdown.c scrolling.c undo.c zoom.c splash.c TXNCompat.c
OBJS = $(SRCS:.c=.o)

CFLAGS = \
    -arch $(ARCH) \
    -mmacosx-version-min=$(MINVER) \
    -isysroot $(SDK) \
    -DARTFUL_PRO=1 \
    -DCARBON=1 \
    -I. \
    -Wall \
    -Wno-deprecated-declarations \
    -Wno-four-char-constants \
    -Wno-unused-variable \
    -std=gnu99 \
    -fpascal-strings \
    -g

# Clang/newer GCC warning suppressions (not supported by GCC 4.0 for PPC)
ifneq ($(ARCH),ppc)
CFLAGS += -Wno-pointer-sign -Wno-unknown-escape-sequence -Wno-unused-but-set-variable
endif

# GCC for PPC doesn't support -Wl,-dead_strip_dylibs or -Wl,-w
ifeq ($(ARCH),ppc)
LDFLAGS = \
    -arch $(ARCH) \
    -mmacosx-version-min=$(MINVER) \
    -isysroot $(SDK) \
    -framework Carbon
else
LDFLAGS = \
    -arch $(ARCH) \
    -mmacosx-version-min=$(MINVER) \
    -isysroot $(SDK) \
    -framework Carbon \
    -Wl,-w \
    -Wl,-dead_strip_dylibs
endif

##############################################################################
# Build rules
##############################################################################

.PHONY: all ppc bundle clean

all: $(APP_NAME)

# Convenience target: build native PPC binary
ppc:
	$(MAKE) ARCH=ppc APP_NAME=ArtfulTypePro-ppc
	@echo ""
	@file ArtfulTypePro-ppc

$(APP_NAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── App bundle ───────────────────────────────────────────────────────────────
bundle: $(APP_NAME)
	mkdir -p $(BUNDLE)/Contents/MacOS
	mkdir -p $(BUNDLE)/Contents/Resources
	cp $(APP_NAME) $(BUNDLE)/Contents/MacOS/$(APP_NAME)
	cp Info.plist  $(BUNDLE)/Contents/
	# Copy the resource fork as a classic .rsrc binary inside the bundle.
	# If you have a compiled .rsrc from Rez, place it at:
	#   $(BUNDLE)/Contents/Resources/application.rsrc
	@echo "Bundle created: $(BUNDLE)"
	@echo "Run:  open $(BUNDLE)   (on a PPC or Rosetta Mac)"

# ── Stripped release build ───────────────────────────────────────────────────
release: bundle
	$(STRIP) $(BUNDLE)/Contents/MacOS/$(APP_NAME)

clean:
	rm -f $(OBJS) $(APP_NAME)
	rm -rf $(BUNDLE)

##############################################################################
# Dependency notes
# ─────────────────
# The classic Retro68 'app/' directory is intentionally untouched.
# This Makefile builds ONLY the Carbon port in 'app-carbon/'.
#
# Resources (menus, dialogs, string tables) are loaded from the app's
# resource fork at runtime.  Use Rez (from Xcode) to compile:
#   Rez -useDF -o $(APP_NAME) main_pro.r
# Then add -useDF flag when running to find the resource fork inside the
# bundle.  Alternatively, compile to a flat .rsrc and place it in
#   $(BUNDLE)/Contents/Resources/application.rsrc
# and call HOpenResFile() with the path at startup.
##############################################################################
