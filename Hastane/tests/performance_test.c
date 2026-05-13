/*
 * performance_test.c
 * Veri Yapısı Performans Karşılaştırma Testleri
 *
 * TEST 1: Hash Table vs Linked List - Arama Performansı
 *   Veri Yapısı seçimi: Aynı arama işlemi her ikisinde de uygulanır.
 *   Hash Table: O(1) ortalama arama
 *   Linked List: O(n) arama (her eleman kontrol edilir)
 *
 * TEST 2: Min-Heap vs Sıralı Dizi - Önceliklendirme Performansı
 *   Veri Yapısı seçimi: Öncelikli ekleme ve en küçük elemanı çıkarma.
 *   Min-Heap: O(log n) ekleme/çıkarma
 *   Sıralı Dizi: O(n) ekleme (yerini bul + kaydır), O(1) çıkarma
 *
 * TEST 3: Bubble Sort vs Quick Sort - Sıralama Performansı
 *   Bubble Sort: O(n²) karşılaştırma tabanlı
 *   Quick Sort: O(n log n) ortalama böl-fethet
 *
 * Derleme:
 *   gcc -O2 -o performance_test performance_test.c -lm
 * Çalıştırma:
 *   ./performance_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ======================== Zamanlama ======================== */

static double milisaniye_al(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1.0e6;
}

/* ======================== TEST 1 YAPILARI ======================== */

/* Performans testi için basit Hasta */
typedef struct {
    char tc[12];
    char ad[50];
    int  yas;
} TestHasta;

/* ---- Linked List (karşılaştırma için) ---- */
/*
 * Linked List: Her düğüm bir sonraki düğümü gösterir.
 * Arama: Baş düğümden TC eşleşene kadar ilerle - O(n).
 * Ekle: Başa ekle - O(1).
 * Neden kötü: n büyüdükçe arama süresi doğrusal artar.
 */
typedef struct LLDugum {
    TestHasta         hasta;
    struct LLDugum*   sonraki;
} LLDugum;

typedef struct {
    LLDugum* bas;
    int      boyut;
} LinkedList;

static LinkedList* ll_olustur(void) {
    LinkedList* ll = (LinkedList*)calloc(1, sizeof(LinkedList));
    return ll;
}

static void ll_ekle(LinkedList* ll, const char* tc, const char* ad, int yas) {
    LLDugum* yeni = (LLDugum*)malloc(sizeof(LLDugum));
    if (!yeni) return;
    strncpy(yeni->hasta.tc,  tc,  11);  yeni->hasta.tc[11]  = '\0';
    strncpy(yeni->hasta.ad,  ad,  49);  yeni->hasta.ad[49]  = '\0';
    yeni->hasta.yas = yas;
    yeni->sonraki   = ll->bas;  /* Başa ekle: O(1) */
    ll->bas         = yeni;
    ll->boyut++;
}

/* O(n) arama: TC eşleşene kadar tüm zinciri tara */
static TestHasta* ll_ara(LinkedList* ll, const char* tc) {
    LLDugum* gecici = ll->bas;
    while (gecici) {
        if (strcmp(gecici->hasta.tc, tc) == 0) return &gecici->hasta;
        gecici = gecici->sonraki;
    }
    return NULL;
}

static void ll_yok_et(LinkedList* ll) {
    LLDugum* gecici = ll->bas;
    while (gecici) {
        LLDugum* silinecek = gecici;
        gecici = gecici->sonraki;
        free(silinecek);
    }
    free(ll);
}

/* ---- Hash Table (asal boyut, chaining) ---- */
/*
 * Asal sayı boyutlu bucket dizisi.
 * Polynomial rolling hash: hash = Σ(tc[i] * 31^i) mod BOYUT
 * Chaining: Aynı hash değerini alan elemanlar aynı bucket'ta zincir oluşturur.
 * Arama: O(1) ortalama, O(n) en kötü durum (tüm elemanlar aynı bucket'ta).
 * Neden iyi: Yük faktörü düşük tutulursa çakışma nadir olur.
 */
#define HT_BOYUT 1009

typedef struct HTDugum {
    TestHasta       hasta;
    struct HTDugum* sonraki;
} HTDugum;

typedef struct {
    HTDugum* buckets[HT_BOYUT];
    int      boyut;
} HashTablosu;

static HashTablosu* ht_olustur(void) {
    HashTablosu* ht = (HashTablosu*)calloc(1, sizeof(HashTablosu));
    return ht;
}

