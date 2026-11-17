/* led_a91x.h: the NW-A91x notification light (/dev/icx1101_led), shared by the player (led.c) and the USB helper.
 * The ioctls come from Sony's /usr/local/test/test_led (see LEARNINGS.md):
 *   0x40046c02 pattern { 0, priority 0..4, repeat (-1 = forever), steps, *step }, step = { ms, brightness 0..100, abortok }
 *   0x6c03 on, 0x6c04 off, 0x6c00 reset. */
#ifndef LED_A91X_H
#define LED_A91X_H
#include <sys/ioctl.h>

enum { LED_NONE, LED_CHARGING, LED_FULL, LED_LOW, LED_ALARM, LED_FOCUS };
struct led_step { int ms, bright, abortok; };
struct led_prm { int zero, prio, repeat, n; const struct led_step *step; };

/* show `what` on the opened light; returns the pattern ioctl's result (0 = accepted), r_reset/r_off may be 0 */
static int led_a91x_set(int fd, int what, int *r_reset, int *r_off)
{
	static struct led_step breathe[81], steady[1] = { { 1000, 100, 1 } }, blip[2] = { { 120, 100, 1 }, { 3880, 0, 1 } },
		fast[2] = { { 125, 100, 1 }, { 375, 0, 1 } }, blink[2] = { { 250, 100, 1 }, { 250, 0, 1 } };
	if (!breathe[0].ms) {
		for (int i = 0; i < 40; i++) {	/* 0 -> 100 -> 0 in 3.2 s (squared: even to the eye), then 0.4 s dark */
			breathe[i].ms = breathe[79 - i].ms = 40;
			breathe[i].bright = breathe[79 - i].bright = (i + 1) * (i + 1) * 100 / 1600;
			breathe[i].abortok = breathe[79 - i].abortok = 1;
		}
		breathe[80].ms = 400; breathe[80].abortok = 1;
	}
	int a = ioctl(fd, 0x6c00), b = ioctl(fd, 0x6c04);
	if (r_reset) *r_reset = a;
	if (r_off) *r_off = b;
	struct led_prm p = { 0, 2, -1, 0, 0 };
	switch (what) {
	case LED_CHARGING: p.prio = 1; p.n = 81; p.step = breathe; break;
	case LED_FULL: p.prio = 1; p.n = 1; p.step = steady; break;
	case LED_LOW: p.prio = 1; p.n = 2; p.step = blip; break;
	case LED_ALARM: p.prio = 3; p.n = 2; p.step = fast; break;
	case LED_FOCUS: p.prio = 3; p.n = 2; p.step = blink; p.repeat = 16; break;
	default: return 0;
	}
	return ioctl(fd, 0x40046c02, &p);
}
#endif
