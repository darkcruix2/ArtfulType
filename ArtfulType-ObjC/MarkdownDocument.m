#import "MarkdownDocument.h"

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
#define kToolbarTheme @"ToolbarTheme"
#define kToolbarInsertTime @"ToolbarInsertTime"
#define kToolbarInsertDate @"ToolbarInsertDate"
@interface MarkdownDocument ()
- (void)updatePreview;
- (void)updateStatusBar;
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
    [toolbar setDisplayMode:NSToolbarDisplayModeIconOnly];
    [window setToolbar:toolbar];
    
    // Create Split View
    NSView *containerView = [[NSView alloc] initWithFrame:windowRect];
    [containerView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    editorContainerView = containerView;
    
    // Create Text View inside a Scroll View
    NSScrollView *scrollView = [[NSScrollView alloc] initWithFrame:windowRect];
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
    WebView *webView = [[WebView alloc] initWithFrame:windowRect];
    [webView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    
    NSString *resourcesPath = [[NSBundle mainBundle] resourcePath];
    NSString *templatePath = [resourcesPath stringByAppendingPathComponent:@"template.html"];
    NSString *templateHTML = [NSString stringWithContentsOfFile:templatePath encoding:NSUTF8StringEncoding error:nil];
    [[webView mainFrame] loadHTMLString:templateHTML baseURL:[NSURL fileURLWithPath:resourcesPath]];
    
    self.previewWebView = webView;
    
    [containerView addSubview:scrollView];
    [containerView addSubview:webView];
    
    // Create Status Bar
    NSView *statusBar = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 800, 24)];
    [statusBar setAutoresizingMask:NSViewWidthSizable | NSViewMaxYMargin];
    
    statusModeLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 2, 150, 18)];
    [statusModeLabel setBezeled:NO];
    [statusModeLabel setDrawsBackground:NO];
    [statusModeLabel setEditable:NO];
    [statusModeLabel setSelectable:NO];
    [statusModeLabel setStringValue:@"Markdown Mode"];
    
    statusWordCountLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(600, 2, 80, 18)];
    [statusWordCountLabel setAutoresizingMask:NSViewMinXMargin];
    [statusWordCountLabel setBezeled:NO];
    [statusWordCountLabel setDrawsBackground:NO];
    [statusWordCountLabel setEditable:NO];
    [statusWordCountLabel setSelectable:NO];
    [statusWordCountLabel setAlignment:NSRightTextAlignment];
    [statusWordCountLabel setStringValue:@"0 words"];
    
    statusCharCountLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(690, 2, 90, 18)];
    [statusCharCountLabel setAutoresizingMask:NSViewMinXMargin];
    [statusCharCountLabel setBezeled:NO];
    [statusCharCountLabel setDrawsBackground:NO];
    [statusCharCountLabel setEditable:NO];
    [statusCharCountLabel setSelectable:NO];
    [statusCharCountLabel setAlignment:NSRightTextAlignment];
    [statusCharCountLabel setStringValue:@"0 chars"];
    
    [statusBar addSubview:statusModeLabel];
    [statusBar addSubview:statusWordCountLabel];
    [statusBar addSubview:statusCharCountLabel];
    
    // Main Container to hold splitView and statusBar
    NSView *mainContainer = [[NSView alloc] initWithFrame:windowRect];
    [mainContainer setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    
    [containerView setFrame:NSMakeRect(0, 24, 800, 576)];
    [containerView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    
    // Default to Markdown mode: hide the preview pane
    [webView setPolicyDelegate:(id)self];
    [webView setHidden:YES];
    
    [mainContainer addSubview:statusBar];
    [mainContainer addSubview:containerView];
    
    [window setContentView:mainContainer];
    
    NSWindowController *wc = [[NSWindowController alloc] initWithWindow:window];
    [self addWindowController:wc];
}

- (NSData *)dataOfType:(NSString *)typeName error:(NSError **)outError {
    if (isWriterMode) {
        NSString *markdown = [self fetchMarkdownFromWebView];
        if (markdown) {
            self.markdownContent = markdown;
            if (self.editorTextView) {
                self.editorTextView.string = markdown;
            }
        }
    } else if (self.editorTextView) {
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
        if (self.editorTextView) {
            self.editorTextView.string = loadedString;
        }
        return YES;
    }
    if (outError) {
        *outError = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil];
    }
    return NO;
}

