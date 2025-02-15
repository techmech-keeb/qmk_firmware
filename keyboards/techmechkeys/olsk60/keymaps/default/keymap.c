#include QMK_KEYBOARD_H
#include "ps2_mouse.h"
#include "ps2.h"

// マウス操作の状態管理
static bool trackpoint_active = false;
static uint16_t trackpoint_timer = 0;
static bool waiting_for_timeout = false;
#define MOUSE_TIMEOUT 800  // マウス停止判定時間（ms）
static bool first_report = true;  // 初回レポート判定用

// キーボード初期化時の処理
void keyboard_post_init_user(void) {
    // LED初期化
    rgblight_enable();
    rgblight_mode(RGBLIGHT_MODE_STATIC_LIGHT);
    rgblight_sethsv(0, 0, 13); // 白色、輝度5%

    // マウス状態初期化
    trackpoint_active = false;
    trackpoint_timer = timer_read();
    waiting_for_timeout = false;
    first_report = true;

    // マウスレポートの初期化
    report_mouse_t empty_report = {0};
    host_mouse_send(&empty_report);
    wait_ms(50);
}

// PS2マウスの初期化前処理
void ps2_mouse_init_user(void) {
    // マウスモードをリモートモードに設定
    ps2_mouse_set_remote_mode();
    wait_ms(50);

    report_mouse_t empty_report = {0};
    host_mouse_send(&empty_report);
    wait_ms(50);
}

// PS2マウス初期化成功時の処理
void ps2_mouse_init_success_user(void) {
    // マウスの解像度、スケーリング、サンプルレートの設定
    ps2_mouse_set_resolution(PS2_MOUSE_4_COUNT_MM);
    ps2_mouse_set_scaling_1_1();
    ps2_mouse_set_sample_rate(PS2_MOUSE_80_SAMPLES_SEC);

    report_mouse_t empty_report = {0};
    host_mouse_send(&empty_report);
    wait_ms(50);
}

// PS2マウスの移動検出時の処理
void ps2_mouse_moved_user(report_mouse_t *mouse_report) {
    static uint16_t last_movement_timer = 0;
    static bool is_accelerating = false;
    static float current_acceleration = 1.0f;
    uint16_t current_time = timer_read();

    // 初回レポート時の初期化処理
    if (first_report) {
        first_report = false;
        mouse_report->buttons = 0;
        last_movement_timer = current_time;
        return;
    }

    // マウス移動時の加速処理
    if (mouse_report->x != 0 || mouse_report->y != 0) {
        // Y軸の上方向（負の値）の移動を2倍に
        int8_t y_movement = mouse_report->y;
        if (y_movement < 0) {
            y_movement = (int8_t)(y_movement * 2.0f);
        }

        // 継続的な移動の検出と加速度の適用
        uint16_t movement_interval = timer_elapsed(last_movement_timer);

        if (movement_interval < MOUSEKEY_INTERVAL * 4) {
            if (!is_accelerating) {
                is_accelerating = true;
                current_acceleration = 1.0f;
            } else {
                // 加速度の計算（最大20倍まで）
                float time_factor = (float)timer_elapsed(last_movement_timer) /
                                  (MOUSEKEY_TIME_TO_MAX * 2);
                if (time_factor > 1.0f) time_factor = 1.0f;

                // 二次曲線的な加速（1.0 + 19.0 = 最大20倍）
                current_acceleration = 1.0f + (time_factor * time_factor * 19.0f);
            }
        } else {
            is_accelerating = false;
            current_acceleration = 1.0f;
        }

        // 加速度の適用
        mouse_report->x = (int8_t)(mouse_report->x * current_acceleration);
        mouse_report->y = (int8_t)(y_movement * current_acceleration);

        last_movement_timer = current_time;

        // マウス操作レイヤーの有効化と状態更新
        trackpoint_active = true;
        trackpoint_timer = current_time;
        waiting_for_timeout = true;
        layer_on(3);
    }
}

// レイヤー状態変更時のLED制御
layer_state_t layer_state_set_user(layer_state_t state) {
    switch (get_highest_layer(state)) {
        case 0:
            rgblight_sethsv(0, 0, 13);     // レイヤー0: 白色、輝度5%
            break;
        case 1:
            rgblight_sethsv(190, 100, 26);  // レイヤー1: Light Sky Blue、輝度10%
            break;
        case 2:
            rgblight_sethsv(35, 255, 26);   // レイヤー2: Orange、輝度10%
            break;
        case 3:
            rgblight_sethsv(0, 255, 38);    // レイヤー3: Red、輝度15%
            break;
    }
    return state;
}

// キー入力時の処理
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // マウスボタンの処理
    switch (keycode) {
        case KC_MS_BTN1:
        case KC_MS_BTN2:
        case KC_MS_BTN3:
        case KC_MS_BTN4:
        case KC_MS_BTN5:
            if (IS_LAYER_ON(3)) {
                return true;
            }
            break;
    }
    return true;
}

// キーマップ定義
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

