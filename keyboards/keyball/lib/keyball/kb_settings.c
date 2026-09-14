// Copyright 2024 keyball-custom contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "kb_settings.h"
#include "eeprom.h"
#include <stdbool.h>
#include <string.h>

// 注意: KB_LAYER_LED_TABLE_EEPROM (0x09E7) 〜 +42バイト(0x0A10)は、既存のscroll/gesture/
// precision layer設定（0x09E0-0x09E5）に続く「空き領域」として使っている。
// EEPROM配置全体の経緯・dynamic keymapとの衝突防止についてはkb_settings.h冒頭のコメントを
// 参照（2026-09-04に0x0200台からdynamic keymapと衝突しない0x0800台へ全面移動済み）。

static kb_settings_t g_cache;
static bool          g_loaded = false;

kb_settings_t kb_settings_get(void) {
    if (!g_loaded) {
        eeprom_read_block(&g_cache,
            (void *)(uintptr_t)KB_SETTINGS_EEPROM_BASE,
            sizeof(kb_settings_t));
        // 0xFFFF = 未初期化EEPROM、0 = 無効値 → デフォルトへ
        if (g_cache.tapping_term == 0xFFFF || g_cache.tapping_term < 50 || g_cache.tapping_term > 1000) {
            memset(&g_cache, 0, sizeof(g_cache));
            g_cache.tapping_term = KB_SETTINGS_DEFAULT_TT;
            // 2026-09-11、本人希望により Permissive Hold を既定でON にする（一度でも
            // 設定を保存したことがある機体は保存値が優先されるので、この初期値が
            // 効くのは工場出荷/設定リセット直後のみ）。
            g_cache.flags |= KB_FLAG_PERMISSIVE_HOLD;
        }
        // AML設定が未初期化(0)または範囲外なら既定値へ補正（旧FWからの移行も安全に）
        if (g_cache.aml_layer == 0 || g_cache.aml_layer > 7)         g_cache.aml_layer = 1;
        if (g_cache.aml_timeout < 100 || g_cache.aml_timeout > 5000) g_cache.aml_timeout = 650;
        if (g_cache.aml_threshold == 0 || g_cache.aml_threshold > 100) g_cache.aml_threshold = 10;
        g_loaded = true;
    }
    return g_cache;
}

void kb_settings_set(const kb_settings_t *s) {
    g_cache = *s;
    eeprom_write_block(s,
        (void *)(uintptr_t)KB_SETTINGS_EEPROM_BASE,
        sizeof(kb_settings_t));
}

// KB_TRACKBALL_LAYERS_MAGIC_EEPROM参照。スクロール/超低速レイヤーが一度でも実際に
// 保存されたかどうかの目印。目印が無い間は生バイトを信用せず既定値を返す
// （0x00で未初期化されるRP2040のEEPROMでは、生バイトの0とレイヤー0を区別できないため）。
static int8_t g_trackball_layers_configured = -1;  // -1=未確認 0=未保存 1=保存済み

static bool trackball_layers_configured(void) {
    if (g_trackball_layers_configured < 0) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_TRACKBALL_LAYERS_MAGIC_EEPROM);
        g_trackball_layers_configured = (v == KB_TRACKBALL_LAYERS_MAGIC_VALUE) ? 1 : 0;
    }
    return g_trackball_layers_configured == 1;
}

static void trackball_layers_mark_configured(void) {
    if (g_trackball_layers_configured == 1) return;
    g_trackball_layers_configured = 1;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_TRACKBALL_LAYERS_MAGIC_EEPROM, KB_TRACKBALL_LAYERS_MAGIC_VALUE);
}

// ── スクロールレイヤー（構造体外・EEPROM末尾に1バイト保存）────────────
static uint8_t g_scroll_layer  = 0xEE;
static bool    g_scroll_loaded = false;

uint8_t kb_scroll_layer_get(void) {
    if (!g_scroll_loaded) {
        if (!trackball_layers_configured()) {
            g_scroll_layer = 3;  // 一度も保存されたことがない → 既定レイヤー3
        } else {
            uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_SCROLL_LAYER_EEPROM);
            if (v <= 7)                  g_scroll_layer = v;             // 0-7 = そのレイヤー
            else if (v == KB_LAYER_NONE) g_scroll_layer = KB_LAYER_NONE; // 明示的に「なし」
            else                         g_scroll_layer = 3;            // 想定外の値 → 既定レイヤー3
        }
        g_scroll_loaded = true;
    }
    return g_scroll_layer;
}

void kb_scroll_layer_set(uint8_t v) {
    g_scroll_layer  = (v <= 7) ? v : KB_LAYER_NONE;
    g_scroll_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_SCROLL_LAYER_EEPROM, g_scroll_layer);
    trackball_layers_mark_configured();
}

#ifdef GESTURE_ENABLE
// ── ジェスチャーしきい値（横・縦、同上パターン）────────────────────────
static uint8_t g_gesture_th_h        = 0xEE;
static bool    g_gesture_th_h_loaded = false;

