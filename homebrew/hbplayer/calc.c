/* calc.c: Tools > Calculator (2026-11-17; scientific mode and memory 2026-11-18). A pocket calculator on the d-pad:
 * the keys are a grid (basic 4 x 5, scientific 5 x 7), the arrows move the cursor (wrapping), PLAY presses the key,
 * BACK leaves. Music keeps playing (poll_key paces the engine). Drawn by scr_calc (screens.c).
 * OPTION: basic / scientific, Memory (the last 20 results, newest first: pick one to use it), clear, close.
 * Functions (sin, x², n!, ...) act on the number shown at once, like a pocket calculator; x^y and EXP are operators.
 * The mode, DEG / RAD and the memory are kept in STATEDIR/calc.txt. */
#include "hb.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM 20
static double mem[MEM];
static char mem_line[MEM][48];	/* "1200 + 34.5 = 1234.5" */
static int nmem, sci, deg = 1;

static void fmt(double v, char *s, size_t n)
{
	if (v == 0) v = 0;	/* no "-0" */
	if (isnan(v) || isinf(v)) { snprintf(s, n, "Error"); return; }
	snprintf(s, n, "%.10g", v);
	if (strlen(s) > 12) snprintf(s, n, "%.5e", v);
}

static void state_path(char *p, size_t n) { snprintf(p, n, "%s/calc.txt", statedir); }
static void load(void)
{
	static int done;
	if (done) return;
	done = 1;
	char p[PATHLEN + 16], line[96]; state_path(p, sizeof p);
	FILE *f = fopen(p, "r");
	if (!f) return;
	while (fgets(line, sizeof line, f)) {
		line[strcspn(line, "\n")] = 0;
		if (!strncmp(line, "mode ", 5)) { sscanf(line + 5, "%d %d", &sci, &deg); continue; }
		double v; char *tab = strchr(line, '\t');
		if (nmem < MEM && tab && sscanf(line, "%lf", &v) == 1) { mem[nmem] = v; snprintf(mem_line[nmem], sizeof mem_line[0], "%s", tab + 1); nmem++; }
	}
	fclose(f);
}
static void save(void)
{
	char p[PATHLEN + 16]; state_path(p, sizeof p);
	FILE *f = fopen(p, "w");
	if (!f) return;
	fprintf(f, "mode %d %d\n", sci, deg);
	for (int i = 0; i < nmem; i++) fprintf(f, "%.17g\t%s\n", mem[i], mem_line[i]);
	fclose(f);
}
static void remember(double v, const char *expr)	/* newest first, at most MEM */
{
	if (isnan(v) || isinf(v)) return;
	if (nmem && mem[0] == v && !strcmp(mem_line[0], expr)) return;
	if (nmem == MEM) nmem--;
	memmove(mem + 1, mem, nmem * sizeof mem[0]);
	memmove(mem_line + 1, mem_line, nmem * sizeof mem_line[0]);
	mem[0] = v; snprintf(mem_line[0], sizeof mem_line[0], "%s", expr);
	nmem++;
	save();
}

static double apply(double a, const char *op, double b, int *err)
{
	if (!strcmp(op, "+")) return a + b;
	if (!strcmp(op, "-")) return a - b;
	if (!strcmp(op, "×")) return a * b;
	if (!strcmp(op, "÷")) { if (b == 0) { *err = 1; return 0; } return a / b; }
	if (!strcmp(op, "x^y")) return pow(a, b);
	if (!strcmp(op, "EXP")) return a * pow(10, b);
	return b;
}
static const char *op_sign(const char *op) { return !strcmp(op, "x^y") ? "^" : !strcmp(op, "EXP") ? "E" : op; }

/* a function of the number shown; *ok = 0: not a function key */
static double func(const char *k, double v, int *ok, int *err)
{
	double r = v * (deg ? M_PI / 180 : 1);
	*ok = 1;
	if (!strcmp(k, "sin")) return sin(r);
	if (!strcmp(k, "cos")) return cos(r);
	if (!strcmp(k, "tan")) { if (fabs(cos(r)) < 1e-12) { *err = 1; return 0; } return tan(r); }
	if (!strcmp(k, "ln")) { if (v <= 0) { *err = 1; return 0; } return log(v); }
	if (!strcmp(k, "log")) { if (v <= 0) { *err = 1; return 0; } return log10(v); }
	if (!strcmp(k, "x²")) return v * v;
	if (!strcmp(k, "√")) { if (v < 0) { *err = 1; return 0; } return sqrt(v); }
	if (!strcmp(k, "1/x")) { if (v == 0) { *err = 1; return 0; } return 1 / v; }
	if (!strcmp(k, "n!")) { if (v < 0 || v > 170 || v != floor(v)) { *err = 1; return 0; } double f = 1; for (int i = 2; i <= (int)v; i++) f *= i; return f; }
	*ok = 0;
	return v;
}

static int mem_sel;
static const char *mem_label(int i) { return mem_line[i]; }
static const char *opt_label(int i)
{
	static char b[40];
	switch (i) {
	case 0: snprintf(b, sizeof b, "Mode: %s", sci ? "scientific" : "basic"); return b;
	case 1: snprintf(b, sizeof b, "Memory (%d)", nmem); return b;
	case 2: return "Clear";
	case 3: return "Close calculator";
	}
	return "";
}

