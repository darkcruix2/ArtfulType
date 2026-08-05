import os

content = """#import "MarkdownDocument.h"

#define kToolbarModeToggle @"ToolbarModeToggle"
#define kToolbarUndo @"ToolbarUndo"
#define kToolbarRedo @"ToolbarRedo"
#define kToolbarBold @"ToolbarBold"
#define kToolbarItalic @"ToolbarItalic"
#define kToolbarCode @"ToolbarCode"
#define kToolbarHeader @"ToolbarHeader"
#define kToolbarBulletList @"ToolbarBulletList"
#define kToolbarNumberedList @"ToolbarNumberedList"
#define kToolbarBlockquote @"ToolbarBlockquote"
#define kToolbarHorizontalLine @"ToolbarHorizontalLine"

@interface MarkdownDocument ()
- (void)updatePreview;
@end

@implementation MarkdownDocument

@synthesize markdownContent;
@synthesize editorTextView;
@synthesize previewWebView;

- (id)init {
    self = [super init];
    if (self) {
        self.markdownContent = @"";
        isWriterMode = NO;
    }
    return self;
}

- (void)makeWindowControllers {
    NSRect windowRect = NSMakeRect(0, 0, 800, 600);
    NSWindow *window = [[NSWindow alloc] initWithContentRect:windowRect 
                                                   styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask | NSMiniaturizableWindowMask) 
                                                     backing:NSBackingStoreBuffered 
                                                       defer:NO];
    [window setReleasedWhenClosed:YES];
    
    NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier:@"MarkdownEditorToolbar"];
    [toolbar setDelegate:self];
    [toolbar setAllowsUserCustomization:YES];
    [toolbar setAutosavesConfiguration:NO];
    [toolbar setDisplayMode:NSToolbarDisplayModeIconAndLabel];
    [window setToolbar:toolbar];
    
    // Create Split View
    NSSplitView *splitView = [[NSSplitView alloc] initWithFrame:windowRect];
    [splitView setVertical:YES];
    [splitView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    mainSplitView = splitView;
    
    // Create Text View inside a Scroll View
    NSScrollView *scrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 400, 600)];
    [scrollView setHasVerticalScroller:YES];
    [scrollView setHasHorizontalScroller:YES];
    [scrollView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    
    NSSize contentSize = [scrollView contentSize];
    NSTextView *textView = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, contentSize.width, contentSize.height)];
    [textView setMinSize:NSMakeSize(0.0, contentSize.height)];
    [textView setMaxSize:NSMakeSize(FLT_MAX, FLT_MAX)];
    [textView setVerticallyResizable:YES];
    [textView setHorizontallyResizable:NO];
    [textView setAutoresizingMask:NSViewWidthSizable];
    [[textView textContainer] setContainerSize:NSMakeSize(contentSize.width, FLT_MAX)];
    [[textView textContainer] setWidthTracksTextView:YES];
    [textView setDelegate:(id)self]; 
    
    NSFont *editorFont = [NSFont fontWithName:@"Monaco" size:13.0];
    if (!editorFont) {
        editorFont = [NSFont userFixedPitchFontOfSize:13.0];
    }
    [textView setFont:editorFont];
    [textView setTextContainerInset:NSMakeSize(10, 10)];
    
    [scrollView setDocumentView:textView];
    self.editorTextView = textView;
    self.editorTextView.string = self.markdownContent;
    
    // Create Web View
    WebView *webView = [[WebView alloc] initWithFrame:NSMakeRect(400, 0, 400, 600)];
    [webView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    
    NSString *resourcesPath = [[NSBundle mainBundle] resourcePath];
    NSString *templatePath = [resourcesPath stringByAppendingPathComponent:@"template.html"];
    NSString *templateHTML = [NSString stringWithContentsOfFile:templatePath encoding:NSUTF8StringEncoding error:nil];
    [[webView mainFrame] loadHTMLString:templateHTML baseURL:[NSURL fileURLWithPath:resourcesPath]];
    
    self.previewWebView = webView;
    
    [splitView addSubview:scrollView];
    [splitView addSubview:webView];
    
    [window setContentView:splitView];
    
    NSWindowController *wc = [[NSWindowController alloc] initWithWindow:window];
    [self addWindowController:wc];
    
    [self performSelector:@selector(updatePreview) withObject:nil afterDelay:0.5];
}

- (NSData *)dataOfType:(NSString *)typeName error:(NSError **)outError {
    if (self.editorTextView) {
        self.markdownContent = self.editorTextView.string;
    }
    NSData *data = [self.markdownContent dataUsingEncoding:NSUTF8StringEncoding];
    if (!data && outError) {
        *outError = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileWriteUnknownError userInfo:nil];
    }
    return data;
}

- (BOOL)readFromData:(NSData *)data ofType:(NSString *)typeName error:(NSError **)outError {
    NSString *loadedString = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    if (loadedString) {
        self.markdownContent = loadedString;
        return YES;
    }
    if (outError) {
        *outError = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil];
    }
    return NO;
}

- (void)textDidChange:(NSNotification *)notification {
    [self updatePreview];
}

- (void)updatePreview {
    if (!self.previewWebView || !self.editorTextView) return;
    
    NSString *rawMarkdown = self.editorTextView.string;
    if (!rawMarkdown) return;
    
    NSString *escaped = [rawMarkdown stringByReplacingOccurrencesOfString:@"\\\\" withString:@"\\\\\\\\"];
    escaped = [escaped stringByReplacingOccurrencesOfString:@"\\"" withString:@"\\\\\\""];
    escaped = [escaped stringByReplacingOccurrencesOfString:@"\\n" withString:@"\\\\n"];
    escaped = [escaped stringByReplacingOccurrencesOfString:@"\\r" withString:@""];
    
    NSString *jsCode = [NSString stringWithFormat:@"if (typeof updateMarkdown === 'function') { updateMarkdown(\\"%@\\"); }", escaped];
    [self.previewWebView stringByEvaluatingJavaScriptFromString:jsCode];
}

// MARK: - Formatting Helpers
- (void)wrapSelectionWith:(NSString *)wrapper {
    NSRange range = [self.editorTextView selectedRange];
    NSString *selected = [[self.editorTextView string] substringWithRange:range];
    NSString *replacement = [NSString stringWithFormat:@"%@%@%@", wrapper, selected, wrapper];
    [self.editorTextView insertText:replacement];
    if (range.length == 0) {
        [self.editorTextView setSelectedRange:NSMakeRange(range.location + [wrapper length], 0)];
    }
}

- (void)prefixSelectionWith:(NSString *)prefix {
    NSRange range = [self.editorTextView selectedRange];
    NSString *selected = [[self.editorTextView string] substringWithRange:range];
    if (range.length == 0) {
        [self.editorTextView insertText:prefix];
    } else {
        NSArray *lines = [selected componentsSeparatedByString:@"\\n"];
        NSMutableArray *newLines = [NSMutableArray array];
        for (NSString *line in lines) {
            [newLines addObject:[NSString stringWithFormat:@"%@%@", prefix, line]];
        }
        NSString *replacement = [newLines componentsJoinedByString:@"\\n"];
        [self.editorTextView insertText:replacement];
    }
}

// MARK: - IBActions
- (IBAction)toggleMode:(id)sender {
    isWriterMode = !isWriterMode;
    NSView *previewPane = [[mainSplitView subviews] objectAtIndex:1];
    [previewPane setHidden:isWriterMode];
    [mainSplitView adjustSubviews];
}
- (IBAction)insertBold:(id)sender { [self wrapSelectionWith:@"**"]; }
- (IBAction)insertItalic:(id)sender { [self wrapSelectionWith:@"*"]; }
- (IBAction)insertCodeBlock:(id)sender { [self wrapSelectionWith:@"```\\n"]; }
- (IBAction)insertBulletList:(id)sender { [self prefixSelectionWith:@"- "]; }
- (IBAction)insertNumberedList:(id)sender { [self prefixSelectionWith:@"1. "]; }
- (IBAction)insertBlockquote:(id)sender { [self prefixSelectionWith:@"> "]; }
- (IBAction)insertHorizontalLine:(id)sender { [self.editorTextView insertText:@"\\n---\\n"]; }
- (IBAction)insertHeader:(id)sender {
    if ([sender isKindOfClass:[NSPopUpButton class]]) {
        NSPopUpButton *btn = (NSPopUpButton *)sender;
        NSInteger index = [btn indexOfSelectedItem];
        if (index >= 0 && index < 6) {
            NSString *prefix = [@"" stringByPaddingToLength:(index+1) withString:@"#" startingAtIndex:0];
            [self prefixSelectionWith:[NSString stringWithFormat:@"%@ ", prefix]];
        }
    } else {
        [self prefixSelectionWith:@"# "];
    }
}

// MARK: - NSToolbarDelegate
- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar {
    return [NSArray arrayWithObjects:
            kToolbarModeToggle,
            NSToolbarFlexibleSpaceItemIdentifier,
            kToolbarBold,
            kToolbarItalic,
            kToolbarCode,
            NSToolbarSeparatorItemIdentifier,
            kToolbarHeader,
            kToolbarBulletList,
            kToolbarNumberedList,
            kToolbarBlockquote,
            kToolbarHorizontalLine,
            nil];
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
    return [self toolbarDefaultItemIdentifiers:toolbar];
}

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag {
    NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
    
    if ([itemIdentifier isEqualToString:kToolbarModeToggle]) {
        NSSegmentedControl *seg = [[NSSegmentedControl alloc] initWithFrame:NSMakeRect(0,0, 160, 24)];
        [seg setSegmentCount:2];
        [seg setLabel:@"Writer" forSegment:0];
        [seg setLabel:@"Markdown" forSegment:1];
        [seg setSelectedSegment:1];
        [seg setTarget:self];
        [seg setAction:@selector(toggleMode:)];
        [[seg cell] setTrackingMode:NSSegmentSwitchTrackingSelectOne];
        [item setView:seg];
        [item setMinSize:NSMakeSize(160, 24)];
        [item setMaxSize:NSMakeSize(160, 24)];
        [item setLabel:@"Mode"];
    } else if ([itemIdentifier isEqualToString:kToolbarHeader]) {
        NSPopUpButton *btn = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(0,0, 80, 24) pullsDown:NO];
        [btn addItemWithTitle:@"H1"];
        [btn addItemWithTitle:@"H2"];
        [btn addItemWithTitle:@"H3"];
        [btn addItemWithTitle:@"H4"];
        [btn addItemWithTitle:@"H5"];
        [btn addItemWithTitle:@"H6"];
        [btn setTarget:self];
        [btn setAction:@selector(insertHeader:)];
        [item setView:btn];
        [item setMinSize:NSMakeSize(80, 24)];
        [item setMaxSize:NSMakeSize(80, 24)];
        [item setLabel:@"Header"];
    } else {
        // Text buttons
        [item setTarget:self];
        if ([itemIdentifier isEqualToString:kToolbarBold]) { [item setLabel:@"Bold"]; [item setAction:@selector(insertBold:)]; }
        else if ([itemIdentifier isEqualToString:kToolbarItalic]) { [item setLabel:@"Italic"]; [item setAction:@selector(insertItalic:)]; }
        else if ([itemIdentifier isEqualToString:kToolbarCode]) { [item setLabel:@"Code"]; [item setAction:@selector(insertCodeBlock:)]; }
        else if ([itemIdentifier isEqualToString:kToolbarBulletList]) { [item setLabel:@"Bullet"]; [item setAction:@selector(insertBulletList:)]; }
        else if ([itemIdentifier isEqualToString:kToolbarNumberedList]) { [item setLabel:@"Number"]; [item setAction:@selector(insertNumberedList:)]; }
        else if ([itemIdentifier isEqualToString:kToolbarBlockquote]) { [item setLabel:@"Quote"]; [item setAction:@selector(insertBlockquote:)]; }
        else if ([itemIdentifier isEqualToString:kToolbarHorizontalLine]) { [item setLabel:@"Line"]; [item setAction:@selector(insertHorizontalLine:)]; }
        
        NSButton *btn = [[NSButton alloc] initWithFrame:NSMakeRect(0,0, 60, 24)];
        [btn setTitle:[item label]];
        [btn setBezelStyle:NSRoundedBezelStyle];
        [btn setTarget:self];
        [btn setAction:[item action]];
        [item setView:btn];
        [item setMinSize:NSMakeSize(60, 24)];
        [item setMaxSize:NSMakeSize(60, 24)];
    }
    
    return item;
}

@end
"""

with open("/mnt/volume1/MyCode/workspace/ArtfulType/ArtfulType-ObjC/MarkdownDocument.m", "w") as f:
    f.write(content)