- (void)textDidChange:(NSNotification *)notification {
    [self updateStatusBar];
}

- (void)updateStatusBar {
    NSString *text = @"";
    if (isWriterMode && self.previewWebView) {
        NSString *encoded = [self.previewWebView stringByEvaluatingJavaScriptFromString:@"encodeURIComponent(document.getElementById('content').innerText || document.getElementById('content').textContent || '')"];
        if (encoded && ![encoded isEqualToString:@"undefined"]) {
            text = [encoded stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
        }
    } else if (self.editorTextView) {
        text = self.editorTextView.string;
    }
    
    if (!text) text = @"";
    
    NSUInteger charCount = [text length];
    
    NSCharacterSet *separators = [NSCharacterSet whitespaceAndNewlineCharacterSet];
    NSArray *words = [text componentsSeparatedByCharactersInSet:separators];
    NSUInteger wordCount = 0;
    for (NSString *word in words) {
        if ([word length] > 0) wordCount++;
    }
    
    [statusWordCountLabel setStringValue:[NSString stringWithFormat:@"%lu words", (unsigned long)wordCount]];
    [statusCharCountLabel setStringValue:[NSString stringWithFormat:@"%lu chars", (unsigned long)charCount]];
}

- (void)webView:(WebView *)webView decidePolicyForNavigationAction:(NSDictionary *)actionInformation request:(NSURLRequest *)request frame:(WebFrame *)frame decisionListener:(id<WebPolicyDecisionListener>)listener {
    NSURL *url = [request URL];
    if ([[url scheme] isEqualToString:@"artfultype"]) {
        if ([[url host] isEqualToString:@"textdidchange"]) {
            [self updateStatusBar];
        }
        [listener ignore];
        return;
    }
    [listener use];
}

// Retrieve multi-line markdown from the WebView safely.
// Old WebKit's stringByEvaluatingJavaScriptFromString: truncates at the first
// newline character, so we split the content into individual lines in JS and
// fetch each line as a separate ObjC call, then re-join them here.
- (NSString *)fetchMarkdownFromWebView {
    NSInteger i;
    if (!self.previewWebView) return nil;
    NSString *countStr = [self.previewWebView stringByEvaluatingJavaScriptFromString:@"prepareMarkdownLines()"];
    if (!countStr || [countStr length] == 0) return nil;
    NSInteger lineCount = [countStr integerValue];
    NSMutableArray *lines = [NSMutableArray arrayWithCapacity:lineCount];
    for (i = 0; i < lineCount; i++) {
        NSString *js = [NSString stringWithFormat:@"getMarkdownLine(%ld)", (long)i];
        NSString *line = [self.previewWebView stringByEvaluatingJavaScriptFromString:js];
        if (!line) line = @"";
        [lines addObject:line];
    }
    return [lines componentsJoinedByString:@"\n"];
}

- (void)updatePreview {
    if (!self.previewWebView || !self.editorTextView) return;
    
    NSString *rawMarkdown = self.editorTextView.string;
    if (!rawMarkdown) return;
    
    NSString *escaped = [rawMarkdown stringByReplacingOccurrencesOfString:@"\\" withString:@"\\\\"];
    escaped = [escaped stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""];
    escaped = [escaped stringByReplacingOccurrencesOfString:@"\n" withString:@"\\n"];
    escaped = [escaped stringByReplacingOccurrencesOfString:@"\r" withString:@""];
    
    NSString *jsCode = [NSString stringWithFormat:@"if (typeof updateMarkdown === 'function') { updateMarkdown(\"%@\"); }", escaped];
    [self.previewWebView stringByEvaluatingJavaScriptFromString:jsCode];
}

// MARK: - Formatting Helpers
- (void)executeWriterCommand:(NSString *)command value:(NSString *)value {
    NSString *js;
    if (value) {
        js = [NSString stringWithFormat:@"document.execCommand('%@', false, '%@');", command, value];
    } else {
        js = [NSString stringWithFormat:@"document.execCommand('%@', false, null);", command];
    }
    [self.previewWebView stringByEvaluatingJavaScriptFromString:js];
}

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
        NSArray *lines = [selected componentsSeparatedByString:@"\n"];
        NSMutableArray *newLines = [NSMutableArray array];
        for (NSString *line in lines) {
            [newLines addObject:[NSString stringWithFormat:@"%@%@", prefix, line]];
        }
        NSString *replacement = [newLines componentsJoinedByString:@"\n"];
        [self.editorTextView insertText:replacement];
    }
}