uint8_t kb_gesture_th_h_get(void) {
    if (!g_gesture_th_h_loaded) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_GESTURE_TH_H_EEPROM);
        g_gesture_th_h = (v >= KB_GESTURE_TH_MIN && v <= KB_GESTURE_TH_MAX) ? v : KB_GESTURE_TH_DEFAULT;
        g_gesture_th_h_loaded = true;
    }
    return g_gesture_th_h;
}

void kb_gesture_th_h_set(uint8_t v) {
    g_gesture_th_h = (v >= KB_GESTURE_TH_MIN && v <= KB_GESTURE_TH_MAX) ? v : KB_GESTURE_TH_DEFAULT;
    g_gesture_th_h_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_GESTURE_TH_H_EEPROM, g_gesture_th_h);
}

static uint8_t g_gesture_th_v        = 0xEE;
static bool    g_gesture_th_v_loaded = false;

uint8_t kb_gesture_th_v_get(void) {
    if (!g_gesture_th_v_loaded) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_GESTURE_TH_V_EEPROM);
        g_gesture_th_v = (v >= KB_GESTURE_TH_MIN && v <= KB_GESTURE_TH_MAX) ? v : KB_GESTURE_TH_DEFAULT;
        g_gesture_th_v_loaded = true;
    }
    return g_gesture_th_v;
}

void kb_gesture_th_v_set(uint8_t v) {
    g_gesture_th_v = (v >= KB_GESTURE_TH_MIN && v <= KB_GESTURE_TH_MAX) ? v : KB_GESTURE_TH_DEFAULT;
    g_gesture_th_v_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_GESTURE_TH_V_EEPROM, g_gesture_th_v);
}

// ── 複数ジェスチャーモード（kb_layer_led_get/setと同様のテーブル読み書きだが、
//    呼び出し頻度がトラックボール移動のたびと高いため、こちらはRAMキャッシュする）──
static kb_gesture_mode_t g_gesture_modes[KB_GESTURE_MODE_COUNT];
static bool              g_gesture_modes_loaded = false;

static void kb_gesture_modes_ensure_loaded(void) {
    if (g_gesture_modes_loaded) return;
    for (uint8_t i = 0; i < KB_GESTURE_MODE_COUNT; i++) {
        uint16_t addr = KB_GESTURE_MODE_TABLE_EEPROM + (uint16_t)i * KB_GESTURE_MODE_ENTRY_SIZE;
        uint8_t  buf[KB_GESTURE_MODE_ENTRY_SIZE];
        eeprom_read_block(buf, (const void *)(uintptr_t)addr, KB_GESTURE_MODE_ENTRY_SIZE);

        kb_gesture_mode_t m;
        m.key[0]     = ((uint16_t)buf[0] << 8) | buf[1];
        m.key[1]     = ((uint16_t)buf[2] << 8) | buf[3];
        m.key[2]     = ((uint16_t)buf[4] << 8) | buf[5];
        m.key[3]     = ((uint16_t)buf[6] << 8) | buf[7];
        m.continuous = buf[8];
        m.layer      = buf[9];

        // 未書き込みEEPROMならモードごとの既定値へ補正。RP2040のEEPROM(wear leveling方式)は
        // 未書き込み領域が0xFFではなく0x00で初期化されるため、両方を「未設定」とみなす
        // （2026-09-09発覚: 0x00を無視すると生バイトのlayer==0が「レイヤー0に連動」と
        // 誤認識され、一度もWeb UIで保存していないモードがレイヤー0で暴発する事故になる）。
        bool all_ff   = m.key[0] == 0xFFFF && m.key[1] == 0xFFFF && m.key[2] == 0xFFFF && m.key[3] == 0xFFFF;
        bool all_zero = m.key[0] == 0 && m.key[1] == 0 && m.key[2] == 0 && m.key[3] == 0 && m.continuous == 0 && m.layer == 0;
        if (all_ff || all_zero) {
            if (i == 0) {
                // モード1のみ、旧単一ジェスチャーと同じデフォルト割り当てを引き継ぐ
                m.key[0] = KB_GESTURE_DEFAULT_UP;
                m.key[1] = KB_GESTURE_DEFAULT_DOWN;
                m.key[2] = KB_GESTURE_DEFAULT_LEFT;
                m.key[3] = KB_GESTURE_DEFAULT_RIGHT;
            } else {
                m.key[0] = m.key[1] = m.key[2] = m.key[3] = 0;  // 未設定
            }
            m.continuous = 0;
            m.layer      = KB_LAYER_NONE;
        } else if (m.layer > 7) {
            m.layer = KB_LAYER_NONE;  // 範囲外は「なし」に補正
        }
        g_gesture_modes[i] = m;
    }
    g_gesture_modes_loaded = true;
}

