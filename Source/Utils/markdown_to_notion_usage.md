# markdown_to_notion.py 使い方

マークダウンをNotion APIで使えるブロック形式に変換、または直接投稿するスクリプト。

## 対応フォーマット

- 見出し (`# ## ###`)
- 箇条書きリスト (`-`)
- 番号リスト (`1. 2. 3.`)
- チェックボックス (`- [ ]` / `- [x]`)
- コードブロック (` ```言語 ... ``` `) - mermaid対応
- 引用 (`>`)
- 区切り線 (`---`)
- インライン装飾: `**太字**`, `*斜体*`, `~~取り消し~~`, `` `コード` ``, `==ハイライト==`, `[リンク](url)`

## 使用方法

### 1. JSON出力（ブロック配列のみ）

```bash
# ファイルから
python3 markdown_to_notion.py docs/plan.md

# 標準入力から
echo "# Hello" | python3 markdown_to_notion.py
```

### 2. APIパラメータ形式で出力

```bash
python3 markdown_to_notion.py docs/plan.md --api
# → {"children": [...]} 形式

python3 markdown_to_notion.py docs/plan.md --api --block-id "ページID"
# → {"children": [...], "block_id": "..."} 形式
```

### 3. 直接Notionに投稿

```bash
# 環境変数でトークン設定
export NOTION_API_TOKEN="secret_xxx..."

# 投稿
python3 markdown_to_notion.py docs/plan.md --post --block-id "2dca7fbf-f43c-806d-829d-cacf8cae5835"

# トークンを引数で渡す場合
python3 markdown_to_notion.py docs/plan.md --post -b "ページID" -t "secret_xxx"
```

### 4. タスク管理（to-do操作）

```bash
# タスク一覧を表示
python3 markdown_to_notion.py --list-todos -b "2dca7fbf-f43c-806d-829d-cacf8cae5835"
# 出力例:
# [ ] 1a2b3c4d... : バグ修正
# [x] 5e6f7g8h... : ドキュメント作成
# [ ] 9i0j1k2l... : テスト追加

# タスクをチェック（IDの先頭数文字で指定）
python3 markdown_to_notion.py --check "1a2b" -b "ページID"

# タスクをチェック（テキストの部分一致で指定）
python3 markdown_to_notion.py --check "バグ修正" -b "ページID"

# タスクのチェックを外す
python3 markdown_to_notion.py --uncheck "バグ修正" -b "ページID"
```

## オプション一覧

| オプション | 短縮 | 説明 |
|-----------|------|------|
| `--api` | - | APIパラメータ形式（`children`キー付き）で出力 |
| `--post` | - | 直接Notionに投稿 |
| `--block-id` | `-b` | 投稿先のページ/ブロックID |
| `--token` | `-t` | Notion APIトークン（または`NOTION_API_TOKEN`環境変数） |
| `--list-todos` | - | ページ内のto-doアイテムを一覧表示 |
| `--check` | - | to-doをチェック済みにする（IDまたはテキストで指定） |
| `--uncheck` | - | to-doのチェックを外す（IDまたはテキストで指定） |

## SAROSアプリのページID

```
2dca7fbf-f43c-806d-829d-cacf8cae5835
```

## 他プロジェクトで使用する場合

このスクリプトは汎用的に使えます。以下を準備してください:

1. **Notionインテグレーション作成**
   - https://www.notion.so/my-integrations でインテグレーションを作成
   - APIトークン（`secret_xxx...`）を取得

2. **ページへの接続**
   - 投稿先のNotionページを開く
   - 右上「...」→「接続」→ 作成したインテグレーションを追加
   - これがないと403エラーになる

3. **ページIDの取得**
   - NotionページのURLから取得: `https://notion.so/PageName-{ページID}`
   - またはページを「リンクをコピー」してIDを抽出

4. **使用**
   ```bash
   export NOTION_API_TOKEN="secret_xxx..."
   python3 markdown_to_notion.py content.md --post -b "ページID"
   ```
