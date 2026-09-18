// Copyright 2024 keyball-custom contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// USB HID Country Code: 15 = Japan (JIS)
// macOSがKeyballをJIS配列キーボードとして自動認識するために必要
// これによりユーザーが手動でplistを編集する必要がなくなる
#define USB_HID_KEYBOARD_COUNTRY_CODE 15

// Dynamic keymapのレイヤー数。
// RP2040は大容量フラッシュで容量制約がないため8に増やす（本人決定）。
// AVR版(keyball-link-firmware)は4のまま据え置き。
#define DYNAMIC_KEYMAP_LAYER_COUNT 8

// トラックボールのデフォルトCPI
#define KEYBALL_CPI_DEFAULT 500

// スクロール量の全体基準を下げる（2=従来の半分の速さ）
// 別マウスと併用しPC側のスクロール量を上げても速くなりすぎないように
#define KEYBALL_SCROLL_DIV_BASE 2

// 高解像度スクロールはmacOSで意図せず高速化したため一旦無効化
// #define WHEEL_EXTENDED_REPORT
// #define POINTING_DEVICE_HIRES_SCROLL_ENABLE
// #define POINTING_DEVICE_HIRES_SCROLL_MULTIPLIER 120
// #define POINTING_DEVICE_HIRES_SCROLL_EXPONENT 0

// 自動マウスレイヤー
#define POINTING_DEVICE_AUTO_MOUSE_ENABLE
#define AUTO_MOUSE_DEFAULT_LAYER 1
// 発動しきい値（移動量）をランタイム変更可能にする
#define KEYBALL_AML_THRESHOLD_RUNTIME

#define TAP_CODE_DELAY 5

// OLED常時点灯（既定は無操作60秒でオフになる仕様のため、タイムアウトを無効化）
#define OLED_TIMEOUT 0

// 詳細設定: per-key オーバーライド（LED版では容量確保のため簡略化）
#define TAPPING_TERM           200
#define TAPPING_TERM_PER_KEY
#define PERMISSIVE_HOLD_PER_KEY

// OS自動判別（rules.mk の OS_DETECTION_ENABLE と対）。
// DEBOUNCE: 判定を250ms安定待ちしてから確定する（既定値のため実質no-op）。
// SPLIT_DETECTED_OS_ENABLE: 分割の左右で判定結果を共有する（将来のOLED表示用）。
//
// 2026-09-11、OS_DETECTION_KEYBOARD_RESETを削除した。この定義はKVM切替器で
// USBが再構成されない問題への対処用で、「一度CONFIGURED状態になった後USBが
// INIT状態まで下がったら本体をソフトリセットする」という動作をする
// （quantum/os_detection.cのos_detection_notify_usb_device_state_change/
// os_detection_task参照）。KVMを使わない通常のPC直結でも、USBホスト側の
// 列挙シーケンス次第ではCONFIGURED→INITの一時的な遷移が起こり得て、その
// たびに本体が丸ごとリセットされてしまう。起動に時間がかかる・起動完了までOLEDに
// ノイズが出る、という症状はこの余計なリセットが原因だった可能性が高いため、
// KVMを使わないこの用途では不要と判断し削除した。
#define OS_DETECTION_DEBOUNCE 250
#define SPLIT_DETECTED_OS_ENABLE

// ===== RGBLIGHT =====
// LED総数を実際のハードウェア構成に合わせて上書き
// （トラックボール側26 + 非搭載側29 = 55個。Keyball+は左右非対称）
//
// 2026-09-18: このキーマップ（web_configurator）は「ボール搭載側 = is_keyboard_left()
// が真になる基板」で組み立てたユニット（ボール右手・デフォルト構成）専用。
// この前提が成立しない「ボール左手」ユニット向けには、g_led_config/keymaps[]は
// 一切変更せずis_keyboard_left()の判定結果だけを反転させる別キーマップ
// web_configurator_leftballを用意した（詳細はそちらのkeymap.cのコメント参照）。
// 両ユニットが同じg_led_config/keymaps[]/RGB_MATRIX_SPLITを共有できるよう、
// このファイルの値は変更しないこと。
#undef RGBLIGHT_LED_COUNT
#define RGBLIGHT_LED_COUNT 55
#undef RGBLED_SPLIT
#define RGBLED_SPLIT { 26, 29 }

// エフェクトのレパートリー。RP2040は容量制約がないため主要なものを一通り有効化。
#define RGBLIGHT_EFFECT_BREATHING
#define RGBLIGHT_EFFECT_RAINBOW_MOOD
#define RGBLIGHT_EFFECT_RAINBOW_SWIRL
#define RGBLIGHT_EFFECT_SNAKE
#define RGBLIGHT_EFFECT_KNIGHT
#define RGBLIGHT_EFFECT_CHRISTMAS
#define RGBLIGHT_EFFECT_STATIC_GRADIENT
#define RGBLIGHT_EFFECT_TWINKLE
#define RGBLIGHT_EFFECT_ALTERNATING

// デフォルト設定
#define RGBLIGHT_DEFAULT_MODE RGBLIGHT_MODE_BREATHING
#define RGBLIGHT_DEFAULT_HUE  170
#define RGBLIGHT_DEFAULT_SAT  255
#define RGBLIGHT_DEFAULT_VAL  100

// 最大輝度（消費電力抑制）
#undef RGBLIGHT_LIMIT_VAL
#define RGBLIGHT_LIMIT_VAL 150

// ===== RGB_MATRIX（波紋演出のためRGBLIGHTから移行中。現在はLED物理位置の実測用暫定ビルド） =====
// LED総数を実際のハードウェア構成に合わせて上書き（RGBLIGHTと同じく26+29=55個）
#undef RGB_MATRIX_LED_COUNT
#define RGB_MATRIX_LED_COUNT 55
#undef RGB_MATRIX_SPLIT
#define RGB_MATRIX_SPLIT { 26, 29 }
// 自作エフェクト（LED_TEST診断・REACTIVE_KEYS）を有効化する
#define RGB_MATRIX_CUSTOM_USER
// キー入力に反応するエフェクト（REACTIVE_KEYS）に必要
#define RGB_MATRIX_KEYPRESSES
// kb_hid.cのLED_EFFECT_MAPが参照する組み込みエフェクト。
// RGBLIGHT版の呼吸・レインボー・スワール・グラデーションに対応する組み込み
// エフェクトを有効化（スネーク・ナイトライダー・クリスマス・交互点灯・きらめき・
// ハロウィン・イースターは、要望に合う組み込みが無いためrgb_matrix_user.incで
// 自作している）。
#define ENABLE_RGB_MATRIX_BREATHING
#define ENABLE_RGB_MATRIX_CYCLE_ALL
#define ENABLE_RGB_MATRIX_CYCLE_SPIRAL
#define ENABLE_RGB_MATRIX_GRADIENT_UP_DOWN
// タイピングヒートマップは組み込み(TYPING_HEATMAP)ではなくrgb_matrix_user.incの
// 自作HEATMAPを使う（組み込み版は隣接キーへの熱伝播や色相固定が要望に合わなかった
// ため）。よってRGB_MATRIX_FRAMEBUFFER_EFFECTSは不要。
// 起動時のモードはRGB_MATRIX_DEFAULT_MODEではなくkeyball_apply_normal_led()が
// kb_led_config（EEPROM保存の独自管理領域）から反映するため、ここでは指定しない。
// kb_led_configが未初期化(0xFF)の場合はkb_settings.cの既定値（呼吸相当のeffect_id=2）
// が使われる。
// 最大輝度（消費電力抑制、RGBLIGHT側と揃える）
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 150
