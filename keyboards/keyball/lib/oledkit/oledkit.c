/*
Copyright 2021 @Yowkees
Copyright 2021 MURAOKA Taro (aka KoRoN, @kaoriya)

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

#include "quantum.h"

#if defined(OLED_ENABLE) && !defined(OLEDKIT_DISABLE)

__attribute__((weak)) void oledkit_render_logo_user(void) {
    // Require `OLED_FONT_H "keyboards/keyball/lib/logofont/logofont.c"`
    char ch = 0x80;
    for (int y = 0; y < 3; y++) {
        oled_write_P(PSTR("  "), false);
        for (int x = 0; x < 16; x++) {
            oled_write_char(ch++, false);
        }
        oled_advance_page(false);
    }
}

__attribute__((weak)) void oledkit_render_info_user(void) {
    oledkit_render_logo_user();
}

// ── 起動演出（2026-10-09、本人希望）──
// 起動直後は消灯して待ち、左右がつながったら四方からドットが集まってKeyballのロゴになる。
// その後いつもの表示（マスター=情報表示、スレーブ=ロゴ）に戻る。段階と経過時間は
// keyball.c（LEDモードの同期で左右そろう）が上書きするこの関数で受け取る。
// 0=待機（消灯） 1=演出中（elapsed_msに経過ms） 2=終了（いつもの表示）。
__attribute__((weak)) uint8_t oledkit_boot_phase(uint16_t *elapsed_ms) {
    (void)elapsed_ms;
    return 2;
}

// ロゴの点はOLEDの文字データ（logofont.cのfont[]、0x80〜0xAFの48文字=16×3文字分）から
// 直接読む。1文字は横6ドット×縦8ドットで、1バイトが縦8ドット（下位ビットが上）。
extern const unsigned char font[];
#    define BOOT_LOGO_W 96
#    define BOOT_LOGO_H 24
static bool boot_logo_pixel(uint8_t x, uint8_t y) {
    uint8_t ch = (uint8_t)(0x80 + (y / 8) * 16 + x / OLED_FONT_WIDTH);
    uint8_t b  = pgm_read_byte(&font[(uint16_t)(ch - OLED_FONT_START) * OLED_FONT_WIDTH + x % OLED_FONT_WIDTH]);
    return (b >> (y % 8)) & 1;
}

static void render_boot_anim(uint16_t t) {
    const uint16_t TRAVEL_MS = 550;  // 1つの点が端からロゴの位置まで動く時間
    const uint16_t DELAY_MAX = 350;  // 点ごとに動き出しをずらす最大幅（約0.9秒で全部そろう）
    // ロゴの最終位置。スレーブはいつものロゴ表示（左余白2文字=12ドット）と同じ位置にして
    // そのまま自然につながるようにし、マスターは画面中央にする。
    const int16_t X0 = is_keyboard_master() ? (128 - BOOT_LOGO_W) / 2 : 12;
    const int16_t Y0 = 0;

    oled_clear();
    for (uint8_t y = 0; y < BOOT_LOGO_H; y++) {
        for (uint8_t x = 0; x < BOOT_LOGO_W; x++) {
            if (!boot_logo_pixel(x, y)) continue;
            // 点ごとに決まった「出発位置（上下左右の画面外のどこか）」と「出発の遅れ」
            uint32_t h = ((uint32_t)y * BOOT_LOGO_W + x + 1) * 2654435761u;
            h ^= h >> 15;
            int16_t sx, sy;
            switch (h & 3) {
                case 0: sx = (int16_t)((h >> 3) % 128); sy = -4; break;   // 上から
                case 1: sx = (int16_t)((h >> 3) % 128); sy = 35; break;   // 下から
                case 2: sx = -4; sy = (int16_t)((h >> 3) % 32); break;    // 左から
                default: sx = 131; sy = (int16_t)((h >> 3) % 32); break;  // 右から
            }
            uint16_t delay = (uint16_t)((h >> 12) % DELAY_MAX);
            int32_t  p     = (t <= delay) ? 0 : (int32_t)(t - delay) * 1024 / TRAVEL_MS;  // 進み具合（1024=到着）
            if (p > 1024) p = 1024;
            // 最後はゆっくり止まる（ease-out）: e = 1 - (1-p)^3
            int32_t q  = 1024 - p;
            int32_t e  = 1024 - (q * q / 1024) * q / 1024;
            int16_t px = (int16_t)(sx + (int32_t)(X0 + x - sx) * e / 1024);
            int16_t py = (int16_t)(sy + (int32_t)(Y0 + y - sy) * e / 1024);
            if (px >= 0 && px < 128 && py >= 0 && py < 32) oled_write_pixel((uint8_t)px, (uint8_t)py, true);
        }
    }
}

__attribute__((weak)) bool oled_task_user(void) {
    static uint8_t last_phase = 0xFF;
    uint16_t       elapsed    = 0;
    uint8_t        phase      = oledkit_boot_phase(&elapsed);
    if (phase != last_phase) {
        oled_clear();  // 段階が変わった瞬間に一度だけ全消去（演出の点が残らないように）
        last_phase = phase;
    }
    if (phase == 0) return true;  // 待機中は消灯のまま
    if (phase == 1) {
        render_boot_anim(elapsed);
        return true;
    }
    if (is_keyboard_master()) {
        oledkit_render_info_user();
    } else {
        oledkit_render_logo_user();
    }
    return true;
}

__attribute__((weak)) oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    // Logo needs to be rotated 180 degrees.
    //
    // A typical OLED has a narrow margin on the left side near the origin, and
    // a wide margin on the right side. The Keyball logo consists of three
    // lines. If the logo is displayed on an OLED consisting of four lines, the
    // margin on the right side will be too large and the balance is not good.
    //
    // Additionally, by rotating it, the left side of the logo will be above
    // the OLED screen, giving it a natural look.
    return !is_keyboard_master() ? OLED_ROTATION_180 : rotation;
}

#endif // OLED_ENABLE
