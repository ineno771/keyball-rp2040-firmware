// Copyright 2024 keyball-custom contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#include <stdbool.h>

// ── EEPROM配置についての重要な注意（2026-09-04発覚・移動） ──────────────
// 以前はTD config=0x0200、kb_settings=0x0240、マクロ=0x0250…という配置だったが、
// これはQMK標準の動的キーマップ（Keyball LinkのSET_KEYCODEで実際に書き込まれる、
// DYNAMIC_KEYMAP_LAYER_COUNT×MATRIX_ROWS×MATRIX_COLS×2バイト分の領域）の実使用範囲
// 0x0025〜0x0324（8層×8行×6列×2=768バイト、EECONFIG_SIZE=37バイトの直後から）と
// 完全に重複しており、キー割り当てのたびにkb_settings本体やマクロ領域前半が静かに
// 破壊される実害あるバグだった（「dynamic_keymapは先頭〜約0x01C0までしか使わない」
// という当初の見積もりが誤りだった）。0x0800以降に全面移動して解消した。
// 新配置: TD config(0x0800-0x083F,64B) → kb_settings(0x0840-0x084F,16B)
//        → マクロ(0x0850-0x09DF,400B) → 本ファイル後半の各種設定(0x09E0-)
// 実測方法: nvm_dynamic_keymap.cに一時的な_Static_assertを仕込み、qmk compileの
// エラーメッセージから二分探索で実際の境界値を確認した（HANDOFF.md参照）。
#define KB_SETTINGS_EEPROM_BASE  0x0840
#define KB_SETTINGS_DEFAULT_TT   200  // デフォルト Tapping Term (ms)

// dynamic_keymapとの衝突をコンパイル時に検知する。DYNAMIC_KEYMAP_EEPROM_ADDR自体は
// nvm_dynamic_keymap.c内部限定のマクロで外部から参照できないため、同じ計算式
// （EECONFIG_SIZE + レイヤー数×行×列×2）をここで再現している。EECONFIG_SIZEの実測値
// (37)はVIA_ENABLE=off・EECONFIG_KB/USER_DATA_SIZE=0が前提。QMK更新でeeprom_core_tの
// サイズが変わった場合はこの数値がずれる可能性があるので、疑わしい場合は上記の実測
// 方法で確認し直すこと。
#define KB_DYNAMIC_KEYMAP_EEPROM_ADDR_ASSUMED 37
#define KB_DYNAMIC_KEYMAP_EEPROM_END_ASSUMED \
    (KB_DYNAMIC_KEYMAP_EEPROM_ADDR_ASSUMED + (DYNAMIC_KEYMAP_LAYER_COUNT) * (MATRIX_ROWS) * (MATRIX_COLS) * 2)
_Static_assert(KB_SETTINGS_EEPROM_BASE >= KB_DYNAMIC_KEYMAP_EEPROM_END_ASSUMED + 512,
               "kb_settings EEPROMがdynamic_keymap領域と衝突またはマージンが不足しています。KB_SETTINGS_EEPROM_BASEを見直してください。");

// ジェスチャーのデフォルト割り当て（macブラウザ標準・修飾子付きキーコード）
// 0x0800=LGUI(Cmd), 0x0200=LSFT, KC_LBRC=0x2F, KC_RBRC=0x30
#define KB_GESTURE_DEFAULT_UP    0x0A2F  // Cmd+Shift+[ 前のタブ
#define KB_GESTURE_DEFAULT_DOWN  0x0A30  // Cmd+Shift+] 次のタブ
#define KB_GESTURE_DEFAULT_LEFT  0x082F  // Cmd+[ 戻る
#define KB_GESTURE_DEFAULT_RIGHT 0x0830  // Cmd+] 進む

