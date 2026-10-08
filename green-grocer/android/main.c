#include "canvas.h"
#include "../fixtures/catalog.h"

#include <android/configuration.h>
#include <android/input.h>
#include <android_native_app_glue.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

/* Semantic state belongs to the process, never to a window or activity. */
static gg_cart session_cart;
static pthread_mutex_t cart_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct { float x, y, width, height; } rect;
typedef enum { NOTHING, FILTER, ADD, DECREMENT, REMOVE } action;
typedef struct { rect bounds; action kind; gg_product_id identity; } button;
typedef struct {
    struct android_app *app;
    bool redraw, selected_only, touching, dragging, scroll_gesture;
    float density, scroll, max_scroll, down_x, down_y, last_y;
    int32_t pointer;
    rect viewport, body;
    button buttons[1 + 3 * GG_MAX_PRODUCTS];
    size_t button_count;
    button pressed;
    const char *status;
} screen;

static const uint32_t background = 0xFFF4F5EEu;
static const uint32_t ink = 0xFF182D1Cu;
static const uint32_t green = 0xFFD5E8C8u;
static const uint32_t white = 0xFFFFFFFFu;

static bool contains(rect bounds, float x, float y) {
    return x >= bounds.x && x < bounds.x + bounds.width &&
           y >= bounds.y && y < bounds.y + bounds.height;
}

static button hit(const screen *state, float x, float y) {
    for (size_t index = 0; index < state->button_count; ++index) {
        button candidate = state->buttons[index];
        if (candidate.kind != FILTER && !contains(state->body, x, y)) continue;
        if (contains(candidate.bounds, x, y)) return candidate;
    }
    return (button){0};
}

static void money(char *text, size_t capacity, gg_money cents) {
    (void)snprintf(text, capacity, "$%" PRIu64 ".%02" PRIu64,
                   cents / 100, cents % 100);
}

static void text(screen *state, gg_canvas *canvas, const char *value,
                 float x, float y, float size) {
    gg_canvas_text(canvas, value, x, y, size * state->density, ink);
}

/* This bundled fixture is ASCII. Wrap at measured words, without truncating
 * names or shrinking them into unreadable single lines. Not a text framework. */
static float wrapped(screen *state, gg_canvas *canvas, const char *value,
                     float x, float y, float width, float size) {
    const char *cursor = value;
    float line_height = (size + 6) * state->density;
    while (*cursor != '\0') {
        char line[160];
        size_t length = 0, word_end = 0;
        while (cursor[length] != '\0' && length + 1 < sizeof(line)) {
            line[length] = cursor[length];
            line[++length] = '\0';
            if (gg_canvas_measure(canvas, line, size * state->density) > width) {
                --length;
                if (word_end > 0) length = word_end;
                break;
            }
            if (cursor[length - 1] == ' ') word_end = length - 1;
        }
        if (length == 0) length = 1;
        memcpy(line, cursor, length);
        line[length] = '\0';
        text(state, canvas, line, x, y, size);
        y += line_height;
        cursor += length;
        while (*cursor == ' ') ++cursor;
    }
    return y;
}

static void draw_button(screen *state, gg_canvas *canvas, rect bounds,
                        const char *label, action kind, gg_product_id identity) {
    gg_canvas_rect(canvas, bounds.x, bounds.y, bounds.width, bounds.height, green);
    float width = gg_canvas_measure(canvas, label, 16 * state->density);
    text(state, canvas, label, bounds.x + (bounds.width - width) / 2,
         bounds.y + bounds.height / 2 + 6 * state->density, 16);
    state->buttons[state->button_count++] = (button){bounds, kind, identity};
}

static void viewport(screen *state) {
    float width = (float)ANativeWindow_getWidth(state->app->window);
    float height = (float)ANativeWindow_getHeight(state->app->window);
    ARect content = state->app->contentRect;
    state->viewport = (rect){0, 0, width, height};
    if (content.right > content.left && content.bottom > content.top &&
        content.left >= 0 && content.top >= 0 &&
        (float)content.right <= width && (float)content.bottom <= height) {
        state->viewport = (rect){(float)content.left, (float)content.top,
            (float)(content.right - content.left), (float)(content.bottom - content.top)};
    }
    int density = AConfiguration_getDensity(state->app->config);
    state->density = density > 0 && density < 1000 ? (float)density / 160 : 1;
}

