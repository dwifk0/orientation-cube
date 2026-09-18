# ESP32 Tabanlı 6-Eksenli IMU ile Gerçek Zamanlı 3B Küp Görselleştirme

**Ders:** Elektrik Elektronik Ölçmeleri — Dönem Sonu Projesi
**Hazırlayan:** Ahmet Efe Nezli

> **Not:** Bu, dersin teslim edilen proje raporudur içeriği değiştirilmeden korunmuştur.
> §6 ve §8'deki kod ve parametreler **teslim edilen sürüme** aittir. Depodaki
> güncel kod [`firmware/`](../firmware/orientation_cube) altındadır; aradaki farklar,
> özellikle jiroskop ölçeği düzeltmesi, [`DEGISIKLIKLER.md`](DEGISIKLIKLER.md)'de.

---

## 1. Projenin Amacı

BMI160 6-eksenli ataletsel ölçüm birimi (IMU) ile cihazın uzaydaki yönelimi ölçülür, bu yönelim gerçek zamanlı olarak 1.44" TFT ekranda dönen bir 3B küp ile görselleştirilir.

Sistem tamamen **çevrimdışı ve kapalı devre** çalışır: Wi-Fi, Bluetooth, sunucu veya web arayüzü kullanılmaz. Tüm hesaplama ESP32 üzerinde yapılır.

**Temel çıktı:** Kartı elinizde döndürdüğünüzde ekrandaki küp aynı yönde ve aynı açıda döner.

---

## 2. Donanım

### 2.1 Bileşen Listesi

| Bileşen | Model | Görev |
|---|---|---|
| Geliştirme kartı | ESP32-WROOM-32D (DevKit V1, 30 pin) | Ana işlemci, çift çekirdek |
| Ekran | 1.44" TFT, ST7735 sürücü (V1.1 kırmızı modül) | 128×128 piksel görüntü |
| Sensör | BMI160 | 3 eksen jiroskop + 3 eksen ivmeölçer |
| Güç | 18650 Li-ion + MT3608 yükseltici | Taşınabilir besleme |

### 2.2 Pin Bağlantıları

**TFT Ekran (SPI — VSPI donanım arayüzü):**

| TFT Pini | ESP32 GPIO | Açıklama |
|---|---|---|
| CS | GPIO 5 | Chip Select (VSPI CS0) |
| RST | GPIO 4 | Reset |
| DC (A0) | GPIO 2 | Data/Command seçici |
| SDA (MOSI) | GPIO 23 | VSPI MOSI |
| SCK | GPIO 18 | VSPI CLK |
| VCC | 3.3V | **5V bağlanmamalı** |
| GND | GND | Ortak toprak |

**BMI160 Sensör (I2C):**

| BMI160 Pini | ESP32 GPIO | Açıklama |
|---|---|---|
| SDA | GPIO 21 | I2C veri |
| SCL | GPIO 22 | I2C saat |
| SDO | GND | Adresi 0x68'e sabitler |
| VCC | 3.3V | Besleme |
| GND | GND | Ortak toprak |

> **I2C adres notu:** BMI160'ın SDO pini boştayken veya HIGH iken adres `0x69`, GND'ye çekildiğinde `0x68` olur. Kullanılan kütüphane `0x68` varsayılanıyla daha kararlı çalıştığı için SDO pini GND'ye bağlanmıştır.

### 2.3 Kullanılan Kütüphaneler

```
Adafruit_GFX       → Temel grafik primitifleri (çizgi, üçgen, metin)
Adafruit_ST7735    → ST7735 sürücü katmanı
DFRobot_BMI160     → BMI160 sensör sürücüsü
Wire               → I2C haberleşme
SPI                → SPI haberleşme
```

---

## 3. Kullanılan Algoritmalar

### 3.1 3B Rotasyon Matrisleri

Küpün 8 köşesi normalize koordinatlarda tanımlıdır (`-1` ile `+1` arası). Her köşe, üç eksende sırayla döndürülür.

**X ekseni etrafında dönüş (Pitch):**
```
y' = y·cos(θx) − z·sin(θx)
z' = y·sin(θx) + z·cos(θx)
```

