# 🏥 Hastane Randevu ve Hasta Öncelik Sistemi

**BMT210 Veri Yapıları** dersi projesi.  
C dili ile yazılmış HTTP sunucusu + HTML/CSS/Vanilla JS web arayüzü.

---

## Proje Özeti

Gerçek bir hastane senaryosunu modelleyen, 5 farklı veri yapısının birlikte kullanıldığı tam işlevli bir yönetim sistemi. Projenin odak noktası görsel arayüz değil, veri yapılarının doğru ve performanslı uygulanmasıdır.

---

## Kullanılan Veri Yapıları

| Modül | Veri Yapısı | Neden Seçildi | Temel Operasyon |
|---|---|---|---|
| Hasta Kaydı | **Hash Table** (Chaining) | O(1) ortalama CRUD | TC → hasta eşlemi |
| Acil Triaj | **Min-Heap** | O(log n) öncelikli çıkarma | Kök = en kritik hasta |
| Randevu Kuyruğu | **Queue** (Linked List) | FIFO, O(1) enqueue/dequeue | İlk gelen ilk muayene |
| İşlem Geçmişi | **Stack** (Linked List) | LIFO, O(1) push/pop | "Geri Al" özelliği |
| Poliklinik Yapısı | **N-ary Tree** | Hiyerarşik modelleme | DFS gezinti, ebeveyn O(1) |

### Hash Table — Chaining ile Çakışma Çözümü
- **Hash fonksiyonu:** Polynomial rolling hash — `Σ(tc[i] × 31^i) mod 1009`
- **Bucket boyutu:** 1009 (asal sayı — homojen dağılım için)
- **Çakışma:** Aynı bucket'ta linked list zinciri oluşturulur
- **Yük faktörü:** `toplam_hasta / 1009` — dashboard'da canlı gösterilir
- **Karşılaştırma:** Performance testinde Linked List aramasına karşı benchmark yapılır

### Min-Heap — Dizi Tabanlı Öncelik Kuyruğu
- **Dizi indeks ilişkisi:** Ebeveyn(i) = `(i−1)/2`, Çocuklar = `2i+1`, `2i+2`
- **Sift-up:** Ekleme sonrası heap özelliğini yukarı doğru düzeltir — O(log n)
- **Sift-down:** Kök çıkarıldıktan sonra aşağı düzeltir — O(log n)
- **Kök garantisi:** Her zaman en düşük aciliyet skoru = en kritik hasta

### Queue — Çift Pointer FIFO
- **`on` pointer:** Dequeue tarafı — O(1) çıkarma
- **`arka` pointer:** Enqueue tarafı — O(1) ekleme
- **İptal:** TC'ye göre O(n) arama + O(1) zincir güncelleme

### Stack — LIFO Geri Alma
- **Push/Pop:** O(1) — tepe pointer'ı değiştirmek yeterli
- **Undo data:** Her işlemin geri alınabilmesi için JSON formatında yedek veri saklanır
- Desteklenen geri almalar: hasta ekleme/silme/güncelleme, randevu alma, poliklinik ekleme

### N-ary Tree — Hiyerarşik Poliklinik Yapısı
- **Her düğüm:** `ad`, `cocuklar[MAX_COCUK]`, `cocuk_sayisi`, `ebeveyn` pointer
- **Ebeveyn pointer:** Silme işleminde O(1) üst birime erişim sağlar
- **DFS arama:** O(n) — isme göre düğüm bulma
- **JSON serialization:** Özyinelemeli tek geçiş — O(n)
- **Alternatifler ve neden seçilmedi:**
  - *Düz liste (parent_id):* Alt birim bulmak O(n) tekrarlı arama gerektirir
  - *HashMap:* Ebeveyn bilgisi kaybolur, silmede ayrı tarama şart
  - *Nested Sets:* Ekleme/silmede tüm tabloyu günceller — O(n) maliyet
  - *Graf:* Döngü kontrolü gerektirir, ağaç için fazla karmaşık

---

## Proje Dosya Yapısı