kb_gesture_mode_t kb_gesture_mode_get(uint8_t mode) {
    kb_gesture_modes_ensure_loaded();
    if (mode >= KB_GESTURE_MODE_COUNT) mode = 0;
    return g_gesture_modes[mode];
}

void kb_gesture_mode_set(uint8_t mode, const kb_gesture_mode_t *cfg) {
    kb_gesture_modes_ensure_loaded();
    if (mode >= KB_GESTURE_MODE_COUNT) return;
    g_gesture_modes[mode] = *cfg;

    uint8_t buf[KB_GESTURE_MODE_ENTRY_SIZE] = {
        (uint8_t)(cfg->key[0] >> 8), (uint8_t)cfg->key[0],
        (uint8_t)(cfg->key[1] >> 8), (uint8_t)cfg->key[1],
        (uint8_t)(cfg->key[2] >> 8), (uint8_t)cfg->key[2],
        (uint8_t)(cfg->key[3] >> 8), (uint8_t)cfg->key[3],
        cfg->continuous, cfg->layer,
    };
    uint16_t addr = KB_GESTURE_MODE_TABLE_EEPROM + (uint16_t)mode * KB_GESTURE_MODE_ENTRY_SIZE;
    eeprom_write_block(buf, (void *)(uintptr_t)addr, KB_GESTURE_MODE_ENTRY_SIZE);
}

// ── ジェスチャー連動LEDウェーブの速さ（ジェスチャーしきい値と同パターン）────
static uint8_t g_gesture_wave_speed        = 0xEE;
static bool    g_gesture_wave_speed_loaded = false;

uint8_t kb_gesture_wave_speed_get(void) {
    if (!g_gesture_wave_speed_loaded) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_GESTURE_WAVE_SPEED_EEPROM);
        g_gesture_wave_speed = (v >= KB_GESTURE_WAVE_SPEED_MIN && v <= KB_GESTURE_WAVE_SPEED_MAX) ? v : KB_GESTURE_WAVE_SPEED_DEFAULT;
        g_gesture_wave_speed_loaded = true;
    }
    return g_gesture_wave_speed;
}

void kb_gesture_wave_speed_set(uint8_t v) {
    g_gesture_wave_speed = (v >= KB_GESTURE_WAVE_SPEED_MIN && v <= KB_GESTURE_WAVE_SPEED_MAX) ? v : KB_GESTURE_WAVE_SPEED_DEFAULT;
    g_gesture_wave_speed_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_GESTURE_WAVE_SPEED_EEPROM, g_gesture_wave_speed);
}

// ── ジェスチャー連動LEDウェーブ機能の有効/無効（既定ON。理由はkb_settings.h参照）──
static int8_t g_gesture_wave_enable = -1;  // -1=未確認 0=OFF 1=ON

bool kb_gesture_wave_enable_get(void) {
    if (g_gesture_wave_enable < 0) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_GESTURE_WAVE_ENABLE_EEPROM);
        g_gesture_wave_enable = (v == 1) ? 0 : 1;  // 1のみ明示的なOFF、それ以外は既定ON
    }
    return g_gesture_wave_enable != 0;
}

void kb_gesture_wave_enable_set(bool v) {
    g_gesture_wave_enable = v ? 1 : 0;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_GESTURE_WAVE_ENABLE_EEPROM, v ? 0 : 1);
}
#endif

// ── 超低速モードのCPI分周値（同上パターン）──────────────────────────
static uint8_t g_precision_div        = 0xEE;
static bool    g_precision_div_loaded = false;

uint8_t kb_precision_div_get(void) {
    if (!g_precision_div_loaded) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_PRECISION_DIV_EEPROM);
        g_precision_div = (v >= KB_PRECISION_DIV_MIN && v <= KB_PRECISION_DIV_MAX) ? v : KB_PRECISION_DIV_DEFAULT;
        g_precision_div_loaded = true;
    }
    return g_precision_div;
}

void kb_precision_div_set(uint8_t v) {
    g_precision_div = (v >= KB_PRECISION_DIV_MIN && v <= KB_PRECISION_DIV_MAX) ? v : KB_PRECISION_DIV_DEFAULT;
    g_precision_div_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_PRECISION_DIV_EEPROM, g_precision_div);
}

// ── 超低速モードの連動レイヤー（ジェスチャーレイヤーと同パターン。未初期化/0xFE は「なし」）──
static uint8_t g_precision_layer        = 0xEE;
static bool    g_precision_layer_loaded = false;

uint8_t kb_precision_layer_get(void) {
    if (!g_precision_layer_loaded) {
        if (!trackball_layers_configured()) {
            g_precision_layer = KB_LAYER_NONE;  // 一度も保存されたことがない → 既定は「なし」
        } else {
            uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_PRECISION_LAYER_EEPROM);
            g_precision_layer = (v <= 7) ? v : KB_LAYER_NONE;  // 0-7=レイヤー / それ以外=なし
        }
        g_precision_layer_loaded = true;
    }
    return g_precision_layer;
}

