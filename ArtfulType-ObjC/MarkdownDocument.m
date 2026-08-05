#import "MarkdownDocument.h"

@implementation MarkdownDocument

- (instancetype)init {
    self = [super init];
    if (self) {
        _markdownContent = @"";
    }
    return self;
}

- (NSString *)windowNibName {
    // Override returning the nib file name of the document
    // If you need to use a subclass of NSWindowController or if your document supports multiple NSWindowControllers, you should remove this method and override -makeWindowControllers instead.
    return @"Document";
}

- (void)windowControllerDidLoadNib:(NSWindowController *)aController {
    [super windowControllerDidLoadNib:aController];
    // Add any code here that needs to be executed once the windowController has loaded the document's window.
    
    // Set up the editor text view
    self.editorTextView.string = self.markdownContent;
    self.editorTextView.delegate = self;
    
    [self updatePreview];
}

- (NSData *)dataOfType:(NSString *)typeName error:(NSError **)outError {
    // Insert code here to write your document to data of the specified type.
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
    // Insert code here to read your document from the given data of the specified type.
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

// NSTextViewDelegate method
- (void)textDidChange:(NSNotification *)notification {
    [self updatePreview];
}

- (void)updatePreview {
    if (!self.previewWebView || !self.editorTextView) return;
    
    NSString *currentMarkdown = self.editorTextView.string;
    
    // NOTE: For a true macOS 10.5 implementation, you'd integrate a C-based Markdown parser like 'sundown' here.
    // For this demonstration, we are just displaying the raw text wrapped in basic HTML.
    
    // Escape basic HTML entities to prevent rendering issues in raw mode
    NSString *escapedString = [currentMarkdown stringByReplacingOccurrencesOfString:@"&" withString:@"&amp;"];
    escapedString = [escapedString stringByReplacingOccurrencesOfString:@"<" withString:@"&lt;"];
    escapedString = [escapedString stringByReplacingOccurrencesOfString:@">" withString:@"&gt;"];
    
    // Replace newlines with <br> for basic preview
    NSString *htmlBody = [escapedString stringByReplacingOccurrencesOfString:@"\n" withString:@"<br/>"];
    
    NSString *fullHTML = [NSString stringWithFormat:@"<html><body style='font-family: Helvetica, sans-serif; padding: 20px;'>%@</body></html>", htmlBody];
    
    [[self.previewWebView mainFrame] loadHTMLString:fullHTML baseURL:nil];
}

@end
