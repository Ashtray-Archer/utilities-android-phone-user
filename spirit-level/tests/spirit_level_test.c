#include "spirit_level_renderer.h"

#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <string.h>

static struct spirit_level_geometry reading(float x, float y, float z, int rotation)
{
    struct accelerometer_model model;
    accelerometer_model_initialize(&model);
    enum compact_acceleration_encode_status status =
        accelerometer_model_accept(&model, (struct physical_acceleration){x, y, z});
    return spirit_level_geometry_make(&model, status, rotation);
}

static void near(float actual, float expected, float tolerance)
{
    if (!(fabsf(actual - expected) <= tolerance)) {
        fprintf(stderr, "value %.9g expected %.9g +/- %.9g\n",
            (double)actual, (double)expected, (double)tolerance);
        assert(false);
    }
}

static void check_segment(
    const struct spirit_level_geometry *geometry, int width, int height)
{
    struct spirit_level_segment segment = spirit_level_segment_make(width, height, geometry);
    assert(segment.first_x >= 4 && segment.first_x <= width - 5);
    assert(segment.last_x >= 4 && segment.last_x <= width - 5);
    assert(segment.first_y >= 4 && segment.first_y <= height - SPIRIT_LEVEL_FOOTER_HEIGHT - 5);
    assert(segment.last_y >= 4 && segment.last_y <= height - SPIRIT_LEVEL_FOOTER_HEIGHT - 5);
    assert(segment.first_x != segment.last_x || segment.first_y != segment.last_y);
    float dx = (float)(segment.last_x - segment.first_x);
    float dy = (float)(segment.last_y - segment.first_y);
    /* Pixel rounding may move either endpoint by half a pixel per coordinate. */
    assert(fabsf(dx * geometry->line_down - dy * geometry->line_right) <= 1.5F);
    assert(segment.first_x == 4 || segment.first_x == width - 5 ||
        segment.first_y == 4 || segment.first_y == height - SPIRIT_LEVEL_FOOTER_HEIGHT - 5);
}

static void test_cardinals(void)
{
    struct spirit_level_geometry up = reading(0, 0, 9.81F, 0);
    struct spirit_level_geometry down = reading(0, 0, -9.81F, 0);
    assert(up.status == SPIRIT_LEVEL_FLAT && up.face_up);
    assert(down.status == SPIRIT_LEVEL_FLAT && !down.face_up);
    near(up.line_right, 1, 0);
    near(up.line_down, 0, 0);
    near(up.inclination_radians, 0, 0.02F);
    near(down.inclination_radians, acosf(-1), 0.02F);

    struct spirit_level_geometry portrait = reading(0, 9.81F, 0, 0);
    near(portrait.line_right, 1, 0.01F);
    near(portrait.line_down, 0, 0.01F);
    assert(portrait.marker_down < -0.99F);
    struct spirit_level_geometry landscape = reading(9.81F, 0, 0, 0);
    near(landscape.line_right, 0, 0.01F);
    near(landscape.line_down, 1, 0.01F);
    assert(landscape.marker_right > 0.99F);
    struct spirit_level_geometry inverted = reading(0, -9.81F, 0, 0);
    assert(inverted.line_right < -0.99F && inverted.marker_down > 0.99F);
    struct spirit_level_geometry opposite = reading(-9.81F, 0, 0, 0);
    assert(opposite.line_down < -0.99F && opposite.marker_right < -0.99F);

    /* Independent sign convention checks, including natural-landscape devices. */
    const float expected[4][2] = {{2, 3}, {3, -2}, {-2, -3}, {-3, 2}};
    for (int rotation = 0; rotation < 4; ++rotation) {
        float right = 0;
        float above = 0;
        assert(spirit_level_screen_axes((struct physical_acceleration){2, 3, 4},
            rotation, &right, &above));
        near(right, expected[rotation][0], 0);
        near(above, expected[rotation][1], 0);
    }
    struct spirit_level_geometry quarter_turn = reading(9.81F, 0, 0, 1);
    assert(quarter_turn.line_right < -0.99F && quarter_turn.marker_down > 0.99F);

    /* Holding x/y fixed while tilting out of the screen must move the marker. */
    struct spirit_level_geometry shallow = reading(1, 2, 9, 0);
    struct spirit_level_geometry steep = reading(1, 2, 1, 0);
    assert(hypotf(shallow.marker_right, shallow.marker_down) <
        hypotf(steep.marker_right, steep.marker_down));
    puts("PASS cardinals, display rotations, face-down and out-of-plane tilt");
}

