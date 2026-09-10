# Keyball RP2040 ファームウェア — 引き継ぎメモ

> このファイルは「チャットをリセットしても／別デバイスに移っても作業を続けられるように」まとめた引き継ぎ書です。
> 次のセッションは、まずこのファイルと [README.md](README.md) を読んでから作業してください。

## 0. プロジェクト概要
- **何のプロジェクトか**: SparkFun Pro Micro RP2040を使った、Keyballシリーズ向けの大容量フラッシュ版ファームウェア。まずKeyball39から着手。既存のATmega32u4版（keyball-link-firmware）はフラッシュ32KBの制約で「LED版」「通常版（マクロ・ジェスチャー）」を分けてビルドしていたが、RP2040の16MBフラッシュで全機能を1つに統合するのが目的。
- **場所**: `~/keyball-rp2040-firmware`
- **GitHub**: https://github.com/ineno771/keyball-rp2040-firmware （個人アカウント配下・Public。keyball-plus-firmwareと同様）
- **ベースにした既存プロジェクト**: `~/keyball-link-firmware`（GitHub: Yowkees/keyball-link-firmware）のKeyball39定義。これがKeyball Link（Web版設定ツール）から現在実際に書き込まれているファームウェア。
- **ハードウェア方針**: 基板は無改修。既存Keyball39の12ピンPro Microソケット（コンスルー接続）に、SparkFun Pro RP2040をそのまま挿す。分割両側ともRP2040化。
- **現在のファームバージョン**: 0.1.0（`keyboards/keyball/lib/keyball/kb_version.h`。AVR版とは別系統の番号）

---

## 1. ユーザーについて（重要・厳守）
- プログラミング**初心者**。専門用語は必ず簡単な説明を添える。**日本語**で回答。
- **コミット/プッシュは毎回メッセージ案を提案**してから。実際の実行は「お願いします」等の明確な指示があったときだけ。
- 手動作業が必要なときは**ステップごとに**説明。
- ターミナル作業（qmk compile / git 等）は**ClaudeがBashツールで実行**する。
- ユーザーはKeyball販売元「Shirogane Lab」の人間。将来的な製品化・販売を見据えている。
- **実機を保有**: Keyball39実機・SparkFun Pro RP2040ともに手元にある（2026-08-25時点）。実機テストが可能な段階。

---

## 2. 決定事項（すり合わせ済み）
- **ハードウェア**: 新規PCB設計はしない。既存Keyball39基板＋12Pコンスルー接続のまま。分割両側ともSparkFun Pro RP2040。
- **LED電圧問題**: SparkFun Pro RP2040は3.3Vロジックだが、既存LED（SK6812MINI-E）はVDD 5V駆動（回路図で確認済み。`power_VCC`ネットがPro MicroのRAWピン=5Vに直結）。データ信号線(DIN)がWS2812系の規格上わずかに電圧不足になる可能性があるが、実例は多く動く可能性が高いため**実機テストで様子見**という結論。レベルシフタ内蔵のHelios等も検討したが、標準12ピンの範囲外にしかレベルシフト済みピンが無く、今回のPCB無改修方針とは相容れないため不採用。
- **機能統合方針**: LED版/通常版の分岐を廃止し、RGBLIGHT・マクロ・ジェスチャー・OLED・Dynamic Keymapを全て同時有効化。
- **USB PID**: 新規発行（`0x0600`）。AVR版keyball39（`0x0200`）との混同を避けるため。
- **今回のスコープ**: ファームウェア開発のみ。Keyball Linkからの書き込み対応（現状はAVR109プロトコルのみでRP2040のBOOTSEL+UF2方式には非対応）は次の別プロジェクトとして後日着手する。
- **新機能の実装対象はRP2040版のみ（2026-08-25決定）**: AVR版（keyball-link-firmware、Pro Micro/ATmega32u4）への新機能移植も検討したが、フラッシュ空き容量を実測したところ機種・バージョンによってかなり厳しいことが判明したため、**当面はRP2040版のみに実装対象を絞る**方針にした。
  - 実測結果（web_configuratorキーマップ、2026-08-25時点）: Keyball39 通常版2,350B/LED版932B、Keyball44 通常版1,134B/LED版476B、**Keyball61 通常版816B/LED版190B**（ボトルネック）。
  - 試作計測: 超低速モード（CPI一時変更）は+74B、レイヤー切替LED演出（RGBLIGHT flash）は+8Bで、単体なら軽量な機能はAVR版でも載る可能性はあるが、Keyball61 LED版の190Bという制約を考えると複数機能を同時に載せるのは困難。将来、AVR版への逆移植を再検討する際はこの数値を参照。
