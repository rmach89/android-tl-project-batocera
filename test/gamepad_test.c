#include "luna_boot.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static long long now_ms;
static long long fake_clock(void) { return now_ms; }
struct ev { int id, action; float x, y; } evs[256];
static int nev;
static void emit(void *ctx, int id, int action, float x, float y) {
  (void)ctx; assert(nev < 256); evs[nev++] = (struct ev){id, action, x, y};
}
static struct ev *last(int id) { for (int i = nev - 1; i >= 0; --i) if (evs[i].id == id) return &evs[i]; return 0; }
int main(int argc, char **argv) {
  (void)argc;
  const char *path = argv[1];
  FILE *f = fopen(path, "w");
  fputs("stick 0.2 0.8 0.1\nwalk CTRL 0.04\nlook 0.6 0.4 1.0\n"
        "button SPACE 0.9 0.7\nbutton PAD_A 0.9 0.7\nmacro PAD_RT 0.5,0.5 @50 0.6,0.6\n"
        "button PAD_UP 0.94 0.2\n", f);
  fclose(f);
  luna_input_set_clock(fake_clock);
  luna_keymap m;
  assert(luna_keymap_load(&m, path));
  assert(m.look && m.count == 3 && m.nmacros == 1);
  const float W = 1000, H = 500;

  /* Pad button taps its point. */
  assert(luna_keymap_key(&m, LUNA_PAD_KEY(0), 1, W, H, emit, 0));
  assert(nev == 1 && evs[0].action == 0 && fabsf(evs[0].x - 900) < .01f && fabsf(evs[0].y - 350) < .01f);
  assert(luna_keymap_key(&m, LUNA_PAD_KEY(0), 0, W, H, emit, 0));
  assert(nev == 2 && evs[1].action == 1);

  /* Inside the dead zone nothing moves. */
  nev = 0;
  assert(luna_keymap_axis(&m, 0, .1f, .1f, W, H, emit, 0) && nev == 0);
  /* Full right: press at center, move by radius (0.1 * 500 = 50 px). */
  assert(luna_keymap_axis(&m, 0, 1, 0, W, H, emit, 0));
  assert(nev == 2 && evs[0].id == 1 && evs[0].action == 0 && evs[1].action == 2);
  assert(fabsf(evs[1].x - 250) < .01f && fabsf(evs[1].y - 400) < .01f);
  /* Half travel past the dead zone: 0.6 -> (0.6-0.2)/0.8 = 0.5 -> 25 px. */
  luna_keymap_axis(&m, 0, .6f, 0, W, H, emit, 0);
  assert(fabsf(last(1)->x - 225) < .01f);
  /* Back to center lifts. */
  luna_keymap_axis(&m, 0, 0, 0, W, H, emit, 0);
  assert(last(1)->action == 1);

  /* A bound d-pad button taps; WASD and an unbound d-pad would steer. */
  nev = 0;
  luna_keymap_key(&m, LUNA_PAD_KEY(11), 1, W, H, emit, 0);
  assert(nev == 1 && evs[0].id != 1);
  luna_keymap_key(&m, LUNA_PAD_KEY(11), 0, W, H, emit, 0);
  nev = 0;
  assert(luna_keymap_key(&m, LUNA_PAD_KEY(14), 1, W, H, emit, 0)); /* left, unbound */
  assert(evs[0].id == 1 && fabsf(last(1)->x - 150) < .01f);
  luna_keymap_key(&m, 'A', 1, W, H, emit, 0);   /* keyboard too */
  luna_keymap_key(&m, LUNA_PAD_KEY(14), 0, W, H, emit, 0);
  assert(last(1)->action == 2);                  /* still held by A */
  luna_keymap_key(&m, 'A', 0, W, H, emit, 0);
  assert(last(1)->action == 1);

  /* Right stick drags the look contact and re-centers before running out. */
  nev = 0; now_ms = 1000;
  assert(luna_keymap_axis(&m, 1, 1, 0, W, H, emit, 0));
  luna_input_tick(&m, W, H, emit, 0);
  assert(last(LUNA_LOOK_ID)->action == 0 && fabsf(last(LUNA_LOOK_ID)->x - 600) < .01f);
  now_ms += 16; luna_input_tick(&m, W, H, emit, 0);
  assert(last(LUNA_LOOK_ID)->action == 2 && fabsf(last(LUNA_LOOK_ID)->x - 608) < .01f); /* 500*16/1000 */
  int lifts = 0;
  for (int i = 0; i < 40; ++i) {
    now_ms += 16; luna_input_tick(&m, W, H, emit, 0);
    if (evs[nev - 1].id == LUNA_LOOK_ID && evs[nev - 1].action == 1) ++lifts;
    if (nev > 200) nev = 0;
  }
  assert(lifts >= 1);
  luna_keymap_axis(&m, 1, 0, 0, W, H, emit, 0);
  if (m.look_down) assert(0);

  /* Trigger runs a macro. */
  nev = 0;
  luna_keymap_key(&m, LUNA_PAD_KEY(16), 1, W, H, emit, 0);
  luna_input_tick(&m, W, H, emit, 0);
  assert(nev == 1 && fabsf(evs[0].x - 500) < .01f);

  /* Release clears everything that is down. */
  luna_keymap_axis(&m, 0, 0, 1, W, H, emit, 0);
  luna_keymap_axis(&m, 1, 0, -1, W, H, emit, 0);
  now_ms += 16; luna_input_tick(&m, W, H, emit, 0);
  nev = 0;
  luna_keymap_release(&m, W, H, emit, 0);
  assert(last(1) && last(1)->action == 1 && last(LUNA_LOOK_ID) && last(LUNA_LOOK_ID)->action == 1);
  assert(m.pad_x == 0 && m.pad_y == 0 && !m.look_down);

  /* Bad lines are refused; existing keyboard-only maps still load. */
  const char *bad[] = { "stick 0.2 0.8 0.1\nlook 0.6 0.4 0\n", "stick 0.2 0.8 0.1\nbutton PAD_Q 0.5 0.5\n",
                        "stick 0.2 0.8 0.1\nlook 2 0.4 1\n" };
  for (int i = 0; i < 3; ++i) { f = fopen(path, "w"); fputs(bad[i], f); fclose(f); assert(!luna_keymap_load(&m, path)); }
  assert(luna_keymap_load(&m, "keymaps/genshin.conf"));
  assert(luna_keymap_load(&m, "keymaps/crossworlds.conf"));
  /* With the map off, nothing is consumed (the host forwards Android keys). */
  assert(luna_keymap_load(&m, "off") && !luna_keymap_key(&m, LUNA_PAD_KEY(0), 1, W, H, emit, 0) &&
         !luna_keymap_axis(&m, 0, 1, 0, W, H, emit, 0));
  puts("gamepad keymap tests passed");
  return 0;
}