void kb_precision_layer_set(uint8_t v) {
    g_precision_layer        = (v <= 7) ? v : KB_LAYER_NONE;
    g_precision_layer_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_PRECISION_LAYER_EEPROM, g_precision_layer);
    trackball_layers_mark_configured();
}

// ── レイヤー連動LED有効フラグ（同上パターン）────────────────────────
static uint8_t g_layer_led_enable        = 0xEE;
static bool    g_layer_led_enable_loaded = false;

bool kb_layer_led_enable_get(void) {
    if (!g_layer_led_enable_loaded) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_LAYER_LED_ENABLE_EEPROM);
        g_layer_led_enable        = (v == 1) ? 1 : 0;  // 未初期化(0xFF)・不正値は無効扱い
        g_layer_led_enable_loaded = true;
    }
    return g_layer_led_enable != 0;
}

void kb_layer_led_enable_set(bool v) {
    g_layer_led_enable        = v ? 1 : 0;
    g_layer_led_enable_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_LAYER_LED_ENABLE_EEPROM, g_layer_led_enable);
}

// ── レイヤー別LED設定 ────────────────────────────────────────
// 呼び出し頻度が低い（レイヤー切替時とWeb UI表示時のみ）ためキャッシュせず毎回EEPROMを読む。
kb_layer_led_t kb_layer_led_get(uint8_t layer) {
    kb_layer_led_t cfg = { .enabled = 0, .effect_id = 0, .hue = 0, .sat = 255, .val = 150, .speed = 128 };
    if (layer >= 1 && layer <= KB_LAYER_LED_MAX_LAYER) {
        uint16_t addr = KB_LAYER_LED_TABLE_EEPROM + (uint16_t)(layer - 1) * KB_LAYER_LED_ENTRY_SIZE;
        uint8_t  buf[KB_LAYER_LED_ENTRY_SIZE];
        eeprom_read_block(buf, (const void *)(uintptr_t)addr, KB_LAYER_LED_ENTRY_SIZE);
        if (buf[0] <= 1) {  // 0/1以外（未初期化の0xFF等）は既定値のまま
            cfg.enabled   = buf[0];
            cfg.effect_id = buf[1];
            cfg.hue       = buf[2];
            cfg.sat       = buf[3];
            cfg.val       = buf[4];
            cfg.speed     = buf[5];
        }
    }
    return cfg;
}

void kb_layer_led_set(uint8_t layer, const kb_layer_led_t *cfg) {
    if (layer < 1 || layer > KB_LAYER_LED_MAX_LAYER) return;
    uint16_t addr             = KB_LAYER_LED_TABLE_EEPROM + (uint16_t)(layer - 1) * KB_LAYER_LED_ENTRY_SIZE;
    uint8_t  buf[KB_LAYER_LED_ENTRY_SIZE] = {
        cfg->enabled ? 1 : 0, cfg->effect_id, cfg->hue, cfg->sat, cfg->val, cfg->speed,
    };
    eeprom_write_block(buf, (void *)(uintptr_t)addr, KB_LAYER_LED_ENTRY_SIZE);
}

// ── 通常（レイヤー0）のLED設定 ────────────────────────────────
// 値の妥当性検証（有効なeffect_id範囲かどうか）はプロトコル層(kb_hid.c)の責務とし、
// ここでは他の設定と同様に生のバイトをそのまま保存・返す。
static kb_led_config_t g_led_config        = {0};
static bool            g_led_config_loaded = false;

kb_led_config_t kb_led_config_get(void) {
    if (!g_led_config_loaded) {
        uint8_t buf[sizeof(kb_led_config_t)];
        eeprom_read_block(buf, (const void *)(uintptr_t)KB_LED_CONFIG_EEPROM, sizeof(buf));
        if (buf[0] == 0xFF) {
            // 未初期化。RGBLIGHT_DEFAULT_MODE(呼吸)相当のそれらしい既定値にしておく。
            g_led_config = (kb_led_config_t){.effect_id = 2, .hue = 170, .sat = 255, .val = 100, .speed = 128};
        } else {
            memcpy(&g_led_config, buf, sizeof(buf));
        }
        g_led_config_loaded = true;
    }
    return g_led_config;
}

void kb_led_config_set(const kb_led_config_t *cfg) {
    g_led_config        = *cfg;
    g_led_config_loaded = true;
    eeprom_write_block(cfg, (void *)(uintptr_t)KB_LED_CONFIG_EEPROM, sizeof(*cfg));
}

// ── 慣性スクロール有効フラグ（レイヤー連動LED有効フラグと同パターン）──────
static uint8_t g_scroll_inertia_enable        = 0xEE;
static bool    g_scroll_inertia_enable_loaded = false;

bool kb_scroll_inertia_enable_get(void) {
    if (!g_scroll_inertia_enable_loaded) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_SCROLL_INERTIA_ENABLE_EEPROM);
        g_scroll_inertia_enable        = (v == 1) ? 1 : 0;  // 未初期化(0xFF)・不正値は無効扱い
        g_scroll_inertia_enable_loaded = true;
    }
    return g_scroll_inertia_enable != 0;
}

