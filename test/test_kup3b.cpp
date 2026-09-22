// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
#include "test.h"
#include "../firmware/orientation_cube/src/kup3b.h"
#include <cmath>
using namespace kup3b;

int main() {
    const Izdusum iz{50.0f, 4.0f, 64, 64};

    // ── donus yokken simetri ────────────────────────────────────────────
    {
        Nokta2B k[KOSE];
        kup_yansit(0, 0, 0, iz, k);
        // On yuz (z=-1) arka yuzden (z=+1) BUYUK gorunmeli: perspektif.
        const int on_en  = k[1].x - k[0].x;   // z = -1 kenari
        const int arka_en= k[5].x - k[4].x;   // z = +1 kenari
        t::dogru("yakin yuz daha genis (perspektif)", on_en > arka_en);
        t::esit("merkez korunuyor", 128, k[0].x + k[1].x);
    }

    // ── ortografik olsaydi iki kenar esit olurdu ────────────────────────
    {
        Izdusum uzak{50.0f, 400.0f, 64, 64};   // mesafe cok buyuk ~ ortografik
        Nokta2B k[KOSE];
        kup_yansit(0, 0, 0, uzak, k);
        t::esit("uzakta perspektif kayboluyor",
                k[1].x - k[0].x, k[5].x - k[4].x);
    }

    // ── odak buyuyunce kup buyur ────────────────────────────────────────
    {
        Nokta2B a[KOSE], b[KOSE];
        kup_yansit(0, 0, 0, iz, a);
        Izdusum buyuk = iz; buyuk.odak = 100.0f;
        kup_yansit(0, 0, 0, buyuk, b);
        t::dogru("odak iki katinda kup daha genis",
                 (b[1].x - b[0].x) > (a[1].x - a[0].x));
    }

    // ── 360 derece donus baslangica donuyor ─────────────────────────────
    {
        Nokta2B a[KOSE], b[KOSE];
        kup_yansit(0, 0, 0, iz, a);
        kup_yansit(2.0f * 3.14159265f, 0, 0, iz, b);
        t::dogru("tam tur ayni yere geliyor",
                 std::abs(a[0].x - b[0].x) <= 1 && std::abs(a[0].y - b[0].y) <= 1);
    }

    // ── arka yuz eleme ──────────────────────────────────────────────────
    {
        Nokta2B k[KOSE];
        kup_yansit(0, 0, 0, iz, k);
        int gorunen = 0;
        for (int i = 0; i < YUZEY; i++) if (gorunur(k, i)) gorunen++;
        t::dogru("6 yuzeyin bir kismi eleniyor", gorunen > 0 && gorunen < YUZEY);
        // Elenmeseydi arka yuzeyler on yuzeyleri uzerine cizerdi.
    }
    {
        // Kupu cevirince gorunen yuzey kumesi DEGISMELI.
        Nokta2B a[KOSE], b[KOSE];
        kup_yansit(0, 0, 0, iz, a);
        kup_yansit(3.14159265f, 0, 0, iz, b);
        bool fark = false;
        for (int i = 0; i < YUZEY; i++) if (gorunur(a, i) != gorunur(b, i)) fark = true;
        t::dogru("180 derece donunce gorunen yuzeyler degisti", fark);
    }

    // ── capraz carpim isareti ───────────────────────────────────────────
    {
        Nokta2B p0{0,0}, p1{10,0}, p2{10,10};
        t::dogru("saat yonu pozitif", yuzey_isareti(p0,p1,p2) > 0);
        t::dogru("ters yon negatif",  yuzey_isareti(p0,p2,p1) < 0);
    }

    // ── payda korumasi: kup gozlemcinin arkasina gecerse ────────────────
    {
        Izdusum yakin{50.0f, 0.5f, 64, 64};    // mesafe < kup yaricapi
        Nokta2B k[KOSE];
        kup_yansit(0, 0, 0, yakin, k);
        bool makul = true;
        for (int i = 0; i < KOSE; i++)
            if (std::abs(k[i].x) > 100000 || std::abs(k[i].y) > 100000) makul = false;
        t::dogru("payda korumasi sonsuza gitmeyi engelledi", makul);
    }
    return t::rapor("kup3b");
}
