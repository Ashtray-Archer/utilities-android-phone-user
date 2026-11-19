#include "canvas.h"
#include "recipe_document.h"
#include "recipe_engine.h"

#include <android/configuration.h>
#include <android/input.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct { float x, y, width, height; } rect;
typedef struct { rect bounds; size_t row, control; bool present; } button;
typedef struct {
    struct android_app *app;
    gg_recipe_engine engine;
    gg_recipe_view view;
    bool available, redraw, touching, dragging, scroll_gesture;
    float density, scroll, max_scroll, down_x, down_y, last_y;
    int32_t pointer;
    size_t generation, pressed_generation;
    rect viewport, body;
    button buttons[GG_RECIPE_VIEW_ROWS * GG_RECIPE_ROW_CONTROLS];
    size_t button_count;
    button pressed;
    const char *platform_notice;
} screen;

static const uint32_t background = 0xFF202521u, ink = 0xFFEAF0E9u;
static const uint32_t green = 0xFF39563Du, footer_color = 0xFF161B17u;

static bool contains(rect bounds, float x, float y) {
    return x >= bounds.x && x < bounds.x + bounds.width &&
           y >= bounds.y && y < bounds.y + bounds.height;
}

static button hit(const screen *state, float x, float y) {
    if (!contains(state->body, x, y)) return (button){0};
    for (size_t index = 0; index < state->button_count; ++index)
        if (contains(state->buttons[index].bounds, x, y)) return state->buttons[index];
    return (button){0};
}

static bool same_text(const gg_text *left, const gg_text *right) {
    return left->length == right->length && (left->length == 0 ||
        memcmp(left->characters, right->characters, left->length * sizeof(uint16_t)) == 0);
}

/* Layout consumes explicit UTF-16 lengths. It never turns a NUL into EOF or
 * divides a surrogate pair when wrapping a long word. */
static float wrapped(screen *state, gg_canvas *canvas, const gg_text *value,
                     float x, float baseline, float width, float size) {
    size_t start = 0;
    float line = (size + 6) * state->density;
    while (start < value->length) {
        size_t end = start, word = start;
        while (end < value->length && value->characters[end] != '\n') {
            size_t next = end + 1;
            if (value->characters[end] >= 0xD800 && value->characters[end] <= 0xDBFF &&
                next < value->length) ++next;
            if (end > start && gg_canvas_measure16(canvas, value->characters + start,
                    next - start, size * state->density) > width) {
                if (word > start) end = word;
                break;
            }
            if (value->characters[end] == ' ') word = end;
            end = next;
        }
        if (end > start) gg_canvas_text16(canvas, value->characters + start, end - start,
            x, baseline, size * state->density, ink);
        baseline += line;
        start = end;
        if (start < value->length && value->characters[start] == '\n') ++start;
        while (start < value->length && value->characters[start] == ' ') ++start;
    }
    return baseline;
}

static void viewport(screen *state) {
    float width = (float)ANativeWindow_getWidth(state->app->window);
    float height = (float)ANativeWindow_getHeight(state->app->window);
    ARect content = state->app->contentRect;
    state->viewport = (rect){0, 0, width, height};
    if (content.right > content.left && content.bottom > content.top &&
        content.left >= 0 && content.top >= 0 &&
        (float)content.right <= width && (float)content.bottom <= height)
        state->viewport = (rect){(float)content.left, (float)content.top,
            (float)(content.right - content.left), (float)(content.bottom - content.top)};
    int density = AConfiguration_getDensity(state->app->config);
    state->density = density > 0 && density < 1000 ? (float)density / 160 : 1;
}

