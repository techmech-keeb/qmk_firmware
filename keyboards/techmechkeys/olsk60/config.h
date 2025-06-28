// Copyright 2022 techmech-keeb (@techmech-keeb)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// RP2040・ハードウェア固有の設定
#define PICO_XOSC_STARTUP_DELAY_MULTIPLIER 64

// 起動時の安定化設定
#define QMK_STARTUP_DELAY 500

// PS2マウスの基本設定
#define PS2_MOUSE_USE_REMOTE_MODE
#define PS2_MOUSE_INIT_DELAY 1000
#define PS2_MOUSE_RESET_DELAY 100

// マウス加速の基本パラメータ
#define MOUSEKEY_INTERVAL 16      // マウス移動の更新間隔（ms）
#define MOUSEKEY_DELAY 0          // マウスキー開始までの遅延（ms）
#define MOUSEKEY_TIME_TO_MAX 40   // 最大速度までの時間（ms）
#define MOUSEKEY_MAX_SPEED 7      // 最大速度
#define MOUSEKEY_WHEEL_DELAY 0    // ホイール開始までの遅延（ms）

// スクロール設定
#define PS2_MOUSE_SCROLL_DIVISOR_V 4
#define PS2_MOUSE_SCROLL_DIVISOR_H 4

// RGBライトの設定
#define RGBLIGHT_LAYERS
#define RGBLIGHT_LAYERS_OVERRIDE_RGB_OFF
#define RGBLIGHT_DEFAULT_MODE RGBLIGHT_MODE_STATIC_LIGHT

// レイヤー制御の設定
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

/* Audio support */
#ifdef AUDIO_ENABLE
  #define AUDIO_PIN GP28
  #define AUDIO_PWM_DRIVER PWMD6
  #define AUDIO_PWM_CHANNEL RP2040_PWM_CHANNEL_A
  #define AUDIO_INIT_DELAY
  // スタートアップソングは実行時に制御するため、ここでは無効化
  // #define STARTUP_SONG CUSTOM_STARTUP_SONG
#endif

// QMK標準の起動音を無効化
#define STARTUP_SOUND NO_SOUND