typedef struct {
    uint16_t tapping_term;   // 50-1000ms
    uint8_t  flags;          // KB_FLAG_* ビットフィールド
    uint8_t  aml_layer;      // 自動マウスレイヤーの対象レイヤー（0-7）
    uint16_t aml_timeout;    // 自動マウスレイヤーのタイムアウト(ms)
    uint8_t  aml_threshold;  // 自動マウスレイヤーの発動しきい値（移動量。小さいほど敏感）
} __attribute__((packed)) kb_settings_t;

#define KB_FLAG_AUTO_SHIFT       (1 << 0)
#define KB_FLAG_COMBO            (1 << 1)
#define KB_FLAG_PERMISSIVE_HOLD  (1 << 2)
#define KB_FLAG_RETRO_TAPPING    (1 << 3)
#define KB_FLAG_SCROLL_INV_V     (1 << 4)  // 縦スクロール反転
#define KB_FLAG_SCROLL_INV_H     (1 << 5)  // 横スクロール反転
#define KB_FLAG_AML_DISABLE      (1 << 6)  // 自動マウスレイヤー無効（0=有効・後方互換）
#define KB_FLAG_OS_AUTO_SWAP     (1 << 7)  // OS自動判別: Mac/iOS接続時にCmd(GUI)とCtrlを自動入れ替え（0=何もしない・既定）

// EEPROMから読み込む（初回のみ; 以降はRAMキャッシュを返す）
kb_settings_t kb_settings_get(void);

// EEPROMに書き込みRAMキャッシュも更新する
void kb_settings_set(const kb_settings_t *s);

// ── トラックボール動作レイヤー（kb_settings構造体は満杯のため、マクロ領域の
//    直後 0x09E0- に格納）──
#define KB_SCROLL_LAYER_EEPROM   0x09E0  // スクロールレイヤー保存先
// 0x09E1は旧・単一ジェスチャーレイヤー設定が使っていたが、複数ジェスチャーモード化
// （2026-09-09）でモードごとのレイヤー(KB_GESTURE_MODE_TABLE_EEPROM内)に置き換わり
// 未使用になった。他の設定と隣接させたくないため空き番地のまま残している。
#define KB_GESTURE_TH_H_EEPROM  0x09E2  // ジェスチャー横方向しきい値保存先
#define KB_GESTURE_TH_V_EEPROM  0x09E3  // ジェスチャー縦方向しきい値保存先
#define KB_PRECISION_DIV_EEPROM   0x09E4  // 超低速モードの分周値保存先
#define KB_PRECISION_LAYER_EEPROM 0x09E5  // 超低速モードの連動レイヤー保存先
#define KB_LAYER_NONE            0xFE    // 「なし」を表す値（0xFF=未初期化と区別）

// 2026-09-09発覚: RP2040のEEPROM(wear leveling方式)は、一度も書き込んだことのない
// 領域が0xFFではなく0x00で初期化される。KB_SCROLL_LAYER_EEPROM/KB_PRECISION_LAYER_EEPROM
// は「生バイト0-7=そのままレイヤー番号」という設計のため、未書き込みの0x00が
// 「レイヤー0」と区別できず、一度もWeb UIで保存したことがない設定が常にレイヤー0に
// 連動してしまう事故が起きた（レイヤー0でトラックボールが握りつぶされ動かなくなる）。
// レイヤー0自体は精密モードなどで有効な選択肢のため、生バイトの意味を変える対応
// （0オフセットなど）は避け、代わりに「一度でも実際に保存されたか」を示す目印を
// 別バイトに持たせ、目印が無い間は生バイトを信用せず既定値を返すようにした。
#define KB_TRACKBALL_LAYERS_MAGIC_EEPROM 0x0A41  // スクロール/超低速レイヤーが実際に保存済みかの目印(1バイト)
#define KB_TRACKBALL_LAYERS_MAGIC_VALUE  0x7A

// スクロールレイヤー（0-7=そのレイヤーでスクロール / KB_LAYER_NONE=無効。既定3）
uint8_t kb_scroll_layer_get(void);
void    kb_scroll_layer_set(uint8_t v);