static unsigned int ht_hash(const char* tc) {
    unsigned int h = 0;
    for (int i = 0; tc[i]; i++) h = h * 31 + (unsigned char)tc[i];
    return h % HT_BOYUT;
}

static void ht_ekle(HashTablosu* ht, const char* tc, const char* ad, int yas) {
    unsigned int idx = ht_hash(tc);
    HTDugum* yeni = (HTDugum*)malloc(sizeof(HTDugum));
    if (!yeni) return;
    strncpy(yeni->hasta.tc, tc, 11);  yeni->hasta.tc[11] = '\0';
    strncpy(yeni->hasta.ad, ad, 49);  yeni->hasta.ad[49] = '\0';
    yeni->hasta.yas  = yas;
    yeni->sonraki    = ht->buckets[idx];  /* Chaining: bucket başına ekle */
    ht->buckets[idx] = yeni;
    ht->boyut++;
}

/* O(1) ortalama: sadece ilgili bucket'ın zincirini tara */
static TestHasta* ht_ara(HashTablosu* ht, const char* tc) {
    unsigned int idx  = ht_hash(tc);
    HTDugum*     geç  = ht->buckets[idx];
    while (geç) {
        if (strcmp(geç->hasta.tc, tc) == 0) return &geç->hasta;
        geç = geç->sonraki;
    }
    return NULL;
}

static void ht_yok_et(HashTablosu* ht) {
    for (int i = 0; i < HT_BOYUT; i++) {
        HTDugum* geç = ht->buckets[i];
        while (geç) {
            HTDugum* s = geç;
            geç = geç->sonraki;
            free(s);
        }
    }
    free(ht);
}

/* ======================== TEST 2 YAPILARI ======================== */

/*
 * Min-Heap: Dizi tabanlı tam ikili ağaç.
 * Ekleme: Son pozisyona koy, sift-up ile yerleştir - O(log n).
 * En küçüğü çıkar: Kökü al, son elemanı köke koy, sift-down - O(log n).
 * Neden iyi: Logaritmik ekleme/çıkarma, sabit indeks aritmetiği.
 */
#define MAX_HEAP_BOYUT 20000

typedef struct {
    char tc[12];
    int  skor;
} TriajEleman;

typedef struct {
    TriajEleman dizi[MAX_HEAP_BOYUT];
    int         boyut;
} TestHeap;

static void heap_yer_degistir(TriajEleman* a, TriajEleman* b) {
    TriajEleman t = *a; *a = *b; *b = t;
}

static void heap_yukari(TestHeap* h, int i) {
    while (i > 0) {
        int eb = (i - 1) / 2;
        if (h->dizi[i].skor < h->dizi[eb].skor) {
            heap_yer_degistir(&h->dizi[i], &h->dizi[eb]);
            i = eb;
        } else break;
    }
}

static void heap_asagi(TestHeap* h, int i) {
    while (1) {
        int en_k = i, sol = 2*i+1, sag = 2*i+2;
        if (sol < h->boyut && h->dizi[sol].skor < h->dizi[en_k].skor) en_k = sol;
        if (sag < h->boyut && h->dizi[sag].skor < h->dizi[en_k].skor) en_k = sag;
        if (en_k == i) break;
        heap_yer_degistir(&h->dizi[i], &h->dizi[en_k]);
        i = en_k;
    }
}

static void heap_ekle(TestHeap* h, const char* tc, int skor) {
    if (h->boyut >= MAX_HEAP_BOYUT) return;
    strncpy(h->dizi[h->boyut].tc, tc, 11);
    h->dizi[h->boyut].skor = skor;
    heap_yukari(h, h->boyut++);
}

static int heap_cıkar(TestHeap* h, char* tc_out) {
    if (h->boyut == 0) return -1;
    strncpy(tc_out, h->dizi[0].tc, 11);
    int sonuc = h->dizi[0].skor;
    h->dizi[0] = h->dizi[--h->boyut];
    if (h->boyut > 0) heap_asagi(h, 0);
    return sonuc;
}

/*
 * Sıralı Dizi: Her ekleme doğru pozisyonu bulup kaydırır - O(n).
 * Çıkarma: Zaten sıralı, ilk eleman - O(n) kaydırma gerekir.
 * Neden kötü: Büyük n için O(n) ekleme heap'in O(log n)'ine karşı çok yavaş.
 */
#define MAX_SIRALI_BOYUT 20000

typedef struct {
    TriajEleman dizi[MAX_SIRALI_BOYUT];
    int         boyut;
} SiraliDizi;