// MARK: - IBActions
- (IBAction)toggleMode:(id)sender {
    if ([sender isKindOfClass:[NSSegmentedControl class]]) {
        NSSegmentedControl *seg = (NSSegmentedControl *)sender;
        BOOL wantsWriterMode = ([seg selectedSegment] == 0);
        if (isWriterMode == wantsWriterMode) return; 
        isWriterMode = wantsWriterMode;
    } else {
        isWriterMode = !isWriterMode;
    }

    NSScrollView *editorPane = [self.editorTextView enclosingScrollView];
    NSView *previewPane = self.previewWebView;
    
    if (isWriterMode) {
        // Entering Writer Mode: hide text editor, show full web view
        [self updatePreview]; // push current markdown into webview
        [editorPane setHidden:YES];
        [previewPane setHidden:NO];
        [statusModeLabel setStringValue:@"Writer Mode"];
        // Focus the contenteditable div via JS
        [self.previewWebView stringByEvaluatingJavaScriptFromString:@"var el = document.getElementById('content'); if (el) { el.focus(); }"];
    } else {
        // Entering Markdown Mode: pull text line-by-line from WebView
        NSString *markdown = [self fetchMarkdownFromWebView];
        if (markdown && [markdown length] > 0) {
            [self.editorTextView setString:markdown];
            self.markdownContent = markdown;
        }
        [self.previewWebView stringByEvaluatingJavaScriptFromString:@"if (document.activeElement) { document.activeElement.blur(); }"];
        [editorPane setHidden:NO];
        [previewPane setHidden:YES]; // Hide preview completely in Markdown mode
        [statusModeLabel setStringValue:@"Markdown Mode"];
    }
    
    [self updateStatusBar];
}

- (IBAction)insertBold:(id)sender {
    if (isWriterMode) { [self executeWriterCommand:@"bold" value:nil]; }
    else { [self wrapSelectionWith:@"**"]; }
}

- (IBAction)insertItalic:(id)sender {
    if (isWriterMode) { [self executeWriterCommand:@"italic" value:nil]; }
    else { [self wrapSelectionWith:@"*"]; }
}

- (IBAction)insertCodeBlock:(id)sender {
    if (isWriterMode) {
        // Simple code block execution for writer mode
        NSString *js = @"var sel = window.getSelection(); if (sel.rangeCount > 0) { var range = sel.getRangeAt(0); var code = document.createElement('code'); code.textContent = range.toString() || ' '; range.deleteContents(); range.insertNode(code); }";
        [self.previewWebView stringByEvaluatingJavaScriptFromString:js];
    } else {
        [self wrapSelectionWith:@"```\n"];
    }
}

- (IBAction)insertBulletList:(id)sender {
    if (isWriterMode) { [self executeWriterCommand:@"insertUnorderedList" value:nil]; }
    else { [self prefixSelectionWith:@"- "]; }
}

- (IBAction)insertNumberedList:(id)sender {
    if (isWriterMode) { [self executeWriterCommand:@"insertOrderedList" value:nil]; }
    else { [self prefixSelectionWith:@"1. "]; }
}

- (IBAction)insertBlockquote:(id)sender {
    if (isWriterMode) { [self executeWriterCommand:@"formatBlock" value:@"blockquote"]; }
    else { [self prefixSelectionWith:@"> "]; }
}

- (IBAction)insertHorizontalLine:(id)sender {
    if (isWriterMode) { [self executeWriterCommand:@"insertHorizontalRule" value:nil]; }
    else { [self.editorTextView insertText:@"\n---\n"]; }
}

