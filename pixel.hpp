/** \file
 * LED Library for the BeagleBone (Black).
 *
 * Drives up to 32 ws281x LED strips using the PRU to have no CPU overhead.
 * Allows easy double buffering of frames.
 */

#ifndef _pixelbone_hpp_
#define _pixelbone_hpp_

#include <cstdint>
#include <numeric>
#include "opc_client.hpp"

/** LEDscape pixel format is BRGA.
 *
 * data is laid out with BRGA format, since that is how it will
 * be translated during the clock out from the PRU.
 */
struct pixel_t {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  pixel_t(uint8_t _r, uint8_t _g, uint8_t _b) : b(_b), r(_r), g(_g){};
} __attribute__((__packed__));

class PixelBone_Pixel {
  std::vector<std::pair<uint8_t, uint16_t>> channel_pixels;
  OPCClient opc;
  std::vector<uint8_t> framebuffer;
  uint16_t num_pixels;
  uint8_t brightness;

 public:
  PixelBone_Pixel(uint8_t channel, uint16_t pixel_count)
      : num_pixels(pixel_count) {
    channel_pixels = {std::pair<uint8_t, uint16_t>(channel, pixel_count)};
    framebuffer.resize(pixel_count * sizeof(pixel_t));
  };

  PixelBone_Pixel(std::vector<std::pair<uint8_t, uint16_t>> channel_pixels)
      : channel_pixels(channel_pixels) {
    num_pixels = std::accumulate(
        std::begin(channel_pixels), std::end(channel_pixels), 0,
        [](uint16_t num_pixels, std::pair<uint8_t, uint16_t> channel_pixel) {
          return num_pixels + channel_pixel.second;
        });

    framebuffer.resize(num_pixels * sizeof(pixel_t));
  }
  ~PixelBone_Pixel(){};

  inline bool setServer(const char* hostport) { return opc.resolve(hostport); }

  void show(void);
  void clear(void);
  void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b);
  void setPixelColor(uint16_t n, uint32_t c);
  void setPixel(uint16_t n, pixel_t c);
  uint32_t numPixels() const;
  pixel_t* const getPixel(uint16_t n) const;
  uint32_t getPixelColor(uint16_t n) const;
  static uint32_t Color(uint8_t red, uint8_t green, uint8_t blue);
  static uint32_t HSL(uint32_t hue, uint32_t saturation, uint32_t brightness);

 private:
  static uint32_t h2rgb(uint32_t v1, uint32_t v2, uint32_t hue);
};

#endif