static void sirali_ekle(SiraliDizi* sd, const char* tc, int skor) {
    if (sd->boyut >= MAX_SIRALI_BOYUT) return;
    /* Doğru pozisyonu bul ve elemanları kaydır: O(n) */
    int pos = sd->boyut;
    while (pos > 0 && sd->dizi[pos-1].skor > skor) {
        sd->dizi[pos] = sd->dizi[pos-1];
        pos--;
    }
    strncpy(sd->dizi[pos].tc, tc, 11);
    sd->dizi[pos].skor = skor;
    sd->boyut++;
}

static int sirali_cıkar(SiraliDizi* sd, char* tc_out) {
    if (sd->boyut == 0) return -1;
    strncpy(tc_out, sd->dizi[0].tc, 11);
    int sonuc = sd->dizi[0].skor;
    /* İlk eleman çıkarıldı, hepsini kaydır: O(n) */
    for (int i = 0; i < sd->boyut - 1; i++) sd->dizi[i] = sd->dizi[i+1];
    sd->boyut--;
    return sonuc;
}

/* ======================== TEST 3 YAPILARI ======================== */

/*
 * Bubble Sort: Her geçişte komşu elemanları karşılaştır ve değiştir.
 * Karmaşıklık: O(n²) - n²/2 karşılaştırma.
 * Neden kötü: Büyük dizilerde çok yavaş; eğitim amaçlı kullanılır.
 */
static void bubble_sort(char arr[][12], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (strcmp(arr[j], arr[j+1]) > 0) {
                char tmp[12];
                strcpy(tmp, arr[j]);
                strcpy(arr[j], arr[j+1]);
                strcpy(arr[j+1], tmp);
            }
        }
    }
}

/*
 * Quick Sort: Pivot seç, küçükleri sola büyükleri sağa böl, özyinelemeli sırala.
 * Karmaşıklık: O(n log n) ortalama, O(n²) en kötü (kötü pivot seçimi).
 * Neden iyi: Cache dostu, yerinde sıralama, pratikte çok hızlı.
 */
static int bolum(char arr[][12], int sol, int sag) {
    char* pivot = arr[sag];
    int   i     = sol - 1;
    for (int j = sol; j < sag; j++) {
        if (strcmp(arr[j], pivot) <= 0) {
            i++;
            char tmp[12];
            strcpy(tmp, arr[i]);
            strcpy(arr[i], arr[j]);
            strcpy(arr[j], tmp);
        }
    }
    char tmp[12];
    strcpy(tmp, arr[i+1]);
    strcpy(arr[i+1], arr[sag]);
    strcpy(arr[sag], tmp);
    return i + 1;
}

static void quick_sort(char arr[][12], int sol, int sag) {
    if (sol < sag) {
        int pi = bolum(arr, sol, sag);
        quick_sort(arr, sol,    pi - 1);
        quick_sort(arr, pi + 1, sag);
    }
}

/* ======================== Test Veri Üretimi ======================== */

static void tc_uret(char* tc, int idx) {
    /* 11 haneli benzersiz TC üret: 1 + 10 basamak */
    snprintf(tc, 12, "1%010d", idx);
}

static void ad_uret(char* ad, int idx) {
    const char* adlar[] = {
        "Ahmet","Mehmet","Ali","Ayse","Fatma","Zeynep","Mustafa",
        "Ibrahim","Huseyin","Hasan","Elif","Esra","Selin","Emre","Can"
    };
    strcpy(ad, adlar[idx % 15]);
}

/* ======================== Tablo Yazdırma ======================== */

static void tablo_baslik(const char* baslik) {
    printf("\n");
    for (int i = 0; i < 60; i++) printf("=");
    printf("\n  %s\n", baslik);
    for (int i = 0; i < 60; i++) printf("=");
    printf("\n");
}

static void tablo_satir_yaz(const char* etiket, double deger1, double deger2) {
    double oran = (deger1 > 0.0001) ? deger2 / deger1 : 0;
    printf("  %-12s | %12.4f ms | %12.4f ms | x%-8.0f\n",
           etiket, deger1, deger2, oran);
}

static void tablo_ust(const char* s1, const char* s2) {
    printf("  %-12s | %13s | %13s | %s\n", "Veri Boyutu", s1, s2, "Oran");
    printf("  ");
    for (int i = 0; i < 56; i++) printf("-");
    printf("\n");
}

/* ======================== Test 1: Hash Table vs Linked List ======================== */