- **RP2040の大容量フラッシュを活かした将来機能案**（本人からのアイデア、まだ未着手）: RGB_MATRIXへの移行、マクロ・設定の大容量化。トラックボールジェスチャー機能の強化・LEDパターンの増設は、以下の個別項目として詳細化済み（2026-08-25）。
- **トラックボール「矢印キーモード」構想（アイデア段階、未着手。2026-08-25仕様変更）**: トラックボールが回転している間、その方向の矢印キーが入力されるモード。
  - **仕様変更**: 当初の「回転中は押しっぱなし」案を廃止し、**回転速度に応じて連続入力される方式に置き換え**（本人決定）。速く回すと矢印キーが高頻度で連続送信され、ゆっくり回すと1回転で矢印キー1回だけ送信される（コマ送り的な操作感）。
  - 決定済み仕様（変更なし）: (1) 誤入力防止のデッドゾーン（閾値）を設ける。(2) 斜め入力は無し。上下・左右いずれか強く動いている軸だけを採用する。
  - 既存のジェスチャー機能（`GESTURE_ENABLE`、トラックボール移動量を累積して方向判定するロジック）と技術的に近く、流用しやすい見込み。
- **トラックボール「超低速（精密作業）モード」（実装済み・2026-08-25）**: 新規キーコード`PRC_MO`（`keyball.h`）を押している間、CPIを分周値（既定4、範囲2-20、`kb_precision_div_get/set`でEEPROM保存）で割って移動量を下げる。離すと元のCPIに復帰。
  - 分周値はKeyball Linkから調整できるよう、新規HIDコマンド`KB_HID_CMD_GET_PRECISION`(0x18)/`KB_HID_CMD_SET_PRECISION`(0x19)を追加済み（`kb_hid.h`/`kb_hid.c`）。**Keyball Link側のUI実装も完了済み**（`~/keyball-configurator`のSettingsTab、コミット`935d441`ほか）。
  - `PRC_MO`はデフォルトのキーマップ配列には未割り当て。Keyball Linkのキー割り当てUIから任意のキーに割り当てる運用。
  - ビルド確認済み（フラッシュ+432バイト、RP2040なので問題なし）。実機動作確認はこれから。
- **移動方向矯正（軸スナップ）モード構想（アイデア段階、未着手）**: 対応キー（例: Shift）を押しながらトラックボールを回転させると、上下・左右・斜め45度の直線的な移動のみに制限されるモード。デザインツールのShift+ドラッグと同様の挙動を想定。
  - 決定済み仕様: **毎フレーム方向を再判定するのではなく、押し始めた瞬間の移動方向で1回だけ方向を確定・ロックする**方式を採用（本人了承済み）。斜め境界付近での手ブレによるジグザグを防ぐため。
  - 既存のジェスチャー機能の「保留→確定」状態管理ロジックを流用できる見込み。
- **方向別感度調整構想（アイデア段階、未着手）**: トラックボール操作は指の動かしやすい方向・動かしにくい方向があるため、ボールの回転方向によって移動量（感度）を個別に調整できる機能。
  - 決定済み仕様: **8方向（上下左右＋斜め4方向）ごとに個別の感度値を持てるようにする**（本人希望。縦横2軸のみのシンプル案ではなく、より細かい8方向案を採用）。
  - 既存のCPI（感度）ランタイム変更の仕組みを流用できる見込み。Keyball Linkからの調整UIも必要になりそう。