void kb_scroll_inertia_enable_set(bool v) {
    g_scroll_inertia_enable        = v ? 1 : 0;
    g_scroll_inertia_enable_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_SCROLL_INERTIA_ENABLE_EEPROM, g_scroll_inertia_enable);
}

// ── 慣性スクロールの強さ（未初期化(0xFF)ならデフォルトへ。0-254のみ有効値）───
static uint16_t g_scroll_inertia_strength        = 0xFFFF;
static bool     g_scroll_inertia_strength_loaded = false;

uint8_t kb_scroll_inertia_strength_get(void) {
    if (!g_scroll_inertia_strength_loaded) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_SCROLL_INERTIA_STRENGTH_EEPROM);
        g_scroll_inertia_strength        = (v > KB_SCROLL_INERTIA_STRENGTH_MAX) ? KB_SCROLL_INERTIA_STRENGTH_DEFAULT : v;
        g_scroll_inertia_strength_loaded = true;
    }
    return (uint8_t)g_scroll_inertia_strength;
}

void kb_scroll_inertia_strength_set(uint8_t v) {
    g_scroll_inertia_strength        = (v > KB_SCROLL_INERTIA_STRENGTH_MAX) ? KB_SCROLL_INERTIA_STRENGTH_MAX : v;
    g_scroll_inertia_strength_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_SCROLL_INERTIA_STRENGTH_EEPROM, (uint8_t)g_scroll_inertia_strength);
}

// ── 慣性の発動しきい値の倍率×10（範囲外・未初期化(0xFF)ならデフォルトへ）───
static uint16_t g_scroll_inertia_flick_mult        = 0xFFFF;
static bool     g_scroll_inertia_flick_mult_loaded = false;

uint8_t kb_scroll_inertia_flick_mult_get(void) {
    if (!g_scroll_inertia_flick_mult_loaded) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_SCROLL_INERTIA_FLICK_MULT_EEPROM);
        g_scroll_inertia_flick_mult = (v < KB_SCROLL_INERTIA_FLICK_MULT_MIN || v > KB_SCROLL_INERTIA_FLICK_MULT_MAX)
                                          ? KB_SCROLL_INERTIA_FLICK_MULT_DEFAULT
                                          : v;
        g_scroll_inertia_flick_mult_loaded = true;
    }
    return (uint8_t)g_scroll_inertia_flick_mult;
}

void kb_scroll_inertia_flick_mult_set(uint8_t v) {
    if (v < KB_SCROLL_INERTIA_FLICK_MULT_MIN) v = KB_SCROLL_INERTIA_FLICK_MULT_MIN;
    if (v > KB_SCROLL_INERTIA_FLICK_MULT_MAX) v = KB_SCROLL_INERTIA_FLICK_MULT_MAX;
    g_scroll_inertia_flick_mult        = v;
    g_scroll_inertia_flick_mult_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_SCROLL_INERTIA_FLICK_MULT_EEPROM, (uint8_t)g_scroll_inertia_flick_mult);
}

// ── シェイク機能（発動キー・感度。ジェスチャーしきい値と同パターン）──────────
static uint16_t g_shake_key        = 0;
static bool     g_shake_key_loaded = false;

uint16_t kb_shake_key_get(void) {
    if (!g_shake_key_loaded) {
        uint8_t hi = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_SHAKE_KEY_EEPROM);
        uint8_t lo = eeprom_read_byte((const uint8_t *)(uintptr_t)(KB_SHAKE_KEY_EEPROM + 1));
        g_shake_key        = ((uint16_t)hi << 8) | lo;
        g_shake_key_loaded = true;
    }
    return g_shake_key;
}

void kb_shake_key_set(uint16_t v) {
    g_shake_key        = v;
    g_shake_key_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_SHAKE_KEY_EEPROM, (uint8_t)(v >> 8));
    eeprom_write_byte((uint8_t *)(uintptr_t)(KB_SHAKE_KEY_EEPROM + 1), (uint8_t)v);
}

static uint8_t g_shake_threshold        = 0xEE;
static bool    g_shake_threshold_loaded = false;

uint8_t kb_shake_threshold_get(void) {
    if (!g_shake_threshold_loaded) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_SHAKE_THRESHOLD_EEPROM);
        g_shake_threshold = (v >= KB_SHAKE_THRESHOLD_MIN && v <= KB_SHAKE_THRESHOLD_MAX) ? v : KB_SHAKE_THRESHOLD_DEFAULT;
        g_shake_threshold_loaded = true;
    }
    return g_shake_threshold;
}

void kb_shake_threshold_set(uint8_t v) {
    g_shake_threshold        = (v >= KB_SHAKE_THRESHOLD_MIN && v <= KB_SHAKE_THRESHOLD_MAX) ? v : KB_SHAKE_THRESHOLD_DEFAULT;
    g_shake_threshold_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_SHAKE_THRESHOLD_EEPROM, g_shake_threshold);
}

