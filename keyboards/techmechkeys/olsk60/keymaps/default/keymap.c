#include QMK_KEYBOARD_H
#include "ps2_mouse.h"
#include "ps2.h"
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "eeconfig.h"
#include <stdlib.h>
#include "send_string.h"

// 音声定義
#define CUSTOM_STARTUP_SONG SONG(Q__NOTE(_C4), Q__NOTE(_E4), Q__NOTE(_G4), H__NOTE(_C5))
#define TYPEWRITER_SOUND SONG(E__NOTE(_C6), E__NOTE(_E6), E__NOTE(_G6))

// マウス制御の基本構造体
typedef struct {
    int16_t base_speed;      // 基本速度（256 = 1.0）
    int16_t acceleration;    // 加速度
    int16_t deceleration;    // 減速度
    int16_t smoothing;       // スムージング係数
} speed_profile_t;

// 速度水準の定義
typedef enum {
    SPEED_LEVEL_1 = 0,  // 最も遅い
    SPEED_LEVEL_2,
    SPEED_LEVEL_3,
    SPEED_LEVEL_4,
    SPEED_LEVEL_5,      // 最も速い
    SPEED_LEVEL_COUNT
} speed_level_t;

// 各速度水準の設定
static const speed_profile_t speed_profiles[SPEED_LEVEL_COUNT] = {
    { 128,  32,  64,  64 },  // レベル1: ゆっくり
    { 192,  48,  96,  96 },  // レベル2: やや遅め
    { 256,  64, 128, 128 },  // レベル3: 標準
    { 384,  96, 192, 192 },  // レベル4: やや速め
    { 512, 128, 256, 256 }   // レベル5: 高速
};

// 現在の設定
static speed_profile_t current_profile;
static uint8_t current_level = 2;  // デフォルトはレベル3

// EEPROM保存の最適化用変数
static bool settings_dirty = false;        // 設定が変更されたフラグ
static uint16_t last_save_time = 0;        // 最後に保存した時刻
static uint16_t save_debounce_time = 2000; // 保存遅延時間（ms）
static uint8_t save_count = 0;             // 保存回数カウンタ（デバッグ用）
static bool critical_settings_changed = false; // 重要な設定変更フラグ
static uint8_t change_count = 0;           // 変更回数カウンタ
static uint16_t last_change_time = 0;      // 最後の変更時刻

// --- レイヤー3 ON要因フラグ群 ---
static bool layer3_mouse_active = false;      // マウス操作
static bool layer3_mousebtn_active = false;   // マウスボタン
static bool layer3_standard_active = false;   // 標準レイヤー切り替え（MO(3), TG(3)等）

// プロトタイプ宣言
bool is_layer3_standard_key(uint16_t keycode);

// マウス操作の状態管理
static uint16_t trackpoint_timer = 0;
static bool waiting_for_timeout = false;
#define MOUSE_TIMEOUT 800  // マウス停止判定時間（ms）
static bool first_report = true;

// レイヤーホールドの状態管理

// モディファイヤーキーの状態管理
static bool modifier_active = false;

// 音声制御の状態管理
static uint16_t last_sound_time = 0;
static bool typewriter_sound_playing = false;
static bool key_sound_enabled = false;
static bool typewriter_sound_enabled = false;
static bool all_sound_enabled = false;  // 全音声機能のON/OFF
#define MIN_SOUND_INTERVAL 30
#define TYPEWRITER_SOUND_DURATION 100

// タイプライター音のメロディ定義
float typewriter_song[][2] = TYPEWRITER_SOUND;

// スタートアップソングのメロディ定義
// float custom_startup_song[][2] = CUSTOM_STARTUP_SONG;

// カスタムキーコード定義
enum custom_keycodes {
    // 速度水準の調整
    SPEED_LEVEL_1_KEY = SAFE_RANGE,
    SPEED_LEVEL_2_KEY,
    SPEED_LEVEL_3_KEY,
    SPEED_LEVEL_4_KEY,
    SPEED_LEVEL_5_KEY,

    // 細かいパラメータ調整
    SPEED_UP_KEY,        // 基本速度アップ
    SPEED_DOWN_KEY,      // 基本速度ダウン
    SMOOTH_UP_KEY,       // スムージング係数アップ
    SMOOTH_DOWN_KEY,     // スムージング係数ダウン
    ACCEL_UP_KEY,        // 加速度アップ
    ACCEL_DOWN_KEY,      // 加速度ダウン
    DECEL_UP_KEY,        // 減速度アップ
    DECEL_DOWN_KEY,      // 減速度ダウン

