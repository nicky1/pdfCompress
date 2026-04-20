# pdfCompress

Windows PDF 压缩工具（GUI），支持：

- 普通压缩（QPDF）
- 强压缩（Ghostscript：高/中/低质量）

## 打包说明

仓库内的 GitHub Actions 工作流会自动打包：

- `pdf_compress.exe`
- `tools/ghostscript/...`（随包分发，强压缩模式可直接用）

下载 artifact 后解压即可运行，无需用户单独安装 Ghostscript。