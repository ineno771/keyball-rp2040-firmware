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
