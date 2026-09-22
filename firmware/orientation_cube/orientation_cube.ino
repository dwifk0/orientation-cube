// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
//
// =====================================================================
//  ORIENTATION CUBE — ESP32 + BMI160 ile gercek zamanli yonelim olcumu ve
//          3B kup gorsellestirme
// =====================================================================
//
//  Elektrik Elektronik Olcmeleri donem sonu projesi.
//  Tamamen cevrimdisi calisir: WiFi, Bluetooth, sunucu yok.
//
//  DONANIM
//    ESP32-WROOM-32D (DevKit V1, 30 pin)
//    1.44" TFT, ST7735 surucu, 128x128        SPI (VSPI)
//    BMI160 6 eksen IMU                       I2C, adres 0x68
//    18650 Li-ion + MT3608 yukseltici
//
//  PIN HARITASI
//    TFT  CS 5 · RST 4 · DC 2 · MOSI 23 · SCK 18      (3V3 — 5V BAGLAMA)
//    IMU  SDA 21 · SCL 22 · SDO -> GND (adres 0x68'e sabitler)
//
//  CEKIRDEK AYRIMI
//    Core 0 : sensor okuma + tamamlayici filtre   sabit 100 Hz
//    Core 1 : ekran cizimi (loop)                 kendi hizinda
//  Tek cekirdekte kosulsaydi yavas SPI cizimi sirasinda sensor okunamaz,
//  ornekleme frekansi duser ve aci hesabi bozulurdu.
//
//  ARDUINO IDE AYARLARI
//    Board: ESP32 Dev Module · Flash Mode: DIO  (QIO'da flash read err)
//    Erase All Flash Before Upload: Enabled
//    Yukleme takilirsa: "Connecting..." gorununce BOOT'a bas, "Writing"de birak
// =====================================================================

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <DFRobot_BMI160.h>

#include "src/yonelim.h"
#include "src/kup3b.h"

// ---------------------- pinler ----------------------
#define TFT_CS   5
#define TFT_RST  4
#define TFT_DC   2
#define I2C_SDA  21
#define I2C_SCL  22
static const int8_t IMU_ADRES = 0x68;

// ---------------------- ayarlar ----------------------
static const int   EKRAN    = 128;
static const int   MERKEZ   = EKRAN / 2;
static const float SUZGEC_ALFA  = 0.96f;   // %96 jiro + %4 ivme
static const int   BIAS_ORNEK   = 300;     // ~1,5 s
static const int   BIAS_ARALIK  = 5;       // ms
static const int   SENSOR_MS    = 10;      // Core 0 periyodu -> 100 Hz

// 🔴 GORSEL KAZANC. 1,0 = kart ne kadar donerse kup o kadar doner (1:1).
// Buyuk deger hareketi abartir; gosteri icin guzel ama artik OLCUM
// gostermez, yorum gosterir. Bu yuzden varsayilan 1,0 birakildi ve
// abartma ayri bir ayar olarak duruyor.
static const float GORSEL_KAZANC = 1.0f;

static const kup3b::Izdusum IZDUSUM = { 50.0f, 4.0f, MERKEZ, MERKEZ };

// ---------------------- durum ----------------------
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
DFRobot_BMI160  imu;

static yonelim::Bias   bias;
static yonelim::Suzgec suzgec({SUZGEC_ALFA, 0.1f, 0.05f});

// Cekirdekler arasi paylasilan yonelim. `volatile` derleyicinin degeri
// register'da onbellege almasini engeller.
//
// ⚠ Uc float ATOMIK DEGIL: Core 1 okurken Core 0 araya girip birini
// guncelleyebilir, yani bir kare yarisi eski yarisi yeni aciyla cizilebilir.
// 32 bit float tek komutla yazildigi icin deger BOZULMAZ, yalnizca uc eksen
// bir kare boyunca birbiriyle uyumsuz olabilir — gorsel etkisi yok. Kritik
// bir uygulamada burada mutex ya da cift tampon gerekirdi.
static volatile float aci_x = 0, aci_y = 0, aci_z = 0;

static const uint16_t YUZEY_RENK[kup3b::YUZEY] = {
    ST77XX_RED, ST77XX_GREEN,   ST77XX_BLUE,
    ST77XX_YELLOW, ST77XX_MAGENTA, ST77XX_CYAN
};

// =====================================================================
//  CORE 0 — sensor okuma ve filtre
// =====================================================================
static void sensor_gorevi(void*) {
    uint32_t onceki = millis();
    for (;;) {
        int16_t ham[6] = {0};
        if (imu.getAccelGyroData(ham) == 0) {
            const uint32_t simdi = millis();
            const float dt = (float)(simdi - onceki) / 1000.0f;
            onceki = simdi;

            // ham[0..2] jiroskop, ham[3..5] ivmeolcer
            suzgec.guncelle(ham[0], ham[1], ham[2],
                            ham[3], ham[4], ham[5], bias, dt);

            aci_x = suzgec.pitch() * GORSEL_KAZANC * yonelim::DERECE_RAD;
            aci_y = suzgec.roll()  * GORSEL_KAZANC * yonelim::DERECE_RAD;
            aci_z = suzgec.yaw()   * GORSEL_KAZANC * yonelim::DERECE_RAD;
        }
        vTaskDelay(SENSOR_MS / portTICK_PERIOD_MS);
    }
}

