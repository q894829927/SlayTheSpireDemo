from __future__ import annotations

from PIL import Image, ImageDraw, ImageFilter
import math
import os
import random
import shutil
import zipfile

ROOT = os.path.join("docs", "UIAssets", "SlayTheSpireDemo_UI_MissingAssets")
ZIP_PATH = os.path.join("docs", "UIAssets", "SlayTheSpireDemo_UI_MissingAssets.zip")

BLACK = (13, 13, 16, 255)
BLACK2 = (25, 25, 30, 255)
IVORY = (238, 232, 218, 255)
RED = (205, 30, 42, 255)
RED_DARK = (112, 15, 24, 255)
CYAN = (25, 211, 226, 255)
GREEN = (130, 220, 78, 255)
PURPLE = (150, 75, 225, 255)

random.seed(12)


def save(im: Image.Image, name: str) -> None:
    os.makedirs(ROOT, exist_ok=True)
    im.save(os.path.join(ROOT, name), optimize=True)


def distress_alpha(im: Image.Image, amount: float = 0.02, margin: int = 16) -> Image.Image:
    a = im.getchannel("A")
    d = ImageDraw.Draw(a)
    pix = a.load()
    w, h = im.size
    for _ in range(int(w * h * amount)):
        x = random.randrange(w)
        y = random.randrange(h)
        if pix[x, y] > 40 and (
            x < margin * 3 or x > w - margin * 3 or y < margin * 3 or y > h - margin * 3 or random.random() < 0.12
        ):
            r = random.choice([1, 1, 1, 2, 2, 3])
            d.ellipse((x - r, y - r, x + r, y + r), fill=0)
    im.putalpha(a)
    return im


def skew_panel(size, fill=BLACK, border=IVORY, border_w=6, accent=None):
    w, h = size
    im = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    pts = [(26, 18), (w - 8, 8), (w - 28, h - 16), (8, h - 4)]
    d.polygon(pts, fill=fill)
    d.line(pts + [pts[0]], fill=border, width=border_w, joint="curve")
    if accent:
        d.polygon([(18, h - 42), (int(w * 0.35), h - 60), (int(w * 0.31), h - 20), (12, h - 8)], fill=accent)
        d.polygon([(w - 76, 16), (w - 18, 10), (w - 34, 48)], fill=accent)
    return distress_alpha(im, 0.008)


def card_frame(color, invalid=False):
    w, h = 512, 768
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    outer = [(32, 12), (480, 26), (500, 690), (452, 752), (52, 742), (12, 78)]
    inner = [(58, 54), (447, 64), (462, 674), (425, 710), (82, 704), (52, 98)]
    d.line(outer + [outer[0]], fill=color, width=18, joint="curve")
    d.line(inner + [inner[0]], fill=IVORY, width=5, joint="curve")
    d.polygon([(24, 130), (5, 220), (46, 188)], fill=color)
    d.polygon([(472, 520), (508, 602), (466, 578)], fill=color)
    if invalid:
        d.line([(92, 120), (420, 640)], fill=RED, width=18)
        d.line([(420, 120), (96, 638)], fill=RED, width=18)
    return distress_alpha(im, 0.006)


def reticle(color):
    s = 512
    c = s // 2
    im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for r, width in [(188, 14), (132, 8), (62, 10)]:
        d.ellipse((c - r, c - r, c + r, c + r), outline=color, width=width)
    for ang in [0, 90, 180, 270]:
        a = math.radians(ang)
        x1, y1 = c + math.cos(a) * 206, c + math.sin(a) * 206
        x2, y2 = c + math.cos(a) * 250, c + math.sin(a) * 250
        d.line((x1, y1, x2, y2), fill=color, width=16)
    for ang in [45, 135, 225, 315]:
        a = math.radians(ang)
        p1 = (c + math.cos(a) * 154, c + math.sin(a) * 154)
        p2 = (c + math.cos(a - 0.16) * 204, c + math.sin(a - 0.16) * 204)
        p3 = (c + math.cos(a + 0.16) * 204, c + math.sin(a + 0.16) * 204)
        d.polygon([p1, p2, p3], fill=color)
    d.ellipse((c - 18, c - 18, c + 18, c + 18), fill=color)
    return distress_alpha(im, 0.004)


def burst(color, rays=18, inner=58, outer1=150, outer2=240):
    s = 512
    c = s // 2
    im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    pts = []
    for i in range(rays * 2):
        a = 2 * math.pi * i / (rays * 2)
        r = outer1 if i % 2 else outer2 * random.uniform(0.72, 1.0)
        pts.append((c + math.cos(a) * r, c + math.sin(a) * r))
    d.polygon(pts, fill=color)
    d.ellipse((c - inner, c - inner, c + inner, c + inner), fill=IVORY)
    return distress_alpha(im, 0.01)


def icon_canvas():
    return Image.new("RGBA", (256, 256), (0, 0, 0, 0))