- (IBAction)insertHeader:(id)sender {
    NSInteger index = -1;
    if ([sender isKindOfClass:[NSPopUpButton class]]) {
        index = [(NSPopUpButton *)sender indexOfSelectedItem];
    }
    
    if (isWriterMode) {
        if (index >= 0 && index < 6) {
            NSString *tag = [NSString stringWithFormat:@"H%ld", (long)(index + 1)];
            [self executeWriterCommand:@"formatBlock" value:tag];
        } else {
            [self executeWriterCommand:@"formatBlock" value:@"H1"];
        }
    } else {
        if (index >= 0 && index < 6) {
            NSString *prefix = [@"" stringByPaddingToLength:(index+1) withString:@"#" startingAtIndex:0];
            [self prefixSelectionWith:[NSString stringWithFormat:@"%@ ", prefix]];
        } else {
            [self prefixSelectionWith:@"# "];
        }
    }
}

- (IBAction)changeTheme:(id)sender {
    if ([sender isKindOfClass:[NSPopUpButton class]]) {
        NSPopUpButton *btn = (NSPopUpButton *)sender;
        NSString *title = [btn titleOfSelectedItem];
        NSString *themeId = @"dracula";
        
        if ([title isEqualToString:@"Classic Mac"]) themeId = @"classic-mac";
        else if ([title isEqualToString:@"Win98"]) themeId = @"win98";
        else if ([title isEqualToString:@"Solaris"]) themeId = @"solaris-cde";
        else if ([title isEqualToString:@"Calm"]) themeId = @"calm-rs";
        else if ([title isEqualToString:@"BeOS"]) themeId = @"beos";
        
        NSString *jsCode = [NSString stringWithFormat:@"document.documentElement.setAttribute('data-theme', '%@');", themeId];
        [self.previewWebView stringByEvaluatingJavaScriptFromString:jsCode];
        
        // Update NSTextView colors based on theme roughly
        if ([themeId isEqualToString:@"dracula"]) {
            [self.editorTextView setBackgroundColor:[NSColor colorWithCalibratedRed:0.15 green:0.16 blue:0.21 alpha:1.0]];
            [self.editorTextView setTextColor:[NSColor colorWithCalibratedRed:0.97 green:0.97 blue:0.95 alpha:1.0]];
        } else if ([themeId isEqualToString:@"classic-mac"] || [themeId isEqualToString:@"win98"]) {
            [self.editorTextView setBackgroundColor:[NSColor whiteColor]];
            [self.editorTextView setTextColor:[NSColor blackColor]];
        } else if ([themeId isEqualToString:@"solaris-cde"]) {
            [self.editorTextView setBackgroundColor:[NSColor colorWithCalibratedRed:0.65 green:0.68 blue:0.73 alpha:1.0]];
            [self.editorTextView setTextColor:[NSColor blackColor]];
        } else if ([themeId isEqualToString:@"calm-rs"]) {
            [self.editorTextView setBackgroundColor:[NSColor colorWithCalibratedRed:0.92 green:0.91 blue:0.89 alpha:1.0]];
            [self.editorTextView setTextColor:[NSColor colorWithCalibratedRed:0.17 green:0.24 blue:0.31 alpha:1.0]];
        } else if ([themeId isEqualToString:@"beos"]) {
            [self.editorTextView setBackgroundColor:[NSColor whiteColor]];
            [self.editorTextView setTextColor:[NSColor blackColor]];
        }
    }
}

- (IBAction)insertTime:(id)sender {
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setTimeStyle:NSDateFormatterShortStyle];
    [formatter setDateStyle:NSDateFormatterNoStyle];
    NSString *timeString = [formatter stringFromDate:[NSDate date]];
    [self.editorTextView insertText:timeString];
}

- (IBAction)insertDate:(id)sender {
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setTimeStyle:NSDateFormatterNoStyle];
    [formatter setDateStyle:NSDateFormatterMediumStyle];
    NSString *dateString = [formatter stringFromDate:[NSDate date]];
    [self.editorTextView insertText:dateString];
}

// MARK: - NSToolbarDelegate
// MARK: - Aqua Toolbar Icon Drawing

