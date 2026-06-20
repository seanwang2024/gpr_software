"""
GPR 数据整理脚本：把 D:/gpr_software/GPR_data 下分类格式数据按"原始 ID 分组"
切分成 train/val/test 三个 split，避免增强样本跨 split 造成数据泄露。

输入目录结构：
    GPR_data/
        cavities/             *.JPG / *.jpg
        intact/               *.JPG / *.jpg
        Utilities/            *.JPG / *.jpg
        augmented_cavities/   <orig>_aug_<n>.jpg
        augmented_intact/     <orig>_aug_<n>.jpg
        augmented_utilities/  <orig>_aug_<n>.jpg

输出目录结构：
    dataset/
        train/{cavities,intact,utilities}/train_<class>_<00001>.jpg
        val/{cavities,intact,utilities}/val_<class>_<00001>.jpg
        test/{cavities,intact,utilities}/test_<class>_<00001>.jpg

切分策略：每个类按"原始 ID"列表随机 80/10/10 切分（seed=42），
同一原始 ID 的所有变体（原图 + 增强图）整体进入同一 split。
"""

import re
import shutil
import random
from pathlib import Path
from collections import defaultdict

SRC_ROOT = Path("D:/gpr_software/GPR_data")
DST_ROOT = Path(__file__).parent / "dataset"
STATS_FILE = Path(__file__).parent / "dataset_stats.txt"
SEED = 42
RATIO = (0.8, 0.1, 0.1)  # train / val / test

CLASS_MAP = {
    "cavities": ["cavities", "augmented_cavities"],
    "intact":   ["intact",   "augmented_intact"],
    "utilities": ["Utilities", "augmented_utilities"],
}

IMG_EXT = {".jpg", ".jpeg", ".png", ".bmp"}


def parse_orig_id(filename: str) -> str:
    """从文件名解析原始 ID。
    '10.JPG'         -> '10'
    '1 (2).JPG'      -> '1 (2)'
    '001.jpg'        -> '001'
    '10_aug_3.jpg'   -> '10'
    '1 (2)_aug_10.jpg' -> '1 (2)'
    """
    stem = Path(filename).stem
    m = re.match(r"^(.+?)_aug_\d+$", stem, re.IGNORECASE)
    return m.group(1) if m else stem


def collect_class(src_dirs):
    """收集一个类的所有图片，按原始 ID 分组。
    返回 dict: orig_id -> [Path, Path, ...]
    """
    groups = defaultdict(list)
    for d in src_dirs:
        if not d.exists():
            print(f"  [WARN] missing dir: {d}")
            continue
        for f in d.iterdir():
            if not f.is_file() or f.suffix.lower() not in IMG_EXT:
                continue
            oid = parse_orig_id(f.name)
            groups[oid].append(f)
    return groups


def split_ids(orig_ids, ratios, seed):
    """按 ratios 切分原始 ID 列表。"""
    rnd = random.Random(seed)
    ids = sorted(orig_ids)
    rnd.shuffle(ids)
    n = len(ids)
    n_train = int(round(n * ratios[0]))
    n_val = int(round(n * ratios[1]))
    train_ids = ids[:n_train]
    val_ids = ids[n_train:n_train + n_val]
    test_ids = ids[n_train + n_val:]
    return train_ids, val_ids, test_ids


def main():
    print(f"Source: {SRC_ROOT}")
    print(f"Output: {DST_ROOT}")
    DST_ROOT.mkdir(parents=True, exist_ok=True)

    stats = {}
    counters = defaultdict(int)  # (split, cls) -> counter for filename

    for cls, subdirs in CLASS_MAP.items():
        src_dirs = [SRC_ROOT / s for s in subdirs]
        print(f"\n=== Class: {cls} ===")
        groups = collect_class(src_dirs)
        print(f"  Original IDs: {len(groups)}")
        print(f"  Total images: {sum(len(v) for v in groups.values())}")

        train_ids, val_ids, test_ids = split_ids(groups.keys(), RATIO, SEED)
        print(f"  Split (by orig IDs): train={len(train_ids)} val={len(val_ids)} test={len(test_ids)}")

        cls_counts = {"train": 0, "val": 0, "test": 0}
        for split, id_list in [("train", train_ids), ("val", val_ids), ("test", test_ids)]:
            out_dir = DST_ROOT / split / cls
            out_dir.mkdir(parents=True, exist_ok=True)
            for oid in id_list:
                for src_file in groups[oid]:
                    counters[(split, cls)] += 1
                    cls_counts[split] += 1
                    dst_name = f"{split}_{cls}_{counters[(split, cls)]:05d}{src_file.suffix.lower()}"
                    dst = out_dir / dst_name
                    shutil.copy2(src_file, dst)
        stats[cls] = cls_counts

    # 写统计报告
    with STATS_FILE.open("w", encoding="utf-8") as f:
        f.write("GPR Dataset Split Statistics\n")
        f.write("=" * 50 + "\n\n")
        total = {"train": 0, "val": 0, "test": 0}
        for cls, counts in stats.items():
            f.write(f"{cls}:\n")
            for split in ["train", "val", "test"]:
                f.write(f"  {split:6s}: {counts[split]} images\n")
                total[split] += counts[split]
            f.write("\n")
        f.write("=" * 50 + "\n")
        f.write("TOTAL:\n")
        for split in ["train", "val", "test"]:
            f.write(f"  {split:6s}: {total[split]} images\n")
        grand = sum(total.values())
        f.write(f"\nGrand total: {grand} images\n")

    print(f"\nStats written to: {STATS_FILE}")
    print("Done.")


if __name__ == "__main__":
    main()
