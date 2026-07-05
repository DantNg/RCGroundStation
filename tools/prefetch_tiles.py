#!/usr/bin/env python3
"""Tải trước tile bản đồ (vd toàn Việt Nam) vào cache của GCS để dùng offline.

Ứng dụng vẽ bản đồ 2D bằng tile XYZ tải online rồi cache ra
``~/.lite_gcs/tiles/{provider}/{z}/{x}/{y}.png`` (xem map_widget.cpp). Ở thực địa
không có internet, tile chưa từng tải = ô trống. Script này tải trước tile cho
một khung bao (mặc định: Việt Nam) vào đúng thư mục cache đó, nên khi ra hiện
trường bản đồ đã có sẵn.

Bố cục đĩa khớp app: {provider}/{z}/{x}/{y}.png — provider là "Satellite" hoặc
"Street" (khớp tên trong providers() của map_widget.cpp).

CẢNH BÁO chính sách: tải hàng loạt tile OSM (Street) vi phạm Tile Usage Policy
của OpenStreetMap; hãy giữ zoom thấp/vừa và số lượng nhỏ, hoặc dùng nguồn tile
riêng cho vùng zoom cao. ArcGIS World Imagery (Satellite) dễ chịu hơn nhưng vẫn
nên dùng chừng mực. Zoom cao chỉ nên tải quanh khu vực bay, đừng tải toàn quốc.

Ví dụ:
    # Ảnh vệ tinh toàn VN, zoom 5..12 (mặc định)
    python tools/prefetch_tiles.py

    # Bản đồ đường, zoom 5..11
    python tools/prefetch_tiles.py --provider Street --zmax 11

    # Zoom cao quanh một sân bay (bbox nhỏ)
    python tools/prefetch_tiles.py --zmin 13 --zmax 16 \
        --bbox 105.75 20.98 105.90 21.08
"""
from __future__ import annotations

import argparse
import math
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

# Khớp providers() và kUserAgent trong cpp/src/ui/map_widget.cpp.
USER_AGENT = "LiteGCS-Desktop/1.0 (+https://github.com/)"
PROVIDERS = {
    "Satellite": {
        # ArcGIS dùng thứ tự {z}/{y}/{x}.
        "url": "https://server.arcgisonline.com/ArcGIS/rest/services/"
               "World_Imagery/MapServer/tile/{z}/{y}/{x}",
        "max_zoom": 19,
    },
    "Street": {
        "url": "https://tile.openstreetmap.org/{z}/{x}/{y}.png",
        "max_zoom": 19,
    },
}

# Khung bao Việt Nam (đất liền), lon/lat. Điều chỉnh bằng --bbox nếu cần.
VN_BBOX = (102.0, 8.0, 110.0, 23.6)  # west, south, east, north


def cache_dir() -> Path:
    """Khớp cacheDir() trong map_widget.cpp: ~/.lite_gcs/tiles."""
    return Path.home() / ".lite_gcs" / "tiles"


def lon2x(lon: float, z: float) -> int:
    n = 2 ** z
    return int((lon + 180.0) / 360.0 * n)