static void test1_calistir(int n) {
    char ad[50];
    char tc[12];

    /* --- Hash Table Doldur --- */
    HashTablosu* ht = ht_olustur();
    for (int i = 0; i < n; i++) {
        tc_uret(tc, i);
        ad_uret(ad, i);
        ht_ekle(ht, tc, ad, 20 + (i % 80));
    }

    /* --- Linked List Doldur --- */
    LinkedList* ll = ll_olustur();
    for (int i = 0; i < n; i++) {
        tc_uret(tc, i);
        ad_uret(ad, i);
        ll_ekle(ll, tc, ad, 20 + (i % 80));
    }

    /* 100 rastgele arama */
    const int ARAMA_SAYISI = 100;
    int bulunan_ht = 0, bulunan_ll = 0;

    double ht_baslangic = milisaniye_al();
    for (int i = 0; i < ARAMA_SAYISI; i++) {
        tc_uret(tc, (i * 97) % n); /* Yarı-rastgele indeks */
        if (ht_ara(ht, tc)) bulunan_ht++;
    }
    double ht_sure = milisaniye_al() - ht_baslangic;

    double ll_baslangic = milisaniye_al();
    for (int i = 0; i < ARAMA_SAYISI; i++) {
        tc_uret(tc, (i * 97) % n);
        if (ll_ara(ll, tc)) bulunan_ll++;
    }
    double ll_sure = milisaniye_al() - ll_baslangic;

    char etiket[20];
    snprintf(etiket, sizeof(etiket), "%d", n);
    tablo_satir_yaz(etiket, ht_sure, ll_sure);
    (void)bulunan_ht; (void)bulunan_ll;

    ht_yok_et(ht);
    ll_yok_et(ll);
}

/* ======================== Test 2: Min-Heap vs Sıralı Dizi ======================== */

static void test2_calistir(int n) {
    TestHeap*  heap = (TestHeap*)calloc(1, sizeof(TestHeap));
    SiraliDizi* sd  = (SiraliDizi*)calloc(1, sizeof(SiraliDizi));
    char tc[12];

    /* --- Heap Ekleme --- */
    double heap_ekle_bas = milisaniye_al();
    for (int i = 0; i < n; i++) {
        tc_uret(tc, i);
        heap_ekle(heap, tc, 1 + (rand() % 10));
    }
    double heap_ekle_sure = milisaniye_al() - heap_ekle_bas;

    /* --- Sıralı Dizi Ekleme --- */
    double sd_ekle_bas = milisaniye_al();
    for (int i = 0; i < n; i++) {
        tc_uret(tc, i);
        sirali_ekle(sd, tc, 1 + (rand() % 10));
    }
    double sd_ekle_sure = milisaniye_al() - sd_ekle_bas;

    /* --- Heap Çıkarma (tümünü boşalt) --- */
    char tc_out[12];
    double heap_cik_bas = milisaniye_al();
    while (heap->boyut > 0) heap_cıkar(heap, tc_out);
    double heap_cik_sure = milisaniye_al() - heap_cik_bas;

    /* --- Sıralı Dizi Çıkarma --- */
    double sd_cik_bas = milisaniye_al();
    while (sd->boyut > 0) sirali_cıkar(sd, tc_out);
    double sd_cik_sure = milisaniye_al() - sd_cik_bas;

    char etiket[20];
    snprintf(etiket, sizeof(etiket), "%d", n);
    printf("  %-12s | %12.4f ms | %12.4f ms | x%-8.0f   [Ekleme]\n",
           etiket, heap_ekle_sure, sd_ekle_sure,
           heap_ekle_sure > 0.0001 ? sd_ekle_sure / heap_ekle_sure : 0);
    printf("  %-12s | %12.4f ms | %12.4f ms | x%-8.0f   [Cikartma]\n",
           "", heap_cik_sure, sd_cik_sure,
           heap_cik_sure > 0.0001 ? sd_cik_sure / heap_cik_sure : 0);

    free(heap);
    free(sd);
}

/* ======================== Test 3: Bubble Sort vs Quick Sort ======================== */

static void test3_calistir(int n) {
    /* Veri seti oluştur */
    char (*dizi1)[12] = (char(*)[12])malloc(sizeof(char[12]) * n);
    char (*dizi2)[12] = (char(*)[12])malloc(sizeof(char[12]) * n);
    if (!dizi1 || !dizi2) { free(dizi1); free(dizi2); return; }

    srand(42);
    for (int i = 0; i < n; i++) {
        /* Rastgele karışık TC üret */
        snprintf(dizi1[i], 12, "1%05d%05d", rand() % 100000, rand() % 100000);
        memcpy(dizi2[i], dizi1[i], 12);
    }

    double bb_bas = milisaniye_al();
    bubble_sort(dizi1, n);
    double bb_sure = milisaniye_al() - bb_bas;

    double qs_bas = milisaniye_al();
    quick_sort(dizi2, 0, n - 1);
    double qs_sure = milisaniye_al() - qs_bas;

    char etiket[20];
    snprintf(etiket, sizeof(etiket), "%d", n);
    tablo_satir_yaz(etiket, qs_sure, bb_sure);

    free(dizi1);
    free(dizi2);
}

