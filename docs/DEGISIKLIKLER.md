# Kodun elden geçirilmesi — ne değişti, neden

Orijinal tek dosyalık sketch ([`PROJE_RAPORU.md`](PROJE_RAPORU.md) §8) çalışıyordu;
bu revizyon davranışı korurken yapıyı ve birkaç gerçek kusuru düzeltiyor.

## 1. Yapı: tek dosya → donanım katmanı + saf çekirdek

| Önce | Sonra |
|---|---|
| tek `.ino` içinde her şey | `src/yonelim.*` · `src/kup3b.*` · `orientation_cube.ino` |

`src/` altındaki iki modül `Wire`, `SPI`, `tft` ya da `millis` çağırmıyor —
ham sensör verisi ve geçen süre parametre olarak geliyor. Sonuç: **aynı kod hem
ESP32'de koşuyor hem masaüstünde test ediliyor.** 28 birim test kartsız çalışıyor.

## 2. Düzeltilen gerçek kusurlar

**Filtre durumu paylaşılan değişken üzerinden okunuyordu.** Orijinalde
`gyroPitch = (angleX * 57.2958) + gx*dt` satırı, filtrenin kendi çıktısını
çekirdekler arası `volatile` değişkenden geri okuyordu. Her tikte radyan↔derece
gidip geliyor, üstelik filtre durumu diğer çekirdeğin görebildiği bir yere
bağlanmış oluyordu. Artık durum modülün içinde derece cinsinden duruyor;
paylaşılan değişkene yalnızca **sonuç yazılıyor**.

**`while(1)` watchdog resetine yol açıyordu.** Sensör hatası ekranında boş
sonsuz döngü vardı; ESP32'de bu görev zamanlayıcısını aç bırakır ve kart kendini
resetler — yani "sensör hatası" ekranı hiç görülmeden boot döngüsüne girilir.
`for(;;) delay(1000);` oldu.

**Perspektif paydası korumasızdı.** `faktor = 50.0 / (z + 4.0)` ifadesinde küp
gözlemciye yeterince yaklaşırsa payda sıfıra gider, koordinatlar taşar. Küçük
bir taban değeriyle korundu; test bunu ayrıca doğruluyor.

**Sihirli sayılar adlandırıldı.** `57.2958`, `0.0174533`, `131.2`, `16384.0`
artık `RAD_DERECE`, `DERECE_RAD`, `JIRO_LSB`, `IVME_LSB`. İkisinin nereden
geldiği (`32768/250`, `32768/2`) başlıkta yazılı.

**Δt koruması genişletildi.** Orijinalde yalnız `dt > 0.1` kontrol ediliyordu;
`dt == 0` (aynı milisaniyede iki okuma) durumu açıkta kalıyordu. İkisi de aynı
yedek değere düşüyor.

## 3. Jiroskop ölçeği yanlıştı — "hassasiyet çarpanı" bunu örtüyordu

Orijinal kodda ham jiroskop değeri `131.2`'ye bölünüyordu. Bu, BMI160'ın
**±250 °/s** aralığının katsayısı. Ama `DFRobot_BMI160` kütüphanesi
`I2cInit()` sırasında jiroskobu **±2000 °/s** aralığına kuruyor; oranın
katsayısı **16,4 LSB/(°/s)**. Yani ölçülen açısal hızlar **8 kat küçüktü.**

Bu hata farkında olmadan başka bir ayarla telafi ediliyordu:

| | Orijinal çarpan | 1/8 ölçekle net etki |
|---|---|---|
| pitch, roll | `HIZ_CARPANI = 16` | **2×** fazla |
| yaw | `HIZ_CARPANI / 2 = 8` | **1×** — tam doğru |

Belgedeki *"90° döndürünce 180° dönüyor, yaw ayrıca ikiye bölündü"* notu da
bunun izi: 16 kat yaw'da 2 kat fazla çıkınca deneyerek 8'e indirilmiş ve
tesadüfen doğru değere oturmuş. Pitch/roll'daki 2 kat fazlalık ise
ivmeölçerin sürekli katkısıyla kısmen toparlanıyordu, o yüzden göze batmadı.

**Düzeltme:** `JIRO_LSB = 16.4`, çarpan kaldırıldı (`GORSEL_KAZANC = 1.0`).
Artık küp kartla **aynı açıda** dönüyor — belgedeki temel çıktı tanımı bu.
Bir birim test katsayının ±2000 °/s aralığına ait olduğunu sabitliyor;
131,2'ye geri dönülürse altı test kırılıyor.

⚠ Bu düzeltme kütüphane kaynağından ve hesapla doğrulandı, **kart üzerinde
henüz denenmedi.** Eksen işaretleri (hangi yöne dönüş pozitif) montaja bağlı
ve orijinal haritalama korundu.

## 4. Bilerek dokunmadıklarım

**Her karede tam ekran temizleme.** `fillScreen` + yeniden çizim titremeye yol
açıyor; `f6.ino`'da denediğin fark tabanlı çizim (`prevProjected`) bunu
azaltıyordu ama dolu yüzeylerle birlikte çalışmıyor. Doğru çözüm belgede de
yazdığın **çift tamponlama** (128×128×2 = 32 KB, ESP32'de mümkün). Bu bir
geliştirme, bir düzeltme değil — sen karar ver.

**Eksen haritalaması ve işaretler.** Montaj yönüne bağlı; kart elde olmadan
doğrulanamaz. Orijinaldeki haritalama korundu.

## 5. İkinci yazar

[`PROJE_RAPORU.md`](PROJE_RAPORU.md) içindeki "Hazırlayanlar" satırı ve HUD'daki
ikinci isim, izin alınmış olarak kaldırıldı. Yeni kodda HUD'da isim hiç yok —
ekran alanı açı göstergelerine kaldı.
