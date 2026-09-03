// Copyright 2024 keyball-custom contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "kb_settings.h"
#include "eeprom.h"
#include <stdbool.h>
#include <string.h>

// 注意: KB_LAYER_LED_TABLE_EEPROM (0x03E7) 〜 +42バイト(0x0410)は、既存のscroll/gesture/
// precision layer設定（0x03E0-0x03E5）に続く「空き領域」として使っている。dynamic keymap
// 本体のEEPROM開始位置はQMKコア内部（nvm_dynamic_keymap.c）でのみ定義されビルド時に
// 参照できないため、コンパイル時の衝突検知はできない。実機で書き込み・読み出しが
// 正常に行えることを都度確認すること（既存の0x03E0台の追加時と同様の前提）。

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
        // ジェスチャー未初期化(0xFFFF or 0)ならデフォルト割り当てへ（旧FWからの移行も安全に）
        if (g_cache.gesture[0] == 0xFFFF || g_cache.gesture[0] == 0) {
            g_cache.gesture[0] = KB_GESTURE_DEFAULT_UP;
            g_cache.gesture[1] = KB_GESTURE_DEFAULT_DOWN;
            g_cache.gesture[2] = KB_GESTURE_DEFAULT_LEFT;
            g_cache.gesture[3] = KB_GESTURE_DEFAULT_RIGHT;
        }
        // ジェスチャーのタップキーが未初期化(0xFF)なら「なし」に補正
        if (g_cache.gesture_tap == 0xFF) g_cache.gesture_tap = 0;
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

// ── スクロールレイヤー（構造体外・EEPROM末尾に1バイト保存）────────────
static uint8_t g_scroll_layer  = 0xEE;
static bool    g_scroll_loaded = false;

uint8_t kb_scroll_layer_get(void) {
    if (!g_scroll_loaded) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_SCROLL_LAYER_EEPROM);
        if (v <= 7)                  g_scroll_layer = v;             // 0-7 = そのレイヤー
        else if (v == KB_LAYER_NONE) g_scroll_layer = KB_LAYER_NONE; // 明示的に「なし」
        else                         g_scroll_layer = 3;            // 0xFF未初期化 → 既定レイヤー3
        g_scroll_loaded = true;
    }
    return g_scroll_layer;
}

void kb_scroll_layer_set(uint8_t v) {
    g_scroll_layer  = (v <= 7) ? v : KB_LAYER_NONE;
    g_scroll_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_SCROLL_LAYER_EEPROM, g_scroll_layer);
}

#ifdef GESTURE_ENABLE
// ── ジェスチャーレイヤー（同上。未初期化/0xFE は「なし」）──────────────
static uint8_t g_gesture_layer  = 0xEE;
static bool    g_gesture_loaded = false;

uint8_t kb_gesture_layer_get(void) {
    if (!g_gesture_loaded) {
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_GESTURE_LAYER_EEPROM);
        g_gesture_layer = (v <= 7) ? v : KB_LAYER_NONE;  // 0-7=レイヤー / それ以外=なし
        g_gesture_loaded = true;
    }
    return g_gesture_layer;
}

void kb_gesture_layer_set(uint8_t v) {
    g_gesture_layer  = (v <= 7) ? v : KB_LAYER_NONE;
    g_gesture_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_GESTURE_LAYER_EEPROM, g_gesture_layer);
}

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
        uint8_t v = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_PRECISION_LAYER_EEPROM);
        g_precision_layer = (v <= 7) ? v : KB_LAYER_NONE;  // 0-7=レイヤー / それ以外=なし
        g_precision_layer_loaded = true;
    }
    return g_precision_layer;
}

void kb_precision_layer_set(uint8_t v) {
    g_precision_layer        = (v <= 7) ? v : KB_LAYER_NONE;
    g_precision_layer_loaded = true;
    eeprom_write_byte((uint8_t *)(uintptr_t)KB_PRECISION_LAYER_EEPROM, g_precision_layer);
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