// ── シェイク判定の厳しさ（反転回数・許容時間。ジェスチャーしきい値と同パターン）──
static uint8_t g_shake_reversals_needed        = 0xEE;
static bool    g_shake_reversals_needed_loaded = false;

uint8_t kb_shake_reversals_get(void) {
    if (!g_shake_reversals_needed_loaded) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_SHAKE_REVERSALS_EEPROM);
        g_shake_reversals_needed = (v >= KB_SHAKE_REVERSALS_MIN && v <= KB_SHAKE_REVERSALS_MAX) ? v : KB_SHAKE_REVERSALS_DEFAULT;
        g_shake_reversals_needed_loaded = true;
    }
    return g_shake_reversals_needed;
}

void kb_shake_reversals_set(uint8_t v) {
    g_shake_reversals_needed        = (v >= KB_SHAKE_REVERSALS_MIN && v <= KB_SHAKE_REVERSALS_MAX) ? v : KB_SHAKE_REVERSALS_DEFAULT;
    g_shake_reversals_needed_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_SHAKE_REVERSALS_EEPROM, g_shake_reversals_needed);
}

static uint8_t g_shake_run_max        = 0xEE;
static bool    g_shake_run_max_loaded = false;

uint16_t kb_shake_run_max_ms_get(void) {
    if (!g_shake_run_max_loaded) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_SHAKE_RUN_MAX_EEPROM);
        g_shake_run_max = (v >= KB_SHAKE_RUN_MAX_MIN && v <= KB_SHAKE_RUN_MAX_MAX) ? v : KB_SHAKE_RUN_MAX_DEFAULT;
        g_shake_run_max_loaded = true;
    }
    return (uint16_t)g_shake_run_max * 10;
}

void kb_shake_run_max_ms_set(uint16_t ms) {
    uint8_t v = (uint8_t)(ms / 10);
    g_shake_run_max        = (v >= KB_SHAKE_RUN_MAX_MIN && v <= KB_SHAKE_RUN_MAX_MAX) ? v : KB_SHAKE_RUN_MAX_DEFAULT;
    g_shake_run_max_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_SHAKE_RUN_MAX_EEPROM, g_shake_run_max);
}

// ── ダブルフリック（方向ごとの発動キー・時間窓・フリック判定しきい値）─────────
static uint16_t g_dflick_key[4]     = {0, 0, 0, 0};
static bool     g_dflick_key_loaded = false;

static void kb_dflick_keys_ensure_loaded(void) {
    if (g_dflick_key_loaded) return;
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t addr = KB_DFLICK_KEY_TABLE_EEPROM + (uint16_t)i * 2;
        uint8_t  hi   = eeprom_read_byte((const uint8_t *)(uintptr_t)addr);
        uint8_t  lo   = eeprom_read_byte((const uint8_t *)(uintptr_t)(addr + 1));
        g_dflick_key[i] = ((uint16_t)hi << 8) | lo;
    }
    g_dflick_key_loaded = true;
}

uint16_t kb_dflick_key_get(uint8_t dir) {
    kb_dflick_keys_ensure_loaded();
    if (dir >= 4) return 0;
    return g_dflick_key[dir];
}

void kb_dflick_key_set(uint8_t dir, uint16_t v) {
    kb_dflick_keys_ensure_loaded();
    if (dir >= 4) return;
    g_dflick_key[dir] = v;
    uint16_t addr     = KB_DFLICK_KEY_TABLE_EEPROM + (uint16_t)dir * 2;
    eeprom_write_byte((uint8_t *)(uintptr_t)addr, (uint8_t)(v >> 8));
    eeprom_write_byte((uint8_t *)(uintptr_t)(addr + 1), (uint8_t)v);
}

static uint8_t g_dflick_window        = 0xEE;
static bool    g_dflick_window_loaded = false;

uint16_t kb_dflick_window_ms_get(void) {
    if (!g_dflick_window_loaded) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_DFLICK_WINDOW_EEPROM);
        g_dflick_window = (v >= KB_DFLICK_WINDOW_MIN && v <= KB_DFLICK_WINDOW_MAX) ? v : KB_DFLICK_WINDOW_DEFAULT;
        g_dflick_window_loaded = true;
    }
    return (uint16_t)g_dflick_window * 10;
}

void kb_dflick_window_ms_set(uint16_t ms) {
    uint8_t v = (uint8_t)(ms / 10);
    g_dflick_window        = (v >= KB_DFLICK_WINDOW_MIN && v <= KB_DFLICK_WINDOW_MAX) ? v : KB_DFLICK_WINDOW_DEFAULT;
    g_dflick_window_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_DFLICK_WINDOW_EEPROM, g_dflick_window);
}