- **パイメニュー構想（アイデア段階、未着手）**: キーを押しながら任意方向にボールを転がすことで、円形に配置した複数のショートカット・マクロから1つを選ぶ機能（Photoshop/Blenderのパイメニューに近い発想）。矢印キーモード・軸スナップと同じ「方向検出」の仕組みをそのまま流用できるためfirmware側で完結する（companion appは不要）。
- **（不採用）クリック時刻の位置スムージング**: クリック直前のセンサー履歴を遡って座標を補正する案を検討したが、標準的なUI要素に対しては`MagSnap`（旧keyball-companion。別プロジェクト、https://github.com/ineno771/mag-snap ）側の「磁石的吸着＋吸着中フリーズ」機能で同等の効果が得られるため不採用。自由座標（キャンバス等）への精密クリック用途では価値が残る可能性はあるが、現時点では見送り。
- **LED「波紋」演出（実装済み・2026-09-04頃）**: キーを押した位置から波紋のように光が広がるRIPPLEエフェクトとして実装済み。LEDの光り方設定の1つとして選択可能（`rgb_matrix_user.inc`のRIPPLE）。
- **レイヤー切替通知LED演出（実装済み・2026-08-25）**: `layer_state_set_user`でレイヤー変更を検知し、現在のRGBLIGHTの発光モード・色を保存した上で、レイヤー番号に応じた色で一時的にフラッシュ（`RGBLIGHT_MODE_STATIC_LIGHT`に切替）。`matrix_scan_user`で200ms経過を検知したら元の発光モード・色に自動復帰する。常時表示ではなく切り替えの瞬間だけ光る通知的な演出（本人希望通り）。
  - ビルド確認済み（フラッシュ+432バイトは超低速モードとの合算値）。実機動作確認はこれから。
- **OLED表示のリッチ化構想（アイデア段階、未着手）**: アニメーション・キャラクター表示（待機中に動くキャラクター等）と、情報量の増加（現在のレイヤー・CPI・WPM等の実用情報）の両方を希望。具体的な演出内容・表示項目は未検討。
- **ジェスチャーによるレイヤー切替（トグル式）構想（アイデア段階、未着手）**: キーを押しながら上下左右いずれかの方向にボールを転がすことで、対応するレイヤーへトグル切替する。**8方向ではなく上下左右の4方向を採用**（8方向だと誤操作の可能性が高くなるため、本人判断）。矢印キーモード・軸スナップと同じ「方向検出」の仕組みを流用できる見込み。
- **汎用連続値調整機能（実装済み・2026-09-09）**: 「対応キーを押しながらボールを回転させると回転量に応じてキーが連続送信される」という当初構想は、複数ジェスチャーモードの「方向ごとの連続入力ON/OFF」として実現済み（割当先キー・連続入力の有無ともKeyball Linkから設定変更可能）。詳細は本章末尾の「複数ジェスチャーモード」項目参照。
- **スクロール慣性（モメンタムスクロール、実装済み）**: トラックボールでのスクロール操作に、指を離した後も減速しながら続く慣性を付ける機能。`kb_settings.h`の`kb_scroll_inertia_enable/strength/flick_mult_get/set`、`keyball.c`の`keyball_scroll_inertia_*`で実装済み。強さ・発動しきい値の倍率ともKeyball Linkから調整可能（`GET/SET_SCROLL_INERTIA`、0x1E/0x1F）。
- **レイヤー数の増加（実装済み・2026-09-02）**: `DYNAMIC_KEYMAP_LAYER_COUNT`を4→8に変更（`keyball39/keymaps/web_configurator/config.h`）。ビルド確認済み、実機での動作確認はこれから。Keyball Link側はGET_INFOで実際のレイヤー数を都度取得する設計のため、Web側のコード変更は不要（自動対応）。上記「ジェスチャーによるレイヤー切替」は4方向のみ対応のため、8レイヤー全てにジェスチャーで到達できるわけではない点に注意（残りは既存のレイヤーキー等でアクセスする想定）。
- **複数ジェスチャーモード（実装済み・実機確認済み・2026-09-09）**: 従来1系統だったジェスチャーを4つの独立したモードに拡張。各モードは上下左右に別々のキーを割り当てられ、方向ごとに「連続入力」（回転速度に応じてキーを連続タップ。音量調整・フォントサイズ変更向け）をON/OFFできる。モード選択はレイヤー連動（モードごとに対象レイヤー1つ、`kb_gesture_mode_t.layer`）＋手動キー（`GST_HOLD`=モード1、`GST_HOLD2`〜`4`=モード2〜4、押している間だけ優先しレイヤー連動より一時的に上位）。旧・単一モードの「タップで別キーを送る」機能(`gesture_tap`)は廃止（ホールド専用に統一）。AVR版（keyball-link/plus-firmware）は無変更・影響なし。詳細設計は`kb_settings.h`の`kb_gesture_mode_t`、`kb_hid.h`の`KB_HID_CMD_GET/SET_GESTURE_MODE`(0x20/0x21)・`GET/SET_GESTURE_THRESHOLD`(0x22/0x23)、`keymap.c`の`gst_active_mode()`まわりを参照。Keyball Link側UIも実装・実機確認済み（`rp2040-dev`ブランチ）。
- **ジェスチャー連動LEDウェーブ（実装済み・実機確認済み・2026-09-09〜10）**: 複数ジェスチャーモードで実際にキーが送出された瞬間（単発は1回、連続入力は連続して）、その方向に応じてLEDが列/行単位で帯状に流れる`GESTURE_WAVE`エフェクト。選択式のLEDエフェクトではなく、ジェスチャー発火の瞬間だけ現在のLED表示（通常LED・レイヤー連動LEDのどちらでも）を強制的に一時上書きし、終わったら自動復帰する常時有効なオーバーレイとして実装（`keyball_gesture_wave_task`）。専用の速さ設定(`kb_gesture_wave_speed_get/set`、`GET/SET_GESTURE_WAVE_SPEED`0x24/0x25)とON/OFF設定(`kb_gesture_wave_enable_get/set`、`GET/SET_GESTURE_WAVE_ENABLE`0x26/0x27)を追加。連続入力時は前のウェーブが終わるまで次を発動させない。分割両ハーフ間の速度同期は`KEYBALL_GESTURE_WAVE`RPCで実施（各ハーフが自分のEEPROMを持つため、直接読むとズレる点に注意）。左右方向の起点は実機確認の結果、素直な物理x座標の大小と逆だったため反転させている（`rgb_matrix_user.inc`のGESTURE_WAVE参照）。AVR版は無変更。Keyball Link側UIも実装・実機確認済み（`rp2040-dev`ブランチ）。