**Y ekseni etrafında dönüş (Roll):**
```
x' = x·cos(θy) + z·sin(θy)
z' = −x·sin(θy) + z·cos(θy)
```

**Z ekseni etrafında dönüş (Yaw):**
```
x' = x·cos(θz) − y·sin(θz)
y' = x·sin(θz) + y·cos(θz)
```

Bu üç dönüşüm ardışık uygulanarak küpün son yönelimi elde edilir.

### 3.2 Perspektif Projeksiyon

3B koordinatları 2B ekrana yansıtmak için perspektif bölme kullanılır. Uzaktaki noktalar küçük, yakındakiler büyük görünür:

```
faktör = ODAK / (z + MESAFE)        // ODAK = 50.0, MESAFE = 4.0
ekran_x = x · faktör + 64
ekran_y = y · faktör + 64
```

`z + MESAFE` payda olduğu için küp gözlemciye yaklaştıkça büyür, uzaklaştıkça küçülür. Bu, ortografik projeksiyona göre çok daha doğal bir derinlik hissi verir.

### 3.3 Back-Face Culling (Arka Yüz Eleme)

Küpün 6 yüzeyinden yalnızca gözlemciye bakanlar çizilir. Bu hem doğru görüntü verir (arka yüzey öndekini gizlemez) hem de çizim yükünü yaklaşık yarıya indirir.

Bir yüzeyin görünür olup olmadığı, ekran düzlemindeki iki kenar vektörünün **çapraz çarpımının z bileşeni** ile bulunur:

```
val = (P1x − P0x)·(P2y − P0y) − (P1y − P0y)·(P2x − P0x)
```

- `val > 0` → yüzey gözlemciye bakıyor, **çiz**
- `val ≤ 0` → yüzey arkada kalmış, **atla**

Her dörtgen yüzey iki üçgene bölünerek `fillTriangle` ile doldurulur.

### 3.4 Tamamlayıcı Filtre (Complementary Filter)

Bu projenin ölçme tekniği açısından **en kritik kısmı** budur.

IMU'daki iki sensörün de kendine özgü zayıflığı vardır:

| Sensör | Ölçtüğü | Güçlü yanı | Zayıf yanı |
|---|---|---|---|
| Jiroskop | Açısal hız (°/s) | Hızlı tepki, gürültüsüz | Entegrasyon sonucu **drift** (zamanla kayma) |
| İvmeölçer | Doğrusal ivme (g) | Yerçekimi referanslı, **drift yok** | Titreşime aşırı duyarlı, gürültülü |

**Jiroskoptan açı:** Açısal hız zamana göre integre edilir.
```
açı_jiro = önceki_açı + (ω · Δt)
```
Her adımda küçük bir hata birikir, dakikalar içinde açı gerçekten sapar.

**İvmeölçerden açı:** Yerçekimi vektörünün bileşenlerinden trigonometrik olarak hesaplanır.
```
pitch_ivme = atan2(−ay, az)
roll_ivme  = atan2(−ax, √(ay² + az²))
```
Bu değer uzun vadede doğrudur ama anlık titreşimlerde çok gürültülüdür.

**Birleştirme:** İki ölçüm ağırlıklı ortalamayla birleştirilir.
```
açı = α · açı_jiro + (1 − α) · açı_ivme        // α = 0.96
```

Yani sonuç %96 jiroskop, %4 ivmeölçerdir. Jiroskop kısa vadeli hassasiyeti, ivmeölçer ise uzun vadeli referansı sağlar — ivmeölçerin sürekli küçük katkısı jiroskobun driftini yavaşça sıfıra çeker.

> **Neden Kalman değil?** Kalman filtresi teorik olarak daha optimaldir ama matris işlemleri gerektirir ve gömülü sistemde çok daha pahalıdır. Tamamlayıcı filtre tek satırlık hesapla benzer sonuç verir; bu ölçekteki uygulamalarda endüstri standardıdır.

**Yaw (Z) ekseni notu:** Yerçekimi Z ekseni etrafındaki dönüşe göre değişmediği için ivmeölçer yaw'ı düzeltemez. Bu yüzden yaw yalnızca jiroskoptan gelir ve zamanla yavaşça drift eder. Tam çözüm için manyetometre (pusula) gerekir; BMI160'ta manyetometre yoktur.

