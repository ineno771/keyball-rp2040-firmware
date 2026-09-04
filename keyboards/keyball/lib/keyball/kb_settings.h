// Copyright 2024 keyball-custom contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#include <stdbool.h>

// ── EEPROM配置についての重要な注意（2026-09-04発覚・移動） ──────────────
// 以前はTD config=0x0200、kb_settings=0x0240、マクロ=0x0250…という配置だったが、
// これはQMK標準の動的キーマップ（Keyball LinkのSET_KEYCODEで実際に書き込まれる、
// DYNAMIC_KEYMAP_LAYER_COUNT×MATRIX_ROWS×MATRIX_COLS×2バイト分の領域）の実使用範囲
// 0x0025〜0x0324（8層×8行×6列×2=768バイト、EECONFIG_SIZE=37バイトの直後から）と
// 完全に重複しており、キー割り当てのたびにkb_settings本体やマクロ領域前半が静かに
// 破壊される実害あるバグだった（「dynamic_keymapは先頭〜約0x01C0までしか使わない」
// という当初の見積もりが誤りだった）。0x0800以降に全面移動して解消した。
// 新配置: TD config(0x0800-0x083F,64B) → kb_settings(0x0840-0x084F,16B)
//        → マクロ(0x0850-0x09DF,400B) → 本ファイル後半の各種設定(0x09E0-)
// 実測方法: nvm_dynamic_keymap.cに一時的な_Static_assertを仕込み、qmk compileの
// エラーメッセージから二分探索で実際の境界値を確認した（HANDOFF.md参照）。
#define KB_SETTINGS_EEPROM_BASE  0x0840
#define KB_SETTINGS_DEFAULT_TT   200  // デフォルト Tapping Term (ms)

// dynamic_keymapとの衝突をコンパイル時に検知する。DYNAMIC_KEYMAP_EEPROM_ADDR自体は
// nvm_dynamic_keymap.c内部限定のマクロで外部から参照できないため、同じ計算式
// （EECONFIG_SIZE + レイヤー数×行×列×2）をここで再現している。EECONFIG_SIZEの実測値
// (37)はVIA_ENABLE=off・EECONFIG_KB/USER_DATA_SIZE=0が前提。QMK更新でeeprom_core_tの
// サイズが変わった場合はこの数値がずれる可能性があるので、疑わしい場合は上記の実測
// 方法で確認し直すこと。
#define KB_DYNAMIC_KEYMAP_EEPROM_ADDR_ASSUMED 37
#define KB_DYNAMIC_KEYMAP_EEPROM_END_ASSUMED \
    (KB_DYNAMIC_KEYMAP_EEPROM_ADDR_ASSUMED + (DYNAMIC_KEYMAP_LAYER_COUNT) * (MATRIX_ROWS) * (MATRIX_COLS) * 2)
_Static_assert(KB_SETTINGS_EEPROM_BASE >= KB_DYNAMIC_KEYMAP_EEPROM_END_ASSUMED + 512,
               "kb_settings EEPROMがdynamic_keymap領域と衝突またはマージンが不足しています。KB_SETTINGS_EEPROM_BASEを見直してください。");

// ジェスチャーのデフォルト割り当て（macブラウザ標準・修飾子付きキーコード）
// 0x0800=LGUI(Cmd), 0x0200=LSFT, KC_LBRC=0x2F, KC_RBRC=0x30
#define KB_GESTURE_DEFAULT_UP    0x0A2F  // Cmd+Shift+[ 前のタブ
#define KB_GESTURE_DEFAULT_DOWN  0x0A30  // Cmd+Shift+] 次のタブ
#define KB_GESTURE_DEFAULT_LEFT  0x082F  // Cmd+[ 戻る
#define KB_GESTURE_DEFAULT_RIGHT 0x0830  // Cmd+] 進む

typedef struct {
    uint16_t tapping_term;   // 50-1000ms
    uint8_t  flags;          // KB_FLAG_* ビットフィールド
    uint8_t  aml_layer;      // 自動マウスレイヤーの対象レイヤー（0-7）
    uint16_t aml_timeout;    // 自動マウスレイヤーのタイムアウト(ms)
    uint8_t  aml_threshold;  // 自動マウスレイヤーの発動しきい値（移動量。小さいほど敏感）
    uint8_t  gesture_tap;    // ジェスチャーキーをタップした時に送る基本キーコード（0=なし=長押し専用）
    uint16_t gesture[4];     // ジェスチャー割り当て [0]上 [1]下 [2]左 [3]右（0=未設定→デフォルト）
} __attribute__((packed)) kb_settings_t;

#define KB_FLAG_AUTO_SHIFT       (1 << 0)
#define KB_FLAG_COMBO            (1 << 1)
#define KB_FLAG_PERMISSIVE_HOLD  (1 << 2)
#define KB_FLAG_RETRO_TAPPING    (1 << 3)
#define KB_FLAG_SCROLL_INV_V     (1 << 4)  // 縦スクロール反転
#define KB_FLAG_SCROLL_INV_H     (1 << 5)  // 横スクロール反転
#define KB_FLAG_AML_DISABLE      (1 << 6)  // 自動マウスレイヤー無効（0=有効・後方互換）

// EEPROMから読み込む（初回のみ; 以降はRAMキャッシュを返す）
kb_settings_t kb_settings_get(void);

// EEPROMに書き込みRAMキャッシュも更新する
void kb_settings_set(const kb_settings_t *s);

