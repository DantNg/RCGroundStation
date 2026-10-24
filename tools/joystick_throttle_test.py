#!/usr/bin/env python3
"""Thử thuật toán xử lý THROTTLE cho cần lái ảo TỰ TRẢ VỀ GIỮA.

Vấn đề: cần lái ảo (và nhiều cần vật lý) tự bật về GIỮA khi nhả tay. Với
roll/pitch/yaw thì đúng — giữa = trung tính = 0. Nhưng THROTTLE thì "nhả tay về
giữa" nghĩa là ga về mid → drone tự giữ/nhảy ga ngoài ý muốn. Ta cần một cách để
throttle hành xử hợp lý khi nhả tay.

UI này mô phỏng một cần lái springs-to-center và cho chọn 4 thuật toán throttle
để so sánh trực tiếp (đồ thị + số). Không kết nối mạng — chỉ để thử thuật toán.
Ánh xạ ra MANUAL_CONTROL.z ∈ [0,1000] (giống app thật).

Chạy:  python tools/joystick_throttle_test.py   (chỉ cần Python + tkinter stdlib)

Bốn thuật toán:
  1. ABS-GIỮA   throttle = (y+1)/2         → nhả tay: ga về 0.5 (mid). Hợp
                 ALT_HOLD (mid≈giữ độ cao) nhưng KHÔNG về 0.
  2. ABS-ĐÁY    throttle = max(0, y)        → chỉ nửa TRÊN của cần dùng cho ga;
                 nhả tay (giữa): ga về 0. Nửa dưới bỏ. Đơn giản, an toàn mặt đất.
  3. TÍCH-PHÂN  throttle += y * rate * dt   → đẩy lên: ga TĂNG dần; đẩy xuống:
   (KHUYẾN NGHỊ) GIẢM dần; nhả tay (giữa, y=0): ga GIỮ nguyên. Đây là cách chuẩn
                 cho cần tự về giữa: kéo xuống hết để về 0, không rơi đột ngột.
  4. CHỐT-GIỮ   khi giữ: ga = (y+1)/2; nhả tay: GIỮ giá trị cuối (latch).
"""
import tkinter as tk
from collections import deque

# ── tham số hiển thị / vật lý ─────────────────────────────────────────────────
PAD = 280            # cạnh vùng cần lái (px)
C = PAD / 2          # tâm
KNOB_R = 26          # bán kính núm
TRAVEL = C - KNOB_R - 8  # tầm di chuyển tối đa của tâm núm
TICK_MS = 30         # chu kỳ vòng lặp (ms)
SPRING = 0.45        # hệ số bật về giữa mỗi tick khi nhả (0..1, lớn = nhanh)
GRAPH_W, GRAPH_H = 460, 180
HIST = deque(maxlen=GRAPH_W)  # lịch sử throttle để vẽ đồ thị


