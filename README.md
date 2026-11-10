# 🌟 Smart LED WS2812B Controller - ESP8266 & Mobile-First Web App

Proyek firmware & Web App lengkap untuk mengontrol strip LED Addressable **WS2812B** menggunakan mikrokontroler **ESP8266** (NodeMCU V2 / Wemos D1 Mini). Web App dirancang khusus dengan gaya **Cyber-Minimalist Dark Mode** yang optimal dioperasikan dengan satu jempol pada layar smartphone tanpa perlu install aplikasi pihak ketiga.

---

## ⚡ 1. Skema Rangkaian Wiring Hardware

Untuk memastikan strip LED bekerja stabil tanpa jitter, tegangan drop, atau merusak pin data mikrokontroler:

```
[ Power Supply 5V (2A - 5A) ]
   (+) 5V -------------------+-------------------------------+--- (5V / VDD) Strip WS2812B
                             |                               |
                           [ + ] Kapasitor                 [VIN / 5V]
                           [1000µF 16V]                    ESP8266
                           [ - ] Elektrolit                (NodeMCU / Wemos)
                             |                               |
   (-) GND ------------------+-------------------------------+--- (GND) Strip WS2812B
                             |                               |
                             +-------------------------------+--- (GND) ESP8266
                                                                (COMMON GROUND)

   ESP8266 PIN D4 (GPIO2) -----[ Resistor 330Ω ]-------------> (DIN) Strip WS2812B
```

### Tabel Sambungan Pin

| Komponen Sumber | Pin / Jalur | Menuju Komponen | Keterangan |
| :--- | :--- | :--- | :--- |
| **Power Supply 5V** | **+5V** | Strip LED (5V/VDD) & VIN ESP8266 | Berikan arus sesuai panjang strip LED |
| **Power Supply 5V** | **GND** | Strip LED (GND) & GND ESP8266 | **Wajib Common Ground!** |
| **Kapasitor Elektrolit** | **1000µF 16V** | Paralel antara +5V dan GND | Mencegah lonjakan arus (surge) saat power-on |
| **ESP8266** | **D4 (GPIO2)** | Resistor 330Ω $\rightarrow$ DIN Strip | Meredam pantulan sinyal frekuensi tinggi |
| **Resistor** | **330Ω - 470Ω** | Antara Pin D4 dan DIN Strip LED | Melindungi LED pertama dari spike tegangan |

> [!IMPORTANT]
> **Common Ground Wajib Terhubung:** Hubungkan kabel `GND` dari power supply 5V ke `GND` ESP8266 dan `GND` strip LED. Jika tidak, data WS2812B akan noise dan warna LED akan berkedip acak.

---

## 🛠️ 2. Struktur Proyek

```
smart-led-controller/
├── platformio.ini               # Konfigurasi PlatformIO & dependency
├── include/
│   ├── config_manager.h         # Persistensi data LittleFS + Debounced save
│   ├── led_effects.h            # Engine 21+ efek animasi FastLED
│   ├── ntp_manager.h            # Sinkronisasi jam internet NTP (UTC+7 / WIB)
│   └── web_server.h             # AsyncWebServer + AsyncWebSocket + ElegantOTA
├── src/
│   └── main.cpp                 # Entry point & task scheduling
├── data/                        # Filesystem LittleFS (Web App Frontend)
│   ├── index.html               # UI Mobile-First Responsive
│   ├── style.css                # Glassmorphism Cyber-Dark theme
│   └── app.js                   # WebSocket client, Canvas Color Wheel, Logic
└── README.md                    # Dokumentasi lengkap
```

---

## 🚀 3. Panduan Kompilasi & Flash (PlatformIO)

Pastikan mikrokontroler ESP8266 telah terhubung ke laptop/PC melalui kabel data Micro USB.

### Langkah A: Upload Web App Frontend ke LittleFS
File `index.html`, `style.css`, dan `app.js` disimpan di flash memory LittleFS:

1. **Build Filesystem Image:**
   ```bash
   pio run --target buildfs
   ```
2. **Upload Filesystem Image ke ESP8266:**
   ```bash
   pio run --target uploadfs
   ```