static void render(screen *state) {
    viewport(state);
    gg_canvas canvas;
    if (!gg_canvas_begin(&canvas, state->app->activity, state->app->window)) return;
    float d = state->density;
    rect view = state->viewport;
    float header_height = 92 * d, footer_height = 112 * d;
    state->body = (rect){view.x, view.y + header_height, view.width,
        view.height - header_height - footer_height};
    if (state->body.height < 0) state->body.height = 0;
    state->button_count = 0;
    gg_canvas_clear(&canvas, background);
    float x = view.x + 16 * d, width = view.width - 32 * d;
    float y = state->body.y + 20 * d - state->scroll;
    for (size_t row = 0; row < state->view.row_count; ++row) {
        const gg_recipe_row *current = &state->view.rows[row];
        if (current->text.length != 0)
            y = wrapped(state, &canvas, &current->text, x, y, width, 18) + 8 * d;
        if (current->control_count != 0) {
            float button_width = (width - 8 * d * (float)(current->control_count - 1)) /
                (float)current->control_count;
            for (size_t control = 0; control < current->control_count; ++control) {
                rect bounds = {x + (float)control * (button_width + 8 * d), y, button_width, 52 * d};
                gg_canvas_rect(&canvas, bounds.x, bounds.y, bounds.width, bounds.height, green);
                const gg_text *label = &current->controls[control].label;
                float label_width = gg_canvas_measure16(&canvas, label->characters, label->length, 16 * d);
                gg_canvas_text16(&canvas, label->characters, label->length,
                    bounds.x + (bounds.width - label_width) / 2, bounds.y + 32 * d, 16 * d, ink);
                state->buttons[state->button_count++] = (button){bounds, row, control, true};
#ifdef GG_RECIPE_OBSERVE
                __android_log_print(ANDROID_LOG_INFO, "GGRecipeBounds", "row=%zu control=%zu x=%d y=%d",
                    row, control, (int)(bounds.x + bounds.width / 2), (int)(bounds.y + bounds.height / 2));
#endif
            }
            y += 68 * d;
        }
        y += 12 * d;
    }
    float content_height = y + state->scroll - state->body.y;
    state->max_scroll = content_height > state->body.height ? content_height - state->body.height : 0;
    if (state->scroll > state->max_scroll) { state->scroll = state->max_scroll; state->redraw = true; }
    /* Fixed areas cover scrolled rows; hit testing clips those rows as well. */
    gg_canvas_rect(&canvas, view.x, view.y, view.width, header_height, background);
    gg_canvas_text(&canvas, "Green Grocer", x, view.y + 30*d, 22*d, ink);
    if (state->available) (void)wrapped(state, &canvas, &state->view.title, x, view.y + 62*d, width, 20);
    else gg_canvas_text(&canvas, "Recipe runtime unavailable", x, view.y + 62*d, 18*d, ink);
    float footer = view.y + view.height - footer_height;
    gg_canvas_rect(&canvas, view.x, footer, view.width, footer_height, footer_color);
    if (state->platform_notice != NULL)
        gg_canvas_text(&canvas, state->platform_notice, x, footer + 26*d, 14*d, ink);
    else (void)wrapped(state, &canvas, &state->view.notice, x, footer + 26*d, width, 14);
    gg_canvas_end(&canvas);
}

static void apply(screen *state, button pressed) {
    if (!state->available || !pressed.present || pressed.row >= state->view.row_count ||
        pressed.control >= state->view.rows[pressed.row].control_count) return;
    const gg_text *token = &state->view.rows[pressed.row].controls[pressed.control].token;
    if (gg_text_is_ascii(token, "platform:import-recipe-file")) {
        state->platform_notice = gg_recipe_document_request(state->app->activity, state->engine.env)
            ? "Choose a recipe file" : "A file import is already pending";
    } else {
        gg_recipe_view candidate = {0};
        if (gg_recipe_engine_action(&state->engine, token, &candidate)) {
            if (!same_text(&state->view.title, &candidate.title)) state->scroll = 0;
            gg_recipe_view_dispose(&state->view);
            state->view = candidate;
            ++state->generation;
            state->platform_notice = NULL;
        } else state->platform_notice = "Recipe action failed; previous screen retained";
    }
    state->redraw = true;
}

