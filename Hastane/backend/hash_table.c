/*
 * hash_table.c
 * Hasta kayıt modülü implementasyonu.
 * Veri Yapısı: Hash Table - Chaining (Linked List ile çakışma çözümü)
 *
 * Hash fonksiyonu: Polynomial rolling hash
 *   hash = Σ(tc[i] * 31^i) mod HASH_TABLE_BOYUTU
 *   31 asal sayısı, ASCII karakter dağılımında iyi çakışma oranı verir.
 *
 * Chaining: Çakışan TC'ler aynı bucket'ın linked list'ine eklenir.
 *   En kötü durum arama O(n), ortalama O(1) - yük faktörü düşük tutulursa.
 */

#include "hash_table.h"

/*
 * Polynomial rolling hash.
 * TC string'inin her karakterini ağırlıklı olarak toplar.
 * Neden 31? Küçük asal sayı, hızlı hesaplama + iyi dağılım.
 */
unsigned int hash_fonksiyonu(const char* tc) {
    unsigned int hash = 0;
    for (int i = 0; tc[i] != '\0'; i++) {
        hash = hash * 31 + (unsigned char)tc[i];
    }
    return hash % HASH_TABLE_BOYUTU;
}

HashTable* hash_table_olustur(void) {
    HashTable* ht = (HashTable*)calloc(1, sizeof(HashTable));
    if (!ht) return NULL;
    /* calloc ile tüm bucket pointer'ları NULL olarak başlatılır */
    return ht;
}

/* Rekürsif olarak tüm bucket zincirlerini serbest bırakır */
static void zincir_yok_et(HastaNode* dugum) {
    while (dugum) {
        HastaNode* silinecek = dugum;
        dugum = dugum->sonraki;
        free(silinecek);
    }
}

void hash_table_yok_et(HashTable* ht) {
    if (!ht) return;
    for (int i = 0; i < HASH_TABLE_BOYUTU; i++) {
        zincir_yok_et(ht->buckets[i]);
    }
    free(ht);
}

int hasta_ekle(HashTable* ht,
               const char* tc, const char* ad, const char* soyad,
               int yas, const char* poliklinik, const char* telefon) {
    if (!ht || !tc || tc[0] == '\0') return -1;

    unsigned int indeks = hash_fonksiyonu(tc);

    /* Aynı TC zaten var mı kontrol et (chaining zincirinde gez) */
    HastaNode* gecici = ht->buckets[indeks];
    while (gecici) {
        if (strcmp(gecici->hasta.tc, tc) == 0) return -2; /* Zaten kayıtlı */
        gecici = gecici->sonraki;
    }

    /* Yeni düğüm oluştur */
    HastaNode* yeni = (HastaNode*)malloc(sizeof(HastaNode));
    if (!yeni) return -1;

    strncpy(yeni->hasta.tc,          tc,          11);  yeni->hasta.tc[11]         = '\0';
    strncpy(yeni->hasta.ad,          ad,          49);  yeni->hasta.ad[49]         = '\0';
    strncpy(yeni->hasta.soyad,       soyad,       49);  yeni->hasta.soyad[49]      = '\0';
    strncpy(yeni->hasta.poliklinik,  poliklinik,  49);  yeni->hasta.poliklinik[49] = '\0';
    strncpy(yeni->hasta.telefon,     telefon,     14);  yeni->hasta.telefon[14]    = '\0';
    yeni->hasta.yas = yas;

    /* Bucket başına ekle (O(1) - zincirin sonuna eklemeye gerek yok) */
    yeni->sonraki = ht->buckets[indeks];
    ht->buckets[indeks] = yeni;
    ht->toplam_hasta++;
    return 0;
}

int hasta_sil(HashTable* ht, const char* tc) {
    if (!ht || !tc) return -1;

    unsigned int indeks = hash_fonksiyonu(tc);
    HastaNode* onceki = NULL;
    HastaNode* gecici = ht->buckets[indeks];

    /* Chaining zincirinde TC'yi bul */
    while (gecici) {
        if (strcmp(gecici->hasta.tc, tc) == 0) {
            /* Çift pointer tekniği: onceki varsa → sonraki bağla, yoksa bucket başını güncelle */
            if (onceki) {
                onceki->sonraki = gecici->sonraki;
            } else {
                ht->buckets[indeks] = gecici->sonraki;
            }
            free(gecici);
            ht->toplam_hasta--;
            return 0;
        }
        onceki = gecici;
        gecici = gecici->sonraki;
    }
    return -1; /* Bulunamadı */
}

Hasta* hasta_ara(HashTable* ht, const char* tc) {
    if (!ht || !tc) return NULL;

    unsigned int indeks = hash_fonksiyonu(tc);
    HastaNode* gecici = ht->buckets[indeks];

    /* Sadece ilgili bucket'ın zincirinde arama - O(k), k = zincir uzunluğu */
    while (gecici) {
        if (strcmp(gecici->hasta.tc, tc) == 0) {
            return &gecici->hasta;
        }
        gecici = gecici->sonraki;
    }
    return NULL;
}

int hasta_guncelle(HashTable* ht,
                   const char* tc, const char* ad, const char* soyad,
                   int yas, const char* poliklinik, const char* telefon) {
    Hasta* h = hasta_ara(ht, tc);
    if (!h) return -1;

    if (ad && ad[0])         { strncpy(h->ad,         ad,         49); h->ad[49]         = '\0'; }
    if (soyad && soyad[0])   { strncpy(h->soyad,      soyad,      49); h->soyad[49]      = '\0'; }
    if (yas > 0)               h->yas = yas;
    if (poliklinik && poliklinik[0]) { strncpy(h->poliklinik, poliklinik, 49); h->poliklinik[49] = '\0'; }
    if (telefon && telefon[0]) { strncpy(h->telefon,   telefon,    14); h->telefon[14]    = '\0'; }
    return 0;
}

/*
 * Tüm hastaları düz dizi olarak toplar.
 * O(n): tüm bucket'ları ve zincirlerini gezer.
 * Çağıran free() ile belleği serbest bırakmalıdır.
 */
Hasta* tum_hastalari_listele(HashTable* ht, int* sayi) {
    if (!ht || !sayi) return NULL;
    *sayi = ht->toplam_hasta;
    if (*sayi == 0) return NULL;

    Hasta* dizi = (Hasta*)malloc(sizeof(Hasta) * (*sayi));
    if (!dizi) return NULL;

    int idx = 0;
    for (int i = 0; i < HASH_TABLE_BOYUTU; i++) {
        HastaNode* gecici = ht->buckets[i];
        while (gecici && idx < *sayi) {
            dizi[idx++] = gecici->hasta;
            gecici = gecici->sonraki;
        }
    }
    return dizi;
}
