# GitHub Actions Windows ビルド設定

## 概要

GitHub Actionsを使用して、プッシュ時に自動的にWindows用のEXEファイルをビルドする仕組みを設定しました。

## 変更内容

### 1. CMakeLists.txt の修正

`CI_BUILD`フラグを追加し、環境に応じてJUCEのパスを切り替えるようにしました：

- **ローカル環境** (`CI_BUILD=OFF`): `/Users/mtsh/JUCE` を使用
- **CI環境** (`CI_BUILD=ON`): プロジェクトルートにダウンロードしたJUCEを使用

また、Apple Silicon向けの設定をmacOSのみに制限しました。

### 2. GitHub Actions ワークフロー

`.github/workflows/build-windows.yml` を作成：

| 項目 | 設定 |
|------|------|
| トリガー | main/masterへのpush、PR、手動実行 |
| OS | windows-latest |
| JUCEバージョン | 7.0.9 |
| キャッシュ | JUCEをキャッシュして2回目以降のビルドを高速化 |
| 出力 | EXEファイルをArtifactとしてアップロード |

## 使い方

### 自動ビルド

1. コードをmainまたはmasterブランチにプッシュ
2. GitHubリポジトリの「Actions」タブでビルド状況を確認
3. ビルド完了後、「Artifacts」セクションから`SAROS-Windows`をダウンロード

### 手動ビルド

1. GitHubリポジトリの「Actions」タブを開く
2. 「Build Windows」ワークフローを選択
3. 「Run workflow」ボタンをクリック

## ファイル構成

```
SAROS/
├── .github/
│   └── workflows/
│       └── build-windows.yml  # Windowsビルド用ワークフロー
├── CMakeLists.txt             # CI_BUILD対応版
└── ...
```

## 注意事項

- JUCEのバージョンは現在 **7.0.9** に固定されています
- ビルド時間は初回約10〜15分、キャッシュ使用時は約5〜8分程度
- 成果物は90日間保持されます
