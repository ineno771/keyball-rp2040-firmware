// Copyright 2024 keyball-custom contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "kb_hid.h"
#include "keyball.h"
#include "raw_hid.h"
#include "dynamic_keymap.h"
#include "eeconfig.h"
#ifdef TAP_DANCE_ENABLE
#    include "td_config.h"
#endif
#include "kb_settings.h"
#ifdef COMBO_ENABLE
#    include "kb_combo.h"
#endif
#ifdef OS_DETECTION_ENABLE
#    include "os_detection.h"
// keymap.c 側で実体定義。OS判別結果と KB_FLAG_OS_AUTO_SWAP から Cmd/Ctrl 入れ替えを反映する。
void kb_apply_os_swap(os_variant_t os);
#endif
#ifndef LED_VERSION_BUILD
#include "kb_macro.h"
#endif
#include "kb_version.h"
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
#    include "pointing_device_auto_mouse.h"
#endif
#ifdef KEYBALL_AML_THRESHOLD_RUNTIME
extern uint8_t kb_aml_threshold;
#endif

#define RAW_PACKET_SIZE 32

// GET_INFO レスポンス用のモデル番号変換
// KEYBALL_MODEL は 39/44/61 の数値で定義されている
static uint8_t get_model_id(void) {
    return (uint8_t)KEYBALL_MODEL;
}

#if defined(RGBLIGHT_ENABLE) || defined(RGB_MATRIX_ENABLE)
// 季節限定エフェクトかどうかの判定はバックエンドに依存しないIDだけの概念なので
// 両対応で共有する（実装はkb_hid.h参照）。
bool kb_hid_led_effect_is_seasonal(uint8_t effect_id) {
    return effect_id == KB_LED_EFFECT_HALLOWEEN || effect_id == KB_LED_EFFECT_EASTER;
}
#endif

#if defined(RGBLIGHT_ENABLE) && !defined(RGB_MATRIX_ENABLE)
// エフェクトID対応表（Web側のID⇔QMKのRGBLIGHT_MODE_*）。
// GET/SET_LEDだけでなく、レイヤー連動LED（keyball.c）からも参照する唯一の変換元。
#define LED_EFFECT_COUNT 11
static const uint8_t LED_EFFECT_MAP[LED_EFFECT_COUNT] = {
    0,                             //  0: オフ
    RGBLIGHT_MODE_STATIC_LIGHT,    //  1: 単色
    RGBLIGHT_MODE_BREATHING,       //  2: 呼吸
    RGBLIGHT_MODE_RAINBOW_MOOD,    //  3: レインボー
    RGBLIGHT_MODE_RAINBOW_SWIRL,   //  4: スワール
    RGBLIGHT_MODE_SNAKE,           //  5: スネーク
    RGBLIGHT_MODE_KNIGHT,          //  6: ナイトライダー
    RGBLIGHT_MODE_CHRISTMAS,       //  7: クリスマス
    RGBLIGHT_MODE_STATIC_GRADIENT, //  8: グラデーション
    RGBLIGHT_MODE_TWINKLE,         //  9: きらめき
    RGBLIGHT_MODE_ALTERNATING,     // 10: 交互点灯（本体そのまま。左右ハーフ単位への対応は一旦見送り）
};

uint8_t kb_hid_led_effect_to_mode(uint8_t effect_id) {
    if (effect_id >= LED_EFFECT_COUNT) effect_id = 0;
    return LED_EFFECT_MAP[effect_id];
}

uint8_t kb_hid_led_effect_count(void) {
    return LED_EFFECT_COUNT;
}
#endif

#ifdef RGB_MATRIX_ENABLE
// エフェクトID対応表（Web側のID⇔QMKのRGB_MATRIX_*）。RGBLIGHT版と同じ0-10のID体系に
// 揃えてあるため、Web UI（keyball-configurator）のLED_EFFECTS一覧をそのまま流用できる。
// GET/SET_LEDだけでなく、レイヤー連動LED（keyball.c）からも参照する唯一の変換元。
#define RGB_MATRIX_LED_EFFECT_COUNT 11
// 以下はRGB_MATRIX限定の追加エフェクト。RGBLIGHT版と共有のこの表には含めない
#define LED_EFFECT_ID_REACTIVE_KEYS   14
#define LED_EFFECT_ID_TYPING_HEATMAP  15
#define LED_EFFECT_ID_TRACKBALL       16
#define LED_EFFECT_ID_RIPPLE          17
// GESTURE_ENABLEのあるファーム（複数ジェスチャーモード対応）でのみ意味を持つ。
// GESTURE_ENABLE無しのビルドでもID自体は予約しておき、他エフェクトとの番号衝突を防ぐ。
#define LED_EFFECT_ID_GESTURE_WAVE    18
#ifdef RGB_MATRIX_CUSTOM_USER
static const uint8_t RGB_MATRIX_LED_EFFECT_MAP[RGB_MATRIX_LED_EFFECT_COUNT] = {
    RGB_MATRIX_NONE,               //  0: オフ
    RGB_MATRIX_SOLID_COLOR,        //  1: 単色
    RGB_MATRIX_BREATHING,          //  2: 呼吸
    RGB_MATRIX_CYCLE_ALL,          //  3: レインボー
    RGB_MATRIX_CYCLE_SPIRAL,       //  4: スワール
    RGB_MATRIX_CUSTOM_SNAKE,       //  5: スネーク
    RGB_MATRIX_CUSTOM_KNIGHT,      //  6: ナイトライダー
    RGB_MATRIX_CUSTOM_CHRISTMAS,   //  7: クリスマス
    RGB_MATRIX_GRADIENT_UP_DOWN,   //  8: グラデーション
    RGB_MATRIX_CUSTOM_TWINKLE,     //  9: きらめき
    RGB_MATRIX_CUSTOM_ALTERNATING, // 10: 交互点灯
};
#else
static const uint8_t RGB_MATRIX_LED_EFFECT_MAP[RGB_MATRIX_LED_EFFECT_COUNT] = {
    RGB_MATRIX_NONE, RGB_MATRIX_SOLID_COLOR,  RGB_MATRIX_BREATHING,     RGB_MATRIX_CYCLE_ALL,
    RGB_MATRIX_CYCLE_SPIRAL,     RGB_MATRIX_BREATHING, RGB_MATRIX_BREATHING, RGB_MATRIX_BREATHING,
    RGB_MATRIX_GRADIENT_UP_DOWN, RGB_MATRIX_BREATHING, RGB_MATRIX_BREATHING,
};
#endif

