#!/usr/bin/env python3
"""
Convert image to JD79661 4-color e-paper format with Floyd-Steinberg dithering
Output: C array for embedding in firmware
"""

from PIL import Image
import sys
import os
import numpy as np

# Display specs
WIDTH = 152
HEIGHT = 152

# Color palette for 4-color e-paper (RGB values)
PALETTE = {
    'black':  (0, 0, 0),
    'white':  (255, 255, 255),
    'yellow': (255, 255, 0),
    'red':    (255, 0, 0),
}

# Color codes (2-bit per pixel)
COLOR_CODES = {
    'black':  0b00,
    'white':  0b01,
    'yellow': 0b10,
    'red':    0b11,
}

def color_distance(c1, c2):
    """Calculate Euclidean distance between two RGB colors"""
    return sum((a - b) ** 2 for a, b in zip(c1, c2)) ** 0.5

def find_closest_color(rgb):
    """Find closest color in palette"""
    min_dist = float('inf')
    closest = 'white'
    
    for name, palette_rgb in PALETTE.items():
        dist = color_distance(rgb, palette_rgb)
        if dist < min_dist:
            min_dist = dist
            closest = name
    
    return closest, PALETTE[closest]

def floyd_steinberg_dither(img):
    """Apply Floyd-Steinberg dithering to image"""
    # Convert to numpy array for easier manipulation
    pixels = np.array(img, dtype=float)
    height, width = pixels.shape[:2]
    
    result = []
    
    for y in range(height):
        row_colors = []
        for x in range(width):
            old_pixel = tuple(pixels[y, x].astype(int))
            
            # Find closest palette color
            color_name, new_pixel = find_closest_color(old_pixel)
            row_colors.append(color_name)
            
            # Calculate quantization error
            quant_error = pixels[y, x] - np.array(new_pixel)
            
            # Distribute error to neighboring pixels
            if x + 1 < width:
                pixels[y, x + 1] += quant_error * 7/16
            if y + 1 < height:
                if x > 0:
                    pixels[y + 1, x - 1] += quant_error * 3/16
                pixels[y + 1, x] += quant_error * 5/16
                if x + 1 < width:
                    pixels[y + 1, x + 1] += quant_error * 1/16
        
        result.append(row_colors)
    
    return result

def convert_image(input_path, output_path):
    """Convert image to e-paper format"""
    
    # Load and resize image
    print(f"Loading image: {input_path}")
    img = Image.open(input_path)
    
    # Resize to display size
    img = img.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    img = img.convert('RGB')
    
    print(f"Image resized to {WIDTH}x{HEIGHT}")
    print("Applying Floyd-Steinberg dithering...")
    
    # Apply dithering
    dithered = floyd_steinberg_dither(img)
    
    # Convert to e-paper format
    # Each byte contains 4 pixels (2 bits each)
    buffer_size = WIDTH * HEIGHT // 4
    buffer = []
    
    for y in range(HEIGHT):
        for x in range(0, WIDTH, 4):  # Process 4 pixels at a time
            byte_val = 0
            for i in range(4):
                if x + i < WIDTH:
                    color_name = dithered[y][x + i]
                    color_code = COLOR_CODES[color_name]
                    # Pack 4 pixels into 1 byte (MSB first)
                    byte_val |= (color_code << ((3 - i) * 2))
            buffer.append(byte_val)
    
    print(f"Buffer size: {len(buffer)} bytes")
    
    # Generate C header file
    var_name = os.path.splitext(os.path.basename(output_path))[0]
    
    with open(output_path, 'w') as f:
        f.write(f"/* Auto-generated from {os.path.basename(input_path)} */\n")
        f.write(f"/* Image size: {WIDTH}x{HEIGHT}, 4-color with dithering */\n\n")
        f.write(f"#ifndef IMAGE_{var_name.upper()}_H\n")
        f.write(f"#define IMAGE_{var_name.upper()}_H\n\n")
        f.write(f"#include <stdint.h>\n\n")
        f.write(f"#define IMAGE_{var_name.upper()}_SIZE {len(buffer)}\n\n")
        f.write(f"static const uint8_t image_{var_name}[{len(buffer)}] = {{\n")
        
        # Write data in rows of 16 bytes
        for i in range(0, len(buffer), 16):
            row = buffer[i:i+16]
            hex_str = ', '.join(f'0x{b:02X}' for b in row)
            f.write(f"    {hex_str},\n")
        
        f.write("};\n\n")
        f.write(f"#endif /* IMAGE_{var_name.upper()}_H */\n")
    
    print(f"Generated: {output_path}")
    
    # Create preview image
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