// =====================================================================
//  Kalibrasyon ekrani
// =====================================================================
static void kalibrasyon_ekrani() {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_CYAN);  tft.setCursor(22, 10); tft.print("[ KALIBRASYON ]");
    tft.setTextColor(ST77XX_YELLOW);tft.setCursor(8, 28);  tft.print("Cihazi sabit tutun!");
    tft.drawRect(4, 100, 120, 14, ST77XX_WHITE);

    for (int i = 0; i < BIAS_ORNEK; i++) {
        int16_t ham[6] = {0};
        if (imu.getAccelGyroData(ham) == 0) bias.ekle(ham[0], ham[1], ham[2]);

        if (i % 3 == 0) {
            const int   en = (int)((float)i / BIAS_ORNEK * 116.0f);
            const uint16_t renk = (i < BIAS_ORNEK / 3)       ? ST77XX_BLUE
                                : (i < BIAS_ORNEK * 2 / 3)   ? ST77XX_CYAN
                                                             : ST77XX_GREEN;
            tft.fillRect(6, 102, en, 10, renk);
            tft.fillRect(45, 78, 40, 12, ST77XX_BLACK);
            tft.setTextColor(ST77XX_WHITE);
            tft.setCursor(45, 80);
            tft.print("%"); tft.print((int)((float)i / BIAS_ORNEK * 100.0f));
        }
        delay(BIAS_ARALIK);
    }
    bias.bitir();

    tft.fillRect(6, 102, 116, 10, ST77XX_GREEN);
    tft.setTextColor(ST77XX_GREEN); tft.setTextSize(2);
    tft.setCursor(28, 62); tft.print("HAZIR!");
    delay(800);
    tft.fillScreen(ST77XX_BLACK);
}

static void hata_ekrani(const char* mesaj) {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_RED); tft.setTextSize(1);
    tft.setCursor(5, 55); tft.println(mesaj);
    // ⚠ Bos `while(1)` watchdog resetine yol acar. Beklerken zamanlayiciya
    // nefes aldiriliyor.
    for (;;) delay(1000);
}

void setup() {
    Serial.begin(115200);

    tft.initR(INITR_144GREENTAB);   // bu modul icin dogru varyant
    tft.setRotation(0);
    tft.fillScreen(ST77XX_BLACK);

    Wire.begin(I2C_SDA, I2C_SCL);
    if (imu.softReset() != BMI160_OK || imu.I2cInit(IMU_ADRES) != BMI160_OK)
        hata_ekrani("SENSOR HATASI!");

    kalibrasyon_ekrani();

    xTaskCreatePinnedToCore(sensor_gorevi, "sensor", 4096,
                            nullptr, 1, nullptr, /*cekirdek=*/0);
}

// =====================================================================
//  CORE 1 — yalniz ekran cizimi
// =====================================================================
void loop() {
    kup3b::Nokta2B kose[kup3b::KOSE];
    kup3b::kup_yansit(aci_x, aci_y, aci_z, IZDUSUM, kose);

    tft.fillScreen(ST77XX_BLACK);

    for (int i = 0; i < kup3b::YUZEY; i++) {
        if (!kup3b::gorunur(kose, i)) continue;      // arka yuz eleme
        const int* y = kup3b::YUZEYLER[i];
        const kup3b::Nokta2B &p0 = kose[y[0]], &p1 = kose[y[1]],
                             &p2 = kose[y[2]], &p3 = kose[y[3]];
        tft.fillTriangle(p0.x,p0.y, p1.x,p1.y, p2.x,p2.y, YUZEY_RENK[i]);
        tft.fillTriangle(p0.x,p0.y, p2.x,p2.y, p3.x,p3.y, YUZEY_RENK[i]);
        tft.drawTriangle(p0.x,p0.y, p1.x,p1.y, p2.x,p2.y, ST77XX_BLACK);
        tft.drawTriangle(p0.x,p0.y, p2.x,p2.y, p3.x,p3.y, ST77XX_BLACK);
    }

    // HUD — her yazimdan once bolge siyaha boyaniyor, yoksa rakamlar
    // ust uste binip okunmaz hale geliyor.
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_GREEN);
    tft.fillRect(0, 0, 75, 10, ST77XX_BLACK);
    tft.setCursor(2, 2);  tft.print("P:"); tft.print((int)(aci_x * yonelim::RAD_DERECE));
    tft.fillRect(0, 11, 75, 10, ST77XX_BLACK);
    tft.setCursor(2, 12); tft.print("R:"); tft.print((int)(aci_y * yonelim::RAD_DERECE));
    tft.fillRect(86, 0, 42, 10, ST77XX_BLACK);
    tft.setCursor(88, 2); tft.print("Z:"); tft.print((int)(aci_z * yonelim::RAD_DERECE));

    delay(10);
}
