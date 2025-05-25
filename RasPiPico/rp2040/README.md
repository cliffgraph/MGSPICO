# ビルドの方法

## 前準備
- pico-sdk(https://github.com/raspberrypi/pico-sdk) がセットアップされていること（方法は省略）
- AILZ80ASM.exe(https://github.com/AILight/AILZ80ASM) を入手し、playlibフォルダに、AILZ80ASM.exe を格納する

## Raspberry Pi Pico 用のプログラムを生成する
```sh
> mkdir build
> cd build
> cmake -G "NMake Makefiles" ..
> nmake
```
build/mgspico/に、mgspico.uf2 が生成される

* Raspberry Pi Pico 2(RP2350)用のプログラムを生成したい場合は、
> cmake -G "NMake Makefiles" ..

の部分を、

> cmake -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2 -G "NMake Makefiles" ..

に変更すること


## ビルドの方法（もう少し詳しく）
（１）Developer PowerShell for VS 2019 を開く
（２）mgspico フォルダと同階層に、build フォルダを作成する。
（３）build フォルダへ移動する
（４）下記を実行する

> set PICO_SDK_PATH "C:\Pico\pico-sdk"	*1
> cmake -G "NMake Makefiles" ..

（５）ビルド実行

> nmake

（６）build\mgspico\ フォルダに、mgspico.uf2 ファイルが出来上がればOK。


*1: "C:\Pico\pico-sdk" は、pico-sdkを格納している場所へのパス。環境に合わせて変更すること。
PowerShell の場合は、 $env:PICO_SDK_PATH="C:\Pico\pico-sdk"

2025/05/25 harumakkin
以上

