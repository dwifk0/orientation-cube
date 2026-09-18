// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Ahmet Efe Nezli

#include "yonelim.h"
#include <math.h>

namespace yonelim {

void Bias::ekle(int16_t gx, int16_t gy, int16_t gz) {
    sx_ += gx; sy_ += gy; sz_ += gz; n_++;
}

void Bias::bitir() {
    if (n_ == 0) { ox_ = oy_ = oz_ = 0.0f; return; }
    ox_ = (float)sx_ / (float)n_;
    oy_ = (float)sy_ / (float)n_;
    oz_ = (float)sz_ / (float)n_;
}

float ivme_pitch(float ax, float ay, float az) {
    (void)ax;
    return atan2f(-ay, az) * RAD_DERECE;
}

float ivme_roll(float ax, float ay, float az) {
    return atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_DERECE;
}

void Suzgec::guncelle(int16_t gx, int16_t gy, int16_t gz,
                      int16_t ax, int16_t ay, int16_t az,
                      const Bias& bias, float dt) {
    // ⚠ Sabit dt varsaymak yerine gercek gecen sure kullaniliyor. Ekran
    // cizimi yavasladiginda sabit dt aci hesabini sessizce bozardi.
    // Takilma (ornegin uzun bir SPI blogu) durumunda dt makul bir degere
    // cekiliyor; aksi halde tek bir buyuk dt aciyi bir anda firlatir.
    if (dt <= 0.0f || dt > a_.dt_tavan) dt = a_.dt_yedek;

    // Ham -> derece/saniye (bias cikarilmis)
    const float wx = ((float)gx - bias.x()) / JIRO_LSB;
    const float wy = ((float)gy - bias.y()) / JIRO_LSB;
    const float wz = ((float)gz - bias.z()) / JIRO_LSB;

    // Jiroskop entegrasyonu — kisa vadede dogru, uzun vadede kayar
    const float jiro_pitch = pitch_ + wx * dt;
    const float jiro_roll  = roll_  + wy * dt;
    const float jiro_yaw   = yaw_   - wz * dt;   // isaret: montaj yonu

    // Ivmeolcerden mutlak aci — uzun vadede dogru, anlik gurultulu
    const float gax = (float)ax / IVME_LSB;
    const float gay = (float)ay / IVME_LSB;
    const float gaz = (float)az / IVME_LSB;

    pitch_ = a_.alfa * jiro_pitch + (1.0f - a_.alfa) * ivme_pitch(gax, gay, gaz);
    roll_  = a_.alfa * jiro_roll  + (1.0f - a_.alfa) * ivme_roll (gax, gay, gaz);

    // ⚠ Yaw'da ikinci terim YOK — duzeltilecek bir referans yok.
    yaw_ = jiro_yaw;
}

}  // namespace yonelim
