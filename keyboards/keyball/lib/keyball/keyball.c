/*
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

#include "quantum.h"
#ifdef SPLIT_KEYBOARD
#    include "transactions.h"
#endif

#include "keyball.h"
#include "kb_settings.h"
#include "drivers/pmw3360/pmw3360.h"
#if defined(RGBLIGHT_ENABLE) || defined(RGB_MATRIX_ENABLE)
#    include "kb_hid.h"
#    include "color.h"
#endif

#include <string.h>

const uint8_t CPI_DEFAULT    = KEYBALL_CPI_DEFAULT / 100;
const uint8_t CPI_MAX        = pmw3360_MAXCPI + 1;
const uint8_t SCROLL_DIV_MAX = 7;
const uint8_t ACCEL_MAX      = 10;

static uint8_t keyball_accel_value = 0;

const uint16_t AML_TIMEOUT_MIN = 100;
const uint16_t AML_TIMEOUT_MAX = 1000;
const uint16_t AML_TIMEOUT_QU  = 50;   // Quantization Unit

static const char BL = '\xB0'; // Blank indicator character
#ifdef OLED_ENABLE
static const char LFSTR_ON[] PROGMEM = "\xB2\xB3";
static const char LFSTR_OFF[] PROGMEM = "\xB4\xB5";
#endif

#ifdef KEYBALL_AML_THRESHOLD_RUNTIME
// 自動マウスレイヤーの発動しきい値（ランタイム変更可能）。デフォルトはQMK標準の10。
uint8_t kb_aml_threshold = 10;
#endif

keyball_t keyball = {
    .this_have_ball = false,
    .that_enable    = false,
    .that_have_ball = false,

    .this_motion = {0},
    .that_motion = {0},

    .cpi_value   = 0,
    .cpi_changed = false,

    .scroll_mode = false,
    .scroll_div  = 0,

    .pressing_keys = { BL, BL, BL, BL, BL, BL, 0 },
};

//////////////////////////////////////////////////////////////////////////////
// Hook points

__attribute__((weak)) void keyball_on_adjust_layout(keyball_adjust_t v) {}

//////////////////////////////////////////////////////////////////////////////
// Static utilities

// add16 adds two int16_t with clipping.
static int16_t add16(int16_t a, int16_t b) {
    int16_t r = a + b;
    if (a >= 0 && b >= 0 && r < 0) {
        r = 32767;
    } else if (a < 0 && b < 0 && r >= 0) {
        r = -32768;
    }
    return r;
}

// divmod16 divides *v by div, returns the quotient, and assigns the remainder
// to *v.
static int16_t __attribute__((unused)) divmod16(int16_t *v, int16_t div) {
    int16_t r = *v / div;
    *v -= r * div;
    return r;
}

// clip2int8 clips an integer fit into int8_t.
static inline int8_t clip2int8(int16_t v) {
    return (v) < -127 ? -127 : (v) > 127 ? 127 : (int8_t)v;
}

#ifdef OLED_ENABLE
static const char *format_4d(int16_t d) {
    static char buf[5] = {0}; // max width (4) + NUL (1)
    char        lead   = ' ';
    if (d < 0) {
        d    = -d;
        lead = '-';
    }
    buf[3] = (d % 10) + '0';
    d /= 10;
    if (d == 0) {
        buf[2] = lead;
        lead   = ' ';
    } else {
        buf[2] = (d % 10) + '0';
        d /= 10;
    }
    if (d == 0) {
        buf[1] = lead;
        lead   = ' ';
    } else {
        buf[1] = (d % 10) + '0';
        d /= 10;
    }
    buf[0] = lead;
    return buf;
}

static char to_1x(uint8_t x) {
    x &= 0x0f;
    return x < 10 ? x + '0' : x + 'a' - 10;
}
#endif

static void add_cpi(int8_t delta) {
    int16_t v = keyball_get_cpi() + delta;
    keyball_set_cpi(v < 1 ? 1 : v);
}

static void add_scroll_div(int8_t delta) {
    int8_t v = keyball_get_scroll_div() + delta;
    keyball_set_scroll_div(v < 1 ? 1 : v);
}

//////////////////////////////////////////////////////////////////////////////
// Pointing device driver

#if KEYBALL_MODEL == 46
void keyboard_pre_init_kb(void) {
    keyball.this_have_ball = pmw3360_init();
    keyboard_pre_init_user();
}
#endif

bool pointing_device_driver_init(void) {
#if KEYBALL_MODEL != 46
    keyball.this_have_ball = pmw3360_init();
#endif
    if (keyball.this_have_ball) {
#if defined(KEYBALL_PMW3360_UPLOAD_SROM_ID)
#    if KEYBALL_PMW3360_UPLOAD_SROM_ID == 0x04
        pmw3360_srom_upload(pmw3360_srom_0x04);
#    elif KEYBALL_PMW3360_UPLOAD_SROM_ID == 0x81
        pmw3360_srom_upload(pmw3360_srom_0x81);
#    else
#        error Invalid value for KEYBALL_PMW3360_UPLOAD_SROM_ID. Please choose 0x04 or 0x81 or disable it.
#    endif
#endif
        pmw3360_cpi_set(CPI_DEFAULT - 1);
    }
    return true;
}

uint16_t pointing_device_driver_get_cpi(void) {
    return keyball_get_cpi();
}

void pointing_device_driver_set_cpi(uint16_t cpi) {
    keyball_set_cpi(cpi);
}

__attribute__((weak)) void keyball_on_apply_motion_to_mouse_move(keyball_motion_t *m, report_mouse_t *r, bool is_left) {
#if KEYBALL_MODEL == 61 || KEYBALL_MODEL == 39 || KEYBALL_MODEL == 147 || KEYBALL_MODEL == 44 || KEYBALL_MODEL == 139
    r->x = clip2int8(m->y);
    r->y = clip2int8(m->x);
    if (is_left) {
        r->x = -r->x;
        r->y = -r->y;
    }
#elif KEYBALL_MODEL == 46
    r->x = clip2int8(m->x);
    r->y = -clip2int8(m->y);
#else
#    error("unknown Keyball model")
#endif
    // clear motion
    m->x = 0;
    m->y = 0;
}

// ── 慣性スクロール ────────────────────────────────────────────
// ボールを弾いた直後の速度を覚えておき、ボールが止まった後もその速度を
// 徐々に減衰させながら合成モーションとして注入し続けることで「弾いた後も
// しばらく滑るように」スクロールが続く演出を実現する。this_motion用/
// that_motion用で別々に状態を持つ必要があるため、m（呼び出し元が渡してくる
// ポインタ）がkeyball.this_motionかkeyball.that_motionかで添字を分ける。
//
// 注意（重要）: 下のスクロール消費処理（非hires時）はdivmod16で「割り切れ
// なかった端数」をm->x/m->yに残し続ける設計になっている。端数は一度base_div
// を下回るとそれ以上減らず、新しいボール入力が無い限りずっと同じ値のまま
// 居座り続ける。そのため「m->x/m->yが0でない」ことを「今回新しくボールが
// 動いた」判定に使うと、端数が残っている間ずっと"入力あり"と誤判定して
// しまい、慣性が全く働かなくなる（実際に最初の実装がこの不具合だった）。
// 代わりに「前回この関数を抜けた時点でm->x/m->yに残っていたはずの値」を
// 覚えておき、そこから変化していない場合だけ「新規入力なし」と判定する。
typedef struct {
    int32_t  v0x_fp, v0y_fp;                      // 滑走開始時の速度（1/256カウント単位）
    int32_t  accx_fp, accy_fp;                    // 1カウント未満の端数の持ち越し（1/256カウント単位）
    uint16_t coast_frames;                        // 今回の滑走の総フレーム数（フリックの速さと強さで決まる）
    int32_t  ema_x_fp, ema_y_fp;                  // 直近数フレームを均したボールの速さ（1/256カウント単位）
    int32_t  flick_x_fp, flick_y_fp;              // 今の一連の動きでの均した速さの最大値＝フリックの速さ
    uint16_t coast_frame;                         // 滑走開始から何フレーム経ったか
    int16_t  peak_vx, peak_vy;                    // 今の一連の動きで観測した最大速度（フリックの勢い）
    int16_t  prev_remainder_x, prev_remainder_y;  // 前回消費後にm->x/m->yへ残っていたはずの値
    bool     coasting;                            // 現在、慣性で滑っている最中か
    bool     streak_active;                       // 実入力が連続している一連の動きの最中か（ピークの区切り判定用）
    uint32_t coast_started_at;                    // 滑走を開始した時刻（万一の張り付き防止の安全弁用）
} keyball_scroll_inertia_t;

// 慣性で滑らせ続ける最大時間。万一どこかの計算に想定外の値が入り込んでも、
// この時間を過ぎたら問答無用で強制終了する安全弁（スクロールが延々と反応しなく
// なる不具合の再発防止）。
#define KEYBALL_SCROLL_INERTIA_MAX_COAST_MS 3000

// 滑走の仕方（2026-10-02作り直し、本人指摘「強さ1でもまだ強い」「速く滑ってビタっと
// 止まる。徐々に減速してほしい」「慣性のスクロール速度が速すぎる。フリック時の
// スピードも考慮してほしい」）。以前は「ピーク速度×3を初速にして指数減衰」だった。
// 今は摩擦のように一定の割合で減速する:
//  ・初速＝フリックの速さそのもの（倍率はかけない）。フリックの速さは1フレーム(8ms)の
//    瞬間最大値ではなく、直近数フレームを均した速さの最大値を使う（瞬間値はトゲが
//    出やすく、手で弾いた体感より速く滑り出してしまうため）。
//  ・減速の度合いは強さで決まり、フリックの速さに関係なく一定。そのため、ゆっくり
//    弾けばすぐ止まり、速く弾くほど長く・遠くまで滑る。
//  ・速度は最後まで一定の割合で落ちて0になる（徐々に遅くなって止まる）。
// 強さは「基準の速さ(KEYBALL_SCROLL_INERTIA_REF_X10×分周値/10、既定の発動しきい値と
// 同じ速さ)で弾いた時に止まるまでのフレーム数」として(強さ²+強さ)に比例するカーブで
// 割り当て、弱い側ほど細かく調整できるようにしている（強さ1≈56ms、8≈420ms、15≈1.3秒。
// 呼び出し周期はKEYBALL_REPORTMOUSE_INTERVAL＝8ms）。
#define KEYBALL_SCROLL_INERTIA_FRAMES_MIN 6    // 強さ0
#define KEYBALL_SCROLL_INERTIA_FRAMES_MAX 160  // 強さ最大
#define KEYBALL_SCROLL_INERTIA_FRAMES_CAP 300  // 1回の滑走の上限（約2.4秒）
#define KEYBALL_SCROLL_INERTIA_REF_X10    25   // 基準の速さ＝分周値×2.5/フレーム

// 慣性を発動させる最低速度（base_divの倍数）。ゆっくり意図的にスクロール
// している時は発動させたくない、速く弾いた時だけ発動してほしい、という
// 要望への対応。base_div（分周値=1目盛りあたりの生カウント数）の倍数で
// 表しているのは、この値がユーザーのスクロール感度設定によって変わる
// ため（感度を変えても「何目盛り分の速さが要るか」という相対的な基準は
// 変わらないようにするため）。倍率自体はkb_scroll_inertia_flick_mult_get()
// でWeb UIから調整できる（×10した整数で保持。例:30なら3.0倍）。

static keyball_scroll_inertia_t g_scroll_inertia[2];  // [0]=this_motion起点 [1]=that_motion起点

static void keyball_scroll_inertia_reset(void) {
    memset(g_scroll_inertia, 0, sizeof(g_scroll_inertia));
}

// int32_tの値をint16_tの範囲に収める（ブースト倍率をかけた後のオーバーフロー防止）
static int16_t keyball_clip_int16(int32_t v) {
    if (v > INT16_MAX) return INT16_MAX;
    if (v < INT16_MIN) return INT16_MIN;
    return (int16_t)v;
}

static keyball_scroll_inertia_t *keyball_scroll_inertia_of(const keyball_motion_t *m) {
    return &g_scroll_inertia[(m == &keyball.this_motion) ? 0 : 1];
}

// motion_to_mouse()から参照し、スクロールモードがOFFでもこの物理ボール起点は
// スクロール側の処理を呼び続けるべきかどうかを判定する。
//
// 注意（重要）: 判定材料を「coastingフラグが既にtrueかどうか」だけにすると
// 取りこぼす。トリガーキーとボールをほぼ同時に離す自然な操作では、「まだ
// 実入力があった最後のフレーム」の直後に「モードOFF」が来てしまい、
// coastingフラグが一度もtrueになる前にmotion_to_mouse()が「移動」側に
// 切り替わってしまう（スクロール側の関数が呼ばれないと、そもそも
// coasting=trueにする判定処理自体が実行されない）。そのため、既にcoasting中
// でなくても「直近の速度がまだ十分残っている」場合は同様にスクロール側を
// 呼び続けるようにしている。
static bool keyball_scroll_inertia_should_apply(const keyball_motion_t *m) {
    if (!kb_scroll_inertia_enable_get()) return false;  // 無効時は素通し（move側の通常動作に任せる）
    keyball_scroll_inertia_t *inertia = keyball_scroll_inertia_of(m);
    if (inertia->coasting) return true;
    // まだcoasting=trueになっていなくても、ピーク速度が「速く弾いた」と
    // 言えるレベルに達していればスクロール側を呼び続ける必要がある
    // （理由は下のkeyball_on_apply_motion_to_mouse_scroll側のコメント参照）。
    int16_t base_div  = (1 << (keyball_get_scroll_div() - 1)) * KEYBALL_SCROLL_DIV_BASE;
    int16_t min_flick = (int16_t)((int32_t)base_div * kb_scroll_inertia_flick_mult_get() / 10);
    return (abs(inertia->peak_vx) + abs(inertia->peak_vy)) >= min_flick;
}

__attribute__((weak)) void keyball_on_apply_motion_to_mouse_scroll(keyball_motion_t *m, report_mouse_t *r, bool is_left) {
    // 実際のボール入力（m->x/m->y、まだ消費されていない生の蓄積値）を消費する前に
    // 慣性スクロールの速度更新・合成モーションの注入を行う。
    keyball_scroll_inertia_t *inertia = keyball_scroll_inertia_of(m);

    // スクロールモード切替直後は、should_report()がボタン押下自体をボールの動きと
    // 誤検出しないようm->x/m->yを強制的に0にする（KEYBALL_SCROLLBALL_INHIVITOR、
    // 既定50ms）。この強制ゼロを「新規入力」と誤判定すると、慣性で滑らせたい
    // まさにその瞬間（モードOFF直後）に速度が握りつぶされてしまうため、この
    // 期間中は新規入力判定そのものを無効化する（滑っている速度・状態はそのまま
    // 維持し、既存の慣性判定へフォールスルーさせる）。
#if defined(KEYBALL_SCROLLBALL_INHIVITOR) && KEYBALL_SCROLLBALL_INHIVITOR > 0
    bool inhibited = TIMER_DIFF_32(timer_read32(), keyball.scroll_mode_changed) < KEYBALL_SCROLLBALL_INHIVITOR;
#else
    bool inhibited = false;
#endif
    // 下のスクロール消費や慣性のしきい値計算でも使うのでここで計算しておく。
    int16_t base_div = (1 << (keyball_get_scroll_div() - 1)) * KEYBALL_SCROLL_DIV_BASE;

    // 慣性を発動させる最低速度。この値未満のピーク速度（＝ゆっくり意図的に
    // 動かした場合）では滑走を開始させない。継続・停止のしきい値には使わない
    // （継続中はここより小さい速度でも、蓄積により発生し続けるのが正しい
    // 挙動なので、開始判定にだけ使う）。
    int16_t min_flick = (int16_t)((int32_t)base_div * kb_scroll_inertia_flick_mult_get() / 10);

    bool new_input = !inhibited && ((m->x != inertia->prev_remainder_x) || (m->y != inertia->prev_remainder_y));
    if (new_input) {
        // 実入力あり: ピーク速度を更新する（一連の動きの中で一番速かった
        // 瞬間を覚えておき、指を離す直前にたまたま減速していても取りこぼ
        // さないようにする）。
        //
        // 注意（重要）: streak_activeが立っていない（＝止まっていた状態から
        // 今回はじめて動き出した）場合は、まずピークをリセットしてから今回の
        // サンプルだけを見る。リセットしないと、以前の（無関係な・逆方向の
        // こともある）一連の動きで記録された古いピークがいつまでも居座り
        // 続け、今回はゆっくり上にスクロールしたつもりが、古い「下方向への
        // 速い動き」のピークがまだ大きいという理由で下方向に滑ってしまう、
        // 上下（または左右）が逆転する不具合の原因になっていた。
        int32_t sample_x_fp = (int32_t)(m->x - inertia->prev_remainder_x) * 256;
        int32_t sample_y_fp = (int32_t)(m->y - inertia->prev_remainder_y) * 256;
        if (!inertia->streak_active) {
            inertia->peak_vx    = 0;
            inertia->peak_vy    = 0;
            inertia->ema_x_fp   = sample_x_fp;
            inertia->ema_y_fp   = sample_y_fp;
            inertia->flick_x_fp = 0;
            inertia->flick_y_fp = 0;
        } else {
            inertia->ema_x_fp += (sample_x_fp - inertia->ema_x_fp) / 2;
            inertia->ema_y_fp += (sample_y_fp - inertia->ema_y_fp) / 2;
        }
        inertia->streak_active = true;
        inertia->coasting      = false;
        if (abs(m->x) > abs(inertia->peak_vx)) inertia->peak_vx = m->x;
        if (abs(m->y) > abs(inertia->peak_vy)) inertia->peak_vy = m->y;
        if (labs(inertia->ema_x_fp) > labs(inertia->flick_x_fp)) inertia->flick_x_fp = inertia->ema_x_fp;
        if (labs(inertia->ema_y_fp) > labs(inertia->flick_y_fp)) inertia->flick_y_fp = inertia->ema_y_fp;
    } else {
        // 新規入力なし: 一連の動きは途切れた。次にnew_inputになった時は
        // 新しい一連の動きとしてピークを取り直す（上のstreak_activeの
        // リセット処理を参照）。
        inertia->streak_active = false;
        if (kb_scroll_inertia_enable_get() &&
            (inertia->coasting || (abs(inertia->peak_vx) + abs(inertia->peak_vy)) >= min_flick)) {
            // 慣性ON・ピーク速度が「速く弾いた」と言えるレベルに達している
            // （またはすでに滑走中）: 減衰させながら滑らせる。
            //
            // 注意（重要）: 滑走を開始した後の停止判定は速度のしきい値ではなく
            // 経過フレーム数(coast_frames)で行う。min_flickのようなbase_div基準の
            // しきい値で止めると、減速の終盤（base_div未満）で強制停止してしまうが、
            // 下でm->xに「代入」ではなく「加算」しているため、base_div未満の速度でも
            // 複数フレームかけて蓄積しいずれ分周値を超えた時点で正しくスクロールが
            // 発生する（これが最後の「徐々に遅くなる」部分）。
            if (!inertia->coasting) {
                // 滑走開始: 初速はフリックの速さそのもの。止まるまでのフレーム数は
                // 「強さで決まる基準フレーム数 × フリックの速さ÷基準の速さ」
                // （一定の割合で減速するので、速いほど長く滑る）。
                uint32_t st       = kb_scroll_inertia_strength_get();
                uint32_t st_mx    = KB_SCROLL_INERTIA_STRENGTH_MAX;
                uint32_t ref_frms = KEYBALL_SCROLL_INERTIA_FRAMES_MIN +
                                    (KEYBALL_SCROLL_INERTIA_FRAMES_MAX - KEYBALL_SCROLL_INERTIA_FRAMES_MIN) * (st * st + st) / (st_mx * st_mx + st_mx);
                int32_t  ref_fp   = (int32_t)base_div * KEYBALL_SCROLL_INERTIA_REF_X10 * 256 / 10;
                int32_t  speed_fp = labs(inertia->flick_x_fp) + labs(inertia->flick_y_fp);
                uint32_t frames   = (uint32_t)((int64_t)ref_frms * speed_fp / (ref_fp > 0 ? ref_fp : 1));
                if (frames < 1) frames = 1;
                if (frames > KEYBALL_SCROLL_INERTIA_FRAMES_CAP) frames = KEYBALL_SCROLL_INERTIA_FRAMES_CAP;
                inertia->coast_frames     = (uint16_t)frames;
                inertia->v0x_fp           = inertia->flick_x_fp;
                inertia->v0y_fp           = inertia->flick_y_fp;
                inertia->accx_fp          = 0;
                inertia->accy_fp          = 0;
                inertia->coast_frame      = 0;
                inertia->peak_vx          = 0;
                inertia->peak_vy          = 0;
                inertia->coast_started_at = timer_read32();
            }
            inertia->coasting = true;
            // 残りフレーム数に比例した速度＝一定の割合で減速して最後に0になる。
            // 1カウント未満の端数は次フレームへ持ち越すので、遅くなっても途中で途切れない。
            int32_t left = (int32_t)(inertia->coast_frames - inertia->coast_frame);
            inertia->accx_fp += inertia->v0x_fp * left / inertia->coast_frames;
            inertia->accy_fp += inertia->v0y_fp * left / inertia->coast_frames;
            int32_t out_x = inertia->accx_fp / 256;
            int32_t out_y = inertia->accy_fp / 256;
            inertia->accx_fp -= out_x * 256;
            inertia->accy_fp -= out_y * 256;
            m->x = add16(m->x, keyball_clip_int16(out_x));
            m->y = add16(m->y, keyball_clip_int16(out_y));
            inertia->coast_frame++;
            if (inertia->coast_frame >= inertia->coast_frames ||
                TIMER_DIFF_32(timer_read32(), inertia->coast_started_at) > KEYBALL_SCROLL_INERTIA_MAX_COAST_MS) {
                inertia->coasting = false;
            }
        }
    }
#ifdef POINTING_DEVICE_HIRES_SCROLL_ENABLE
    // 高解像度スクロール: 生の動きを分解能でスケールして送り、OS側で細かく刻ませる（滑らか）
    uint16_t res = pointing_device_get_hires_scroll_resolution();
    int16_t  x   = (int16_t)((int32_t)m->x * res / base_div);
    int16_t  y   = (int16_t)((int32_t)m->y * res / base_div);
    m->x = 0;
    m->y = 0;
#else
    // consume motion of trackball.
    int16_t div = base_div;
    int16_t x = divmod16(&m->x, div);
    int16_t y = divmod16(&m->y, div);
#endif
    // 次回呼び出し時の「新規入力なし」判定用に、消費後の残り（端数）を覚えておく
    inertia->prev_remainder_x = m->x;
    inertia->prev_remainder_y = m->y;

    // apply to mouse report.
#if KEYBALL_MODEL == 61 || KEYBALL_MODEL == 39 || KEYBALL_MODEL == 147 || KEYBALL_MODEL == 44 || KEYBALL_MODEL == 139
#    ifdef POINTING_DEVICE_HIRES_SCROLL_ENABLE
    r->h = y;
    r->v = -x;
#    else
    r->h = clip2int8(y);
    r->v = -clip2int8(x);
#    endif
    if (is_left) {
        r->h = -r->h;
        r->v = -r->v;
    }
#elif KEYBALL_MODEL == 46
    r->h = clip2int8(x);
    r->v = clip2int8(y);
#else
#    error("unknown Keyball model")
#endif

    // Scroll snapping
#if KEYBALL_SCROLLSNAP_ENABLE == 1
    // Old behavior up to 1.3.2)
    uint32_t now = timer_read32();
    if (r->h != 0 || r->v != 0) {
        keyball.scroll_snap_last = now;
    } else if (TIMER_DIFF_32(now, keyball.scroll_snap_last) >= KEYBALL_SCROLLSNAP_RESET_TIMER) {
        keyball.scroll_snap_tension_h = 0;
    }
    if (abs(keyball.scroll_snap_tension_h) < KEYBALL_SCROLLSNAP_TENSION_THRESHOLD) {
        keyball.scroll_snap_tension_h += y;
        r->h = 0;
    }
#elif KEYBALL_SCROLLSNAP_ENABLE == 2
    // New behavior
    switch (keyball_get_scrollsnap_mode()) {
        case KEYBALL_SCROLLSNAP_MODE_VERTICAL:
            r->h = 0;
            break;
        case KEYBALL_SCROLLSNAP_MODE_HORIZONTAL:
            r->v = 0;
            break;
        default:
            // pass by without doing anything
            break;
    }
#endif
}

static void motion_to_mouse(keyball_motion_t *m, report_mouse_t *r, bool is_left, bool as_scroll) {
    // スクロールモードが既にOFFでも、慣性で滑っている最中はスクロール側の処理を
    // 呼び続ける。ここでas_scroll単体の判定にしてしまうと、モードOFFになった
    // 瞬間に慣性が打ち切られてしまう（本来はここからしばらく滑らせたい）。
    if (as_scroll || keyball_scroll_inertia_should_apply(m)) {
        keyball_on_apply_motion_to_mouse_scroll(m, r, is_left);
    } else {
        keyball_on_apply_motion_to_mouse_move(m, r, is_left);
    }
}

static inline bool should_report(void) {
    uint32_t now = timer_read32();
#if defined(KEYBALL_REPORTMOUSE_INTERVAL) && KEYBALL_REPORTMOUSE_INTERVAL > 0
    // throttling mouse report rate.
    static uint32_t last = 0;
    if (TIMER_DIFF_32(now, last) < KEYBALL_REPORTMOUSE_INTERVAL) {
        return false;
    }
    last = now;
#endif
#if defined(KEYBALL_SCROLLBALL_INHIVITOR) && KEYBALL_SCROLLBALL_INHIVITOR > 0
    if (TIMER_DIFF_32(now, keyball.scroll_mode_changed) < KEYBALL_SCROLLBALL_INHIVITOR) {
        keyball.this_motion.x = 0;
        keyball.this_motion.y = 0;
        keyball.that_motion.x = 0;
        keyball.that_motion.y = 0;
    }
#endif
    return true;
}

// ボールの向きの補正（is_leftで符号を反転させる処理）に使う「このハーフは左手か」。
// 既定はis_keyboard_left()。44・61・+のkeymap.cはis_keyboard_left()を「ボール搭載側=左」に
// 上書きしているため、こちらは基板本来の左右(AVR版と同じ値)を返すよう上書きする
// （2026-10-06、左手ボールのKeyball61でカーソルが上下左右とも反転していた不具合の修正）。
__attribute__((weak)) bool keyball_motion_is_left(void) {
    return is_keyboard_left();
}

report_mouse_t pointing_device_driver_get_report(report_mouse_t rep) {
    // fetch from optical sensor.
    if (keyball.this_have_ball) {
        pmw3360_motion_t d = {0};
        if (pmw3360_motion_burst(&d)) {
            ATOMIC_BLOCK_FORCEON {
                keyball.this_motion.x = add16(keyball.this_motion.x, d.x);
                keyball.this_motion.y = add16(keyball.this_motion.y, d.y);
            }
        }
    }
    // report mouse event, if keyboard is primary.
    if (is_keyboard_master() && should_report()) {
        // modify mouse report by PMW3360 motion.
        motion_to_mouse(&keyball.this_motion, &rep, keyball_motion_is_left(), keyball.scroll_mode);
        motion_to_mouse(&keyball.that_motion, &rep, !keyball_motion_is_left(), keyball.scroll_mode ^ keyball.this_have_ball);
        // store mouse report for OLED.
        keyball.last_mouse = rep;
    }
    return rep;
}

#ifdef GESTURE_ENABLE
//////////////////////////////////////////////////////////////////////////////
// Gesture-linked LED wave (see keyball.h for keyball_gesture_wave_trigger)

// RIPPLE(rgb_matrix_user.inc)と同じ「空きスロット優先・無ければ最古を上書き」方式。
// マスター・スレーブ双方でローカルに呼ばれ、それぞれのkeyball.gesture_wave_*を更新する。
static void gesture_wave_record_local(uint8_t direction) {
    uint8_t slot = KEYBALL_GESTURE_WAVE_SLOT_COUNT;
    for (uint8_t s = 0; s < KEYBALL_GESTURE_WAVE_SLOT_COUNT; s++) {
        if (!keyball.gesture_wave_active[s]) {
            slot = s;
            break;
        }
    }
    if (slot == KEYBALL_GESTURE_WAVE_SLOT_COUNT) {
        uint16_t oldest = 0;
        for (uint8_t s = 0; s < KEYBALL_GESTURE_WAVE_SLOT_COUNT; s++) {
            uint16_t e = timer_elapsed(keyball.gesture_wave_start[s]);
            if (e >= oldest) {
                oldest = e;
                slot   = s;
            }
        }
    }
    keyball.gesture_wave_start[slot]  = timer_read();
    keyball.gesture_wave_dir[slot]    = direction;
    keyball.gesture_wave_active[slot] = true;
}

// direction・speed・style・hue/sat/valをまとめてRPCで送るための型。speed同様、
// styleや色も keyball_gesture_wave_trigger直前のコメント参照の理由で一緒に運ぶ
// 必要がある（2026-09-30〜、モードごとに設定を持つようになったのに合わせてstyleも
// 追加。modeそのものは送らず、マスターが自分のEEPROMから解決した最終的な値だけを
// 送ることで、スレーブ側は自分のEEPROMを一切参照しなくて済む＝ハーフ間のズレが
// 起きない）。
typedef struct {
    uint8_t direction;
    uint8_t speed;
    uint8_t style;
    uint8_t hue;
    uint8_t sat;
    uint8_t val;
} gesture_wave_rpc_t;

#ifdef SPLIT_KEYBOARD
// 反対側のハーフにも同じ瞬間にウェーブを表示させるための送信待ちフラグ。
// housekeeping_task_kbから毎スキャン呼ばれるrpc_gesture_wave_invoke(下記)が実際の
// 送信を担当する。keyball_gesture_wave_trigger自体はマスター側のジェスチャー
// エンジン(keymap.c)からしか呼ばれない想定。
static bool               g_gesture_wave_pending = false;
static gesture_wave_rpc_t g_gesture_wave_pending_payload;
#endif

void keyball_gesture_wave_trigger(uint8_t direction, uint8_t mode) {
    if (!kb_gesture_wave_enable_get()) return;  // 機能自体がOFFなら何もしない
    if (keyball_boot_active()) return;          // 起動演出中は出さない
    if (mode >= KB_GESTURE_MODE_COUNT) mode = 0;  // 呼び出し元(keymap.c)のgst_active_modeは
                                                   // 常に0-5のはずだが念のため防御的にクランプ

    // 連続入力などで短時間に何度も呼ばれても、前のウェーブがまだ表示中なら次を
    // 発動させない（本人希望：重ねて発動させず、前のウェーブが終わってから次を
    // 出したい。2026-09-29、一度「連続発火で上書きする」方式を試したが、やはり
    // この動作に戻すことになった）。ここでの判定はマスター自身のローカルな
    // gesture_wave_active[]で行う（スレーブへは実際に発動を決めた時だけRPCで
    // 伝えるので、スレーブ側が勝手に多重発動することもない）。
    for (uint8_t s = 0; s < KEYBALL_GESTURE_WAVE_SLOT_COUNT; s++) {
        if (keyball.gesture_wave_active[s]) return;
    }

    // ウェーブの速さ・見た目・色は各ハーフが自分のEEPROMから読むと分割両ハーフで値が
    // ずれる（kb_settings.hのKB_GESTURE_WAVE_SPEED_EEPROM/STYLE_EEPROM/COLOR_EEPROM
    // 参照）。呼び出し元はマスターのジェスチャーエンジンのみなので、ここで読む値は
    // 常にWeb UIが実際に書き込んだ側（＝マスター）の値であり、これを両ハーフ共通の
    // 「正」としてRPCで配る。2026-09-30〜: モードごとの設定になったため、発火元から
    // 渡されたmodeでこの時点で解決し、以後（RPC送信・後続のタスク/描画）は解決済みの
    // 値だけを見る（モード自体は運ばない）。
    keyball.gesture_wave_speed          = kb_gesture_wave_speed_get(mode);
    keyball.gesture_wave_style          = kb_gesture_wave_style_get(mode);
    kb_gesture_wave_color_t wave_color  = kb_gesture_wave_color_get(mode);
    keyball.gesture_wave_hue            = wave_color.hue;
    keyball.gesture_wave_sat            = wave_color.sat;
    keyball.gesture_wave_val            = wave_color.val;
    gesture_wave_record_local(direction);
#ifdef SPLIT_KEYBOARD
    g_gesture_wave_pending         = true;
    g_gesture_wave_pending_payload = (gesture_wave_rpc_t){
        .direction = direction, .speed = keyball.gesture_wave_speed, .style = keyball.gesture_wave_style,
        .hue = wave_color.hue, .sat = wave_color.sat, .val = wave_color.val};
#endif
}
#endif

//////////////////////////////////////////////////////////////////////////////
// Split RPC

#ifdef SPLIT_KEYBOARD

// ── 起動時のLEDエフェクトの左右のずれをなくす（2026-10-07、本人報告）──
// 電源を入れると、USBを挿していない側は自分がスレーブと判断するまで待ってから動き出す
// ため、左右でエフェクトの始まりがずれたままになっていた（Web UIでLEDを変えると揃う）。
// 左右の情報交換（GET_INFO）が終わった時点で、マスターがエフェクトを一度別のものに
// 切り替えてすぐ戻すことで、既存のRGB_MATRIXの同期（PUT_RGB_MATRIX）に乗せて両側で
// エフェクトを同時にかけ直す。新しいハーフ間通信は追加しない（定期RPCは増やさない方針）。
#    ifdef RGB_MATRIX_ENABLE
static uint8_t  g_led_resync_phase = 0;  // 0=なし 1=切り替え待ち 2=戻し待ち
static uint8_t  g_led_resync_mode  = 0;
static uint16_t g_led_resync_timer = 0;
#    endif

static void led_resync_request(void) {
#    ifdef RGB_MATRIX_ENABLE
    g_led_resync_phase = 1;
#    endif
}

static void led_resync_task(void) {
#    ifdef RGB_MATRIX_ENABLE
    if (g_led_resync_phase == 1) {
        if (!rgb_matrix_is_enabled()) {
            g_led_resync_phase = 0;
            return;
        }
        g_led_resync_mode = rgb_matrix_get_mode();
        rgb_matrix_mode_noeeprom(g_led_resync_mode == RGB_MATRIX_SOLID_COLOR ? RGB_MATRIX_BREATHING : RGB_MATRIX_SOLID_COLOR);
        g_led_resync_timer = timer_read();
        g_led_resync_phase = 2;
    } else if (g_led_resync_phase == 2 && timer_elapsed(g_led_resync_timer) >= 30) {
        // 30ms待つ間に、切り替えたモードがスレーブへ送られる
        rgb_matrix_mode_noeeprom(g_led_resync_mode);
        g_led_resync_phase = 0;
    }
#    endif
}

// ── 起動演出（2026-10-09、本人希望）──
// 電源を入れると、USB側が先に光り、反対側が遅れて光り、最後にLEDが再スタートする、という
// ばらばらな見た目だったため、次の流れにする:
//   1) 起動直後は左右ともLED・OLEDを消したまま待つ（BOOT_WAIT）
//   2) 左右の情報交換（GET_INFO）が済んだ瞬間に、マスターがLEDモードを起動演出
//      （BOOT_SPREAD、中央から外へ光が広がる）に切り替える。反対側へは既存のRGB_MATRIXの
//      同期（PUT_RGB_MATRIX）で伝わるので、新しいハーフ間通信は追加していない。
//      OLEDは両ハーフとも「今のLEDモード」を見て起動演出（ドットが集まってロゴになる、
//      oledkit.c）を表示する。
//   3) KB_BOOT_ANIM_MS後に、マスターがいつものLED設定へ戻す（これも同期で両側に伝わり、
//      エフェクトが左右同時に始まる）。OLEDもいつもの表示に戻る。
// 反対側がつながっていない（片側だけで使う）場合は、KB_BOOT_FALLBACK_MS待って始める。
// LEDを「オフ」に設定している場合、LEDは消えたままでOLEDだけ演出する（本人指定）。
#    ifdef RGB_MATRIX_ENABLE
#        define KB_BOOT_ANIM_MS 1500
#        define KB_BOOT_FALLBACK_MS 4000
#        define KB_BOOT_SLAVE_GIVEUP_MS 8000
enum { KB_BOOT_WAIT = 0, KB_BOOT_ANIM = 1, KB_BOOT_DONE = 2 };
static uint8_t  g_boot_phase      = KB_BOOT_DONE;  // keyball_boot_init()で WAIT にする
static uint16_t g_boot_anim_start = 0;
static uint8_t  g_boot_saved_mode = RGB_MATRIX_SOLID_COLOR;
static bool     g_boot_negotiated = false;

// LEDのモードを切り替える。QMKのrgb_matrix_mode_noeeprom()はLEDがオフの時は何もしない
// （quantum/rgb_matrix/rgb_matrix.c）ため、オフの時はモードの値だけを直接書き換える。
// 反対側への合図はこのモードの値の同期（PUT_RGB_MATRIX、100msごとの強制同期あり）で
// 伝わるので、LEDがオフでもスレーブのOLEDの起動演出が動くようにするため（2026-10-09、
// LEDオフの状態でスレーブのロゴ演出が出なかった）。LEDはオフのままなので光らない。
static void boot_set_mode(uint8_t mode) {
    if (rgb_matrix_is_enabled()) {
        rgb_matrix_mode_noeeprom(mode);
    } else {
        rgb_matrix_config.mode = mode;
    }
}

static bool boot_is_boot_mode(uint8_t mode) {
    return mode == RGB_MATRIX_CUSTOM_BOOT_WAIT || mode == RGB_MATRIX_CUSTOM_BOOT_SPREAD;
}

// keyboard_post_init_kbから両ハーフで呼ぶ（いつものLED設定を反映した直後）
static void keyball_boot_init(void) {
    uint8_t mode      = rgb_matrix_get_mode();
    g_boot_saved_mode = boot_is_boot_mode(mode) ? RGB_MATRIX_SOLID_COLOR : mode;
    g_boot_phase      = KB_BOOT_WAIT;
    boot_set_mode(RGB_MATRIX_CUSTOM_BOOT_WAIT);  // オン/オフの状態はそのまま
}

static void boot_start_anim(void) {
    g_boot_phase      = KB_BOOT_ANIM;
    g_boot_anim_start = timer_read();
    boot_set_mode(RGB_MATRIX_CUSTOM_BOOT_SPREAD);
}

static void apply_layer_led_now(uint8_t hl);

// 毎スキャン両ハーフで呼ぶ
static void keyball_boot_task(void) {
    if (g_boot_phase == KB_BOOT_DONE) return;
    if (is_keyboard_master()) {
        if (g_boot_phase == KB_BOOT_WAIT) {
            if (g_boot_negotiated || timer_read32() >= KB_BOOT_FALLBACK_MS) boot_start_anim();
        } else if (timer_elapsed(g_boot_anim_start) >= KB_BOOT_ANIM_MS) {
            g_boot_phase = KB_BOOT_DONE;
            // 演出中に押されたレイヤーも含め、今あるべきLED表示へ戻す
            apply_layer_led_now(get_highest_layer(layer_state | default_layer_state));
            // LEDオフ設定の時はモードが起動演出のまま残るので、元のモードへ戻す
            // （反対側のOLEDがモードを見て演出を終えるため）
            if (boot_is_boot_mode(rgb_matrix_get_mode())) boot_set_mode(g_boot_saved_mode);
        }
    } else {
        // スレーブはマスターから同期されたLEDモードで段階を判断する。
        // QMKのスレーブ側は、マスターからまだ何も届いていない間も受信用の共有メモリ（中身は0）を
        // 毎スキャンrgb_matrix_configへ写すため、その間はモードが0になる
        // （quantum/split_common/transactions.cのrgb_matrix_handlers_slave）。モード0は
        // LED設定では使わない値（オフはdisableで表す）なので「まだ届いていない」とみなして待つ
        // （2026-10-09、これを「演出終了」と誤判定してスレーブのロゴ演出が出ていなかった）。
        uint8_t mode = rgb_matrix_get_mode();
        if (mode == RGB_MATRIX_CUSTOM_BOOT_SPREAD) {
            if (g_boot_phase != KB_BOOT_ANIM) {
                g_boot_phase      = KB_BOOT_ANIM;
                g_boot_anim_start = timer_read();
            }
        } else if (timer_read32() >= KB_BOOT_SLAVE_GIVEUP_MS) {
            g_boot_phase = KB_BOOT_DONE;
        } else if (mode != 0 && mode != RGB_MATRIX_CUSTOM_BOOT_WAIT) {
            g_boot_phase = KB_BOOT_DONE;  // 演出が終わった（または演出の後につながった）
        }
    }
}

bool keyball_boot_active(void) {
    return g_boot_phase != KB_BOOT_DONE;
}

// oledkit.cの弱い関数を上書き。0=待機（消灯） 1=演出中（elapsedに経過ms） 2=終了
uint8_t oledkit_boot_phase(uint16_t *elapsed_ms) {
    if (g_boot_phase == KB_BOOT_ANIM && elapsed_ms) *elapsed_ms = timer_elapsed(g_boot_anim_start);
    return g_boot_phase;
}
#    else
bool keyball_boot_active(void) {
    return false;
}
#    endif

static void rpc_get_info_handler(uint8_t in_buflen, const void *in_data, uint8_t out_buflen, void *out_data) {
    keyball_info_t info = {
        .ballcnt = keyball.this_have_ball ? 1 : 0,
    };
    *(keyball_info_t *)out_data = info;
    keyball_on_adjust_layout(KEYBALL_ADJUST_SECONDARY);
}

static void rpc_get_info_invoke(void) {
    static bool     negotiated = false;
    static uint32_t last_sync  = 0;
    static int      round      = 0;
    uint32_t        now        = timer_read32();
    if (negotiated || TIMER_DIFF_32(now, last_sync) < KEYBALL_TX_GETINFO_INTERVAL) {
        return;
    }
    last_sync = now;
    round++;
    keyball_info_t recv = {0};
    if (!transaction_rpc_exec(KEYBALL_GET_INFO, 0, NULL, sizeof(recv), &recv)) {
        // 反対側の起動がまだ間に合っていないだけの可能性があるため、応答が
        // 得られるまで無期限にリトライする（失敗を「ボールなし」と確定させない。
        // 以前は既定回数で諦めてnegotiated=trueにしてしまい、その後の電源投入中
        // ずっと「反対側にボールなし」と誤認識したままになる不具合があった）。
        dprintf("keyball:rpc_get_info_invoke: missed #%d\n", round);
        return;
    }
    negotiated             = true;
    keyball.that_enable    = true;
    keyball.that_have_ball = recv.ballcnt > 0;
    dprintf("keyball:rpc_get_info_invoke: negotiated #%d %d\n", round, keyball.that_have_ball);

    // split keyboard negotiation completed.

#    ifdef VIA_ENABLE
    // adjust VIA layout options according to current combination.
    uint8_t  layouts = (keyball.this_have_ball ? (is_keyboard_left() ? 0x02 : 0x01) : 0x00) | (keyball.that_have_ball ? (is_keyboard_left() ? 0x01 : 0x02) : 0x00);
    uint32_t curr    = via_get_layout_options();
    uint32_t next    = (curr & ~0x3) | layouts;
    if (next != curr) {
        via_set_layout_options(next);
    }
#    endif

    keyball_on_adjust_layout(KEYBALL_ADJUST_PRIMARY);
#    ifdef RGB_MATRIX_ENABLE
    // 起動待ちの間につながった時は起動演出を始める合図にする。演出が既に始まった・終わった後に
    // 遅れてつながった時は、従来どおりエフェクトをかけ直して左右をそろえる。
    if (g_boot_phase == KB_BOOT_WAIT) {
        g_boot_negotiated = true;
        return;
    }
#    endif
    led_resync_request();
}

static void rpc_get_motion_handler(uint8_t in_buflen, const void *in_data, uint8_t out_buflen, void *out_data) {
    *(keyball_motion_t *)out_data = keyball.this_motion;
    // clear motion
    keyball.this_motion.x = 0;
    keyball.this_motion.y = 0;
}

static void rpc_get_motion_invoke(void) {
    static uint32_t last_sync = 0;
    uint32_t        now       = timer_read32();
    if (TIMER_DIFF_32(now, last_sync) < KEYBALL_TX_GETMOTION_INTERVAL) {
        return;
    }
    keyball_motion_t recv = {0};
    if (transaction_rpc_exec(KEYBALL_GET_MOTION, 0, NULL, sizeof(recv), &recv)) {
        keyball.that_motion.x = add16(keyball.that_motion.x, recv.x);
        keyball.that_motion.y = add16(keyball.that_motion.y, recv.y);
    }
    last_sync = now;
    return;
}

static void rpc_set_cpi_handler(uint8_t in_buflen, const void *in_data, uint8_t out_buflen, void *out_data) {
    keyball_cpi_t cpi = *(keyball_cpi_t *)in_data;
    keyball_set_cpi(cpi);

    // 2026-09-18: このハーフ（スレーブ）自身のEEPROMにもCPIを保存する。
    // 従来はマスターから届いたCPIをRAM(and this_have_ball==trueならセンサー本体)に
    // 反映するだけで、スレーブ自身のEEPROMには一切書き込んでいなかった。CPI等
    // (keyball_config_t)はマスター側のkeyboard_post_init_kb()でmagic不一致なら
    // 既定値へリセット＆保存する仕組みになっているため、ボール側(この環境では
    // スレーブ)を単独で再起動・再書き込みするたびにmagicが一致せず既定CPIへ戻り、
    // マスターが次にCPIを送り直すまでの間、実際のCPIが設定値とズレてしまっていた
    // （本人報告:「ファームウェアを書き直すとトラックボール設定がリセットされる」）。
    // ここでスレーブ自身のEEPROMにも書いておくことで、スレーブ単独の再起動後も
    // マスターからの再送を待たずに正しいCPIで起動できるようにする。
    keyball_config_t c = {.raw = eeconfig_read_kb()};
    c.cpi   = cpi;
    c.magic = KEYBALL_CONFIG_MAGIC;
    eeconfig_update_kb(c.raw);
}

static void rpc_set_cpi_invoke(void) {
    if (!keyball.cpi_changed) {
        return;
    }
    keyball_cpi_t req = keyball.cpi_value;
    if (!transaction_rpc_send(KEYBALL_SET_CPI, sizeof(req), &req)) {
        return;
    }
    keyball.cpi_changed = false;
}

#ifdef GESTURE_ENABLE
static void rpc_gesture_wave_handler(uint8_t in_buflen, const void *in_data, uint8_t out_buflen, void *out_data) {
    const gesture_wave_rpc_t *req = (const gesture_wave_rpc_t *)in_data;
    keyball.gesture_wave_speed    = req->speed;
    keyball.gesture_wave_style    = req->style;
    keyball.gesture_wave_hue      = req->hue;
    keyball.gesture_wave_sat      = req->sat;
    keyball.gesture_wave_val      = req->val;
    gesture_wave_record_local(req->direction);
}

static void rpc_gesture_wave_invoke(void) {
    if (!g_gesture_wave_pending) {
        return;
    }
    gesture_wave_rpc_t req = g_gesture_wave_pending_payload;
    if (!transaction_rpc_send(KEYBALL_GESTURE_WAVE, sizeof(req), &req)) {
        return;
    }
    g_gesture_wave_pending = false;
}
#endif


#endif

//////////////////////////////////////////////////////////////////////////////
// OLED utility

#ifdef OLED_ENABLE
// clang-format off
const char PROGMEM code_to_name[] = {
    'a', 'b', 'c', 'd', 'e', 'f',  'g', 'h', 'i',  'j',
    'k', 'l', 'm', 'n', 'o', 'p',  'q', 'r', 's',  't',
    'u', 'v', 'w', 'x', 'y', 'z',  '1', '2', '3',  '4',
    '5', '6', '7', '8', '9', '0',  'R', 'E', 'B',  'T',
    '_', '-', '=', '[', ']', '\\', '#', ';', '\'', '`',
    ',', '.', '/',
};
// clang-format on
#endif

void keyball_oled_render_ballinfo(void) {
#ifdef OLED_ENABLE
    // Format: `Ball:{mouse x}{mouse y}{mouse h}{mouse v}`
    //
    // Output example:
    //
    //     Ball: -12  34   0   0

    // 1st line, "Ball" label, mouse x, y, h, and v.
    oled_write_P(PSTR("Ball\xB1"), false);
    oled_write(format_4d(keyball.last_mouse.x), false);
    oled_write(format_4d(keyball.last_mouse.y), false);
    oled_write(format_4d(keyball.last_mouse.h), false);
    oled_write(format_4d(keyball.last_mouse.v), false);

    // 2nd line, empty label and CPI
    oled_write_P(PSTR("    \xB1\xBC\xBD"), false);
    oled_write(format_4d(keyball_get_cpi()) + 1, false);
    oled_write_P(PSTR("00 "), false);

    // indicate scroll snap mode: "VT" (vertical), "HO" (horizontal), and "SCR" (free)
#if 1 && KEYBALL_SCROLLSNAP_ENABLE == 2
    switch (keyball_get_scrollsnap_mode()) {
        case KEYBALL_SCROLLSNAP_MODE_VERTICAL:
            oled_write_P(PSTR("VT"), false);
            break;
        case KEYBALL_SCROLLSNAP_MODE_HORIZONTAL:
            oled_write_P(PSTR("HO"), false);
            break;
        default:
            oled_write_P(PSTR("\xBE\xBF"), false);
            break;
    }
#else
    oled_write_P(PSTR("\xBE\xBF"), false);
#endif
    // indicate scroll mode: on/off
    if (keyball.scroll_mode) {
        oled_write_P(LFSTR_ON, false);
    } else {
        oled_write_P(LFSTR_OFF, false);
    }

    // indicate scroll divider:
    oled_write_P(PSTR(" \xC0\xC1"), false);
    oled_write_char('0' + keyball_get_scroll_div(), false);

    // 慣性スクロールのデバッグ表示（一時的）: 現在滑走中かどうか
    oled_write_P(PSTR(" "), false);
    oled_write_char((g_scroll_inertia[0].coasting || g_scroll_inertia[1].coasting) ? 'C' : '.', false);
#endif
}

void keyball_oled_render_ballsubinfo(void) {
#ifdef OLED_ENABLE
#endif
}

void keyball_oled_render_keyinfo(void) {
#ifdef OLED_ENABLE
    // Format: `Key :  R{row}  C{col} K{kc} {name}{name}{name}`
    //
    // Where `kc` is lower 8 bit of keycode.
    // Where `name`s are readable labels for pressing keys, valid between 4 and 56.
    //
    // `row`, `col`, and `kc` indicates the last processed key,
    // but `name`s indicate unreleased keys in best effort.
    //
    // It is aligned to fit with output of keyball_oled_render_ballinfo().
    // For example:
    //
    //     Key :  R2  C3 K06 abc
    //     Ball:   0   0   0   0

    // "Key" Label
    oled_write_P(PSTR("Key \xB1"), false);

    // Row and column
    oled_write_char('\xB8', false);
    oled_write_char(to_1x(keyball.last_pos.row), false);
    oled_write_char('\xB9', false);
    oled_write_char(to_1x(keyball.last_pos.col), false);

    // Keycode
    oled_write_P(PSTR("\xBA\xBB"), false);
    oled_write_char(to_1x(keyball.last_kc >> 4), false);
    oled_write_char(to_1x(keyball.last_kc), false);

    // Pressing keys
    oled_write_P(PSTR("  "), false);
    oled_write(keyball.pressing_keys, false);
#endif
}

void keyball_oled_render_layerinfo(void) {
#ifdef OLED_ENABLE
    // Format: `Layer:{layer state}`
    //
    // Output example:
    //
    //     Layer:-23------------
    //
    oled_write_P(PSTR("L\xB6\xB7r\xB1"), false);
    for (uint8_t i = 1; i < 8; i++) {
        oled_write_char((layer_state_is(i) ? to_1x(i) : BL), false);
    }
    oled_write_char(' ', false);

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    oled_write_P(PSTR("\xC2\xC3"), false);
    if (get_auto_mouse_enable()) {
        oled_write_P(LFSTR_ON, false);
    } else {
        oled_write_P(LFSTR_OFF, false);
    }

    oled_write(format_4d(get_auto_mouse_timeout() / 10) + 1, false);
    oled_write_char('0', false);
#    else
    oled_write_P(PSTR("\xC2\xC3\xB4\xB5 ---"), false);
#    endif
#endif
}

//////////////////////////////////////////////////////////////////////////////
// Public API functions

bool keyball_get_scroll_mode(void) {
    return keyball.scroll_mode;
}

void keyball_set_scroll_mode(bool mode) {
    if (mode != keyball.scroll_mode) {
        keyball.scroll_mode_changed = timer_read32();
        // OFF→ONの時だけ慣性の速度をリセットする。しばらく経ってから再度
        // スクロールモードに入った時に古い速度で急に滑り出すのを防ぐため。
        // ON→OFFの時はリセットしない（弾いた後に指を離した瞬間こそ慣性で
        // 滑らせたい瞬間なので、ここで消してしまうと機能そのものが働かなくなる）。
        if (mode) {
            keyball_scroll_inertia_reset();
        }
    }
    keyball.scroll_mode = mode;
}

// 超低速（精密作業）モード: PRC_MOキーの押下とレイヤー連動の2系統から要求され、
// どちらか一方でも要求していればON。片方だけ解除されても、もう一方が要求中なら
// CPIを戻さない（先に切れた方に引っ張られてCPIが元に戻ってしまう事故を防ぐ）。
static bool    precision_req_key    = false;
static bool    precision_req_layer  = false;
static uint8_t precision_saved_cpi  = 0;
static bool    precision_active     = false;

static void precision_apply(void) {
    bool want = precision_req_key || precision_req_layer;
    if (want && !precision_active) {
        precision_saved_cpi = keyball_get_cpi();
        uint8_t div = kb_precision_div_get();
        // 四捨五入で分周する（切り捨てだと分周値どおりの比率にならないため）。
        // なお、実CPIは100刻みが下限のため、ベースCPIが低い場合は指定した分周値まで
        // 到達できず100CPIに張り付くことがある（ハードウェアの制約）。
        uint16_t v = (precision_saved_cpi + div / 2) / div;
        keyball_set_cpi(v < 1 ? 1 : (uint8_t)v);
        precision_active = true;
    } else if (!want && precision_active) {
        keyball_set_cpi(precision_saved_cpi);
        precision_active = false;
    }
}

bool keyball_get_precision_mode(void) {
    return precision_active;
}

void keyball_set_precision_key(bool pressed) {
    precision_req_key = pressed;
    precision_apply();
}

void keyball_set_precision_layer(bool on) {
    precision_req_layer = on;
    precision_apply();
}

#if defined(RGBLIGHT_ENABLE) || defined(RGB_MATRIX_ENABLE)
// レイヤー連動LED: 通常（レイヤー0）のLED設定はkb_led_config（kb_settings.c、RGBLIGHT/
// RGB_MATRIX本体のEEPROM機能とは独立）に一元管理する。理由: オーバーライド中はバックエンド
// 本体の「現在の表示」がレイヤー側の色になっているため、本体側の状態を読んでも通常設定を
// 復元できず、オーバーライド中にSET_LEDされた変更がレイヤーを抜けた時に消えてしまう事故が
// あった。
static bool g_layer_led_overriding = false;

bool keyball_layer_led_overriding(void) {
    return g_layer_led_overriding;
}

void keyball_apply_normal_led(void) {
    kb_led_config_t cfg = kb_led_config_get();
#ifdef RGB_MATRIX_ENABLE
    if (cfg.effect_id == 0) {
        rgb_matrix_disable_noeeprom();
    } else {
        rgb_matrix_enable_noeeprom();
        rgb_matrix_mode_noeeprom(kb_hid_led_effect_to_rgb_matrix_mode(cfg.effect_id));
        rgb_matrix_sethsv_noeeprom(cfg.hue, cfg.sat, cfg.val);
        rgb_matrix_set_speed_noeeprom(cfg.speed);
    }
#else
    if (cfg.effect_id == 0) {
        rgblight_disable_noeeprom();
    } else {
        rgblight_enable_noeeprom();
        rgblight_mode_noeeprom(kb_hid_led_effect_is_seasonal(cfg.effect_id) ? RGBLIGHT_MODE_STATIC_LIGHT : kb_hid_led_effect_to_mode(cfg.effect_id));
        rgblight_sethsv_noeeprom(cfg.hue, cfg.sat, cfg.val);
        rgblight_set_speed_noeeprom(cfg.speed);
    }
#endif
}

// 実際にレイヤー連動LED/通常LEDのどちらを表示すべきか判定して即座に反映する（既に
// その状態である場合の冗長な書き込みスキップは行わない）。keyball_apply_layer_led本体
// （冗長スキップあり）と、ジェスチャーウェーブのオーバーライド終了時（現在の表示が
// ウェーブになっている＝どちらの状態であってもとにかく強制的に書き戻す必要がある）
// の両方から使う共通処理。
static void apply_layer_led_now(uint8_t hl) {
    bool           layer_led_on = kb_layer_led_enable_get();
    kb_layer_led_t layer_led    = layer_led_on ? kb_layer_led_get(hl) : (kb_layer_led_t){0};

    if (layer_led_on && layer_led.enabled) {
        g_layer_led_overriding = true;
#ifdef RGB_MATRIX_ENABLE
        rgb_matrix_mode_noeeprom(kb_hid_led_effect_to_rgb_matrix_mode(layer_led.effect_id));
        rgb_matrix_sethsv_noeeprom(layer_led.hue, layer_led.sat, layer_led.val);
        rgb_matrix_set_speed_noeeprom(layer_led.speed);
#else
        rgblight_mode_noeeprom(kb_hid_led_effect_is_seasonal(layer_led.effect_id) ? RGBLIGHT_MODE_STATIC_LIGHT : kb_hid_led_effect_to_mode(layer_led.effect_id));
        rgblight_sethsv_noeeprom(layer_led.hue, layer_led.sat, layer_led.val);
        rgblight_set_speed_noeeprom(layer_led.speed);
#endif
    } else {
        g_layer_led_overriding = false;
        keyball_apply_normal_led();
    }
}

void keyball_apply_layer_led(uint8_t hl) {
    // 起動演出中はLEDモードを奪わない（演出の終わりに今のレイヤーの表示へ戻す）
    if (keyball_boot_active()) return;
#if defined(GESTURE_ENABLE) && defined(RGB_MATRIX_ENABLE)
    // ジェスチャー連動LEDウェーブの表示中は、レイヤー切り替えでRGB_MATRIXモードを
    // 奪わない（2026-09-29修正）。複数ジェスチャーモードはレイヤー切り替えで有効化
    // する仕組みのため、「トラックボールを振ってウェーブが表示中」に「キーを離して
    // レイヤーが戻る」操作は頻繁にタイミングが重なる。ここでガードしないと表示が
    // 途中で奪われて乱れるだけでなく、旧実装（スロットの後片付けをGESTURE_WAVE()
    // 描画関数任せにしていた版）ではモードを奪われている間は後片付けが一切実行され
    // ず、スロットが永久にactiveのまま固まって以後ウェーブが二度と発火しなくなる
    // 不具合の原因になっていた（該当スロットがkeyball_gesture_wave_trigger()の
    // 「発火中は次を出さない」ガードを永久に塞いでしまうため）。後片付け自体は
    // keyball_gesture_wave_task()側に移したため今はそちらの不具合は起きないが、
    // 表示が途中で消える見た目の乱れは残るため、念のためここでもガードしておく。
    if (keyball_gesture_wave_overriding()) return;
#endif

    bool           layer_led_on = kb_layer_led_enable_get();
    kb_layer_led_t layer_led    = layer_led_on ? kb_layer_led_get(hl) : (kb_layer_led_t){0};

    if ((layer_led_on && layer_led.enabled) || g_layer_led_overriding) {
        apply_layer_led_now(hl);
    }
}

#if defined(GESTURE_ENABLE) && defined(RGB_MATRIX_ENABLE)
// ジェスチャーウェーブが今まさに発火中かどうかを見て、RGB_MATRIXモードを強制的に
// GESTURE_WAVEへ上書き/復帰する。通常LED・レイヤー連動LEDのどちらの設定が選ばれて
// いても関係なく、ジェスチャー発火の瞬間だけ最優先でウェーブを表示するための仕組み
// （詳細はkeyball.hのコメント参照）。housekeeping_task_kbから両ハーフで毎スキャン
// 呼ばれる。
static bool g_gesture_wave_overriding = false;  // LEDオフ時の全体上書き中のみtrue
static bool g_gesture_wave_drawing    = false;  // ウェーブ表示中（重ね描き・全体上書きどちらでも）

// ウェーブ1回分の表示時間。スタイル（シャープ/ブリージング）ごとに描画関数
// (rgb_matrix_user.incのGESTURE_WAVE/TRACKBALL_BREATH)が使っている式と必ず
// 揃えること（下のスロット後片付け判定に使うため）。
// 2026-09-30〜: 見た目・速さがモードごとの設定になったため、ここではmode引数なしの
// kb_gesture_wave_style_get()は呼べない（そもそもどのモードか分からない）。代わりに
// keyball_gesture_wave_trigger()が発火時点で解決してkeyball.gesture_wave_style/speed
// へ書き込んだ値を見る（発火中は常にこの2フィールドが「今表示中のウェーブの設定」を
// 表している。複数スロットが同時activeになることは現状の設計上ない＝トリガー側の
// ガードで直列化されているため、単一の解決済み値で問題ない）。
static uint16_t gesture_wave_duration_ms(void) {
    // 帯自体が動く時間（端から端まで流れきる時間）はGESTURE_WAVEと完全に同じ式
    // （2026-09-29〜、本人希望「動きはシャープと同じ」を受けてBREATHだけ遅くする
    // のをやめた）。
    uint16_t travel_ms = 900 - ((uint16_t)keyball.gesture_wave_speed * 700 / 255);  // 200〜900ms
    if (keyball.gesture_wave_style == KB_GESTURE_WAVE_STYLE_BREATH) {
        // BREATHは帯が通り過ぎた後もLEDごとに残像フェードが続くため、その猶予分
        // だけスロットの寿命を延ばす。rgb_matrix_user.incのTRACKBALL_BREATH()の
        // FADE_OUT_MSと必ず同じ値にすること（ズレると、猶予が足りずフェード完了前に
        // レイヤー連動LED側へ表示が戻ってしまう、または逆に無駄に長く居座る）。
        const uint16_t FADE_OUT_MS = 600;
        return travel_ms + FADE_OUT_MS;
    }
    return travel_ms;
}

void keyball_gesture_wave_task(void) {
    if (keyball_boot_active()) return;  // 起動演出中はLEDモードを切り替えない
    // スロットの後片付け（表示時間を過ぎたらactiveを戻す）はここで行う。以前は
    // GESTURE_WAVE()描画関数の中でだけ行っていたが、レイヤー連動LED側が
    // RGB_MATRIXモードを奪っている間は描画関数自体が呼ばれず後片付けができない
    // ままになり、そのスロットが永久にactiveのまま固まって以後ウェーブが
    // 二度と発火しなくなる不具合があった（keyball_gesture_wave_trigger()の
    // 「いずれかのスロットがactiveなら次を出さない」ガードが永久に塞がるため。
    // 2026-09-29発見・修正）。housekeeping_task_kbからRGB_MATRIXモードに関係なく
    // 毎スキャン呼ばれるこの関数に移すことで、モードが何であっても確実に
    // 解放されるようにした。
    uint16_t wave_ms = gesture_wave_duration_ms();
    bool     active  = false;
    for (uint8_t s = 0; s < KEYBALL_GESTURE_WAVE_SLOT_COUNT; s++) {
        if (!keyball.gesture_wave_active[s]) continue;
        if (timer_elapsed(keyball.gesture_wave_start[s]) >= wave_ms) {
            keyball.gesture_wave_active[s] = false;
        } else {
            active = true;
        }
    }

    // 2026-10-02〜: 通常はRGB_MATRIXモードを切り替えず、今のエフェクトの上に重ね描き
    // する（rgb_matrix_user.incのkeyball_gesture_wave_overlay()）。アンダーグローを
    // 一切変えないため。LED自体がオフ設定の時だけは重ね描きの土台が無いので、従来通り
    // モードごとウェーブへ切り替える（その場合アンダーグローは消灯のまま描かれる）。
    if (active && !g_gesture_wave_drawing) {
        g_gesture_wave_drawing = true;
        if (!rgb_matrix_is_enabled()) {
            g_gesture_wave_overriding = true;
            rgb_matrix_enable_noeeprom();
            rgb_matrix_mode_noeeprom(keyball.gesture_wave_style == KB_GESTURE_WAVE_STYLE_BREATH ? RGB_MATRIX_CUSTOM_TRACKBALL_BREATH : RGB_MATRIX_CUSTOM_GESTURE_WAVE);
        }
    } else if (!active && g_gesture_wave_drawing) {
        g_gesture_wave_drawing = false;
        if (g_gesture_wave_overriding) {
            g_gesture_wave_overriding = false;
            apply_layer_led_now(get_highest_layer(layer_state));
        }
    }
}

bool keyball_gesture_wave_overriding(void) {
    return g_gesture_wave_overriding;
}

bool keyball_gesture_wave_drawing(void) {
    return g_gesture_wave_drawing;
}
#endif

#ifdef RGBLIGHT_ENABLE
// 季節限定LEDエフェクト（RGBLIGHT版のみ）: 単色モードの色を毎フレーム少しずつ変えることで
// クロスフェードさせる（RGBLIGHT本体のクリスマスエフェクトと似た「ゆっくり色が移り変わる」
// 動き）。RGB_MATRIX版はこれとは別に、クリスマスと同じ「市松模様に交互点灯」する動きの
// 専用エフェクト(HALLOWEEN・EASTER)をrgb_matrix_user.incに自作しており、本関数は使わない。
//
// 以前は全LEDを直接rgblight_setrgb_at()+rgblight_set()で書き換えていたが、この方法は
// RGBLIGHT本体の状態（rgblight_config）を経由しないため、スプリット同期の仕組み
// （変更のあったconfigだけをマスターからスレーブへ送る）に乗らず、スレーブ側は
// 「最初に一度届いた色のまま」の単色表示になってしまっていた（実機で確認）。
// rgblight_sethsv_noeeprom()経由にすることで、RGBLIGHT本体の標準の同期経路にそのまま
// 乗るようにしている。
typedef struct {
    uint8_t hue[3];
    uint8_t count;  // 2または3
} seasonal_palette_t;

static seasonal_palette_t seasonal_palette_for(uint8_t effect_id) {
    switch (effect_id) {
        case KB_LED_EFFECT_HALLOWEEN: return (seasonal_palette_t){.hue = {21, 191, 0}, .count = 2};    // オレンジ×紫
        case KB_LED_EFFECT_EASTER:    return (seasonal_palette_t){.hue = {228, 90, 42}, .count = 3};   // パステルピンク×パステルグリーン×パステルイエロー
        default:                      return (seasonal_palette_t){.hue = {0, 85, 0}, .count = 2};      // 万一の不正値
    }
}

// パレットの色数に関わらず同じ巡回ロジックで描画する。2色の場合は
// 「0→1→0→1…」と巡回するだけで、ピンポン往復と体感上同じ動きになる。
static uint8_t  g_seasonal_pos       = 0;  // 0 .. (32*count - 1) を巡回
static uint16_t g_seasonal_last_tick = 0;

void keyball_seasonal_led_task(void) {
    // ハロウィン・イースター。effect_idの解決がスプリットのスレーブ側で不正確でも、
    // マスターが毎フレームsethsv_noeeprom()系で色を押し出すことで標準の同期経路に
    // そのまま乗るため実害がない（詳細はファイル冒頭のコメント参照）。
    uint8_t        hl       = get_highest_layer(layer_state);
    kb_layer_led_t override = kb_layer_led_enable_get() ? kb_layer_led_get(hl) : (kb_layer_led_t){0};

    uint8_t effect_id, sat, val, speed;
    if (override.enabled) {
        effect_id = override.effect_id;
        sat       = override.sat;
        val       = override.val;
        speed     = override.speed;
    } else {
        kb_led_config_t cfg = kb_led_config_get();
        effect_id            = cfg.effect_id;
        sat                  = cfg.sat;
        val                  = cfg.val;
        speed                = cfg.speed;
    }

    if (!kb_hid_led_effect_is_seasonal(effect_id)) return;  // 通常のモードはバックエンド本体のtaskに任せる

    uint16_t interval = 60 - ((uint16_t)speed * 55 / 255);  // 5〜60ms（speedが大きいほど速い）
    if (timer_elapsed(g_seasonal_last_tick) < interval) return;
    g_seasonal_last_tick = timer_read();

    seasonal_palette_t pal     = seasonal_palette_for(effect_id);
    const uint8_t      max_pos = 32;
    uint8_t             total   = max_pos * pal.count;
    if (g_seasonal_pos >= total) g_seasonal_pos = 0;  // count変更直後の安全策

    uint8_t segment   = g_seasonal_pos / max_pos;
    uint8_t local_pos = g_seasonal_pos % max_pos;
    uint8_t hue_from  = pal.hue[segment];
    uint8_t hue_to    = pal.hue[(segment + 1) % pal.count];

    // 色相を円環上の短い経路で補間する（例: 21→191の場合、遠回りせず255側から回る）。
    // 直接補間すると無関係な色を経由してしまうことがあるため。
    int16_t diff = (int16_t)hue_to - (int16_t)hue_from;
    if (diff > 128) diff -= 256;
    if (diff < -128) diff += 256;

    const uint8_t max_pos_bezier = max_pos;
    uint32_t      xa             = (uint32_t)local_pos * local_pos * local_pos;
    uint32_t      xb             = (uint32_t)(max_pos_bezier - local_pos) * (max_pos_bezier - local_pos) * (max_pos_bezier - local_pos);
    uint8_t       t              = (uint8_t)(((uint32_t)255 * xa) / (xa + xb));  // 0-255の補間係数

    uint8_t blended_hue = (uint8_t)(hue_from + (diff * (int32_t)t) / 255);

    rgblight_mode_noeeprom(RGBLIGHT_MODE_STATIC_LIGHT);
    rgblight_sethsv_noeeprom(blended_hue, sat, val);

    g_seasonal_pos = (uint8_t)((g_seasonal_pos + 1) % total);
}
#endif // RGBLIGHT_ENABLE（季節限定LEDエフェクト、RGBLIGHT版のみ）
#endif // defined(RGBLIGHT_ENABLE) || defined(RGB_MATRIX_ENABLE)（レイヤー連動LED、両対応）

keyball_scrollsnap_mode_t keyball_get_scrollsnap_mode(void) {
#if KEYBALL_SCROLLSNAP_ENABLE == 2
    return keyball.scrollsnap_mode;
#else
    return 0;
#endif
}

void keyball_set_scrollsnap_mode(keyball_scrollsnap_mode_t mode) {
#if KEYBALL_SCROLLSNAP_ENABLE == 2
    keyball.scrollsnap_mode = mode;
#endif
}

uint8_t keyball_get_scroll_div(void) {
    return keyball.scroll_div == 0 ? KEYBALL_SCROLL_DIV_DEFAULT : keyball.scroll_div;
}

void keyball_set_scroll_div(uint8_t div) {
    keyball.scroll_div = div > SCROLL_DIV_MAX ? SCROLL_DIV_MAX : div;
}

uint8_t keyball_get_cpi(void) {
    return keyball.cpi_value == 0 ? CPI_DEFAULT : keyball.cpi_value;
}

void keyball_set_cpi(uint8_t cpi) {
    if (cpi > CPI_MAX) {
        cpi = CPI_MAX;
    }
    keyball.cpi_value   = cpi;
    keyball.cpi_changed = true;
    if (keyball.this_have_ball) {
        pmw3360_cpi_set(cpi == 0 ? CPI_DEFAULT - 1 : cpi - 1);
    }
}

uint8_t keyball_get_accel(void) {
    return keyball_accel_value;
}

void keyball_set_accel(uint8_t accel) {
    keyball_accel_value = accel > ACCEL_MAX ? ACCEL_MAX : accel;
}

//////////////////////////////////////////////////////////////////////////////
// Keyboard hooks

void keyboard_post_init_kb(void) {
#ifdef SPLIT_KEYBOARD
    // register transaction handlers on secondary.
    if (!is_keyboard_master()) {
        transaction_register_rpc(KEYBALL_GET_INFO, rpc_get_info_handler);
        transaction_register_rpc(KEYBALL_GET_MOTION, rpc_get_motion_handler);
        transaction_register_rpc(KEYBALL_SET_CPI, rpc_set_cpi_handler);
#ifdef GESTURE_ENABLE
        transaction_register_rpc(KEYBALL_GESTURE_WAVE, rpc_gesture_wave_handler);
#endif
    }
#endif

    // read keyball configuration from EEPROM
    if (eeconfig_is_enabled()) {
        keyball_config_t c = {.raw = eeconfig_read_kb()};
        if (c.magic != KEYBALL_CONFIG_MAGIC) {
            // 未初期化・別フォーマット・旧ファームの残存データを正規の設定として
            // 誤読しないよう、目印(magic)不一致時は安全な既定値へリセットする
            // （kb_settings.cと同じ考え方）。
            c      = (keyball_config_t){0};
            c.magic = KEYBALL_CONFIG_MAGIC;
            eeconfig_update_kb(c.raw);
        }
        keyball_set_cpi(c.cpi);
        keyball_set_scroll_div(c.sdiv);
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
        set_auto_mouse_enable(c.amle);
        set_auto_mouse_timeout(c.amlto == 0 ? AUTO_MOUSE_TIME : (c.amlto + 1) * AML_TIMEOUT_QU);
#endif
#if KEYBALL_SCROLLSNAP_ENABLE == 2
        keyball_set_scrollsnap_mode(c.ssnap);
#endif
        keyball_set_accel(c.accel);
    }

    keyball_on_adjust_layout(KEYBALL_ADJUST_PENDING);

#if defined(RGBLIGHT_ENABLE) || defined(RGB_MATRIX_ENABLE)
    // 通常（レイヤー0）のLED設定をkb_led_config（独自管理）から起動時に反映する。
    // RGB_MATRIX版もGET/SET_LEDがkb_led_config経由になったため、RGBLIGHT版と同様に
    // ここで反映するだけでよい（以前はEEPROM未初期化を強制リセットする暫定処理が
    // あったが、正式にkb_led_config経由になったので不要になった）。
    keyball_apply_normal_led();
#endif
#if defined(SPLIT_KEYBOARD) && defined(RGB_MATRIX_ENABLE)
    keyball_boot_init();
#endif

    keyboard_post_init_user();
}

#if SPLIT_KEYBOARD
void housekeeping_task_kb(void) {
    if (is_keyboard_master()) {
        rpc_get_info_invoke();
        led_resync_task();
        if (keyball.that_have_ball) {
            rpc_get_motion_invoke();
            rpc_set_cpi_invoke();
        }
#ifdef GESTURE_ENABLE
        rpc_gesture_wave_invoke();
#endif
    }
#ifdef RGB_MATRIX_ENABLE
    keyball_boot_task();
#endif
    // ウェーブのオーバーライド判定は両ハーフが自分のLED表示について独立に決めるため、
    // is_keyboard_master()の外（両ハーフで毎回実行）に置く。
#if defined(GESTURE_ENABLE) && defined(RGB_MATRIX_ENABLE)
    keyball_gesture_wave_task();
#endif
}
#endif

#ifdef OLED_ENABLE
static void pressing_keys_update(uint16_t keycode, keyrecord_t *record) {
    // Process only valid keycodes.
    if (keycode >= 4 && keycode < 57) {
        char value = pgm_read_byte(code_to_name + keycode - 4);
        char where = BL;
        if (!record->event.pressed) {
            // Swap `value` and `where` when releasing.
            where = value;
            value = BL;
        }
        // Rewrite the last `where` of pressing_keys to `value` .
        for (int i = 0; i < KEYBALL_OLED_MAX_PRESSING_KEYCODES; i++) {
            if (keyball.pressing_keys[i] == where) {
                keyball.pressing_keys[i] = value;
                break;
            }
        }
    }
}
#endif

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
bool is_mouse_record_kb(uint16_t keycode, keyrecord_t* record) {
    switch (keycode) {
        case SCRL_MO:
            return true;
    }
    return is_mouse_record_user(keycode, record);
}
#endif

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    // store last keycode, row, and col for OLED
    keyball.last_kc  = keycode;
    keyball.last_pos = record->event.key;

#ifdef OLED_ENABLE
    pressing_keys_update(keycode, record);
#endif

    if (!process_record_user(keycode, record)) {
        return false;
    }

    // strip QK_MODS part.
    if (keycode >= QK_MODS && keycode <= QK_MODS_MAX) {
        keycode &= 0xff;
    }

    switch (keycode) {
#ifndef MOUSEKEY_ENABLE
        // process KC_MS_BTN1~8 by myself
        // See process_action() in quantum/action.c for details.
        case MS_BTN1 ... MS_BTN8: {
            extern void register_mouse(uint8_t mouse_keycode, bool pressed);
            register_mouse(keycode, record->event.pressed);
            // to apply QK_MODS actions, allow to process others.
            return true;
        }
#endif

        case SCRL_MO:
            keyball_set_scroll_mode(record->event.pressed);
            // process_auto_mouse may use this in future, if changed order of
            // processes.
            return true;

        case PRC_MO:
            // 超低速（精密作業）モード: 押している間だけCPIを分周値で割る
            keyball_set_precision_key(record->event.pressed);
            return true;
    }

    // process events which works on pressed only.
    if (record->event.pressed) {
        switch (keycode) {
            case KBC_RST:
                keyball_set_cpi(0);
                keyball_set_scroll_div(0);
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
                set_auto_mouse_enable(false);
                set_auto_mouse_timeout(AUTO_MOUSE_TIME);
#endif
                break;
            case KBC_SAVE: {
                keyball_config_t c = {
                    .cpi   = keyball.cpi_value,
                    .sdiv  = keyball.scroll_div,
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
                    .amle  = get_auto_mouse_enable(),
                    .amlto = (get_auto_mouse_timeout() / AML_TIMEOUT_QU) - 1,
#endif
#if KEYBALL_SCROLLSNAP_ENABLE == 2
                    .ssnap = keyball_get_scrollsnap_mode(),
#endif
                    .accel = keyball_accel_value,
                };
                eeconfig_update_kb(c.raw);
            } break;

            case CPI_I100:
                add_cpi(1);
                break;
            case CPI_D100:
                add_cpi(-1);
                break;
            case CPI_I1K:
                add_cpi(10);
                break;
            case CPI_D1K:
                add_cpi(-10);
                break;

            case SCRL_TO:
                keyball_set_scroll_mode(!keyball.scroll_mode);
                break;
            case SCRL_DVI:
                add_scroll_div(1);
                break;
            case SCRL_DVD:
                add_scroll_div(-1);
                break;

#if KEYBALL_SCROLLSNAP_ENABLE == 2
            case SSNP_HOR:
                keyball_set_scrollsnap_mode(KEYBALL_SCROLLSNAP_MODE_HORIZONTAL);
                break;
            case SSNP_VRT:
                keyball_set_scrollsnap_mode(KEYBALL_SCROLLSNAP_MODE_VERTICAL);
                break;
            case SSNP_FRE:
                keyball_set_scrollsnap_mode(KEYBALL_SCROLLSNAP_MODE_FREE);
                break;
#endif

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
            case AML_TO:
                set_auto_mouse_enable(!get_auto_mouse_enable());
                break;
            case AML_I50:
                {
                    uint16_t v = get_auto_mouse_timeout() + 50;
                    set_auto_mouse_timeout(MIN(v, AML_TIMEOUT_MAX));
                }
                break;
            case AML_D50:
                {
                    uint16_t v = get_auto_mouse_timeout() - 50;
                    set_auto_mouse_timeout(MAX(v, AML_TIMEOUT_MIN));
                }
                break;
            case AML_OFF:
                auto_mouse_layer_off();
                break;
#endif

            default:
                return true;
        }
        return false;
    }

    return true;
}

// Disable functions keycode_config() and mod_config() in keycode_config.c to
// reduce size.  These functions are provided for customizing magic keycode.
// These two functions are mostly unnecessary if `MAGIC_KEYCODE_ENABLE = no` is
// set.
//
// If `MAGIC_KEYCODE_ENABLE = no` and you want to keep these two functions as
// they are, define the macro KEYBALL_KEEP_MAGIC_FUNCTIONS.
//
// See: https://docs.qmk.fm/#/squeezing_avr?id=magic-functions
//
#if !defined(MAGIC_KEYCODE_ENABLE) && !defined(KEYBALL_KEEP_MAGIC_FUNCTIONS)

uint16_t keycode_config(uint16_t keycode) {
    return keycode;
}

uint8_t mod_config(uint8_t mod) {
    return mod;
}

#endif