    // 音声制御（統合）
    ALL_SOUND_TOGGLE,    // 全音声機能ON/OFF
};

// モディファイヤーキーの判定
bool is_modifier_key(uint16_t keycode) {
    switch (keycode) {
        case KC_LCTL:
        case KC_RCTL:
        case KC_LSFT:
        case KC_RSFT:
        case KC_LALT:
        case KC_RALT:
        case KC_LGUI:
        case KC_RGUI:
            return true;
        default:
            return false;
    }
}

// マウス関連キーの判定
bool is_mouse_key(uint16_t keycode) {
    switch (keycode) {
        case KC_MS_BTN1:
        case KC_MS_BTN2:
        case KC_MS_BTN3:
        case KC_MS_UP:
        case KC_MS_DOWN:
        case KC_MS_LEFT:
        case KC_MS_RIGHT:
            return true;
        default:
            return false;
    }
}

// 音声再生関数
void play_key_sound(uint16_t keycode) {
    if (!all_sound_enabled || !key_sound_enabled) return;

    uint16_t current_time = timer_read();

    if (keycode == KC_ENT) {
        // エンターキー: タイプライター音
        if (typewriter_sound_enabled && !typewriter_sound_playing) {
            PLAY_SONG(typewriter_song);
            typewriter_sound_playing = true;
            last_sound_time = current_time;
        }
    } else {
        // その他のキー: ランダム音（間隔制御）
        if (timer_elapsed(last_sound_time) > MIN_SOUND_INTERVAL) {
            float random_freq = 200.0f + (rand() % 800);  // 200-1000Hz
            audio_play_note(random_freq, 50);
            last_sound_time = current_time;
        }
    }
}

// 設定変更のマーク（即座保存せず、遅延保存をスケジュール）
void mark_settings_dirty(void) {
    settings_dirty = true;
    uint16_t current_time = timer_read();

    // 変更頻度の監視
    if (timer_elapsed(last_change_time) < 1000) {  // 1秒以内の変更
        change_count++;
        if (change_count > 5) {  // 5回以上の連続変更で保存間隔を延長
            save_debounce_time = 5000;  // 5秒に延長
        }
    } else {
        change_count = 0;
        save_debounce_time = 2000;  // 通常の2秒に戻す
    }

    last_change_time = current_time;

    if (last_save_time == 0) {
        last_save_time = current_time;
    }
}

// 重要な設定変更のマーク（即座保存をスケジュール）
void mark_critical_settings_dirty(void) {
    critical_settings_changed = true;
    mark_settings_dirty();
}

// 設定の保存（音声設定を含む）- 最適化版
void save_mouse_settings(void) {
    // 4バイトに圧縮された設定データ
    uint32_t compressed_settings = 0;

    // 速度レベル（3ビット）
    compressed_settings |= (current_level & 0x07) << 21;

    // 音声設定（3ビット）
    compressed_settings |= (all_sound_enabled & 0x01) << 20;
    compressed_settings |= (key_sound_enabled & 0x01) << 19;
    compressed_settings |= (typewriter_sound_enabled & 0x01) << 18;

    // マジックナンバーとバージョン（8ビット）
    compressed_settings |= 0xA5;

    eeconfig_update_user_datablock(&compressed_settings, 0, sizeof(compressed_settings));

    save_count++;
    last_save_time = timer_read();
    settings_dirty = false;
}

// 重要な設定変更の即座保存
void save_critical_settings(void) {
    save_mouse_settings();
    critical_settings_changed = false;
}

// 遅延保存のチェックと実行
void check_delayed_save(void) {
    // 重要な設定変更がある場合は即座保存
    if (critical_settings_changed) {
        save_critical_settings();
        return;
    }

    // 通常の遅延保存
    if (settings_dirty && timer_elapsed(last_save_time) > save_debounce_time) {
        save_mouse_settings();
    }
}

// 設定の読み込み（音声設定を含む）- 最適化版
bool load_mouse_settings(void) {
    uint32_t val = 0;
    eeconfig_read_user_datablock(&val, 0, sizeof(val));

    uint8_t magic = val & 0xFF;

    if (magic == 0xA5) {
        // 速度レベル（3ビット）
        current_level = (val >> 21) & 0x07;

        // 音声設定（3ビット）
        all_sound_enabled = (val >> 20) & 0x01;
        key_sound_enabled = (val >> 19) & 0x01;
        typewriter_sound_enabled = (val >> 18) & 0x01;

        if (current_level < SPEED_LEVEL_COUNT) {
            current_profile = speed_profiles[current_level];
            return true;
        }
    }
    return false;
}

