/* =============================================================================
 * Quark-OS kernel/desktop/de_input.c
 * IRQ-safe key event queue for the DE. Keyboard driver pushes scancodes via
 * de_input_irq_sink(); the DE pops decoded events.
 * ============================================================================= */

#include <quark/de.h>
#include <quark/de_apps.h>

#define QUEUE_SIZE 64

static de_key_event_t queue[QUEUE_SIZE];
static volatile int   q_head = 0;
static volatile int   q_tail = 0;

/* Modifier state, updated by the IRQ sink. */
static volatile uint8_t shift_down = 0;
static volatile uint8_t ctrl_down  = 0;
static volatile uint8_t alt_down   = 0;
static volatile uint8_t ext_flag   = 0;   /* set by 0xE0 prefix */

static const char ascii_lower[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,  'a','s',
    'd','f','g','h','j','k','l',';','\'','`', 0, '\\','z','x','c','v',
    'b','n','m',',','.','/', 0,  '*', 0,  ' ', 0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static const char ascii_upper[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,  'A','S',
    'D','F','G','H','J','K','L',':','"', '~', 0,  '|','Z','X','C','V',
    'B','N','M','<','>','?', 0,  '*', 0,  ' ', 0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

void de_input_init(void) {
    q_head = q_tail = 0;
    shift_down = ctrl_down = alt_down = ext_flag = 0;
}

int de_input_pending(void) {
    return q_head != q_tail;
}

int de_input_pop(de_key_event_t* out) {
    if (q_head == q_tail) return 0;
    *out = queue[q_tail];
    q_tail = (q_tail + 1) % QUEUE_SIZE;
    return 1;
}

static void push_event(const de_key_event_t* e) {
    int next = (q_head + 1) % QUEUE_SIZE;
    if (next == q_tail) return;   /* full */
    queue[q_head] = *e;
    q_head = next;
}

void de_input_irq_sink(uint8_t sc) {
    /* Extended prefix */
    if (sc == 0xE0) { ext_flag = 1; return; }

    /* Break codes (key release) */
    if (sc & 0x80) {
        uint8_t mk = sc & 0x7F;
        if (mk == 0x2A || mk == 0x36) shift_down = 0;
        if (mk == 0x1D) ctrl_down = 0;
        if (mk == 0x38) alt_down = 0;
        ext_flag = 0;
        return;
    }

    /* Make codes */
    uint8_t mk = sc & 0x7F;

    if (ext_flag) {
        /* Extended keys are arrows, del, home, end, pgup/pgdn */
        de_key_event_t e;
        kmemset(&e, 0, sizeof(e));
        e.extended = 1;
        e.shift = shift_down;
        e.ctrl  = ctrl_down;
        e.alt   = alt_down;
        switch (mk) {
            case 0x48: e.keycode = DE_KEY_UP;    break;
            case 0x50: e.keycode = DE_KEY_DOWN;  break;
            case 0x4B: e.keycode = DE_KEY_LEFT;  break;
            case 0x4D: e.keycode = DE_KEY_RIGHT; break;
            case 0x47: e.keycode = DE_KEY_HOME;  break;
            case 0x4F: e.keycode = DE_KEY_END;   break;
            case 0x49: e.keycode = DE_KEY_PGUP;  break;
            case 0x51: e.keycode = DE_KEY_PGDN;  break;
            case 0x53: e.keycode = DE_KEY_DEL;   break;
            default:   e.keycode = DE_KEY_NONE;  break;
        }
        ext_flag = 0;
        if (e.keycode != DE_KEY_NONE) push_event(&e);
        return;
    }

    /* Modifiers */
    if (mk == 0x2A || mk == 0x36) { shift_down = 1; return; }
    if (mk == 0x1D)               { ctrl_down  = 1; return; }
    if (mk == 0x38)               { alt_down   = 1; return; }

    de_key_event_t e;
    kmemset(&e, 0, sizeof(e));
    e.scancode = sc;
    e.shift = shift_down;
    e.ctrl  = ctrl_down;
    e.alt   = alt_down;

    /* Special keys that come without an E0 prefix */
    if (mk == 0x1C) { e.keycode = DE_KEY_ENTER; e.ascii = '\n'; }
    else if (mk == 0x0E) { e.keycode = DE_KEY_NONE; e.ascii = '\b'; }
    else if (mk == 0x0F) { e.keycode = DE_KEY_TAB; e.ascii = '\t'; }
    else if (mk == 0x01) { e.keycode = DE_KEY_ESC; }
    else {
        char c = shift_down ? ascii_upper[mk] : ascii_lower[mk];
        e.ascii = c;
    }
    push_event(&e);
}
