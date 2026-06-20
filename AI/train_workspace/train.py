"""
YOLOv8 GPR 三类分类训练脚本（CPU）。

数据：由 organize.py 生成的 dataset/{train,val,test}/{cavities,intact,utilities}/
模型：yolov8s-cls.pt（首次运行自动下载，约 22MB）
输出：runs/gpr_cls/weights/best.pt + last.pt

预计 CPU 训练时长：1-3 小时（patience=10 通常 30 epoch 内收敛）。
"""

import sys
from pathlib import Path
from ultralytics import YOLO

HERE = Path(__file__).parent
DATA_DIR = HERE / "dataset"
RUNS_DIR = HERE / "runs"
PROJECT = str(RUNS_DIR)
NAME = "gpr_cls"
PRETRAINED = "yolov8s-cls.pt"

# 训练超参（CPU 友好）
EPOCHS = 50
IMGSZ = 224
BATCH = 32
WORKERS = 0  # Windows CPU 训练用 0 避免 DataLoader 死锁
PATIENCE = 10  # 早停：10 epoch 无提升则停


def main():
    if not DATA_DIR.exists():
        print(f"[ERROR] dataset dir not found: {DATA_DIR}")
        print("Please run `python organize.py` first.")
        sys.exit(1)

    n_train_imgs = sum(1 for _ in (DATA_DIR / "train").rglob("*") if _.is_file())
    print(f"Dataset: {DATA_DIR}")
    print(f"Train images: {n_train_imgs}")
    print(f"Model: {PRETRAINED} (CPU mode)")
    print(f"Epochs: {EPOCHS}  Batch: {BATCH}  Imgsz: {IMGSZ}")
    print("=" * 60)

    model = YOLO(PRETRAINED)

    results = model.train(
        data=str(DATA_DIR),
        epochs=EPOCHS,
        imgsz=IMGSZ,
        batch=BATCH,
        device="cpu",
        workers=WORKERS,
        project=PROJECT,
        name=NAME,
        patience=PATIENCE,
        pretrained=True,
        verbose=True,
        # 分类专用：标签平滑 + mixup 提升泛化
        label_smoothing=0.05,
        auto_augment="randaugment",
    )

    best = RUNS_DIR / NAME / "weights" / "best.pt"
    print("=" * 60)
    print(f"Best weights: {best}")
    print(f"Results saved to: {RUNS_DIR / NAME}")
    print("Next step: python export.py")


if __name__ == "__main__":
    main()
