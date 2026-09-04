// Copyright 2024 keyball-custom contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "td_config.h"
#include "eeprom.h"
#include <string.h>

// タップダンス設定をEEPROMの 0x0800 番地以降に保存
// 旧配置(0x0200)は「dynamic_keymapは先頭〜約0x01C0まで」という誤った前提に基づいており、
// 実際にはdynamic_keymapは0x0025〜0x0324（8レイヤー×8行×6列×2バイト=768バイト）を使うため
// 完全に衝突していた（2026-09-04発覚）。0x0800以降はdynamic_keymapは絶対に使わない安全な領域。
// kb_settings.h側の集中コメント・衝突防止Static_assertも参照。
#define TD_EEPROM_BASE 0x0800

td_slot_t td_config_get(uint8_t idx) {
    td_slot_t slot;
    if (idx >= TD_SLOT_COUNT) {
        memset(&slot, 0, sizeof(slot));
        return slot;
    }
    eeprom_read_block(&slot,
        (void *)(uintptr_t)(TD_EEPROM_BASE + idx * sizeof(td_slot_t)),
        sizeof(td_slot_t));
    return slot;
}

void td_config_set(uint8_t idx, const td_slot_t *slot) {
    if (idx >= TD_SLOT_COUNT) return;
    eeprom_write_block(slot,
        (void *)(uintptr_t)(TD_EEPROM_BASE + idx * sizeof(td_slot_t)),
        sizeof(td_slot_t));
}
