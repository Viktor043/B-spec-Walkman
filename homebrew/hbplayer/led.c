/* led.c: the NW-A91x's notification light (2026-11-17; the A82x has none). Driver icx1101_led, loaded by Sony's
 * /etc/init.d/icx1087driver before our hook runs. The interface comes from Sony's own /usr/local/test/test_led
 * (run in Unicorn, every pattern's ioctl captured):
 *   ioctl 0x40046c02 (LED pattern): { 0, priority 0..4, repeat (-1 = forever), steps, *step } with
 *     step = { ms, brightness 0..100, abortok }. Sony's "hotaru" (firefly) = 103 steps of 30 ms ramping up and down.
 *   ioctl 0x6c03 LED on, 0x6c04 LED off, 0x6c00 reset (cancels patterns), 0xc0046c01 read the state.
 * What it shows: charging = a slow breathing, charged and still plugged = steady, battery at 10 % or less = a short
 * blip every 4 s; the alarm blinks while it rings and the end of a Focus session blinks a few seconds.
 * Settings > Display > Notification light turns it all off. */
#include "hb.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../common/led_a91x.h"

#define LED_DEV "/dev/icx1101_led"

int ui_led = 1;	/* setting: the light on (A91x) */
static int fd = -2, shown = -1, alert, logged;
static double alert_until;

static int dev(void)
{
	if (fd == -2) {
		fd = hw_a91x ? open(LED_DEV, O_RDWR) : -1;
		if (hw_a91x && fd < 0) lg("led: cannot open %s", LED_DEV);
	}
	return fd;
}

static void run(int what)
{
	if (what == shown || dev() < 0) return;
	shown = what;
	int r0, r1, r2 = led_a91x_set(fd, what, &r0, &r1);
	if (logged < 12) { logged++; lg("led: %d (reset %d, off %d, pattern %d)", what, r0, r1, r2); }
}

/* an event pattern on top of the battery one: secs > 0 for that long, 0 until led_alert(LED_NONE) */
void led_alert(int what, double secs)
{
	alert = ui_led ? what : LED_NONE;
	alert_until = secs > 0 ? now() + secs : 0;
	led_tick();
}

/* from idle_tick: the battery pattern unless an event pattern is on */
void led_tick(void)
{
	if (!hw_a91x) return;
	if (alert && alert_until && now() > alert_until) alert = LED_NONE;
	int what = LED_NONE;
	if (!ui_led) what = LED_NONE;
	else if (alert) what = alert;
	else if (ui_st.charging) what = ui_st.batt >= 100 ? LED_FULL : LED_CHARGING;
	else if (batt_power) what = LED_FULL;	/* plugged in, not charging: charged */
	else if (ui_st.batt > 0 && ui_st.batt <= 10) what = LED_LOW;
	run(what);
}

void led_stop(void) { run(LED_NONE); }