// パラメータの調整
void adjust_parameter(int16_t *param, int16_t delta, int16_t min, int16_t max) {
    *param += delta;
    if (*param < min) *param = min;
    if (*param > max) *param = max;
    mark_settings_dirty();  // 即座保存ではなく遅延保存をスケジュール
}

// 速度水準の変更
void change_speed_level(speed_level_t level) {
    if (level < SPEED_LEVEL_COUNT) {
        current_level = level;
        current_profile = speed_profiles[level];
        mark_settings_dirty();  // 即座保存ではなく遅延保存をスケジュール
    }
}

// キーボード初期化時の処理
void keyboard_post_init_user(void) {
    // LED初期化
    rgblight_enable();
    rgblight_mode(RGBLIGHT_MODE_STATIC_LIGHT);
    rgblight_sethsv(0, 0, 13);

    // マウス状態初期化
    trackpoint_timer = timer_read();
    waiting_for_timeout = false;
    first_report = true;

    // マウスレポートの初期化
    report_mouse_t empty_report = {0};
    host_mouse_send(&empty_report);
    wait_ms(50);

    // 設定の読み込み
    if (!load_mouse_settings()) {
        current_level = 2;  // デフォルトはレベル3
        current_profile = speed_profiles[current_level];
        key_sound_enabled = false;      // デフォルトはOFF
        typewriter_sound_enabled = false; // デフォルトはOFF
        all_sound_enabled = false;       // デフォルトはOFF
        save_mouse_settings();
    }
}

// PS2マウスの移動検出時の処理
void ps2_mouse_moved_user(report_mouse_t *mouse_report) {
    static int16_t current_speed = 256;  // 1.0を256として表現
    uint16_t current_time = timer_read();

    // 初回レポート時の初期化処理
    static bool first_report = true;
    if (first_report) {
        first_report = false;
        mouse_report->buttons = 0;
        return;
    }

    // マウス移動時の処理
    if (mouse_report->x != 0 || mouse_report->y != 0) {
        // 移動量の計算
        int16_t movement = sqrtf(mouse_report->x * mouse_report->x +
                               mouse_report->y * mouse_report->y);

        // 目標速度の計算
        int16_t target_speed = current_profile.base_speed +
                             (movement * current_profile.acceleration / 256);

        // 速度の更新
        if (current_speed < target_speed) {
            current_speed += current_profile.acceleration;
            if (current_speed > target_speed) {
                current_speed = target_speed;
            }
        } else if (current_speed > target_speed) {
            current_speed -= current_profile.deceleration;
            if (current_speed < target_speed) {
                current_speed = target_speed;
            }
        }

        // 移動量の適用
        mouse_report->x = (int8_t)((mouse_report->x * current_speed) / 256);
        mouse_report->y = (int8_t)((mouse_report->y * current_speed) / 256);

        // マウス操作レイヤーの有効化（A案: フラグのみ）
        layer3_mouse_active = true;
        trackpoint_timer = current_time;
        waiting_for_timeout = true;
    }
}