- (NSImage *)makeIconSize:(float)sz {
    return [[NSImage alloc] initWithSize:NSMakeSize(sz, sz)];
}

- (NSColor *)iconColor {
    // Dark graphite matching macOS Aqua toolbar icon tint
    return [NSColor colorWithCalibratedRed:0.20 green:0.20 blue:0.20 alpha:0.85];
}

// Helper: create a properly-sized NSButton for use as a toolbar item view
- (NSButton *)toolbarButtonWithImage:(NSImage *)img action:(SEL)action tooltip:(NSString *)tip {
    NSButton *btn = [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 36, 26)];
    [btn setButtonType:NSMomentaryPushInButton];
    [btn setBezelStyle:NSTexturedRoundedBezelStyle];
    [btn setImage:img];
    [btn setImagePosition:NSImageOnly];
    [btn setTarget:self];
    [btn setAction:action];
    [btn setToolTip:tip];
    [btn setBordered:YES];
    return btn;
}

// Pencil icon for Writer mode segment
// Clean pencil icon for Writer mode
- (NSImage *)iconWriter {
    NSImage *img = [self makeIconSize:18];
    [img lockFocus];
    NSColor *c = [self iconColor];
    [c set];

    // Draw pencil as a rotated shape: tip at bottom-left, eraser at top-right
    // Main body parallelogram
    NSBezierPath *body = [NSBezierPath bezierPath];
    [body moveToPoint:NSMakePoint(4,  2)];   // bottom-left of body
    [body lineToPoint:NSMakePoint(7,  2)];   // bottom-right of body
    [body lineToPoint:NSMakePoint(16, 11)];  // top-right of body
    [body lineToPoint:NSMakePoint(13, 11)];  // top-left of body
    [body closePath];
    [body fill];

    // Eraser cap (small rectangle at top-right, slightly lighter)
    [[NSColor colorWithCalibratedWhite:0.5 alpha:0.9] set];
    NSBezierPath *eraser = [NSBezierPath bezierPath];
    [eraser moveToPoint:NSMakePoint(13, 11)];
    [eraser lineToPoint:NSMakePoint(16, 11)];
    [eraser lineToPoint:NSMakePoint(17, 13)];
    [eraser lineToPoint:NSMakePoint(14, 13)];
    [eraser closePath];
    [eraser fill];

    // Pencil tip triangle at bottom-left
    [c set];
    NSBezierPath *tipTri = [NSBezierPath bezierPath];
    [tipTri moveToPoint:NSMakePoint(4,  2)];   // right side of tip base
    [tipTri lineToPoint:NSMakePoint(7,  2)];   // right side of tip base
    [tipTri lineToPoint:NSMakePoint(5,  0)];   // actual sharp point
    [tipTri closePath];
    [tipTri fill];

    // Thin divider line between eraser and body
    [[NSColor colorWithCalibratedWhite:0.7 alpha:1.0] set];
    NSBezierPath *divider = [NSBezierPath bezierPath];
    [divider moveToPoint:NSMakePoint(13, 11)];
    [divider lineToPoint:NSMakePoint(16, 11)];
    [divider setLineWidth:0.5];
    [divider stroke];

    [img unlockFocus];
    return img;
}

