// LED_TEST: 指定インデックスのLEDのみ点灯（LED診断用）
// hue フィールドにLEDインデックスを格納。rgb_matrix_config は左右同期済みなので両側で動作する
RGB_MATRIX_EFFECT(LED_TEST)

// REACTIVE_KEYS: 押したキーのLEDだけが光る（隣接LEDへの波及なし）。
// アンダーグロー(LED_FLAG_UNDERGLOW)は常時、設定色でそのまま点灯し続ける。
// g_last_hit_tracker（両ハーフでそれぞれローカルに更新される）を使うため左右両側で
// 独立して動作する。旧SOLID_RIPPLE（波紋演出）から仕様変更（2026-09-04）。
// LEDごとに直接フェード状態を持つ設計にしたことで、旧実装にあった以下の問題も
// 併せて解消している：8個までしか同時に覚えられない・バッファシフトで誤検出する、
// といった制約や不具合が構造的に起きなくなった（LEDの数だけ独立した状態を持つため）。
RGB_MATRIX_EFFECT(REACTIVE_KEYS)

// 以下4つはRGBLIGHT版に存在したがRGB_MATRIXに組み込みが無いエフェクトの再現。
// いずれも片側ハーフのLED範囲(led_min〜led_max)だけを使ってローカルに完結させて
// いるため、RGBLIGHT版で問題になっていたスプリット両ハーフ間の同期は考慮不要。
RGB_MATRIX_EFFECT(SNAKE)
RGB_MATRIX_EFFECT(KNIGHT)
RGB_MATRIX_EFFECT(CHRISTMAS)
RGB_MATRIX_EFFECT(ALTERNATING)

// ハロウィン・イースター（季節限定）。RGBLIGHT版は単色クロスフェードだったが、
// 本人希望によりクリスマスと同じ「市松模様に交互点灯」する動きに揃えている。
RGB_MATRIX_EFFECT(HALLOWEEN)
RGB_MATRIX_EFFECT(EASTER)

// TWINKLE: 基本は設定色で常時点灯し、ランダムなLEDが時々明るく煌めいてから元に戻る
// （星の瞬きのような演出）。組み込みのRAINDROPS/STARLIGHTは常に色相・明るさが
// 揺れ動いてしまい「基本は常時点灯」という要望に合わなかったため自作した。
RGB_MATRIX_EFFECT(TWINKLE)

// TRACKBALL: トラックボールを転がした向き・速さに応じて全LEDが一瞬色相を変えつつ
// 明るく光り、動きが止まると元の設定色にフェードして戻る。Keyballならではの演出。
RGB_MATRIX_EFFECT(TRACKBALL)

// RIPPLE: 押したキーの物理座標(g_led_config.point)を中心に波紋がリング状に広がる。
// アンダーグロー(LED_FLAG_UNDERGLOW)は常時、設定色でそのまま点灯し続ける
// （本人希望。REACTIVE_KEYSと同じ扱い）。
RGB_MATRIX_EFFECT(RIPPLE)

// HEATMAP: 押したキーだけをLEDインデックス単位で蓄熱・減衰させる自作タイピング
// ヒートマップ。QMK組み込みのTYPING_HEATMAPは
// (1) 隣接キーにも熱が伝播するため押していないキーまで反応する
// (2) 色相を熱量から内部計算する固定仕様で、ユーザーの色相設定を無視する
// という2点が要望に合わなかったため、g_last_hit_trackerとLEDごとの蓄熱テーブルに
// よる自作実装に置き換えている（色相・彩度はrgb_matrix_config.hsvをそのまま使う）。
RGB_MATRIX_EFFECT(HEATMAP)

#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

// TRACKBALL専用: keyball.this_motion/that_motion（トラックボールの移動量）を
// 読むために必要。書き込みは一切行わない（ポインタ移動処理には影響しない）。
// keyball39.c等と同じく、keyboards/keyball/を基準にした相対パスで指定する
// （rgb_matrix_user.incはquantum/rgb_matrix/rgb_matrix.cから#includeされるため）。
// keyball.hはreport_mouse_t（tmk_core/protocol/report.h）に依存しているが、
// rgb_matrix.cの時点ではまだincludeされていないため先に読み込んでおく。
#include "report.h"
#include "lib/keyball/keyball.h"