def mask_canvas():
    return Image.new("L", (512, 512), 0)


def main():
    shutil.rmtree(ROOT, ignore_errors=True)
    os.makedirs(ROOT, exist_ok=True)

    save(card_frame(IVORY), "CardFrame_Focus_Ivory.png")
    save(card_frame(CYAN), "CardFrame_Hover_Cyan.png")
    save(card_frame(RED, True), "CardFrame_Invalid_Red.png")

    save(reticle(RED), "TargetReticle_Red.png")
    save(reticle(CYAN), "TargetReticle_Cyan.png")

    im = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.polygon([(50, 256), (348, 72), (330, 190), (474, 256), (330, 322), (348, 440)], fill=RED)
    d.polygon([(94, 256), (324, 126), (306, 214), (410, 256), (306, 298), (324, 386)], fill=IVORY)
    save(distress_alpha(im, 0.005), "TargetArrow_Head.png")

    im = Image.new("RGBA", (1024, 160), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.polygon([(0, 64), (940, 38), (1024, 80), (940, 122), (0, 96)], fill=RED)
    d.polygon([(0, 72), (930, 56), (982, 80), (930, 104), (0, 88)], fill=IVORY)
    save(distress_alpha(im, 0.003), "TargetArrow_Body.png")

    save(reticle(CYAN).filter(ImageFilter.GaussianBlur(1.6)), "SelectionPulse_Cyan.png")

    save(burst(CYAN, 16), "FX_BlockImpact_Cyan.png")
    save(burst(IVORY, 14), "FX_CriticalBurst_Ivory.png")
    save(burst(RED, 20), "FX_DamageBurst_Red.png")
    save(burst(GREEN, 12), "FX_HealBurst_Green.png")
    save(burst(PURPLE, 12), "FX_StatusApply_Purple.png")

    im = Image.new("RGBA", (1024, 256), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for off, col in [(0, RED), (18, IVORY), (44, RED)]:
        d.polygon([(20, 150 - off), (790, 40 + off // 2), (1000, 70 + off), (820, 120 + off), (70, 210 - off // 3)], fill=col)
    save(distress_alpha(im, 0.008), "FX_SlashTrail_Red.png")

    save(skew_panel((1024, 260), BLACK, IVORY, 6, RED), "Panel_TurnBanner_Blank.png")
    save(skew_panel((1200, 560), BLACK2, IVORY, 7, RED_DARK), "Panel_Terminal_Blank.png")
    save(skew_panel((640, 112), BLACK, IVORY, 4, RED), "Panel_TooltipRow_Blank.png")
    save(skew_panel((320, 96), BLACK, IVORY, 4, CYAN), "Panel_TabHeader_Blank.png")
    save(skew_panel((210, 72), BLACK, IVORY, 4, RED), "Chip_Overflow_Blank.png")

    for square, name in [(False, "Prompt_Gamepad_Blank.png"), (True, "Prompt_Keyboard_Blank.png")]:
        s = 256
        im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
        d = ImageDraw.Draw(im)
        if square:
            d.rounded_rectangle((38, 38, 218, 218), radius=34, fill=BLACK, outline=IVORY, width=10)
            d.rounded_rectangle((55, 55, 201, 201), radius=24, outline=RED, width=7)
        else:
            d.ellipse((34, 34, 222, 222), fill=BLACK, outline=IVORY, width=10)
            d.ellipse((54, 54, 202, 202), outline=RED, width=7)
        save(distress_alpha(im, 0.004), name)

    im = icon_canvas(); d = ImageDraw.Draw(im)
    for o in [0, 18, 36]: d.polygon([(58 + o, 48 - o), (160 + o, 48 - o), (182 + o, 190 - o), (78 + o, 190 - o)], fill=IVORY, outline=BLACK, width=5)
    save(im, "Icon_DrawPile.png")

    im = icon_canvas(); d = ImageDraw.Draw(im)
    for o in [0, 18, 36]: d.polygon([(52 + o, 62 - o), (162 + o, 42 - o), (188 + o, 176 - o), (78 + o, 198 - o)], fill=BLACK2, outline=IVORY, width=8)
    save(im, "Icon_DiscardPile.png")

    im = icon_canvas(); d = ImageDraw.Draw(im)
    flame = [(128, 30), (168, 84), (158, 113), (192, 130), (172, 204), (128, 230), (82, 204), (62, 130), (98, 106), (92, 72)]
    d.polygon(flame, fill=RED)
    d.polygon([(128, 78), (146, 118), (135, 140), (155, 154), (145, 190), (128, 202), (111, 190), (101, 154), (120, 140), (110, 118)], fill=IVORY)
    save(im, "Icon_ExhaustPile.png")

    im = icon_canvas(); d = ImageDraw.Draw(im)
    d.polygon([(128, 220), (46, 135), (52, 77), (94, 48), (128, 70), (162, 48), (204, 77), (210, 135)], fill=RED)
    save(im, "Icon_HP.png")

    im = icon_canvas(); d = ImageDraw.Draw(im)
    d.polygon([(128, 24), (206, 60), (190, 168), (128, 228), (66, 168), (50, 60)], fill=CYAN)
    d.polygon([(128, 54), (174, 76), (162, 148), (128, 184), (94, 148), (82, 76)], fill=BLACK)
    save(im, "Icon_Block.png")

    im = icon_canvas(); d = ImageDraw.Draw(im)
    d.ellipse((38, 38, 218, 218), outline=CYAN, width=24)
    d.polygon([(145, 32), (92, 128), (132, 126), (106, 224), (176, 110), (136, 112)], fill=IVORY)
    save(im, "Icon_Energy.png")

    m = mask_canvas(); d = ImageDraw.Draw(m)
    for k in range(10):
        y = 125 + k * 18 + random.randint(-8, 8)
        d.polygon([(20, y), (450, y - 80 + random.randint(-10, 10)), (500, y - 40), (80, y + 70)], fill=random.randint(150, 255))
    m = m.filter(ImageFilter.GaussianBlur(1))
    rgba = Image.new("RGBA", m.size, (255, 255, 255, 0)); rgba.putalpha(m); save(rgba, "Mask_BrushSlash.png")

    m = mask_canvas(); d = ImageDraw.Draw(m)
    for y in range(16, 496, 18):
        for x in range(16, 496, 18):
            t = x / 512
            r = max(1, int(7 * (1 - t)))
            if random.random() < 0.95: d.ellipse((x-r, y-r, x+r, y+r), fill=int(255 * (1 - t * 0.25)))
    rgba = Image.new("RGBA", m.size, (255, 255, 255, 0)); rgba.putalpha(m); save(rgba, "Mask_HalftoneGradient.png")

    m = mask_canvas(); d = ImageDraw.Draw(m)
    edge = [(180 + random.randint(-42, 42), y) for y in range(0, 513, 14)]
    d.polygon([(0, 0)] + edge + [(0, 512)], fill=255)
    rgba = Image.new("RGBA", m.size, (255, 255, 255, 0)); rgba.putalpha(m); save(rgba, "Mask_TornEdge.png")

    m = Image.new("L", (512, 512), 0); px = m.load()
    for y in range(512):
        for x in range(512):
            dx = (x - 256) / 256; dy = (y - 256) / 256
            v = max(0, min(1, 1 - (dx * dx + dy * dy)))
            px[x, y] = int(255 * (v ** 1.7))
    rgba = Image.new("RGBA", m.size, (255, 255, 255, 0)); rgba.putalpha(m); save(rgba, "Mask_Vignette.png")

    m = mask_canvas(); d = ImageDraw.Draw(m); d.ellipse((44, 44, 468, 468), outline=255, width=52)
    rgba = Image.new("RGBA", m.size, (255, 255, 255, 0)); rgba.putalpha(m); save(rgba, "Mask_RadialRing.png")

    m = mask_canvas(); d = ImageDraw.Draw(m)
    for _ in range(170):
        x = random.randint(0, 500); y = random.randint(0, 500); ln = random.randint(10, 100)
        d.line((x, y, min(511, x + ln), max(0, y - random.randint(0, 18))), fill=random.randint(80, 220), width=random.choice([1, 1, 2]))
    rgba = Image.new("RGBA", m.size, (255, 255, 255, 0)); rgba.putalpha(m); save(rgba, "Mask_Scratches.png")

    m = mask_canvas(); d = ImageDraw.Draw(m)
    for _ in range(45):
        x = random.randint(30, 482); y = random.randint(80, 430); r = random.randint(25, 80)
        d.ellipse((x-r, y-r, x+r, y+r), fill=random.randint(30, 100))
    m = m.filter(ImageFilter.GaussianBlur(22))
    rgba = Image.new("RGBA", m.size, (255, 255, 255, 0)); rgba.putalpha(m); save(rgba, "Mask_Smoke.png")

    pngs = sorted(f for f in os.listdir(ROOT) if f.lower().endswith(".png"))
    if len(pngs) != 34:
        raise RuntimeError(f"Expected 34 PNG assets, generated {len(pngs)}")

    os.makedirs(os.path.dirname(ZIP_PATH), exist_ok=True)
    with zipfile.ZipFile(ZIP_PATH, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name in pngs:
            z.write(os.path.join(ROOT, name), arcname=name)

    with zipfile.ZipFile(ZIP_PATH, "r") as z:
        if z.testzip() is not None or len(z.namelist()) != 34:
            raise RuntimeError("ZIP integrity check failed")

    print(f"Generated {len(pngs)} PNGs and valid ZIP: {ZIP_PATH} ({os.path.getsize(ZIP_PATH)} bytes)")


if __name__ == "__main__":
    main()