class JoyTester:
    def __init__(self, root):
        self.root = root
        root.title("Thử thuật toán throttle — cần lái tự về giữa")
        root.configure(bg="#0a0d0b")

        # trạng thái cần
        self.kx = 0.0   # -1..1 (phải dương)  → yaw
        self.ky = 0.0   # -1..1 (lên dương)   → điều khiển throttle
        self.held = False
        self.throttle = 0.0  # 0..1 (giá trị ga tích luỹ / hiện tại)
        self.mode = tk.StringVar(value="rate")
        self.rate = tk.DoubleVar(value=0.8)  # đơn vị ga / giây (mode tích phân)

        self._build_ui()
        self._tick()

    # ── dựng giao diện ────────────────────────────────────────────────────────
    def _build_ui(self):
        wrap = tk.Frame(self.root, bg="#0a0d0b")
        wrap.pack(padx=14, pady=14)

        # cần lái (trái)
        left = tk.Frame(wrap, bg="#0a0d0b")
        left.grid(row=0, column=0, padx=(0, 16), sticky="n")
        tk.Label(left, text="CẦN TRÁI  (dọc = throttle · ngang = yaw)",
                 fg="#8a8894", bg="#0a0d0b", font=("Consolas", 9)).pack(pady=(0, 6))
        self.pad = tk.Canvas(left, width=PAD, height=PAD, bg="#0e1210",
                             highlightthickness=1, highlightbackground="#20281f")
        self.pad.pack()
        self.pad.bind("<Button-1>", self._on_press)
        self.pad.bind("<B1-Motion>", self._on_drag)
        self.pad.bind("<ButtonRelease-1>", self._on_release)

        tk.Label(left, text="Kéo núm rồi NHẢ TAY để xem throttle xử lý thế nào.",
                 fg="#6f6d7a", bg="#0a0d0b", font=("Consolas", 8)).pack(pady=(6, 0))

        # thanh ga đứng (cạnh cần)
        self.bar = tk.Canvas(wrap, width=46, height=PAD, bg="#0e1210",
                             highlightthickness=1, highlightbackground="#20281f")
        self.bar.grid(row=0, column=1, padx=(0, 16), sticky="n")

        # bảng điều khiển (phải)
        right = tk.Frame(wrap, bg="#0a0d0b")
        right.grid(row=0, column=2, sticky="n")

        tk.Label(right, text="THUẬT TOÁN THROTTLE", fg="#37e0a0", bg="#0a0d0b",
                 font=("Consolas", 10, "bold")).pack(anchor="w")
        modes = [
            ("rate", "3. TÍCH-PHÂN tốc độ — nhả tay GIỮ ga  (khuyến nghị)"),
            ("abs_center", "1. ABS-GIỮA — nhả tay ga về 0.5 (mid)"),
            ("abs_bottom", "2. ABS-ĐÁY — nửa trên = ga, nhả tay về 0"),
            ("latch", "4. CHỐT-GIỮ — giữ giá trị cuối khi nhả"),
        ]
        for val, txt in modes:
            tk.Radiobutton(right, text=txt, value=val, variable=self.mode,
                           command=self._reset_throttle,
                           fg="#e9e7ee", bg="#0a0d0b", selectcolor="#0e1210",
                           activebackground="#0a0d0b", activeforeground="#37e0a0",
                           font=("Consolas", 9), anchor="w",
                           highlightthickness=0).pack(anchor="w", fill="x")

        rframe = tk.Frame(right, bg="#0a0d0b")
        rframe.pack(anchor="w", pady=(8, 4), fill="x")
        tk.Label(rframe, text="Tốc độ (ga/giây, cho TÍCH-PHÂN):", fg="#8a8894",
                 bg="#0a0d0b", font=("Consolas", 8)).pack(anchor="w")
        tk.Scale(rframe, from_=0.2, to=3.0, resolution=0.1, orient="horizontal",
                 variable=self.rate, length=300, bg="#0a0d0b", fg="#37e0a0",
                 troughcolor="#0e1210", highlightthickness=0,
                 font=("Consolas", 8)).pack(anchor="w")

        tk.Button(right, text="RESET GA VỀ 0", command=self._reset_throttle,
                  bg="#0e1210", fg="#ff6b6b", activebackground="#1a0f0f",
                  font=("Consolas", 9), relief="flat").pack(anchor="w", pady=(0, 8))

        # số liệu
        self.readout = tk.Label(right, text="", fg="#e9e7ee", bg="#0a0d0b",
                                font=("Consolas", 11), justify="left")
        self.readout.pack(anchor="w")

        # đồ thị throttle theo thời gian
        tk.Label(right, text="THROTTLE THEO THỜI GIAN (nhả tay ở mũi tên):",
                 fg="#8a8894", bg="#0a0d0b", font=("Consolas", 8)).pack(
            anchor="w", pady=(8, 2))
        self.graph = tk.Canvas(right, width=GRAPH_W, height=GRAPH_H, bg="#060a09",
                               highlightthickness=1, highlightbackground="#20281f")
        self.graph.pack(anchor="w")

    # ── xử lý chuột ───────────────────────────────────────────────────────────
    def _set_from_mouse(self, ev):
        dx = (ev.x - C) / TRAVEL
        dy = -(ev.y - C) / TRAVEL
        # kẹp trong hình tròn bán kính 1
        mag = (dx * dx + dy * dy) ** 0.5
        if mag > 1.0:
            dx, dy = dx / mag, dy / mag
        self.kx, self.ky = dx, dy

    def _on_press(self, ev):
        self.held = True
        self._set_from_mouse(ev)

    def _on_drag(self, ev):
        if self.held:
            self._set_from_mouse(ev)

    def _on_release(self, _ev):
        self.held = False
        HIST.append((self.throttle, True))  # đánh dấu thời điểm nhả

    def _reset_throttle(self):
        self.throttle = 0.0

    # ── vòng lặp cập nhật ─────────────────────────────────────────────────────
    def _tick(self):
        dt = TICK_MS / 1000.0

        # cần TỰ VỀ GIỮA khi không giữ (cả hai trục)
        if not self.held:
            self.kx *= (1 - SPRING)
            self.ky *= (1 - SPRING)
            if abs(self.kx) < 1e-3:
                self.kx = 0.0
            if abs(self.ky) < 1e-3:
                self.ky = 0.0

        # tính throttle theo thuật toán đã chọn
        m = self.mode.get()
        if m == "abs_center":
            self.throttle = (self.ky + 1) / 2
        elif m == "abs_bottom":
            self.throttle = max(0.0, self.ky)
        elif m == "rate":
            # deadband nhỏ để nhả tay không trôi ga do nhiễu
            y = self.ky if abs(self.ky) > 0.05 else 0.0
            self.throttle += y * self.rate.get() * dt
            self.throttle = min(1.0, max(0.0, self.throttle))
        elif m == "latch":
            if self.held:
                self.throttle = (self.ky + 1) / 2

        if not (HIST and HIST[-1][1]):  # tránh trùng dấu nhả
            HIST.append((self.throttle, False))
        elif not self.held:
            HIST.append((self.throttle, False))

        self._redraw()
        self.root.after(TICK_MS, self._tick)

    # ── vẽ ────────────────────────────────────────────────────────────────────
    def _redraw(self):
        # cần lái
        p = self.pad
        p.delete("all")
        p.create_oval(6, 6, PAD - 6, PAD - 6, outline="#20281f", width=1)
        p.create_line(C, 14, C, PAD - 14, fill="#1a211d")
        p.create_line(14, C, PAD - 14, C, fill="#1a211d")
        kxpx = C + self.kx * TRAVEL
        kypx = C - self.ky * TRAVEL
        col = "#37e0a0" if self.held else "#2aa77a"
        p.create_oval(kxpx - KNOB_R, kypx - KNOB_R, kxpx + KNOB_R, kypx + KNOB_R,
                      fill=col, outline="#0a0d0b", width=2)

        # thanh ga đứng
        b = self.bar
        b.delete("all")
        h = (PAD - 8) * self.throttle
        b.create_rectangle(6, PAD - 4 - h, 40, PAD - 4, fill="#37e0a0", width=0)
        b.create_text(23, 12, text="GA", fill="#8a8894", font=("Consolas", 8))

        # số liệu
        z = round(self.throttle * 1000)
        yaw = round(self.kx * 1000)
        self.readout.config(text=(
            f"stick y  : {self.ky:+.2f}   (lên dương)\n"
            f"stick x  : {self.kx:+.2f}   → yaw {yaw:+d}\n"
            f"throttle : {self.throttle:.3f}\n"
            f"MANUAL_CONTROL.z : {z:4d}  (0..1000)\n"
            f"trạng thái: {'ĐANG GIỮ' if self.held else 'ĐÃ NHẢ (về giữa)'}"
        ))

        # đồ thị
        g = self.graph
        g.delete("all")
        # lưới 0 / 0.5 / 1
        for frac, lbl in ((0.0, "1.0"), (0.5, "0.5"), (1.0, "0.0")):
            y = 6 + (GRAPH_H - 12) * frac
            g.create_line(24, y, GRAPH_W - 4, y, fill="#12201a")
            g.create_text(12, y, text=lbl, fill="#4a5a52", font=("Consolas", 7))
        pts = list(HIST)
        if len(pts) > 1:
            coords = []
            x0 = GRAPH_W - len(pts)
            for i, (val, _rel) in enumerate(pts):
                x = x0 + i
                y = 6 + (GRAPH_H - 12) * (1 - val)
                coords += [x, y]
            g.create_line(*coords, fill="#37e0a0", width=2)
            # đánh dấu các điểm nhả tay
            for i, (val, rel) in enumerate(pts):
                if rel:
                    x = x0 + i
                    y = 6 + (GRAPH_H - 12) * (1 - val)
                    g.create_text(x, y - 8, text="▼", fill="#ffb020",
                                  font=("Consolas", 8))


if __name__ == "__main__":
    root = tk.Tk()
    JoyTester(root)
    root.mainloop()
