// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
//
// KUP3B — 3B donusum, perspektif izdusum ve arka yuz eleme.
//
// Kupun 8 kosesi normalize koordinatlarda (-1..+1) tanimli. Her kose uc
// eksende sirayla dondurulup 2B ekrana yansitiliyor.
//
// Bu dosyada donanim yok: cizim cagiran tarafta, burada yalnizca geometri.

#pragma once
#include <stdint.h>

namespace kup3b {

constexpr int KOSE  = 8;
constexpr int YUZEY = 6;

struct Nokta2B { int x, y; };

struct Izdusum {
    float odak;      // buyudukce kup buyur
    float mesafe;    // perspektif derinligi; kucuk deger abartili perspektif
    int   merkez_x, merkez_y;
};

// Kupun kose koordinatlari ve yuzey tanimlari (kose indisleri).
extern const float KOSELER[KOSE][3];
extern const int   YUZEYLER[YUZEY][4];

// Tek noktayi dondur ve ekrana yansit. Acilar RADYAN.
//
// Perspektif bolme:  faktor = odak / (z + mesafe)
// `z + mesafe` payda oldugu icin kup gozlemciye yaklastikca buyur. Ortografik
// izdusume gore cok daha dogal bir derinlik hissi verir.
Nokta2B yansit(float x, float y, float z,
               float aci_x, float aci_y, float aci_z,
               const Izdusum& iz);

// Kupun 8 kosesini birden yansitir.
void kup_yansit(float aci_x, float aci_y, float aci_z,
                const Izdusum& iz, Nokta2B* cikti);

// ─────────────────────────────────────────────────────────────────────────
// ARKA YUZ ELEME (back-face culling)
//
// 6 yuzeyden yalnizca gozlemciye bakanlar cizilir. Hem dogru goruntu verir
// (arka yuzey ondekini gizlemez) hem cizim yukunu yariya indirir.
//
// Olcut: ekran duzlemindeki iki kenar vektorunun capraz carpiminin isareti.
// Sonuc pozitifse yuzey bize donuk.
// ─────────────────────────────────────────────────────────────────────────
long yuzey_isareti(const Nokta2B& p0, const Nokta2B& p1, const Nokta2B& p2);
bool gorunur(const Nokta2B* kose, int yuzey_indisi);

}  // namespace kup3b