static void test_unavailable(void)
{
    struct accelerometer_model model;
    accelerometer_model_initialize(&model);
    struct spirit_level_geometry view = spirit_level_geometry_make(
        &model, COMPACT_ACCELERATION_ENCODE_OK, 0);
    assert(view.status == SPIRIT_LEVEL_WAITING);
    view = reading(0, 0, 0, 0);
    assert(view.status == SPIRIT_LEVEL_NO_GRAVITY);
    view = reading(1, 2, 3, -1);
    assert(view.status == SPIRIT_LEVEL_NO_ROTATION);
    view = reading(1, 2, 3, 4);
    assert(view.status == SPIRIT_LEVEL_NO_ROTATION);
    view = reading(FLT_MAX, 0, 0, 0);
    assert(view.status == SPIRIT_LEVEL_SATURATED);
    enum compact_acceleration_encode_status status =
        accelerometer_model_accept(&model, (struct physical_acceleration){1, 2, 9});
    assert(status == COMPACT_ACCELERATION_ENCODE_OK);
    struct compact_acceleration before = model.retained;
    const float nonfinite[] = {NAN, INFINITY, -INFINITY};
    for (size_t index = 0; index < sizeof(nonfinite) / sizeof(nonfinite[0]); ++index) {
        for (int axis = 0; axis < 3; ++axis) {
            float values[3] = {1, 2, 9};
            values[axis] = nonfinite[index];
            status = accelerometer_model_accept(&model,
                (struct physical_acceleration){values[0], values[1], values[2]});
            assert(status == COMPACT_ACCELERATION_ENCODE_NONFINITE);
            assert(memcmp(&before, &model.retained, sizeof(before)) == 0);
            view = spirit_level_geometry_make(&model, status, 0);
            assert(view.status == SPIRIT_LEVEL_INVALID); /* Not the stale valid tilt. */
        }
    }
    model.retained = (struct compact_acceleration){1, 0, 0, 0};
    view = spirit_level_geometry_make(&model, COMPACT_ACCELERATION_ENCODE_OK, 0);
    assert(view.status == SPIRIT_LEVEL_INVALID);
    model.retained = compact_acceleration_canonical_zero();
    view = spirit_level_geometry_make(&model, COMPACT_ACCELERATION_ENCODE_OK, 0);
    /* Zero RESIDUAL is balanced gravity, not absence of physical acceleration. */
    assert(view.status == SPIRIT_LEVEL_TRACKING && !view.face_up);
    model.have_android_sample = false;
    view = spirit_level_geometry_make(&model, COMPACT_ACCELERATION_ENCODE_OK, 0);
    assert(view.status == SPIRIT_LEVEL_WAITING);
    puts("PASS waiting, near-zero, nonfinite, saturation and malformed retained state");
}

static void test_sphere(void)
{
    unsigned int count = 0;
    float largest_inclination_error = 0;
    const float pi = acosf(-1.0F);
    for (int inclination = 0; inclination <= 36; ++inclination) {
        float theta = pi * (float)inclination / 36.0F;
        for (int azimuth = 0; azimuth < 72; ++azimuth) {
            float phi = 2.0F * pi * (float)azimuth / 72.0F;
            struct physical_acceleration raw = {
                9.81F * sinf(theta) * cosf(phi),
                9.81F * sinf(theta) * sinf(phi),
                9.81F * cosf(theta)};
            struct accelerometer_model model;
            accelerometer_model_initialize(&model);
            enum compact_acceleration_encode_status status = accelerometer_model_accept(&model, raw);
            assert(status == COMPACT_ACCELERATION_ENCODE_OK);
            struct physical_acceleration decoded;
            assert(accelerometer_model_reconstruct(&model, &decoded));
            for (int rotation = 0; rotation < 4; ++rotation) {
                struct spirit_level_geometry view = spirit_level_geometry_make(&model, status, rotation);
                assert(spirit_level_has_tilt(view.status));
                near(hypotf(view.line_right, view.line_down), 1, 0.000002F);
                assert(hypotf(view.marker_right, view.marker_down) <= 1.000001F);
                float inclination_error = fabsf(view.inclination_radians - theta);
                largest_inclination_error = fmaxf(largest_inclination_error, inclination_error);
                assert(inclination_error < 0.02F);
                if (view.status == SPIRIT_LEVEL_TRACKING) {
                    float right;
                    float above;
                    assert(spirit_level_screen_axes(decoded, rotation, &right, &above));
                    near(view.line_right * right - view.line_down * above, 0, 0.000003F);
                    assert(spirit_level_screen_axes(raw, rotation, &right, &above));
                    /* Independent raw oracle bound, not a second production path. */
                    assert(fabsf(view.line_right * right - view.line_down * above) < 0.15F);
                }
                check_segment(&view, 320, 480);
                check_segment(&view, 480, 320);
                check_segment(&view, 32, 48);
                ++count;
            }
        }
    }
    printf("PASS %u retained-state sphere/rotation cases; max inclination error %.6f radians\n",
        count, (double)largest_inclination_error);
}