### 3.5 Otomatik Sıfır Noktası Kalibrasyonu

Her jiroskop, hareketsizken bile sıfırdan farklı bir değer üretir (bias/offset). Bu hata integre edildiğinde küp kendi kendine dönmeye başlar.

Açılışta cihaz sabit tutulurken 300 örnek alınır ve aritmetik ortalaması hesaplanır:

```
offset = Σ(ölçüm_i) / N        // N = 300, örnekleme aralığı 5 ms
```

Sonraki tüm ölçümlerden bu offset çıkarılır:
```
düzeltilmiş = (ham_ölçüm − offset) / 131.2 × HIZ_ÇARPANI
```

`131.2` sayısı BMI160'ın ±250 °/s aralığındaki hassasiyet katsayısıdır (LSB/°/s): `32768 / 250 ≈ 131.2`.

Kalibrasyon süresince ekranda ilerleme çubuğu ve yüzde göstergesi gösterilir.

### 3.6 Dinamik Zaman Adımı (Δt)

Sabit bir `delay` değerine güvenmek yerine, her döngüde gerçek geçen süre ölçülür:

```c
unsigned long şimdi = millis();
float dt = (şimdi − önceki) / 1000.0;
önceki = şimdi;
if (dt > 0.1) dt = 0.05;    // takılma durumunda güvenlik sınırı
```

İşlemci yükü değişse bile açı entegrasyonu doğru kalır. Sabit Δt kullanılsaydı, ekran çizimi yavaşladığında açı hesabı hatalı olurdu.

### 3.7 Çift Çekirdekli Görev Ayrımı (Dual-Core)

ESP32'nin iki Xtensa LX6 çekirdeği FreeRTOS üzerinden ayrı görevlere atanmıştır:

| Çekirdek | Görev | Periyot |
|---|---|---|
| **Core 0** | Sensör okuma + filtre hesabı | 10 ms |
| **Core 1** | Ekran çizimi (`loop()`) | ~10-30 ms |

```c
xTaskCreatePinnedToCore(sensorTaskCode, "SensorTask", 10000, NULL, 1, &SensorTask, 0);
```

**Neden önemli?** Tek çekirdekte çalışsaydı, ekran çizimi (yavaş SPI işlemi) sırasında sensör okunamaz, örnekleme frekansı düşer ve açı hesabı bozulurdu. Ayrı çekirdeklerde sensör sabit 100 Hz'de örneklenirken ekran kendi hızında çizer.

İki çekirdek arasındaki veri paylaşımı `volatile` değişkenlerle yapılır:
```c
volatile float angleX, angleY, angleZ;
```
`volatile` anahtar kelimesi derleyicinin bu değişkenleri register'da önbelleğe almasını engeller — böylece bir çekirdeğin yazdığını diğeri anında görür.

---

## 4. Yazılım Akışı

```
BAŞLAT
  │
  ├─ Seri port başlat (115200 baud)
  ├─ TFT başlat → initR(INITR_144GREENTAB), rotation 0
  ├─ I2C başlat → Wire.begin(21, 22)
  ├─ BMI160 başlat → softReset() + I2cInit(0x68)
  │     └─ başarısızsa "SENSOR HATASI!" gösterip dur
  │
  ├─ KALİBRASYON EKRANI
  │     ├─ 300 örnek al (5 ms aralıkla, ~1.5 saniye)
  │     ├─ İlerleme çubuğunu güncelle (mavi → cyan → yeşil)
  │     ├─ Yüzde göstergesini güncelle
  │     └─ Offset'leri hesapla ve sakla
  │
  ├─ Core 0'da sensör görevini başlat
  │
  └─ ANA DÖNGÜ (Core 1)
        ├─ 8 köşeyi döndür ve projeksiyon yap
        ├─ Ekranı temizle
        ├─ Görünür yüzeyleri renkli doldur (back-face culling)
        ├─ HUD bilgilerini yaz (P / R / Z açıları)
        ├─ İsimleri yaz
        └─ tekrarla
```

---

