// LEDエフェクト連動OLEDアニメーション。各フレームは128x32のOLED全画面を1枚のバイト列
// (512byte, SSD1306ページ形式)で表現する。
// 2026-10-09: ファームウェア容量削減のため、データは圧縮して持つ（約690KB→約270KB）。
// 元データはframes/anim_*.h（非圧縮、ビルドには使わない）、圧縮版はframes_packed/anim_*.h
// （tools/pack_anim_frames.pyで生成）。kb_anim_frame()で1コマずつ元に戻して使う。15fps固定だが、フレーム数はシーンごとに異なる（本人提供データの
// 都合。タイピングヒートマップ360フレーム・リップル180フレーム、他は90フレーム）ため、
// KB_ANIM_FRAME_COUNTのような共通定数ではなく、シーンごとの専用定数(KB_ANIM_*_FRAMES)を
// 使うこと。
#pragma once

#include <stdint.h>
#include "progmem.h"

#define KB_ANIM_FRAME_BYTES 512
#define KB_ANIM_FRAME_MS 67  // 1000ms / 15fps

#define KB_ANIM_XMAS_FRAMES     90
#define KB_ANIM_HALLOW_FRAMES   90
#define KB_ANIM_EASTER_FRAMES   90
#define KB_ANIM_TWINKLE_FRAMES  90
#define KB_ANIM_GRADIENT_FRAMES 90
#define KB_ANIM_RAINBOW_FRAMES  90
#define KB_ANIM_BREATH_FRAMES   90
#define KB_ANIM_SWIRL_FRAMES    90
#define KB_ANIM_KALEIDO_FRAMES  90
#define KB_ANIM_HEATMAP_FRAMES  360
#define KB_ANIM_RIPPLE_FRAMES   180

// 圧縮済みアニメーション1本分。data/offsetsの形式はtools/pack_anim_frames.py参照。
typedef struct {
    const uint8_t  *data;
    const uint32_t *offsets;  // コマkのデータはdata[offsets[k]]〜data[offsets[k+1]-1]
    uint16_t        frames;
} kb_anim_t;

extern const kb_anim_t anim_xmas;
extern const kb_anim_t anim_hallow;
extern const kb_anim_t anim_easter;
extern const kb_anim_t anim_twinkle;
extern const kb_anim_t anim_gradient;
extern const kb_anim_t anim_rainbow;
extern const kb_anim_t anim_breath;
extern const kb_anim_t anim_swirl;
extern const kb_anim_t anim_kaleido;
extern const kb_anim_t anim_heatmap;
extern const kb_anim_t anim_ripple;

// animのidx番目のコマを元に戻し、512バイトの画像を返す（内部の作業用バッファを指す。
// 次に呼ぶまで有効）。直前に元に戻したコマから先へ進む時は差分だけ適用するので軽い。
const uint8_t *kb_anim_frame(const kb_anim_t *anim, uint16_t idx);
