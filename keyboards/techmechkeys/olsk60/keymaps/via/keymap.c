#include QMK_KEYBOARD_H
#include "ps2_mouse.h"
#include "ps2.h"
#include <stdbool.h>
#include <stdint.h>
#include "eeconfig.h"

// マウス操作の状態管理
static bool trackpoint_active = false;
static uint16_t trackpoint_timer = 0;
static bool waiting_for_timeout = false;
static bool mouse_button_active = false;  // マウスボタンの状態を追加
static bool timeout_occurred = false;     // タイムアウト発生フラグを追加
#define MOUSE_TIMEOUT 800  // マウス停止判定時間（ms）
static bool first_report = true;  // 初回レポート判定用

// レイヤーホールドの状態管理
static bool layer3_held = false;

// 加速水準の定義
typedef enum {
    ACCEL_VL = 0, // Very Low
    ACCEL_L,
    ACCEL_M,
    ACCEL_H,
    ACCEL_VH,
    ACCEL_LEVELS
} accel_level_t;

// 各水準の最大加速倍率
static const float accel_max_factors[ACCEL_LEVELS] = {
    5.0f,   // VL
    10.0f,  // L
    25.0f,  // M
    45.0f,  // H
    60.0f   // VH
};

// レイヤー1・2のLED色（HSV）
#define LAYER1_COLOR_HSV 120, 255, 26  // 水色
#define LAYER2_COLOR_HSV 85, 255, 26   // 緑

// 各加速水準のLED色（HSV）
static const uint8_t accel_led_colors[ACCEL_LEVELS][3] = {
    { 35, 255, 38 },   // VL: 黄色
    { 11, 255, 38 },   // L: オレンジ
    { 0, 255, 38 },    // M: 赤
    { 200, 255, 38 },  // H: 紫
    { 140, 255, 38 }   // VH: 青
};

// EEPROM用マジックナンバー
#define ACCEL_LEVEL_MAGIC 0xA5

// 現在の加速水準
static accel_level_t current_accel_level = ACCEL_M;

// マウス加速時間の水準定義
#define MOUSEKEY_TIME_TO_MAX_LOW 120   // 低速設定
#define MOUSEKEY_TIME_TO_MAX_MED 80    // 中速設定
#define MOUSEKEY_TIME_TO_MAX_HIGH 50   // 高速設定

// EEPROM用マジックナンバー（既存のACCEL_LEVEL_MAGICとは別の値を使用）
#define MOUSE_ACCEL_TIME_MAGIC 0xB5

// 現在の加速時間設定を保持する変数
static uint8_t current_mouse_accel_time = MOUSEKEY_TIME_TO_MAX_MED;

// EEPROMから加速水準を読み込む（QMK user_config領域を利用）
void load_accel_level_from_eeprom(void) {
    uint32_t val = eeconfig_read_user();
    uint8_t magic = (val >> 8) & 0xFF;
    uint8_t accel = (val >> 0) & 0xFF;

    if (magic == ACCEL_LEVEL_MAGIC && accel < ACCEL_LEVELS) {
        // 初期化済みなら保存値を使う
        current_accel_level = (accel_level_t)accel;
    } else {
        // 未初期化ならMで初期化
        current_accel_level = ACCEL_M;
        // マジックナンバーとともに保存
        uint32_t new_val = (val & 0xFFFF0000) | (ACCEL_LEVEL_MAGIC << 8) | ((uint8_t)ACCEL_M);
        eeconfig_update_user(new_val);
    }
}

// EEPROMに加速水準を書き込む
void save_accel_level_to_eeprom(accel_level_t level) {
    uint32_t val = eeconfig_read_user();
    // マジックナンバーも必ずセット
    val = (val & 0xFFFF0000) | (ACCEL_LEVEL_MAGIC << 8) | ((uint8_t)level);
    eeconfig_update_user(val);
}

// EEPROMから加速時間設定を読み込む
void load_mouse_accel_time_from_eeprom(void) {
    uint32_t val = eeconfig_read_user();
    uint8_t magic = (val >> 16) & 0xFF;
    uint8_t accel_time = (val >> 24) & 0xFF;

    if (magic == MOUSE_ACCEL_TIME_MAGIC) {
        // 保存された値が有効な範囲内かチェック
        if (accel_time == MOUSEKEY_TIME_TO_MAX_LOW ||
            accel_time == MOUSEKEY_TIME_TO_MAX_MED ||
            accel_time == MOUSEKEY_TIME_TO_MAX_HIGH) {
            current_mouse_accel_time = accel_time;
        } else {
            // 無効な値の場合はデフォルト値を使用
            current_mouse_accel_time = MOUSEKEY_TIME_TO_MAX_MED;
        }
    } else {
        // 未初期化の場合はデフォルト値を使用
        current_mouse_accel_time = MOUSEKEY_TIME_TO_MAX_MED;
    }
}