## 5. Karşılaşılan Problemler ve Çözümleri

### 5.1 TFT_eSPI ile beyaz ekran
**Sorun:** `TFT_eSPI` kütüphanesi ile ekran hiç başlamadı, tamamen beyaz kaldı.
**Çözüm:** `Adafruit_GFX` + `Adafruit_ST7735` ikilisine geçildi. `initR(INITR_144GREENTAB)` parametresi bu modül için doğru olan başlatma varyantıdır.

### 5.2 BMI160Gen kütüphanesi derlenmiyor
**Sorun:** `ss_spi.cpp` dosyasında `'OUTPUT' was not declared in this scope` hatası.
**Sebep:** Dosya `Arduino.h`'yi include etmiyordu, `pinMode`, `digitalWrite`, `HIGH`, `LOW` gibi tanımlar bulunamıyordu.
**Çözüm:** `#include <Arduino.h>` satırı dosyanın en üstüne, `#if defined(...)` bloğunun **dışına** eklendi. (Blok içine konulduğunda ESP32 derlemesinde `#else` dalına girildiği için etkisiz kalıyordu.)

### 5.3 BMI160Gen watchdog reset'e sebep oluyor
**Sorun:** `BMI160.begin()` çağrısı 5 saniye takılıyor, ardından ESP32 watchdog tarafından resetleniyordu — sonsuz boot loop.
**Çözüm:** Kütüphane `DFRobot_BMI160` ile değiştirildi. Bu kütüphanenin `softReset()` + `I2cInit()` çağrıları sorunsuz çalıştı.

### 5.4 Flash okuma hatası (`flash read err, 1000`)
**Sorun:** ESP32 boot edemiyor, sürekli reset atıyordu.
**Çözüm:** `Tools → Flash Mode` ayarı **QIO**'dan **DIO**'ya alındı. Ayrıca `Erase All Flash Before Sketch Upload` etkinleştirilip flash tamamen temizlendi.

### 5.5 Eksenlerin ters/kayık çalışması
**Sorun:** Kartı öne eğdiğinde küp yana dönüyordu, ya da tam ters yöne gidiyordu.
**Çözüm:** Sensörün fiziksel montaj yönü ile ekran koordinat sistemi örtüşmediği için eksen haritalaması deneyerek ayarlandı. Ayrıca ivmeölçer pitch hesabındaki işaret düzeltildi: `atan2(ay, az)` → `atan2(-ay, az)`.

### 5.6 HUD sayılarının üst üste binmesi
**Sorun:** Açı değerleri her karede aynı yere yazıldığı için rakamlar okunamaz yeşil bloklara dönüşüyordu.
**Çözüm:** Her yazımdan önce ilgili bölge `fillRect` ile siyaha boyanıyor.

### 5.7 ESP32'nin yanması
**Sorun:** CMD pinine yanlışlıkla 5V verildi. CMD, ESP32'nin dahili flash belleğine giden SPI hattıdır. Sonrasında kart `Failed to connect to ESP32: No serial data received` hatası vermeye başladı, bir daha programlanamadı.
**Çözüm:** Yedek ESP32 kartı ile devam edildi. (30 pin ve 38 pin DevKit varyantlarında kullanılan GPIO numaraları aynı olduğu için kodda değişiklik gerekmedi.)

---

## 6. Ayarlanabilir Parametreler

Kod içinde deneyerek ayarlanabilecek değerler:

| Parametre | Varsayılan | Etkisi |
|---|---|---|
| `HIZ_CARPANI` | `16.0` | Küpün dönüş hassasiyeti. Artırınca daha hızlı tepki verir. |
| Z bölücü | `HIZ_CARPANI / 2.0` | Yaw ekseni ayrı ölçeklenir (90° döndürmede 180° dönme sorununun çözümü). |
| `0.96` | Filtre katsayısı | Büyütünce jiroskoba, küçültünce ivmeölçere ağırlık verir. |
| `scale` (50.0) | Projeksiyon | Küpün ekrandaki boyutu. |
| `distance` (4.0) | Projeksiyon | Perspektif derinliği. Küçültünce perspektif abartılı olur. |
| `samples` (300) | Kalibrasyon | Örnek sayısı. Artırınca daha doğru offset ama daha uzun bekleme. |
| `delay(10)` | Kare hızı | Düşürünce daha akıcı, işlemci yükü artar. |