#ifdef GESTURE_ENABLE
// ジェスチャー発動しきい値（移動量の累積。小さいほど敏感。既定50、範囲10-200）
// 横方向(左右)・縦方向(上下)を別々に持つ。指の動かし方の癖に合わせて片方だけ調整できる。
// 4モード共通（モードが変わるのは「動きの意味」であって「感度」ではないため）。
#define KB_GESTURE_TH_DEFAULT 50
#define KB_GESTURE_TH_MIN     10
#define KB_GESTURE_TH_MAX     200
uint8_t kb_gesture_th_h_get(void);
void    kb_gesture_th_h_set(uint8_t v);
uint8_t kb_gesture_th_v_get(void);
void    kb_gesture_th_v_set(uint8_t v);

// ── 複数ジェスチャーモード（2026-09-09〜）─────────────────────────
// 4つの独立したジェスチャーモードを持つ。各モードは上下左右の割当キーと、
// 方向ごとの「連続入力」ON/OFF、連動レイヤー（0-7 / KB_LAYER_NONEでなし）を持つ。
// モード選択はGST_HOLD〜4キー（押している間だけ優先）またはレイヤー連動で行う
// （選択ロジック自体はkeymap.c側の責務。ここは設定の保存・取得のみ）。
#define KB_GESTURE_MODE_COUNT 4

typedef struct {
    uint16_t key[4];     // 割当キー [0]上 [1]下 [2]左 [3]右（0=未設定）
    uint8_t  continuous; // 方向ごとの連続入力ON/OFF（bit0=上,bit1=下,bit2=左,bit3=右）
    uint8_t  layer;      // 連動レイヤー（0-7 / KB_LAYER_NONEでなし）
} __attribute__((packed)) kb_gesture_mode_t;

// レイヤー連動LEDテーブル(0x09E7-0x0A10)の直後、慣性スクロール設定(-0x0A18)の
// さらに直後の空き領域。4モード×10バイト=40バイト（0x0A19-0x0A40）。
// 直後の0x0A41はKB_TRACKBALL_LAYERS_MAGIC_EEPROM、0x0A42はKB_GESTURE_WAVE_SPEED_EEPROM、
// 0x0A43はKB_GESTURE_WAVE_ENABLE_EEPROMで使用済み。さらに0x0A44-0x0A46はシェイク、
// 0x0A47-0x0A50はダブルフリック、0x0A51-0x0A52は両者の有効/無効フラグの設定で
// 使用済み（本ファイル末尾参照）。次にここへ設定を追加する場合は0x0A53以降を使うこと。
#define KB_GESTURE_MODE_TABLE_EEPROM 0x0A19
#define KB_GESTURE_MODE_ENTRY_SIZE   10

// モードN（0-3）の設定を取得・変更する
kb_gesture_mode_t kb_gesture_mode_get(uint8_t mode);
void              kb_gesture_mode_set(uint8_t mode, const kb_gesture_mode_t *cfg);

// ── ジェスチャー連動LEDウェーブの速さ（2026-09-09〜）─────────────────
// ウェーブが端から端まで流れきる速さ（大きいほど速い＝すぐ消える）。通常LED・
// レイヤー連動LEDのどちらの速度設定とも独立している（ウェーブは選択式のエフェクト
// ではなく、ジェスチャー発火時に現在の光り方を一時的に上書きする演出のため、
// 速度だけ専用の設定を持つ）。
// 下限を0ではなく1にしているのは、RP2040のEEPROM(wear leveling方式)が未書き込み
// 領域を0x00で初期化するため（kb_settings.h冒頭のKB_TRACKBALL_LAYERS_MAGIC_EEPROM
// 参照）。0を有効値に含めると未設定と区別できなくなるので、他のしきい値設定
// （KB_GESTURE_TH_*等）と同じく「範囲外なら既定値」方式で回避する。
// KB_GESTURE_MODE_TABLE_EEPROM(0x0A19-0x0A40)・KB_TRACKBALL_LAYERS_MAGIC_EEPROM(0x0A41)
// の直後の空き番地（コメント参照）。
#define KB_GESTURE_WAVE_SPEED_EEPROM  0x0A42
#define KB_GESTURE_WAVE_SPEED_MIN     1
#define KB_GESTURE_WAVE_SPEED_MAX     255
#define KB_GESTURE_WAVE_SPEED_DEFAULT 200
uint8_t kb_gesture_wave_speed_get(void);
void    kb_gesture_wave_speed_set(uint8_t v);

