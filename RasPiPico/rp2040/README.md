##ビルドの方法

```sh
> mkdir build
> cd build
> cmake -G "NMake Makefiles" ..
> nmake
```
build/mgspico/に、mgspico.uf2　が生成される.

##ビルドの方法（もう少し詳しく）
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


以上