---

## 3. 直近の作業（2026-08-25）
- リポジトリ新規作成。`~/keyball-link-firmware/keyboards/keyball/keyball39`（board定義・lib・drivers）をベースにコピー。
- QMKの公式コンバーター `CONVERT_TO=sparkfun_pm2040` を使ってRP2040化。ピン配置はほぼそのまま流用できることを確認済み（Keyball39の既存ピン使用がこのコンバーターの対応ピンと完全一致）。
- ビルドが通るところまで確認済み（`.uf2`生成成功、フラッシュ使用量は約48.5KB/16MB）。ポート作業で必要だった修正点はREADME.mdの「AVR版との違い」セクション参照。
- このデバイス（ビルド環境）のarm-none-eabi-gccが依存関係破損（`libisl`, `libmpc`が不足）していたため修復済み。`osx-cross/arm` tapを信頼設定に追加した。
- QMKのlint警告のうち、`info.json`と`keyboard.json`の重複は解消済み（`info.json`を削除）。LAYOUTマクロ警告（`.h`ファイル内定義への警告）は、実際に使用している`web_configurator`キーマップがこのマクロを使わず生配列でキーマップ定義しているため実害なしと判断し、**意図的に未対応のまま保留**（39キー×4パターン分の座標をJSON化する必要があり、誤記リスクの割に得るものが少ないため）。
- 実機テストを試みたところ、**既存のコンスルーでは高さが足りずKeyball39基板にうまく挿さらない**ことが判明。新しいコンスルーを購入してから作業再開の予定（2026-08-25時点、部品待ち）。
- Keyball Link（Web版）の対応状況を確認: デバイス検出はVID（`0x5957`）のみでフィルタし、機種名はGET_INFOコマンドで実行時に`KEYBALL_MODEL`を読み取る方式のため、**Web側のコード変更なしで今回のRP2040版もKeyball39として正しく認識される**見込み（未検証）。
- BOOTSELボタンへのアクセス問題（コンスルー接続だと基板が裏返しになりボタンを押しにくい）への対処法をREADMEに追記: 初回書き込みは基板単体で行い、以降はQMKのダブルタップリセット機能（`RP2040_BOOTLOADER_DOUBLE_TAP_RESET`、既定で有効）を使ってKeyball39本体のリセットボタン（SW19）から再書き込みできるようにした。
- GitHubにリポジトリ作成・push完了: https://github.com/ineno771/keyball-rp2040-firmware （個人アカウント配下・Public）

---