static void test_renderer(void)
{
    enum { WIDTH = 320, HEIGHT = 480, STRIDE = 327, GUARD = 16 };
    size_t count = (size_t)STRIDE * HEIGHT + 2 * GUARD;
    uint32_t *first = malloc(count * sizeof(*first));
    uint32_t *second = malloc(count * sizeof(*second));
    assert(first != NULL && second != NULL);
    for (size_t index = 0; index < count; ++index) {
        first[index] = UINT32_C(0x12345678);
        second[index] = UINT32_C(0x12345678);
    }
    struct spirit_level_surface a = {first + GUARD, WIDTH, HEIGHT, STRIDE};
    struct spirit_level_surface b = {second + GUARD, WIDTH, HEIGHT, STRIDE};
    struct spirit_level_geometry view = reading(4, 5, 7, 0);
    char diagnostics[64];
    spirit_level_diagnostics(diagnostics, 1.25F, -2.5F, 9.75F);
    assert(strcmp(diagnostics, "X +1.25 Y -2.5 Z +9.75") == 0);
    assert(spirit_level_draw(a, &view, diagnostics));
    spirit_level_diagnostics(diagnostics, FLT_MAX, -FLT_MAX, FLT_MIN);
    assert(strstr(diagnostics, "X ") && strstr(diagnostics, " Y ") && strstr(diagnostics, " Z "));
    assert(strlen(diagnostics) * 6 + 4 < WIDTH);
    assert(spirit_level_draw(b, &view, diagnostics));
    size_t line_pixels = 0;
    size_t marker_pixels = 0;
    size_t footer_difference = 0;
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < STRIDE; ++x) {
            size_t index = (size_t)y * STRIDE + (size_t)x;
            if (x >= WIDTH) {
                assert(a.pixels[index] == UINT32_C(0x12345678));
                assert(b.pixels[index] == UINT32_C(0x12345678));
            } else if (y < HEIGHT - SPIRIT_LEVEL_FOOTER_HEIGHT) {
                assert(a.pixels[index] == b.pixels[index]);
                line_pixels += a.pixels[index] == SPIRIT_LEVEL_LINE;
                marker_pixels += a.pixels[index] == SPIRIT_LEVEL_MARKER;
            } else {
                footer_difference += a.pixels[index] != b.pixels[index];
            }
        }
    }
    assert(line_pixels > 200 && marker_pixels > 0 && footer_difference > 0);
    for (int index = 0; index < GUARD; ++index) {
        assert(first[index] == UINT32_C(0x12345678));
        assert(second[index] == UINT32_C(0x12345678));
        assert(first[count - 1U - (size_t)index] == UINT32_C(0x12345678));
        assert(second[count - 1U - (size_t)index] == UINT32_C(0x12345678));
    }
    /* Re-render a vertical line, a face-down pole and unavailable states. */
    view = reading(9.81F, 0, 0, 0);
    assert(spirit_level_draw(a, &view, diagnostics));
    view = reading(0, 0, -9.81F, 0);
    assert(spirit_level_draw(a, &view, diagnostics));
    for (int status = SPIRIT_LEVEL_WAITING; status <= SPIRIT_LEVEL_NO_SENSOR; ++status) {
        view.status = (enum spirit_level_status)status;
        assert(spirit_level_draw(a, &view, diagnostics));
        for (int y = 0; y < HEIGHT - SPIRIT_LEVEL_FOOTER_HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                uint32_t pixel = a.pixels[(size_t)y * STRIDE + (size_t)x];
                assert(pixel != SPIRIT_LEVEL_MARKER && pixel != SPIRIT_LEVEL_LINE);
            }
        }
    }
    a.stride = WIDTH - 1;
    assert(!spirit_level_draw(a, &view, diagnostics));
    a.stride = STRIDE;
    a.height = 1;
    assert(!spirit_level_draw(a, &view, diagnostics));
    a.height = HEIGHT;
    a.pixels = NULL;
    assert(!spirit_level_draw(a, &view, diagnostics));
    free(first);
    free(second);
    puts("PASS production pixel renderer, raw-only footer, clipping and buffer guards");
}

int main(void)
{
    test_cardinals();
    test_unavailable();
    test_sphere();
    test_renderer();
    puts("PASS spirit-level synthetic acceptance (not Android or physical-device execution)");
    return 0;
}
