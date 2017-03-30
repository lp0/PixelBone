/** \file
 * UDP image packet receiver.
 *
 * Based on the HackRockCity LED Display code:
 * https://github.com/agwn/pyramidTransmitter/blob/master/LEDDisplay.pde
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <cinttypes>
#include <cerrno>
#include "../pixel.hpp"

int main(int argc, char **argv) {
	int port = 2812;
	uint16_t num_pixels = 1;

	const int sock = socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (sock < 0)
		die("socket failed: %s\n", strerror(errno));

	if (bind(sock, (const struct sockaddr *)&addr, sizeof(addr)) < 0)
		die("bind port %d failed: %s\n", port, strerror(errno));

	PixelBone_Pixel strip {num_pixels};
	uint8_t rgb[3];

	strip.setPixelColor(0, 0, 0, 0);
	strip.wait();
	strip.show();
	strip.moveToNextBuffer();

	while (1) {
		char buf[6] = { 0 };
		const ssize_t rc = recv(sock, buf, sizeof(buf), 0);
		if (rc < 0) {
			printf("recv failed: %s\n", strerror(errno));
			continue;
		}
		if (rc != sizeof(buf)) {
			continue;
		}

		rgb[0] = ((buf[0] < 'A' ? (buf[0] - '0') : (buf[0] - 'A' + 10)) << 4) | (buf[1] < 'A' ? (buf[1] - '0') : (buf[1] - 'A' + 10));
		rgb[1] = ((buf[2] < 'A' ? (buf[2] - '0') : (buf[2] - 'A' + 10)) << 4) | (buf[3] < 'A' ? (buf[3] - '0') : (buf[3] - 'A' + 10));
		rgb[2] = ((buf[4] < 'A' ? (buf[4] - '0') : (buf[4] - 'A' + 10)) << 4) | (buf[5] < 'A' ? (buf[5] - '0') : (buf[5] - 'A' + 10));

		printf("#%02X%02X%02X", rgb[0], rgb[1], rgb[2]);
		rgb[0] = rgb[0] * 127 / 255;
		rgb[1] = rgb[1] * 95 / 255;
		rgb[2] = rgb[2] * 80 / 255;
		printf(" -> #%02X%02X%02X\n", rgb[0], rgb[1], rgb[2]);

		strip.setPixelColor(0, rgb[0], rgb[1], rgb[2]);
		strip.wait();
		strip.show();
		strip.moveToNextBuffer();
	}

	return 0;
}