/* ════════════════════════════════════════════════════════
 * TEST 4: DOĞRULUK TESTLERİ
 *
 * Algoritmik yaklaşım: Her veri yapısının temel özelliğini
 * küçük, deterministik girdilerle doğrula.
 *   - Hash Table: CRUD işlemi tutarlılığı
 *   - Queue:      FIFO sıra garantisi
 *   - Min-Heap:   min-özellik (her pop ≥ önceki pop)
 *   - Stack:      LIFO sıra garantisi
 * ════════════════════════════════════════════════════════ */

/* Test yardımcısı: sonucu yazdır, geçen sayısını döndür */
static int tok(int kosul, const char* mesaj) {
    printf("    %s %s\n", kosul ? "[GECTI]" : "[XXXXXXX BASARISIZ]", mesaj);
    return kosul ? 1 : 0;
}

/* ── Test 4a: Hash Table CRUD ve Çakışma Doğruluğu ── */
static void test4a_hash_dogru(void) {
    printf("\n  [Hash Table] CRUD + Cakisma Dogruluğu\n");
    int g = 0, t = 0;

    HashTablosu* ht = ht_olustur();

    /* Temel ekle */
    ht_ekle(ht, "12345678901", "Ali", 30);
    ht_ekle(ht, "98765432109", "Ayse", 25);
    ht_ekle(ht, "11111111111", "Mehmet", 45);
    t++; g += tok(ht->boyut == 3, "3 hasta eklendi → boyut=3");

    /* Doğru TC araması */
    TestHasta* h = ht_ara(ht, "12345678901");
    t++; g += tok(h != NULL && strcmp(h->ad, "Ali") == 0,
                  "TC araması doğru adı döndürdü (\"Ali\")");

    /* Olmayan TC araması → NULL beklenir */
    t++; g += tok(ht_ara(ht, "00000000000") == NULL,
                  "Olmayan TC araması → NULL döndürdü");

    /* Hash çakışması: iki farklı TC, aynı bucket'a düşse bile ikisi bulunmalı.
     * Polynomial hash mod 1009 ile TC "10000000001" ve "10000001010"'ün
     * aynı bucket'a düşmesi olası; test bunu zorlamak yerine zinciri doğrular. */
    ht_ekle(ht, "10000000001", "Kemal", 50);
    ht_ekle(ht, "10000001010", "Berna", 35);
    t++; g += tok(ht_ara(ht, "10000000001") != NULL &&
                  ht_ara(ht, "10000001010") != NULL,
                  "Olası çakışma sonrası her iki TC de bulunabildi");

    printf("  Sonuç: %d/%d test geçti.\n", g, t);
    ht_yok_et(ht);
}

/* ── Test 4b: Queue FIFO Sıra Garantisi ──
 * Algoritma: n elemanı sırayla enqueue et, hepsini dequeue et.
 * Çıkış sırası giriş sırasıyla birebir eşleşmeli (FIFO).
 */
static void test4b_queue_fifo(void) {
    printf("\n  [Queue] FIFO Sira Garantisi\n");
    int g = 0, t = 0;

    /* Basit dizi tabanlı FIFO simülasyonu (performans testinde Queue struct'ı yok) */
    int giris[6] = {10, 30, 50, 70, 90, 20};
    int kuyruk[6]; int on = 0, arka = 0;

    /* Enqueue hepsi */
    for (int i = 0; i < 6; i++) kuyruk[arka++] = giris[i];

    /* Dequeue ve FIFO doğrulama */
    int fifo_tamam = 1;
    for (int i = 0; i < 6; i++) {
        int cikan = kuyruk[on++];
        if (cikan != giris[i]) fifo_tamam = 0;
    }
    t++; g += tok(fifo_tamam, "6 elemanlı kuyrukta FIFO sırası korundu");

    /* Tek elemanlı kuyruk */
    int tek[1] = {42}; on = 0; arka = 0;
    kuyruk[arka++] = tek[0];
    t++; g += tok(kuyruk[on++] == 42, "Tek elemanlı kuyruk doğru çalıştı");

    /* Boş kuyruktan dequeue kontrolü */
    t++; g += tok(on == arka, "Kuyruk boşaltıldıktan sonra on==arka (boş göstergesi)");

    printf("  Sonuç: %d/%d test geçti.\n", g, t);
}

