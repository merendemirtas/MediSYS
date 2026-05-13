/*
 * stack.c
 * İşlem geçmişi modülü implementasyonu.
 * Veri Yapısı: Linked List tabanlı LIFO Stack
 *
 * LIFO garantisi: En son push edilen eleman ilk pop edilir.
 *   - push: Yeni düğümü 'tepe' önüne ekler - O(1).
 *   - pop:  'tepe' düğümünü çıkarır - O(1).
 *
 * Linked list: Sabit kapasiteli dizi stack'in aksine bellek sınırı yok.
 * MAX_GECMIS: Stack'in tutacağı maksimum işlem sayısı (taşma koruması).
 */

#include "stack.h"

#define MAX_GECMIS 500

Stack* stack_olustur(void) {
    Stack* s = (Stack*)calloc(1, sizeof(Stack));
    if (!s) return NULL;
    return s;
}

void stack_yok_et(Stack* s) {
    if (!s) return;
    StackNode* gecici = s->tepe;
    while (gecici) {
        StackNode* silinecek = gecici;
        gecici = gecici->sonraki;
        free(silinecek);
    }
    free(s);
}

/*
 * Push: Yeni düğümü tepe önüne ekler - O(1).
 * Eğer MAX_GECMIS aşılmışsa en alttaki (en eski) düğüm silinir.
 */
int islem_kaydet(Stack* s,
                 const char* islem_tipi, const char* tc,
                 const char* aciklama,   const char* undo_data) {
    if (!s) return -1;

    StackNode* yeni = (StackNode*)malloc(sizeof(StackNode));
    if (!yeni) return -1;

    strncpy(yeni->islem_tipi, islem_tipi, 29); yeni->islem_tipi[29] = '\0';
    strncpy(yeni->tc,         tc,         11); yeni->tc[11]         = '\0';
    strncpy(yeni->aciklama,   aciklama,  199); yeni->aciklama[199]  = '\0';
    if (undo_data) {
        strncpy(yeni->undo_data, undo_data, 511);
        yeni->undo_data[511] = '\0';
    } else {
        yeni->undo_data[0] = '\0';
    }
    yeni->zaman    = time(NULL);
    yeni->sonraki  = s->tepe;
    s->tepe        = yeni;
    s->boyut++;

    /* Maksimum geçmişe ulaşıldıysa en alttaki düğümü sil */
    if (s->boyut > MAX_GECMIS) {
        StackNode* onceki = s->tepe;
        while (onceki->sonraki && onceki->sonraki->sonraki) {
            onceki = onceki->sonraki;
        }
        if (onceki->sonraki) {
            free(onceki->sonraki);
            onceki->sonraki = NULL;
            s->boyut--;
        }
    }
    return 0;
}

/*
 * Pop: Tepe düğümünü stack'ten çıkarır - O(1).
 * Çağıran free() ile sonucu serbest bırakmalıdır.
 */
StackNode* son_islemi_geri_al(Stack* s) {
    if (!s || !s->tepe) return NULL;

    StackNode* cikarilan = s->tepe;
    s->tepe = s->tepe->sonraki;
    cikarilan->sonraki = NULL;
    s->boyut--;
    return cikarilan;
}

/*
 * Son n işlemi dizi olarak döndürür (tepe = indeks 0).
 * Stack'i değiştirmez, sadece kopyalar.
 * Çağıran free() etmeli.
 */
StackNode* gecmis_goster(Stack* s, int n, int* sayi) {
    if (!s || !sayi || n <= 0) return NULL;

    int gercek_sayi = (n < s->boyut) ? n : s->boyut;
    *sayi = gercek_sayi;
    if (*sayi == 0) return NULL;

    StackNode* dizi = (StackNode*)malloc(sizeof(StackNode) * (*sayi));
    if (!dizi) return NULL;

    StackNode* gecici = s->tepe;
    for (int i = 0; i < *sayi && gecici; i++) {
        dizi[i] = *gecici;
        dizi[i].sonraki = NULL; /* Kopyadaki pointer'ı geçersiz kıl */
        gecici = gecici->sonraki;
    }
    return dizi;
}
