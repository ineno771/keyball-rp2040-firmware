// Copyright 2024 keyball-custom contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>

// コンボ（2026-09-10〜。複数キー同時押しで別のキーを発動）。
// QMKのCOMBO機能はcombo_t.keysが「PROGMEM上の固定配列」であることを前提に
// 設計されているが、pgm_read_word()はRP2040(ChibiOS)では単なるポインタ参照
// （platforms/progmem.h参照。AVRのようなPROGMEM専用アドレス空間が無いため）
// なので、RAM上の可変配列を指すようにしてもそのまま動く。これを利用して、
// keymap.c側のkey_combos[]の実体（各スロットのkeys配列とkeycode）をEEPROMから
// 読み込んで書き換えることで、Web UIから組み合わせを編集できるようにしている。
#define KB_COMBO_SLOT_COUNT 8  // 設定できるコンボの数
#define KB_COMBO_MAX_KEYS   4  // 1コンボあたりの同時押しキー数の上限

typedef struct {
    uint16_t keys[KB_COMBO_MAX_KEYS];  // 同時押しするキー（0=未使用。先頭0個から順に詰めて設定すること）
    uint16_t keycode;                  // 発動するキー（0=このスロットは無効）
} __attribute__((packed)) kb_combo_slot_t;

// スロットN（0-7）の設定を取得・変更する。設定変更は即座に実行中のcombo定義にも反映される
// （再起動不要）。
kb_combo_slot_t kb_combo_get(uint8_t idx);
void            kb_combo_set(uint8_t idx, const kb_combo_slot_t *slot);

// 起動時にEEPROMから全スロットを読み込み、keymap.c側のkey_combos[]へ反映する。
// keyboard_post_init_user()から一度だけ呼ぶこと。
void kb_combo_init(void);
