APP_NAME = ArtfulType
SOURCES = main.m AppDelegate.m MarkdownDocument.m
CFLAGS = -Wall -framework Cocoa -framework WebKit

all:
	mkdir -p build
	gcc $(CFLAGS) $(SOURCES) -o build/$(APP_NAME)

clean:
	rm -rf build

.PHONY: all clean
