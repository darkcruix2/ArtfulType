import sys
from PIL import Image, ImageOps

def tint_image(src, dest, color):
    img = Image.open(src).convert("RGBA")
    
    # Create a solid color image of the same size
    tint = Image.new("RGBA", img.size, color)
    
    # Extract alpha from original image
    r, g, b, alpha = img.split()
    
    # We want to keep the original image's luminosity/shading if possible, 
    # but the user said "with the colors of the dracula theme". 
    # Let's multiply the original RGB with the tint color.
    # We can use ImageOps.colorize on the grayscale version.
    
    gray = ImageOps.grayscale(img)
    # Colorize maps black to black, white to the tint color, midtones are blended.
    # Let's map black to #9580FF and white to #FF80BF.
    colorized = ImageOps.colorize(gray, "#9580FF", "#FF80BF")
    
    # Put original alpha back
    colorized.putalpha(alpha)
    colorized.save(dest)

if __name__ == "__main__":
    tint_image(sys.argv[1], sys.argv[2], "#FF80BF")