```
hastane-sistemi/
├── backend/
│   ├── main.c              → HTTP sunucu (POSIX socket), tüm API handler'ları
│   ├── hash_table.c / .h   → Hasta kaydı — Hash Table + Chaining
│   ├── heap.c / .h         → Acil triaj — Min-Heap
│   ├── queue.c / .h        → Normal randevu — Queue (Linked List)
│   ├── stack.c / .h        → İşlem geçmişi — Stack (Linked List)
│   ├── tree.c / .h         → Poliklinik hiyerarşisi — N-ary Tree
│   └── Makefile
├── frontend/
│   ├── index.html          → Tek sayfa uygulama (6 bölüm)
│   ├── style.css           → Premium medikal UI (Deep Navy + Medical Teal)
│   └── app.js              → Fetch API + DOM + SVG grafikleri
├── data/
│   ├── hastalar.txt        → 100 başlangıç hasta kaydı (sunucu açılışta yükler)
│   └── randevular.txt      → Örnek randevu verileri
├── tests/
│   └── performance_test.c  → 3 karşılaştırmalı benchmark testi
└── README.md
```

---

## Kurulum ve Çalıştırma

### Gereksinimler
- GCC (`gcc --version` ile kontrol edin)
- POSIX uyumlu işletim sistemi: macOS veya Linux
- `make` aracı

### 1. Derleme

```bash
cd backend
make
```

Başarılı çıktı:
```
Derleme tamamlandi: ./hastane_server
Calistirmak icin: ./hastane_server 8080
```

### 2. Sunucuyu Başlatma

```bash
./hastane_server 8080
```

```
========================================
  Hastane Sistemi Sunucu Baslatildi
  Port    : 8080
  Adres   : http://localhost:8080
========================================
[Bilgi] 100 hasta yuklendi: ../data/hastalar.txt
```

### 3. Tarayıcıda Açma

```
http://localhost:8080
```

### 4. Performans Testleri

```bash
cd tests
gcc -O2 -o performance_test performance_test.c
./performance_test
```

---

## API Endpoint'leri

### Genel

```
GET  /api/dashboard       → Canlı istatistikler + kuyruk listesi (JSON)
GET  /api/istatistikler   → Hash Table yük analizi, poliklinik dağılımı,
                            triaj skor histogramı, heap/stack dolulukları
```

### Hasta Yönetimi (Hash Table)

```
POST   /api/hasta          → Hasta ekle            O(1) ort.
GET    /api/hasta?tc=...   → TC ile hasta ara       O(1) ort.
GET    /api/hastalar       → Tüm hastaları listele  O(n)
PUT    /api/hasta/:tc      → Hasta güncelle         O(1) ort.
DELETE /api/hasta/:tc      → Hasta sil              O(1) ort.
```

### Randevu Kuyruğu (Queue)

```
POST   /api/randevu          → Kuyruğa ekle (enqueue)    O(1)
GET    /api/randevu/kuyruk   → Kuyruğu görüntüle          O(n)
POST   /api/randevu/cagir    → Sıradakini çağır (dequeue) O(1)
DELETE /api/randevu/:tc      → Randevu iptal              O(n)
```

### Acil Triaj (Min-Heap)

```
POST   /api/triaj          → Triaja ekle (skor: 1–10)  O(log n)
GET    /api/triaj/kuyruk   → Triaj listesi             O(n)
POST   /api/triaj/cagir    → En acili çağır (pop)       O(log n)
```

### Poliklinik Yönetimi (N-ary Tree)

```
GET    /api/poliklinikler      → Ağaç yapısı (JSON)        O(n)
POST   /api/poliklinik         → Yeni birim ekle            O(n bul) + O(1 ekle)
DELETE /api/poliklinik/:ad     → Birim + alt ağacı sil      O(n bul) + O(k sil)
```

### İşlem Geçmişi (Stack)

```
GET    /api/gecmis            → Son 50 işlem          O(50)
POST   /api/gecmis/geri-al    → Son işlemi geri al    O(1) pop + O(1) undo
```

---

## Web Arayüzü — Bölüm Detayları

### Dashboard
Her 5 saniyede `/api/dashboard` ve `/api/istatistikler` polling yapılır.

| Widget | Açıklama |
|---|---|
| 4 istatistik kartı | Toplam hasta, randevu kuyruğu, triaj, bugünkü işlem |
| Canlı kuyruk tabloları | Normal FIFO + Triaj Min-Heap anlık görünüm |
| **Hash Table Gauge** | SVG daire grafik — yük faktörü (%) canlı |
| **Bucket Histogram** | 0–9+ uzunluklu zincirlerin dağılımı |
| **Poliklinik Bar Chart** | Her birime düşen hasta sayısı (yatay çubuk) |
| **Triaj Skor Chart** | 1–10 skor dağılımı (dikey, kırmızı/sarı/yeşil) |
| Heap / Stack doluluk | Progress bar göstergeleri |