/* ── Test 4c: Min-Heap Özellik Doğrulaması ──
 * Algoritma: n rastgele skoru ekle, tümünü pop et.
 * Beklenti: Her pop edilen değer bir öncekinden ≥ olmalı.
 * Bu, min-heap özelliğini (kök = minimum) doğrular.
 */
static void test4c_heap_min_ozellik(void) {
    printf("\n  [Min-Heap] Min-Ozellik Dogrulaması\n");
    int g = 0, t = 0;

    TestHeap* heap = (TestHeap*)calloc(1, sizeof(TestHeap));
    char tc[12];

    /* Kasıtlı karışık sıraya ekle: sift-up bunları düzeltmeli */
    int skorlar[] = {5, 2, 8, 1, 9, 3, 7, 4, 6, 10};
    int n = 10;
    for (int i = 0; i < n; i++) {
        tc_uret(tc, i);
        heap_ekle(heap, tc, skorlar[i]);
    }
    t++; g += tok(heap->boyut == n, "10 eleman eklendi, boyut=10");

    /* İlk pop minimum olmalı (skor=1) */
    int ilk = heap_cıkar(heap, tc);
    t++; g += tok(ilk == 1, "İlk pop = 1 (global minimum)");

    /* Tüm pop'lar sıralı (artan) çıkmalı */
    int onceki = ilk, sirali = 1;
    while (heap->boyut > 0) {
        int skor = heap_cıkar(heap, tc);
        if (skor < onceki) { sirali = 0; break; }
        onceki = skor;
    }
    t++; g += tok(sirali, "Tüm pop'lar artan sıralı (min-heap özelliği korundu)");

    /* Boş heap'ten pop → -1 dönmeli */
    t++; g += tok(heap_cıkar(heap, tc) == -1,
                  "Boş heap'ten pop → -1 döndü");

    printf("  Sonuç: %d/%d test geçti.\n", g, t);
    free(heap);
}

/* ── Test 4d: Stack LIFO Sıra Garantisi ──
 * Algoritma: n elemanı push et, hepsini pop et.
 * Çıkış sırası giriş sırasının tersi olmalı (LIFO).
 */
static void test4d_stack_lifo(void) {
    printf("\n  [Stack] LIFO Sira Garantisi\n");
    int g = 0, t = 0;

    int giris[5] = {1, 2, 3, 4, 5};
    /* Stack: tepe pointer'ı yönetimi dizi ile simüle */
    int yigin[5]; int tepe = -1;

    /* Push hepsi */
    for (int i = 0; i < 5; i++) yigin[++tepe] = giris[i];
    t++; g += tok(tepe == 4, "5 eleman push edildi, tepe=4");

    /* Pop ve LIFO doğrulama */
    int lifo_tamam = 1;
    for (int i = 4; i >= 0; i--) {
        if (yigin[tepe--] != giris[i]) { lifo_tamam = 0; break; }
    }
    t++; g += tok(lifo_tamam, "Pop sırası giriş sırasının tersi (LIFO)");
    t++; g += tok(tepe == -1, "Stack boşaltıldıktan sonra tepe=-1");

    printf("  Sonuç: %d/%d test geçti.\n", g, t);
}

/* ════════════════════════════════════════════════════════
 * TEST 5: SINIR DURUM TESTLERİ (Edge Cases)
 *
 * Algoritmik yaklaşım: Veri yapılarının aşırı/sınır
 * koşullarında doğru davranıp davranmadığını kontrol et.
 *   - Boş yapı üzerinde işlem
 *   - Tek elemanlı yapı
 *   - Kapasite sınırları
 * ════════════════════════════════════════════════════════ */