def lat2y(lat: float, z: float) -> int:
    n = 2 ** z
    lat = max(min(lat, 85.05112878), -85.05112878)
    lat_rad = math.radians(lat)
    return int((1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n)


def tile_range(bbox, z):
    """Trả (x0, x1, y0, y1) bao trọn bbox ở mức zoom z (bao gồm hai đầu)."""
    west, south, east, north = bbox
    x0, x1 = lon2x(west, z), lon2x(east, z)
    # vĩ độ lớn (bắc) → y nhỏ, nên y0 tính từ north.
    y0, y1 = lat2y(north, z), lat2y(south, z)
    n = 2 ** z
    clamp = lambda v: max(0, min(n - 1, v))
    return clamp(min(x0, x1)), clamp(max(x0, x1)), clamp(min(y0, y1)), clamp(max(y0, y1))


def count_tiles(bbox, zmin, zmax) -> int:
    total = 0
    for z in range(zmin, zmax + 1):
        x0, x1, y0, y1 = tile_range(bbox, z)
        total += (x1 - x0 + 1) * (y1 - y0 + 1)
    return total


def fetch_one(session_url_tmpl, dest_root, provider_name, z, x, y, retries=2):
    """Tải một tile về đĩa nếu chưa có. Trả 'ok' | 'skip' | 'fail'."""
    path = dest_root / provider_name / str(z) / str(x) / f"{y}.png"
    if path.exists() and path.stat().st_size > 0:
        return "skip"
    url = session_url_tmpl.format(z=z, x=x, y=y)
    for attempt in range(retries + 1):
        try:
            req = Request(url, headers={"User-Agent": USER_AGENT})
            with urlopen(req, timeout=20) as resp:
                data = resp.read()
            if not data:
                return "fail"
            path.parent.mkdir(parents=True, exist_ok=True)
            tmp = path.with_suffix(".png.part")
            tmp.write_bytes(data)
            tmp.replace(path)
            return "ok"
        except HTTPError as e:
            if e.code in (404, 403):
                return "fail"  # không có tile / bị chặn — đừng thử lại
        except (URLError, TimeoutError, OSError):
            pass
        if attempt < retries:
            time.sleep(0.5 * (attempt + 1))
    return "fail"


def main() -> int:
    # Console Windows mặc định cp1252 không in được tiếng Việt — ép UTF-8.
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8")
        except (AttributeError, ValueError):
            pass

    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--provider", choices=sorted(PROVIDERS), default="Satellite",
                    help="Lớp tile cần tải (mặc định Satellite).")
    ap.add_argument("--zmin", type=int, default=5, help="Zoom nhỏ nhất (mặc định 5).")
    ap.add_argument("--zmax", type=int, default=12, help="Zoom lớn nhất (mặc định 12).")
    ap.add_argument("--bbox", type=float, nargs=4, metavar=("W", "S", "E", "N"),
                    default=list(VN_BBOX), help="Khung bao lon/lat: west south east north.")
    ap.add_argument("--workers", type=int, default=4,
                    help="Số luồng tải song song (giảm với OSM cho lịch sự).")
    ap.add_argument("--out", type=Path, default=cache_dir(),
                    help="Thư mục cache (mặc định ~/.lite_gcs/tiles).")
    ap.add_argument("--yes", action="store_true", help="Bỏ qua hỏi xác nhận.")
    args = ap.parse_args()

    if args.zmin > args.zmax:
        ap.error("--zmin phải ≤ --zmax")
    max_zoom = PROVIDERS[args.provider]["max_zoom"]
    if args.zmax > max_zoom:
        ap.error(f"{args.provider} chỉ hỗ trợ tới zoom {max_zoom}")

    bbox = tuple(args.bbox)
    url_tmpl = PROVIDERS[args.provider]["url"]
    total = count_tiles(bbox, args.zmin, args.zmax)

    print(f"Provider : {args.provider}")
    print(f"BBox     : W={bbox[0]} S={bbox[1]} E={bbox[2]} N={bbox[3]}")
    print(f"Zoom     : {args.zmin}..{args.zmax}")
    print(f"Đích     : {args.out}")
    print(f"Số tile  : ~{total:,} (kích thước ước tính ~{total * 20 / 1024:.1f} MB)")
    if total > 200_000:
        print("!! Rất nhiều tile — cân nhắc giảm zmax hoặc thu nhỏ bbox.")
    if args.provider == "Street" and args.zmax >= 13:
        print("!! Tải hàng loạt OSM ở zoom cao vi phạm chính sách OSM. Hãy thận trọng.")
    if not args.yes:
        try:
            if input("Tiếp tục tải? [y/N] ").strip().lower() not in ("y", "yes"):
                print("Đã hủy.")
                return 1
        except EOFError:
            print("Không có tty — dùng --yes để tải không hỏi.")
            return 1

    ok = skip = fail = 0
    done = 0
    t_start = time.time()

    def work(job):
        z, x, y = job
        return fetch_one(url_tmpl, args.out, args.provider, z, x, y)

    jobs = []
    for z in range(args.zmin, args.zmax + 1):
        x0, x1, y0, y1 = tile_range(bbox, z)
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                jobs.append((z, x, y))

    with ThreadPoolExecutor(max_workers=max(1, args.workers)) as ex:
        for result in ex.map(work, jobs):
            done += 1
            if result == "ok":
                ok += 1
            elif result == "skip":
                skip += 1
            else:
                fail += 1
            if done % 200 == 0 or done == total:
                elapsed = time.time() - t_start
                rate = done / elapsed if elapsed else 0
                print(f"\r{done:,}/{total:,}  ok={ok} skip={skip} fail={fail}  "
                      f"{rate:.0f} tile/s", end="", flush=True)

    print()
    print(f"Xong: tải {ok}, bỏ qua {skip} (đã có), lỗi {fail}.")
    if fail:
        print("Một số tile lỗi (có thể ngoài vùng phủ hoặc bị giới hạn tốc độ).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
