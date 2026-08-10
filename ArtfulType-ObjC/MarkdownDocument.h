#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@interface MarkdownDocument : NSDocument {
    NSString *markdownContent;
    NSTextView *editorTextView;
    WebView *previewWebView;
    NSView *editorContainerView;
    BOOL isWriterMode;
    NSTextField *statusModeLabel;
    NSTextField *statusWordCountLabel;
    NSTextField *statusCharCountLabel;
}

@property (nonatomic, copy) NSString *markdownContent;

// IBOutlets to connect to the UI in Interface Builder
@property (assign) IBOutlet NSTextView *editorTextView;
@property (assign) IBOutlet WebView *previewWebView;

- (IBAction)toggleMode:(id)sender;
- (NSString *)fetchMarkdownFromWebView;

- (IBAction)insertBold:(id)sender;
- (IBAction)insertItalic:(id)sender;
- (IBAction)insertCodeBlock:(id)sender;
- (IBAction)insertHeader:(id)sender;
- (IBAction)insertBulletList:(id)sender;
- (IBAction)insertNumberedList:(id)sender;
- (IBAction)insertBlockquote:(id)sender;
- (IBAction)insertHorizontalLine:(id)sender;
- (IBAction)changeTheme:(id)sender;
- (IBAction)insertTime:(id)sender;
- (IBAction)insertDate:(id)sender;

@end
