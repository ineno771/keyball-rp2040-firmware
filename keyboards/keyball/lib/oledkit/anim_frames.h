// LEDエフェクト連動OLEDアニメーション（クリスマス/ハロウィン/イースター/トゥインクル）。
// 実体（フレームデータ本体）は anim_frames.c 側。90フレーム/15fps(6秒ループ)、
// 各フレームは128x32のOLED全画面を1枚のバイト列(512byte, SSD1306ページ形式)で
// 表現しており、oled_write_raw_P()でそのままフレームバッファに転送できる。
#pragma once

#include <stdint.h>
#include "progmem.h"

#define KB_ANIM_FRAME_COUNT 90
#define KB_ANIM_FRAME_BYTES 512
#define KB_ANIM_FRAME_MS 67  // 1000ms / 15fps

extern const uint8_t anim_xmas[KB_ANIM_FRAME_COUNT][KB_ANIM_FRAME_BYTES];
extern const uint8_t anim_hallow[KB_ANIM_FRAME_COUNT][KB_ANIM_FRAME_BYTES];
extern const uint8_t anim_easter[KB_ANIM_FRAME_COUNT][KB_ANIM_FRAME_BYTES];
extern const uint8_t anim_twinkle[KB_ANIM_FRAME_COUNT][KB_ANIM_FRAME_BYTES];
