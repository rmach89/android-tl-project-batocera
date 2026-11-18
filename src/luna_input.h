/* SPDX-License-Identifier: MPL-2.0 */
#ifndef LUNA_INPUT_H
#define LUNA_INPUT_H
#ifdef __cplusplus
extern "C" {
#endif
/* Android input sources, device ids and gamepad axes (MotionEvent.AXIS_*). */
#define LUNA_SOURCE_KEYBOARD    0x00000101
#define LUNA_SOURCE_DPAD        0x00000201
#define LUNA_SOURCE_GAMEPAD     0x00000401
#define LUNA_SOURCE_TOUCHSCREEN 0x00001002
#define LUNA_SOURCE_JOYSTICK    0x01000010
#define LUNA_TOUCH_DEVICE_ID 0
#define LUNA_PAD_DEVICE_ID   16
#define LUNA_PAD_AXES 10   /* X Y Z RZ HAT_X HAT_Y LTRIGGER RTRIGGER GAS BRAKE */
#define LUNA_TOUCH_MAX 10
#define LUNA_HOST_TOUCH_ID 20
#define LUNA_HOST_FINGERS 7    /* the first finger, then ids 20-25 */
typedef struct luna_touch_point { int id; float x, y; } luna_touch_point;
typedef struct luna_touch_event {
    int action;
    float x, y; /* pointer zero, also used by legacy single-pointer producers */
    long long event_ms, down_ms;
    int pointer_count;
    luna_touch_point pointers[LUNA_TOUCH_MAX];
    /* 0 for the touchscreen.  A joystick event (source LUNA_SOURCE_JOYSTICK)
     * has one pointer at (AXIS_X, AXIS_Y) and its axes in pad[], indexed by
     * luna_pad_axis_slot(); luna_event_axis() reads them. */
    int source, device_id;
    float pad[LUNA_PAD_AXES];
} luna_touch_event;
typedef struct luna_touch_state {
    int count;
    long long down_ms;
    luna_touch_point points[LUNA_TOUCH_MAX];
} luna_touch_state;
/* action: DOWN=0, UP=1, MOVE=2, CANCEL=3; id identifies a stable contact.
 * Returns a complete immutable Android event, including the lifting pointer
 * on POINTER_UP.  Pointer indices can change; pointer IDs do not. */
/* A touchscreen's own contact identifiers (X11 touch ids, Wayland ids,
 * Windows dwIDs) to slots 0..LUNA_HOST_FINGERS-1.  action 0 claims a slot,
 * 1 releases it, 2 finds it; -1 when there is none (an eleventh finger, or
 * one the map never saw go down). */
typedef struct luna_finger_map {
    long long key[LUNA_HOST_FINGERS];
    unsigned char used[LUNA_HOST_FINGERS];
} luna_finger_map;
static inline int luna_finger_slot(luna_finger_map *m, long long key, int action)
{
  if (!m) return -1;
  for (int i = 0; i < LUNA_HOST_FINGERS; ++i)
    if (m->used[i] && m->key[i] == key) {
      if (action == 1) m->used[i] = 0;
      return action == 0 ? -1 : i;   /* a second "down" for a live finger */
    }
  if (action != 0) return -1;
  for (int i = 0; i < LUNA_HOST_FINGERS; ++i)
    if (!m->used[i]) {
      m->used[i] = 1;
      m->key[i] = key;
      return i;
    }
  return -1;
}
int luna_touch_update(luna_touch_state *, int id, int action, float x, float y,
                      long long now_ms, luna_touch_event *out);
int luna_event_count(const luna_touch_event *);
int luna_event_id(const luna_touch_event *, int index);
float luna_event_x(const luna_touch_event *, int index);
float luna_event_y(const luna_touch_event *, int index);

/* The host's game controller as Android sees it: one InputDevice with id
 * LUNA_PAD_DEVICE_ID while a controller is connected.  The window thread
 * publishes it; the Java runtime and the JNI stubs read it from any thread. */
typedef struct luna_gamepad_info {
    int connected, vendor, product;
    char name[128], descriptor[48];
} luna_gamepad_info;
void luna_gamepad_set(const luna_gamepad_info *);   /* NULL: disconnected */
int luna_gamepad_get(luna_gamepad_info *);          /* 1 while connected */
/* Ids InputDevice.getDeviceIds() lists; returns how many. */
int luna_input_device_ids(int *out, int capacity);
/* SOURCE_* bits of a device, 0 when there is no such device. */
int luna_input_device_sources(int id);
/* The AXIS_* a pad reports, in slot order, and each one's range. */
int luna_pad_axis_slot(int axis);                    /* -1 if not reported */
int luna_pad_axis_at(int slot);
void luna_pad_axis_range(int axis, float *min, float *max, float *flat, float *fuzz);
/* A joystick MotionEvent carrying axes[] (one value per slot). */
void luna_pad_motion(luna_touch_event *out, const float axes[LUNA_PAD_AXES], long long now_ms);
/* What the window thread queues for the Java side. */
typedef struct luna_pad_event {
    int type;              /* 1 key, 2 motion */
    int action, keycode;   /* key: KeyEvent.ACTION_* and KEYCODE_* */
    luna_touch_event motion;
} luna_pad_event;
#include <stddef.h>
#define LUNA_KEYMAP_BUTTONS 20   /* keyboard and gamepad bindings together */
/* Gamepad buttons share the key space above every keyboard code: 0-14 are
 * GLFW_GAMEPAD_BUTTON_* (A B X Y LB RB BACK START GUIDE L3 R3 UP RIGHT DOWN
 * LEFT), 15 and 16 the left and right triggers pulled past halfway.  Names in
 * keymap files: PAD_A ... PAD_LEFT, PAD_LT, PAD_RT. */
#define LUNA_PAD_KEY(button) (1000 + (button))
#define LUNA_PAD_BUTTONS 17
/* Touch contact ids (Android pointer ids, 0-31): 0 the mouse or the first
 * finger on a real touchscreen, 1 the keymap joystick, 2-18 keymap buttons
 * and macros, 19 the right stick's camera drag, 20-25 further fingers on a
 * touchscreen, 26-28 mouse look (F8), 29 the wheel slide, 30-31 the
 * Ctrl+wheel pinch. */
#define LUNA_LOOK_ID 19
#define LUNA_KEY_POINTS 4      /* contacts one key can press at the same time */
#define LUNA_KEYMAP_MACROS 8
#define LUNA_MACRO_STEPS 24
#define LUNA_TIMED_MAX 256
/* button KEY x y [x y ...] [repeat=MS] [hold=MS]
 *   Every point is pressed together (a simultaneous tap).  With repeat= the
 *   points are tapped again every MS milliseconds while the key is held, each
 *   tap lasting hold= milliseconds.  x/y mirror the first point. */
typedef struct luna_key_binding {
    int key, held;
    float x, y;
    int npoints, id, repeat_ms, hold_ms, phase;
    float px[LUNA_KEY_POINTS], py[LUNA_KEY_POINTS];
    long long cycle_ms;
} luna_key_binding;
/* macro KEY step... [loop]
 *   A step is "x,y[|x,y...][/hold_ms]" (taps those points together) or
 *   "@ms" (waits).  The sequence runs once per press; "loop" keeps running it
 *   while the key is held. */
typedef struct luna_macro_step {
    int npoints, hold_ms, wait_ms;
    float x[LUNA_KEY_POINTS], y[LUNA_KEY_POINTS];
} luna_macro_step;
typedef struct luna_macro {
    int key, held, loop, nsteps, id, pending;
    unsigned down;                 /* contacts currently pressed, by offset */
    luna_macro_step steps[LUNA_MACRO_STEPS];
} luna_macro;
typedef struct luna_timed { long long at_ms; int id, action, macro; float x, y; } luna_timed;
typedef struct luna_keymap {
    int enabled, direction[4], stick_down, count, nmacros, nqueue;
    float stick_x, stick_y, radius;
    int walk_key, walk_held;
    float walk_radius;
    /* Left stick: an analog offset of the joystick, -1..1 after the dead zone.
     * Right stick: "look X Y SPEED" drags contact LUNA_LOOK_ID from (X,Y). */
    float pad_x, pad_y;
    int look, look_down;
    float look_x, look_y, look_speed, look_ax, look_ay, look_cx, look_cy;
    long long look_ms;
    luna_key_binding buttons[LUNA_KEYMAP_BUTTONS];
    luna_macro macros[LUNA_KEYMAP_MACROS];
    luna_timed queue[LUNA_TIMED_MAX];
} luna_keymap;
typedef void (*luna_keymap_emit)(void *, int id, int action, float x, float y);
/* Selection can come from the menu thread. The input pump takes a validated
 * replacement and releases old contacts before applying it. */
int luna_keymap_select(const char *path);
void luna_keymap_current(char *out, size_t capacity);
int luna_keymap_take(luna_keymap *out);
/* Profiles are ordinary normalized touch layouts; no guest-specific behavior. */
int luna_keymap_load(luna_keymap *, const char *profile);
int luna_keymap_key(luna_keymap *, int key, int pressed, float width, float height,
                    luna_keymap_emit, void *);
void luna_keymap_release(luna_keymap *, float width, float height, luna_keymap_emit, void *);
/* A gamepad stick, x/y in -1..1 as GLFW reports them (y down).  stick 0 is the
 * left stick and drives the "stick" joystick; 1 is the right stick and drives
 * "look".  Returns 0 when the keymap does not use that stick. */
int luna_keymap_axis(luna_keymap *, int stick, float x, float y, float width, float height,
                     luna_keymap_emit, void *);

/* Editing a keymap file: the stick and plain one-point buttons are modelled,
 * every other line is kept verbatim in its place (see luna_input.c). */
/* One-point buttons the editor places: the contact ids keymaps own (2-18). */
#define LUNA_KEDIT_MAX (LUNA_LOOK_ID - 2)
enum { LUNA_KEDIT_KEEP = -1, LUNA_KEDIT_STICK = -2 };
enum { LUNA_KEDIT_BADKEY = -1, LUNA_KEDIT_TAKEN = -2, LUNA_KEDIT_FULL = -3 };
typedef struct luna_kedit_button { int key, origin; float x, y; } luna_kedit_button;
typedef struct luna_kedit {
    float stick_x, stick_y, radius;
    int has_stick, n, kept_buttons, nreserved, nlines, cap_lines;
    luna_kedit_button b[LUNA_KEDIT_MAX];
    int reserved[32];          /* keys owned by kept lines (walk, macros, ...) */
    char **lines;
    int *tags;                 /* LUNA_KEDIT_KEEP / _STICK / button origin */
} luna_kedit;
/* NULL, "" or "off" starts an empty keymap with a default stick. */
int luna_kedit_load(luna_kedit *, const char *path);
int luna_kedit_find(const luna_kedit *, int key);
/* index -1 adds (or moves the button already on that key); otherwise rebinds
 * button `index`, dropping any other button on the key.  Returns the index or
 * LUNA_KEDIT_BADKEY / _TAKEN / _FULL. */
int luna_kedit_set(luna_kedit *, int index, int key, float x, float y);
void luna_kedit_remove(luna_kedit *, int index);
/* Writes path (first save keeps the old file as path.bak); only a file that
 * luna_keymap_load accepts replaces it. */
int luna_kedit_save(const luna_kedit *, const char *path);
void luna_kedit_free(luna_kedit *);
/* Keymap name of a GLFW key code ("A", "7", "SPACE", ...) or NULL. */
const char *luna_key_name(int code);
int luna_key_normalize(int code);

/* Everything that happens on a clock rather than a key event: repeat taps,
 * macro steps and the replay of a recording.  Call it regularly from the
 * thread that delivers keys (it emits through the same callback). */
void luna_input_tick(luna_keymap *, float width, float height, luna_keymap_emit, void *);
/* Tests substitute the clock (milliseconds, monotonic). */
void luna_input_set_clock(long long (*now_ms)(void));

/* Recording keeps every touch contact, whatever produced it, as text lines
 * "ms id action x y" with x,y as fractions of the view, so a recording replays
 * at any resolution.  Replay goes through the emit callback and is not itself
 * recorded.  luna_input_record_event is safe from any thread. */
int luna_record_start(const char *path);
void luna_record_stop(void);
int luna_record_active(void);
void luna_record_event(int id, int action, float nx, float ny);
int luna_play_start(const char *path, int loop);
void luna_play_stop(void);
int luna_play_active(void);
void luna_record_last_path(char *out, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