static uint8_t g_dflick_flick_threshold        = 0xEE;
static bool    g_dflick_flick_threshold_loaded = false;

uint8_t kb_dflick_flick_threshold_get(void) {
    if (!g_dflick_flick_threshold_loaded) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_DFLICK_FLICK_THRESHOLD_EEPROM);
        g_dflick_flick_threshold = (v >= KB_DFLICK_FLICK_THRESHOLD_MIN && v <= KB_DFLICK_FLICK_THRESHOLD_MAX) ? v : KB_DFLICK_FLICK_THRESHOLD_DEFAULT;
        g_dflick_flick_threshold_loaded = true;
    }
    return g_dflick_flick_threshold;
}

void kb_dflick_flick_threshold_set(uint8_t v) {
    g_dflick_flick_threshold        = (v >= KB_DFLICK_FLICK_THRESHOLD_MIN && v <= KB_DFLICK_FLICK_THRESHOLD_MAX) ? v : KB_DFLICK_FLICK_THRESHOLD_DEFAULT;
    g_dflick_flick_threshold_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_DFLICK_FLICK_THRESHOLD_EEPROM, g_dflick_flick_threshold);
}

// ── シェイク・ダブルフリックそれぞれの有効/無効（既定ON。ウェーブ有効/無効と同パターン）──
static int8_t g_shake_enable = -1;  // -1=未確認 0=OFF 1=ON

bool kb_shake_enable_get(void) {
    if (g_shake_enable < 0) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_SHAKE_ENABLE_EEPROM);
        g_shake_enable = (v == 1) ? 0 : 1;  // 1のみ明示的なOFF、それ以外は既定ON
    }
    return g_shake_enable != 0;
}

void kb_shake_enable_set(bool v) {
    g_shake_enable = v ? 1 : 0;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_SHAKE_ENABLE_EEPROM, v ? 0 : 1);
}

static int8_t g_dflick_enable = -1;  // -1=未確認 0=OFF 1=ON

bool kb_dflick_enable_get(void) {
    if (g_dflick_enable < 0) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_DFLICK_ENABLE_EEPROM);
        g_dflick_enable = (v == 1) ? 0 : 1;  // 1のみ明示的なOFF、それ以外は既定ON
    }
    return g_dflick_enable != 0;
}

void kb_dflick_enable_set(bool v) {
    g_dflick_enable = v ? 1 : 0;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_DFLICK_ENABLE_EEPROM, v ? 0 : 1);
}

// ── ダブルフリックの最大継続時間（ジェスチャーしきい値と同パターン）──────────
static uint8_t g_dflick_max_duration        = 0xEE;
static bool    g_dflick_max_duration_loaded = false;

uint16_t kb_dflick_max_duration_ms_get(void) {
    if (!g_dflick_max_duration_loaded) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_DFLICK_MAX_DURATION_EEPROM);
        g_dflick_max_duration = (v >= KB_DFLICK_MAX_DURATION_MIN && v <= KB_DFLICK_MAX_DURATION_MAX) ? v : KB_DFLICK_MAX_DURATION_DEFAULT;
        g_dflick_max_duration_loaded = true;
    }
    return (uint16_t)g_dflick_max_duration * 10;
}

void kb_dflick_max_duration_ms_set(uint16_t ms) {
    uint8_t v = (uint8_t)(ms / 10);
    g_dflick_max_duration        = (v >= KB_DFLICK_MAX_DURATION_MIN && v <= KB_DFLICK_MAX_DURATION_MAX) ? v : KB_DFLICK_MAX_DURATION_DEFAULT;
    g_dflick_max_duration_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_DFLICK_MAX_DURATION_EEPROM, g_dflick_max_duration);
}

// ── DPIカーブ（kb_settings.h参照）────────────────────────────────
const uint8_t KB_DPI_CURVE_X[KB_DPI_CURVE_POINT_COUNT] = {0, 16, 32, 48, 64, 80, 96, 112, 127};

static int8_t g_dpi_curve_enable = -1;  // -1=未確認 0=OFF 1=ON

bool kb_dpi_curve_enable_get(void) {
    if (g_dpi_curve_enable < 0) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_DPI_CURVE_ENABLE_EEPROM);
        g_dpi_curve_enable = (v == 1) ? 1 : 0;  // 1のみ明示的なON、それ以外(未書込みの0x00含む)は既定OFF
    }
    return g_dpi_curve_enable != 0;
}

void kb_dpi_curve_enable_set(bool v) {
    g_dpi_curve_enable = v ? 1 : 0;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_DPI_CURVE_ENABLE_EEPROM, v ? 1 : 0);
}

static uint8_t g_dpi_curve_points[KB_DPI_CURVE_POINT_COUNT];
static bool    g_dpi_curve_points_loaded = false;

