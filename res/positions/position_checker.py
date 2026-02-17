#!/usr/bin/env python3
"""
Find overlapping XYZ positions across multiple files.
Usage: python find_overlaps.py file1.txt file2.txt file3.txt ...
"""

import sys
from collections import defaultdict


def parse_positions(filepath):
    """Parse XYZ positions from a file."""
    positions = []
    with open(filepath, "r") as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) >= 3:
                try:
                    pos = (float(parts[0]), float(parts[1]), float(parts[2]))
                    positions.append(pos)
                except ValueError:
                    print(f"  Warning: Skipping invalid line {line_num} in {filepath}: {line}")
    return positions


def find_overlaps(filepaths, tolerance=5e-1):
    """Find positions that appear in multiple files.
    
    Args:
        filepaths: List of file paths to check.
        tolerance: Floating point comparison tolerance. Positions within this
                   distance (per axis) are considered the same.
    """
    # Map each position to the set of files it appears in
    # Round to handle floating point noise
    decimals = max(0, -int(f"{tolerance:e}".split("e")[1]))  # derive from tolerance
    pos_to_files = defaultdict(set)

    for fpath in filepaths:
        try:
            positions = parse_positions(fpath)
            print(f"Loaded {len(positions)} positions from {fpath}")
            for pos in positions:
                key = (round(pos[0], decimals), round(pos[1], decimals), round(pos[2], decimals))
                pos_to_files[key].add(fpath)
        except FileNotFoundError:
            print(f"Error: File not found: {fpath}")
        except Exception as e:
            print(f"Error reading {fpath}: {e}")

    # Filter to only positions found in 2+ files
    overlaps = {pos: files for pos, files in pos_to_files.items() if len(files) > 1}
    return overlaps


def main():
    if len(sys.argv) < 2:
        print("Usage: python find_overlaps.py file1.txt file2.txt ...")
        sys.exit(1)

    filepaths = sys.argv[1:]
    print(f"Checking {len(filepaths)} files for overlapping positions...\n")

    overlaps = find_overlaps(filepaths)

    if not overlaps:
        print("\nNo overlapping positions found.")
    else:
        print(f"\nFound {len(overlaps)} overlapping position(s):\n")
        for pos, files in sorted(overlaps.items()):
            print(f"  Position: {pos[0]:>12.6f} {pos[1]:>12.6f} {pos[2]:>12.6f}")
            print(f"  Found in: {', '.join(sorted(files))}")
            print()


if __name__ == "__main__":
    main()