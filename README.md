# SmartFan IoT - Kelompok 3

SmartFan IoT adalah proyek kipas pintar berbasis ESP32 yang dapat dikontrol melalui Blynk dan sensor suara. Sistem ini dilengkapi monitoring listrik, tampilan OLED, RTC untuk waktu real-time, serta penyimpanan data ke EEPROM agar timer dan statistik tetap tersimpan.

## Fitur

- Kontrol kipas melalui aplikasi Blynk.
- Kontrol otomatis dengan sensor suara/tepukan.
- Timer penggunaan kipas per sesi.
- Batas kuota harian 8 jam.
- Monitoring tegangan, arus, daya, energi, frekuensi, dan power factor menggunakan PZEM-004T.
- Tampilan informasi pada OLED SSD1306 128x64.
- Sinkronisasi waktu menggunakan NTP dan RTC DS3231.
- Penyimpanan data timer, kuota, runtime, dan jumlah tepukan ke EEPROM eksternal.
- Statistik harian penggunaan kipas.

## Komponen

- ESP32 DevKit
- Relay module
- Sensor suara
- OLED SSD1306 128x64 I2C
- RTC DS3231
- PZEM-004T v3.0
- EEPROM eksternal I2C
- Kipas/beban listrik
- Kabel jumper dan power supply

## Pin ESP32

| Komponen | Pin ESP32 |
| --- | --- |
| Relay | GPIO 18 |
| Sensor suara | GPIO 34 |
| OLED SDA | GPIO 21 |
| OLED SCL | GPIO 22 |
| RTC SDA | GPIO 21 |
| RTC SCL | GPIO 22 |
| EEPROM SDA | GPIO 21 |
| EEPROM SCL | GPIO 22 |
| PZEM RX/TX | GPIO 16 / GPIO 17 |

## Virtual Pin Blynk

| Virtual Pin | Fungsi |
| --- | --- |
| V0 | Voltage |
| V1 | Current |
| V2 | Power |
| V3 | Energy |
| V4 | Frequency |
| V5 | Power Factor |
| V6 | Tombol tambah 1 jam |
| V7 | Level suara |
| V8 | Jumlah tepukan hari ini |
| V9 | Timer sesi |
| V10 | Status kipas |
| V11 | Sisa kuota harian |
| V12 | Runtime hari ini |

## Cara Menjalankan

1. Clone repository ini.

   ```bash
   git clone https://github.com/anantafikri/SmartFan-IoT-Kelompok-3.git
   ```

2. Buka folder proyek menggunakan VS Code + PlatformIO.

3. Sesuaikan konfigurasi WiFi dan Blynk di `src/main.cpp`.

   ```cpp
   #define BLYNK_TEMPLATE_ID "..."
   #define BLYNK_TEMPLATE_NAME "..."
   #define BLYNK_AUTH_TOKEN "..."

   char ssid[] = "...";
   char pass[] = "...";
   ```

4. Build proyek.

   ```bash
   pio run
   ```

5. Upload ke ESP32.

   ```bash
   pio run --target upload
   ```

6. Buka Serial Monitor.

   ```bash
   pio device monitor
   ```

## Library

Library yang digunakan sudah didefinisikan di `platformio.ini`.

- Adafruit SSD1306
- Adafruit GFX Library
- PZEM-004T-v30
- Blynk
- RTClib

## Struktur Folder

```text
.
+-- include/
+-- lib/
+-- src/
|   +-- main.cpp
+-- test/
+-- platformio.ini
+-- README.md
```

## Catatan

- Pastikan koneksi WiFi aktif agar Blynk dan sinkronisasi NTP dapat berjalan.
- Pastikan alamat I2C OLED adalah `0x3C`.
- EEPROM eksternal pada kode menggunakan alamat I2C `0x57`.
- Hati-hati saat menghubungkan PZEM dan beban AC. Pastikan wiring aman dan sesuai standar kelistrikan.
