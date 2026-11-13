#!/usr/bin/env python3
"""Log frames from stress_tx.ino arriving at radio #2 (via the FT232 adapter).

Usage (live):    python log_rx.py --port COM6 --size 15 --label 10m --out results/10m.csv
Usage (replay):  python log_rx.py --replay capture.bin --size 15 --label test --out results/test.csv

Frame format: [0xA5][seq hi][seq lo][pad x (size-4)][crc8].  --size is the total frame length.
Stop a live run with Ctrl+C (or use --duration seconds). One CSV row per good frame (seq=-1 marks a corrupted frame).
"""
import argparse
import csv
import os
import sys
import time

MAGIC = 0xA5


def crc8(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


class Parser:
    """Pulls valid frames out of a byte stream, resyncing on the magic byte after errors."""

    def __init__(self, size):
        self.size = size
        self.buf = bytearray()
        self.bad = 0  # frames that looked like frames but failed the CRC

    def feed(self, data: bytes):
        self.buf += data
        out = []
        while True:
            i = self.buf.find(bytes([MAGIC]))
            if i < 0:
                self.buf.clear()
                break
            if i > 0:
                del self.buf[:i]
            if len(self.buf) < self.size:
                break
            frame = bytes(self.buf[: self.size])
            if crc8(frame[:-1]) == frame[-1]:
                out.append((frame[1] << 8) | frame[2])
                del self.buf[: self.size]
            else:
                self.bad += 1
                out.append(-1)  # -1 marks a corrupted frame in the CSV
                del self.buf[0]  # skip this magic byte and look for the next one
        return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--size", type=int, default=15, help="total frame bytes (PAYLOAD_PAD + 4)")
    ap.add_argument("--label", required=True, help="condition name, e.g. 10m or 'through wall'")
    ap.add_argument("--out", required=True)
    ap.add_argument("--duration", type=float, default=0, help="stop after N seconds (0 = until Ctrl+C)")
    ap.add_argument("--replay", help="read bytes from a file instead of a serial port (testing)")
    args = ap.parse_args()

    parser = Parser(args.size)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    rows = []
    t0 = time.time()

    def handle(chunk, now):
        for seq in parser.feed(chunk):
            rows.append((now - t0, seq))
            tag = "BAD CRC" if seq < 0 else f"seq {seq}"
            print(f"{tag}  (t={now - t0:.1f}s)", flush=True)

    if args.replay:
        with open(args.replay, "rb") as f:
            data = f.read()
        for i in range(0, len(data), 7):          # feed in odd chunks, like a real port
            handle(data[i:i + 7], t0 + i / 1000.0)
    else:
        if not args.port:
            sys.exit("--port is required for a live run")
        import serial                               # pip install pyserial
        ser = serial.Serial(args.port, args.baud, timeout=0.2)
        print(f"Listening on {args.port} at {args.baud} baud. Ctrl+C to stop.")
        try:
            while True:
                chunk = ser.read(64)
                now = time.time()
                if chunk:
                    handle(chunk, now)
                if args.duration and now - t0 >= args.duration:
                    break
        except KeyboardInterrupt:
            pass
        finally:
            ser.close()

    with open(args.out, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["label", "t_s", "seq"])
        for t, seq in rows:
            w.writerow([args.label, f"{t:.3f}", seq])

    print(f"\nSaved {len(rows) - parser.bad} good frames, {parser.bad} bad-checksum frames -> {args.out}")


if __name__ == "__main__":
    main()
