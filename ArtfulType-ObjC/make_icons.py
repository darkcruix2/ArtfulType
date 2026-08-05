#!/usr/bin/env python
# Generates 32x32 macOS Aqua-style toolbar icon TIFFs for ArtfulType
# Run on the legacy Mac: python make_icons.py

import AppKit
import CoreGraphics
import os

OUTPUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "icons")
if not os.path.exists(OUTPUT_DIR):
    os.makedirs(OUTPUT_DIR)

SIZE = 32

def make_image(draw_fn, name):
    """Create a 32x32 TIFF icon and save it."""
    img = AppKit.NSImage.alloc().initWithSize_(AppKit.NSMakeSize(SIZE, SIZE))
    img.lockFocus()

    # Clear background (transparent)
    AppKit.NSColor.clearColor().set()
    AppKit.NSRectFill(AppKit.NSMakeRect(0, 0, SIZE, SIZE))

    draw_fn()

    img.unlockFocus()

    # Save as TIFF
    tiff_data = img.TIFFRepresentation()
    path = os.path.join(OUTPUT_DIR, name + ".tiff")
    tiff_data.writeToFile_atomically_(path, True)
    print("Written: " + path)

def setup_aqua_text(text, font_size=18, bold=False):
    """Draw Aqua-styled text centered in the icon."""
    if bold:
        font = AppKit.NSFont.boldSystemFontOfSize_(font_size)
    else:
        font = AppKit.NSFont.systemFontOfSize_(font_size)

    # Shadow
    shadow = AppKit.NSShadow.alloc().init()
    shadow.setShadowColor_(AppKit.NSColor.colorWithCalibratedWhite_alpha_(0.0, 0.4))
    shadow.setShadowOffset_(AppKit.NSMakeSize(0, -1))
    shadow.setShadowBlurRadius_(2.0)

    # Main color - blue-grey matching Aqua system icons
    color = AppKit.NSColor.colorWithCalibratedRed_green_blue_alpha_(0.25, 0.35, 0.55, 1.0)

    attrs = AppKit.NSDictionary.dictionaryWithObjectsAndKeys_(
        font, AppKit.NSFontAttributeName,
        color, AppKit.NSForegroundColorAttributeName,
        shadow, AppKit.NSShadowAttributeName,
        None
    )
    size = AppKit.NSString.stringWithString_(text).sizeWithAttributes_(attrs)
    x = (SIZE - size.width) / 2.0
    y = (SIZE - size.height) / 2.0
    AppKit.NSString.stringWithString_(text).drawAtPoint_withAttributes_(
        AppKit.NSMakePoint(x, y), attrs
    )

def draw_bold():
    setup_aqua_text("B", font_size=20, bold=True)

def draw_italic():
    # Italic I - use oblique styling
    font = AppKit.NSFont.fontWithName_size_("Times-Italic", 22)
    if not font:
        font = AppKit.NSFont.systemFontOfSize_(20)

    shadow = AppKit.NSShadow.alloc().init()
    shadow.setShadowColor_(AppKit.NSColor.colorWithCalibratedWhite_alpha_(0.0, 0.4))
    shadow.setShadowOffset_(AppKit.NSMakeSize(0, -1))
    shadow.setShadowBlurRadius_(2.0)

    color = AppKit.NSColor.colorWithCalibratedRed_green_blue_alpha_(0.25, 0.35, 0.55, 1.0)
    attrs = AppKit.NSDictionary.dictionaryWithObjectsAndKeys_(
        font, AppKit.NSFontAttributeName,
        color, AppKit.NSForegroundColorAttributeName,
        shadow, AppKit.NSShadowAttributeName,
        None
    )
    text = "I"
    size = AppKit.NSString.stringWithString_(text).sizeWithAttributes_(attrs)
    x = (SIZE - size.width) / 2.0
    y = (SIZE - size.height) / 2.0
    AppKit.NSString.stringWithString_(text).drawAtPoint_withAttributes_(
        AppKit.NSMakePoint(x, y), attrs
    )

def draw_code():
    setup_aqua_text("</>", font_size=12, bold=True)

def draw_bullet():
    # Draw a bullet and two lines
    color = AppKit.NSColor.colorWithCalibratedRed_green_blue_alpha_(0.25, 0.35, 0.55, 1.0)
    color.set()

    # Bullet dot
    dot = AppKit.NSBezierPath.bezierPathWithOvalInRect_(AppKit.NSMakeRect(5, 22, 5, 5))
    dot.fill()

    # Lines
    line1 = AppKit.NSBezierPath.bezierPath()
    line1.moveToPoint_(AppKit.NSMakePoint(13, 24))
    line1.lineToPoint_(AppKit.NSMakePoint(27, 24))
    line1.setLineWidth_(2.0)
    line1.stroke()

    dot2 = AppKit.NSBezierPath.bezierPathWithOvalInRect_(AppKit.NSMakeRect(5, 14, 5, 5))
    dot2.fill()

    line2 = AppKit.NSBezierPath.bezierPath()
    line2.moveToPoint_(AppKit.NSMakePoint(13, 16))
    line2.lineToPoint_(AppKit.NSMakePoint(27, 16))
    line2.setLineWidth_(2.0)
    line2.stroke()

    dot3 = AppKit.NSBezierPath.bezierPathWithOvalInRect_(AppKit.NSMakeRect(5, 6, 5, 5))
    dot3.fill()

    line3 = AppKit.NSBezierPath.bezierPath()
    line3.moveToPoint_(AppKit.NSMakePoint(13, 8))
    line3.lineToPoint_(AppKit.NSMakePoint(27, 8))
    line3.setLineWidth_(2.0)
    line3.stroke()