static void render(screen *state) {
    viewport(state);
    gg_cart cart;
    (void)pthread_mutex_lock(&cart_mutex);
    cart = session_cart;
    (void)pthread_mutex_unlock(&cart_mutex);
    gg_cart_row rows[GG_MAX_PRODUCTS];
    size_t count = 0;
    gg_money total = 0;
    if (gg_rows(&gg_sample_catalog, &cart, state->selected_only, rows,
                GG_MAX_PRODUCTS, &count) != GG_OK ||
        gg_total(&gg_sample_catalog, &cart, &total) != GG_OK) {
        state->status = "Cart is invalid";
        return;
    }
    gg_canvas canvas;
    if (!gg_canvas_begin(&canvas, state->app->activity, state->app->window)) return;
    float d = state->density;
    rect view = state->viewport;
    float header_height = 128 * d, footer_height = 96 * d;
    state->body = (rect){view.x, view.y + header_height, view.width,
        view.height - header_height - footer_height};
    if (state->body.height < 0) state->body.height = 0;
    state->button_count = 0;
    gg_canvas_clear(&canvas, background);
    float y = state->body.y + 16 * d - state->scroll;
    float x = view.x + 16 * d;
    float row_width = view.width - 32 * d;
    for (size_t index = 0; index < count; ++index) {
        gg_cart_row row = rows[index];
        float top = y;
        /* Local placeholder; image_key is NULL in this fixture. */
        gg_canvas_rect(&canvas, x, y, 28 * d, 28 * d, green);
        text(state, &canvas, "GG", x + 3 * d, y + 20 * d, 12);
        y = wrapped(state, &canvas, row.product->name, x + 40 * d,
                    y + 18 * d, row_width - 40 * d, 18);
        char price[32];
        money(price, sizeof(price), row.product->price_cents);
        char description[160];
        (void)snprintf(description, sizeof(description), "%s / %s",
                       row.product->unit, price);
        y = wrapped(state, &canvas, description, x, y + 8 * d, row_width, 14);
        char amount[80];
        (void)snprintf(amount, sizeof(amount), "Amount: %" PRIu32, row.amount);
        text(state, &canvas, amount, x, y + 12 * d, 16);
        y += 28 * d;
        draw_button(state, &canvas, (rect){x, y, 48*d, 48*d}, "-", DECREMENT, row.product->identity);
        draw_button(state, &canvas, (rect){x+56*d, y, 48*d, 48*d}, "+", ADD, row.product->identity);
        draw_button(state, &canvas, (rect){x+112*d, y, 80*d, 48*d}, "Remove", REMOVE, row.product->identity);
        y += 64 * d;
        gg_canvas_rect(&canvas, x, y - 2*d, row_width, d, green);
        if (y <= top) break;
    }
    if (count == 0) text(state, &canvas, "No selected products", x, y + 20 * d, 18);
    float content_height = y + state->scroll - state->body.y;
    state->max_scroll = content_height > state->body.height ? content_height - state->body.height : 0;
    if (state->scroll > state->max_scroll) {
        state->scroll = state->max_scroll;
        state->redraw = true; /* Redraw once using the clamped projection. */
    }
    /* Paint fixed areas last; row hit testing also clips to the body. */
    gg_canvas_rect(&canvas, view.x, view.y, view.width, header_height, background);
    text(state, &canvas, "Green Grocer", x, view.y + 30*d, 24);
    text(state, &canvas, "Sample catalog / session-only cart", x, view.y + 54*d, 14);
    draw_button(state, &canvas, (rect){x, view.y+68*d, row_width, 48*d},
                state->selected_only ? "Show all products" : "Selected only", FILTER, 0);
    float footer = view.y + view.height - footer_height;
    gg_canvas_rect(&canvas, view.x, footer, view.width, footer_height, white);
    char selected[80];
    (void)snprintf(selected, sizeof(selected), "%zu selected products", cart.count);
    text(state, &canvas, selected, x, footer + 24*d, 16);
    char total_text[32], label[80];
    money(total_text, sizeof(total_text), total);
    (void)snprintf(label, sizeof(label), "Sample total: %s", total_text);
    text(state, &canvas, label, x, footer + 52*d, 20);
    if (state->status != NULL) text(state, &canvas, state->status, x, footer + 78*d, 12);
    gg_canvas_end(&canvas);
}