// キー入力時の処理
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // VIAで動的に割り当てられたカスタムキーコードの処理
    if (keycode >= SPEED_LEVEL_1_KEY && keycode <= ALL_SOUND_TOGGLE) {
        if (record->event.pressed) {
            // 音声再生（キー押下時）
            play_key_sound(keycode);

            switch (keycode) {
                // 速度水準の変更
                case SPEED_LEVEL_1_KEY:
                case SPEED_LEVEL_2_KEY:
                case SPEED_LEVEL_3_KEY:
                case SPEED_LEVEL_4_KEY:
                case SPEED_LEVEL_5_KEY:
                    change_speed_level(keycode - SPEED_LEVEL_1_KEY);
                    return false;

                // パラメータの微調整
                case SPEED_UP_KEY:
                    adjust_parameter(&current_profile.base_speed, 32, 64, 1024);
                    return false;
                case SPEED_DOWN_KEY:
                    adjust_parameter(&current_profile.base_speed, -32, 64, 1024);
                    return false;
                case SMOOTH_UP_KEY:
                    adjust_parameter(&current_profile.smoothing, 16, 32, 256);
                    return false;
                case SMOOTH_DOWN_KEY:
                    adjust_parameter(&current_profile.smoothing, -16, 32, 256);
                    return false;
                case ACCEL_UP_KEY:
                    adjust_parameter(&current_profile.acceleration, 16, 16, 256);
                    return false;
                case ACCEL_DOWN_KEY:
                    adjust_parameter(&current_profile.acceleration, -16, 16, 256);
                    return false;
                case DECEL_UP_KEY:
                    adjust_parameter(&current_profile.deceleration, 16, 32, 512);
                    return false;
                case DECEL_DOWN_KEY:
                    adjust_parameter(&current_profile.deceleration, -16, 32, 512);
                    return false;

                // 音声制御（統合）
                case ALL_SOUND_TOGGLE:
                    all_sound_enabled = !all_sound_enabled;
                    key_sound_enabled = all_sound_enabled;
                    typewriter_sound_enabled = all_sound_enabled;

                    // LEDフィードバック（2回点滅、点灯時間短縮）
                    for (int i = 0; i < 2; i++) {
                        if (all_sound_enabled) {
                            // ON: 緑色で点灯
                            rgblight_sethsv(85, 255, 128);
                        } else {
                            // OFF: 赤色で点灯
                            rgblight_sethsv(0, 255, 128);
                        }
                        wait_ms(60);
                        // 消灯
                        rgblight_sethsv(0, 0, 0);
                        wait_ms(40);
                    }
                    // 元のLED状態に戻す
                    layer_state_set_user(layer_state);

                    mark_critical_settings_dirty();  // 音声設定は重要な設定として即座保存
                    return false;
            }
        }
        return false; // カスタムキーコードは処理済み
    }

    if (record->event.pressed) {
        // 音声再生（キー押下時）
        play_key_sound(keycode);

        // モディファイヤーキーの状態を更新
        if (is_modifier_key(keycode)) {
            modifier_active = true;
        }

        // マウスボタンの状態を更新
        if (keycode == KC_MS_BTN1 || keycode == KC_MS_BTN2 || keycode == KC_MS_BTN3) {
            layer3_mousebtn_active = true;
        }

        // 標準レイヤー切り替えキー（MO(3)等）を押した場合
        if (is_layer3_standard_key(keycode)) {
            layer3_standard_active = true;
        }
    } else {
        // キーリリース時の処理
        if (is_modifier_key(keycode)) {
            modifier_active = false;
        }
        if (keycode == KC_MS_BTN1 || keycode == KC_MS_BTN2 || keycode == KC_MS_BTN3) {
            layer3_mousebtn_active = false;
        }
        if (keycode == KC_ENT) {
            typewriter_sound_playing = false;
        }

        // 標準レイヤー切り替えキー（MO(3)等）をリリースした場合
        if (is_layer3_standard_key(keycode)) {
            layer3_standard_active = false;
        }

        // すべてのアクティブ状態が解除された時にタイムアウトチェックを再開
        if (!layer3_mousebtn_active && !modifier_active && trackpoint_timer) {
            trackpoint_timer = timer_read();
            waiting_for_timeout = true;
        }
    }
    return true;
}

// レイヤー状態変更時のLED制御
layer_state_t layer_state_set_user(layer_state_t state) {
    switch (get_highest_layer(state)) {
        case 0:
            rgblight_sethsv(0, 0, 13); // 白
            break;
        case 1:
            rgblight_sethsv(120, 255, 26); // 水色
            break;
        case 2:
            rgblight_sethsv(85, 255, 26);  // 緑
            break;
        case 3:
            if (layer3_mouse_active || layer3_mousebtn_active || modifier_active || layer3_standard_active) {
                // 速度水準に応じた色（見分けやすい色に変更）
                uint8_t hue;
                switch (current_level) {
                    case 0: // レベル1: 最も遅い - 青色
                        hue = 170;
                        break;
                    case 1: // レベル2: やや遅め - 水色
                        hue = 120;
                        break;
                    case 2: // レベル3: 標準 - 緑色
                        hue = 85;
                        break;
                    case 3: // レベル4: やや速め - 黄色
                        hue = 43;
                        break;
                    case 4: // レベル5: 最も速い - 赤色
                        hue = 0;
                        break;
                    default:
                        hue = 85; // デフォルトは緑色
                        break;
                }
                rgblight_sethsv(hue, 255, 38);
            } else {
                rgblight_sethsv(0, 0, 13);
            }
            break;
    }
    return state;
}

