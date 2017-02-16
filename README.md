![Testing PixelBone](https://lh3.googleusercontent.com/-de4gV0F2_Gk/U1vb6bDet1I/AAAAAAAACJg/mGFfGTMWo4c/w1084-h813-no/IMG_20140426_121532.jpg)

# Overview

This is a modified version of the LEDscape library designed to control chains of WS2812 and WS2812b chips via OPC.

# Installation and Usage

To use PixelBone, download it to your BeagleBone Black.

First, make sure that PixelBone compiles:

```sh
make
```


Once everything is connected, run the `rgb-test` program:

```sh
./examples/rgb-test
```

The LEDs should now be fading prettily. If not, go back and make
sure everything is setup correctly.


API
===

`pixel.hpp` and `matrix.hpp` defines the API. The key components are:

```cpp
class PixelBone_Pixel {
public:
  PixelBone_Pixel(uint16_t pixel_count);
  void show(void);
  void clear(void);
  void setPixelColor(uint32_t n, uint8_t r, uint8_t g, uint8_t b);
  void setPixelColor(uint32_t n, uint32_t c);
  void moveToNextBuffer();
  uint32_t wait();
  uint32_t numPixels() const;
  uint32_t getPixelColor(uint32_t n) const;
  static uint32_t Color(uint8_t red, uint8_t green, uint8_t blue);
  static uint32_t HSB(uint16_t hue, uint8_t saturation, uint8_t brightness);
};

class PixelBone_Matrix{
public:
  // Constructor for single matrix:
  PixelBone_Matrix(int w, int h,
                   uint8_t matrixType = MATRIX_TOP + MATRIX_LEFT + MATRIX_ROWS);

  // Constructor for tiled matrices:
  PixelBone_Matrix(uint8_t matrixW, uint8_t matrixH, uint8_t tX, uint8_t tY,
                   uint8_t matrixType = MATRIX_TOP + MATRIX_LEFT + MATRIX_ROWS +
                                        TILE_TOP + TILE_LEFT + TILE_ROWS);

  void drawPixel(int16_t x, int16_t y, uint16_t color);
  void fillScreen(uint16_t color);
  static uint16_t Color(uint8_t r, uint8_t g, uint8_t b);
};
```

You can double buffer like this:

```cpp
const int num_pixels = 256;
PixelBone_Pixel strip(num_pixels);

while (true) {
	render(strip); //modify the pixels here

	// wait for the previous frame to finish;
	strip.wait();
	strip.show()

	// Alternate frame buffers on each draw command
	strip.moveToNextBuffer();
}
```

The 24-bit RGB data to be displayed is laid out with BRGA format,
since that is how it will be translated during the clock out from the PRU.

```cpp
struct PixelBone_pixel_t{
	uint8_t b;
	uint8_t r;
	uint8_t g;
	uint8_t a;
} __attribute__((__packed__));
```

#Low level API

If you want to poke at the PRU directly, there is a command structure
shared in PRU DRAM that holds a pointer to the current frame buffer,
the length in pixels, a command byte and a response byte.
Once the PRU has cleared the command byte you are free to re-write the
dma address or number of pixels.

```cpp
struct ws281x_command_t {
	// in the DDR shared with the PRU
	const uintptr_t pixels_dma;

	// Length in pixels of the longest LED strip.
	unsigned num_pixels;

	// write 1 to start, 0xFF to abort. will be cleared when started
	volatile unsigned command;

	// will have a non-zero response written when done
	volatile unsigned response;
} __attribute__((__packed__));
```

Reference
==========
* http://www.adafruit.com/products/1138
* http://www.adafruit.com/datasheets/WS2811.pdf
