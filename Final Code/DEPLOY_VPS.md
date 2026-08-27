# GreenFlow — Panduan Install di VPS (Ubuntu/Debian)

Asumsi: VPS baru, akses root/sudo, Ubuntu 22.04+ (langkah sama untuk Debian).

## 1. Update sistem & install dependency dasar

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y python3 python3-pip python3-venv git build-essential
```

## 2. Install & konfigurasi Mosquitto (MQTT broker)

```bash
sudo apt install -y mosquitto mosquitto-clients
```

Edit konfigurasi supaya bisa terima gambar JPEG (default limit terlalu kecil) dan
bisa diakses dari luar (ESP32 connect ke sini):

```bash
sudo nano /etc/mosquitto/conf.d/greenflow.conf
```

Isi file itu dengan:

```
listener 1883 0.0.0.0
max_packet_size 100000
allow_anonymous false
password_file /etc/mosquitto/passwd
```

Buat user MQTT (dipakai server Flask & kedua ESP32 nanti):

```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd greenflow
# masukkan password saat diminta
sudo systemctl restart mosquitto
sudo systemctl enable mosquitto
```

Cek broker jalan:
```bash
sudo systemctl status mosquitto
```

> `allow_anonymous false` artinya wajib pakai username/password — ini penting karena
> broker akan terekspos ke internet. Kalau mau buka port 1883 ke publik, pastikan
> juga firewall (langkah 6) hanya izinkan IP yang perlu, atau pakai VPN/WireGuard
> antara ESP32 dan VPS kalau bisa.

## 3. Install MongoDB

```bash
curl -fsSL https://pgp.mongodb.com/server-7.0.asc | sudo gpg -o /usr/share/keyrings/mongodb-server-7.0.gpg --dearmor
echo "deb [ signed-by=/usr/share/keyrings/mongodb-server-7.0.gpg arch=amd64] https://repo.mongodb.org/apt/ubuntu jammy/mongodb-org/7.0 multiverse" | sudo tee /etc/apt/sources.list.d/mongodb-org-7.0.list
sudo apt update
sudo apt install -y mongodb-org
sudo systemctl start mongod
sudo systemctl enable mongod
```

(Kalau Ubuntu versi lain, ganti `jammy` sesuai codename OS-nya, atau langsung pakai
MongoDB Atlas gratis dan skip langkah ini — tinggal isi `GF_MONGO_URI` ke connection
string Atlas-nya.)

## 4. Deploy kode GreenFlow

Upload folder `greenflow/` ke VPS (via `scp`, `git`, atau upload manual), lalu:

```bash
cd ~/greenflow/server
python3 -m venv venv
source venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt
```

Siapkan model YOLOv8n ncnn (kalau belum ada foldernya di VPS):
```bash
pip install ultralytics
yolo export model=yolov8n.pt format=ncnn
# taruh folder yolov8n_ncnn_model/ di dalam server/
```

## 5. Set environment variable & test jalan manual dulu

```bash
export GF_MQTT_BROKER=localhost
export GF_MQTT_USER=greenflow
export GF_MQTT_PASS=password_yang_tadi_dibuat
export GF_MONGO_URI=mongodb://localhost:27017
python app.py
```

Test dari terminal lain:
```bash
curl http://localhost:5000/health
```

Kalau sudah OK, tekan Ctrl+C, lanjut ke langkah 6 supaya jalan permanen (auto-restart,
auto-start saat reboot).

## 6. Jalankan sebagai systemd service (biar tetap hidup 24 jam)

```bash
sudo nano /etc/systemd/system/greenflow.service
```

Isi:
```ini
[Unit]
Description=GreenFlow Flask + MQTT server
After=network.target mosquitto.service mongod.service

[Service]
User=root
WorkingDirectory=/root/greenflow/server
Environment="GF_MQTT_BROKER=localhost"
Environment="GF_MQTT_USER=greenflow"
Environment="GF_MQTT_PASS=password_yang_tadi_dibuat"
Environment="GF_MONGO_URI=mongodb://localhost:27017"
ExecStart=/root/greenflow/server/venv/bin/python /root/greenflow/server/app.py
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```
(Sesuaikan `WorkingDirectory`/path venv kalau folder atau user-nya beda.)

Aktifkan:
```bash
sudo systemctl daemon-reload
sudo systemctl enable greenflow
sudo systemctl start greenflow
sudo systemctl status greenflow
```

Lihat log kalau ada error:
```bash
sudo journalctl -u greenflow -f
```

## 7. Buka firewall (kalau pakai ufw)

```bash
sudo ufw allow 1883/tcp   # MQTT — dari ESP32-CAM & ESP32-C6
sudo ufw allow 5000/tcp   # Flask API (opsional, kalau mau diakses dari luar)
sudo ufw enable
sudo ufw status
```

## 8. Update firmware ESP32 dengan alamat & kredensial VPS

Di kedua file `.ino`, isi:
```cpp
const char* MQTT_BROKER = "IP_VPS_KAMU";
const char* MQTT_USER   = "greenflow";
const char* MQTT_PASS   = "password_yang_tadi_dibuat";
```

## Checklist cepat kalau nanti bermasalah

- `sudo systemctl status mosquitto mongod greenflow` — pastikan ketiganya `active (running)`
- `mosquitto_sub -h localhost -u greenflow -P <password> -t 'greenflow/#' -v` — pantau semua topic langsung dari VPS untuk cek data ESP32 benar-benar masuk
- `curl http://IP_VPS:5000/detections/recent` — cek hasil deteksi sudah tersimpan
- Kalau frame gambar gagal terkirim dari ESP32-CAM, cek lagi `max_packet_size` di Mosquitto dan `setBufferSize()` di firmware sudah cukup besar