// ジェスチャー連動LEDウェーブ機能そのものの有効/無効（既定: 有効）。他の有効/無効
// フラグ（kb_layer_led_enable等）と違い既定がONなので、判定を反転させている：
// RP2040のEEPROM(wear leveling方式)は未書き込み領域が0x00になるため、通常の
// 「1だけが有効」方式のままだと未設定時に既定でOFFになってしまう。代わりに
// 「1という値だけが明示的なOFF、それ以外(未書込みの0x00含む)は既定のON」とする
// ことで、未設定時にちゃんと既定ONになるようにしている。
#define KB_GESTURE_WAVE_ENABLE_EEPROM 0x0A43
bool kb_gesture_wave_enable_get(void);
void kb_gesture_wave_enable_set(bool v);
#endif

// 超低速（精密作業）モードのCPI分周値（押している間、CPIをこの値で割る。既定4、範囲2-5）
// 上限は5: 実CPIは100刻みが下限のため、デフォルトCPI(500)ではこれ以上大きくしても
// 100CPIに張り付くだけで差が出ない（本人判断で5を上限に固定）。
#define KB_PRECISION_DIV_DEFAULT 4
#define KB_PRECISION_DIV_MIN     2
#define KB_PRECISION_DIV_MAX     5
uint8_t kb_precision_div_get(void);
void    kb_precision_div_set(uint8_t v);

// 超低速モードの連動レイヤー（0-7=そのレイヤーで自動的に超低速モード / KB_LAYER_NONE=なし。既定なし）
// PRC_MOキーとは独立に働き、どちらか一方でも条件を満たせば超低速モードになる。
uint8_t kb_precision_layer_get(void);
void    kb_precision_layer_set(uint8_t v);

// ── レイヤー連動LED（レイヤーごとに異なる光り方を設定できる機能）──────────
// 有効フラグ1バイト + レイヤー1-7それぞれ6バイトのテーブル（レイヤー0は
// 通常のLED設定＝GET/SET_LEDの値をそのまま使うので対象外）。
#define KB_LAYER_LED_ENABLE_EEPROM 0x09E6  // 機能そのものの有効/無効
#define KB_LAYER_LED_TABLE_EEPROM  0x09E7  // レイヤー別LED設定テーブル先頭（0x09E7-0x0A10、42バイト）
#define KB_LAYER_LED_MAX_LAYER     7       // 対象レイヤーの最大値（1-7）
#define KB_LAYER_LED_ENTRY_SIZE    6        // 1レイヤーあたりのバイト数

typedef struct {
    uint8_t enabled;    // このレイヤーで専用の光り方を使うか（0/1）
    uint8_t effect_id;  // GET/SET_LEDと同じエフェクトID体系
    uint8_t hue;
    uint8_t sat;
    uint8_t val;
    uint8_t speed;
} __attribute__((packed)) kb_layer_led_t;

// レイヤー連動LED機能そのものの有効/無効（既定: 無効）
bool kb_layer_led_enable_get(void);
void kb_layer_led_enable_set(bool v);

// レイヤーN（1-KB_LAYER_LED_MAX_LAYER）のLED設定を取得・変更する（既定: enabled=0）
kb_layer_led_t kb_layer_led_get(uint8_t layer);
void           kb_layer_led_set(uint8_t layer, const kb_layer_led_t *cfg);