static int32_t input(struct android_app *app, AInputEvent *event) {
    screen *state = app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY && AKeyEvent_getKeyCode(event) == AKEYCODE_BACK) {
        if (AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_UP) {
            gg_recipe_view candidate = {0};
            int result = state->available ? gg_recipe_engine_back(&state->engine, &candidate) : 0;
            if (result == 0) ANativeActivity_finish(app->activity);
            else if (result > 0) {
                gg_recipe_view_dispose(&state->view);
                state->view = candidate;
                state->scroll = 0;
                state->platform_notice = NULL;
                ++state->generation;
                state->redraw = true;
            } else state->platform_notice = "Back navigation failed; previous screen retained";
        }
        return 1;
    }
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
        state->pressed_generation = state->generation;
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
            if (!state->dragging && state->pressed_generation == state->generation &&
                released.present && state->pressed.present && released.row == state->pressed.row &&
                released.control == state->pressed.control) apply(state, released);
            state->touching = false;
        }
    }
    return 1;
}

static void command(struct android_app *app, int32_t kind) {
    screen *state = app->userData;
    if (kind == APP_CMD_TERM_WINDOW || kind == APP_CMD_LOST_FOCUS || kind == APP_CMD_PAUSE ||
        kind == APP_CMD_WINDOW_RESIZED || kind == APP_CMD_CONTENT_RECT_CHANGED || kind == APP_CMD_CONFIG_CHANGED)
        state->touching = false;
    if (kind == APP_CMD_INIT_WINDOW || kind == APP_CMD_WINDOW_RESIZED ||
        kind == APP_CMD_CONTENT_RECT_CHANGED || kind == APP_CMD_CONFIG_CHANGED ||
        kind == APP_CMD_GAINED_FOCUS || kind == APP_CMD_RESUME) state->redraw = true;
}

static void completed_document(screen *state) {
    char *path = NULL;
    gg_document_result result = gg_recipe_document_take(&path);
    if (result == GG_DOCUMENT_NONE) return;
    if (result == GG_DOCUMENT_READY && path != NULL && state->available) {
        gg_recipe_view candidate = {0};
        if (gg_recipe_engine_import(&state->engine, path, &candidate)) {
            gg_recipe_view_dispose(&state->view);
            state->view = candidate;
            state->scroll = 0;
            ++state->generation;
            state->platform_notice = NULL;
        } else state->platform_notice = "Recipe import failed; previous screen retained";
    } else state->platform_notice = result == GG_DOCUMENT_CANCELLED ? "Import cancelled" : "Recipe file could not be opened";
    if (path != NULL) { (void)unlink(path); free(path); }
    state->touching = false;
    state->redraw = true;
}

void android_main(struct android_app *app) {
    screen *state = calloc(1, sizeof(*state));
    if (state == NULL) { ANativeActivity_finish(app->activity); return; }
    state->app = app;
    state->redraw = true;
    state->density = 1;
    state->available = gg_recipe_engine_open(&state->engine, app->activity, &state->view);
    app->userData = state;
    app->onAppCmd = command;
    app->onInputEvent = input;
    gg_recipe_document_listen(app->looper);
    while (app->destroyRequested == 0) {
        completed_document(state);
        int events;
        struct android_poll_source *source = NULL;
        int identifier = ALooper_pollOnce(state->redraw && app->window != NULL ? 0 : -1,
            NULL, &events, (void **)&source);
        if (identifier >= 0 && source != NULL) source->process(app, source);
        if (app->destroyRequested != 0) break;
        completed_document(state);
        if (state->redraw && app->window != NULL) { state->redraw = false; render(state); }
    }
    gg_recipe_document_unlisten(app->looper);
    gg_recipe_view_dispose(&state->view);
    gg_recipe_engine_close(&state->engine);
    app->userData = NULL;
    free(state);
}
