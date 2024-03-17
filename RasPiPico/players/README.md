# MSGPICO software
## players(.com) and playersk(.com)
players(.com) は、mgspico.uf2からの指示を受けMGSDRV.comを制御するためのものです。
playersk(.com) は、mgspico.uf2からの指示を受けKINROU5.BINを制御するためのものです。
playersとplayerskは、ファイル先頭のDRV_KINR5の定義が#TRUEか#FALSEだけの違いです。
MSGPICO は、MGSPICO-xxx 基板と４つのソフトウェアから構成されています。
- MGSDRV(.com) (in a MicroSD card)
- players(.com) (in a MicroSD card)
- playersk(.com) (in a MicroSD card)
- mgspico.uf2 (in a RP2040 flash-rom)

## How to build
### Preparation
- ソースコード players.z80, playersk.z80 アセンブラしてふたつのファイルを生成します。
- 作業はWindowsで行います。
- アセンブラ は、AILZ80ASM.exe を使用します（https://github.com/AILight/AILZ80ASM）
今回、AILZ80ASM v1.0.12 を使用しました

### Build 
1. AILZ80ASMをダウンロードして、AILZ80ASM.exeを同じフォルダに格納する。
2. build.cmd を実行する
3. players(.com)とplayersk(.com) が生成される

/Harumakkin