// 通常（レイヤー0）のLED設定。RGBLIGHT本体のEEPROM機能には頼らず、ここで完全に
// 独立管理する。理由: レイヤー連動LEDが有効な間、RGBLIGHT本体の「現在の表示」は
// レイヤーのオーバーライドに上書きされているため、rgblight_get_*()を読んでも
// 本来の「通常」の設定は取得できない（オーバーライド中にSET_LEDされた場合、
// 元に戻った時にオーバーライド前の古い値へ戻ってしまう事故が起きていた）。
// SET_LEDのたびに必ずここへ保存し、実際に画面へ反映するかどうかは
// 呼び出し側（kb_hid.c）がkeyball_layer_led_overriding()を見て判断する。
typedef struct {
    uint8_t effect_id;
    uint8_t hue;
    uint8_t sat;
    uint8_t val;
    uint8_t speed;
} __attribute__((packed)) kb_led_config_t;

#define KB_LED_CONFIG_EEPROM 0x0A11  // 5バイト（0x0A11-0x0A15）
kb_led_config_t kb_led_config_get(void);
void            kb_led_config_set(const kb_led_config_t *cfg);

// ── 慣性スクロール（ボールを弾いた後もしばらくスクロールが減衰しながら続く）──
// 強さの上限を254に制限しているのは、EEPROM未初期化時の値0xFF(255)と衝突させない
// ため（255まで許すと、ユーザーが明示的に255を選んでも再起動後に未初期化と誤認され
// デフォルト値に戻ってしまう）。
#define KB_SCROLL_INERTIA_ENABLE_EEPROM    0x0A16  // 有効/無効（1バイト）
#define KB_SCROLL_INERTIA_STRENGTH_EEPROM  0x0A17  // 強さ（1バイト）
#define KB_SCROLL_INERTIA_STRENGTH_MIN     0
// 【注意】以前は254だったが、実機で最大値付近が強すぎるとの指摘があり15に縮小した
// （2026-09-17）。減衰の時定数計算(keyball.cのkeyball_on_apply_motion_to_mouse_scroll)
// はこの値に対して比例計算しているため、ここを変えるだけで自動的に追従する。
#define KB_SCROLL_INERTIA_STRENGTH_MAX     15
#define KB_SCROLL_INERTIA_STRENGTH_DEFAULT 8

// 発動しきい値の倍率。実際の倍率は「値÷10」（例: 25なら2.5倍）。0.1刻みまで
// 表現できるよう10倍した整数で保持する。下限を0にしていないのは、0倍だと
// しきい値が常に0になり「ゆっくり動かしても発動する」という本来の目的に
// 反してしまうため。
#define KB_SCROLL_INERTIA_FLICK_MULT_EEPROM   0x0A18  // 発動しきい値の倍率×10（1バイト）
#define KB_SCROLL_INERTIA_FLICK_MULT_MIN      1    // 0.1倍
#define KB_SCROLL_INERTIA_FLICK_MULT_MAX      30   // 3.0倍
#define KB_SCROLL_INERTIA_FLICK_MULT_DEFAULT  25   // 2.5倍

// 慣性スクロール機能そのものの有効/無効（既定: 無効）
bool kb_scroll_inertia_enable_get(void);
void kb_scroll_inertia_enable_set(bool v);

// 慣性の強さ（0-254、大きいほど長く・遠くまで滑る。既定128）
uint8_t kb_scroll_inertia_strength_get(void);
void    kb_scroll_inertia_strength_set(uint8_t v);

// 慣性を発動させる最低速度の倍率×10（1-30 = 0.1〜3.0倍、既定30 = 3.0倍）。
// ゆっくり動かした時は発動させず、速く弾いた時だけ発動させるためのしきい値。
// 大きいほど「よほど速く弾かないと発動しない」、小さいほど「そこそこの
// 速さでも発動する」ようになる。
uint8_t kb_scroll_inertia_flick_mult_get(void);
void    kb_scroll_inertia_flick_mult_set(uint8_t v);

