from pathlib import Path
corpus=Path("/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/corpus")
vscode=Path("/Applications/Visual Studio Code.app/Contents/Resources/app/node_modules.asar.unpacked")
runtime=Path("/Users/adnanbenali/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules")
cases=[
    ("typescript",vscode/"@vscode/tree-sitter-wasm/wasm/tree-sitter-typescript.wasm",5,4),
    ("json",corpus/"api.json",5,4),
    ("ocr",runtime/"tesseract.js-core/tesseract-core.wasm",5,4),
    ("jpeg2000",runtime/"pdfjs-dist/wasm/openjpeg.wasm",5,4),
    ("typescript-t1",vscode/"@vscode/tree-sitter-wasm/wasm/tree-sitter-typescript.wasm",5,1),
    ("typescript-light",vscode/"@vscode/tree-sitter-wasm/wasm/tree-sitter-typescript.wasm",3,4),
    ("retina",corpus/"retina.ppm",5,3),
]
