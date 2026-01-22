# SAROS Mobile Console 実装計画書

## 1. 概要
**SAROS Mobile Console** は、スマートフォンやタブレットなどの外部デバイスから、SAROSプロジェクト専属のAIエージェントと対話し、開発状況の確認、相談、簡易的なタスク実行を行うためのWebアプリケーションです。

IDE上のエージェント（Antigravity）の「分身」をローカルサーバーとして立ち上げ、外出先や離席時でも開発フローを回せるようにすることを目的とします。

## 2. システム構成

### アーキテクチャ
- **Frontend / Backend**: [Streamlit](https://streamlit.io/) (Python)
    - チャットUIの構築とバックエンド処理をPythonのみで完結。
- **Brain (AI)**: Google Gemini API (Gemini 1.5 Pro / Flash)
    - コード理解、回答生成、ツール判断を担当。
    - ※IDEのエージェントとは独立したAPIキーを使用。
- **Connectivity**: [ngrok](https://ngrok.com/)
    - ローカルサーバー（localhost:8501）をセキュアにインターネットへ公開。
- **Storage**: ローカルファイルシステム
    - SAROSプロジェクトのソースコード、ドキュメントに直接アクセス。

### ディレクトリ構成案
`tools/mobile_console/` 配下に配置し、メインプロジェクトとは分離して管理する。

```
SAROS/
├── tools/
│   └── mobile_console/
│       ├── app.py              # Streamlitメインアプリケーション
│       ├── agent_core.py       # Gemini APIとの通信・コンテキスト管理
│       ├── tools/              # MCP相当の機能群
│       │   ├── file_reader.py  # コード読み込み
│       │   ├── notion_sync.py  # Notion連携
│       │   └── commander.py    # コマンド実行
│       ├── .env                # APIキー設定（git管理外）
│       └── requirements.txt    # 依存ライブラリ
```

## 3. 機能ロードマップ

### Phase 1: "Ask SAROS"（相談・閲覧）
**目標**: 外出先からコードの仕様を確認したり、アイデアを壁打ちできる状態。

- **対話機能**: Gemini 1.5 Proによる自然な会話。
- **コンテキスト認識**:
    - システムプロンプトによる「SAROS開発者」としての役割付与。
    - `docs/` フォルダ内の重要ドキュメントの自動読み込み。
- **コード参照**:
    - 「`MainComponent.cpp`を見せて」で内容を表示。
    - 特定のクラスや関数の検索。

### Phase 2: "Reporter"（記録・連携）
**目標**: 思いついたアイデアやバグ報告をその場でNotionに記録し、タスク化する。

- **Notion連携**:
    - Notionデータベースの検索。
    - バグ報告、ToDo、メモの新規投稿。
- **会話ログ保存**:
    - チャットの内容をMarkdownとして保存、またはNotionにアーカイブ。

### Phase 3: "Operator"（操作・修正）
**目標**: 軽微なバグ修正やビルド実行をスマホから指示する。

- **ファイル編集**:
    - AIによるコード修正の提案と適用。
    - 新規ファイル作成。
- **Git操作**:
    - ステータス確認、add、commit、branch作成。
- **コマンド実行**:
    - ビルドコマンドの実行と結果通知。

## 4. セキュリティ対策
外部公開するため、セキュリティには十分配慮する。

1.  **Basic認証**: Streamlitまたはngrok側でパスワード認証をかける。
2.  **APIキー管理**: `.env` ファイルで管理し、リポジトリには含めない。
3.  **実行制限**: Phase 3の「ファイル書き込み」「コマンド実行」は、確認ダイアログを挟むか、ReadOnlyモードスイッチを設ける。

## 5. 必要な準備
1.  **Google AI Studio API Key**: Gemini API利用のため（無料枠で可）。
2.  **Notion API Token**: 既存のトークンを流用可能。
3.  **ngrok Account**: 固定ドメイン利用や認証機能強化のため（推奨）。

## 6. 開発フロー
1.  プロトタイプ作成（Phase 1機能の実装）。
2.  ローカル（Mac）での動作確認。
3.  ngrokによる外部公開テスト。
4.  Notion連携機能の追加（Phase 2）。
5.  運用しながら機能改善。