### Langkah B: Kompilasi & Upload Firmware
1. **Kompilasi Firmware (Build):**
   ```bash
   pio run
   ```
2. **Upload Firmware ke ESP8266:**
   ```bash
   pio run --target upload
   ```
3. **Membuka Serial Monitor (115200 baud):**
   ```bash
   pio device monitor -b 115200
   ```

*(Opsional untuk board Wemos D1 Mini, tambahkan flag environment: `-e d1_mini`)*

---

## 📱 4. Cara Penggunaan Pertama Kali (Koneksi & Kontrol)

1. **WiFi Captive Portal:**
   - Saat pertama kali dinyalakan, ESP8266 akan membuka Access Point bernama:
     `SmartLED-Setup` (tanpa password).
   - Hubungkan HP kamu ke WiFi `SmartLED-Setup`. Browser HP akan otomatis terbuka (Captive Portal).
   - Pilih jaringan WiFi rumah/kamar kamu, masukkan password, lalu klik **Save**.
2. **Mengakses Web App:**
   - Setelah tersambung, buka browser di HP dan ketik:
     `http://smartled.local`
     *(atau buka alamat IP ESP8266 yang tampil di Serial Monitor / router).*
3. **Kontrol Real-Time (Zero-Latency):**
   - Perubahan warna, kecerahan, dan efek animasi tersinkronisasi seketika di semua HP yang membuka web tersebut via WebSocket.

---

## ✨ 5. Fitur Utama Web App

1. **Cyber-Minimalist Dark Mode:**
   - Palet warna gelap `#090d16` dengan glassmorphism blur.
   - Glow aksen neon otomatis beradaptasi mengikuti warna LED yang sedang aktif.
2. **Kontrol Utama Ergonomis:**
   - Tombol Power bulat besar dengan animasi denyut glow.
   - Interactive Color Wheel berbasis **HTML5 Canvas** responsif sentuhan.
   - Slider kecerahan (0–100%) dan Color Temperature (2700K Warm – 6500K Cool).
   - 6 Quick Presets: *Warm Relax, Cool Focus, Cyberpunk, Sunset, Night Red, Aurora*.
3. **21 Pilihan Efek Animasi:**
   - *Solid, Rainbow, Rainbow Wave, Breathing, Comet/Meteor, Fire 2012, Twinkle/Sparkle, Police Strobe, Color Wipe, Theater Chase, Gradient Flow, Pacman Chaser, Sinelon, Juggle, Matrix Rain, Strobe Flash, BPM Pulse, Larson Scanner, Ripple, Heartbeat, Confetti*.
   - Slider **Kecepatan** dan **Intensitas / Trail**.
4. **Pembuat Gradasi Kustom:**
   - Pilih 2 hingga 4 titik warna bebas. Gradasi halus langsung dihitung dan dipetakan ke seluruh strip LED.
5. **Jadwal & Timer Cerdas:**
   - **Sleep Timer:** Pilihan cepat 15m, 30m, 45m, 60m dengan opsi *Gradual Dim* (meredup perlahan sebelum mati otomatis).
   - **Daily Schedule:** Menyalakan dan mematikan LED secara otomatis setiap hari berdasarkan waktu presisi NTP server (WIB / UTC+7).
6. **Hardware & Power Management:**
   - Ubah jumlah titik LED (1–300) secara dinamis tanpa kompilasi ulang.
   - Batas arus dinamis (`FastLED.setMaxPowerInVoltsAndMilliamps`) untuk proteksi adaptor daya.
   - Indikator diagnostik sistem: Alamat IP, Free Heap RAM, Uptime, WiFi RSSI dBm.
   - Tombol **Reset WiFi** dan link langsung ke **OTA Firmware Update** (`/update`).

---

## 🔄 6. Update Firmware Lewat Browser (OTA Update)

Kamu dapat memperbarui firmware atau filesystem LittleFS di masa mendatang tanpa perlu menyambungkan kabel USB:
1. Akses halaman: `http://smartled.local/update`
2. Pilih file firmware (`.bin`) dari folder `.pio/build/nodemcuv2/firmware.bin` atau filesystem (`littlefs.bin`).
3. Klik tombol Update. ESP8266 akan reboot otomatis dengan firmware baru.
