#import <Cocoa/Cocoa.h>
#import "AppDelegate.h"

int main(int argc, const char * argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    NSApplication *app = [NSApplication sharedApplication];
    
    // Create AppDelegate
    AppDelegate *delegate = [[AppDelegate alloc] init];
    [app setDelegate:delegate];
    
    // Create Main Menu
    NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
    
    // App Menu
    NSMenuItem *appMenuItem = [[NSMenuItem alloc] initWithTitle:@"ArtfulType" action:nil keyEquivalent:@""];
    [mainMenu addItem:appMenuItem];
    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"ArtfulType"];
    NSMenuItem *quitMenuItem = [[NSMenuItem alloc] initWithTitle:@"Quit ArtfulType" action:@selector(terminate:) keyEquivalent:@"q"];
    [appMenu addItem:quitMenuItem];
    [appMenuItem setSubmenu:appMenu];
    
    // File Menu
    NSMenuItem *fileMenuItem = [[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""];
    NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
    [fileMenu addItemWithTitle:@"New" action:@selector(newDocument:) keyEquivalent:@"n"];
    [fileMenu addItemWithTitle:@"Open..." action:@selector(openDocument:) keyEquivalent:@"o"];
    [fileMenu addItemWithTitle:@"Save" action:@selector(saveDocument:) keyEquivalent:@"s"];
    [fileMenu addItemWithTitle:@"Save As..." action:@selector(saveDocumentAs:) keyEquivalent:@"S"];
    [fileMenuItem setSubmenu:fileMenu];
    [mainMenu addItem:fileMenuItem];
    
    // Edit Menu (needed for Copy/Paste)
    NSMenuItem *editMenuItem = [[NSMenuItem alloc] initWithTitle:@"Edit" action:nil keyEquivalent:@""];
    NSMenu *editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    [editMenu addItemWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];
    [editMenu addItemWithTitle:@"Redo" action:@selector(redo:) keyEquivalent:@"Z"];
    [editMenu addItem:[NSMenuItem separatorItem]];
    [editMenu addItemWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
    [editMenu addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
    [editMenu addItemWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
    [editMenu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
    [editMenuItem setSubmenu:editMenu];
    [mainMenu addItem:editMenuItem];
    
    // Format Menu
    NSMenuItem *formatMenuItem = [[NSMenuItem alloc] initWithTitle:@"Format" action:nil keyEquivalent:@""];
    NSMenu *formatMenu = [[NSMenu alloc] initWithTitle:@"Format"];
    
    [formatMenu addItemWithTitle:@"Toggle Mode" action:@selector(toggleMode:) keyEquivalent:@"m"];
    [formatMenu addItem:[NSMenuItem separatorItem]];
    [formatMenu addItemWithTitle:@"Bold" action:@selector(insertBold:) keyEquivalent:@"b"];
    [formatMenu addItemWithTitle:@"Italic" action:@selector(insertItalic:) keyEquivalent:@"i"];
    [formatMenu addItemWithTitle:@"Code" action:@selector(insertCodeBlock:) keyEquivalent:@"k"];
    [formatMenu addItemWithTitle:@"Blockquote" action:@selector(insertBlockquote:) keyEquivalent:@"q"];
    [formatMenu addItem:[NSMenuItem separatorItem]];
    
    // Add Alt modifier for time/date
    NSMenuItem *timeItem = [formatMenu addItemWithTitle:@"Insert Time" action:@selector(insertTime:) keyEquivalent:@"t"];
    [timeItem setKeyEquivalentModifierMask:NSAlternateKeyMask | NSCommandKeyMask];
    
    NSMenuItem *dateItem = [formatMenu addItemWithTitle:@"Insert Date" action:@selector(insertDate:) keyEquivalent:@"d"];
    [dateItem setKeyEquivalentModifierMask:NSAlternateKeyMask | NSCommandKeyMask];
    
    [formatMenu addItem:[NSMenuItem separatorItem]];
    [formatMenu addItemWithTitle:@"Heading 1" action:@selector(insertHeader:) keyEquivalent:@"1"];
    [formatMenu addItemWithTitle:@"Heading 2" action:@selector(insertHeader:) keyEquivalent:@"2"];
    [formatMenu addItemWithTitle:@"Heading 3" action:@selector(insertHeader:) keyEquivalent:@"3"];
    [formatMenu addItemWithTitle:@"Heading 4" action:@selector(insertHeader:) keyEquivalent:@"4"];
    [formatMenu addItemWithTitle:@"Heading 5" action:@selector(insertHeader:) keyEquivalent:@"5"];
    [formatMenu addItemWithTitle:@"Heading 6" action:@selector(insertHeader:) keyEquivalent:@"6"];
    
    [formatMenuItem setSubmenu:formatMenu];
    [mainMenu addItem:formatMenuItem];
    
    [app setMainMenu:mainMenu];
    
    [app run];
    
    [pool drain];
    return 0;
}