// ── シェイク機能（2026-09-10〜。トラックボールを振ると設定したキーを発動）──────
// ジェスチャーモードの選択状態に関わらず常時判定する（「振る」動作は方向ジェスチャー
// と混同しにくいため）。キー未設定(0、EEPROM未書込み時と同じ値)なら常に何もしない。
// KB_GESTURE_MODE_TABLE_EEPROM(0x0A19-0x0A40)・KB_TRACKBALL_LAYERS_MAGIC_EEPROM(0x0A41)・
// KB_GESTURE_WAVE_SPEED_EEPROM(0x0A42)・KB_GESTURE_WAVE_ENABLE_EEPROM(0x0A43)の直後。
#define KB_SHAKE_KEY_EEPROM       0x0A44  // 発動キー（2バイト、0=未設定）
#define KB_SHAKE_THRESHOLD_EEPROM 0x0A46  // 感度（1バイト。小さいほど敏感）
#define KB_SHAKE_THRESHOLD_MIN     10
#define KB_SHAKE_THRESHOLD_MAX     200
#define KB_SHAKE_THRESHOLD_DEFAULT 60

uint16_t kb_shake_key_get(void);
void     kb_shake_key_set(uint16_t v);
uint8_t  kb_shake_threshold_get(void);
void     kb_shake_threshold_set(uint8_t v);

// シェイク判定の厳しさ（2026-09-10〜。本人希望により今までkeymap.cにハード
// コードしていた値をWeb UIから調整できるようにした。現状の既定値
// （反転6回・700ms以内）を中心に、緩める方向・厳しくする方向の両方に余白を
// 持たせている。KB_COMBO_EEPROM_BASE(0x0A54-0x0AA3、kb_combo.h参照)の直後。
#define KB_SHAKE_REVERSALS_EEPROM  0x0AA4  // 発動に必要な反転回数(1バイト)
#define KB_SHAKE_REVERSALS_MIN      2   // 1往復
#define KB_SHAKE_REVERSALS_MAX      12  // 6往復
#define KB_SHAKE_REVERSALS_DEFAULT  6   // 3往復（今までの固定値）

#define KB_SHAKE_RUN_MAX_EEPROM    0x0AA5  // 反転が全て収まるべき時間の上限(1バイト、10ms単位)
#define KB_SHAKE_RUN_MAX_MIN        10   // 100ms
#define KB_SHAKE_RUN_MAX_MAX        200  // 2000ms
#define KB_SHAKE_RUN_MAX_DEFAULT    70   // 700ms（今までの固定値）
// 次に設定を追加する場合は0x0AA6以降を使うこと。

uint8_t  kb_shake_reversals_get(void);
void     kb_shake_reversals_set(uint8_t v);
// 実際のms値を返す/受け取る（内部は10ms単位で保存）
uint16_t kb_shake_run_max_ms_get(void);
void     kb_shake_run_max_ms_set(uint16_t ms);