// --- matrix_scan_userの修正版 ---
void matrix_scan_user(void) {
    check_delayed_save();

    // マウス停止タイマーによるON要因解除（1箇所のみ）
    if (waiting_for_timeout && timer_elapsed(trackpoint_timer) > MOUSE_TIMEOUT) {
        layer3_mouse_active = false;
        waiting_for_timeout = false;
    }

    // いずれかのON要因があればレイヤー3 ON、全てOFFならOFF
    // モディファイヤーキーは、マウス操作中またはレイヤー3が既にアクティブな場合のみ有効
    bool mouse_or_layer3_active = layer3_mouse_active || layer3_mousebtn_active || layer3_standard_active;
    bool modifier_should_activate = modifier_active && (mouse_or_layer3_active || layer_state_is(3));
    bool any_layer3 = layer3_mouse_active || layer3_mousebtn_active || modifier_should_activate || layer3_standard_active;
    if (any_layer3) {
        layer_on(3);
    } else {
        layer_off(3);
    }
}

// キーマップ定義
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
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
        KC_LSFT,  KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,        KC_NUM,  KC_M,    KC_QUOT, KC_DOT,  KC_SLSH,  KC_PGUP,    KC_RSFT,
        KC_LCTL,  KC_LGUI, KC_LALT,          KC_SPC,        MO(1),          KC_DEL,           MO(2),   KC_HOME,  KC_PGDN,  KC_END, KC_DOWN
    ),
    [2] = LAYOUT(
        EE_CLR,   SPEED_LEVEL_1_KEY, SPEED_LEVEL_2_KEY, SPEED_LEVEL_3_KEY, SPEED_LEVEL_4_KEY, SPEED_LEVEL_5_KEY,   KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    QK_RBT,
        KC_TAB,   SPEED_UP_KEY, SPEED_DOWN_KEY, SMOOTH_UP_KEY, SMOOTH_DOWN_KEY, KC_T,        KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,     KC_LBRC,  KC_BSLS,
        KC_LCTL,  ACCEL_UP_KEY, ACCEL_DOWN_KEY, DECEL_UP_KEY, DECEL_DOWN_KEY, KC_G,        KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_ENT,
        KC_LSFT,  ALL_SOUND_TOGGLE, KC_X,    KC_C,    KC_V,    KC_B,        KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,  KC_UP,    KC_RSFT,
        KC_LCTL,  KC_LGUI, KC_LALT,          KC_SPC,        MO(1),          KC_DEL,           MO(2),   KC_LEFT,  KC_DOWN,  KC_RGHT, KC_DOWN
    ),
    [3] = LAYOUT(
        KC_ESC,   KC_1,    KC_2,    KC_3,    KC_4,    KC_5,        KC_6,    KC_7,       KC_8,       KC_9,       KC_0,       KC_BSPC,
        KC_TAB,   KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,        KC_Y,    KC_U,       KC_I,       KC_O,       KC_P,       KC_LBRC,  KC_BSLS,
        KC_LCTL,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G,        KC_H,    KC_J,       KC_K,       KC_L,       KC_SCLN,    KC_ENT,
        KC_LSFT,  KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,        KC_N,    KC_M,       KC_COMM,    KC_DOT,     KC_SLSH,    KC_UP,    KC_RSFT,
        KC_LCTL,  MO(3),   KC_LALT,          KC_MS_BTN1,   KC_MS_BTN3,      KC_MS_BTN2,             MO(1),      KC_LEFT,    KC_DOWN,  KC_RGHT, KC_DOWN
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

bool is_layer3_standard_key(uint16_t keycode) {
    // MO(3), TG(3), TO(3) などの標準レイヤー切り替えキーを判定
    // QMKの内部キーコード範囲で判定
    if (keycode >= QK_MOMENTARY && keycode <= QK_MOMENTARY_MAX) {
        // MO(n)の場合、レイヤー番号を取得
        uint8_t layer = keycode - QK_MOMENTARY;
        return (layer == 3);
    }
    if (keycode >= QK_TO && keycode <= QK_TO_MAX) {
        // TO(n)の場合
        uint8_t layer = keycode - QK_TO;
        return (layer == 3);
    }
    if (keycode >= QK_TOGGLE_LAYER && keycode <= QK_TOGGLE_LAYER_MAX) {
        // TG(n)の場合
        uint8_t layer = keycode - QK_TOGGLE_LAYER;
        return (layer == 3);
    }
    if (keycode >= QK_LAYER_TAP && keycode <= QK_LAYER_TAP_MAX) {
        // LT(n, k)の場合
        uint8_t layer = (keycode - QK_LAYER_TAP) >> 8;
        return (layer == 3);
    }
    return false;
}