def draw_numbered():
    color = AppKit.NSColor.colorWithCalibratedRed_green_blue_alpha_(0.25, 0.35, 0.55, 1.0)

    font = AppKit.NSFont.boldSystemFontOfSize_(9)
    attrs = AppKit.NSDictionary.dictionaryWithObjectsAndKeys_(
        font, AppKit.NSFontAttributeName,
        color, AppKit.NSForegroundColorAttributeName,
        None
    )

    for i, row in enumerate(["1.", "2.", "3."]):
        y = 22 - i * 8
        AppKit.NSString.stringWithString_(row).drawAtPoint_withAttributes_(
            AppKit.NSMakePoint(4, y), attrs
        )
        color.set()
        line = AppKit.NSBezierPath.bezierPath()
        line.moveToPoint_(AppKit.NSMakePoint(14, y + 4))
        line.lineToPoint_(AppKit.NSMakePoint(27, y + 4))
        line.setLineWidth_(2.0)
        line.stroke()

def draw_blockquote():
    color = AppKit.NSColor.colorWithCalibratedRed_green_blue_alpha_(0.25, 0.35, 0.55, 1.0)
    color.set()
    # Vertical bar on the left
    bar = AppKit.NSBezierPath.bezierPathWithRect_(AppKit.NSMakeRect(5, 5, 3, 22))
    bar.fill()
    # Three horizontal lines
    for row_y in [22, 14, 8]:
        line = AppKit.NSBezierPath.bezierPath()
        line.moveToPoint_(AppKit.NSMakePoint(12, row_y))
        line.lineToPoint_(AppKit.NSMakePoint(27, row_y))
        line.setLineWidth_(2.0)
        line.stroke()

def draw_hrule():
    color = AppKit.NSColor.colorWithCalibratedRed_green_blue_alpha_(0.25, 0.35, 0.55, 1.0)
    color.set()
    # Three horizontal lines with middle one bolder (the rule)
    for y, w in [(22, 1.5), (16, 3.0), (10, 1.5)]:
        line = AppKit.NSBezierPath.bezierPath()
        line.moveToPoint_(AppKit.NSMakePoint(4, y))
        line.lineToPoint_(AppKit.NSMakePoint(28, y))
        line.setLineWidth_(w)
        line.stroke()

def draw_undo():
    # Left-curling arrow
    color = AppKit.NSColor.colorWithCalibratedRed_green_blue_alpha_(0.25, 0.35, 0.55, 1.0)
    color.set()
    path = AppKit.NSBezierPath.bezierPath()
    path.appendBezierPathWithArcWithCenter_radius_startAngle_endAngle_(
        AppKit.NSMakePoint(16, 14), 10, 0, 210
    )
    path.setLineWidth_(3.0)
    path.lineCapStyle = AppKit.NSRoundLineCapStyle
    path.stroke()
    # Arrowhead at start
    arrow = AppKit.NSBezierPath.bezierPath()
    arrow.moveToPoint_(AppKit.NSMakePoint(6, 22))
    arrow.lineToPoint_(AppKit.NSMakePoint(2, 17))
    arrow.lineToPoint_(AppKit.NSMakePoint(11, 17))
    arrow.closePath()
    arrow.fill()

def draw_redo():
    # Right-curling arrow
    color = AppKit.NSColor.colorWithCalibratedRed_green_blue_alpha_(0.25, 0.35, 0.55, 1.0)
    color.set()
    path = AppKit.NSBezierPath.bezierPath()
    path.appendBezierPathWithArcWithCenter_radius_startAngle_endAngle_clockwise_(
        AppKit.NSMakePoint(16, 14), 10, 210, 0, True
    )
    path.setLineWidth_(3.0)
    path.stroke()
    # Arrowhead at end
    arrow = AppKit.NSBezierPath.bezierPath()
    arrow.moveToPoint_(AppKit.NSMakePoint(26, 22))
    arrow.lineToPoint_(AppKit.NSMakePoint(30, 17))
    arrow.lineToPoint_(AppKit.NSMakePoint(21, 17))
    arrow.closePath()
    arrow.fill()

app = AppKit.NSApplication.sharedApplication()

make_image(draw_bold,       "tb_bold")
make_image(draw_italic,     "tb_italic")
make_image(draw_code,       "tb_code")
make_image(draw_bullet,     "tb_bullet")
make_image(draw_numbered,   "tb_numbered")
make_image(draw_blockquote, "tb_blockquote")
make_image(draw_hrule,      "tb_hrule")
make_image(draw_undo,       "tb_undo")
make_image(draw_redo,       "tb_redo")

print("All icons generated in: " + OUTPUT_DIR)
