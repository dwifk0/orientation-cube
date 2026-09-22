// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
#include "test.h"
#include "../firmware/orientation_cube/src/yonelim.h"
#include <cmath>
using namespace yonelim;

static bool yakin(float a, float b, float t = 0.01f) { return std::fabs(a-b) < t; }

// Duz duran bir cihaz: yercekimi tamamen +Z'de, jiroskop sifir.
static const int16_t DUZ_AZ = (int16_t)IVME_LSB;

int main() {
    // ── bias kalibrasyonu ───────────────────────────────────────────────
    {
        Bias b;
        for (int i = 0; i < 100; i++) b.ekle(50, -30, 10);
        b.bitir();
        t::esit("ornek sayisi", 100, (long long)b.ornek());
        t::dogru("x offset ortalamasi", yakin(b.x(),  50.0f));
        t::dogru("y offset ortalamasi", yakin(b.y(), -30.0f));
        // Kalibre edilmemis bir jiroskopta bu offset integre edilir ve kup
        // kendi kendine donmeye baslar.
    }
    {
        Bias b; b.bitir();
        t::dogru("hic ornek yoksa offset sifir", yakin(b.x(), 0.0f));
    }

    // ── ivmeolcerden aci ────────────────────────────────────────────────
    t::dogru("duz dururken pitch 0", yakin(ivme_pitch(0, 0, 1), 0.0f));
    t::dogru("duz dururken roll 0",  yakin(ivme_roll (0, 0, 1), 0.0f));
    t::dogru("one egik pitch negatif", ivme_pitch(0, 0.5f, 0.87f) < 0.0f);
    t::dogru("yana egik roll negatif",  ivme_roll (0.5f, 0, 0.87f) < 0.0f);

    Ayar a{0.96f, 0.1f, 0.05f};

    // ── bias CIKARILIYOR mu ─────────────────────────────────────────────
    {
        Bias b;
        for (int i = 0; i < 10; i++) b.ekle(64, 0, 0);
        b.bitir();
        Suzgec s(a);
        // Sensor hala 64 basiyor ama cihaz DURUYOR: offset cikinca aci
        // buyumemeli.
        for (int i = 0; i < 200; i++) s.guncelle(64, 0, 0, 0, 0, DUZ_AZ, b, 0.01f);
        t::dogru("bias cikarildi, aci kaymadi", yakin(s.pitch(), 0.0f, 0.5f));
    }
    {
        Bias b; b.bitir();                       // kalibrasyon YOK
        Suzgec s(a);
        for (int i = 0; i < 400; i++) s.guncelle(64, 0, 0, 0, 0, DUZ_AZ, b, 0.01f);
        // 🔴 BEKLENMEYEN AMA DOGRU SONUC: kalibrasyon olmasa bile aci
        // SINIRSIZ kaymiyor. Tamamlayici filtre onu sabit bir kalici hataya
        // oturtuyor:  x* = alfa*w*dt / (1-alfa)
        //             = 0,96 * (64/16,4) * 0,01 / 0,04  ~ 0,94 derece
        // Yani filtre bias hatasini da bastiriyor — ama SIFIRLAMIYOR,
        // kalici bir ofset olarak birakiyor. Kalibrasyon hala gerekli.
        t::dogru("kalibrasyonsuz kalici ofset olusuyor", s.pitch() > 0.5f);
        t::dogru("ama sinirli kaliyor, kacmiyor",        s.pitch() < 1.5f);
    }
    {
        // Ayni hata SAF jiroskopta sinirsiz buyur — karsilastirma budur.
        Bias b; b.bitir();
        Ayar saf{1.0f, 0.1f, 0.05f};
        Suzgec s(saf);
        for (int i = 0; i < 400; i++) s.guncelle(64, 0, 0, 0, 0, DUZ_AZ, b, 0.01f);
        t::dogru("saf jiroskopta drift sinirsiz buyur", s.pitch() > 10.0f);
    }

    // ── olcek katsayisi: 16,4 LSB / (derece/s) ──────────────────────────
    {
        // Kutuphanenin kurdugu +/-2000 derece/s araligina ait katsayi.
        // 131,2 (+/-250) geri gelirse kup 8 kat yavas doner.
        t::dogru("JIRO_LSB +/-2000 derece/s araligina ait", yakin(JIRO_LSB, 16.4f, 0.05f));
    }
    {
        Bias b; b.bitir();
        Ayar saf{1.0f, 0.1f, 0.05f};             // tamamen jiroskop
        Suzgec s(saf);
        // 16,4 ham deger = 1 derece/s. 1 saniye boyunca -> 1 derece.
        for (int i = 0; i < 100; i++) s.guncelle(164, 0, 0, 0, 0, DUZ_AZ, b, 0.01f);
        t::dogru("164 ham ~ 10 derece/s -> 1 s'de 10 derece",
                 yakin(s.pitch(), 10.0f, 0.1f));
    }

    // ── tamamlayici filtre driftı geri cekiyor ──────────────────────────
    {
        Bias b; b.bitir();
        Suzgec s(a);
        // Cihaz DUZ duruyor ama jiroskop sabit bir yalan sutunu basiyor.
        for (int i = 0; i < 2000; i++) s.guncelle(200, 0, 0, 0, 0, DUZ_AZ, b, 0.01f);
        const float suzgecli = s.pitch();

        Ayar saf{1.0f, 0.1f, 0.05f};
        Suzgec s2(saf);
        for (int i = 0; i < 2000; i++) s2.guncelle(200, 0, 0, 0, 0, DUZ_AZ, b, 0.01f);

        t::dogru("filtresiz drift cok daha buyuk", s2.pitch() > suzgecli * 3.0f);
        t::dogru("filtreli aci sinirli kaliyor", suzgecli < 5.0f);
        // Ivmeolcerin surekli %4'luk katkisi jiroskobun driftini sifira cekiyor.
    }

    // ── ⚠ YAW DUZELTILEMEZ ──────────────────────────────────────────────
    {
        Bias b; b.bitir();
        Suzgec s(a);
        for (int i = 0; i < 2000; i++) s.guncelle(0, 0, 200, 0, 0, DUZ_AZ, b, 0.01f);
        t::dogru("yaw serbestce drift ediyor", std::fabs(s.yaw()) > 10.0f);
        // Yercekimi Z etrafindaki donuse gore degismedigi icin ivmeolcerin
        // yaw hakkinda soyleyecegi bir sey yok. Cozum manyetometre.
    }

    // ── dt korumasi ─────────────────────────────────────────────────────
    {
        Bias b; b.bitir();
        Ayar saf{1.0f, 0.1f, 0.05f};
        Suzgec s(saf);
        s.guncelle(1640, 0, 0, 0, 0, DUZ_AZ, b, 5.0f);    // 100 derece/s, takilma: 5 s
        t::dogru("asiri dt yedek degere cekildi", yakin(s.pitch(), 5.0f, 0.1f));
        // Korunmasaydi tek karede 500 derece firlardi.
        s.sifirla();
        s.guncelle(1640, 0, 0, 0, 0, DUZ_AZ, b, 0.0f);
        t::dogru("sifir dt de yedege duser", yakin(s.pitch(), 5.0f, 0.1f));
    }
    return t::rapor("yonelim");
}