---

## 7. Ölçme Tekniği Açısından Değerlendirme

Bu proje, Elektrik Elektronik Ölçmeleri dersi kapsamında şu konuları pratikte kapsar:

**Sensör hata kaynakları**
- Bias (sıfır noktası kayması) → kalibrasyon ile giderildi
- Drift (zamanla birikimli hata) → tamamlayıcı filtre ile bastırıldı
- Gürültü → filtre ağırlıklandırması ile azaltıldı

**Sensör füzyonu**
İki farklı fiziksel prensiple çalışan sensörün (açısal hız vs. yerçekimi vektörü) birleştirilerek tek bir sensörden daha iyi sonuç elde edilmesi.

**Ölçek faktörü ve birim dönüşümü**
Ham ADC değerinden fiziksel birime (°/s) geçiş: `ham / 131.2`. Bu katsayı sensörün seçili ölçüm aralığına (±250 °/s) bağlıdır.

**Örnekleme frekansı ve zaman senkronizasyonu**
Sabit Δt varsayımının neden hatalı olduğu, gerçek zaman ölçümünün neden gerekli olduğu.

**Sayısal haberleşme protokolleri**
I2C (sensör, adres seçimi dahil) ve SPI (ekran) protokollerinin aynı sistemde eşzamanlı kullanımı.

---

## 8. Tam Kaynak Kod

