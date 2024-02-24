# MGSPICO
2024/02/24 harumakkin

![mgspico-03](docs/pics/mgspico.png)</br>**fig.1 MGSPICO**

## これは何？
MSX本体が無くてもFM音源カートリッジと[MGSDRV](https://gigamix.hatenablog.com/entry/mgsdrv/)を使用してMGS楽曲データを再生し鑑賞できる個人製作のハードウェアです。MGSDRV は Raspberry Pi Pico内で動作しますが、RP2040用に移植したものではなく、MSX用のMGSDRVを [HopStepZ](https://github.com/cliffgraph/HopStepZ) というMGSDRV専用エミュレータを使用して動作させています。
また、ファームウェアver1.2から、[勤労五号(KINROU5.DRV)](https://sakuramail.net/fswold/music.html#muskin)というMuSICA上位互換ドライバも使用することでMuSICA楽曲データも再生できるようになりました。MuSICAデータを再生することができるようになっても名前はMGSPICOのままです。ご容赦を。

## 使い方
### 用意するもの
- MGSPICO
- [MGSDRV.COM(Ver3.20)](https://gigamix.hatenablog.com/entry/mgsdrv/)
- MGS楽曲データファイル（.MGSファイル）
- [KINROU5.DRV(Ver2.00)](https://sakuramail.net/fswold/music.html#muskin)
- MuSICA楽曲データファイル（.BGMファイル）
- microSD カード
- FM音源カートリッジ
- SCC音源カートリッジ
- DC5V電源(microUSB、もしくは、センタープラス 2.1mm DCプラグ）

#### 動作確認済みFM音源/SCC音源カートリッジ
|カテゴリ|カートリッジ|備考|
|:-|:-|:-|
|PSG/OPLL|[SoundCoreSLOT EX](http://niga2.sytes.net/sp/coreslot.pdf)||
|PSG/OPLL|[MSX SOUND ENHANCER](https://www.kadenken.com/view/item/000000001175)||
|SCC|MSX2版 スナッチャー 付属SCCカートリッジ|(MGS/MuSICA供にOK)|
|SCC|MSX版 SALAMANDER カートリッジ|MuSICA(KINROU5.DRV)では現状検証中|
|MIDI|[MIDI PAC v2](https://shop.supersoniqs.com/)|MGSPICOに直接接続では使用できず。[MSX Slot Expander](https://www.8bits4ever.net/product-page/msx-slot-expander)経由で使用できた|

## microSD カードの準備
- microSDカードを用意します。（32GBの容量の物で動作確認しています、それ以外では行っていません）
- microSDカードを、FAT、もしくは、FAT32フォーマットを施します。
- microSDカードに、MGSDRV.COM、player.com をコピーします
また、MuSICAデータも鑑賞したい場合は、KINROU5.DRV、playerK.comもコピーします
- 鑑賞したいMGSデータファイル(.MGS)、MuSICAデータファイル(.BGM)もカードへコピーします（1000ファイルまで）

## セットアップする
- microSDカードをMGSPICOのスロットに取り付ける
- FM音源／SCC音源を取り付ける
- AudioOutにスピーカーなどを取り付ける
- MGSPICO に電源を取り付ける。microUSB or DCプラグ（電圧と極性を間違わないこと！）

![ex1](docs/pics/ex1.png)
![ex2](docs/pics/ex2.png)</br>**fig.2 セットアップ例**

## 操作方法
**注意**：MGSとMuSICAは同時に使用できません。●スイッチを押しながら電源をONするとMuSICAモードで起動します。何もせず電源をONするとMGSモードで起動します
- STEP1. 電源を入れます
- STEP2. ディスプレイにMGSPICOのタイトルと、"for MGS" もしくは、"for MuSICA"が表示されます。
- STEP3. FM音源／SCC音源を認識できれば、音符マークと、SCCマークが表示されます
すぐにmicroSD内の最大3つの楽曲ファイルのファイル名が表示されます
- STEP4. ▲／▼ スイッチでファイルを選択できます
- STEP5. ● スイッチで、再生を開始します、同じファイルを選択した状態でもう一度押すと再生を停止します

## トラブルシュート
1. "Not found MGSDRV.COM"、"Not found PLAYER.COM" と表示される。この場合は次のことが考えられます
	- microSDカードが正しく挿入されていない。フォーマットし直しや、容量を変更を個なってみてください
	- microSDカードにMGSDRV.COM、PLAYER.COMが正しく格納されていない。格納されているか確認してください
2. "Not found KINROU5.DRV"、"Not found PLAYERK.COM" と表示される。この場合は次のことが考えられます
	- microSDカードが正しく挿入されていない。フォーマットし直しや、容量を変更を個なってみてください
	- microSDカードにKINROU5.DRV、PLAYERK.COMが正しく格納されていない。格納されているか確認してください
3. MSX SOUND ENHANCERとSCCを組み合わせるて使用するケースでFM音源が鳴らない
	- MGSPICOの電源をSW2を押しながら入れると解決します（ファイルリストが表示されるまでの１秒間押します）
	- MSX SOUND ENHANCERはFM音源認識用ダミーROMを持っていますが、SCCカートリッジをパススルースロットで使用するためにこのROMを切り離す設定にしているかと思います。そのためMGSDRVがFM音源認識できずFM音源のデータを再生しません。MGSPICOの電源をSW2を押しながら入れるとMGSPICOが持つFM音源認識用ダミーROMと同じことをMGSDRVに対して行いますので、MGSDRVがFM音源があるものとして動作します。

## ガーバーデータと部品表
- MGS-PICO-XXX/ ディレクトリ内を参照のこと。はんだ付けの難度は高いです。
- raspberry Pi Picoにインストールするファイルは、RasPiPico/dist/mgspico.uf2 です

# LICENSEと利用に関する注意事項
1. MGSPICOのファームウェアとそのソースコード、回路図データおよび資料ファイルは MIT License で配布されます。ただし、MGSPICO は、FatFsと8x16文字フォントを使用しています。FatFs/8x16文字フォントのソースコードの扱いに関しては各々のLICENSEに従ってください。
2. 本作品は同人ハードウェア＆ソフトウェアです。本作品の設計およびソフトウェアは品質を保証していません。音源カートリッジや音響設備、その周辺機器が故障、破損したとしても自身で責任を負える方のみ本作品をご利用ください。特にハードウェアの製作を伴いますのでリスクがあります。製作の腕に自身のある方のみご利用ください。
3. 本作品の設計資料とソースコードの改変や改造、また、別の作品への利用、商用利用は自由です。ただし、1. 2.の制限を超える利用は各自でその責任と義務を負ってください。

### MGSPICOが組込利用しているソフトウェア(PICO-SDK以外)
- FatFs Copyright (C) 20xx, ChaN, all right reserved. http://elm-chan.org/fsw/ff/00index_e.html
- 8x16 文字フォント FONT8X16MIN.h https://github.com/askn37/OLED_SSD1306
### 起動時に読み込んで使用しているソフトウェア
- MGSDRV (C) Ain./Gigamix https://gigamix.hatenablog.com/entry/mgsdrv/
- 勤労５号（MuSICA互換ドライバ）(C) 1996,1997 Keiichi Kuroda / BTO(MuSICA Laboratory) All rights reserved.

## 修正履歴
- 2024/02/24 firmware mgspico.uf2(v1.2)
MuSICA互換ドライバを使用してMuSICAデータファイルも再生できるようにした。
MuSICAデータ再生中のレベルインジケータ表示のために、KINROU5.DRVのワークアエリアの一部を公開いただきました作者様（@yarinige.bsky.social）と、仲介いただいたMSX Club Gigamix主宰(@nf-ban.gigamix.jp) 様に感謝申し上げます
- 2024/02/17 firmware mgspico.uf2(v1.1)
起動時に検出した音源をロゴで表示するようにした。また再生中は再生時間を表示するようにした。そのほかリファクタリング。
- 2024/02/15 MGSPICO-03C
ガーバーデータを修正した（microSDとスペーサーの位置が干渉してしまうので、microSDスロットの位置を少し移動した。機能に変更なし）。
- 2024/02/11 MGSPICO-03B
初公開

## 余禄
MGSPICOの機能に関係ないですが、開発中に見つけたものをメモ書きしておきます。
- SoundCoreSLOT EXへは、CLOCK信号を供給しなくても鳴ります。自前のクリスタルを内蔵しているようです。ただしFMPACKと同様±12Vの供給は必要です。MSX SOUND ENHANCERはCLOCK信号の供給は必須ですが、±12Vは必要ありません。音質へのこだわりなのだと思うのですが二者のアプローチの違いが面白いです。