uint8_t kb_hid_led_effect_to_rgb_matrix_mode(uint8_t effect_id) {
#ifdef RGB_MATRIX_CUSTOM_USER
    // ハロウィン・イースターはクリスマスと同じ「市松模様に交互点灯」する自作エフェクトを
    // 使う（RGBLIGHT版の単色クロスフェードとは動きを変えている。本人希望）。
    if (effect_id == KB_LED_EFFECT_HALLOWEEN) return RGB_MATRIX_CUSTOM_HALLOWEEN;
    if (effect_id == KB_LED_EFFECT_EASTER) return RGB_MATRIX_CUSTOM_EASTER;
    if (effect_id == LED_EFFECT_ID_REACTIVE_KEYS) return RGB_MATRIX_CUSTOM_REACTIVE_KEYS;
    // タイピングヒートマップは組み込みのRGB_MATRIX_TYPING_HEATMAPではなく自作の
    // RGB_MATRIX_CUSTOM_HEATMAPを使う（組み込み版は隣接キーへの熱の伝播があり
    // 押していないキーも反応する上、色相がユーザー設定を無視して固定される仕様のため）。
    if (effect_id == LED_EFFECT_ID_TYPING_HEATMAP) return RGB_MATRIX_CUSTOM_HEATMAP;
    if (effect_id == LED_EFFECT_ID_TRACKBALL) return RGB_MATRIX_CUSTOM_TRACKBALL;
    if (effect_id == LED_EFFECT_ID_RIPPLE) return RGB_MATRIX_CUSTOM_RIPPLE;
#ifdef GESTURE_ENABLE
    if (effect_id == LED_EFFECT_ID_GESTURE_WAVE) return RGB_MATRIX_CUSTOM_GESTURE_WAVE;
#endif
#endif
    if (effect_id >= RGB_MATRIX_LED_EFFECT_COUNT) effect_id = 0;
    return RGB_MATRIX_LED_EFFECT_MAP[effect_id];
}

void kb_led_config_sync_from_rgb_matrix(void) {
    // レイヤー連動LEDのオーバーライド表示中は、色相・彩度・明るさ・速度も含めて画面に
    // 出ている値が全て「通常」のものではない（apply_layer_led_now()がそのレイヤーの
    // 色をrgb_matrix本体へ直接書き込むため）。ここで読み戻すとkb_led_configが上書きの
    // 色で汚染されてしまうので、その場合は何もしない（一時的な変更は、オーバーライドが
    // 終わった時点で元の通常設定に戻る仕様のまま＝変更は捨てられる）。
    if (keyball_layer_led_overriding()) return;

    kb_led_config_t cfg = kb_led_config_get();

    if (!rgb_matrix_is_enabled()) {
        cfg.effect_id = 0;
        kb_led_config_set(&cfg);
        return;
    }

    bool sync_mode = true;
#ifdef GESTURE_ENABLE
    // ジェスチャーウェーブのオーバーライド中は、keyball_gesture_wave_task()が現在の
    // モード(effect_id)だけを強制的にウェーブ自身へ切り替えている（色相・彩度・明るさ・
    // 速度には一切触れない。keyball.c参照）。そのためモードだけは「ウェーブ」という
    // 一時的な値になっており同期できないが、色相・彩度・明るさ・速度は「通常」の値の
    // ままなので、このオーバーライド中でも問題なく同期してよい（2026-09-11発覚：
    // ここを他のフィールドまで丸ごとスキップしていたため、ジェスチャーウェーブが
    // 頻繁に発火する状況ではSat等の変更がしばしば同期されずに消えてしまっていた）。
    if (keyball_gesture_wave_overriding()) sync_mode = false;
#endif

    if (sync_mode) {
        uint8_t mode      = rgb_matrix_get_mode();
        uint8_t effect_id = cfg.effect_id;  // 該当するIDが見つからない時は変更しない
        for (uint8_t i = 0; i < RGB_MATRIX_LED_EFFECT_COUNT; i++) {
            if (RGB_MATRIX_LED_EFFECT_MAP[i] == mode) {
                effect_id = i;
                break;
            }
        }
#ifdef RGB_MATRIX_CUSTOM_USER
        if (mode == RGB_MATRIX_CUSTOM_HALLOWEEN) {
            effect_id = KB_LED_EFFECT_HALLOWEEN;
        } else if (mode == RGB_MATRIX_CUSTOM_EASTER) {
            effect_id = KB_LED_EFFECT_EASTER;
        } else if (mode == RGB_MATRIX_CUSTOM_REACTIVE_KEYS) {
            effect_id = LED_EFFECT_ID_REACTIVE_KEYS;
        } else if (mode == RGB_MATRIX_CUSTOM_HEATMAP) {
            effect_id = LED_EFFECT_ID_TYPING_HEATMAP;
        } else if (mode == RGB_MATRIX_CUSTOM_TRACKBALL) {
            effect_id = LED_EFFECT_ID_TRACKBALL;
        } else if (mode == RGB_MATRIX_CUSTOM_RIPPLE) {
            effect_id = LED_EFFECT_ID_RIPPLE;
        }
#ifdef GESTURE_ENABLE
        else if (mode == RGB_MATRIX_CUSTOM_GESTURE_WAVE) {
            effect_id = LED_EFFECT_ID_GESTURE_WAVE;
        }
#endif
#endif
        cfg.effect_id = effect_id;
    }
    cfg.hue   = rgb_matrix_get_hue();
    cfg.sat   = rgb_matrix_get_sat();
    cfg.val   = rgb_matrix_get_val();
    cfg.speed = rgb_matrix_get_speed();

    kb_led_config_set(&cfg);
}
#endif