// EEPROMに加速時間設定を保存
void save_mouse_accel_time_to_eeprom(uint8_t accel_time) {
    uint32_t val = eeconfig_read_user();
    // マジックナンバーとともに保存
    val = (val & 0x00FFFFFF) | (MOUSE_ACCEL_TIME_MAGIC << 16) | (accel_time << 24);
    eeconfig_update_user(val);
}

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

    load_accel_level_from_eeprom();
    load_mouse_accel_time_from_eeprom();
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
        // Y軸の上方向（負の値）の移動を1.5倍に
        int8_t y_movement = mouse_report->y;
        if (y_movement < 0) {
            y_movement = (int8_t)(y_movement * 1.5f);
        }

        // 継続的な移動の検出と加速度の適用
        uint16_t movement_interval = timer_elapsed(last_movement_timer);

        if (movement_interval < current_mouse_accel_time * 4) {
            if (!is_accelerating) {
                is_accelerating = true;
                current_acceleration = 1.0f;
            } else {
                // 加速度の計算（最大倍率は水準ごとに変化）
                float time_factor = (float)timer_elapsed(last_movement_timer) /
                                  (current_mouse_accel_time * 2);
                if (time_factor > 1.0f) time_factor = 1.0f;

                float max_factor = accel_max_factors[current_accel_level];
                current_acceleration = 1.0f + (time_factor * time_factor * (max_factor - 1.0f));
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
            rgblight_sethsv(0, 0, 13); // 白
            break;
        case 1:
            rgblight_sethsv(LAYER1_COLOR_HSV); // 水色
            break;
        case 2:
            rgblight_sethsv(LAYER2_COLOR_HSV); // 緑
            break;
        case 3: {
            if (trackpoint_active || mouse_button_active || layer3_held) {
                // マウス操作、マウスボタン、またはレイヤーホールドがアクティブな場合は、レイヤー3の色を維持
                const uint8_t *color = accel_led_colors[current_accel_level];
                rgblight_sethsv(color[0], color[1], color[2]);
            } else {
                // すべて非アクティブな場合は、レイヤー0の色に戻す
                rgblight_sethsv(0, 0, 13);
            }
            break;
        }
    }
    return state;
}

// カスタムキーコード定義
enum custom_keycodes {
<<<<<<< HEAD
    ACCEL_VL_KEY = SAFE_RANGE,
    ACCEL_L_KEY,
    ACCEL_M_KEY,
    ACCEL_H_KEY,
    ACCEL_VH_KEY,
    LAYER3_HOLD_KEY,
    // 新しい加速時間設定用キーコードを追加
    MOUSE_ACCEL_LOW,
    MOUSE_ACCEL_MED,
    MOUSE_ACCEL_HIGH
};

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
                bool previous_state = mouse_button_active;
                mouse_button_active = record->event.pressed;

                if (mouse_button_active) {
                    trackpoint_active = true;
                    trackpoint_timer = timer_read();
                    waiting_for_timeout = true;
                    timeout_occurred = false;
                } else if (previous_state) {
                    if (timeout_occurred) {
                        layer_off(3);
                    } else {
                        trackpoint_timer = timer_read();
                        waiting_for_timeout = true;
                    }
                }
                return true;
            }
            break;
    }

    // 加速水準変更キーの処理
    if (record->event.pressed) {
        switch (keycode) {
            case ACCEL_VL_KEY:
            case ACCEL_L_KEY:
            case ACCEL_M_KEY:
            case ACCEL_H_KEY:
            case ACCEL_VH_KEY: {
                accel_level_t new_level = (accel_level_t)(keycode - ACCEL_VL_KEY);
                if (new_level < ACCEL_LEVELS) {
                    current_accel_level = new_level;
                    save_accel_level_to_eeprom(new_level);
                    const uint8_t *color = accel_led_colors[new_level];
                    rgblight_sethsv(color[0], color[1], color[2]);
                }
                return false;
            }
            // 新しい加速時間設定用のケースを追加
            case MOUSE_ACCEL_LOW:
                current_mouse_accel_time = MOUSEKEY_TIME_TO_MAX_LOW;
                save_mouse_accel_time_to_eeprom(MOUSEKEY_TIME_TO_MAX_LOW);
                return false;
            case MOUSE_ACCEL_MED:
                current_mouse_accel_time = MOUSEKEY_TIME_TO_MAX_MED;
                save_mouse_accel_time_to_eeprom(MOUSEKEY_TIME_TO_MAX_MED);
                return false;
            case MOUSE_ACCEL_HIGH:
                current_mouse_accel_time = MOUSEKEY_TIME_TO_MAX_HIGH;
                save_mouse_accel_time_to_eeprom(MOUSEKEY_TIME_TO_MAX_HIGH);
                return false;
        }
    }

    // レイヤーホールドキーの処理
    if (keycode == LAYER3_HOLD_KEY) {
        layer3_held = record->event.pressed;
        if (layer3_held) {
            layer_on(3);
        } else {
            layer_off(3);
        }
        return false;
    }

    return true;
}

