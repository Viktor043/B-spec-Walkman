/* vis.c: the now-playing spectrum analyzer (2026-11-18), Winamp style. The engine hands over the PCM it writes
 * (vis_push16 / vis_push32: the last 256 frames, mono); vis_frame turns them into VIS_BANDS bars with a 256-point
 * integer FFT (Hann window, log-spaced bands, a dB-like scale), fast attack, slow fall and peak caps that hold and
 * drop. Sony's DSP plays some files by itself (Max. battery) and then no PCM passes through the player: the bars
 * follow a gentle generated motion instead, so the screen never freezes. */
#include "hb.h"
#include "screens.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define N 256
static short ring[N];
static double pushed_at;

void vis_push16(const short *s, int frames)
{
	if (frames <= 0) return;
	int n = frames < N ? frames : N, from = frames - n;
	if (n < N) memmove(ring, ring + n, (N - n) * sizeof ring[0]);
	for (int i = 0; i < n; i++) ring[N - n + i] = (short)((s[2 * (from + i)] + s[2 * (from + i) + 1]) >> 1);
	pushed_at = now();
}
void vis_push32(const int *s, int frames)
{
	if (frames <= 0) return;
	int n = frames < N ? frames : N, from = frames - n;
	if (n < N) memmove(ring, ring + n, (N - n) * sizeof ring[0]);
	for (int i = 0; i < n; i++) ring[N - n + i] = (short)(((s[2 * (from + i)] >> 17) + (s[2 * (from + i) + 1] >> 17)));
	pushed_at = now();
}

static short cosq[N / 2], sinq[N / 2], hann[N];
static unsigned char band_lo[VIS_BANDS + 1];
static void init(void)
{
	static int done;
	if (done) return;
	done = 1;
	for (int i = 0; i < N / 2; i++) { cosq[i] = (short)lrint(32767 * cos(2 * M_PI * i / N)); sinq[i] = (short)lrint(-32767 * sin(2 * M_PI * i / N)); }
	for (int i = 0; i < N; i++) hann[i] = (short)lrint(32767 * (0.5 - 0.5 * cos(2 * M_PI * i / (N - 1))));
	for (int b = 0; b <= VIS_BANDS; b++) {	/* bins 1..127 (172 Hz .. 22 kHz), log spaced, at least one bin each */
		int lo = (int)lrint(1.6 * pow(127 / 1.6, (double)b / VIS_BANDS));
		if (b && lo <= band_lo[b - 1]) lo = band_lo[b - 1] + 1;
		band_lo[b] = (unsigned char)(lo > 127 ? 127 : lo);
	}
}

/* radix-2 in place, Q15 twiddles, a >>1 per stage (no overflow) */
static void fft(int *re, int *im)
{
	for (int i = 1, j = 0; i < N; i++) {
		int bit = N >> 1;
		for (; j & bit; bit >>= 1) j ^= bit;
		j ^= bit;
		if (i < j) { int t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
	}
	for (int len = 2; len <= N; len <<= 1) {
		int step = N / len;
		for (int i = 0; i < N; i += len) for (int k = 0; k < len / 2; k++) {
			int wr = cosq[k * step], wi = sinq[k * step], a = i + k, b = a + len / 2;
			int tr = (re[b] * wr - im[b] * wi) >> 15, ti = (re[b] * wi + im[b] * wr) >> 15;
			re[b] = (re[a] - tr) >> 1; im[b] = (im[a] - ti) >> 1;
			re[a] = (re[a] + tr) >> 1; im[a] = (im[a] + ti) >> 1;
		}
	}
}

static int lvl(unsigned m)	/* ~ 8 x log2(m): 6 dB = 8 steps */
{
	if (!m) return 0;
	int e = 31 - __builtin_clz(m);
	int frac = e >= 3 ? (int)((m >> (e - 3)) & 7) : (int)((m << (3 - e)) & 7);
	return e * 8 + frac;
}

static unsigned char bars[VIS_BANDS], peaks[VIS_BANDS], hold[VIS_BANDS];
static int fake[VIS_BANDS];

void vis_frame(const unsigned char **h, const unsigned char **pk, int playing)
{
	init();
	int target[VIS_BANDS];
	int live = playing && now() - pushed_at < 0.4;
	if (live) {
		static int re[N], im[N];
		for (int i = 0; i < N; i++) { re[i] = (ring[i] * hann[i]) >> 15; im[i] = 0; }
		fft(re, im);
		for (int b = 0; b < VIS_BANDS; b++) {
			unsigned m = 0;
			for (int k = band_lo[b]; k < band_lo[b + 1] || k == band_lo[b]; k++) {
				unsigned a = (unsigned)abs(re[k]), c = (unsigned)abs(im[k]);
				unsigned mag = a > c ? a + c * 3 / 8 : c + a * 3 / 8;
				if (mag > m) m = mag;
			}
			int v = (lvl(m) - 16) * 255 / 72;	/* about 54 dB of range above the noise floor */
			v += b * 3;	/* a slight tilt: music has less energy up high */
			target[b] = v < 0 ? 0 : v > 255 ? 255 : v;
		}
	} else if (playing) {	/* no PCM (the DSP plays by itself): a slow generated motion with a soft 2 Hz pulse */
		static unsigned seed = 12345;
		double t = now();
		int pulse = (int)(60 * (0.5 + 0.5 * sin(t * 2 * M_PI * 2)));
		for (int b = 0; b < VIS_BANDS; b++) {
			seed = seed * 1103515245 + 12345;
			fake[b] += (int)((seed >> 16) % 41) - 20;
			int base = 150 - b * 4;
			if (fake[b] < -60) fake[b] = -60;
			if (fake[b] > 60) fake[b] = 60;
			int v = base + fake[b] + (b < 6 ? pulse : pulse / 3);
			target[b] = v < 0 ? 0 : v > 255 ? 255 : v;
		}
	} else memset(target, 0, sizeof target);
	for (int b = 0; b < VIS_BANDS; b++) {
		int v = bars[b];
		v = target[b] > v ? target[b] : v - 14 < target[b] ? target[b] : v - 14;	/* up at once, down 14 a frame */
		bars[b] = (unsigned char)(v < 0 ? 0 : v);
		if (bars[b] >= peaks[b]) { peaks[b] = bars[b]; hold[b] = 8; }
		else if (hold[b]) hold[b]--;
		else peaks[b] = peaks[b] > 6 ? peaks[b] - 6 : 0;
	}
	*h = bars; *pk = peaks;
}
int vis_active(void)	/* anything still moving (bars or caps up) */
{
	for (int b = 0; b < VIS_BANDS; b++) if (bars[b] || peaks[b]) return 1;
	return 0;
}
