import 'dart:math';
import 'dart:typed_data';
import 'package:image/image.dart' as img;

const int epdWidth = 152;
const int epdHeight = 152;
const int bufferSize = epdWidth * epdHeight ~/ 4; // 5776 bytes

const int colorCodeBlack = 0x00;
const int colorCodeWhite = 0x01;
const int colorCodeYellow = 0x02;
const int colorCodeRed = 0x03;

class _Rgb {
  final int r, g, b;
  const _Rgb(this.r, this.g, this.b);
}

final Map<int, _Rgb> _palette = {
  colorCodeBlack: _Rgb(0, 0, 0),
  colorCodeWhite: _Rgb(255, 255, 255),
  colorCodeYellow: _Rgb(220, 220, 150),
  colorCodeRed: _Rgb(180, 100, 100),
};

class _Hsv {
  final double h, s, v;
  const _Hsv(this.h, this.s, this.v);
}

_Hsv _rgbToHsv(double r, double g, double b) {
  r /= 255.0;
  g /= 255.0;
  b /= 255.0;
  final mx = max(r, max(g, b));
  final mn = min(r, min(g, b));
  final df = mx - mn;

  double h;
  if (df == 0) {
    h = 0;
  } else if (mx == r) {
    h = (60 * ((g - b) / df) + 360) % 360;
  } else if (mx == g) {
    h = (60 * ((b - r) / df) + 120) % 360;
  } else {
    h = (60 * ((r - g) / df) + 240) % 360;
  }

  final s = mx == 0 ? 0.0 : df / mx;
  return _Hsv(h, s, mx);
}

int _smartColorMatch(double r, double g, double b) {
  final hsv = _rgbToHsv(r, g, b);
  final h = hsv.h, s = hsv.s, v = hsv.v;

  final isSkin = (h < 60 && s > 0.2 && s < 0.6 && v > 0.4);

  if (isSkin) {
    if (v < 0.5) return colorCodeRed;
    if (v < 0.65) return colorCodeRed;
    if (v < 0.8) return colorCodeYellow;
    return colorCodeWhite;
  }

  if (v < 0.45) return colorCodeBlack;

  if (v > 0.75 && s < 0.5) return colorCodeWhite;

  if (s < 0.3) {
    return v < 0.6 ? colorCodeBlack : colorCodeWhite;
  }

  if (s < 0.6) {
    if (v < 0.55) return colorCodeBlack;
    if (v > 0.75) return colorCodeWhite;
    return v < 0.65 ? colorCodeRed : colorCodeYellow;
  }

  if (h < 60) {
    if (v < 0.5) return colorCodeBlack;
    if (v > 0.75) return colorCodeYellow;
    return colorCodeRed;
  } else if (h < 180) {
    if (v < 0.5) return colorCodeBlack;
    if (v > 0.75) return colorCodeYellow;
    return v > 0.62 ? colorCodeYellow : colorCodeRed;
  } else if (h < 270) {
    if (v < 0.55) return colorCodeBlack;
    if (v > 0.75) return colorCodeWhite;
    return colorCodeRed;
  } else {
    if (v < 0.5) return colorCodeBlack;
    if (v > 0.75) return colorCodeYellow;
    return colorCodeRed;
  }
}

/// Result of converting an image for the e-paper display.
class ConversionResult {
  /// Raw 2bpp packed bytes ready to send to firmware (5776 bytes).
  final Uint8List buffer;

  /// RGBA preview image for display in the app.
  final img.Image preview;

  const ConversionResult({required this.buffer, required this.preview});
}

/// Convert any image bytes to the 4-color e-paper format.
/// This ports the Python convert_image.py algorithm to Dart.
ConversionResult convertImageForEpd(Uint8List imageBytes) {
  var source = img.decodeImage(imageBytes);
  if (source == null) {
    throw Exception('Failed to decode image');
  }

  source = img.copyResize(source,
      width: epdWidth, height: epdHeight, interpolation: img.Interpolation.linear);

  source = img.adjustColor(source, contrast: 1.15);
  source = img.gaussianBlur(source, radius: 1);

  // Work with float pixel data for dithering
  final pixels = List.generate(
    epdHeight,
    (y) => List.generate(epdWidth, (x) {
      final p = source!.getPixel(x, y);
      return [p.r.toDouble(), p.g.toDouble(), p.b.toDouble()];
    }),
  );

  // Atkinson dithering with reduced error diffusion
  final colorMap = List.generate(epdHeight, (_) => List.filled(epdWidth, 0));

  for (int y = 0; y < epdHeight; y++) {
    for (int x = 0; x < epdWidth; x++) {
      final oldR = pixels[y][x][0].clamp(0.0, 255.0);
      final oldG = pixels[y][x][1].clamp(0.0, 255.0);
      final oldB = pixels[y][x][2].clamp(0.0, 255.0);

      final colorCode = _smartColorMatch(oldR, oldG, oldB);
      colorMap[y][x] = colorCode;

      final pal = _palette[colorCode]!;
      final errR = oldR - pal.r;
      final errG = oldG - pal.g;
      final errB = oldB - pal.b;

      void diffuse(int dy, int dx, double factor) {
        final ny = y + dy, nx = x + dx;
        if (ny >= 0 && ny < epdHeight && nx >= 0 && nx < epdWidth) {
          pixels[ny][nx][0] += errR * factor;
          pixels[ny][nx][1] += errG * factor;
          pixels[ny][nx][2] += errB * factor;
        }
      }

      // Reduced Atkinson: 1/16 instead of 1/8
      diffuse(0, 1, 1 / 16);
      diffuse(0, 2, 1 / 16);
      diffuse(1, -1, 1 / 16);
      diffuse(1, 0, 1 / 16);
      diffuse(1, 1, 1 / 16);
      diffuse(2, 0, 1 / 16);
    }
  }

  // Pack into 2bpp buffer (4 pixels per byte)
  final buffer = Uint8List(bufferSize);
  int idx = 0;
  for (int y = 0; y < epdHeight; y++) {
    for (int x = 0; x < epdWidth; x += 4) {
      int byteVal = 0;
      for (int i = 0; i < 4; i++) {
        if (x + i < epdWidth) {
          byteVal |= (colorMap[y][x + i] << ((3 - i) * 2));
        }
      }
      buffer[idx++] = byteVal;
    }
  }

  // Build RGBA preview
  final preview = img.Image(width: epdWidth, height: epdHeight);
  for (int y = 0; y < epdHeight; y++) {
    for (int x = 0; x < epdWidth; x++) {
      final pal = _palette[colorMap[y][x]]!;
      preview.setPixelRgba(x, y, pal.r, pal.g, pal.b, 255);
    }
  }

  return ConversionResult(buffer: buffer, preview: preview);
}
