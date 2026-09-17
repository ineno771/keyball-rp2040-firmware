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
// LED配置。
//
// Keyball+はKeyball39と同じ物理キー配置（マトリクス8行×6列、行0-3=トラックボール側・
// 行4-7=非搭載側）だが、LED数・配線順が異なる。ユーザー確認済みの実機配線情報を反映：
//
// トラックボール側（26個）: アンダーグロー8個(idx0-7)、キー連動18個(idx8-25)。
//   キー連動は列ごとに「列0(4段:L00,L10,L20,L30)→列1(3段)→列2→列3→列4(3段)
//   →親指(L35:トラックボール側→L34:マイコン側)」の順で配線。
//   L31,L32,L33はトラックボール実装スペースのためLEDなし（スイッチはあるがLED非搭載）。
//
// 非搭載側（29個）: アンダーグロー8個(idx26-33)、キー連動21個(idx34-54)。
//   キー連動は列ごとに「列0(4段)→列1(4段)→列2(4段)→列3(4段)→列4(4段)→R35(単独)」の順。
//   トラックボール側と異なり全列が4段目(R30-R34)を持ち、LEDも全て実装されている。
//
// アンダーグロー8個の物理配置（マイコン下側から外側へ1-5、折り返してマイコン上側へ6-8）
// および全LEDの物理座標(x,y)は暫定値（実機到着後、Keyball Linkの
// 「LED位置実測（開発用）」機能で校正すること）。マトリクス→LEDインデックス対応と
// フラグ（アンダーグロー/キー連動の区別）はユーザー申告の実機配線情報に基づく確定値。
led_config_t g_led_config = {
    {
        // キーマトリクス(8行 x 6列) → LEDインデックス
        {  8, 12, 15, 18, 21, NO_LED },
        {  9, 13, 16, 19, 22, NO_LED },
        { 10, 14, 17, 20, 23, NO_LED },
        { 11, NO_LED, NO_LED, NO_LED, 25, 24 },
        { 34, 38, 42, 46, 50, NO_LED },
        { 35, 39, 43, 47, 51, NO_LED },
        { 36, 40, 44, 48, 52, NO_LED },
        { 37, 41, 45, 49, 53, 54 },
    },
    {
        // LEDインデックス(0-54) → 物理座標(x,y)。暫定値（実機到着後に校正）。
        {5,66}, {23,66}, {41,66}, {59,66}, {77,66}, {95,66}, {113,66}, {131,66}, {0,0}, {0,18},
        {0,36}, {0,54}, {18,0}, {18,18}, {18,36}, {36,0}, {36,18}, {36,36}, {54,0}, {54,18},
        {54,36}, {72,0}, {72,18}, {72,36}, {90,54}, {72,54}, {140,66}, {122,66}, {104,66}, {86,66},
        {68,66}, {50,66}, {32,66}, {14,66}, {198,0}, {198,18}, {198,36}, {198,54}, {180,0}, {180,18},
        {180,36}, {180,54}, {162,0}, {162,18}, {162,36}, {162,54}, {144,0}, {144,18}, {144,36}, {144,54},
        {126,0}, {126,18}, {126,36}, {126,54}, {108,54},
    },
    {
        // LEDインデックス → フラグ。トラックボール側: アンダーグロー8個(idx0-7)+
        // キー連動18個(idx8-25)=26個。非搭載側: アンダーグロー8個(idx26-33)+
        // キー連動21個(idx34-54)=29個。
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    },
};
#endif

void keyball_on_adjust_layout(keyball_adjust_t v) {
#ifdef RGBLIGHT_ENABLE
    // adjust RGBLIGHT's clipping and effect ranges
    // Keyball+はLED数が左右非対称（トラックボール側26個・非搭載側29個）のため、
    // Keyball39用の値をそのまま使うと実LED数とズレて起動直後にLEDの点灯範囲外が生じる
    uint8_t lednum_this = keyball.this_have_ball ? 26 : 29;
    uint8_t lednum_that = !keyball.that_enable ? 0 : keyball.that_have_ball ? 26 : 29;
    rgblight_set_clipping_range(is_keyboard_left() ? 0 : lednum_that, lednum_this);
    rgblight_set_effect_range(0, lednum_this + lednum_that);
#endif
}