// ── ダブルフリック（2026-09-10〜。同じ方向へ短時間で2回フリックすると発火）──
// 2026-09-10、本人希望によりジェスチャーモード（GST_HOLD〜4キーやジェスチャー
// レイヤー）とは完全に独立させ、通常のトラックボール操作（カーソル移動）中に
// 動作するようにした。「動き始めてから止まるまで(streak)」を1つの塊として捉え、
// streakが止まった瞬間に判定する（慣性スクロールのフリック検出=keyball.cの
// keyball_scroll_inertia_should_applyと同じ考え方）。カーソルの動き自体は
// 変更せず観測するだけなので、通常のマウス操作を妨げない。
// KB_SHAKE_THRESHOLD_EEPROM(0x0A46)の直後。
//
// 2026-09-10、方向検知はstreak中の「ピーク速度」ではなく「streak全体の移動量
// 合計」で判定する方式に変更した（ノイズに弱いピーク値方式より安定するため、
// ジェスチャー機能の方向判定と同じ考え方に揃えた）。しきい値の意味も「ピーク
// 速度」から「移動量合計」に変わったため、範囲・既定値をジェスチャーの発動
// しきい値(KB_GESTURE_TH_*)と同じ基準に合わせている。
#define KB_DFLICK_KEY_TABLE_EEPROM     0x0A47  // 方向ごとの発動キー(4×2バイト=8バイト。0=未設定)
// 2026-09-11、本人が実機で詰めた値（感度30・時間窓500ms・動作時間上限420ms）を
// 新しい既定値にし、それらを中心に調整範囲を組み直した。
#define KB_DFLICK_WINDOW_EEPROM        0x0A4F  // 2回目のフリックを認識する時間の上限(1バイト、10ms単位)
#define KB_DFLICK_WINDOW_MIN            20  // 200ms
#define KB_DFLICK_WINDOW_MAX            80  // 800ms
#define KB_DFLICK_WINDOW_DEFAULT        50  // 500ms
#define KB_DFLICK_FLICK_THRESHOLD_EEPROM 0x0A50  // フリック判定のしきい値(streak全体の移動量合計。1バイト)
#define KB_DFLICK_FLICK_THRESHOLD_MIN     5
#define KB_DFLICK_FLICK_THRESHOLD_MAX     60
#define KB_DFLICK_FLICK_THRESHOLD_DEFAULT 30

// dir: 0上 1下 2左 3右（kb_gesture_mode_tのkey[]と同じ並び）
uint16_t kb_dflick_key_get(uint8_t dir);
void     kb_dflick_key_set(uint8_t dir, uint16_t v);
// 実際のms値を返す/受け取る（内部は10ms単位で保存）
uint16_t kb_dflick_window_ms_get(void);
void     kb_dflick_window_ms_set(uint16_t ms);
uint8_t  kb_dflick_flick_threshold_get(void);
void     kb_dflick_flick_threshold_set(uint8_t v);

// ── シェイク・ダブルフリックそれぞれの有効/無効（2026-09-10〜。既定: 両方有効）──
// 2026-09-10、本人が「シェイクとダブルフリックのどちらが原因で動作しないのか
// 切り分けたい」との要望で追加。キー未設定(0)でも実質無効になるが、キー設定を
// 消さずに機能ごと止められるようにする。他の既定ONフラグ（kb_gesture_wave_enable
// 等）と同じ「1のみ明示的なOFF、それ以外(未書込みの0x00含む)は既定のON」パターン。
// KB_DFLICK_FLICK_THRESHOLD_EEPROM(0x0A50)の直後。
#define KB_SHAKE_ENABLE_EEPROM  0x0A51
#define KB_DFLICK_ENABLE_EEPROM 0x0A52

// ダブルフリックの「フリックとみなす最大継続時間」（2026-09-10〜）。今まで
// FLICK_MAX_DURATION_MSとしてkeymap.cにハードコードしていたが（120ms固定）、
// 「感度・時間窓を一番緩くしても発火しない」という報告を受けて発覚: 実際の
// トラックボールは指で弾いた後も慣性で転がり続けるため、実測の継続時間が
// 120msを超えるケースがあり、他の設定をどれだけ緩めても無条件に弾かれていた
// （Web UIから見えない・調整できない値だったため気づきにくかった）。これを
// Web UIから調整できるようにした。
// 2026-09-11、本人が実機で詰めた値(420ms)を既定値にし、中心に範囲を組み直した。
#define KB_DFLICK_MAX_DURATION_EEPROM  0x0A53  // 最大継続時間(1バイト、10ms単位)
#define KB_DFLICK_MAX_DURATION_MIN      15  // 150ms
#define KB_DFLICK_MAX_DURATION_MAX      70  // 700ms
#define KB_DFLICK_MAX_DURATION_DEFAULT  42  // 420ms
// 0x0A54-0x0AA3(80バイト)はコンボ設定(kb_combo.h/c、KB_COMBO_EEPROM_BASE)、
// 0x0AA4-0x0AA5はシェイク判定の厳しさ設定(本ファイル末尾参照)が使用済み。
// 次にここへ設定を追加する場合は0x0AA6以降を使うこと。