static void apply(screen *state, button pressed) {
    if (pressed.kind == NOTHING) return;
    if (pressed.kind == FILTER) {
        state->selected_only = !state->selected_only;
        state->scroll = 0;
    } else {
        gg_result result = GG_OK;
        (void)pthread_mutex_lock(&cart_mutex);
        if (pressed.kind == ADD) result = gg_add(&gg_sample_catalog, &session_cart, pressed.identity);
        if (pressed.kind == REMOVE) result = gg_remove(&gg_sample_catalog, &session_cart, pressed.identity);
        if (pressed.kind == DECREMENT) {
            uint32_t amount = 0;
            result = gg_amount(&gg_sample_catalog, &session_cart, pressed.identity, &amount);
            if (result == GG_OK) result = gg_set_amount(&gg_sample_catalog, &session_cart,
                                                       pressed.identity, amount > 0 ? amount - 1 : 0);
        }
        (void)pthread_mutex_unlock(&cart_mutex);
        state->status = result == GG_OK ? NULL : "Cart change rejected: amount or total limit";
    }
    state->redraw = true;
}

static int32_t input(struct android_app *app, AInputEvent *event) {
    screen *state = app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;
    int32_t kind = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    if (kind == AMOTION_EVENT_ACTION_CANCEL || AMotionEvent_getPointerCount(event) != 1) {
        state->touching = false;
        return 1;
    }
    float x = AMotionEvent_getX(event, 0), y = AMotionEvent_getY(event, 0);
    int32_t pointer = AMotionEvent_getPointerId(event, 0);
    if (kind == AMOTION_EVENT_ACTION_DOWN) {
        state->touching = true;
        state->dragging = false;
        state->down_x = x;
        state->down_y = state->last_y = y;
        state->pointer = pointer;
        state->pressed = hit(state, x, y);
        state->scroll_gesture = contains(state->body, x, y);
    } else if (state->touching && state->pointer == pointer) {
        float dx = x - state->down_x, dy = y - state->down_y;
        float slop = 8 * state->density;
        if (dx*dx + dy*dy > slop*slop) state->dragging = true;
        if (kind == AMOTION_EVENT_ACTION_MOVE && state->dragging && state->scroll_gesture) {
            state->scroll += state->last_y - y;
            if (state->scroll < 0) state->scroll = 0;
            if (state->scroll > state->max_scroll) state->scroll = state->max_scroll;
            state->redraw = true;
        }
        state->last_y = y;
        if (kind == AMOTION_EVENT_ACTION_UP) {
            button released = hit(state, x, y);
            if (!state->dragging && released.kind == state->pressed.kind &&
                released.identity == state->pressed.identity) apply(state, released);
            state->touching = false;
        }
    }
    return 1;
}

static void command(struct android_app *app, int32_t kind) {
    screen *state = app->userData;
    if (kind == APP_CMD_TERM_WINDOW || kind == APP_CMD_LOST_FOCUS || kind == APP_CMD_PAUSE ||
        kind == APP_CMD_WINDOW_RESIZED || kind == APP_CMD_CONTENT_RECT_CHANGED ||
        kind == APP_CMD_CONFIG_CHANGED)
        state->touching = false;
    if (kind == APP_CMD_INIT_WINDOW || kind == APP_CMD_WINDOW_RESIZED ||
        kind == APP_CMD_CONTENT_RECT_CHANGED || kind == APP_CMD_CONFIG_CHANGED ||
        kind == APP_CMD_GAINED_FOCUS || kind == APP_CMD_RESUME) state->redraw = true;
}

void android_main(struct android_app *app) {
    screen state = {.app = app, .redraw = true, .density = 1};
    app->userData = &state;
    app->onAppCmd = command;
    app->onInputEvent = input;
    while (app->destroyRequested == 0) {
        int events;
        struct android_poll_source *source = NULL;
        int identifier = ALooper_pollOnce(state.redraw && app->window != NULL ? 0 : -1,
                                         NULL, &events, (void **)&source);
        if (identifier >= 0 && source != NULL) source->process(app, source);
        if (app->destroyRequested != 0) break;
        if (state.redraw && app->window != NULL) {
            state.redraw = false;
            render(&state);
        }
    }
    app->userData = NULL;
}