const uint8_t *kb_dpi_curve_points_get(void) {
    if (!g_dpi_curve_points_loaded) {
        // KB_TRACKBALL_LAYERS_MAGIC_EEPROMと同じ理由（RP2040のEEPROMは未書込み領域が
        // 0x00になるため、目印バイトが無い間は生バイトを信用せず既定のY=Xを返す）。
        uint8_t magic = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_DPI_CURVE_MAGIC_EEPROM);
        if (magic == KB_DPI_CURVE_MAGIC_VALUE) {
            eeprom_read_block(g_dpi_curve_points, (const void *)(uintptr_t)KB_DPI_CURVE_POINTS_EEPROM, KB_DPI_CURVE_POINT_COUNT);
        } else {
            for (uint8_t i = 0; i < KB_DPI_CURVE_POINT_COUNT; i++) g_dpi_curve_points[i] = KB_DPI_CURVE_X[i];
        }
        g_dpi_curve_points_loaded = true;
    }
    return g_dpi_curve_points;
}

static bool g_dpi_curve_lut_valid = false;

void kb_dpi_curve_points_set(const uint8_t *points) {
    memcpy(g_dpi_curve_points, points, KB_DPI_CURVE_POINT_COUNT);
    g_dpi_curve_points_loaded = true;
    eeprom_write_block(g_dpi_curve_points, (void *)(uintptr_t)KB_DPI_CURVE_POINTS_EEPROM, KB_DPI_CURVE_POINT_COUNT);
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_DPI_CURVE_MAGIC_EEPROM, KB_DPI_CURVE_MAGIC_VALUE);
    g_dpi_curve_lut_valid = false;  // 設定が変わったのでルックアップテーブルを作り直す
}

// 単調3次エルミート曲線（Fritsch-Carlsonの簡略版）で5点を滑らかに結ぶ。
// sqrtを使わない代わりに、各区間の接線の傾き比(alpha/beta)を個別に[0,3]へ
// クランプする十分条件を使う（本来の円条件より少し保守的だが、オーバーシュート
// せず単調性が崩れにくい・sqrtf不要で組み込み向き、という判断）。
static uint8_t g_dpi_curve_lut[KB_DPI_CURVE_LUT_SIZE];

static void kb_dpi_curve_rebuild_lut(void) {
    const uint8_t *ys = kb_dpi_curve_points_get();
    const uint8_t  n  = KB_DPI_CURVE_POINT_COUNT;

    float d[KB_DPI_CURVE_POINT_COUNT - 1];  // 区間ごとの平均勾配（割線）
    for (uint8_t i = 0; i < n - 1; i++) {
        float dx = (float)(KB_DPI_CURVE_X[i + 1] - KB_DPI_CURVE_X[i]);
        d[i]     = ((float)ys[i + 1] - (float)ys[i]) / dx;
    }

    float m[KB_DPI_CURVE_POINT_COUNT];  // 各点の接線の傾き
    m[0]     = d[0];
    m[n - 1] = d[n - 2];
    for (uint8_t i = 1; i < n - 1; i++) {
        m[i] = (d[i - 1] + d[i]) / 2.0f;
    }
    for (uint8_t i = 0; i < n - 1; i++) {
        if (d[i] == 0.0f) {
            m[i] = 0.0f;
            m[i + 1] = 0.0f;
            continue;
        }
        float alpha = m[i] / d[i];
        float beta  = m[i + 1] / d[i];
        if (alpha < 0.0f) m[i] = 0.0f;
        else if (alpha > 3.0f) m[i] = 3.0f * d[i];
        if (beta < 0.0f) m[i + 1] = 0.0f;
        else if (beta > 3.0f) m[i + 1] = 3.0f * d[i];
    }

    for (uint16_t x = 0; x < KB_DPI_CURVE_LUT_SIZE; x++) {
        uint8_t seg = n - 2;
        for (uint8_t i = 0; i < n - 1; i++) {
            if (x <= KB_DPI_CURVE_X[i + 1]) { seg = i; break; }
        }
        float x0 = (float)KB_DPI_CURVE_X[seg];
        float x1 = (float)KB_DPI_CURVE_X[seg + 1];
        float h  = x1 - x0;
        float t  = (h > 0.0f) ? ((float)x - x0) / h : 0.0f;
        float t2 = t * t, t3 = t2 * t;
        float h00 = 2 * t3 - 3 * t2 + 1;
        float h10 = t3 - 2 * t2 + t;
        float h01 = -2 * t3 + 3 * t2;
        float h11 = t3 - t2;
        float y   = h00 * ys[seg] + h10 * h * m[seg] + h01 * ys[seg + 1] + h11 * h * m[seg + 1];
        int32_t yi = (int32_t)(y + 0.5f);
        if (yi < 0) yi = 0;
        if (yi > 255) yi = 255;
        g_dpi_curve_lut[x] = (uint8_t)yi;
    }
    g_dpi_curve_lut_valid = true;
}

const uint8_t *kb_dpi_curve_lut_get(void) {
    if (!g_dpi_curve_lut_valid) kb_dpi_curve_rebuild_lut();
    return g_dpi_curve_lut;
}