uint16_t kb_dflick_max_duration_ms_get(void);
void     kb_dflick_max_duration_ms_set(uint16_t ms);

bool kb_shake_enable_get(void);
void kb_shake_enable_set(bool v);
bool kb_dflick_enable_get(void);
void kb_dflick_enable_set(bool v);

// ── DPIカーブ（2026-09-11、本人希望で追加。同日、本人希望で5点→9点に増量）──
// Photoshopのトーンカーブのように、トラックボールの「動きの速さ」に対する
// 「実際に送る速さ」を折れ線グラフで自由に調整できる機能。
// X軸（入力の速さ）は0-127に固定（点の位置はKB_DPI_CURVE_X参照）、
// Y軸（出力の速さ）だけをEEPROMに保存する。既定値はY=X（対角線＝何も変えない）。
// 有効時は既存の「加速度」設定(keyball_get_accel)より優先される
// （keymap.cのkeyball_on_apply_motion_to_mouse_move参照）。
#define KB_DPI_CURVE_POINT_COUNT 9
extern const uint8_t KB_DPI_CURVE_X[KB_DPI_CURVE_POINT_COUNT];  // 各点のX座標（固定・変更不可）

// 0x0A54-0x0AA3(80バイト)はコンボ設定、0x0AA4-0x0AA5はシェイク判定の厳しさ設定
// (本ファイル前方参照)が使用済み。次にここへ設定を追加する場合は0x0AB1から。
#define KB_DPI_CURVE_ENABLE_EEPROM 0x0AA6  // 有効/無効（1バイト。1=ON明示、それ以外(未書込み含む)=OFF既定）
#define KB_DPI_CURVE_MAGIC_EEPROM  0x0AA7  // 出力の点が実際に保存済みかの目印(1バイト)
// 2026-09-11、点数を5→9へ増やした際にマジックバイトの値も変更した（旧5点データを
// 誤って9点として読み込み、末尾4点が未書込み領域の0x00＝出力0になって曲線が
// 急落するのを防ぐため。値が変わっていれば「未保存」扱いになり既定のY=Xへ戻る）。
#define KB_DPI_CURVE_MAGIC_VALUE   0xC6
#define KB_DPI_CURVE_POINTS_EEPROM 0x0AA8  // 出力値9点（0x0AA8-0x0AB0、各1バイト、0-255）
// 次にここへ設定を追加する場合は0x0AB1から（一度「加速度スクロール」用に割り当てた
// が、機能自体を削除したため未使用に戻っている。実機に書き込まれたことは無い）。

bool kb_dpi_curve_enable_get(void);
void kb_dpi_curve_enable_set(bool v);

// 出力の点を返す（要素数KB_DPI_CURVE_POINT_COUNTの内部バッファへのポインタ。
// 呼び出し側で書き換えないこと）。未保存なら既定のY=X（KB_DPI_CURVE_Xと同じ値）を返す。
const uint8_t *kb_dpi_curve_points_get(void);
// 出力の点を保存する（points は要素数KB_DPI_CURVE_POINT_COUNTの配列）。
void kb_dpi_curve_points_set(const uint8_t *points);

// 5点を単調3次エルミート曲線（Fritsch-Carlson系、オーバーシュートしない滑らかな
// 曲線）で結んだ、入力の速さ(0-127)ごとの出力値を並べたルックアップテーブルを返す
// （要素数KB_DPI_CURVE_LUT_SIZE）。設定変更時にだけ計算し直すのでキャッシュされ、
// 毎回のポインター移動処理では単純な配列参照だけで済む（keymap.c参照）。
#define KB_DPI_CURVE_LUT_SIZE 128
const uint8_t *kb_dpi_curve_lut_get(void);
