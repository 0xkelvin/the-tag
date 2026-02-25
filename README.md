# the-tag
<img width="679" height="653" alt="image" src="https://github.com/user-attachments/assets/c280b38f-db3c-409a-aaa3-c54f0d905b22" />


# Components: 
- [Seeed Studio XIAO nRF54L15 Sense](https://www.seeedstudio.com/Seeed-Studio-XIAO-nRF54L15-Sense-Pre-Soldered-p-6500.html?srsltid=AfmBOoqPTqsxPYSpyJ1X8-ABDJ6ZhmzmzD8qlfOUBaC0Au0bjOC8lFFK)
- [ePaper Breakout Board for Seeed Studio XIAO](https://www.seeedstudio.com/ePaper-Breakout-Board-p-5804.html?srsltid=AfmBOoqj34irMFfLHrAsJpeER5vtpA0sM0G_N_hMMq1ya_562vqUIcqu)
- [E-ink 1,54 inch Shopee 4 colors](https://shopee.vn/%F0%9F%94%A5M%C3%A0n-h%C3%ACnh-E-Paper-M%C3%A0n-h%C3%ACnh-E-Ink-1.54-''-M%C3%A0n-h%C3%ACnh-E-Paper-152x152-M%C3%A0n-h%C3%ACnh-E-Ink-m%C3%A0u-Mini-i.127928025.41873920173)
- Or [E-ink 1,54 inch from proe 4 colors](https://www.proe.vn/1-64inch-square-e-paper-g-raw-display-168-168-red-yellow-black-white)  


# Hardware Connection for Developer version:
<img width="543" height="164" alt="image" src="https://github.com/user-attachments/assets/2be2b3ba-df4d-4b64-a039-8cb74c12c2f6" />
<img width="699" height="291" alt="image" src="https://github.com/user-attachments/assets/857440ce-5541-45d5-8d43-4ffeb70f7e5b" />


# Share Folder 
- [Google Driver project folder](https://drive.google.com/drive/folders/1gwg72AfddP2lZDSd2DLhJ3Zl0bu9NLsG?usp=drive_link)


# the-tag
<img width="679" height="653" alt="image" src="https://github.com/user-attachments/assets/c280b38f-db3c-409a-aaa3-c54f0d905b22" />


# Components: 
- [Seeed Studio XIAO nRF54L15 Sense](https://www.seeedstudio.com/Seeed-Studio-XIAO-nRF54L15-Sense-Pre-Soldered-p-6500.html?srsltid=AfmBOoqPTqsxPYSpyJ1X8-ABDJ6ZhmzmzD8qlfOUBaC0Au0bjOC8lFFK)
- [ePaper Breakout Board for Seeed Studio XIAO](https://www.seeedstudio.com/ePaper-Breakout-Board-p-5804.html?srsltid=AfmBOoqj34irMFfLHrAsJpeER5vtpA0sM0G_N_hMMq1ya_562vqUIcqu)
- [E-ink 1,54 inch Shopee 4 colors](https://shopee.vn/%F0%9F%94%A5M%C3%A0n-h%C3%ACnh-E-Paper-M%C3%A0n-h%C3%ACnh-E-Ink-1.54-''-M%C3%A0n-h%C3%ACnh-E-Paper-152x152-M%C3%A0n-h%C3%ACnh-E-Ink-m%C3%A0u-Mini-i.127928025.41873920173)
- Or [E-ink 1,54 inch from proe 4 colors](https://www.proe.vn/1-64inch-square-e-paper-g-raw-display-168-168-red-yellow-black-white)  


# Hardware Connection for Developer version:
<img width="543" height="164" alt="image" src="https://github.com/user-attachments/assets/2be2b3ba-df4d-4b64-a039-8cb74c12c2f6" />
<img width="699" height="291" alt="image" src="https://github.com/user-attachments/assets/857440ce-5541-45d5-8d43-4ffeb70f7e5b" />


# Share Folder 
- [Google Driver project folder](https://drive.google.com/drive/folders/1gwg72AfddP2lZDSd2DLhJ3Zl0bu9NLsG?usp=drive_link)


# 🚀 Getting Started

This project uses **West** (Zephyr’s meta-tool) to manage the workspace and modules.

Follow the steps below to initialize, build, and flash the firmware.

---

## 📦 1️⃣ Initialize the Workspace

Initialize the workspace directly from this repository:

```bash
west init -m git@github.com:bluleap-ai/the-tag.git --mr main etag
cd etag
west update
```

## 🛠️ 2️⃣ Build the Firmware

Build for Seeed XIAO nRF54L15 (CPUAPP core) using sysbuild:

```bash
west build -b xiao_nrf54l15/nrf54l15/cpuapp the-tag --sysbuild -p
```

## 🔥 3️⃣ Flash the Device

After a successful build: Make sure your board is connected and in programming mode.
```bash
west flash
```