### Hasta Yönetimi
- Yeni hasta kayıt formu (hash table'a ekler)
- TC ile O(1) hızlı arama — kart görünümü
- Tüm hasta listesi tablosu (güncelle / sil)
- Güncelleme formunda undo verisi Stack'e kaydedilir

### Randevu Kuyruğu
- Kuyruğa hasta ekleme (enqueue — O(1))
- "Sıradaki Hastayı Çağır" butonu (dequeue — O(1))
- Randevu iptal TC ile (O(n) arama)
- Mevcut FIFO sırası listesi

### Acil Triaj
- 1–10 slider ile aciliyet skoru; skor göstergesi gerçek zamanlı renk değiştirir
- Renk kodlu liste: 🔴 Kritik (1–3), 🟡 Orta (4–6), 🟢 Stabil (7–10)
- "En Acil Hastayı Çağır" → Min-Heap pop, kök döndürülür

### Poliklinikler — SVG Ağaç Görselleştirmesi
Algoritmik düzen hesabı:
1. **yaprakSay(n)** — Alt ağaç yaprak sayısı (bottom-up DFS) → y boyutunu belirler
2. **konumAta(n, seviye, y0)** — x = seviye × adım; y = çocuk cy'lerinin ortası
3. **SVG render** — Kenarlar (bezier), düğümler (iç içe `<g>` çiftleri)

```
KÖK             ANA BİRİM       POLİKLİNİK
─ ─ ─ ─ ─       ─ ─ ─ ─ ─ ─    ─ ─ ─ ─ ─ ─ ─

■ Hastane ──────■ Dahiliye ─────□ Kardiyoloji  ●
(lacivert)      (teal)      ├───□ Noroloji      ●
                             └───□ Gastro...     ●
                ─■ Cerrahi  ────□ Genel Cerrahi ●
                             └───□ Ortopedi      ●
                ─■ Acil     ────□ Travma         ●
                             └───□ Kardiyak Acil ●
```

**CSS/SVG transform çakışması ve çözümü:**  
CSS `transform:scale()` animasyonu, SVG `transform=translate(x,y)` attribute'ünü
aynı `<g>` elementinde ezip tüm düğümleri (0,0)'a yığıyordu.
Çözüm: İki iç içe `<g>` — dış SVG attribute ile konum, iç CSS ile animasyon.

### İşlem Geçmişi
- Son 50 işlemin tablosu — Stack tepe'den iterator olarak okunur
- "Son İşlemi Geri Al" — Stack pop + işlem tipine göre undo uygulanır
- İşlem tipleri renkli rozetlerle gösterilir

---

## Performans Testi Çıktısı

```
=== TEST 1: ARAMA — Hash Table vs Linked List (100 arama) ===
Veri    | Hash Table  | Linked List  | Oran
100     | ~0.001 ms   | ~0.045 ms    | ×45
1000    | ~0.001 ms   | ~0.412 ms    | ×412
10000   | ~0.002 ms   | ~4.231 ms    | ×2115

=== TEST 2: ÖNCELİKLENDİRME — Min-Heap vs Sıralı Dizi ===
Veri    | Min-Heap Ekle | Sıralı Dizi Ekle | Oran
1000    | ~0.12 ms      | ~0.29 ms         | ×2
10000   | ~1.06 ms      | ~22.60 ms        | ×21

=== TEST 3: SIRALAMA — Quick Sort vs Bubble Sort ===
Veri    | Quick Sort  | Bubble Sort  | Oran
1000    | ~0.21 ms    | ~7.46 ms     | ×36
10000   | ~2.94 ms    | çok uzun     | —
```

---

## Teknik Detaylar

### HTTP Sunucu
- POSIX socket API ile sıfırdan yazıldı (libmicrohttpd gerekmez)
- Her bağlantı için `pthread_create` ile thread açılır
- Tek global `pthread_mutex_t` tüm veri yapılarını korur
- Statik dosyalar `../frontend/` dizininden servis edilir
- JSON tamamen manuel oluşturulur (harici kütüphane yok)
- CORS ve OPTIONS preflight desteği

### Derleme Seçenekleri
```
CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -std=c99
```

### Başlangıç Verisi Formatı (`hastalar.txt`)
```
# TC,AD,SOYAD,YAS,POLİKLİNİK,TELEFON
10000000001,Ahmet,Yilmaz,45,Kardiyoloji,05321234567
```

---

*BMT210 Veri Yapıları — Muhammet Eren Demirtaş*
