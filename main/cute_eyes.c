#include "lvgl.h"
#include "esp_log.h"
#include <stdlib.h>
#include <math.h>

static const char *TAG = "cute_eyes";

typedef enum {
    EMOTION_NEUTRAL = 0,
    EMOTION_HAPPY,
    EMOTION_CURIOUS,
    EMOTION_SLEEPY,
} eye_emotion_t;

typedef struct {
    lv_obj_t *white;
    lv_obj_t *blush;
    lv_obj_t *shadow;
    lv_obj_t *iris_outer;
    lv_obj_t *iris_inner;
    lv_obj_t *pupil;
    lv_obj_t *shine_big;
    lv_obj_t *shine_small;
    lv_obj_t *lid_top;
    lv_obj_t *lid_bottom;
} eye_parts_t;

typedef struct {
    lv_obj_t *root;
    eye_parts_t left;
    eye_parts_t right;
    lv_timer_t *timer;
    int16_t scr_w;
    int16_t scr_h;
    int16_t eye_rx;
    int16_t eye_ry;
    int16_t iris_r;
    int16_t pupil_r;
    int16_t shine_r1;
    int16_t shine_r2;
    int16_t left_cx;
    int16_t right_cx;
    int16_t eye_cy;
    uint32_t elapsed_ms;
    float look_x;
    float look_y;
    float target_x;
    float target_y;
    uint32_t next_target_ms;
    uint32_t next_blink_ms;
    uint8_t blink_state;
    uint32_t blink_tick;
    float blink_value;
    eye_emotion_t emotion;
    uint32_t emotion_tick;
    uint32_t emotion_duration_ms;
    float squint;
    float lash_lift;
    float pupil_scale;
} cute_eyes_t;

static cute_eyes_t eyes_state = {0};

static float frand_range(float min_v, float max_v)
{
    float r = (float)rand() / (float)RAND_MAX;
    return min_v + r * (max_v - min_v);
}

static uint32_t random_blink_delay(void)
{
    return 1500U + (uint32_t)(rand() % 4000);
}

static eye_emotion_t random_emotion(void)
{
    int pick = rand() % 6;
    if (pick <= 2) {
        return EMOTION_NEUTRAL;
    }
    if (pick == 3) {
        return EMOTION_HAPPY;
    }
    if (pick == 4) {
        return EMOTION_CURIOUS;
    }
    return EMOTION_SLEEPY;
}

static lv_obj_t *create_circle(lv_obj_t *parent, int32_t size, lv_color_t color, lv_opa_t opa)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, size, size);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(obj, opa, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
    return obj;
}

static void style_eye_white(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xE8F0FF), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 4, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_clip_corner(obj, true, 0);
}