// VIA用のカスタムキーコード処理を追加
void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    uint8_t command_id = data[0];
    switch (command_id) {
        case 0x01: { // 加速水準変更
            uint8_t level = data[1];
            if (level < ACCEL_LEVELS) {
                current_accel_level = (accel_level_t)level;
                save_accel_level_to_eeprom(current_accel_level);
                const uint8_t *color = accel_led_colors[current_accel_level];
                rgblight_sethsv(color[0], color[1], color[2]);
            }
            break;
        }
    }
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
//   │ Ctrl │ Gui  │ Alt  │  Space       │MO(1) │  Del           │MO(2)│Left │Down │Right│  │Down │
//   └──────┴──────┴──────┴──────────────┴──────┴────────────────┴─────┴─────┴─────┴─────┘  └─────┘

    [0] = LAYOUT(
        KC_ESC,   KC_1,    KC_2,    KC_3,    KC_4,    KC_5,        KC_6,    KC_7,    KC_8,    KC_9,    KC_0,     KC_BSPC,
        KC_TAB,   KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,        KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,     KC_LBRC,  KC_BSLS,
        KC_LCTL,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G,        KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_ENT,
        KC_LSFT,  KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,        KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,  KC_UP,    KC_RSFT,
        KC_LCTL,  KC_LGUI, KC_LALT,          KC_SPC,        MO(1),          KC_DEL,           MO(2),   KC_LEFT,  KC_DOWN,  KC_RGHT, KC_DOWN
    ),
    [1] = LAYOUT(
        KC_GRV,   KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,       KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,   KC_BSPC,
        KC_TAB,   KC_F11,  KC_F12,  KC_UP,   KC_R,    KC_T,        KC_Y,    KC_U,    KC_INS,  KC_O,    KC_PSCR,  KC_LBRC,  KC_INT3,
        KC_CAPS,  KC_A,    KC_LEFT, KC_DOWN, KC_RGHT, KC_G,        KC_H,    KC_MINS, KC_EQL,  KC_LBRC, KC_RBRC,  KC_ENT,
        KC_LSFT,  KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,        KC_NUM,  KC_M,    KC_QUOT, KC_DOT,  KC_SLSH,  KC_UP,    KC_RSFT,
        KC_LCTL,  KC_LGUI, KC_LALT,          KC_SPC,        MO(1),          KC_DEL,           MO(2),   KC_LEFT,  KC_DOWN,  KC_RGHT, KC_DOWN
    ),
    [2] = LAYOUT(
        KC_ESC,   ACCEL_VL_KEY, ACCEL_L_KEY, ACCEL_M_KEY, ACCEL_H_KEY, ACCEL_VH_KEY,   KC_6,    KC_7,    KC_8,    KC_9,    KC_0,     KC_BSPC,
        KC_TAB,   MOUSE_ACCEL_LOW, MOUSE_ACCEL_MED, MOUSE_ACCEL_HIGH, KC_R,    KC_T,        KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,     KC_LBRC,  KC_BSLS,
        KC_LCTL,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G,        KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_ENT,
        KC_LSFT,  KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,        KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,  KC_UP,    KC_RSFT,
        KC_LCTL,  KC_LGUI, KC_LALT,          KC_SPC,        MO(1),          KC_DEL,           MO(2),   KC_LEFT,  KC_DOWN,  KC_RGHT, KC_DOWN
    ),
    [3] = LAYOUT(
        KC_ESC,   KC_1,    KC_2,    KC_3,    KC_4,    KC_5,        KC_6,    KC_7,       KC_8,       KC_9,       KC_0,       KC_BSPC,
        KC_TAB,   KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,        KC_Y,    KC_U,       KC_I,       KC_O,       KC_P,       KC_LBRC,  KC_BSLS,
        KC_LCTL,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G,        KC_H,    KC_MS_BTN1, KC_MS_BTN2, KC_MS_BTN4, KC_MS_BTN5, KC_ENT,
        KC_LSFT,  KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,        KC_N,    KC_M,       KC_COMM,    KC_DOT,     KC_SLSH,    KC_UP,    KC_RSFT,
        LAYER3_HOLD_KEY,  KC_LGUI, KC_LALT,          KC_MS_BTN1,   KC_MS_BTN3,      KC_MS_BTN2,             MO(1),      KC_LEFT,    KC_DOWN,  KC_RGHT, KC_DOWN
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
        if (trackpoint_active && !mouse_button_active && !layer3_held) {
            // マウス操作が非アクティブで、マウスボタンも押されておらず、レイヤーホールドもされていない場合
            trackpoint_active = false;
            timeout_occurred = true;
            layer_off(3);
        } else if (mouse_button_active || layer3_held) {
            // マウスボタンが押されているか、レイヤーホールドされている場合はタイマーをリセット
            trackpoint_timer = timer_read();
            timeout_occurred = false;
        }
        waiting_for_timeout = false;
    }
}
