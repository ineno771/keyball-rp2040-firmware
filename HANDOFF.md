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
- **RP2040の大容量フラッシュを活かした将来機能案**（本人からのアイデア、まだ未着手）: RGB_MATRIXへの移行、マクロ・設定の大容量化、トラックボールジェスチャー機能の強化、LEDパターンの増設。

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
- [ ] **コンスルー再購入待ち**: 既存のコンスルーは高さ不足でKeyball39基板にうまく挿さらないことが判明（2026-08-25）。新しいコンスルーの到着待ちで実機テストがブロック中。
- [ ] **実機でのBOOTSEL書き込みテスト**: `.uf2`をSparkFun Pro RP2040にBOOTSELモードで書き込み、実際に動作するか確認。キーマトリクス・トラックボール・LED・OLED・分割間通信すべての動作確認が必要。
- [ ] **Keyball Linkでの認識確認**: VIDのみでの検出＋GET_INFOでの機種判定のため動くと見込んでいるが、実機での動作未検証。
- [ ] **LED電圧問題の実機確認**: 3.3Vロジックで5V駆動のLEDチェーンが正常に光るか（チラつき等が出ないか）。
- [ ] **LAYOUTマクロ警告の解消**（優先度低）: 上記の通り実害なしのため保留中。
- [ ] **Keyball44/61のRP2040対応**: 今回はKeyball39のみ。他機種は今後別途。
- [ ] **RP2040の大容量フラッシュを活かした新機能**: RGB_MATRIX移行・マクロ大容量化・ジェスチャー強化・LEDパターン増設（本人からのアイデア。詳細はこれから）。
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

## 7. ハードウェア識別情報
- USB VID `0x5957`（Yowkees共通）・PID `0x0600`（keyball-rp2040-firmware Keyball39専用、新規発行）
- 内部モデル識別番号（`KEYBALL_MODEL`）: `39`（物理形状がAVR版Keyball39と同一のため）
