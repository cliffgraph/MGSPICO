# playersk(.com)
playersk(.com) は、mgspico.uf2からの指示を受けMGSDRV.comを制御するための物です。
MSGPIC は、MGSPICO-xxx 基板と３つのソフトウェアから構成されています。
- MGSPICO-XXA
- MGSDRV(.com) (in a MicroSD card)
- playersk(.com) (in a MicroSD card)
- mgspico.uf2 (in a RP2040 flash-rom)

## How to build
### Preparation
- ソースコード playersk.z80 からアセンブラを使用して playersk(.com) ファイルを生成します。
- 作業はWindowsで行います。
- アセンブラ は、AILZ80ASM.exe を使用します（https://github.com/AILight/AILZ80ASM）
今回、AILZ80ASM v1.0.12 を使用しました

### Build 
1. AILZ80ASMをダウンロードして、AILZ80ASM.exeを同じフォルダに格納する。
2. build.cmd を実行する
3. playersk(.com) が生成される

/Harumakkin
