#!/usr/bin/env python3
"""
Convert image to JD79661 4-color e-paper format with improved color mapping
Output: C array for embedding in firmware
"""

from PIL import Image, ImageEnhance
import sys
import os
import numpy as np

# Display specs
WIDTH = 152
HEIGHT = 152

# Color palette - adjusted for better natural color representation
PALETTE = {
    'black':  (0, 0, 0),       # Shadows, dark areas
    'white':  (255, 255, 255), # Highlights, bright areas
    'yellow': (220, 220, 150), # Mid-tones, warm colors, greens
    'red':    (180, 100, 100), # Skin tones, browns, darker mid-tones
}

# Color codes (2-bit per pixel)
COLOR_CODES = {
    'black':  0b00,
    'white':  0b01,
    'yellow': 0b10,
    'red':    0b11,
}

def rgb_to_hsv(r, g, b):
    """Convert RGB to HSV"""
    r, g, b = r/255.0, g/255.0, b/255.0
    mx = max(r, g, b)
    mn = min(r, g, b)
    df = mx - mn
    
    if mx == mn:
        h = 0
    elif mx == r:
        h = (60 * ((g-b)/df) + 360) % 360
    elif mx == g:
        h = (60 * ((b-r)/df) + 120) % 360
    elif mx == b:
        h = (60 * ((r-g)/df) + 240) % 360
    
    s = 0 if mx == 0 else df/mx
    v = mx
    
    return h, s, v

def smart_color_match(r, g, b):
    """Smart color matching - optimized for portraits"""
    h, s, v = rgb_to_hsv(r, g, b)
    
    # Skin tone detection (hue 0-60, medium saturation)
    is_skin = (h < 60 and 0.2 < s < 0.6 and v > 0.4)
    
    if is_skin:
        # Skin tones - use red/yellow carefully to avoid over-brightness
        if v < 0.5:
            return 'red'  # Darker skin/shadows
        elif v < 0.65:
            return 'red'  # Mid-tone skin
        elif v < 0.8:
            return 'yellow'  # Lighter skin
        else:
            return 'white'  # Highlights only
    
    # Shadows -> black (expanded)
    if v < 0.45:
        return 'black'
    
    # Highlights -> white
    if v > 0.75:
        if s < 0.5:
            return 'white'
    
    # Low saturation (grayscale)
    if s < 0.3:
        return 'black' if v < 0.6 else 'white'
    
    # Medium saturation
    if s < 0.6:
        if v < 0.55:
            return 'black'
        elif v > 0.75:
            return 'white'
        else:
            return 'red' if v < 0.65 else 'yellow'
    
    # High saturation (colored pixels)
    # Red/Orange/Brown (0-60 degrees)
    if h < 60:
        if v < 0.5:
            return 'black'
        elif v > 0.75:
            return 'yellow'
        else:
            return 'red'
    
    # Yellow/Green (60-180 degrees)
    elif h < 180:
        if v < 0.5:
            return 'black'
        elif v > 0.75:
            return 'yellow'
        else:
            return 'yellow' if v > 0.62 else 'red'
    
    # Cyan/Blue (180-270 degrees)
    elif h < 270:
        if v < 0.55:
            return 'black'
        elif v > 0.75:
            return 'white'
        else:
            return 'red'
    
    # Magenta/Red (270-360 degrees)
    else:
        if v < 0.5:
            return 'black'
        elif v > 0.75:
            return 'yellow'
        else:
            return 'red'

def atkinson_dither(img):
    """Apply Atkinson dithering with reduced error diffusion for smoother result"""
    pixels = np.array(img, dtype=float)
    height, width = pixels.shape[:2]
    
    result = []
    
    for y in range(height):
        row_colors = []
        for x in range(width):
            old_pixel = tuple(pixels[y, x].astype(int))
            
            # Smart color matching
            color_name = smart_color_match(old_pixel[0], old_pixel[1], old_pixel[2])
            new_pixel = np.array(PALETTE[color_name])
            row_colors.append(color_name)
            
            # Calculate quantization error
            quant_error = pixels[y, x] - new_pixel
            
            # Reduced Atkinson dithering (50% error diffusion for smoother result)
            # Original distributes 6/8, we distribute 3/8 (half)
            if x + 1 < width:
                pixels[y, x + 1] += quant_error * 1/16  # was 1/8
            if x + 2 < width:
                pixels[y, x + 2] += quant_error * 1/16  # was 1/8
            
            if y + 1 < height:
                if x > 0:
                    pixels[y + 1, x - 1] += quant_error * 1/16  # was 1/8
                pixels[y + 1, x] += quant_error * 1/16  # was 1/8
                if x + 1 < width:
                    pixels[y + 1, x + 1] += quant_error * 1/16  # was 1/8
            
            if y + 2 < height:
                pixels[y + 2, x] += quant_error * 1/16  # was 1/8
        
        result.append(row_colors)
    
    return result

