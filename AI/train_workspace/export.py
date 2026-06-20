"""
导出训练好的 YOLOv8-cls 模型为 ONNX 格式，供 OpenCV DNN 在 C++ 端推理。

输入：runs/gpr_cls/weights/best.pt
输出：D:/gpr_software/AI/yolo_gpr_cls.onnx（约 22MB）

OpenCV DNN 推理输入约定：
- 输入尺寸：1×3×224×224（BGR 或 RGB，需在 C++ 端与 export 时颜色顺序一致）
- 输入归一化：[0,1]（即 pixel/255）
- 输出：1×N_classes 概率向量（softmax 已应用）
- opset=12（OpenCV 4.11 兼容性好）
"""

import shutil
import sys
from pathlib import Path
from ultralytics import YOLO

HERE = Path(__file__).parent
# 用 last.pt（约 epoch 16/17，acc 0.973）按用户指定；如要最佳可改回 best.pt
BEST_PT = HERE / "runs" / "gpr_cls" / "weights" / "last.pt"
TARGET_DIR = Path("D:/gpr_software/AI")
TARGET = TARGET_DIR / "yolo_gpr_cls.onnx"

IMGSZ = 224
OPSET = 12


def main():
    if not BEST_PT.exists():
        print(f"[ERROR] trained weights not found: {BEST_PT}")
        print("Please run `python train.py` first.")
        sys.exit(1)

    TARGET_DIR.mkdir(parents=True, exist_ok=True)

    print(f"Loading: {BEST_PT}")
    model = YOLO(str(BEST_PT))

    print(f"Exporting to ONNX (imgsz={IMGSZ}, opset={OPSET}) ...")
    out_path = model.export(
        format="onnx",
        imgsz=IMGSZ,
        opset=OPSET,
        simplify=True,    # onnx-simplifier 简化计算图，加速 OpenCV DNN
        dynamic=False,    # 固定 batch=1
        half=False,       # CPU 不支持 FP16
    )
    out_path = Path(out_path)
    print(f"Ultralytics produced: {out_path}")

    if out_path.resolve() != TARGET.resolve():
        shutil.move(str(out_path), str(TARGET))
    print(f"Moved to: {TARGET}")
    print(f"Size: {TARGET.stat().st_size / 1024 / 1024:.2f} MB")
    print("Done.")


if __name__ == "__main__":
    main()
