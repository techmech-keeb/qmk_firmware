// Copyright 2022 techmech-keeb (@techmech-keeb)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "config_common.h"

// RP2040・ハードウェア固有の設定
#define PICO_XOSC_STARTUP_DELAY_MULTIPLIER 64

// 起動時の安定化設定
#define QMK_STARTUP_DELAY 500

// PS2マウスの基本設定
#define PS2_MOUSE_USE_REMOTE_MODE
#define PS2_MOUSE_INIT_DELAY 1000
#define PS2_MOUSE_RESET_DELAY 100

// マウス加速の基本パラメータ
#define MOUSEKEY_INTERVAL 8      // マウス移動の更新間隔（ms）

// スクロール設定
#define PS2_MOUSE_SCROLL_DIVISOR_V 4
#define PS2_MOUSE_SCROLL_DIVISOR_H 4

// RGBライトの設定
#define RGBLIGHT_LAYERS
#define RGBLIGHT_LAYERS_OVERRIDE_RGB_OFF
#define RGBLIGHT_DEFAULT_MODE RGBLIGHT_MODE_STATIC_LIGHT

// レイヤー制御の設定ｓ
#define LAYER_STATE_8BIT  // 8ビットレイヤー制御を使用

// デバウンス設定
#define DEBOUNCE 10  // デバウンス時間（ms）

// VIA設定
#define VIA_EEPROM_LAYOUT_OPTIONS_SIZE 2
#define VIA_EEPROM_CUSTOM_CONFIG_SIZE 32
#define DYNAMIC_KEYMAP_LAYER_COUNT 4
#define DYNAMIC_KEYMAP_MACRO_COUNT 32

// カスタムキーコードの設定
#define VIA_CUSTOM_KEYCODE_RANGE 0x5F80
#define VIA_CUSTOM_KEYCODE_START 0x5F80
#define VIA_CUSTOM_KEYCODE_END 0x5F8F

// クリッキー機能の設定
#define AUDIO_PIN GP28
#define AUDIO_CLICKY
#define AUDIO_CLICKY_FREQ_RANDOMNESS 0.3f
#define AUDIO_CLICKY_FREQ_DEFAULT 440.0f
#define AUDIO_CLICKY_FREQ_MIN 65.0f
#define AUDIO_CLICKY_FREQ_MAX 1500.0f
#define AUDIO_CLICKY_FREQ_FACTOR 0.5f
#define AUDIO_CLICKY_DELAY_DURATION 0.1f




