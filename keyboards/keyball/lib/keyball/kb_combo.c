// Copyright 2024 keyball-custom contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "kb_combo.h"
#include "eeprom.h"
#include "process_combo.h"
#include <string.h>

// kb_settings.h(0x0840-)とは別に確保した専用領域。kb_settings.hの集中コメント
// （EEPROM配置の一覧）にもこの範囲(0x0A54-0x0AA3、8スロット×10バイト=80バイト)
// を予約済みとして記載しているので、新しい設定を追加する際はそちらを確認すること。
#define KB_COMBO_EEPROM_BASE 0x0A54

// keymap.c側で実体を定義しているQMK COMBO機能の本体。ここから読み書きする。
extern combo_t key_combos[KB_COMBO_SLOT_COUNT];

// key_combos[i].keysが指す先の実体（RAM上の可変配列）。末尾は必ず0(COMBO_END)にする。
static uint16_t g_combo_keybuf[KB_COMBO_SLOT_COUNT][KB_COMBO_MAX_KEYS + 1];

kb_combo_slot_t kb_combo_get(uint8_t idx) {
    kb_combo_slot_t slot;
    if (idx >= KB_COMBO_SLOT_COUNT) {
        memset(&slot, 0, sizeof(slot));
        return slot;
    }
    eeprom_read_block(&slot, (void *)(uintptr_t)(KB_COMBO_EEPROM_BASE + (uint16_t)idx * sizeof(slot)), sizeof(slot));
    return slot;
}

static void kb_combo_apply(uint8_t idx, const kb_combo_slot_t *slot) {
    for (uint8_t k = 0; k < KB_COMBO_MAX_KEYS; k++) {
        g_combo_keybuf[idx][k] = slot->keys[k];
    }
    g_combo_keybuf[idx][KB_COMBO_MAX_KEYS] = 0;  // COMBO_END
    key_combos[idx].keys    = g_combo_keybuf[idx];
    key_combos[idx].keycode = slot->keycode;
}

void kb_combo_set(uint8_t idx, const kb_combo_slot_t *slot) {
    if (idx >= KB_COMBO_SLOT_COUNT) return;
    eeprom_write_block(slot, (void *)(uintptr_t)(KB_COMBO_EEPROM_BASE + (uint16_t)idx * sizeof(*slot)), sizeof(*slot));
    kb_combo_apply(idx, slot);
}

void kb_combo_init(void) {
    for (uint8_t i = 0; i < KB_COMBO_SLOT_COUNT; i++) {
        kb_combo_slot_t slot = kb_combo_get(i);
        kb_combo_apply(i, &slot);
    }
}
