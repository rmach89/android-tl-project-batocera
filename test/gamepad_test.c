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

  /* A joystick MotionEvent carries its axes; touches are unchanged. */
  {
    const float axes[LUNA_PAD_AXES] = { .5f, -.25f, .75f, -1, 1, -1, .3f, .9f, .9f, .3f };
    luna_touch_event j;
    luna_pad_motion(&j, axes, 1234);
    assert(j.source == LUNA_SOURCE_JOYSTICK && j.device_id == LUNA_PAD_DEVICE_ID && j.action == 2);
    assert(luna_event_count(&j) == 1);
    assert(luna_event_axis(&j, 0, 0) == .5f && luna_event_axis(&j, 1, 0) == -.25f);    /* X, Y */
    assert(luna_event_axis(&j, 11, 0) == .75f && luna_event_axis(&j, 14, 0) == -1);    /* Z, RZ */
    assert(luna_event_axis(&j, 15, 0) == 1 && luna_event_axis(&j, 16, 0) == -1);       /* HAT */
    assert(luna_event_axis(&j, 17, 0) == .3f && luna_event_axis(&j, 18, 0) == .9f);    /* triggers */
    assert(luna_event_axis(&j, 22, 0) == .9f && luna_event_axis(&j, 23, 0) == .3f);    /* GAS, BRAKE */
    assert(luna_event_axis(&j, 2, 0) == 0 && luna_event_axis(&j, 0, 1) == 0);          /* no pressure, one pointer */
    assert(luna_event_x(&j, 0) == .5f);
    luna_touch_state ts = {0};
    luna_touch_event t;
    assert(luna_touch_update(&ts, 0, 0, 10, 20, 1, &t) && t.source == 0 && luna_event_axis(&t, 2, 0) == 1);
    float mn, mx, flat;
    luna_pad_axis_range(17, &mn, &mx, &flat, NULL);
    assert(mn == 0 && mx == 1 && flat == 0);
    luna_pad_axis_range(0, &mn, &mx, NULL, NULL);
    assert(mn == -1 && mx == 1);
    for (int i = 0; i < LUNA_PAD_AXES; ++i) assert(luna_pad_axis_slot(luna_pad_axis_at(i)) == i);
    assert(luna_pad_axis_slot(2) < 0 && luna_pad_axis_at(LUNA_PAD_AXES) < 0);
  }
  /* The device model: the controller is listed only while connected. */
  {
    int ids[4];
    assert(luna_input_device_ids(ids, 4) == 0);
    assert(luna_input_device_sources(LUNA_PAD_DEVICE_ID) == 0);
    assert(luna_input_device_sources(LUNA_TOUCH_DEVICE_ID) == LUNA_SOURCE_TOUCHSCREEN);
    assert(luna_input_device_sources(-1) == LUNA_SOURCE_KEYBOARD);
    luna_gamepad_info info = { .connected = 1, .vendor = 0x045e, .product = 0x0b12 };
    strcpy(info.name, "Xbox Wireless Controller");
    luna_gamepad_set(&info);
    assert(luna_input_device_ids(ids, 4) == 1 && ids[0] == LUNA_PAD_DEVICE_ID);
    const int src = luna_input_device_sources(LUNA_PAD_DEVICE_ID);
    assert((src & LUNA_SOURCE_GAMEPAD) == LUNA_SOURCE_GAMEPAD && (src & LUNA_SOURCE_JOYSTICK) == LUNA_SOURCE_JOYSTICK);
    luna_gamepad_info got;
    assert(luna_gamepad_get(&got) && got.vendor == 0x045e && !strcmp(got.name, "Xbox Wireless Controller"));
    luna_gamepad_set(NULL);
    assert(!luna_gamepad_get(&got) && luna_input_device_ids(ids, 4) == 0);
  }
  /* Touchscreen contacts to finger slots. */
  {
    luna_finger_map fm = {0};
    assert(luna_finger_slot(&fm, 77, 2) == -1);            /* never went down */
    assert(luna_finger_slot(&fm, 77, 0) == 0);
    assert(luna_finger_slot(&fm, 78, 0) == 1);
    assert(luna_finger_slot(&fm, 77, 0) == -1);            /* already down */
    assert(luna_finger_slot(&fm, 78, 2) == 1);
    assert(luna_finger_slot(&fm, 77, 1) == 0);             /* lifts and frees */
    assert(luna_finger_slot(&fm, 79, 0) == 0);             /* reused */
    for (long long k = 100; k < 100 + LUNA_HOST_FINGERS - 2; ++k) assert(luna_finger_slot(&fm, k, 0) >= 0);
    assert(luna_finger_slot(&fm, 999, 0) == -1);           /* an eleventh finger */
    /* Contact ids stay inside Android's 0..31 and clear of each other. */
    assert(LUNA_LOOK_ID < LUNA_HOST_TOUCH_ID && LUNA_HOST_TOUCH_ID + LUNA_HOST_FINGERS - 1 <= 26);  /* 26-28: mouse look */
  }
  /* The keymap editor keeps controller bindings and the look line. */
  {
    const char *maps[] = { "keymaps/genshin.conf", "keymaps/crossworlds.conf" };
    for (int k = 0; k < 2; ++k) {
      luna_kedit e;
      luna_keymap before, after;
      assert(luna_keymap_load(&before, maps[k]) && luna_kedit_load(&e, maps[k]));
      for (int i = 0; i < e.n; ++i) assert(luna_key_name(e.b[i].key));
      assert(luna_kedit_save(&e, path));
      luna_kedit_free(&e);
      assert(luna_keymap_load(&after, path));
      assert(after.count == before.count && after.look == before.look);
    }
    char bak[512];
    snprintf(bak, sizeof bak, "%s.bak", path);
    remove(bak);
  }
  puts("gamepad keymap tests passed");
  return 0;
}