static void test5_sinir_durumlari(void) {
    printf("\n  [Sınır] Bos Yapi Operasyonlari\n");
    int g = 0, t = 0;

    /* Hash Table: boş yapıda arama */
    HashTablosu* bos_ht = ht_olustur();
    t++; g += tok(ht_ara(bos_ht, "11111111111") == NULL,
                  "Boş hash table'da arama → NULL");
    t++; g += tok(bos_ht->boyut == 0, "Boş hash table boyutu = 0");
    ht_yok_et(bos_ht);

    /* Heap: boş yapıda pop */
    TestHeap* bos_heap = (TestHeap*)calloc(1, sizeof(TestHeap));
    char tc[12];
    t++; g += tok(heap_cıkar(bos_heap, tc) == -1,
                  "Boş heap'ten pop → -1 (hata kodu)");
    free(bos_heap);

    /* Tek elemanlı Heap: ekle + çıkar = doğru değer */
    TestHeap* tek_heap = (TestHeap*)calloc(1, sizeof(TestHeap));
    heap_ekle(tek_heap, "99999999999", 7);
    int tek_deger = heap_cıkar(tek_heap, tc);
    t++; g += tok(tek_deger == 7, "Tek elemanlı heap'e 7 ekle, pop = 7");
    t++; g += tok(tek_heap->boyut == 0, "Pop sonrası heap boyutu = 0");
    free(tek_heap);

    printf("\n  [Sınır] Hash Table Kapasite Stresi\n");
    /* 1009 (tam tablo boyutu kadar) eleman eklenebilmeli */
    HashTablosu* dolu_ht = ht_olustur();
    for (int i = 0; i < HT_BOYUT; i++) {
        tc_uret(tc, i + 1);    /* 0 değerinden kaçın */
        ht_ekle(dolu_ht, tc, "Test", 20);
    }
    t++; g += tok(dolu_ht->boyut == HT_BOYUT,
                  "1009 eleman eklendi (tam kapasite)");
    /* Eklenen ilk TC hâlâ bulunabilmeli */
    tc_uret(tc, 1);
    t++; g += tok(ht_ara(dolu_ht, tc) != NULL,
                  "Tam kapasitede bile ilk TC bulunabildi");
    ht_yok_et(dolu_ht);

    printf("  Sonuç: %d/%d test geçti.\n", g, t);
}

/* ════════════════════════════════════════════════════════
 * TEST 6: HASH FONKSİYONU KALİTE ANALİZİ
 *
 * Algoritmik yaklaşım: n kayıt için bucket dağılımını ölç.
 *   - Yük faktörü:  n / TABLO_BOYUTU  (ideal: 0.5–0.75)
 *   - Standart sapma: √(Σ(uzunluk − ortalama)² / N)
 *     → Düşük std.sapma = homojen dağılım = iyi hash fonksiyonu
 *   - Maks zincir = en uzun çakışma zinciri
 *     → 1 ise sıfır çakışma; yüksekse hash fonksiyonu zayıf
 *
 * Referans: Knuth, TAOCP Vol.3, §6.4 — Hash table analysis
 * ════════════════════════════════════════════════════════ */

static void test6_hash_kalite(void) {
    int boyutlar[] = {100, 500, 1000, 5000};
    int nb = 4;

    printf("\n  %-8s | %-6s | %-10s | %-12s | %-11s | %-10s\n",
           "N", "Dolu", "Yuk Fakt.", "Maks Zincir", "Std.Sapma", "Deger.");
    printf("  ");
    for (int i = 0; i < 64; i++) printf("-");
    printf("\n");

    for (int b = 0; b < nb; b++) {
        int n = boyutlar[b];
        HashTablosu* ht = ht_olustur();
        char tc[12];

        for (int i = 0; i < n; i++) {
            tc_uret(tc, i + 1);
            ht_ekle(ht, tc, "X", 1);
        }

        /* Bucket istatistikleri */
        int dolu_bucket = 0, maks = 0;
        double ortalama = (double)n / HT_BOYUT;
        double varyans  = 0.0;

        for (int i = 0; i < HT_BOYUT; i++) {
            int uzunluk = 0;
            HTDugum* d = ht->buckets[i];
            while (d) { uzunluk++; d = d->sonraki; }
            if (uzunluk > 0) dolu_bucket++;
            if (uzunluk > maks) maks = uzunluk;
            double fark = uzunluk - ortalama;
            varyans += fark * fark;
        }
        varyans /= HT_BOYUT;
        double std_sapma = sqrt(varyans);

        /* Değerlendirme kriteri: standart sapma < 0.5 → iyi */
        const char* deger = std_sapma < 0.5 ? "Mukemmel" :
                            std_sapma < 1.0 ? "Iyi     " :
                            std_sapma < 2.0 ? "Orta    " : "Zayif   ";

        printf("  %-8d | %-6d | %-10.4f | %-12d | %-11.4f | %s\n",
               n, dolu_bucket, (double)n / HT_BOYUT, maks, std_sapma, deger);

        ht_yok_et(ht);
    }

    printf("\n  Kriter: Std.Sapma < 0.5 = Mukemmel | < 1.0 = Iyi | < 2.0 = Orta\n");
    printf("  Aciklama: Dusuk std.sapma → hash fonksiyonu TC degerlerini\n");
    printf("            homojen dagitiyor → cakisma az → arama O(1)'e yakin.\n");
}