## 4. 未確定・残タスク
- [x] **コンスルー再購入待ち**: 新しいコンスルーが到着し(2026-09-02)、Keyball39基板に正しく挿せることを確認。
- [x] **実機でのBOOTSEL書き込みテスト**（2026-09-02完了）: `.uf2`書き込み・両側TRRS接続で、キーマトリクス・トラックボール・OLED・LED・分割間通信すべて動作確認済み。
  - 途中、**トラックボール(SPI)を有効にすると本体が無反応になる重大なバグ**を発見・修正した。原因は`pmw3360_init()`内で`pmw3360_spi_start()`を不要に二重呼び出ししていたこと。`pmw3360_reg_write()`/`pmw3360_reg_read()`は呼び出しごとに自前でSPIバスの開始・終了を完結させるため、外側でさらに`pmw3360_spi_start()`を呼ぶと二重ロックになる。AVR版の`spi_start()`はレジスタ再設定のみで実害がなかったが、ChibiOS(RP2040)版の`spi_start()`はOSの排他ロック(ミューテックス)を取得するため、自分自身が既に保持しているロックを再度取ろうとして無限待機し、**メインループ全体（マトリクス・OLED・LED含む）が停止する**という形で症状が出た。`pmw3360_init()`冒頭の余分な`pmw3360_spi_start()`と末尾の余分な`spi_stop()`を削除して解決（`keyboards/keyball/drivers/pmw3360/pmw3360.c`）。
  - 副次的に、SPIピン(`SPI_SCK_PIN`/`MOSI`/`MISO`)を`keyball39/config.h`に明示指定（B1/B2/B3、実配線に合わせる）。QMKのボード既定値と実は一致していたが、依存関係を明示するため残した。
- [x] **Keyball Linkでの認識確認**: 実機で「Keyball39」として正しく認識されることを確認済み（2026-09-02）。Web側のコード変更は不要だった。
- [x] **macOS JIS配列認識の確認**（2026-09-02完了）: 新規PID(`0x0600`)を初めて認識するMacでは、`com.apple.keyboardtype.plist`書き換え＋USB抜き差しだけではJIS配列が反映されないことがあった（`cfprefsd`再起動レベルでは足りない、より深いキャッシュが残る様子）。**Macを完全に再起動**したところ解消。ファームウェア・Web側とも実装は正しく、原因はmacOS側の「初めて見るPID」に対するキャッシュだった。新しいPIDを発行するたびに起こりうるので、他機種のRP2040化やPID変更時も同様の切り分けをすること。
- [ ] **LED電圧問題の実機確認**: 3.3Vロジックで5V駆動のLEDチェーンが正常に光るか（チラつき等が出ないか）。上記バグ修正後、通常の発光は確認できたが、長時間点灯や高輝度時のチラつき等はまだ未検証。
- [ ] **LAYOUTマクロ警告の解消**（優先度低・本人了承済み）: `default`/`develop`/`via`/`test`の4キーマップが`keyball39.h`の手書きLAYOUTマクロを使用中（本番の`web_configurator`は未使用）。`keyboard.json`側に`matrix`座標データが無いためQMKの警告が出るが、ビルドは正常に成功し実害なし。対応する場合は39キー×4パターン分の`matrix`座標をJSON化した上で`.h`側の手書き定義を削除する必要がある（転記ミスのリスクがあるため急ぎでは対応しない方針）。
- [ ] **Keyball44/61のRP2040対応**: 今回はKeyball39のみ。他機種は今後別途。
- [ ] **RP2040の大容量フラッシュを活かした新機能（残りのアイデア）**: RGB_MATRIX移行・レイヤー数8化・LED波紋演出・レイヤー切替LED演出・汎用連続値調整機能・スクロール慣性・ジェスチャー連動LEDウェーブは実装済み（詳細は2章）。未着手のまま残っているのは：矢印キーモード（速度連動連続入力）、軸スナップモード、方向別感度調整（8方向）、パイメニュー、OLEDリッチ化、ジェスチャーによるレイヤー切替（トグル式。ただし複数ジェスチャーモードの方向別キー割当でTG(layer)等を割り当てれば代替可能）。
- [ ] **Keyball LinkのWeb書き込み対応**: 今回のスコープ外。BOOTSEL+UF2方式への対応（File System Access API等）が必要で、まとまった別プロジェクトになる見込み。

---

