# SAROS 🌌

**A futuristic, high-tech audio looper built with JUCE**

![Platform](https://img.shields.io/badge/platform-macOS-blue)
![Framework](https://img.shields.io/badge/framework-JUCE-orange)
![License](https://img.shields.io/badge/license-GPLv3-green)

## 概要

SAROSは、サイバーパンク/フューチャリスティックなUIを持つ8トラック対応のオーディオルーパーです。リアルタイムオーディオ処理、視覚的なフィードバック、直感的な操作性を備えています。

## ✨ 特徴

### 🎛 コアルーパー機能
- **8トラック同時録音/再生** - 複数のループを重ねてレイヤー構築
- **マスタートラック同期** - 最初に録音したトラックに自動同期
- **ルックバック録音** - トリガー前の音声も自動キャプチャ
- **UNDO機能** - 直前の録音を取り消し
- **トラック別ゲイン調整** - 各トラックの音量を個別に制御

### 🎨 ビジュアライザー
- **円形波形表示** - 録音した波形をリング状に可視化
- **パーティクルアニメーション** - 音楽に反応する動的なパーティクル
- **ズーム/ドラッグ操作** - マウスで拡大・縮小
- **スペクトラム表示** - リアルタイムFFT解析
- **星空背景** - 没入感のあるコスミックなUI

### 🎚 エフェクト (FXPanel)
各トラックに最大4スロットのエフェクトチェーン:
- **Filter** - LPF/HPF切り替え、カットオフ、レゾナンス調整
- **Compressor** - スレッショルド、レシオ調整
- **Delay** - タイム、フィードバック、ミックス調整
- **Reverb** - ルームサイズ、ダンピング、ミックス調整
- **Beat Repeat** - ディビジョン、スレッショルド調整

### 🎹 オーディオ入力
- **自動トリガー検出** - 音声入力で自動録音開始
- **スレッショルド調整** - 感度をカスタマイズ可能
- **スマートゲート** - ノイズを自動カット

### ⌨️ キーボードショートカット
- **カスタムキーマッピング** - 「Settings」でアクションにキーを割り当て可能
- **トランスポート制御** - REC, PLAY, UNDOなどをキー操作
- **トラック操作** - トラック選択、オートアーム切替

### 🎨 UIデザイン
- **サイバーパンクテーマ** - ダークな背景にネオン発光
- **カスタムノブデザイン** - 惑星をモチーフにしたノブ
- **アニメーション付きボタン** - グロウエフェクト
- **レスポンシブレイアウト** - ウィンドウサイズに適応

## 🚀 必要環境

- **OS**: macOS
- **JUCE**: v8.0.10+
- **CMake**: 3.15+
- **C++**: C++17対応コンパイラ

## 🔧 ビルド方法

```bash
# リポジトリをクローン
git clone https://github.com/happytown-s/SAROS.git
cd SAROS

# ビルドディレクトリ作成
mkdir -p build && cd build

# CMake構成
cmake ..

# ビルド
cmake --build .

# アプリケーション実行
open SAROSApp/SAROS.app
```

## 📂 プロジェクト構成

```
SAROS/
├── Source/
│   ├── MainComponent.cpp/h   # メインアプリケーション
│   ├── LooperAudio.cpp/h     # オーディオエンジン
│   ├── CircularVisualizer.h  # 円形ビジュアライザー
│   ├── FXPanel.cpp/h         # エフェクトパネル
│   ├── TransportPanel.cpp/h  # トランスポートコントロール
│   ├── InputManager.cpp/h    # オーディオ入力管理
│   ├── KeyboardMappingManager.h # キーボーマッピング管理
│   ├── ThemeColours.h        # カラースキーム定義
│   └── ...
├── CMakeLists.txt
├── LICENSE               # ライセンスファイル
└── README.md
```

## 🎮 操作方法

| 操作                     | 説明                                         |
| ------------------------ | -------------------------------------------- |
| トラッククリック         | トラック選択 (トグル)                        |
| REC                      | 録音開始/停止                                |
| PLAY                     | 全トラック再生                               |
| UNDO                     | 直前の録音取り消し                           |
| CLEAR                    | 全トラッククリア                             |
| FX                       | エフェクトパネル切り替え                     |
| ビジュアライザードラッグ | ズームイン/アウト                            |
| ダブルクリック           | ズームリセット                               |
| 設定 (⚙)                 | オーディオデバイス・トリガー・キーボード設定 |

## 📝 ライセンス

GNU General Public License v3 (GPL v3)

## 🙏 クレジット

- [JUCE Framework](https://juce.com/) - オーディオ/GUI フレームワーク
