/*
 * queue.h
 * Normal randevu kuyruğu modülü - FIFO (ilk gelen ilk muayene edilir).
 * Veri Yapısı: Linked List tabanlı Queue
 * Neden Queue: FIFO semantiği doğal olarak modeller; enqueue/dequeue O(1).
 * Linked list: sabit boyut kısıtı yok, dinamik büyüme sağlar.
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Kuyruk düğümü: tek yönlü linked list.
 * 'sonraki' pointer ile zincir oluşturulur.
 */
typedef struct QueueNode {
    char   tc[12];
    char   ad[50];
    char   soyad[50];
    char   poliklinik[50];
    char   randevu_saati[10];
    time_t eklenme_zamani;
    struct QueueNode* sonraki;
} QueueNode;

/*
 * Queue yönetim yapısı.
 * on:   dequeue tarafı (ilk eklenen) - O(1) çıkarım için
 * arka: enqueue tarafı (son eklenen) - O(1) ekleme için
 * boyut: anlık kuyruk uzunluğu
 */
typedef struct {
    QueueNode* on;
    QueueNode* arka;
    int        boyut;
} Queue;

Queue*     queue_olustur(void);
void       queue_yok_et(Queue* q);
int        randevu_al(Queue* q,
                      const char* tc, const char* ad, const char* soyad,
                      const char* poliklinik, const char* randevu_saati);
/* Sıradaki hastayı kuyruğun önünden çıkarır; çağıran free() etmeli */
QueueNode* siradaki_hasta_cagir(Queue* q);
/* Kuyruğu dizi olarak döndürür; *sayi güncellenir, çağıran free() etmeli */
QueueNode* kuyruk_durumu_goster(Queue* q, int* sayi);
int        randevu_iptal(Queue* q, const char* tc);

#endif /* QUEUE_H */
