##############################################################################
# ArtfulType Pro — Carbon / PowerPC OS X 10.4 build
#
# TOOLCHAIN OPTIONS
# ─────────────────
# A) Cross-compile from Linux/modern macOS (recommended if you don't have a
#    PPC Mac handy):
#      export TOOLCHAIN_PREFIX = powerpc-apple-darwin8-
#      Set SDK below to the extracted MacOSX10.4u.sdk path.
#
# B) Native on PPC Mac running OS X 10.4–10.5 with Xcode 2.x:
#      export TOOLCHAIN_PREFIX =   (empty — use system cc/gcc)
#      Set SDK = /Developer/SDKs/MacOSX10.4u.sdk  (or leave blank)
#
# C) Rosetta on an Intel Mac running OS X 10.5 with Xcode 3.x:
#      export TOOLCHAIN_PREFIX =
#      Add -arch ppc to CFLAGS below and set SDK to the 10.4u SDK.
##############################################################################

TOOLCHAIN_PREFIX ?= powerpc-apple-darwin8-

CC      = $(TOOLCHAIN_PREFIX)gcc
STRIP   = $(TOOLCHAIN_PREFIX)strip
LIPO    = $(TOOLCHAIN_PREFIX)lipo

# Path to the MacOSX10.4 Universal SDK.
# Override on the command line: make SDK=/path/to/MacOSX10.4u.sdk
SDK     ?= /Developer/SDKs/MacOSX10.4u.sdk

# Minimum OS X version to support
MINVER  = 10.4

APP_NAME  = ArtfulTypePro
BUNDLE    = $(APP_NAME).app

SRCS = main.c file.c markdown.c scrolling.c undo.c zoom.c splash.c TXNCompat.c

OBJS = $(SRCS:.c=.o)

CFLAGS = \
    -arch ppc \
    -mmacosx-version-min=$(MINVER) \
    -isysroot $(SDK) \
    -DARTFUL_PRO=1 \
    -DCARBON=1 \
    -I. \
    -Wall \
    -Wno-deprecated-declarations \
    -g

LDFLAGS = \
    -arch ppc \
    -mmacosx-version-min=$(MINVER) \
    -isysroot $(SDK) \
    -framework Carbon

##############################################################################
# Build rules
##############################################################################

.PHONY: all bundle clean

all: $(APP_NAME)

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