// Markdown icon: M with right-pointing arrow (horizontal, like the Markdown logo)
- (NSImage *)iconMarkdownMode {
    NSImage *img = [self makeIconSize:22];
    [img lockFocus];
    NSColor *c = [self iconColor];
    [c set];

    // --- "M" on the left (x=0..11) ---
    // Left leg
    NSBezierPath *ll = [NSBezierPath bezierPathWithRect:NSMakeRect(0, 3, 2, 11)];
    [ll fill];
    // Right leg
    NSBezierPath *rl = [NSBezierPath bezierPathWithRect:NSMakeRect(9, 3, 2, 11)];
    [rl fill];
    // Left diagonal: top-left corner down to center bottom of V
    NSBezierPath *ld = [NSBezierPath bezierPath];
    [ld moveToPoint:NSMakePoint(0,  14)];
    [ld lineToPoint:NSMakePoint(2,  14)];
    [ld lineToPoint:NSMakePoint(6,   7)];
    [ld lineToPoint:NSMakePoint(4.5, 5)];
    [ld closePath];
    [ld fill];
    // Right diagonal: center bottom of V up to top-right corner
    NSBezierPath *rd = [NSBezierPath bezierPath];
    [rd moveToPoint:NSMakePoint(6,   7)];
    [rd lineToPoint:NSMakePoint(7.5, 5)];
    [rd lineToPoint:NSMakePoint(11, 14)];
    [rd lineToPoint:NSMakePoint(9,  14)];
    [rd closePath];
    [rd fill];

    // --- Downward arrow on the right (x=12..21) ---
    // Vertical shaft going down
    NSBezierPath *shaft = [NSBezierPath bezierPathWithRect:NSMakeRect(15, 8, 3, 11)];
    [shaft fill];
    // Arrowhead triangle pointing DOWN
    NSBezierPath *head = [NSBezierPath bezierPath];
    [head moveToPoint:NSMakePoint(12,  8)];   // left corner
    [head lineToPoint:NSMakePoint(21,  8)];   // right corner
    [head lineToPoint:NSMakePoint(16.5, 2)];  // tip pointing down
    [head closePath];
    [head fill];

    [img unlockFocus];
    return img;
}



- (NSImage *)iconBold {
    NSImage *img = [self makeIconSize:18];
    [img lockFocus];
    NSColor *c = [self iconColor];
    NSDictionary *attrs = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSFont fontWithName:@"Helvetica-Bold" size:15], NSFontAttributeName,
        c, NSForegroundColorAttributeName, nil];
    NSString *s = @"B";
    NSSize sz = [s sizeWithAttributes:attrs];
    [s drawAtPoint:NSMakePoint((18-sz.width)/2.0+1, (18-sz.height)/2.0) withAttributes:attrs];
    [img unlockFocus];
    return img;
}

- (NSImage *)iconItalic {
    NSImage *img = [self makeIconSize:18];
    [img lockFocus];
    NSColor *c = [self iconColor];
    NSFont *f = [NSFont fontWithName:@"Times-Italic" size:17];
    if (!f) f = [NSFont fontWithName:@"Georgia-Italic" size:15];
    if (!f) f = [NSFont systemFontOfSize:15];
    NSDictionary *attrs = [NSDictionary dictionaryWithObjectsAndKeys:
        f, NSFontAttributeName,
        c, NSForegroundColorAttributeName, nil];
    NSString *s = @"I";
    NSSize sz = [s sizeWithAttributes:attrs];
    [s drawAtPoint:NSMakePoint((18-sz.width)/2.0, (18-sz.height)/2.0) withAttributes:attrs];
    [img unlockFocus];
    return img;
}

- (NSImage *)iconCode {
    NSImage *img = [self makeIconSize:18];
    [img lockFocus];
    NSColor *c = [self iconColor];
    NSDictionary *attrs = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSFont fontWithName:@"Monaco" size:9], NSFontAttributeName,
        c, NSForegroundColorAttributeName, nil];
    NSString *s = @"</>";
    NSSize sz = [s sizeWithAttributes:attrs];
    [s drawAtPoint:NSMakePoint((18-sz.width)/2.0, (18-sz.height)/2.0+1) withAttributes:attrs];
    [img unlockFocus];
    return img;
}

- (NSImage *)iconBulletList {
    NSImage *img = [self makeIconSize:18];
    [img lockFocus];
    NSColor *c = [self iconColor];
    [c set];
    int rows[3] = {12, 7, 2};
    int i;
    for (i = 0; i < 3; i++) {
        int y = rows[i];
        NSBezierPath *dot = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(1, y+1, 3, 3)];
        [dot fill];
        NSBezierPath *line = [NSBezierPath bezierPath];
        [line moveToPoint:NSMakePoint(6, y+2)];
        [line lineToPoint:NSMakePoint(17, y+2)];
        [line setLineWidth:1.5];
        [line setLineCapStyle:NSRoundLineCapStyle];
        [line stroke];
    }
    [img unlockFocus];
    return img;
}