def convert_image(input_path, output_path):
    """Convert image to e-paper format"""
    
    print(f"Loading image: {input_path}")
    img = Image.open(input_path)
    
    # Resize with high-quality resampling
    img = img.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    img = img.convert('RGB')
    
    # Apply very slight blur to reduce noise (reduced from 0.5)
    from PIL import ImageFilter
    img = img.filter(ImageFilter.GaussianBlur(radius=0.3))
    
    # Enhance for better detail
    enhancer = ImageEnhance.Contrast(img)
    img = enhancer.enhance(1.15)  # Increased for more detail
    
    enhancer = ImageEnhance.Sharpness(img)
    img = enhancer.enhance(1.4)  # Increased for sharper details
    
    print(f"Image resized to {WIDTH}x{HEIGHT}")
    print("Applying portrait-optimized dithering...")
    
    # Apply dithering
    dithered = atkinson_dither(img)
    
    # Convert to e-paper format
    buffer_size = WIDTH * HEIGHT // 4
    buffer = []
    
    for y in range(HEIGHT):
        for x in range(0, WIDTH, 4):
            byte_val = 0
            for i in range(4):
                if x + i < WIDTH:
                    color_name = dithered[y][x + i]
                    color_code = COLOR_CODES[color_name]
                    byte_val |= (color_code << ((3 - i) * 2))
            buffer.append(byte_val)
    
    print(f"Buffer size: {len(buffer)} bytes")
    
    # Count colors
    color_count = {}
    for row in dithered:
        for color in row:
            color_count[color] = color_count.get(color, 0) + 1
    
    total = WIDTH * HEIGHT
    print("Color distribution:")
    for color in ['black', 'red', 'yellow', 'white']:
        count = color_count.get(color, 0)
        percentage = (count / total) * 100
        print(f"  {color}: {percentage:.1f}%")
    
    # Generate C header file
    var_name = os.path.splitext(os.path.basename(output_path))[0]
    
    with open(output_path, 'w') as f:
        f.write(f"/* Auto-generated from {os.path.basename(input_path)} */\n")
        f.write(f"/* Image size: {WIDTH}x{HEIGHT}, 4-color with smart mapping */\n\n")
        f.write(f"#ifndef IMAGE_{var_name.upper()}_H\n")
        f.write(f"#define IMAGE_{var_name.upper()}_H\n\n")
        f.write(f"#include <stdint.h>\n\n")
        f.write(f"#define IMAGE_{var_name.upper()}_SIZE {len(buffer)}\n\n")
        f.write(f"static const uint8_t image_{var_name}[{len(buffer)}] = {{\n")
        
        for i in range(0, len(buffer), 16):
            row = buffer[i:i+16]
            hex_str = ', '.join(f'0x{b:02X}' for b in row)
            f.write(f"    {hex_str},\n")
        
        f.write("};\n\n")
        f.write(f"#endif /* IMAGE_{var_name.upper()}_H */\n")
    
    print(f"Generated: {output_path}")
    
    # Create preview
    preview_img = Image.new('RGB', (WIDTH, HEIGHT))
    for y in range(HEIGHT):
        for x in range(WIDTH):
            color_name = dithered[y][x]
            preview_img.putpixel((x, y), PALETTE[color_name])
    
    preview_path = output_path.replace('.h', '_preview.png')
    preview_img.save(preview_path)
    print(f"Preview saved: {preview_path}")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python convert_image.py <input_image> <output.h>")
        print("Example: python convert_image.py photo.jpg src/image_photo.h")
        sys.exit(1)
    
    input_path = sys.argv[1]
    output_path = sys.argv[2]
    
    if not os.path.exists(input_path):
        print(f"Error: Input file not found: {input_path}")
        sys.exit(1)
    
    convert_image(input_path, output_path)
    print("\nDone! Include this file in your code:")
    print(f'  #include "{os.path.basename(output_path)}"')


