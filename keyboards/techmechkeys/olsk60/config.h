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




