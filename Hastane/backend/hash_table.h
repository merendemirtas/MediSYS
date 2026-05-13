/*
 * hash_table.h
 * Hasta kayıt modülü - TC kimlik numarası anahtar olarak kullanılır.
 * Veri Yapısı: Hash Table (Chaining ile çakışma çözümü)
 * Neden Hash Table: O(1) ortalama erişim süresi sağlar.
 * Chaining: Her bucket bir linked list tutar, çakışma durumunda zincir uzar.
 */

#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Asal sayı seçildi: çakışmaları minimize eder (hash dağılımı daha düzgün) */
#define HASH_TABLE_BOYUTU 1009

typedef struct Hasta {
    char tc[12];
    char ad[50];
    char soyad[50];
    int  yas;
    char poliklinik[50];
    char telefon[15];
} Hasta;

/*
 * Chaining için linked list düğümü.
 * Her bucket başı bir HastaNode zinciridir.
 * Aynı hash değerini alan TC'ler aynı zincirde tutulur.
 */
typedef struct HastaNode {
    Hasta          hasta;
    struct HastaNode* sonraki;
} HastaNode;

/*
 * Hash Table ana yapısı.
 * buckets: HASH_TABLE_BOYUTU adet linked list başı (pointer dizisi)
 * toplam_hasta: O(1) sayım için tutulan sayaç
 */
typedef struct {
    HastaNode* buckets[HASH_TABLE_BOYUTU];
    int        toplam_hasta;
} HashTable;

/* Yaşam döngüsü */
HashTable* hash_table_olustur(void);
void       hash_table_yok_et(HashTable* ht);

/* Hash fonksiyonu: TC string'ini HASH_TABLE_BOYUTU modüllü indekse çevirir */
unsigned int hash_fonksiyonu(const char* tc);

/* CRUD operasyonları */
int    hasta_ekle(HashTable* ht,
                  const char* tc, const char* ad, const char* soyad,
                  int yas, const char* poliklinik, const char* telefon);
int    hasta_sil(HashTable* ht, const char* tc);
Hasta* hasta_ara(HashTable* ht, const char* tc);
int    hasta_guncelle(HashTable* ht,
                      const char* tc, const char* ad, const char* soyad,
                      int yas, const char* poliklinik, const char* telefon);

/* Tüm hastaları dizi olarak döndürür; *sayi güncellenir, çağıran free() etmeli */
Hasta* tum_hastalari_listele(HashTable* ht, int* sayi);

#endif /* HASH_TABLE_H */
