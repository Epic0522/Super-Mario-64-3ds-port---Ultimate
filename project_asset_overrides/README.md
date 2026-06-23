## Project Asset Overrides

These files are generated asset-side sources that were intentionally modified by this project.

`extract_assets.py` can recreate the base generated files, but it can also overwrite these project-specific adjustments. After extracting assets locally, copy the contents of this directory back into the repository root so the modified files replace the freshly extracted versions.

Example:

```sh
cp -R project_asset_overrides/* .
```

The directory layout mirrors the repository layout on purpose, so the copied files land at the correct paths.

## プロジェクトアセット上書きファイル

ここには、本プロジェクトで意図的に変更した生成アセット側のソースファイルを保存しています。

`extract_assets.py` は元になる生成ファイルを再作成できますが、同時に本プロジェクト固有の調整を上書きしてしまう場合があります。ローカルでアセット抽出を行った後、このディレクトリの内容をリポジトリのルートへコピーし、抽出されたファイルを調整済みのバージョンで置き換えてください。

例：

```sh
cp -R project_asset_overrides/* .
```

このディレクトリはリポジトリ本体と同じパス構造にしてあるため、コピーすると各ファイルが正しい場所へ配置されます。