- (NSImage *)iconNumberedList {
    NSImage *img = [self makeIconSize:18];
    [img lockFocus];
    NSColor *c = [self iconColor];
    [c set];
    NSDictionary *numAttrs = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSFont boldSystemFontOfSize:6], NSFontAttributeName,
        c, NSForegroundColorAttributeName, nil];
    int rows[3] = {12, 7, 2};
    NSString *nums[3];
    nums[0] = @"1.";
    nums[1] = @"2.";
    nums[2] = @"3.";
    int i;
    for (i = 0; i < 3; i++) {
        int y = rows[i];
        [nums[i] drawAtPoint:NSMakePoint(1, y) withAttributes:numAttrs];
        NSBezierPath *line = [NSBezierPath bezierPath];
        [line moveToPoint:NSMakePoint(8, y+3)];
        [line lineToPoint:NSMakePoint(17, y+3)];
        [line setLineWidth:1.5];
        [line setLineCapStyle:NSRoundLineCapStyle];
        [line stroke];
    }
    [img unlockFocus];
    return img;
}

- (NSImage *)iconBlockquote {
    NSImage *img = [self makeIconSize:18];
    [img lockFocus];
    NSColor *c = [self iconColor];
    [c set];
    NSBezierPath *bar = [NSBezierPath bezierPathWithRect:NSMakeRect(2, 2, 2, 14)];
    [bar fill];
    int rows[3] = {12, 7, 2};
    int i;
    for (i = 0; i < 3; i++) {
        NSBezierPath *line = [NSBezierPath bezierPath];
        [line moveToPoint:NSMakePoint(7, rows[i]+2)];
        [line lineToPoint:NSMakePoint(16, rows[i]+2)];
        [line setLineWidth:1.5];
        [line setLineCapStyle:NSRoundLineCapStyle];
        [line stroke];
    }
    [img unlockFocus];
    return img;
}

