#include "pixel.hpp"
#include <cstring>

void PixelBone_Pixel::show(void) {
  std::vector<uint8_t> output_buffer;
  auto bytes_processed = 0;
  for (auto channel_pixel : channel_pixels) {
    auto num_bytes = channel_pixel.second * sizeof(pixel_t);

    // resize the output buffer to match the number of elements
    output_buffer.resize(sizeof(OPCClient::Header) + num_bytes);

    // Setup the OPC message
    OPCClient::Header::view(output_buffer)
        .init(channel_pixel.first, OPCClient::SET_PIXEL_COLORS, num_bytes);

    // Offset starting location
    auto framebuffer_begin = std::begin(framebuffer) + bytes_processed;

    // Move over pixel data from framebuffer to output buffer
    std::move(framebuffer_begin, framebuffer_begin + num_bytes,
              std::begin(output_buffer) + sizeof(OPCClient::Header));

    opc.write(output_buffer);
    bytes_processed += num_bytes;
  }
}

uint32_t PixelBone_Pixel::numPixels() const { return num_pixels; }

pixel_t *const PixelBone_Pixel::getPixel(uint16_t n) const {
  return (pixel_t *)&framebuffer[n * sizeof(pixel_t)];
}

// Convert separate R,G,B into packed 32-bit RGB color.
// Packed format is always RGB, regardless of LED strand color order.
uint32_t PixelBone_Pixel::Color(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

uint32_t PixelBone_Pixel::h2rgb(uint32_t v1, uint32_t v2, uint32_t hue) {
  if (hue < 60) return v1 * 60 + (v2 - v1) * hue;
  if (hue < 180) return v2 * 60;
  if (hue < 240) return v1 * 60 + (v2 - v1) * (240 - hue);

  return v1 * 60;
}

/**
 * Convert HSL (Hue, Saturation, Lightness) to RGB (Red, Green, Blue)
 *
 * hue:        0 to 359 - position on the color wheel, 0=red, 60=orange,
 *                        120=yellow, 180=green, 240=blue, 300=violet
 *
 * saturation: 0 to 100 - how bright or dull the color, 100=full, 0=gray
 *
 * lightness:  0 to 100 - how light the color is, 100=white, 50=color, 0=black
 */

uint32_t PixelBone_Pixel::HSL(uint32_t hue, uint32_t saturation,
                              uint32_t lightness) {
  uint32_t red, green, blue;
  uint32_t var1, var2;

  if (hue > 359) hue = hue % 360;
  if (saturation > 100) saturation = 100;
  if (lightness > 100) lightness = 100;

  // algorithm from: http://www.easyrgb.com/index.php?X=MATH&H=19#text19
  if (saturation == 0) {
    red = green = blue = lightness * 255 / 100;
  } else {
    if (lightness < 50) {
      var2 = lightness * (100 + saturation);
    } else {
      var2 = ((lightness + saturation) * 100) - (saturation * lightness);
    }
    var1 = lightness * 200 - var2;
    red = h2rgb(var1, var2, (hue < 240) ? hue + 120 : hue - 240) * 255 / 600000;
    green = h2rgb(var1, var2, hue) * 255 / 600000;
    blue =
        h2rgb(var1, var2, (hue >= 120) ? hue - 120 : hue + 240) * 255 / 600000;
  }
  return (red << 16) | (green << 8) | blue;
}

// Query color from previously-set pixel (returns packed 32-bit RGB value)
uint32_t PixelBone_Pixel::getPixelColor(uint16_t n) const {
  if (n < num_pixels) {
    pixel_t *const p = getPixel(n);
    return Color(p->r, p->g, p->b);
  }
  return 0;  // Pixel # is out of bounds
}

// Set pixel color from separate R,G,B components:
void PixelBone_Pixel::setPixelColor(uint16_t n, uint8_t r, uint8_t g,
                                    uint8_t b) {
  if (n < num_pixels) {
    // if(brightness) { // See notes in setBrightness()
    //   r = (r * brightness) >> 8;
    //   g = (g * brightness) >> 8;
    //   b = (b * brightness) >> 8;
    // }
    pixel_t *const p = getPixel(n);
    p->r = r;
    p->g = g;
    p->b = b;
  }
}

// Set pixel color from 'packed' 32-bit RGB color:
void PixelBone_Pixel::setPixelColor(uint16_t n, uint32_t c) {
  if (n < num_pixels) {
    uint8_t r = (uint8_t)(c >> 16);
    uint8_t g = (uint8_t)(c >> 8);
    uint8_t b = (uint8_t)c;
    setPixelColor(n, r, g, b);
  }
}

void PixelBone_Pixel::setPixel(uint16_t n, pixel_t p) {
  memcpy(getPixel(n), &p, sizeof(pixel_t));
  // setPixelColor(n, p.r, p.g, p.b);
}

void PixelBone_Pixel::clear() {
  for (uint16_t i = 0; i < num_pixels; i++) {
    setPixelColor(i, 0, 0, 0);
  }
}