## 5. 主要ファイル
| ファイル | 内容 |
|----------|------|
| `keyboards/keyball/keyball39/keyboard.json` | USB VID/PID・機種名・`pin_compatible`設定 |
| `keyboards/keyball/keyball39/rules.mk` | `CONVERT_TO = sparkfun_pm2040` でRP2040化 |
| `keyboards/keyball/keyball39/config.h` | マトリクス・分割通信・LED数などのピン設定（AVR版からほぼそのまま流用） |
| `keyboards/keyball/keyball39/keymaps/web_configurator/` | Web Configurator用キーマップ（本番で使うのはこれ。LED版/通常版の分岐なし・全機能統合済み） |
| `keyboards/keyball/lib/keyball/keyball.h` | `KEYBALL_MODEL`判定（PID上位バイトから算出。`0x0600`→39を追加済み） |
| `keyboards/keyball/drivers/pmw3360/pmw3360.c` | トラックボールセンサードライバ（RP2040向けSPI速度修正済み） |
| `patches/` | QMK本体への修正パッチ（JIS国コード・自動マウスしきい値。keyball-plus-firmwareと共通） |

---

## 6. ビルド環境（このデバイス）
- QMK Firmware本体: `~/qmk_firmware`（ベースコミット`594558ec7b9ac1963870447778426682065e0d20`、2つのパッチ適用済み）
- RP2040向けビルドに必要な `arm-none-eabi-gcc` は `brew`（`osx-cross/arm` tap）でインストール・修復済み。
- ビルド時は毎回、`keyboards/keyball` 一式を `~/keyball-rp2040-firmware` から `~/qmk_firmware/keyboards/` にコピーしてから `qmk compile -kb keyball/keyball39 -km web_configurator` を実行する運用（keyball-plus-firmwareと同じパターン）。
- **重要（2026-09-02判明）**: コピーに`rsync -a`を使うと元ファイルの更新日時がそのままコピー先に引き継がれるため、`.build`内の生成済みキャッシュ（`info_config.h`等）の方が新しいと`make`が「変更なし」と誤判定し、内容を直しても再ビルドに反映されないことがある（実際にUSB PIDが古い値のまま焼かれ続ける事故が発生）。**`qmk compile`の前に必ず該当ターゲットの`.build`キャッシュを削除してから実行する**こと。
  ```bash
  rsync -a --delete ~/keyball-rp2040-firmware/keyboards/keyball/ ~/qmk_firmware/keyboards/keyball/ \
    --exclude keyball46 --exclude keyball61 --exclude keyball44 --exclude one47 --exclude readme.md
  rm -rf ~/qmk_firmware/.build/obj_keyball_keyball39_web_configurator
  rm -f ~/qmk_firmware/.build/keyball_keyball39_web_configurator.uf2 ~/qmk_firmware/.build/keyball_keyball39_web_configurator.elf
  rm -f ~/qmk_firmware/keyball_keyball39_web_configurator.uf2
  cd ~/qmk_firmware && qmk compile -kb keyball/keyball39 -km web_configurator
  ```
- **重要（2026-09-09判明）**: RP2040のEEPROMエミュレーション（wear leveling方式）は、**一度も書き込んだことのない論理アドレスが`0xFF`ではなく`0x00`で初期化される**（`quantum/wear_leveling/wear_leveling.c`が原因）。AVR版や世間一般のEEPROM実装は`0xFF`が未初期化の目印という前提が広く通用するが、RP2040ではこれが成立しない。「生バイト0-7=そのままレイヤー番号」のような設定を新規に追加する際、未書込み時の`0x00`を「値0を明示的に選んだ」と区別できず誤動作する（実例: 複数ジェスチャーモード機能で、一度もWeb UIで保存していないモードがレイヤー0に誤連動しトラックボールが握りつぶされる事故になった。`kb_settings.c`の`trackball_layers_configured()`のように「実際に保存されたことがあるか」を示す目印バイトを別に持たせて対処）。新しいEEPROM設定を追加するときは必ずこれを考慮すること。
- **デバッグ手法**: `web_configurator`キーマップの`rules.mk`で`CONSOLE_ENABLE = yes`にし、`keyboard_post_init_user`で`debug_enable = true`をセットすると、要所の`dprintf`がホストPC側の`qmk console`コマンドで確認できる（RAW HID通信とは別チャンネルなので競合しない）。2026-09-09時点で常時有効のまま運用中。

## 7. ハードウェア識別情報
- USB VID `0x5957`（Yowkees共通）・PID `0x0600`（keyball-rp2040-firmware Keyball39専用、新規発行）
- 内部モデル識別番号（`KEYBALL_MODEL`）: `39`（物理形状がAVR版Keyball39と同一のため）
