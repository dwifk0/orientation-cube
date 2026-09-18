// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Ahmet Efe Nezli

#include "kup3b.h"
#include <math.h>

namespace kup3b {

const float KOSELER[KOSE][3] = {
    {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
    {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
};

const int YUZEYLER[YUZEY][4] = {
    {0, 1, 2, 3}, {4, 5, 6, 7}, {0, 4, 5, 1},
    {1, 5, 6, 2}, {2, 6, 7, 3}, {3, 7, 4, 0}
};

Nokta2B yansit(float x, float y, float z,
               float ax, float ay, float az, const Izdusum& iz) {
    // X ekseni etrafinda (pitch)
    const float cx = cosf(ax), sx = sinf(ax);
    const float y1 =  y * cx - z * sx;
    const float z1 =  y * sx + z * cx;

    // Y ekseni etrafinda (roll)
    const float cy = cosf(ay), sy = sinf(ay);
    const float x2 =  x * cy + z1 * sy;
    const float z2 = -x * sy + z1 * cy;

    // Z ekseni etrafinda (yaw)
    const float cz = cosf(az), sz = sinf(az);
    const float x3 = x2 * cz - y1 * sz;
    const float y3 = x2 * sz + y1 * cz;

    // Perspektif bolme. Payda sifira yaklasirsa nokta sonsuza gider;
    // kup gozlemcinin ARKASINA gecerse isaret de doner. Kucuk bir taban
    // degeriyle korunuyor.
    float payda = z2 + iz.mesafe;
    if (payda < 0.1f) payda = 0.1f;
    const float faktor = iz.odak / payda;

    Nokta2B p;
    p.x = (int)(x3 * faktor) + iz.merkez_x;
    p.y = (int)(y3 * faktor) + iz.merkez_y;
    return p;
}

void kup_yansit(float ax, float ay, float az,
                const Izdusum& iz, Nokta2B* cikti) {
    for (int i = 0; i < KOSE; i++)
        cikti[i] = yansit(KOSELER[i][0], KOSELER[i][1], KOSELER[i][2],
                          ax, ay, az, iz);
}

long yuzey_isareti(const Nokta2B& p0, const Nokta2B& p1, const Nokta2B& p2) {
    return (long)(p1.x - p0.x) * (long)(p2.y - p0.y)
         - (long)(p1.y - p0.y) * (long)(p2.x - p0.x);
}

bool gorunur(const Nokta2B* k, int i) {
    const int* y = YUZEYLER[i];
    return yuzey_isareti(k[y[0]], k[y[1]], k[y[2]]) > 0;
}

}  // namespace kup3b