void calc_run(void)
{
	load();
	char ent[32] = "0", expr[48] = "", op[8] = "";
	double acc = 0, ans = nmem ? mem[0] : 0;
	int fresh = 1, err = 0, sel = sci ? 30 : 16, redraw = 1;	/* the cursor starts on 0 */
	for (;;) {
		int rows = sci ? 7 : 5, cols = sci ? 5 : 4, nk = rows * cols;
		const char *const *keys = sci ? calc_keys_sci : calc_keys_basic;
		if (redraw && backlight_on && !popup_active()) {
			struct calc_view cv = { err ? "Error" : ent, expr, sel, sci, deg };
			scr_calc(&cv); flip(); redraw = 0;
		}
		idle_tick(0);
		int k = poll_key(500);
		if (popup_expired()) redraw = 1;
		if (k == K_REDRAW) { redraw = 1; continue; }
		if (k < 0) continue;
		redraw = 1;
		if (k == K_BACK) return;
		if (k == K_UP) { sel -= cols; if (sel < 0) sel += nk; continue; }
		if (k == K_DOWN) { sel = (sel + cols) % nk; continue; }
		if (k == K_LEFT) { sel = sel % cols ? sel - 1 : sel + cols - 1; continue; }
		if (k == K_RIGHT) { sel = sel % cols < cols - 1 ? sel + 1 : sel - cols + 1; continue; }
		if (k == K_OPTION) {
			struct list_view ov = { sci ? "Scientific" : "Calculator", 4, 0, 0, opt_label, 0, 0, LI_NONE, 0, 0 };
			int s = list_run(&ov);
			if (s == 0) { sci = !sci; sel = sci ? 30 : 16; save(); }
			else if (s == 1) {	/* the memory: pick a result to use it */
				if (!nmem) { eng_note("Memory is empty"); continue; }
				struct list_view mv = { "Memory", nmem, mem_sel, 0, mem_label, 0, 0, LI_NONE, 0, 0 };
				int m = list_run(&mv);
				if (m >= 0) { mem_sel = m; err = 0; fmt(mem[m], ent, sizeof ent); fresh = 1; }
			} else if (s == 2) { err = 0; strcpy(ent, "0"); acc = 0; op[0] = 0; fresh = 1; expr[0] = 0; }
			else if (s == 3) return;
			continue;
		}
		if (k != K_PLAY) { volume_key(k); continue; }
		const char *key = keys[sel];
		if (err) { err = 0; strcpy(ent, "0"); acc = 0; op[0] = 0; fresh = 1; expr[0] = 0; if (!strcmp(key, "C")) continue; }
		double val = strtod(ent, 0);
		int isfn = 0;
		double fr = func(key, val, &isfn, &err);
		if (isfn) {	/* sin, x², n!, ...: on the number shown, at once */
			snprintf(expr, sizeof expr, "%s(%s) =", key, ent);
			fmt(fr, ent, sizeof ent); fresh = 1;
			if (!err) { char line[48]; snprintf(line, sizeof line, "%s %s", expr, ent); remember(fr, line); ans = fr; }
		} else if (key[0] >= '0' && key[0] <= '9') {
			if (fresh) { strcpy(ent, "0"); fresh = 0; }
			size_t digits = 0; for (const char *p = ent; *p; p++) digits += *p >= '0' && *p <= '9';
			if (digits >= 12) continue;
			if (!strcmp(ent, "0")) ent[0] = 0; else if (!strcmp(ent, "-0")) ent[1] = 0;
			strcat(ent, key);
		} else if (!strcmp(key, ".")) {
			if (fresh) { strcpy(ent, "0"); fresh = 0; }
			if (!strchr(ent, '.')) strcat(ent, ".");
		} else if (!strcmp(key, "DEL")) {
			if (fresh) continue;
			size_t l = strlen(ent);
			if (l) ent[--l] = 0;
			if (!l || !strcmp(ent, "-")) strcpy(ent, "0");
		} else if (!strcmp(key, "C")) {
			strcpy(ent, "0"); acc = 0; op[0] = 0; fresh = 1; expr[0] = 0;
		} else if (!strcmp(key, "±")) {
			if (ent[0] == '-') memmove(ent, ent + 1, strlen(ent));
			else if (strcmp(ent, "0") && strlen(ent) < sizeof ent - 1) { memmove(ent + 1, ent, strlen(ent) + 1); ent[0] = '-'; }
		} else if (!strcmp(key, "%")) {	/* 200 + 10 % = 220; 50 x 10 % = 5 */
			fmt(!strcmp(op, "+") || !strcmp(op, "-") ? acc * val / 100 : val / 100, ent, sizeof ent); fresh = 1;
		} else if (!strcmp(key, "π")) { fmt(M_PI, ent, sizeof ent); fresh = 1; }
		else if (!strcmp(key, "e")) { fmt(M_E, ent, sizeof ent); fresh = 1; }
		else if (!strcmp(key, "Ans")) { fmt(ans, ent, sizeof ent); fresh = 1; }
		else if (!strcmp(key, "DEG")) { deg = !deg; save(); }
		else if (!strcmp(key, "=")) {
			if (!op[0]) continue;
			char a[24]; fmt(acc, a, sizeof a);
			snprintf(expr, sizeof expr, "%s %s %s =", a, op_sign(op), ent);
			acc = apply(acc, op, val, &err);
			fmt(acc, ent, sizeof ent); op[0] = 0; fresh = 1;
			if (!err) { char line[48]; snprintf(line, sizeof line, "%s %s", expr, ent); remember(acc, line); ans = acc; }
		} else {	/* + - × ÷ x^y EXP */
			if (op[0] && !fresh) acc = apply(acc, op, val, &err);
			else if (!op[0]) acc = val;
			snprintf(op, sizeof op, "%s", key);
			char a[24]; fmt(acc, a, sizeof a);
			snprintf(expr, sizeof expr, "%s %s", a, op_sign(op));
			fmt(acc, ent, sizeof ent); fresh = 1;
		}
	}
}