static void create_eye_parts(eye_parts_t *eye, lv_obj_t *root, int16_t cx, int16_t cy)
{
    cute_eyes_t *s = &eyes_state;
    int16_t eye_w = s->eye_rx * 2;
    int16_t eye_h = s->eye_ry * 2;

    eye->white = lv_obj_create(root);
    lv_obj_remove_style_all(eye->white);
    lv_obj_set_size(eye->white, eye_w, eye_h);
    lv_obj_set_pos(eye->white, cx - s->eye_rx, cy - s->eye_ry);
    lv_obj_set_style_radius(eye->white, LV_RADIUS_CIRCLE, 0);
    style_eye_white(eye->white);

    eye->blush = lv_obj_create(eye->white);
    lv_obj_remove_style_all(eye->blush);
    lv_obj_set_size(eye->blush, (int32_t)(eye_w * 0.72f), (int32_t)(eye_h * 0.34f));
    lv_obj_set_style_radius(eye->blush, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(eye->blush, lv_color_hex(0xFF788C), 0);
    lv_obj_set_style_bg_opa(eye->blush, LV_OPA_20, 0);
    lv_obj_set_pos(eye->blush, (eye_w - lv_obj_get_width(eye->blush)) / 2, (int32_t)(eye_h * 0.62f));

    eye->shadow = lv_obj_create(eye->white);
    lv_obj_remove_style_all(eye->shadow);
    lv_obj_set_size(eye->shadow, eye_w, (int32_t)(eye_h * 0.46f));
    lv_obj_set_pos(eye->shadow, 0, 0);
    lv_obj_set_style_bg_color(eye->shadow, lv_color_hex(0x0A0A1E), 0);
    lv_obj_set_style_bg_opa(eye->shadow, LV_OPA_40, 0);
    lv_obj_set_style_radius(eye->shadow, LV_RADIUS_CIRCLE, 0);

    eye->iris_outer = create_circle(eye->white, s->iris_r * 2, lv_color_hex(0x1A7A2A), LV_OPA_COVER);
    lv_obj_set_style_border_width(eye->iris_outer, 2, 0);
    lv_obj_set_style_border_color(eye->iris_outer, lv_color_hex(0x0D2A5E), 0);

    eye->iris_inner = create_circle(eye->white, (int32_t)(s->iris_r * 1.35f), lv_color_hex(0x4DCC60), (lv_opa_t)220);

    eye->pupil = create_circle(eye->white, s->pupil_r * 2, lv_color_hex(0x080810), LV_OPA_COVER);
    lv_obj_set_style_bg_grad_color(eye->pupil, lv_color_hex(0x1A1A30), 0);
    lv_obj_set_style_bg_grad_dir(eye->pupil, LV_GRAD_DIR_VER, 0);

    eye->shine_big = create_circle(eye->white, s->shine_r1 * 2, lv_color_hex(0xFFFFFF), (lv_opa_t)250);
    eye->shine_small = create_circle(eye->white, s->shine_r2 * 2, lv_color_hex(0xFFFFFF), (lv_opa_t)160);

    eye->lid_top = lv_obj_create(root);
    lv_obj_remove_style_all(eye->lid_top);
    lv_obj_set_size(eye->lid_top, eye_w, s->eye_ry);
    lv_obj_set_style_bg_color(eye->lid_top, lv_color_hex(0x0A0A0F), 0);
    lv_obj_set_style_bg_opa(eye->lid_top, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(eye->lid_top, s->eye_ry, 0);

    eye->lid_bottom = lv_obj_create(root);
    lv_obj_remove_style_all(eye->lid_bottom);
    lv_obj_set_size(eye->lid_bottom, eye_w, s->eye_ry);
    lv_obj_set_style_bg_color(eye->lid_bottom, lv_color_hex(0x0A0A0F), 0);
    lv_obj_set_style_bg_opa(eye->lid_bottom, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(eye->lid_bottom, s->eye_ry, 0);
}

static void place_eye_core(eye_parts_t *eye, float look_x, float look_y, float wobble_x, float wobble_y)
{
    cute_eyes_t *s = &eyes_state;
    int16_t eye_w = s->eye_rx * 2;
    int16_t eye_h = s->eye_ry * 2;
    float shift_limit_x = s->eye_rx * 0.34f;
    float shift_limit_y = s->eye_ry * 0.34f;

    float raw_x = (float)s->eye_rx + look_x + wobble_x;
    float raw_y = (float)s->eye_ry + look_y + wobble_y;
    if (raw_x < s->iris_r) raw_x = s->iris_r;
    if (raw_x > eye_w - s->iris_r) raw_x = (float)(eye_w - s->iris_r);
    if (raw_y < s->iris_r) raw_y = s->iris_r;
    if (raw_y > eye_h - s->iris_r) raw_y = (float)(eye_h - s->iris_r);

    if (raw_x < s->eye_rx - shift_limit_x) raw_x = s->eye_rx - shift_limit_x;
    if (raw_x > s->eye_rx + shift_limit_x) raw_x = s->eye_rx + shift_limit_x;
    if (raw_y < s->eye_ry - shift_limit_y) raw_y = s->eye_ry - shift_limit_y;
    if (raw_y > s->eye_ry + shift_limit_y) raw_y = s->eye_ry + shift_limit_y;

    int16_t iris_cx = (int16_t)raw_x;
    int16_t iris_cy = (int16_t)raw_y;

    int16_t inner_r = (int16_t)(s->iris_r * 0.67f);
    int16_t pupil_r = (int16_t)(s->pupil_r * s->pupil_scale);
    if (pupil_r < 4) pupil_r = 4;

    lv_obj_set_size(eye->pupil, pupil_r * 2, pupil_r * 2);
    lv_obj_set_pos(eye->iris_outer, iris_cx - s->iris_r, iris_cy - s->iris_r);
    lv_obj_set_pos(eye->iris_inner, iris_cx - inner_r - (int16_t)(s->iris_r * 0.20f), iris_cy - inner_r - (int16_t)(s->iris_r * 0.20f));
    lv_obj_set_pos(eye->pupil, iris_cx - pupil_r, iris_cy - pupil_r);

    lv_obj_set_pos(eye->shine_big,
                   iris_cx - pupil_r - (int16_t)(s->iris_r * 0.28f),
                   iris_cy - pupil_r - (int16_t)(s->iris_r * 0.28f));
    lv_obj_set_pos(eye->shine_small,
                   iris_cx + (int16_t)(s->iris_r * 0.18f),
                   iris_cy + (int16_t)(s->iris_r * 0.20f));
}

static void place_eyelids(eye_parts_t *eye, int16_t center_x, int16_t center_y)
{
    cute_eyes_t *s = &eyes_state;
    float close_amount = s->blink_value;
    if (s->squint > close_amount) {
        close_amount = s->squint;
    }

    int16_t eye_w = s->eye_rx * 2;
    int16_t lid_h = s->eye_ry;

    int16_t top_open = center_y - s->eye_ry - lid_h + 2;
    int16_t top_closed = center_y - (lid_h / 2);
    int16_t bottom_open = center_y + s->eye_ry - 2;
    int16_t bottom_closed = center_y - (lid_h / 2);

    int16_t top_y = (int16_t)(top_open + (top_closed - top_open) * close_amount);
    int16_t bottom_y = (int16_t)(bottom_open + (bottom_closed - bottom_open) * close_amount);

    lv_obj_set_size(eye->lid_top, eye_w, lid_h);
    lv_obj_set_size(eye->lid_bottom, eye_w, lid_h);
    lv_obj_set_pos(eye->lid_top, center_x - s->eye_rx, top_y);
    lv_obj_set_pos(eye->lid_bottom, center_x - s->eye_rx, bottom_y);
}

static void choose_new_target(void)
{
    cute_eyes_t *s = &eyes_state;
    float max_x = s->eye_rx * 0.33f;
    float max_y = s->eye_ry * 0.30f;
    s->target_x = frand_range(-max_x, max_x);
    s->target_y = frand_range(-max_y, max_y);
    s->next_target_ms = 600U + (uint32_t)(rand() % 2200);
}

static void update_emotion(uint32_t dt)
{
    cute_eyes_t *s = &eyes_state;
    if (s->emotion_duration_ms <= dt) {
        s->emotion = random_emotion();
        s->emotion_duration_ms = 3000U + (uint32_t)(rand() % 4000);
    } else {
        s->emotion_duration_ms -= dt;
    }

    float target_squint = 0.0f;
    float target_lash = 0.0f;
    float target_pupil_scale = 1.0f;

    if (s->emotion == EMOTION_HAPPY) {
        target_squint = 0.20f;
        target_pupil_scale = 0.86f;
    } else if (s->emotion == EMOTION_CURIOUS) {
        target_lash = 1.0f;
        target_pupil_scale = 0.95f;
    } else if (s->emotion == EMOTION_SLEEPY) {
        target_squint = 0.38f;
        target_pupil_scale = 1.10f;
    }

    s->squint += (target_squint - s->squint) * 0.10f;
    s->lash_lift += (target_lash - s->lash_lift) * 0.10f;
    s->pupil_scale += (target_pupil_scale - s->pupil_scale) * 0.12f;
}

static void update_blink(uint32_t dt)
{
    cute_eyes_t *s = &eyes_state;

    if (s->blink_state == 0) {
        if (s->next_blink_ms <= dt) {
            s->blink_state = 1;
            s->blink_tick = 0;
        } else {
            s->next_blink_ms -= dt;
        }
        return;
    }

    s->blink_tick += dt;
    if (s->blink_state == 1) {
        s->blink_value = (float)s->blink_tick / 80.0f;
        if (s->blink_value >= 1.0f) {
            s->blink_value = 1.0f;
            s->blink_state = 2;
            s->blink_tick = 0;
        }
    } else {
        s->blink_value = 1.0f - ((float)s->blink_tick / 80.0f);
        if (s->blink_value <= 0.0f) {
            s->blink_value = 0.0f;
            s->blink_state = 0;
            s->next_blink_ms = random_blink_delay();
            if (s->emotion == EMOTION_SLEEPY) {
                s->next_blink_ms /= 2;
            }
        }
    }
}

static void cute_eyes_apply_layout(void)
{
    cute_eyes_t *s = &eyes_state;
    int16_t eye_w = s->eye_rx * 2;
    int16_t eye_h = s->eye_ry * 2;

    lv_obj_set_pos(s->left.white, s->left_cx - s->eye_rx, s->eye_cy - s->eye_ry);
    lv_obj_set_size(s->left.white, eye_w, eye_h);
    lv_obj_set_style_radius(s->left.white, s->eye_ry, 0);

    lv_obj_set_pos(s->right.white, s->right_cx - s->eye_rx, s->eye_cy - s->eye_ry);
    lv_obj_set_size(s->right.white, eye_w, eye_h);
    lv_obj_set_style_radius(s->right.white, s->eye_ry, 0);
}

void cute_eyes_init(lv_obj_t *parent)
{
    cute_eyes_t *s = &eyes_state;
    s->root = parent;

    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0A0A0F), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    s->scr_w = lv_obj_get_width(parent);
    s->scr_h = lv_obj_get_height(parent);

    int16_t min_side = (s->scr_w < s->scr_h) ? s->scr_w : s->scr_h;
    s->eye_rx = (int16_t)(min_side * 0.17f);
    if (s->eye_rx < 48) s->eye_rx = 48;
    s->eye_ry = (int16_t)(s->eye_rx * 0.85f);
    s->iris_r = (int16_t)(s->eye_rx * 0.60f);
    s->pupil_r = (int16_t)(s->eye_rx * 0.48f);
    s->shine_r1 = (int16_t)(s->eye_rx * 0.13f);
    s->shine_r2 = (int16_t)(s->eye_rx * 0.07f);

    int16_t gap = (int16_t)(s->eye_rx * 0.45f);
    s->left_cx = (int16_t)(s->scr_w / 2 - s->eye_rx - gap);
    s->right_cx = (int16_t)(s->scr_w / 2 + s->eye_rx + gap);
    s->eye_cy = (int16_t)(s->scr_h / 2);

    create_eye_parts(&s->left, parent, s->left_cx, s->eye_cy);
    create_eye_parts(&s->right, parent, s->right_cx, s->eye_cy);
    cute_eyes_apply_layout();

    s->look_x = 0.0f;
    s->look_y = 0.0f;
    s->target_x = 0.0f;
    s->target_y = 0.0f;
    s->next_blink_ms = random_blink_delay();
    s->blink_state = 0;
    s->blink_tick = 0;
    s->blink_value = 0.0f;
    s->emotion = EMOTION_NEUTRAL;
    s->emotion_duration_ms = 2500;
    s->squint = 0.0f;
    s->lash_lift = 0.0f;
    s->pupil_scale = 1.0f;
    choose_new_target();
}

static void cute_eyes_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    cute_eyes_t *s = &eyes_state;
    const uint32_t dt = 50;
    s->elapsed_ms += dt;

    if (s->next_target_ms <= dt) {
        choose_new_target();
    } else {
        s->next_target_ms -= dt;
    }

    s->look_x += (s->target_x - s->look_x) * 0.08f;
    s->look_y += (s->target_y - s->look_y) * 0.08f;

    update_blink(dt);
    update_emotion(dt);

    float wobble_lx = sinf((float)s->elapsed_ms * 0.0023f) * s->eye_rx * 0.04f;
    float wobble_ly = cosf((float)s->elapsed_ms * 0.0031f) * s->eye_ry * 0.03f;
    float wobble_rx = sinf((float)s->elapsed_ms * 0.0019f + 1.2f) * s->eye_rx * 0.04f;
    float wobble_ry = cosf((float)s->elapsed_ms * 0.0027f + 0.5f) * s->eye_ry * 0.03f;

    place_eye_core(&s->left, s->look_x, s->look_y, wobble_lx, wobble_ly);
    place_eye_core(&s->right, s->look_x, s->look_y, wobble_rx, wobble_ry);

    place_eyelids(&s->left, s->left_cx, s->eye_cy - (int16_t)(s->lash_lift * 3.0f));
    place_eyelids(&s->right, s->right_cx, s->eye_cy);
}

void cute_eyes_start_animation(void)
{
    if (eyes_state.timer != NULL) {
        lv_timer_del(eyes_state.timer);
        eyes_state.timer = NULL;
    }
    eyes_state.timer = lv_timer_create(cute_eyes_timer_cb, 50, NULL);
}

void cute_eyes_demo(lv_obj_t *parent)
{
    cute_eyes_init(parent);
    cute_eyes_start_animation();
    ESP_LOGI(TAG, "Canvas-style cute eyes started");
}