```cpp
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <DFRobot_BMI160.h>

#define TFT_CS     5
#define TFT_RST    4
#define TFT_DC     2

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
DFRobot_BMI160 bmi160;
const int8_t i2c_addr = 0x68;

#define CX 64
#define CY 64

// Kupun 8 kosesi (normalize koordinat)
float cube[8][3] = {
  {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
  {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
};

// 6 yuzey, her biri 4 kose indeksi
int faces[6][4] = {
  {0, 1, 2, 3}, {4, 5, 6, 7}, {0, 4, 5, 1},
  {1, 5, 6, 2}, {2, 6, 7, 3}, {3, 7, 4, 0}
};

uint16_t colors[6] = {
  ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE,
  ST77XX_YELLOW, ST77XX_MAGENTA, ST77XX_CYAN
};

struct Point2D { int x, y; };
Point2D projected[8];

// Cekirdekler arasi paylasilan yonelim verisi
volatile float angleX = 0, angleY = 0, angleZ = 0;
float gXOffset = 0, gYOffset = 0, gZOffset = 0;

TaskHandle_t SensorTask;

// --- 3B DONUS + PERSPEKTIF PROJEKSIYON ---
void project(float x, float y, float z, int &sx, int &sy) {
  float cosx = cos(angleX), sinx = sin(angleX);
  float y1 = y * cosx - z * sinx;
  float z1 = y * sinx + z * cosx;

  float cosy = cos(angleY), siny = sin(angleY);
  float x2 = x * cosy + z1 * siny;
  float z2 = -x * siny + z1 * cosy;

  float cosz = cos(angleZ), sinz = sin(angleZ);
  float x3 = x2 * cosz - y1 * sinz;
  float y3 = x2 * sinz + y1 * cosz;

  float factor = 50.0 / (z2 + 4.0);
  sx = (int)(x3 * factor) + CX;
  sy = (int)(y3 * factor) + CY;
}

// =======================================================
// CORE 0 : SENSOR OKUMA VE TAMAMLAYICI FILTRE
// =======================================================
void sensorTaskCode(void * parameter) {
  unsigned long lastTime = millis();
  for (;;) {
    int16_t accelGyro[6] = {0};
    if (bmi160.getAccelGyroData(accelGyro) == 0) {

      // Dinamik zaman adimi
      unsigned long currentTime = millis();
      float dt = (currentTime - lastTime) / 1000.0;
      lastTime = currentTime;
      if (dt > 0.1) dt = 0.05;

      float HIZ_CARPANI = 16.0;

      // Ham veriden offset cikar, derece/saniyeye cevir
      float raw_gx = ((accelGyro[0] - gXOffset) / 131.2) * HIZ_CARPANI;
      float raw_gy = ((accelGyro[1] - gYOffset) / 131.2) * HIZ_CARPANI;
      float raw_gz = ((accelGyro[2] - gZOffset) / 131.2) * (HIZ_CARPANI / 2.0);

      // Eksen haritalamasi (montaj yonune gore)
      float gx = raw_gx;
      float gy = raw_gy;
      float gz = -raw_gz;

      // Jiroskop entegrasyonu
      float gyroPitch = (angleX * 57.2958) + (gx * dt);
      float gyroRoll  = (angleY * 57.2958) + (gy * dt);
      float gyroYaw   = (angleZ * 57.2958) + (gz * dt);

      // Ivmeolcerden mutlak aci
      float ax = accelGyro[3] / 16384.0;
      float ay = accelGyro[4] / 16384.0;
      float az = accelGyro[5] / 16384.0;
      float accPitch = atan2(-ay, az) * 57.2958;
      float accRoll  = atan2(-ax, sqrt(ay * ay + az * az)) * 57.2958;

      // Tamamlayici filtre : %96 jiro + %4 ivme
      float finalPitch = 0.96 * gyroPitch + 0.04 * accPitch;
      float finalRoll  = 0.96 * gyroRoll  + 0.04 * accRoll;
      float finalYaw   = gyroYaw;   // yaw sadece jirodan

      angleX = finalPitch * 0.0174533;
      angleY = finalRoll  * 0.0174533;
      angleZ = finalYaw   * 0.0174533;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);

  tft.initR(INITR_144GREENTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);

  Wire.begin(21, 22);
  if (bmi160.softReset() != BMI160_OK || bmi160.I2cInit(i2c_addr) != BMI160_OK) {
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(1);
    tft.setCursor(5, 55);
    tft.println("SENSOR HATASI!");
    while (1);
  }

  // ---------- KALIBRASYON EKRANI ----------
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(22, 10);
  tft.print("[ KALIBRASYON ]");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(8, 28);
  tft.print("Cihazi sabit tutun!");

  tft.setTextColor(ST77XX_RED);
  tft.setCursor(2, 50);  tft.print("X");
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(12, 50); tft.print("Y");
  tft.setTextColor(ST77XX_BLUE);
  tft.setCursor(22, 50); tft.print("Z");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(32, 50); tft.print("olculuyor...");

  tft.drawRect(4, 100, 120, 14, ST77XX_WHITE);

  long gxSum = 0, gySum = 0, gzSum = 0;
  int samples = 300;
  for (int i = 0; i < samples; i++) {
    int16_t ag[6] = {0};
    bmi160.getAccelGyroData(ag);
    gxSum += ag[0];
    gySum += ag[1];
    gzSum += ag[2];

    if (i % 3 == 0) {
      int barWidth = (int)((float)i / samples * 116);
      uint16_t barColor;
      if (i < samples / 3)            barColor = ST77XX_BLUE;
      else if (i < (samples * 2) / 3) barColor = ST77XX_CYAN;
      else                            barColor = ST77XX_GREEN;

      tft.fillRect(6, 102, barWidth, 10, barColor);
      tft.fillRect(45, 78, 40, 12, ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE);
      tft.setCursor(45, 80);
      tft.print("%");
      tft.print((int)((float)i / samples * 100));
    }
    delay(5);
  }

  tft.fillRect(6, 102, 116, 10, ST77XX_GREEN);
  tft.fillRect(45, 78, 40, 12, ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(45, 80);
  tft.print("%100");
  tft.setTextSize(2);
  tft.setCursor(28, 62);
  tft.print("HAZIR!");
  delay(800);

  // Offsetleri sakla
  gXOffset = (float)gxSum / samples;
  gYOffset = (float)gySum / samples;
  gZOffset = (float)gzSum / samples;

  tft.fillScreen(ST77XX_BLACK);

  // Core 0'da sensor gorevini baslat
  xTaskCreatePinnedToCore(
    sensorTaskCode, "SensorTask", 10000,
    NULL, 1, &SensorTask, 0
  );
}

// =======================================================
// CORE 1 : SADECE EKRAN CIZIMI
// =======================================================
void loop() {
  // 8 koseyi dondur ve projeksiyon yap
  for (int i = 0; i < 8; i++) {
    project(cube[i][0], cube[i][1], cube[i][2],
            projected[i].x, projected[i].y);
  }

  tft.fillScreen(ST77XX_BLACK);

  // BACK-FACE CULLING : sadece gorunen yuzeyleri ciz
  for (int i = 0; i < 6; i++) {
    int p0 = faces[i][0], p1 = faces[i][1],
        p2 = faces[i][2], p3 = faces[i][3];

    long val = (long)(projected[p1].x - projected[p0].x) *
                     (projected[p2].y - projected[p0].y) -
               (long)(projected[p1].y - projected[p0].y) *
                     (projected[p2].x - projected[p0].x);

    if (val > 0) {
      tft.fillTriangle(
        projected[p0].x, projected[p0].y,
        projected[p1].x, projected[p1].y,
        projected[p2].x, projected[p2].y, colors[i]);
      tft.fillTriangle(
        projected[p0].x, projected[p0].y,
        projected[p2].x, projected[p2].y,
        projected[p3].x, projected[p3].y, colors[i]);
      tft.drawTriangle(
        projected[p0].x, projected[p0].y,
        projected[p1].x, projected[p1].y,
        projected[p2].x, projected[p2].y, ST77XX_BLACK);
      tft.drawTriangle(
        projected[p0].x, projected[p0].y,
        projected[p2].x, projected[p2].y,
        projected[p3].x, projected[p3].y, ST77XX_BLACK);
    }
  }

  // ---------- HUD ----------
  tft.setTextSize(1);

  tft.fillRect(0, 0, 75, 10, ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(2, 2);
  tft.print("P:"); tft.print((int)(angleX * 57.3));

  tft.fillRect(0, 11, 75, 10, ST77XX_BLACK);
  tft.setCursor(2, 12);
  tft.print("R:"); tft.print((int)(angleY * 57.3));

  tft.fillRect(86, 0, 42, 10, ST77XX_BLACK);
  tft.setCursor(88, 2);
  tft.print("Z:"); tft.print((int)(angleZ * 57.3));

  tft.fillRect(0, 108, 128, 20, ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(2, 110);
  tft.print("Ahmet Efe Nezli");

  delay(10);
}
```

