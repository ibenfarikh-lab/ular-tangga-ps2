# Kanvas Pengaturan V7: 5 baris (GETAR STIK, KECEPATAN, MUSIK, SUARA,
# KEMBALI). Panel (110,96)-(530,352); pusat baris y = 182,216,250,284,318;
# label x=140 digambar di kanvas, nilai digambar kode di x=420.
# Menghasilkan set.png + set_data.h (hanya set_pixels, format lama).
from PIL import Image, ImageDraw, ImageFont, ImageEnhance

FB = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
def font(sz): return ImageFont.truetype(FB, sz)

DIR = "/home/hatch/workspace/ps2-matrix/ular-judul-game"
base = Image.open("/home/hatch/workspace/ps2-matrix/ular-judul/judul-kanvas-baru.png").convert("RGB")
base = ImageEnhance.Brightness(base).enhance(0.5)
img = base.copy()
d = ImageDraw.Draw(img)
d.rounded_rectangle([114, 100, 534, 356], 18, fill=(0, 0, 0))
d.rounded_rectangle([110, 96, 530, 352], 18, fill=(8, 18, 44), outline=(64, 156, 255), width=4)
d.text((320, 124), "PENGATURAN", font=font(28), fill=(240, 246, 255), anchor="mm")
d.line([140, 148, 500, 148], fill=(64, 156, 255), width=2)
d.text((320, 374), "KIRI/KANAN: UBAH    X: PILIH    O: KEMBALI",
       font=font(14), fill=(170, 190, 220), anchor="mm")
img.save(DIR + "/set.png")

px = img.load()
vals = []
for y in range(448):
    for x in range(640):
        r, g, b = px[x, y]
        vals.append(f"{r},{g},{b},128")
with open(DIR + "/set_data.h", "w") as f:
    f.write("#ifndef SET_DATA_H\n#define SET_DATA_H\n")
    f.write("static const unsigned char set_pixels[640*448*4] = {\n")
    for i in range(0, len(vals), 16):
        f.write(",".join(vals[i:i + 16]) + ",\n")
    f.write("};\n#endif\n")
print("set.png + set_data.h (5 baris) selesai")
