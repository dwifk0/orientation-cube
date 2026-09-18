// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Ahmet Efe Nezli
//
// YONELIM — 6 eksenli IMU verisinden tamamlayici filtreyle aci kestirimi.
//
// Iki sensorun de kendine ozgu zayifligi var:
//   jiroskop   : hizli ve gurultusuz, ama entegrasyon DRIFT uretir
//   ivmeolcer  : yercekimi referansli, drift YOK, ama titresime duyarli
//
// Tamamlayici filtre ikisini agirlikli ortalamayla birlestirir:
//   aci = alfa * (onceki + omega*dt) + (1-alfa) * ivmeolcer_acisi
// Jiroskop kisa vadeli hassasiyeti, ivmeolcer uzun vadeli referansi saglar;
// ivmeolcerin surekli kucuk katkisi jiroskobun driftini sifira ceker.
//
// ⚠ YAW DUZELTILEMEZ. Yercekimi Z ekseni etrafindaki donuse gore degismedigi
// icin ivmeolcerin yaw hakkinda soyleyecegi bir sey yok; yaw yalnizca
// jiroskoptan gelir ve yavasca kayar. Tam cozum manyetometre ister — BMI160'ta
// manyetometre yoktur.
//
// Bu dosyada donanim yok: ham sensor verisi ve gecen sure disaridan geliyor.
// Ayni kod hem ESP32'de kosuyor hem masaustunde test ediliyor.

#pragma once
#include <stdint.h>

namespace yonelim {

// BMI160 olcek katsayilari — secili olcum araligina bagli.
//   jiroskop  +/-2000 derece/s -> 16,4 LSB per derece/s (veri sayfasi)
//   ivmeolcer +/-2 g           -> 32768/2 = 16384 LSB per g
//
// 🔴 Araligi BU KOD SECMIYOR, DFRobot_BMI160 kutuphanesi seciyor: I2cInit()
// jiroskobu +/-2000 derece/s'ye, ivmeolceri +/-2 g'ye kuruyor. Ilk surumde
// burada +/-250'nin katsayisi (131,2) vardi; hizlar 8 kat kucuk olculuyordu ve
// bu, jiroskobu 16 ile, yaw'i 8 ile carpan bir "hassasiyet" ayariyla
// farkinda olmadan telafi ediliyordu. Kutuphane ya da aralik degisirse bu
// sayi da degismeli.
constexpr float JIRO_LSB   = 16.4f;
constexpr float IVME_LSB   = 16384.0f;
constexpr float RAD_DERECE = 57.29578f;
constexpr float DERECE_RAD = 0.01745329f;

// ─────────────────────────────────────────────────────────────────────────
// JIROSKOP SIFIR NOKTASI (bias) KALIBRASYONU
//
// Her jiroskop hareketsizken bile sifirdan farkli bir deger uretir. Bu hata
// integre edildiginde kup kendi kendine donmeye baslar. Acilista cihaz sabit
// tutulurken N ornek alinip aritmetik ortalamasi offset olarak saklanir.
// ─────────────────────────────────────────────────────────────────────────
class Bias {
public:
    void ekle(int16_t gx, int16_t gy, int16_t gz);
    void bitir();                       // toplamlari ortalamaya cevirir
    uint16_t ornek() const { return n_; }
    float x() const { return ox_; }
    float y() const { return oy_; }
    float z() const { return oz_; }
private:
    long     sx_ = 0, sy_ = 0, sz_ = 0;
    uint16_t n_  = 0;
    float    ox_ = 0, oy_ = 0, oz_ = 0;
};

struct Ayar {
    float alfa;        // 0..1 — buyudukce jiroskoba agirlik (tipik 0,96)
    float dt_tavan;    // takilma korumasi [s]
    float dt_yedek;    // takilma halinde kullanilacak dt [s]
};

// Aci kestirimi. Durum DERECE cinsinden tutulur; radyana cevirmek cagiranin
// isi. Boylece her tikte radyan-derece gidip gelmesi ortadan kalkiyor.
class Suzgec {
public:
    explicit Suzgec(const Ayar& a) : a_(a) {}

    // Ham IMU verisiyle bir adim ilerletir.
    //   g*  : ham jiroskop (bias CIKARILMAMIS)
    //   a*  : ham ivmeolcer
    //   bias: kalibrasyon sonucu
    //   dt_s: gecen sure [s]
    void guncelle(int16_t gx, int16_t gy, int16_t gz,
                  int16_t ax, int16_t ay, int16_t az,
                  const Bias& bias, float dt_s);

    float pitch() const { return pitch_; }   // derece
    float roll()  const { return roll_;  }   // derece
    float yaw()   const { return yaw_;   }   // derece — drift eder

    void sifirla() { pitch_ = roll_ = yaw_ = 0.0f; }

private:
    Ayar  a_;
    float pitch_ = 0, roll_ = 0, yaw_ = 0;
};

// Ivmeolcerden mutlak aci — yercekimi vektorunun bilesenlerinden.
// Uzun vadede dogru, anlik titresimde gurultulu.
float ivme_pitch(float ax_g, float ay_g, float az_g);
float ivme_roll (float ax_g, float ay_g, float az_g);

}  // namespace yonelim
