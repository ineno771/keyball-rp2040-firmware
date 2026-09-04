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
    0b00011111,
    0b00011111,
    0b00011111,
    0b00111111,
    0b00011111,
    0b00011111,
    0b00011111,
    0b00111111,
};
// clang-format on

#ifdef RGB_MATRIX_ENABLE
// LED物理配置（2026-09-04、v2: LAYOUT macroの実キー構造とビルドガイドの配線順番号を
// 突き合わせて再構成）。
//
// 発光順（実測済み）: ボール側(22個)のラベル1→22が先、続けて無ボール側(24個)の
// ラベル1→24。ボール側はアンダーグロー=ラベル1-6・キー連動=ラベル7-22（16個）、
// 無ボール側はキー連動=ラベル1-18（18個）・アンダーグロー=ラベル19-24。
//
// v1（列×行の総当たりグリッド）は写真の列区切りを目分量で読んだだけで実際の
// キー配置と対応しておらず、波紋が「バラバラ」に見える原因になっていた。
// v2ではLAYOUT_left_ball/right_ball相当の実キー構造に沿って番号を再割当て。
// v3（本人確認済み）: ボール側は親指キーが2つ（無ボール側は3つ）で、内側(col0側)の
// 親指キーが存在しない。col0の親指位置(row3,col0)は専用LEDを持たず、代わりに
// 近くにあるアンダーグローLED（ラベル1,2＝idx0,1）を起点にする。
// col3の中段(row1,row2)は依然として実際の対応が未確認（トラックボール周辺で
// LEDが足りていない箇所の推測）。当面はcol2の同じ行のLEDを流用している。
//
// matrix_coの行0-3=ボール側・行4-7=無ボール側という割り当ても未検証の仮定。
// 波紋を実際に押してみて、押した手と逆側のLEDが光るようであれば行0-3と4-7を
// 入れ替えること（もしくはis_keyboard_left()寄りの判定に修正すること）。
led_config_t g_led_config = {
    {
        // キーマトリクス(8行 x 6列) → LEDインデックス
        { 6, 10, 13, 16, 17, NO_LED },
        { 7, 11, 14, 14, 18, NO_LED },
        { 8, 12, 15, 15, 19, NO_LED },
        { 0, NO_LED, NO_LED, NO_LED, 20, 21 },
        { 36, 32, 28, 25, 22, NO_LED },
        { 37, 33, 29, 26, 23, NO_LED },
        { 38, 34, 30, 27, 24, NO_LED },
        { 39, NO_LED, NO_LED, NO_LED, 35, 31 },
    },
    {
        // LEDインデックス(0-45) → 物理座標(x:0-224, y:0-64)。
        // via.json（VIA用KLEレイアウト定義。実際の列ずれ・親指キーの傾き等を含む本物の
        // 座標）を224x64にスケーリングして算出（参考記事 note.com/yawatajunk/n/n01052b99504f
        // の手法に沿って修正。keyboard.jsonの単純グリッドより物理形状に忠実）。
        {0,40}, {0,40}, {70,3}, {0,7}, {0,29}, {70,25}, {0,7}, {0,18}, {0,29}, {0,35},
        {18,3}, {18,14}, {18,25}, {35,0}, {35,11}, {35,22}, {53,1}, {70,3}, {70,14}, {70,25},
        {94,62}, {112,64}, {154,3}, {154,14}, {154,25}, {171,1}, {171,13}, {171,24}, {189,0}, {189,11},
        {189,22}, {178,48}, {206,3}, {206,14}, {206,25}, {196,62}, {224,7}, {224,18}, {224,29}, {224,40},
        {224,7}, {154,3}, {224,29}, {154,25}, {224,40}, {178,48},
    },
    {
        // LEDインデックス → フラグ
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
    },
};
#endif

void keyball_on_adjust_layout(keyball_adjust_t v) {
#ifdef RGBLIGHT_ENABLE
    // adjust RGBLIGHT's clipping and effect ranges
    uint8_t lednum_this = keyball.this_have_ball ? 22 : 24;
    uint8_t lednum_that = !keyball.that_enable ? 0 : keyball.that_have_ball ? 22 : 24;
    rgblight_set_clipping_range(is_keyboard_left() ? 0 : lednum_that, lednum_this);
    rgblight_set_effect_range(0, lednum_this + lednum_that);
#endif
}