// ── トラックボール動作レイヤー（kb_settings構造体は満杯のため、マクロ領域の
//    直後 0x09E0- に格納）──
#define KB_SCROLL_LAYER_EEPROM   0x09E0  // スクロールレイヤー保存先
#define KB_GESTURE_LAYER_EEPROM  0x09E1  // ジェスチャーレイヤー保存先
#define KB_GESTURE_TH_H_EEPROM  0x09E2  // ジェスチャー横方向しきい値保存先
#define KB_GESTURE_TH_V_EEPROM  0x09E3  // ジェスチャー縦方向しきい値保存先
#define KB_PRECISION_DIV_EEPROM   0x09E4  // 超低速モードの分周値保存先
#define KB_PRECISION_LAYER_EEPROM 0x09E5  // 超低速モードの連動レイヤー保存先
#define KB_LAYER_NONE            0xFE    // 「なし」を表す値（0xFF=未初期化と区別）

// スクロールレイヤー（0-7=そのレイヤーでスクロール / KB_LAYER_NONE=無効。既定3）
uint8_t kb_scroll_layer_get(void);
void    kb_scroll_layer_set(uint8_t v);

#ifdef GESTURE_ENABLE
// ジェスチャーレイヤー（0-7=そのレイヤーでジェスチャー / KB_LAYER_NONE=なし。既定なし）
uint8_t kb_gesture_layer_get(void);
void    kb_gesture_layer_set(uint8_t v);

// ジェスチャー発動しきい値（移動量の累積。小さいほど敏感。既定50、範囲10-200）
// 横方向(左右)・縦方向(上下)を別々に持つ。指の動かし方の癖に合わせて片方だけ調整できる。
#define KB_GESTURE_TH_DEFAULT 50
#define KB_GESTURE_TH_MIN     10
#define KB_GESTURE_TH_MAX     200
uint8_t kb_gesture_th_h_get(void);
void    kb_gesture_th_h_set(uint8_t v);
uint8_t kb_gesture_th_v_get(void);
void    kb_gesture_th_v_set(uint8_t v);
#endif

// 超低速（精密作業）モードのCPI分周値（押している間、CPIをこの値で割る。既定4、範囲2-5）
// 上限は5: 実CPIは100刻みが下限のため、デフォルトCPI(500)ではこれ以上大きくしても
// 100CPIに張り付くだけで差が出ない（本人判断で5を上限に固定）。
#define KB_PRECISION_DIV_DEFAULT 4
#define KB_PRECISION_DIV_MIN     2
#define KB_PRECISION_DIV_MAX     5
uint8_t kb_precision_div_get(void);
void    kb_precision_div_set(uint8_t v);

// 超低速モードの連動レイヤー（0-7=そのレイヤーで自動的に超低速モード / KB_LAYER_NONE=なし。既定なし）
// PRC_MOキーとは独立に働き、どちらか一方でも条件を満たせば超低速モードになる。
uint8_t kb_precision_layer_get(void);
void    kb_precision_layer_set(uint8_t v);

// ── レイヤー連動LED（レイヤーごとに異なる光り方を設定できる機能）──────────
// 有効フラグ1バイト + レイヤー1-7それぞれ6バイトのテーブル（レイヤー0は
// 通常のLED設定＝GET/SET_LEDの値をそのまま使うので対象外）。
#define KB_LAYER_LED_ENABLE_EEPROM 0x09E6  // 機能そのものの有効/無効
#define KB_LAYER_LED_TABLE_EEPROM  0x09E7  // レイヤー別LED設定テーブル先頭（0x09E7-0x0A10、42バイト）
#define KB_LAYER_LED_MAX_LAYER     7       // 対象レイヤーの最大値（1-7）
#define KB_LAYER_LED_ENTRY_SIZE    6        // 1レイヤーあたりのバイト数

typedef struct {
    uint8_t enabled;    // このレイヤーで専用の光り方を使うか（0/1）
    uint8_t effect_id;  // GET/SET_LEDと同じエフェクトID体系
    uint8_t hue;
    uint8_t sat;
    uint8_t val;
    uint8_t speed;
} __attribute__((packed)) kb_layer_led_t;

// レイヤー連動LED機能そのものの有効/無効（既定: 無効）
bool kb_layer_led_enable_get(void);
void kb_layer_led_enable_set(bool v);

// レイヤーN（1-KB_LAYER_LED_MAX_LAYER）のLED設定を取得・変更する（既定: enabled=0）
kb_layer_led_t kb_layer_led_get(uint8_t layer);
void           kb_layer_led_set(uint8_t layer, const kb_layer_led_t *cfg);

// 通常（レイヤー0）のLED設定。RGBLIGHT本体のEEPROM機能には頼らず、ここで完全に
// 独立管理する。理由: レイヤー連動LEDが有効な間、RGBLIGHT本体の「現在の表示」は
// レイヤーのオーバーライドに上書きされているため、rgblight_get_*()を読んでも
// 本来の「通常」の設定は取得できない（オーバーライド中にSET_LEDされた場合、
// 元に戻った時にオーバーライド前の古い値へ戻ってしまう事故が起きていた）。
// SET_LEDのたびに必ずここへ保存し、実際に画面へ反映するかどうかは
// 呼び出し側（kb_hid.c）がkeyball_layer_led_overriding()を見て判断する。
typedef struct {
    uint8_t effect_id;
    uint8_t hue;
    uint8_t sat;
    uint8_t val;
    uint8_t speed;
} __attribute__((packed)) kb_led_config_t;

#define KB_LED_CONFIG_EEPROM 0x0A11  // 5バイト（0x0A11-0x0A15）
kb_led_config_t kb_led_config_get(void);
void            kb_led_config_set(const kb_led_config_t *cfg);
