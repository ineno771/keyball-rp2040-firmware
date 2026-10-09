// Keyball OLED animation frames — 15fps, SSD1306 128x32 page layout, 512 byte/frame。
// 実データは本人提供（2026-10-01差し替え・追加）。2026-10-09からは圧縮版（frames_packed/、
// tools/pack_anim_frames.pyで生成）を使い、ここで1コマずつ元に戻す。
#include "anim_frames.h"
#include <stdbool.h>
#include <stddef.h>

#include "frames_packed/anim_xmas.h"
#include "frames_packed/anim_hallow.h"
#include "frames_packed/anim_easter.h"
#include "frames_packed/anim_twinkle.h"
#include "frames_packed/anim_gradient.h"
#include "frames_packed/anim_rainbow.h"
#include "frames_packed/anim_breath.h"
#include "frames_packed/anim_swirl.h"
#include "frames_packed/anim_kaleido.h"
#include "frames_packed/anim_heatmap.h"
#include "frames_packed/anim_ripple.h"

#define KB_ANIM_KEYFRAME_INTERVAL 30  // tools/pack_anim_frames.pyのKEYFRAME_INTERVALと同じ値

// PackBits形式のデータpを元に戻してbufへ書く（use_xorなら既存の内容とのXOR）。
static void kb_anim_unpack(const uint8_t *p, const uint8_t *end, uint8_t *buf, bool use_xor) {
    uint16_t pos = 0;
    while (p < end && pos < KB_ANIM_FRAME_BYTES) {
        uint8_t c = *p++;
        if (c < 128) {
            uint16_t n = (uint16_t)c + 1;
            for (uint16_t k = 0; k < n && pos < KB_ANIM_FRAME_BYTES; k++, pos++) {
                buf[pos] = use_xor ? (uint8_t)(buf[pos] ^ p[k]) : p[k];
            }
            p += n;
        } else {
            uint16_t n = (uint16_t)c - 125;
            uint8_t  v = *p++;
            for (uint16_t k = 0; k < n && pos < KB_ANIM_FRAME_BYTES; k++, pos++) {
                buf[pos] = use_xor ? (uint8_t)(buf[pos] ^ v) : v;
            }
        }
    }
}

static void kb_anim_apply(const kb_anim_t *anim, uint16_t k, uint8_t *buf) {
    const uint8_t *p   = anim->data + anim->offsets[k];
    const uint8_t *end = anim->data + anim->offsets[k + 1];
    if (p >= end) return;
    bool use_xor = (*p == 1);
    kb_anim_unpack(p + 1, end, buf, use_xor);
}

const uint8_t *kb_anim_frame(const kb_anim_t *anim, uint16_t idx) {
    static uint8_t          buf[KB_ANIM_FRAME_BYTES];
    static const kb_anim_t *cur_anim = NULL;
    static uint16_t         cur_idx  = 0;

    if (idx >= anim->frames) idx = (uint16_t)(idx % anim->frames);
    if (anim == cur_anim && idx == cur_idx) return buf;

    // 同じアニメーションで、直前のコマから同じキーフレーム区間内を先へ進む時は続きから。
    // それ以外（巻き戻り・区間をまたぐ・別のアニメーション）は区間先頭のキーフレームから。
    uint16_t key   = (uint16_t)(idx - (idx % KB_ANIM_KEYFRAME_INTERVAL));
    uint16_t start = key;
    if (anim == cur_anim && cur_idx >= key && cur_idx < idx) {
        start = (uint16_t)(cur_idx + 1);
    }
    for (uint16_t k = start; k <= idx; k++) {
        kb_anim_apply(anim, k, buf);
    }
    cur_anim = anim;
    cur_idx  = idx;
    return buf;
}
