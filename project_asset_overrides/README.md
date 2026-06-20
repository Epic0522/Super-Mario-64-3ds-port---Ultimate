## Project Asset Overrides

These files are generated asset-side sources that were intentionally modified by this project.

`extract_assets.py` can recreate the base generated files, but it can also overwrite these project-specific adjustments. After extracting assets locally, copy the contents of this directory back into the repository root so the modified files replace the freshly extracted versions.

Example:

```sh
cp -R project_asset_overrides/* .
```

The directory layout mirrors the repository layout on purpose, so the copied files land at the correct paths.

## 项目资产覆盖文件

这里存放的是本项目刻意修改过的“生成资产侧源码”。

`extract_assets.py` 能重新生成基础文件，但也可能覆盖这些带有项目特定调整的版本。因此，在本地完成资产提取后，需要将本目录中的内容再复制回仓库根目录，让这些修改过的文件覆盖新提取出的版本。

示例：

```sh
cp -R project_asset_overrides/* .
```

本目录故意保持与仓库相同的路径结构，这样复制后就会落到正确的位置。
