#!/usr/bin/env python3
"""Turn log_rx.py CSVs (one per test condition) into plots and a summary table.

Usage:  python plot_results.py results/*.csv --expected 300 --interval 1.0 --outdir plots
Order of the conditions on the plots = order of the CSV files you pass in
(so list them nearest to farthest, e.g. 1m.csv 10m.csv 25m.csv).
Needs: pip install matplotlib numpy
"""
import argparse
import csv
import math
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

# Colors from the validated categorical palette (slot 1 blue, slot 2 orange) and neutral ink/surface.
BLUE, ORANGE = "#2a78d6", "#eb6834"
SURFACE, INK, INK2, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#e3e2de"

plt.rcParams.update({
    "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
    "text.color": INK, "axes.labelcolor": INK2, "xtick.color": INK2, "ytick.color": INK2,
    "axes.edgecolor": GRID, "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.8,
    "axes.spines.top": False, "axes.spines.right": False, "font.size": 11,
    "axes.titlesize": 13, "axes.titleweight": "bold", "axes.titlelocation": "left",
})


def load(path):
    t, seq, label = [], [], os.path.splitext(os.path.basename(path))[0]
    bad = 0
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            label = row["label"] or label
            s = int(row["seq"])
            if s < 0:
                bad += 1
            else:
                t.append(float(row["t_s"]))
                seq.append(s)
    return {"label": label, "t": np.array(t), "seq": np.array(seq), "bad": bad}


def wilson(k, n, z=1.96):
    """95% confidence interval for a proportion k/n."""
    if n == 0:
        return 0.0, 0.0
    p = k / n
    d = 1 + z * z / n
    c = p + z * z / (2 * n)
    m = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n))
    return (c - m) / d, (c + m) / d


def unwrap(seq):
    """16-bit sequence numbers can wrap in long runs; make them monotonic."""
    out, off, prev = [], 0, None
    for s in seq:
        if prev is not None and s < prev - 30000:
            off += 65536
        out.append(s + off)
        prev = s
    return np.array(out)


