// LEDエフェクト連動OLEDアニメーション。実体（フレームデータ本体）はanim_frames.c側で
// frames/anim_*.hを#includeして定義する。各フレームは128x32のOLED全画面を1枚のバイト列
// (512byte, SSD1306ページ形式)で表現しており、oled_write_raw_P()でそのままフレーム
// バッファに転送できる。15fps固定だが、フレーム数はシーンごとに異なる（本人提供データの
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

extern const uint8_t anim_xmas[KB_ANIM_XMAS_FRAMES][KB_ANIM_FRAME_BYTES];
extern const uint8_t anim_hallow[KB_ANIM_HALLOW_FRAMES][KB_ANIM_FRAME_BYTES];
extern const uint8_t anim_easter[KB_ANIM_EASTER_FRAMES][KB_ANIM_FRAME_BYTES];
extern const uint8_t anim_twinkle[KB_ANIM_TWINKLE_FRAMES][KB_ANIM_FRAME_BYTES];
extern const uint8_t anim_gradient[KB_ANIM_GRADIENT_FRAMES][KB_ANIM_FRAME_BYTES];
extern const uint8_t anim_rainbow[KB_ANIM_RAINBOW_FRAMES][KB_ANIM_FRAME_BYTES];
extern const uint8_t anim_breath[KB_ANIM_BREATH_FRAMES][KB_ANIM_FRAME_BYTES];
extern const uint8_t anim_swirl[KB_ANIM_SWIRL_FRAMES][KB_ANIM_FRAME_BYTES];
extern const uint8_t anim_kaleido[KB_ANIM_KALEIDO_FRAMES][KB_ANIM_FRAME_BYTES];
extern const uint8_t anim_heatmap[KB_ANIM_HEATMAP_FRAMES][KB_ANIM_FRAME_BYTES];
extern const uint8_t anim_ripple[KB_ANIM_RIPPLE_FRAMES][KB_ANIM_FRAME_BYTES];