void kb_hid_receive(uint8_t *data, uint8_t length) {
    uint8_t response[RAW_PACKET_SIZE] = {0};
    uint8_t cmd = data[0];
    response[0] = cmd;

    switch (cmd) {

        // 0x01: キーボード情報を返す
        // 応答: [cmd, model, layers, rows, cols, protocol_ver, status]
        case KB_HID_CMD_GET_INFO: {
            response[1] = get_model_id();
            response[2] = dynamic_keymap_get_layer_count();
            response[3] = MATRIX_ROWS;
            response[4] = MATRIX_COLS;
            response[5] = KB_HID_PROTOCOL_VERSION;
            response[6] = KB_HID_STATUS_OK;
            break;
        }

        // 0x02: 指定位置のキーコードを返す
        // 要求: [cmd, layer, row, col]
        // 応答: [cmd, layer, row, col, keycode_hi, keycode_lo, status]
        case KB_HID_CMD_GET_KEYCODE: {
            uint8_t  layer   = data[1];
            uint8_t  row     = data[2];
            uint8_t  col     = data[3];
            uint16_t keycode = dynamic_keymap_get_keycode(layer, row, col);
            response[1] = layer;
            response[2] = row;
            response[3] = col;
            response[4] = (keycode >> 8) & 0xFF;
            response[5] = keycode & 0xFF;
            response[6] = KB_HID_STATUS_OK;
            break;
        }

        // 0x03: 指定位置にキーコードを書き込む
        // 要求: [cmd, layer, row, col, keycode_hi, keycode_lo]
        // 応答: [cmd, status]
        case KB_HID_CMD_SET_KEYCODE: {
            uint8_t  layer   = data[1];
            uint8_t  row     = data[2];
            uint8_t  col     = data[3];
            uint16_t keycode = ((uint16_t)data[4] << 8) | data[5];
            dynamic_keymap_set_keycode(layer, row, col, keycode);
            response[1] = KB_HID_STATUS_OK;
            break;
        }

        // 0x04: トラックボール設定を返す
        // 応答: [cmd, cpi_index, scroll_div, scrollsnap_mode, status]
        case KB_HID_CMD_GET_TRACKBALL: {
            keyball_config_t config = {.raw = eeconfig_read_kb()};
            response[1] = config.cpi;
            response[2] = config.sdiv;
            response[3] = (uint8_t)keyball_get_scrollsnap_mode();
            response[4] = KB_HID_STATUS_OK;
            break;
        }

        // 0x05: トラックボール設定を変更する
        // 要求: [cmd, cpi_index, scroll_div, scrollsnap_mode]
        // 応答: [cmd, status]
        case KB_HID_CMD_SET_TRACKBALL: {
            uint8_t cpi_idx = data[1];
            uint8_t sdiv    = data[2];
            uint8_t ssnap   = data[3];
            keyball_config_t config = {.raw = eeconfig_read_kb()};
            config.cpi  = cpi_idx;
            config.sdiv = sdiv;
#if KEYBALL_SCROLLSNAP_ENABLE == 2
            if (ssnap <= 2) config.ssnap = ssnap;
#endif
            eeconfig_update_kb(config.raw);
            keyball_set_cpi(cpi_idx);
            keyball_set_scroll_div(sdiv);
            if (ssnap <= 2) keyball_set_scrollsnap_mode((keyball_scrollsnap_mode_t)ssnap);
            response[1] = KB_HID_STATUS_OK;
            break;
        }

        // 0x06: EEPROMに保存（dynamic_keymapは自動保存なのでflushed通知のみ）
        // 応答: [cmd, status]
        case KB_HID_CMD_SAVE: {
            eeconfig_update_kb(eeconfig_read_kb());
            response[1] = KB_HID_STATUS_OK;
            break;
        }

        // 0x08: キーマップをデフォルト（コンパイル済み）に戻す
        // 応答: [cmd, status]
        case KB_HID_CMD_RESET_KEYMAP: {
            dynamic_keymap_reset();
            response[1] = KB_HID_STATUS_OK;
            break;
        }

        // 0x09: アクセラレーション設定を返す
        // 応答: [cmd, accel, status]
        case KB_HID_CMD_GET_ACCEL: {
            response[1] = keyball_get_accel();
            response[2] = KB_HID_STATUS_OK;
            break;
        }

        // 0x0A: アクセラレーション設定を変更してEEPROMに保存する
        // 要求: [cmd, accel]
        // 応答: [cmd, status]
        case KB_HID_CMD_SET_ACCEL: {
            uint8_t accel = data[1];
            keyball_set_accel(accel);
            keyball_config_t config = {.raw = eeconfig_read_kb()};
            config.accel = keyball_get_accel();
            eeconfig_update_kb(config.raw);
            response[1] = KB_HID_STATUS_OK;
            break;
        }

#if defined(RGB_MATRIX_ENABLE) || defined(RGBLIGHT_ENABLE)
        // エフェクトID対応表はファイル先頭のLED_EFFECT_MAP/RGB_MATRIX_LED_EFFECT_MAP
        // （file-scope）を使う。レイヤー連動LED（0x1A-0x1D）からも同じ表を参照するため。
        // RGBLIGHT版・RGB_MATRIX版とも、実際に画面へ反映する処理はkeyball.cの
        // keyball_apply_normal_led()に一本化してあるので、ここではkb_led_config
        // （EEPROM保存の独自管理領域）の読み書きだけを行う。

        // 0x0B: 通常（レイヤー0）のLED設定を返す
        // 応答: [cmd, effect_id, hue, sat, val, speed, status]
        // 現在いるレイヤーやレイヤー連動LEDのオーバーライド状態に関わらず、常に
        // 「通常」の設定（kb_led_config）を返す。
        case KB_HID_CMD_GET_LED: {
            kb_led_config_t cfg = kb_led_config_get();
            response[1] = cfg.effect_id;
            response[2] = cfg.hue;
            response[3] = cfg.sat;
            response[4] = cfg.val;
            response[5] = cfg.speed;
            response[6] = KB_HID_STATUS_OK;
            break;
        }

        // 0x0C: 通常（レイヤー0）のLED設定を変更してEEPROMに保存する
        // 要求: [cmd, effect_id, hue, sat, val, speed]
        // 応答: [cmd, status]
        // 現在いるレイヤーに関わらず、常に「通常」の設定（kb_led_config）を変更する。
        // 画面に今すぐ反映するのは、レイヤー連動LEDのオーバーライドが今アクティブで
        // ない時だけ（アクティブ中なら、そのレイヤーを抜けた瞬間に反映される）。
        case KB_HID_CMD_SET_LED: {
            uint8_t         effect_id = data[1] < KB_LED_EFFECT_TOTAL_COUNT ? data[1] : 0;
            kb_led_config_t cfg       = {
                .effect_id = effect_id, .hue = data[2], .sat = data[3], .val = data[4], .speed = data[5],
            };
            kb_led_config_set(&cfg);
            if (!keyball_layer_led_overriding()) {
                keyball_apply_normal_led();
            }
            response[1] = KB_HID_STATUS_OK;
            break;
        }

        // 0x1A: レイヤー連動LED機能の有効/無効を返す
        // 応答: [cmd, enabled, status]
        case KB_HID_CMD_GET_LAYER_LED_ENABLE: {
            response[1] = kb_layer_led_enable_get() ? 1 : 0;
            response[2] = KB_HID_STATUS_OK;
            break;
        }

        // 0x1B: レイヤー連動LED機能の有効/無効を変更してEEPROMに保存する
        // 要求: [cmd, enabled]
        // 応答: [cmd, status]
        case KB_HID_CMD_SET_LAYER_LED_ENABLE: {
            kb_layer_led_enable_set(data[1] != 0);
            response[1] = KB_HID_STATUS_OK;
            break;
        }

        // 0x1C: 指定レイヤー(1-7)のLED設定を返す
        // 要求: [cmd, layer]
        // 応答: [cmd, layer, enabled, effect_id, hue, sat, val, speed, status]
        case KB_HID_CMD_GET_LAYER_LED: {
            uint8_t        layer = data[1];
            kb_layer_led_t cfg   = kb_layer_led_get(layer);
            response[1] = layer;
            response[2] = cfg.enabled;
            response[3] = cfg.effect_id;
            response[4] = cfg.hue;
            response[5] = cfg.sat;
            response[6] = cfg.val;
            response[7] = cfg.speed;
            response[8] = KB_HID_STATUS_OK;
            break;
        }

        // 0x1D: 指定レイヤー(1-7)のLED設定を変更してEEPROMに保存する
        // 要求: [cmd, layer, enabled, effect_id, hue, sat, val, speed]
        // 応答: [cmd, status]
        case KB_HID_CMD_SET_LAYER_LED: {
            uint8_t        layer     = data[1];
            uint8_t        effect_id = data[3] < KB_LED_EFFECT_TOTAL_COUNT ? data[3] : 0;
            kb_layer_led_t cfg       = {
                .enabled   = data[2] != 0,
                .effect_id = effect_id,
                .hue       = data[4],
                .sat       = data[5],
                .val       = data[6],
                .speed     = data[7],
            };
            kb_layer_led_set(layer, &cfg);
            response[1] = KB_HID_STATUS_OK;
            break;
        }
#endif

#ifdef TAP_DANCE_ENABLE
        // 0x0D: タップダンス設定を返す
        case KB_HID_CMD_GET_TD: {
            uint8_t idx = data[1];
            if (idx >= TD_SLOT_COUNT) { response[1] = KB_HID_STATUS_ERROR; break; }
            td_slot_t slot = td_config_get(idx);
            response[1] = idx;
            response[2] = (slot.tap  >> 8) & 0xFF; response[3] = slot.tap  & 0xFF;
            response[4] = (slot.hold >> 8) & 0xFF; response[5] = slot.hold & 0xFF;
            response[6] = (slot.dtap >> 8) & 0xFF; response[7] = slot.dtap & 0xFF;
            response[8] = slot.flags;
            response[9] = KB_HID_STATUS_OK;
            break;
        }

        // 0x0E: タップダンス設定を変更してEEPROMに保存する
        case KB_HID_CMD_SET_TD: {
            uint8_t idx = data[1];
            if (idx >= TD_SLOT_COUNT) { response[1] = KB_HID_STATUS_ERROR; break; }
            td_slot_t slot;
            slot.tap   = ((uint16_t)data[2] << 8) | data[3];
            slot.hold  = ((uint16_t)data[4] << 8) | data[5];
            slot.dtap  = ((uint16_t)data[6] << 8) | data[7];
            slot.flags = data[8];
            slot._pad  = 0;
            td_config_set(idx, &slot);
            response[1] = KB_HID_STATUS_OK;
            break;
        }
#endif // TAP_DANCE_ENABLE

        // 0x0F: 詳細設定を返す
        // 応答: [cmd, tt_hi, tt_lo, flags, aml_layer, aml_to_hi, aml_to_lo, status]
        case KB_HID_CMD_GET_SETTINGS: {
            kb_settings_t s = kb_settings_get();
            response[1] = (s.tapping_term >> 8) & 0xFF;
            response[2] = s.tapping_term & 0xFF;
            response[3] = s.flags;
            response[4] = s.aml_layer;
            response[5] = (s.aml_timeout >> 8) & 0xFF;
            response[6] = s.aml_timeout & 0xFF;
            response[7] = s.aml_threshold;
            response[8] = KB_HID_STATUS_OK;
            response[9] = kb_scroll_layer_get();  // スクロールレイヤー（0-7 / 0xFE=なし）
            break;
        }

        // 0x10: 詳細設定を変更してEEPROMに保存する
        // 要求: [cmd, tt_hi, tt_lo, flags, aml_layer, aml_to_hi, aml_to_lo, aml_threshold]
        // 応答: [cmd, status]
        case KB_HID_CMD_SET_SETTINGS: {
            kb_settings_t s;
            s.tapping_term  = ((uint16_t)data[1] << 8) | data[2];
            s.flags         = data[3];
            s.aml_layer     = data[4];
            s.aml_timeout   = ((uint16_t)data[5] << 8) | data[6];
            s.aml_threshold = data[7] ? data[7] : 10;
            kb_settings_set(&s);
            kb_scroll_layer_set(data[8]);  // スクロールレイヤー（0-7 / 0xFE=なし）
#ifdef AUTO_SHIFT_ENABLE
            if (s.flags & KB_FLAG_AUTO_SHIFT) autoshift_enable();
            else autoshift_disable();
#endif
#ifdef COMBO_ENABLE
            if (s.flags & KB_FLAG_COMBO) combo_enable();
            else combo_disable();
#endif
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
            set_auto_mouse_layer(s.aml_layer);
            set_auto_mouse_timeout(s.aml_timeout);
            set_auto_mouse_enable(!(s.flags & KB_FLAG_AML_DISABLE));
#endif
#ifdef KEYBALL_AML_THRESHOLD_RUNTIME
            kb_aml_threshold = s.aml_threshold;
#endif
#ifdef OS_DETECTION_ENABLE
            // OS自動判別の入れ替え設定を即時反映（トグルOFFにした場合は解除される）
            kb_apply_os_swap(detected_host_os());
#endif
            response[1] = KB_HID_STATUS_OK;
            break;
        }

#ifdef GESTURE_ENABLE
        // 0x20: 指定ジェスチャーモード(0-3)の設定を返す
        // 要求: [cmd, mode]
        // 応答: [cmd, mode, up_hi,up_lo, down_hi,down_lo, left_hi,left_lo, right_hi,right_lo,
        //        continuous, layer, status]
        case KB_HID_CMD_GET_GESTURE_MODE: {
            uint8_t           mode = data[1];
            kb_gesture_mode_t m    = kb_gesture_mode_get(mode);
            response[1] = mode;
            for (uint8_t i = 0; i < 4; i++) {
                response[2 + i * 2] = (m.key[i] >> 8) & 0xFF;
                response[3 + i * 2] = m.key[i] & 0xFF;
            }
            response[10] = m.continuous;
            response[11] = m.layer;
            response[12] = KB_HID_STATUS_OK;
            break;
        }

        // 0x21: 指定ジェスチャーモード(0-3)の設定を変更してEEPROMに保存
        // 要求: [cmd, mode, up_hi,up_lo, down_hi,down_lo, left_hi,left_lo, right_hi,right_lo,
        //        continuous, layer]
        case KB_HID_CMD_SET_GESTURE_MODE: {
            uint8_t           mode = data[1];
            kb_gesture_mode_t m;
            for (uint8_t i = 0; i < 4; i++) {
                m.key[i] = ((uint16_t)data[2 + i * 2] << 8) | data[3 + i * 2];
            }
            m.continuous = data[10];
            m.layer      = data[11];
            kb_gesture_mode_set(mode, &m);
            response[1] = KB_HID_STATUS_OK;
            break;
        }

        // 0x22: ジェスチャー発動しきい値（全モード共通）を返す
        // 応答: [cmd, th_h, th_v, status]
        case KB_HID_CMD_GET_GESTURE_THRESHOLD: {
            response[1] = kb_gesture_th_h_get();
            response[2] = kb_gesture_th_v_get();
            response[3] = KB_HID_STATUS_OK;
            break;
        }

        // 0x23: ジェスチャー発動しきい値（全モード共通）を変更
        // 要求: [cmd, th_h, th_v]
        case KB_HID_CMD_SET_GESTURE_THRESHOLD: {
            kb_gesture_th_h_set(data[1]);
            kb_gesture_th_v_set(data[2]);
            response[1] = KB_HID_STATUS_OK;
            break;
        }

        // 0x24: ジェスチャー連動LEDウェーブの速さを返す
        // 応答: [cmd, speed, status]
        case KB_HID_CMD_GET_GESTURE_WAVE_SPEED: {
            response[1] = kb_gesture_wave_speed_get();
            response[2] = KB_HID_STATUS_OK;
            break;
        }

        // 0x25: ジェスチャー連動LEDウェーブの速さを変更
        // 要求: [cmd, speed]
        case KB_HID_CMD_SET_GESTURE_WAVE_SPEED: {
            kb_gesture_wave_speed_set(data[1]);
            response[1] = KB_HID_STATUS_OK;
            break;
        }

        // 0x26: ジェスチャー連動LEDウェーブの有効/無効を返す
        case KB_HID_CMD_GET_GESTURE_WAVE_ENABLE: {
            response[1] = kb_gesture_wave_enable_get() ? 1 : 0;
            response[2] = KB_HID_STATUS_OK;
            break;
        }

        // 0x27: ジェスチャー連動LEDウェーブの有効/無効を変更
        case KB_HID_CMD_SET_GESTURE_WAVE_ENABLE: {
            kb_gesture_wave_enable_set(data[1] != 0);
            response[1] = KB_HID_STATUS_OK;
            break;
        }

#endif // GESTURE_ENABLE

        // 0x2A: ダブルフリック（方向別発動キー・時間窓・フリック判定しきい値・最大継続時間・有効/無効）を返す
        // 応答: [cmd, up_hi,up_lo, down_hi,down_lo, left_hi,left_lo, right_hi,right_lo,
        //        window_10ms, flick_threshold, max_duration_10ms, enable, status]
        case KB_HID_CMD_GET_DFLICK: {
            for (uint8_t i = 0; i < 4; i++) {
                uint16_t kc         = kb_dflick_key_get(i);
                response[1 + i * 2] = (kc >> 8) & 0xFF;
                response[2 + i * 2] = kc & 0xFF;
            }
            response[9]  = (uint8_t)(kb_dflick_window_ms_get() / 10);
            response[10] = kb_dflick_flick_threshold_get();
            response[11] = (uint8_t)(kb_dflick_max_duration_ms_get() / 10);
            response[12] = kb_dflick_enable_get() ? 1 : 0;
            response[13] = KB_HID_STATUS_OK;
            break;
        }

        // 0x2B: ダブルフリック（方向別発動キー・時間窓・フリック判定しきい値・最大継続時間・有効/無効）を変更
        // 要求: [cmd, up_hi,up_lo, down_hi,down_lo, left_hi,left_lo, right_hi,right_lo,
        //        window_10ms, flick_threshold, max_duration_10ms, enable]
        case KB_HID_CMD_SET_DFLICK: {
            for (uint8_t i = 0; i < 4; i++) {
                uint16_t kc = ((uint16_t)data[1 + i * 2] << 8) | data[2 + i * 2];
                kb_dflick_key_set(i, kc);
            }
            kb_dflick_window_ms_set((uint16_t)data[9] * 10);
            kb_dflick_flick_threshold_set(data[10]);
            kb_dflick_max_duration_ms_set((uint16_t)data[11] * 10);
            kb_dflick_enable_set(data[12] != 0);
            response[1] = KB_HID_STATUS_OK;
            break;
        }

        // 0x28: シェイク機能（発動キー・感度・反転回数・許容時間・有効/無効）を返す
        // 応答: [cmd, key_hi, key_lo, threshold, reversals, run_max_10ms, enable, status]
        case KB_HID_CMD_GET_SHAKE: {
            uint16_t key = kb_shake_key_get();
            response[1] = (key >> 8) & 0xFF;
            response[2] = key & 0xFF;
            response[3] = kb_shake_threshold_get();
            response[4] = kb_shake_reversals_get();
            response[5] = (uint8_t)(kb_shake_run_max_ms_get() / 10);
            response[6] = kb_shake_enable_get() ? 1 : 0;
            response[7] = KB_HID_STATUS_OK;
            break;
        }

        // 0x29: シェイク機能（発動キー・感度・反転回数・許容時間・有効/無効）を変更
        // 要求: [cmd, key_hi, key_lo, threshold, reversals, run_max_10ms, enable]
        case KB_HID_CMD_SET_SHAKE: {
            uint16_t key = ((uint16_t)data[1] << 8) | data[2];
            kb_shake_key_set(key);
            kb_shake_threshold_set(data[3]);
            kb_shake_reversals_set(data[4]);
            kb_shake_run_max_ms_set((uint16_t)data[5] * 10);
            kb_shake_enable_set(data[6] != 0);
            response[1] = KB_HID_STATUS_OK;
            break;
        }

#ifdef COMBO_ENABLE
        // 0x2C: 指定コンボスロット(0-7)の設定を返す
        // 要求: [cmd, idx]
        // 応答: [cmd, idx, key0_hi,key0_lo, key1_hi,key1_lo, key2_hi,key2_lo, key3_hi,key3_lo,
        //        keycode_hi,keycode_lo, status]
        case KB_HID_CMD_GET_COMBO: {
            uint8_t         idx  = data[1];
            kb_combo_slot_t slot = kb_combo_get(idx);
            response[1] = idx;
            for (uint8_t i = 0; i < KB_COMBO_MAX_KEYS; i++) {
                response[2 + i * 2] = (slot.keys[i] >> 8) & 0xFF;
                response[3 + i * 2] = slot.keys[i] & 0xFF;
            }
            response[10] = (slot.keycode >> 8) & 0xFF;
            response[11] = slot.keycode & 0xFF;
            response[12] = KB_HID_STATUS_OK;
            break;
        }

        // 0x2D: 指定コンボスロット(0-7)の設定を変更してEEPROMに保存
        // 要求: [cmd, idx, key0_hi,key0_lo, key1_hi,key1_lo, key2_hi,key2_lo, key3_hi,key3_lo,
        //        keycode_hi,keycode_lo]
        case KB_HID_CMD_SET_COMBO: {
            uint8_t         idx = data[1];
            kb_combo_slot_t slot;
            for (uint8_t i = 0; i < KB_COMBO_MAX_KEYS; i++) {
                slot.keys[i] = ((uint16_t)data[2 + i * 2] << 8) | data[3 + i * 2];
            }
            slot.keycode = ((uint16_t)data[10] << 8) | data[11];
            kb_combo_set(idx, &slot);
            response[1] = KB_HID_STATUS_OK;
            break;
        }
#endif

#ifdef OS_DETECTION_ENABLE
        // 0x2E: OS自動判別で現在検出しているOS種別を返す
        // 応答: [cmd, os, status]  os = 0:不明 1:Linux 2:Windows 3:macOS 4:iOS
        case KB_HID_CMD_GET_OS: {
            response[1] = (uint8_t)detected_host_os();
            response[2] = KB_HID_STATUS_OK;
            break;
        }
#endif

        // 0x2F: DPIカーブ（トラックボールの動きの速さ→実際に送る速さの5点カーブ）を返す
        // 応答: [cmd, enable, y0, y1, y2, y3, y4, status]
        case KB_HID_CMD_GET_DPI_CURVE: {
            const uint8_t *pts = kb_dpi_curve_points_get();
            response[1] = kb_dpi_curve_enable_get() ? 1 : 0;
            for (uint8_t i = 0; i < KB_DPI_CURVE_POINT_COUNT; i++) response[2 + i] = pts[i];
            response[2 + KB_DPI_CURVE_POINT_COUNT] = KB_HID_STATUS_OK;
            break;
        }

        // 0x30: DPIカーブを変更してEEPROMに保存する
        // 要求: [cmd, enable, y0, y1, y2, y3, y4]
        case KB_HID_CMD_SET_DPI_CURVE: {
            kb_dpi_curve_enable_set(data[1] != 0);
            uint8_t pts[KB_DPI_CURVE_POINT_COUNT];
            for (uint8_t i = 0; i < KB_DPI_CURVE_POINT_COUNT; i++) pts[i] = data[2 + i];
            kb_dpi_curve_points_set(pts);
            response[1] = KB_HID_STATUS_OK;
            break;
        }

        // 0x17: ファームウェアのバージョンを返す（全機種・全バージョン共通で応答）
        // 応答: [cmd, major, minor, patch, status]
        case KB_HID_CMD_GET_VERSION: {
            response[1] = KB_FW_VERSION_MAJOR;
            response[2] = KB_FW_VERSION_MINOR;
            response[3] = KB_FW_VERSION_PATCH;
            response[4] = KB_HID_STATUS_OK;
            break;
        }

        // 0x18: 超低速モードのCPI分周値・連動レイヤーを返す
        // 応答: [cmd, div, layer, status]
        case KB_HID_CMD_GET_PRECISION: {
            response[1] = kb_precision_div_get();
            response[2] = kb_precision_layer_get();  // 連動レイヤー（0-7 / 0xFE=なし）
            response[3] = KB_HID_STATUS_OK;
            break;
        }

        // 0x19: 超低速モードのCPI分周値・連動レイヤーを変更してEEPROMに保存
        // 要求: [cmd, div, layer]
        // 応答: [cmd, status]
        case KB_HID_CMD_SET_PRECISION: {
            kb_precision_div_set(data[1]);
            kb_precision_layer_set(data[2]);  // 連動レイヤー（0-7 / 0xFE=なし）
            response[1] = KB_HID_STATUS_OK;
            break;
        }

        // 0x1E: 慣性スクロール設定を返す
        // 応答: [cmd, enable, strength, flick_mult, status]
        case KB_HID_CMD_GET_SCROLL_INERTIA: {
            response[1] = kb_scroll_inertia_enable_get() ? 1 : 0;
            response[2] = kb_scroll_inertia_strength_get();
            response[3] = kb_scroll_inertia_flick_mult_get();  // 発動しきい値の倍率×10（例:30=3.0倍）
            response[4] = KB_HID_STATUS_OK;
            break;
        }

        // 0x1F: 慣性スクロール設定を変更してEEPROMに保存
        // 要求: [cmd, enable, strength, flick_mult]
        // 応答: [cmd, status]
        case KB_HID_CMD_SET_SCROLL_INERTIA: {
            kb_scroll_inertia_enable_set(data[1] != 0);
            kb_scroll_inertia_strength_set(data[2]);
            kb_scroll_inertia_flick_mult_set(data[3]);
            response[1] = KB_HID_STATUS_OK;
            break;
        }

        // 0x07: ブートローダーへジャンプ（ファームウェア書き込み用）
        // 応答を送信してから 500ms 後にリセット
        case KB_HID_CMD_REBOOT: {
            response[1] = KB_HID_STATUS_OK;
            raw_hid_send(response, RAW_PACKET_SIZE);
            wait_ms(500);
            bootloader_jump();
            return;  // raw_hid_send を二重に呼ばないよう return
        }

#ifdef RGB_MATRIX_ENABLE
        // 0x11: LED診断モード — 指定インデックスのLEDのみ点灯 (0xFF で終了)
        // LED_TEST エフェクトに切替え、hue フィールドにインデックスを格納して左右同期する
        // 要求: [cmd, led_index]  (0xFF = 終了)
        // 応答: [cmd, status]
        case KB_HID_CMD_TEST_LED: {
            uint8_t idx = data[1];
            if (idx == 0xFF) {
                rgb_matrix_mode_noeeprom(RGB_MATRIX_BREATHING);
            } else if (idx < RGB_MATRIX_LED_COUNT) {
#ifdef RGB_MATRIX_CUSTOM_USER
                rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_LED_TEST);
                rgb_matrix_sethsv_noeeprom(idx, 255, 200);
#endif
            }
            response[1] = KB_HID_STATUS_OK;
            break;
        }
#endif

        // 0x12: 現在のキーマトリクス状態を返す
        // 応答: [cmd, rows, row0_mask, row1_mask, ..., row(N-1)_mask, status]
        // 各 row_mask の bit i が 1 のとき col i が押されている
        case KB_HID_CMD_GET_MATRIX: {
            uint8_t rows = MATRIX_ROWS;
            response[1] = rows;
            for (uint8_t r = 0; r < rows && r < 28; r++) {
                matrix_row_t row = matrix_get_row(r);
                response[2 + r] = (uint8_t)(row & 0xFF);
            }
            response[2 + rows] = KB_HID_STATUS_OK;
            break;
        }

#ifndef LED_VERSION_BUILD
        // 0x13: マクロバッファの一部を返す（VIA方式）
        // 要求: [cmd, offset_hi, offset_lo]
        // 応答: [cmd, offset_hi, offset_lo, data×28, status]
        case KB_HID_CMD_GET_MACRO: {
            uint16_t offset = ((uint16_t)data[1] << 8) | data[2];
            response[1] = data[1];
            response[2] = data[2];
            kb_macro_buffer_read(offset, &response[3], MACRO_CHUNK_SIZE);
            response[3 + MACRO_CHUNK_SIZE] = KB_HID_STATUS_OK;
            break;
        }

        // 0x14: マクロバッファの一部を書き込む（VIA方式）
        // 要求: [cmd, offset_hi, offset_lo, len, data×len]
        // 応答: [cmd, status]
        case KB_HID_CMD_SET_MACRO: {
            uint16_t offset = ((uint16_t)data[1] << 8) | data[2];
            uint8_t  len    = data[3] <= MACRO_CHUNK_SIZE ? data[3] : MACRO_CHUNK_SIZE;
            kb_macro_buffer_write(offset, &data[4], len);
            response[1] = KB_HID_STATUS_OK;
            break;
        }
#endif // LED_VERSION_BUILD

        default:
            response[0] = 0xFF;
            response[1] = KB_HID_STATUS_ERROR;
            break;
    }

    raw_hid_send(response, RAW_PACKET_SIZE);
}