- (NSImage *)iconHorizontalLine {
    NSImage *img = [self makeIconSize:18];
    [img lockFocus];
    NSColor *c = [self iconColor];
    [c set];
    NSBezierPath *tl = [NSBezierPath bezierPath];
    [tl moveToPoint:NSMakePoint(2, 13)];
    [tl lineToPoint:NSMakePoint(16, 13)];
    [tl setLineWidth:1.0];
    [tl stroke];
    NSBezierPath *ml = [NSBezierPath bezierPath];
    [ml moveToPoint:NSMakePoint(2, 9)];
    [ml lineToPoint:NSMakePoint(16, 9)];
    [ml setLineWidth:2.5];
    [ml setLineCapStyle:NSRoundLineCapStyle];
    [ml stroke];
    NSBezierPath *bl = [NSBezierPath bezierPath];
    [bl moveToPoint:NSMakePoint(2, 5)];
    [bl lineToPoint:NSMakePoint(16, 5)];
    [bl setLineWidth:1.0];
    [bl stroke];
    [img unlockFocus];
    return img;
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar {
    return [NSArray arrayWithObjects:
            kToolbarModeToggle,
            NSToolbarFlexibleSpaceItemIdentifier,
            kToolbarUndo,
            kToolbarRedo,
            NSToolbarSeparatorItemIdentifier,
            kToolbarBold,
            kToolbarItalic,
            kToolbarCode,
            NSToolbarSeparatorItemIdentifier,
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
        NSSegmentedControl *seg = [[NSSegmentedControl alloc] initWithFrame:NSMakeRect(0,0,76,24)];
        [seg setSegmentCount:2];
        // Icon-only segments: no text label
        [seg setLabel:@"" forSegment:0];
        [seg setLabel:@"" forSegment:1];
        [seg setImage:[self iconWriter]       forSegment:0];
        [seg setImage:[self iconMarkdownMode] forSegment:1];
        [seg setImageScaling:NSScaleNone forSegment:0];
        [seg setImageScaling:NSScaleNone forSegment:1];
        [seg setToolTip:@"Writer / Markdown Mode"];
        [seg setSelectedSegment:1];
        [seg setTarget:self];
        [seg setAction:@selector(toggleMode:)];
        [[seg cell] setTrackingMode:NSSegmentSwitchTrackingSelectOne];
        [item setView:seg];
        [item setMinSize:NSMakeSize(76,24)];
        [item setMaxSize:NSMakeSize(76,24)];
        [item setLabel:@"Mode"];
        [item setToolTip:@"Toggle Writer / Markdown Mode"];

    } else if ([itemIdentifier isEqualToString:kToolbarUndo]) {
        NSButton *btn = [self toolbarButtonWithImage:[NSImage imageNamed:NSImageNameGoLeftTemplate]
                                             action:@selector(undo:)
                                            tooltip:@"Undo"];
        [item setView:btn]; [item setLabel:@"Undo"];
        [item setMinSize:NSMakeSize(36,26)]; [item setMaxSize:NSMakeSize(36,26)];

    } else if ([itemIdentifier isEqualToString:kToolbarRedo]) {
        NSButton *btn = [self toolbarButtonWithImage:[NSImage imageNamed:NSImageNameGoRightTemplate]
                                             action:@selector(redo:)
                                            tooltip:@"Redo"];
        [item setView:btn]; [item setLabel:@"Redo"];
        [item setMinSize:NSMakeSize(36,26)]; [item setMaxSize:NSMakeSize(36,26)];

    } else if ([itemIdentifier isEqualToString:kToolbarBold]) {
        NSButton *btn = [self toolbarButtonWithImage:[self iconBold] action:@selector(insertBold:) tooltip:@"Bold"];
        [item setView:btn]; [item setLabel:@"Bold"];
        [item setMinSize:NSMakeSize(36,26)]; [item setMaxSize:NSMakeSize(36,26)];

    } else if ([itemIdentifier isEqualToString:kToolbarItalic]) {
        NSButton *btn = [self toolbarButtonWithImage:[self iconItalic] action:@selector(insertItalic:) tooltip:@"Italic"];
        [item setView:btn]; [item setLabel:@"Italic"];
        [item setMinSize:NSMakeSize(36,26)]; [item setMaxSize:NSMakeSize(36,26)];

    } else if ([itemIdentifier isEqualToString:kToolbarCode]) {
        NSButton *btn = [self toolbarButtonWithImage:[self iconCode] action:@selector(insertCodeBlock:) tooltip:@"Code"];
        [item setView:btn]; [item setLabel:@"Code"];
        [item setMinSize:NSMakeSize(36,26)]; [item setMaxSize:NSMakeSize(36,26)];

    } else if ([itemIdentifier isEqualToString:kToolbarBulletList]) {
        NSButton *btn = [self toolbarButtonWithImage:[self iconBulletList] action:@selector(insertBulletList:) tooltip:@"Bullet List"];
        [item setView:btn]; [item setLabel:@"Bullets"];
        [item setMinSize:NSMakeSize(36,26)]; [item setMaxSize:NSMakeSize(36,26)];

    } else if ([itemIdentifier isEqualToString:kToolbarNumberedList]) {
        NSButton *btn = [self toolbarButtonWithImage:[self iconNumberedList] action:@selector(insertNumberedList:) tooltip:@"Numbered List"];
        [item setView:btn]; [item setLabel:@"Numbers"];
        [item setMinSize:NSMakeSize(36,26)]; [item setMaxSize:NSMakeSize(36,26)];

    } else if ([itemIdentifier isEqualToString:kToolbarBlockquote]) {
        NSButton *btn = [self toolbarButtonWithImage:[self iconBlockquote] action:@selector(insertBlockquote:) tooltip:@"Blockquote"];
        [item setView:btn]; [item setLabel:@"Quote"];
        [item setMinSize:NSMakeSize(36,26)]; [item setMaxSize:NSMakeSize(36,26)];

    } else if ([itemIdentifier isEqualToString:kToolbarHorizontalLine]) {
        NSButton *btn = [self toolbarButtonWithImage:[self iconHorizontalLine] action:@selector(insertHorizontalLine:) tooltip:@"Horizontal Rule"];
        [item setView:btn]; [item setLabel:@"Rule"];
        [item setMinSize:NSMakeSize(36,26)]; [item setMaxSize:NSMakeSize(36,26)];
    }

    return item;
}

@end