def summarize(run, expected):
    seq = unwrap(run["seq"])
    unique = np.unique(seq)
    got = len(unique)
    dup = len(seq) - got
    lost = np.setdiff1d(np.arange(expected), unique)
    # longest run of consecutive lost packets
    longest, cur, prev = 0, 0, None
    for s in lost:
        cur = cur + 1 if prev is not None and s == prev + 1 else 1
        longest = max(longest, cur)
        prev = s
    lo, hi = wilson(got, expected)
    return {"label": run["label"], "expected": expected, "received": got, "bad_crc": run["bad"],
            "duplicates": dup, "pdr": got / expected, "ci_lo": lo, "ci_hi": hi,
            "longest_loss_burst": longest, "unique": unique}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csvs", nargs="+")
    ap.add_argument("--expected", type=int, default=300, help="frames the transmitter sent per run (FRAME_COUNT)")
    ap.add_argument("--interval", type=float, default=1.0, help="transmit interval in seconds (INTERVAL_MS/1000)")
    ap.add_argument("--outdir", default="plots")
    args = ap.parse_args()
    os.makedirs(args.outdir, exist_ok=True)

    runs = [load(p) for p in args.csvs]
    sums = [summarize(r, args.expected) for r in runs]

    # ---- summary table ----
    with open(os.path.join(args.outdir, "summary.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["condition", "sent", "received", "bad_crc", "duplicates", "PDR_%", "CI95_low_%", "CI95_high_%", "longest_loss_burst"])
        for s in sums:
            w.writerow([s["label"], s["expected"], s["received"], s["bad_crc"], s["duplicates"],
                        f"{100 * s['pdr']:.1f}", f"{100 * s['ci_lo']:.1f}", f"{100 * s['ci_hi']:.1f}", s["longest_loss_burst"]])
    print(f"{'condition':<18}{'received':>10}{'PDR %':>8}{'bad CRC':>9}{'worst gap':>11}")
    for s in sums:
        print(f"{s['label']:<18}{s['received']:>6}/{s['expected']:<4}{100 * s['pdr']:>7.1f}{s['bad_crc']:>9}{s['longest_loss_burst']:>11}")

    labels = [s["label"] for s in sums]
    x = np.arange(len(sums))

    # ---- 1. Delivery ratio per condition ----
    fig, ax = plt.subplots(figsize=(max(6, 1.4 * len(sums) + 2), 4.2))
    pdr = np.array([100 * s["pdr"] for s in sums])
    lo = np.maximum(0, pdr - np.array([100 * s["ci_lo"] for s in sums]))
    hi = np.maximum(0, np.array([100 * s["ci_hi"] for s in sums]) - pdr)  # clamp tiny float error
    ax.bar(x, pdr, width=0.55, color=BLUE, zorder=3)
    ax.errorbar(x, pdr, yerr=[lo, hi], fmt="none", ecolor=INK2, elinewidth=1.4, capsize=4, zorder=4)
    for xi, v in zip(x, pdr):
        ax.text(xi, 3, f"{v:.0f}%", ha="center", va="bottom", color="white", fontweight="bold", zorder=5)
    ax.set_xticks(x, labels)
    ax.set_ylim(0, 105)
    ax.set_ylabel("Packets delivered (%)")
    ax.set_title("Packet delivery ratio by test condition")
    ax.grid(axis="x", visible=False)
    ax.text(0, -0.2, f"Error bars: 95% confidence interval. {args.expected} packets sent per condition.",
            transform=ax.transAxes, color=INK2, fontsize=9)
    fig.tight_layout()
    fig.savefig(os.path.join(args.outdir, "1_delivery_ratio.png"), dpi=200)
    plt.close(fig)

    # ---- 2. Reception timeline: one strip per condition, each cell is one packet ----
    fig, ax = plt.subplots(figsize=(10, 0.9 * len(sums) + 1.6))
    cols = 100
    rows_per = math.ceil(args.expected / cols)
    for yi, s in enumerate(sums):
        got = np.zeros(args.expected, bool)
        got[s["unique"][s["unique"] < args.expected]] = True
        for i in range(args.expected):
            r, c = divmod(i, cols)
            ax.add_patch(plt.Rectangle((c, -(yi * (rows_per + 1)) - r - 1), 0.88, 0.8,
                                       color=BLUE if got[i] else ORANGE, lw=0))
    ax.set_xlim(0, cols)
    ax.set_ylim(-len(sums) * (rows_per + 1) + 0.5, 0.5)
    ax.set_yticks([-(yi * (rows_per + 1)) - rows_per / 2 for yi in range(len(sums))], labels)
    ax.set_xlabel(f"Packet number within each row (left to right, {cols} per row)")
    ax.set_title("Which packets arrived: blue = received, orange = lost")
    ax.grid(False)
    ax.spines["left"].set_visible(False)
    ax.tick_params(axis="y", length=0)
    fig.tight_layout()
    fig.savefig(os.path.join(args.outdir, "2_reception_timeline.png"), dpi=200)
    plt.close(fig)

    # ---- 3. Gap between consecutive packets (jitter / delay) ----
    fig, axes = plt.subplots(1, len(sums), figsize=(3.6 * len(sums), 3.6), sharey=True, squeeze=False)
    for ax, run in zip(axes[0], runs):
        order = np.argsort(unwrap(run["seq"]))
        t = run["t"][order]
        seqs = unwrap(run["seq"])[order]
        ok = np.diff(seqs) == 1          # only gaps between back-to-back packets
        gaps = np.diff(t)[ok]
        if len(gaps):
            half = max(0.3, 0.5 * args.interval)   # window scales with the send interval
            ax.hist(gaps, bins=np.linspace(max(0, args.interval - half), args.interval + half, 41),
                    color=BLUE, edgecolor=SURFACE, linewidth=0.6, zorder=3)
        ax.axvline(args.interval, color=INK2, lw=1, ls="--", zorder=4)
        ax.set_title(run["label"], fontsize=11)
        ax.set_xlabel("Gap between packets (s)")
        ax.grid(axis="x", visible=False)
    axes[0][0].set_ylabel("Count")
    fig.suptitle("Spacing of consecutive packets (dashed = send interval)", x=0.01, ha="left",
                 fontsize=13, fontweight="bold")
    fig.tight_layout()
    fig.savefig(os.path.join(args.outdir, "3_packet_spacing.png"), dpi=200)
    plt.close(fig)

    print(f"\nPlots and summary.csv saved in {args.outdir}/")


if __name__ == "__main__":
    main()
