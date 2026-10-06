/*
Copyright 2021 @Yowkees
Copyright 2022 MURAOKA Taro (aka KoRoN, @kaoriya)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include QMK_KEYBOARD_H

#include "lib/keyball/keyball.h"

//////////////////////////////////////////////////////////////////////////////

// clang-format off
matrix_row_t matrix_mask[MATRIX_ROWS] = {
    0b01110111,
    0b01110111,
    0b01110111,
    0b11110111,
    0b11110111,
    0b01110111,
    0b01110111,
    0b01110111,
    0b11110111,
    0b11110111,
};
// clang-format on

#ifdef RGB_MATRIX_ENABLE
// LED配置（2026-10-05、本人が左手ボールの実機で「LED位置実測（開発用）」を使い1個ずつ
// 確認した配線情報に基づく確定値）。キー名は左手ボール時のもの（ボール搭載側=L）。
//
// ボール搭載側（34個、idx0-33）: アンダーグロー7個(idx0-6)→キー連動27個(idx7-33)。
//   キー連動は外側の列から「列0(L00,L10,L20,L30,L40)→列1(…L41)→列2(…L32)→列3(…L33)→
//   列4(…L34)→列5(…L35)→L36」。最下段のL45・L46はLEDなし（近くにアンダーグローidx0）。
// 非搭載側（37個、idx34-70）: キー連動29個(idx34-62)→アンダーグロー8個(idx63-70)。
//   キー連動は内側から「R36→列5(R05,R15,R25,R35)→列4(…R34)→列3(…R33,R43)→列2(…R42)→
//   列1(…R41)→列0(…R40)」。最下段のR44〜R46はLEDなし。
//
// matrix_coの行0-4=ボール搭載側・行5-9=非搭載側（Keyball+・44と同じ規則。ボールが基板本来の
// is_keyboard_left()=false側にある場合はkeymap.cが行を入れ替える）。duplex matrixのため
// 物理列3以降はマトリクス列+1（列3は欠番）。物理座標はキー配列からの概算値（アンダーグローは
// 本人申告の位置から）。x方向はKeyball39/44/+と同じく「実物の右ほどxが小さい」向き（ジェスチャー
// ウェーブの左右判定がこの向き前提）。この表は左手ボールの実機で作ったため、ボール搭載側
// (左手)の外側の列がx大・非搭載側(右手)の外側の列がx小（2026-10-06、カーソルの向き修正に伴い
// 左右の流れが逆になるのを防ぐため反転）。右手ボールで使う場合は左右の流れが逆になる（保留中）。
led_config_t g_led_config = {
    {
        // キーマトリクス(10行 x 8列) → LEDインデックス
        {      7,     12,     17, NO_LED,     21,     25,     29, NO_LED },
        {      8,     13,     18, NO_LED,     22,     26,     30, NO_LED },
        {      9,     14,     19, NO_LED,     23,     27,     31, NO_LED },
        {     10,     15,     20, NO_LED,     24,     28,     32,     33 },
        {     11,     16, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        {     58,     53,     48, NO_LED,     43,     39,     35, NO_LED },
        {     59,     54,     49, NO_LED,     44,     40,     36, NO_LED },
        {     60,     55,     50, NO_LED,     45,     41,     37, NO_LED },
        {     61,     56,     51, NO_LED,     46,     42,     38,     34 },
        {     62,     57,     52, NO_LED,     47, NO_LED, NO_LED, NO_LED },
    },
    {
        // LEDインデックス(0-70) → 物理座標(x,y)
        {164,49}, {170,42}, {182,42}, {224,49}, {224,21}, {206,0}, {182,0}, {224,0}, {224,14}, {224,28},
        {224,42}, {224,56}, {212,0}, {212,14}, {212,28}, {212,42}, {212,56}, {200,0}, {200,14}, {200,28},
        {200,42}, {188,0}, {188,14}, {188,28}, {188,42}, {176,0}, {176,14}, {176,28}, {176,42}, {164,0},
        {164,14}, {164,28}, {164,42}, {152,42}, {72,42}, {60,0}, {60,14}, {60,28}, {60,42}, {48,0},
        {48,14}, {48,28}, {48,42}, {36,0}, {36,14}, {36,28}, {36,42}, {36,56}, {24,0}, {24,14},
        {24,28}, {24,42}, {24,56}, {12,0}, {12,14}, {12,28}, {12,42}, {12,56}, {0,0}, {0,14},
        {0,28}, {0,42}, {0,56}, {42,0}, {18,0}, {0,21}, {0,49}, {24,49}, {48,49}, {60,49},
        {72,49},
    },
    {
        // LEDインデックス → フラグ
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_UNDERGLOW,
    },
};
#endif

void keyball_on_adjust_layout(keyball_adjust_t v) {
#ifdef RGBLIGHT_ENABLE
    // adjust RGBLIGHT's clipping and effect ranges
    uint8_t lednum_this = keyball.this_have_ball ? 34 : 37;
    uint8_t lednum_that = !keyball.that_enable ? 0 : keyball.that_have_ball ? 34 : 37;
    rgblight_set_clipping_range(is_keyboard_left() ? 0 : lednum_that, lednum_this);
    rgblight_set_effect_range(0, lednum_this + lednum_that);
#endif
}
