#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@interface MarkdownDocument : NSDocument <NSTextViewDelegate>

@property (nonatomic, copy) NSString *markdownContent;

// IBOutlets to connect to the UI in Interface Builder
@property (unsafe_unretained) IBOutlet NSTextView *editorTextView;
@property (assign) IBOutlet WebView *previewWebView;

@end