static bool LED_TEST(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t idx = rgb_matrix_config.hsv.h;  // hue = テスト対象LEDインデックス（両側で同期済み）
    for (uint8_t i = led_min; i < led_max; i++) {
        if (i == idx) {
            rgb_matrix_set_color(i, 255, 100, 0);
        } else {
            rgb_matrix_set_color(i, 0, 0, 0);
        }
    }
    return rgb_matrix_check_finished_leds(led_max);
}

static bool REACTIVE_KEYS(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    // LEDインデックスごとに直近のヒット時刻を保持する（波紋と違い近隣への伝播が
    // ないので、LEDの数だけ独立したスロットを持てば十分。取りこぼしや誤検出の
    // 心配がない）。
    static uint16_t hit_start[RGB_MATRIX_LED_COUNT];
    static bool     hit_active[RGB_MATRIX_LED_COUNT];

    // 新規ヒットを反映
    uint8_t cnt = g_last_hit_tracker.count;
    if (cnt > LED_HITS_TO_REMEMBER) cnt = LED_HITS_TO_REMEMBER;
    for (uint8_t j = 0; j < cnt; j++) {
        if (g_last_hit_tracker.tick[j] > 30) continue;  // 十分新しいヒットのみ対象
        uint8_t idx = g_last_hit_tracker.index[j];
        if (idx < RGB_MATRIX_LED_COUNT) {
            hit_start[idx]  = timer_read();
            hit_active[idx] = true;
        }
    }

    const uint16_t FADE_MS = 250;  // 押してから消えるまでのフェード時間

    for (uint8_t i = led_min; i < led_max; i++) {
        HSV hsv = rgb_matrix_config.hsv;
        if (g_led_config.flags[i] & LED_FLAG_UNDERGLOW) {
            // アンダーグローは常時点灯（設定色そのまま）。
        } else {
            // キー連動LED: 押されていなければ消灯、押された直後はフェードアウト
            hsv.v = 0;
            if (hit_active[i]) {
                uint16_t elapsed = timer_elapsed(hit_start[i]);
                if (elapsed >= FADE_MS) {
                    hit_active[i] = false;
                } else {
                    hsv.v = 255 - (uint8_t)(((uint32_t)elapsed * 255) / FADE_MS);
                }
            }
        }
        RGB rgb = rgb_matrix_hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// SNAKE: 明るい先頭(head)がLED配列上を巡回し、後ろに向かって徐々に暗くなる
// 尻尾を引く（RGBLIGHT_MODE_SNAKEの再現）。片側ハーフのLED範囲内で完結する。
static bool SNAKE(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t count = led_max - led_min;
    if (count == 0) return rgb_matrix_check_finished_leds(led_max);

    // speedが大きいほど短い間隔でheadが1つ進む（50〜400ms）
    uint16_t interval = 400 - ((uint16_t)rgb_matrix_config.speed * 350 / 255);
    static uint16_t last_tick = 0;
    static uint8_t  head      = 0;
    if (timer_elapsed(last_tick) >= interval) {
        last_tick = timer_read();
        head      = (head + 1) % count;
    }

    // 尻尾のフェードは付けず、配線順(インデックス)で1個のLEDだけが点灯して進む
    // （複数箇所が同時に光ってバラバラに見えるという指摘を受けて変更）。
    HSV hsv = rgb_matrix_config.hsv;
    for (uint8_t i = led_min; i < led_max; i++) {
        uint8_t pos = i - led_min;
        HSV     out = hsv;
        if (pos != head) out.v = 0;
        RGB rgb = rgb_matrix_hsv_to_rgb(out);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// KNIGHT: 明るい点がLED配列の両端で折り返しながら往復する（ナイトライダー/Cylon
// スキャナー、RGBLIGHT_MODE_KNIGHTの再現）。
static bool KNIGHT(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t count = led_max - led_min;
    if (count == 0) return rgb_matrix_check_finished_leds(led_max);

    uint16_t interval = 300 - ((uint16_t)rgb_matrix_config.speed * 250 / 255);  // 50〜300ms
    static uint16_t last_tick = 0;
    static int8_t   pos       = 0;
    static int8_t   dir       = 1;
    if (timer_elapsed(last_tick) >= interval) {
        last_tick = timer_read();
        pos += dir;
        if (pos >= (int8_t)count - 1) { pos = count - 1; dir = -1; }
        if (pos <= 0) { pos = 0; dir = 1; }
    }

    const uint8_t TAIL_LEN = 4;
    HSV           hsv      = rgb_matrix_config.hsv;
    for (uint8_t i = led_min; i < led_max; i++) {
        int16_t dist = pos - (int16_t)(i - led_min);
        if (dist < 0) dist = -dist;
        HSV out = hsv;
        out.v   = (dist < TAIL_LEN) ? (uint8_t)((uint16_t)hsv.v * (TAIL_LEN - dist) / TAIL_LEN) : 0;
        RGB rgb = rgb_matrix_hsv_to_rgb(out);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// CHRISTMAS: 赤と緑を交互に配置し、一定間隔で入れ替える（RGBLIGHT_MODE_CHRISTMASの再現）。
// 複数色を市松模様に配置し、一定間隔でずらしていく（CHRISTMAS・HALLOWEEN・EASTERの
// 共通実装）。色数2なら「0→1→0→1…」と交互点灯、3なら3色が順にずれていく。
static void render_checkerboard(effect_params_t* params, const uint8_t* hues, uint8_t count, uint16_t* last_tick, uint8_t* phase) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    uint16_t interval = 1500 - ((uint16_t)rgb_matrix_config.speed * 1400 / 255);  // 100〜1500ms
    if (timer_elapsed(*last_tick) >= interval) {
        *last_tick = timer_read();
        *phase     = (uint8_t)((*phase + 1) % count);
    }

    for (uint8_t i = led_min; i < led_max; i++) {
        uint8_t slot = (uint8_t)(((i - led_min) + *phase) % count);
        HSV     out  = {hues[slot], rgb_matrix_config.hsv.s, rgb_matrix_config.hsv.v};
        RGB     rgb  = rgb_matrix_hsv_to_rgb(out);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
}

static bool CHRISTMAS(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    static const uint8_t hues[] = {0, 85};  // 赤・緑
    static uint16_t      last_tick = 0;
    static uint8_t       phase     = 0;
    render_checkerboard(params, hues, 2, &last_tick, &phase);
    return rgb_matrix_check_finished_leds(led_max);
}

// HALLOWEEN: オレンジ×紫の市松模様（色相はkeyball.cのRGBLIGHT版seasonal_palette_forと
// 揃えてある）。
static bool HALLOWEEN(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    static const uint8_t hues[] = {21, 191};  // オレンジ・紫
    static uint16_t      last_tick = 0;
    static uint8_t       phase     = 0;
    render_checkerboard(params, hues, 2, &last_tick, &phase);
    return rgb_matrix_check_finished_leds(led_max);
}

// EASTER: パステルピンク・グリーン・イエローの3色が順にずれていく市松模様。
static bool EASTER(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    static const uint8_t hues[] = {228, 90, 42};  // パステルピンク・グリーン・イエロー
    static uint16_t      last_tick = 0;
    static uint8_t       phase     = 0;
    render_checkerboard(params, hues, 3, &last_tick, &phase);
    return rgb_matrix_check_finished_leds(led_max);
}

static bool TWINKLE(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    // LEDごとに「今瞬いている最中か」「いつ始まったか」を持つ（REACTIVE_KEYSと同じ
    // 発想で、LEDの数だけ独立したスロットを持つため取りこぼしや誤検出の心配がない）。
    static uint16_t twinkle_start[RGB_MATRIX_LED_COUNT];
    static bool     twinkle_active[RGB_MATRIX_LED_COUNT];
    static uint16_t spawn_last_tick = 0;

    // RGB_MATRIX_USE_LIMITSは1フレームの描画を約10個ずつの小さな区間(chunk)に分けて
    // 複数回(iter)呼び出す仕組み。無し側では一時的にled_min > led_max（範囲が空・反転）
    // になる区間があり、単純なfor文（i=led_min; i<led_max; i++）なら自動的に何もせず
    // 安全だが、以下は配列の添字をled_min基準で直接計算するため、その保護が無いと
    // count(uint8_t)がアンダーフローして巨大な値になり、配列範囲外に書き込んでしまって
    // いた（実機でランダムなキーが反応しなくなる不具合として発現）。
    //
    // さらに、発生位置の抽選は「そのiterのchunk内」ではなくハーフ全体から行う必要がある
    // （chunk単位で抽選すると、判定が真っ先に成立する先頭chunk付近ばかりに偏って発生
    // していた）。ハーフ全体の範囲はconfig.hのRGB_MATRIX_SPLIT { 22, 24 }と一致
    // （ボール側=idx0-21、無し側=idx22-45、実測済み）。
    uint8_t half_min   = is_keyboard_left() ? 0 : 22;
    uint8_t half_max   = is_keyboard_left() ? 22 : RGB_MATRIX_LED_COUNT;
    uint8_t half_count = half_max - half_min;

    if (led_max > led_min) {
        // 瞬きの発生頻度: speedが大きいほど短い間隔で新しい瞬きが生まれる
        // 最速でもそこまで慌ただしくならないよう、全体的に遅めのレンジにしてある
        uint16_t spawn_interval = 3000 - ((uint16_t)rgb_matrix_config.speed * 2200 / 255);  // 800〜3000ms
        if (timer_elapsed(spawn_last_tick) >= spawn_interval) {
            spawn_last_tick     = timer_read();
            uint8_t idx         = half_min + random8_max(half_count);
            twinkle_start[idx]  = timer_read();
            twinkle_active[idx] = true;
        }
    }

    const uint16_t TWINKLE_MS = 1500;  // 瞬き1回分の長さ（明るくなって元に戻るまで）
    for (uint8_t i = led_min; i < led_max; i++) {
        HSV hsv = rgb_matrix_config.hsv;  // 基本は常時点灯（設定色そのまま）
        if (twinkle_active[i]) {
            uint16_t elapsed = timer_elapsed(twinkle_start[i]);
            if (elapsed >= TWINKLE_MS) {
                twinkle_active[i] = false;
            } else {
                // 0→山→0で明るさを一瞬持ち上げつつ彩度を落とし、白っぽく煌めかせる
                uint8_t  t     = (uint8_t)(((uint32_t)elapsed * 255) / TWINKLE_MS);
                uint8_t  boost = t < 128 ? (uint8_t)(t * 2) : (uint8_t)((255 - t) * 2);
                hsv.s          = (uint8_t)((uint16_t)hsv.s * (255 - boost) / 255);
                uint16_t v     = (uint16_t)hsv.v + ((uint16_t)(255 - hsv.v) * boost / 255);
                hsv.v          = v > 255 ? 255 : (uint8_t)v;
            }
        }
        RGB rgb = rgb_matrix_hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// TRACKBALL: keyball.this_motion/that_motion（トラックボールの移動量、split越しに
// 同期済み）を読み、動いていれば向きに応じた色相へ一瞬変えつつ明るさを上げ、止まると
// 元の設定色にフェードして戻る。read-onlyで値を変更しないため、ポインタ移動処理
// （keyball.c側でこれらの値を蓄積・消費している）には一切影響しない。
static bool TRACKBALL(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    // 自分のハーフにボールが無ければ相方ハーフの動きを見る（ボールがどちらの手でも
    // 同じように反応させるため）。
    int16_t dx = keyball.this_have_ball ? keyball.this_motion.x : keyball.that_motion.x;
    int16_t dy = keyball.this_have_ball ? keyball.this_motion.y : keyball.that_motion.y;
    uint8_t adx = (uint8_t)(dx < 0 ? -dx : dx);
    uint8_t ady = (uint8_t)(dy < 0 ? -dy : dy);

    static uint16_t last_move_tick = 0;
    static uint8_t  hold_hue       = 0;
    static uint8_t  hold_speed     = 0;

    if (adx > 0 || ady > 0) {
        last_move_tick = timer_read();

        // 向きを8方向に量子化して色相を割り当てる（atan2を使わない簡易版）。
        // 右=赤(0) 左=シアン(128) 下=青(192) 上=黄緑(64)、斜めはその中間。
        uint8_t hue;
        if (adx > (uint8_t)(ady * 2)) {
            hue = dx > 0 ? 0 : 128;
        } else if (ady > (uint8_t)(adx * 2)) {
            hue = dy > 0 ? 192 : 64;
        } else if (dx > 0) {
            hue = dy > 0 ? 224 : 32;
        } else {
            hue = dy > 0 ? 160 : 96;
        }
        hold_hue = hue;

        uint16_t mag = (uint16_t)adx + (uint16_t)ady;
        hold_speed   = mag > 255 ? 255 : (uint8_t)mag;
    }

    // 動きが止まってから元の色に戻るまでの時間。speedが大きいほど短く（キビキビ）なる
    const uint16_t DECAY_MS = 900 - ((uint16_t)rgb_matrix_config.speed * 700 / 255);  // 200〜900ms
    uint16_t       elapsed  = timer_elapsed(last_move_tick);
    uint8_t        glow     = (elapsed >= DECAY_MS) ? 0 : (uint8_t)((uint32_t)hold_speed * (DECAY_MS - elapsed) / DECAY_MS);

    HSV base = rgb_matrix_config.hsv;
    for (uint8_t i = led_min; i < led_max; i++) {
        HSV out = base;
        if (glow > 0) {
            out.h          = hold_hue;
            uint16_t val_v = (uint16_t)base.v + glow;
            out.v          = val_v > 255 ? 255 : (uint8_t)val_v;
        }
        RGB rgb = rgb_matrix_hsv_to_rgb(out);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// RIPPLE: 押したキーの物理座標(g_led_config.point)を中心に、リング状の波紋が
// 広がっていく。複数の波紋を同時に扱えるよう、g_last_hit_trackerとは別に専用の
// スロット配列を持つ（固定8スロットのリングバッファだった旧SOLID_RIPPLEで
// 「取りこぼす・波紋が途中で終わる」不具合があったため、空きスロット優先・
// 無ければ最古を上書きする方式にしている）。
#define RIPPLE_SLOT_COUNT 6
static bool RIPPLE(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    static uint16_t ripple_start[RIPPLE_SLOT_COUNT];
    static uint8_t  ripple_x[RIPPLE_SLOT_COUNT];
    static uint8_t  ripple_y[RIPPLE_SLOT_COUNT];
    static bool     ripple_active[RIPPLE_SLOT_COUNT];

    // 波紋が広がりきるまでの時間。speedが大きいほど短く（速く）なる
    const uint16_t RIPPLE_MS    = 1200 - ((uint16_t)rgb_matrix_config.speed * 1000 / 255);  // 200〜1200ms
    const uint8_t  RIPPLE_MAX_R = 100;  // 広がる最大半径（座標系はx:0-224,y:0-64程度）
    const uint8_t  RIPPLE_WIDTH = 30;   // リングの太さ

    // マトリクス状態は両ハーフ間で同期されているため、g_last_hit_trackerには
    // 反対側ハーフのキーのヒットも記録される。座標ベースのRIPPLEはこれを素通しすると
    // 波紋の中心座標が反対側ハーフの物理位置になり、中央付近のキー（col4等）を押した際に
    // 反対側ハーフのLEDまで反応してしまう（TWINKLE修正時と同じ理由でper-iterの
    // led_min/led_maxではなくハーフ全体の安定した範囲で判定する必要がある）。
    uint8_t half_min = is_keyboard_left() ? 0 : 22;
    uint8_t half_max = is_keyboard_left() ? 22 : RGB_MATRIX_LED_COUNT;

    // 新規ヒットを取り込む。g_last_hit_trackerはREACTIVE_KEYSと同じく「十分新しい
    // ものだけ」判定で、長時間使用時にtickが古い情報を誤って取り込むのを防ぐ。
    uint8_t cnt = g_last_hit_tracker.count;
    if (cnt > LED_HITS_TO_REMEMBER) cnt = LED_HITS_TO_REMEMBER;
    for (uint8_t j = 0; j < cnt; j++) {
        if (g_last_hit_tracker.tick[j] > 30) continue;
        uint8_t idx = g_last_hit_tracker.index[j];
        if (idx >= RGB_MATRIX_LED_COUNT) continue;
        if (idx < half_min || idx >= half_max) continue;  // 反対側ハーフのキーは無視
        if (g_led_config.flags[idx] & LED_FLAG_UNDERGLOW) continue;  // アンダーグローは起点にしない

        uint8_t slot = RIPPLE_SLOT_COUNT;
        for (uint8_t s = 0; s < RIPPLE_SLOT_COUNT; s++) {
            if (!ripple_active[s]) { slot = s; break; }
        }
        if (slot == RIPPLE_SLOT_COUNT) {
            // 空きが無ければ最も経過時間が長いスロットを上書きする
            uint16_t oldest = 0;
            for (uint8_t s = 0; s < RIPPLE_SLOT_COUNT; s++) {
                uint16_t e = timer_elapsed(ripple_start[s]);
                if (e >= oldest) { oldest = e; slot = s; }
            }
        }
        ripple_start[slot]  = timer_read();
        ripple_x[slot]      = g_led_config.point[idx].x;
        ripple_y[slot]      = g_led_config.point[idx].y;
        ripple_active[slot] = true;
    }

    HSV base = rgb_matrix_config.hsv;
    for (uint8_t i = led_min; i < led_max; i++) {
        HSV out = base;
        if (g_led_config.flags[i] & LED_FLAG_UNDERGLOW) {
            // アンダーグローは常時点灯（設定色そのまま）
        } else {
            out.v = 0;
            for (uint8_t s = 0; s < RIPPLE_SLOT_COUNT; s++) {
                if (!ripple_active[s]) continue;
                uint16_t elapsed = timer_elapsed(ripple_start[s]);
                if (elapsed >= RIPPLE_MS) continue;

                uint16_t radius = (uint16_t)((uint32_t)elapsed * RIPPLE_MAX_R / RIPPLE_MS);
                int16_t  dx     = (int16_t)g_led_config.point[i].x - (int16_t)ripple_x[s];
                int16_t  dy     = (int16_t)g_led_config.point[i].y - (int16_t)ripple_y[s];
                uint8_t  dist   = sqrt16((uint32_t)dx * dx + (uint32_t)dy * dy);
                uint16_t ring_d = (dist > radius) ? (dist - radius) : (radius - dist);

                if (ring_d < RIPPLE_WIDTH) {
                    uint8_t fade        = (uint8_t)(255 - ((uint32_t)elapsed * 255 / RIPPLE_MS));
                    uint8_t ring_bright = (uint8_t)(255 - ((uint32_t)ring_d * 255 / RIPPLE_WIDTH));
                    uint8_t v           = (uint8_t)(((uint16_t)fade * ring_bright) / 255);
                    if (v > out.v) out.v = v;  // 波紋同士が重なる場合は明るい方を採用
                }
            }
        }
        RGB rgb = rgb_matrix_hsv_to_rgb(out);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }

    for (uint8_t s = 0; s < RIPPLE_SLOT_COUNT; s++) {
        if (ripple_active[s] && timer_elapsed(ripple_start[s]) >= RIPPLE_MS) {
            ripple_active[s] = false;
        }
    }

    return rgb_matrix_check_finished_leds(led_max);
}

// ALTERNATING: LED範囲を前半・後半の2グループに分け、明暗を一定間隔で入れ替える
// （RGBLIGHT_MODE_ALTERNATINGの再現）。片側ハーフ内で完結させるため、RGBLIGHT版で
// 課題になっていたスプリット両ハーフ間の同期方式は考慮不要。
static bool ALTERNATING(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t count = led_max - led_min;
    if (count == 0) return rgb_matrix_check_finished_leds(led_max);

    uint16_t interval = 1500 - ((uint16_t)rgb_matrix_config.speed * 1400 / 255);
    static uint16_t last_tick = 0;
    static bool     phase     = false;
    if (timer_elapsed(last_tick) >= interval) {
        last_tick = timer_read();
        phase     = !phase;
    }

    uint8_t half = count / 2;
    HSV     hsv  = rgb_matrix_config.hsv;
    for (uint8_t i = led_min; i < led_max; i++) {
        bool first_group = (i - led_min) < half;
        HSV  out         = hsv;
        out.v            = (first_group == phase) ? hsv.v : 0;
        RGB rgb          = rgb_matrix_hsv_to_rgb(out);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// HEATMAP: 押したキーのLEDだけに熱量(0-255)を持たせ、押すたびに加算・時間経過で
// 減衰させる。明るさ(v)は熱量に関わらず設定値のまま保持し、代わりに色相を熱量に
// 応じて寒色(青)→暖色(赤)へ変化させる（本人希望。何度も押すほど色が変わって
// いく見た目にするため）。彩度はrgb_matrix_config.hsvの設定をそのまま使う。
// アンダーグロー(LED_FLAG_UNDERGLOW)は常時、設定色でそのまま点灯し続ける
// （RIPPLE・REACTIVE_KEYSと同じ扱い）。
#define HEATMAP_INCREASE_STEP    32   // 1打鍵あたりの熱量増加
#define HEATMAP_DECAY_INTERVAL_MS 25  // この間隔ごとに熱量を1ずつ下げる
static bool HEATMAP(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    static uint8_t  heat[RGB_MATRIX_LED_COUNT];
    static uint16_t heat_last_tick[RGB_MATRIX_LED_COUNT];
    static bool     heat_init = false;
    if (!heat_init) {
        // g_last_hit_tracker.tickの初期値0と衝突しないよう、大きな値で初期化しておく
        for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) heat_last_tick[i] = 0xFFFF;
        heat_init = true;
    }

    // 新規ヒットのみ蓄熱する。g_last_hit_trackerは同じ押下がtick<=30の間、複数フレーム
    // に渡って観測され続けるため、「前回見た時よりtickが小さくなった＝新しい押下」の
    // 場合だけ加算することで二重加算を防いでいる（RIPPLE等と違い、蓄積値を扱うため
    // 二重加算すると熱量が実際の打鍵より早く上がってしまう）。
    uint8_t cnt = g_last_hit_tracker.count;
    if (cnt > LED_HITS_TO_REMEMBER) cnt = LED_HITS_TO_REMEMBER;
    for (uint8_t j = 0; j < cnt; j++) {
        if (g_last_hit_tracker.tick[j] > 30) continue;  // 十分新しいヒットのみ対象
        uint8_t idx = g_last_hit_tracker.index[j];
        if (idx >= RGB_MATRIX_LED_COUNT) continue;
        if (g_last_hit_tracker.tick[j] < heat_last_tick[idx]) {
            heat[idx] = qadd8(heat[idx], HEATMAP_INCREASE_STEP);
        }
        heat_last_tick[idx] = g_last_hit_tracker.tick[j];
    }

    // 熱量の減衰タイミング判定はフレーム全体で1回だけ行う
    // （TYPING_HEATMAP同様、複数iterに分かれてもタイマーは1回しか進めない）
    static uint16_t decay_last_tick = 0;
    static bool     do_decay        = false;
    if (params->iter == 0) {
        do_decay = timer_elapsed(decay_last_tick) >= HEATMAP_DECAY_INTERVAL_MS;
        if (do_decay) decay_last_tick = timer_read();
    }

    HSV base = rgb_matrix_config.hsv;
    for (uint8_t i = led_min; i < led_max; i++) {
        HSV out = base;
        if (!(g_led_config.flags[i] & LED_FLAG_UNDERGLOW)) {
            if (heat[i] == 0) {
                out.v = 0;  // 一度も押されていない（または冷めきった）キーは消灯
            } else {
                // 熱量0〜255を色相170(青=寒色)〜0(赤=暖色)にマッピングする。
                // 明るさは熱量に関係なく設定値のまま（本人希望）。
                out.h = (uint8_t)(170 - ((uint16_t)heat[i] * 170 / 255));
                out.v = base.v;
            }
            if (do_decay && heat[i] > 0) heat[i]--;
        }
        RGB rgb = rgb_matrix_hsv_to_rgb(out);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
