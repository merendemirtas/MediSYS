/*
 * heap.h
 * Acil triaj modülü - en düşük aciliyet skoru = en kritik hasta.
 * Veri Yapısı: Min-Heap (öncelik kuyruğu)
 * Neden Min-Heap: En yüksek öncelikli elemanı O(log n) ile ekler/çıkarır.
 * Dizi tabanlı: Ebeveyn i → çocuklar 2i+1, 2i+2; bellekte bitişik, cache dostu.
 */

#ifndef HEAP_H
#define HEAP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_TRIAJ_HASTA 2000

typedef struct {
    char   tc[12];
    char   ad[50];
    char   soyad[50];
    int    aciliyet_skoru;  /* 1 = en acil, 10 = en az acil */
    time_t gelis_zamani;
} TriajHasta;

/*
 * Min-Heap yapısı: dizi tabanlı tam ikili ağaç.
 * boyut: geçerli eleman sayısı
 * Ekleme sonrası sift-up, çıkarma sonrası sift-down ile heap özelliği korunur.
 */
typedef struct {
    TriajHasta hastalar[MAX_TRIAJ_HASTA];
    int        boyut;
} MinHeap;

MinHeap*    heap_olustur(void);
void        heap_yok_et(MinHeap* heap);
int         triaj_ekle(MinHeap* heap,
                       const char* tc, const char* ad, const char* soyad,
                       int aciliyet_skoru);
/* En acil hastayı heap'ten çıkarır; çağıran free() etmeli */
TriajHasta* en_acil_hastay_al(MinHeap* heap);
/* Heap içeriğini kopyalar; *sayi güncellenir, çağıran free() etmeli */
TriajHasta* triaj_listesi_goster(MinHeap* heap, int* sayi);

#endif /* HEAP_H */