//  Ortholinear + Split-Space bar
//
//    ┌─────┬─────┬─────┬─────┬─────┬─────┐    ┌─────┬─────┬─────┬─────┬─────┬──────────┐
//    │ ESC │ 1   │ 2   │ 3   │ 4   │ 5   │    │ 6   │ 7   │ 8   │ 9   │ 0   │Backspace │
//  ┌─┴─────┼─────┼─────┼─────┼─────┼─────┤    ├─────┼─────┼─────┼─────┼─────┼─────┬────└──┐
//  │ Tab   │ Q   │ W   │ E   │ R   │ T   │    │ G   │ H   │ I   │ J   │ K   │ [   │  \    │
// ┌┴───────┼─────┼─────┼─────┼─────┼─────┤    ├─────┼─────┼─────┼─────┼─────┼─────┴──────┬┘
// │ Ctrl   │ A   │ S   │ D   │ F   │ G   │    │ H   │ J   │ K   │ L   │ ;   │ Enter      │
// ├────────┼─────┼─────┼─────┼─────┼─────┤    ├─────┼─────┼─────┼─────┼─────┼─────┬──────┤
// │ Shift  │ Z   │ X   │ C   │ V   │ B   │    │ N   │ M   │ ,   │ .   │ /   │UP   │Shift │
// └─┬──────┼─────┴┬────┴─┬───┴─────┴────┬┴────┴┬────┴─────┴─────┼─────┼─────┼─────┼─────┬┘ ┌─────┐
//   │ Ctrl │ Gui  │ Alt  │  Space       │MO(1) │  Del           │MO(2)│Left │Down │Right│  │ GUI │
//   └──────┴──────┴──────┴──────────────┴──────┴────────────────┴─────┴─────┴─────┴─────┘  └─────┘

    [0] = LAYOUT(
        KC_ESC,   KC_1,    KC_2,    KC_3,    KC_4,    KC_5,        KC_6,    KC_7,    KC_8,    KC_9,    KC_0,     KC_BSPC,
        KC_TAB,   KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,        KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,     KC_LBRC,  KC_BSLS,
        KC_LCTL,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G,        KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_ENT,
        KC_LSFT,  KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,        KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,  KC_UP,    KC_RSFT,
        KC_LCTL,  KC_LGUI, KC_LALT,          KC_SPC,        MO(1),          KC_DEL,           MO(2),   KC_LEFT,  KC_DOWN,  KC_RGHT, KC_LGUI
    ),
    [1] = LAYOUT(
        KC_GRV,   KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,       KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,   KC_BSPC,
        KC_TAB,   KC_F11,  KC_F12,  KC_UP,   KC_R,    KC_T,        KC_Y,    KC_U,    KC_INS,  KC_O,    KC_PSCR,  KC_LBRC,  KC_INT3,
        KC_CAPS,  KC_A,    KC_LEFT, KC_DOWN, KC_RGHT, KC_G,        KC_H,    KC_MINS, KC_EQL,  KC_LBRC, KC_RBRC,  KC_ENT,
        KC_LSFT,  KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,        KC_NUM,  KC_M,    KC_QUOT, KC_DOT,  KC_SLSH,  KC_UP,    KC_RSFT,
        KC_LCTL,  KC_LGUI, KC_LALT,          KC_SPC,        MO(1),          KC_DEL,           MO(2),   KC_LEFT,  KC_DOWN,  KC_RGHT, KC_LGUI
    ),
    [2] = LAYOUT(
        KC_ESC,   KC_1,    KC_2,    KC_3,    KC_4,    KC_5,        KC_6,    KC_7,    KC_8,    KC_9,    KC_0,     KC_BSPC,
        KC_TAB,   KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,        KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,     KC_LBRC,  KC_BSLS,
        KC_LCTL,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G,        KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_ENT,
        KC_LSFT,  KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,        KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,  KC_UP,    KC_RSFT,
        KC_LCTL,  KC_LGUI, KC_LALT,          KC_SPC,        MO(1),          KC_DEL,           MO(2),   KC_LEFT,  KC_DOWN,  KC_RGHT, KC_LGUI
    ),
    [3] = LAYOUT(
        KC_ESC,   KC_1,    KC_2,    KC_3,    KC_4,    KC_5,        KC_6,    KC_7,       KC_8,       KC_9,       KC_0,       KC_BSPC,
        KC_TAB,   KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,        KC_Y,    KC_U,       KC_I,       KC_O,       KC_P,       KC_LBRC,  KC_BSLS,
        KC_LCTL,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G,        KC_H,    KC_MS_BTN1, KC_MS_BTN2, KC_MS_BTN4, KC_MS_BTN5, KC_ENT,
        KC_LSFT,  KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,        KC_N,    KC_M,       KC_COMM,    KC_DOT,     KC_SLSH,    KC_UP,    KC_RSFT,
        KC_LCTL,  KC_LGUI, KC_LALT,          KC_MS_BTN1,   KC_MS_BTN3,      KC_MS_BTN2,             MO(1),      KC_LEFT,    KC_DOWN,  KC_RGHT, KC_LGUI
    ),

};

#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
    [0] =   { ENCODER_CCW_CW(KC_VOLU, KC_VOLD) },
    [1] =   { ENCODER_CCW_CW(KC_PGUP, KC_PGDN) },
    [2] =   { ENCODER_CCW_CW(XXXXXXX, XXXXXXX) },
    [3] =   { ENCODER_CCW_CW(XXXXXXX, XXXXXXX) }
};
#endif

// マトリックススキャン後の処理を追加
void matrix_scan_user(void) {
    // マウス停止時のタイムアウトチェック
    if (waiting_for_timeout && timer_elapsed(trackpoint_timer) > MOUSE_TIMEOUT) {
        if (trackpoint_active) {
            trackpoint_active = false;
            layer_off(3);
        }
        waiting_for_timeout = false;
    }
}