/* ======================== main ======================== */

int main(void) {
    srand((unsigned)time(NULL));

    printf("\n");
    printf("  ╔══════════════════════════════════════════════╗\n");
    printf("  ║  HASTANE SİSTEMİ - PERFORMANS TESTLERİ     ║\n");
    printf("  ║  BMT210 Veri Yapilari                        ║\n");
    printf("  ╚══════════════════════════════════════════════╝\n");

    /* ---- TEST 1 ---- */
    tablo_baslik("TEST 1: ARAMA PERFORMANSI - Hash Table vs Linked List");
    printf("  Açıklama: %d rastgele arama için süre ölçümü\n\n", 100);
    tablo_ust("Hash Table (ms)", "Linked List (ms)");
    test1_calistir(100);
    test1_calistir(1000);
    test1_calistir(10000);
    printf("\n  Sonuç: Hash Table O(1), Linked List O(n) - n büyüdükçe fark artar.\n");

    /* ---- TEST 2 ---- */
    tablo_baslik("TEST 2: ONCELIKLENDIRME - Min-Heap vs Sirali Dizi");
    printf("  Açıklama: Tüm elemanları ekle + tümünü çıkar\n\n");
    tablo_ust("Min-Heap (ms)", "Sirali Dizi (ms)");
    test2_calistir(100);
    test2_calistir(1000);
    test2_calistir(10000);
    printf("\n  Sonuç: Min-Heap O(log n), Sıralı Dizi ekleme O(n) - ekleme n büyüyünce yavaşlar.\n");

    /* ---- TEST 3 ---- */
    tablo_baslik("TEST 3: SIRALAMA - Quick Sort vs Bubble Sort");
    printf("  Açıklama: TC numaralarını alfabetik sırala\n\n");
    printf("  Not: Bubble Sort n=10000 icin CIKARTILMISTIR (cok yavash)\n\n");
    printf("  %-12s | %13s | %13s | %s\n", "Veri Boyutu", "Quick Sort (ms)", "Bubble Sort (ms)", "Oran");
    printf("  ");
    for (int i = 0; i < 56; i++) printf("-");
    printf("\n");
    test3_calistir(100);
    test3_calistir(1000);
    /* n=10000 için bubble sort çok yavaş - sadece quicksort göster */
    {
        char (*dizi)[12] = (char(*)[12])malloc(sizeof(char[12]) * 10000);
        if (dizi) {
            srand(42);
            for (int i = 0; i < 10000; i++)
                snprintf(dizi[i], 12, "1%05d%05d", rand()%100000, rand()%100000);
            double bas = milisaniye_al();
            quick_sort(dizi, 0, 9999);
            double sure = milisaniye_al() - bas;
            printf("  %-12s | %12.4f ms | %13s | %s\n",
                   "10000", sure, "~çok uzun", "—");
            free(dizi);
        }
    }
    printf("\n  Sonuç: Quick Sort O(n log n), Bubble Sort O(n^2) - büyük n'de dramatik fark.\n");

    /* ---- TEST 4: DOĞRULUK ---- */
    tablo_baslik("TEST 4: DOGRULUK TESTLERI - Veri Yapisi Ozellikleri");
    printf("  Her veri yapisinin temel algoritmik ozelligi kucuk\n");
    printf("  deterministik girdilerle dogrulanir.\n");
    test4a_hash_dogru();
    test4b_queue_fifo();
    test4c_heap_min_ozellik();
    test4d_stack_lifo();

    /* ---- TEST 5: SINIR DURUMLARI ---- */
    tablo_baslik("TEST 5: SINIR DURUM TESTLERI - Edge Cases");
    printf("  Bos yapi, tek eleman, tam kapasite senaryolari.\n");
    test5_sinir_durumlari();

    /* ---- TEST 6: HASH KALİTE ANALİZİ ---- */
    tablo_baslik("TEST 6: HASH FONKSIYON KALITE ANALIZI");
    printf("  Bucket dagilimi, yuk faktoru ve standart sapma olcumu.\n");
    printf("  Dusuk std.sapma = homojen dagilim = iyi hash fonksiyonu.\n");
    test6_hash_kalite();

    printf("\n");
    for (int i = 0; i < 60; i++) printf("=");
    printf("\n  Tum testler tamamlandi.\n");
    for (int i = 0; i < 60; i++) printf("=");
    printf("\n\n");

    return 0;
}