---

## 9. Arduino IDE Ayarları

Derleme ve yükleme için gerekli ayarlar (`Tools` menüsü):

```
Board                            : ESP32 Dev Module
Upload Speed                     : 115200
Flash Frequency                  : 40MHz
Flash Mode                       : DIO          ← QIO ile flash hatasi veriyor
Flash Size                       : 4MB (32Mb)
Partition Scheme                 : Default 4MB with spiffs
Erase All Flash Before Upload    : Enabled
Core Debug Level                 : None
```

**Yükleme sorunu yaşanırsa:** `Connecting......` yazısı çıktığında karttaki **BOOT** butonuna basılı tutun, `Writing at...` yazısını görünce bırakın.

---

## 10. Olası Geliştirmeler

- **Manyetometre eklenmesi:** Yaw eksenindeki driftin tamamen giderilmesi için 3 eksen pusula (örn. HMC5883L) eklenebilir, 9-DOF füzyon yapılabilir.
- **Kalman filtresi:** Tamamlayıcı filtre yerine gerçek bir Kalman filtresi ile daha optimal tahmin yapılabilir.
- **Çift tamponlama (double buffering):** Tüm kareyi RAM'de oluşturup tek seferde ekrana basmak titremeyi tamamen ortadan kaldırır (128×128×2 = 32 KB RAM gerekir, ESP32'de mümkün).
- **Z-buffer:** Back-face culling yerine derinlik tamponu ile daha karmaşık modeller doğru çizilebilir.
- **Quaternion tabanlı yönelim:** Euler açıları gimbal lock problemine sahiptir; quaternion kullanımı bunu ortadan kaldırır.
- **Veri kaydı:** SD kart modülü ile yönelim verisi zaman damgalı olarak kaydedilip sonradan analiz edilebilir.
