/*
 * heap.c
 * Acil triaj modülü implementasyonu.
 * Veri Yapısı: Min-Heap (dizi tabanlı tam ikili ağaç)
 *
 * Heap özelliği: Her düğüm, çocuklarından küçük veya eşittir.
 *   → Kök her zaman en küçük aciliyet skorunu (= en kritik hasta) tutar.
 *
 * Dizi indeks ilişkisi:
 *   Ebeveyn(i) = (i-1)/2
 *   Sol çocuk(i) = 2*i+1
 *   Sağ çocuk(i) = 2*i+2
 *
 * sift_up:   Yeni eklenen elemanı yukarı kabarcık gibi taşır - O(log n)
 * sift_down: Kök çıkarıldıktan sonra yeni kökü aşağı iter - O(log n)
 */

#include "heap.h"

MinHeap* heap_olustur(void) {
    MinHeap* heap = (MinHeap*)malloc(sizeof(MinHeap));
    if (!heap) return NULL;
    heap->boyut = 0;
    return heap;
}

void heap_yok_et(MinHeap* heap) {
    free(heap);
}

/* İki TriajHasta elemanını yerinde değiştirir */
static void yer_degistir(TriajHasta* a, TriajHasta* b) {
    TriajHasta gecici = *a;
    *a = *b;
    *b = gecici;
}

/*
 * sift_up: Yeni eklenen son elemanı min-heap özelliği sağlanana dek
 * ebeveynleriyle karşılaştırarak yukarı taşır.
 * Karmaşıklık: O(log n) - ağaç yüksekliği kadar adım.
 */
static void sift_up(MinHeap* heap, int indeks) {
    while (indeks > 0) {
        int ebeveyn = (indeks - 1) / 2;
        /* Min-heap: aciliyet skoru küçükse yukarı taşı */
        if (heap->hastalar[indeks].aciliyet_skoru < heap->hastalar[ebeveyn].aciliyet_skoru) {
            yer_degistir(&heap->hastalar[indeks], &heap->hastalar[ebeveyn]);
            indeks = ebeveyn;
        } else {
            break; /* Heap özelliği sağlandı */
        }
    }
}

/*
 * sift_down: Kök çıkarıldıktan sonra yerine konulan son elemanı
 * min-heap özelliği sağlanana dek çocuklarıyla karşılaştırarak aşağı iter.
 * Karmaşıklık: O(log n).
 */
static void sift_down(MinHeap* heap, int indeks) {
    int boyut = heap->boyut;
    while (1) {
        int en_kucuk = indeks;
        int sol = 2 * indeks + 1;
        int sag = 2 * indeks + 2;

        if (sol < boyut &&
            heap->hastalar[sol].aciliyet_skoru < heap->hastalar[en_kucuk].aciliyet_skoru) {
            en_kucuk = sol;
        }
        if (sag < boyut &&
            heap->hastalar[sag].aciliyet_skoru < heap->hastalar[en_kucuk].aciliyet_skoru) {
            en_kucuk = sag;
        }

        if (en_kucuk == indeks) break; /* Heap özelliği sağlandı */

        yer_degistir(&heap->hastalar[indeks], &heap->hastalar[en_kucuk]);
        indeks = en_kucuk;
    }
}

int triaj_ekle(MinHeap* heap,
               const char* tc, const char* ad, const char* soyad,
               int aciliyet_skoru) {
    if (!heap) return -1;
    if (heap->boyut >= MAX_TRIAJ_HASTA) return -2; /* Kapasite dolu */
    if (aciliyet_skoru < 1 || aciliyet_skoru > 10) return -3;

    /* Yeni hastayı dizinin sonuna ekle */
    int son = heap->boyut;
    strncpy(heap->hastalar[son].tc,    tc,    11); heap->hastalar[son].tc[11]    = '\0';
    strncpy(heap->hastalar[son].ad,    ad,    49); heap->hastalar[son].ad[49]    = '\0';
    strncpy(heap->hastalar[son].soyad, soyad, 49); heap->hastalar[son].soyad[49] = '\0';
    heap->hastalar[son].aciliyet_skoru = aciliyet_skoru;
    heap->hastalar[son].gelis_zamani   = time(NULL);
    heap->boyut++;

    /* Heap özelliğini yukarı doğru düzelt */
    sift_up(heap, son);
    return 0;
}

/*
 * En acil hastayı (kök) heap'ten çıkarır.
 * 1. Kökü son elemanla değiştir.
 * 2. boyutu bir azalt.
 * 3. Yeni kökü sift_down ile aşağı it.
 * Çağıran free() ile sonucu serbest bırakmalıdır.
 */
TriajHasta* en_acil_hastay_al(MinHeap* heap) {
    if (!heap || heap->boyut == 0) return NULL;

    TriajHasta* sonuc = (TriajHasta*)malloc(sizeof(TriajHasta));
    if (!sonuc) return NULL;
    *sonuc = heap->hastalar[0]; /* Kökü kaydet */

    /* Son elemanı köke taşı, boyutu azalt */
    heap->boyut--;
    if (heap->boyut > 0) {
        heap->hastalar[0] = heap->hastalar[heap->boyut];
        sift_down(heap, 0);
    }
    return sonuc;
}

/*
 * Triaj listesinin anlık kopyasını döndürür (heap düzeni korunur).
 * Çağıran free() ile belleği serbest bırakmalıdır.
 */
TriajHasta* triaj_listesi_goster(MinHeap* heap, int* sayi) {
    if (!heap || !sayi) return NULL;
    *sayi = heap->boyut;
    if (*sayi == 0) return NULL;

    TriajHasta* kopya = (TriajHasta*)malloc(sizeof(TriajHasta) * (*sayi));
    if (!kopya) return NULL;
    memcpy(kopya, heap->hastalar, sizeof(TriajHasta) * (*sayi));
    return kopya;
}
