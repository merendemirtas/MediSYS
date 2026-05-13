/*
 * stack.h
 * İşlem geçmişi modülü - "Geri Al" özelliği için LIFO yapısı.
 * Veri Yapısı: Linked List tabanlı Stack
 * Neden Stack: LIFO semantiği "en son yapılanı geri al" için mükemmeldir.
 * push/pop O(1): tepe pointer'ı değiştirmek yeterli.
 */

#ifndef STACK_H
#define STACK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Stack düğümü: tek yönlü linked list (tepe → alta doğru).
 * undo_data: geri alma için gerekli veriyi JSON string olarak saklar.
 *   Örn: HASTA_SIL → silinen hastanın tüm alanları JSON'da tutulur.
 */
typedef struct StackNode {
    char   islem_tipi[30];   /* "HASTA_EKLE", "HASTA_SIL", "RANDEVU_AL" vb. */
    char   tc[12];
    char   aciklama[200];
    char   undo_data[512];   /* Geri alma için yedek veri (JSON formatında) */
    time_t zaman;
    struct StackNode* sonraki;
} StackNode;

/*
 * Stack yönetim yapısı.
 * tepe: en son eklenen işlem (ilk geri alınacak) - O(1) erişim
 * boyut: toplam işlem sayısı
 */
typedef struct {
    StackNode* tepe;
    int        boyut;
} Stack;

Stack*     stack_olustur(void);
void       stack_yok_et(Stack* s);
int        islem_kaydet(Stack* s,
                        const char* islem_tipi, const char* tc,
                        const char* aciklama, const char* undo_data);
/* Son işlemi stack'ten çıkarır; çağıran free() etmeli */
StackNode* son_islemi_geri_al(Stack* s);
/* Son n işlemi dizi olarak döndürür; *sayi güncellenir, çağıran free() etmeli */
StackNode* gecmis_goster(Stack* s, int n, int* sayi);

#endif /* STACK_H */
