APP_NAME = ArtfulType
SOURCES = main.m AppDelegate.m MarkdownDocument.m
CFLAGS = -Wall -framework Cocoa -framework WebKit
APP_BUNDLE = build/$(APP_NAME).app
APP_EXECUTABLE = $(APP_BUNDLE)/Contents/MacOS/$(APP_NAME)
APP_RESOURCES = $(APP_BUNDLE)/Contents/Resources

all:
	mkdir -p $(APP_BUNDLE)/Contents/MacOS
	mkdir -p $(APP_RESOURCES)
	cp Info.plist $(APP_BUNDLE)/Contents/
	cp style.css $(APP_RESOURCES)/
	cp marked.min.js $(APP_RESOURCES)/
	cp template.html $(APP_RESOURCES)/
	-cp ArtfulType.icns $(APP_RESOURCES)/
	gcc $(CFLAGS) $(SOURCES) -o $(APP_EXECUTABLE)

clean:
	rm -rf build

.PHONY: all clean
