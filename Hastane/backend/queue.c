/*
 * queue.c
 * Normal randevu kuyruğu implementasyonu.
 * Veri Yapısı: Linked List tabanlı FIFO Queue
 *
 * FIFO garantisi:
 *   - Enqueue: 'arka' pointer'a O(1) ile ekler.
 *   - Dequeue: 'on' pointer'dan O(1) ile çıkarır.
 *   Çift pointer sayesinde her iki uç O(1) erişim sağlar.
 *
 * Silme (iptal): TC'ye göre O(n) arama + O(1) bağlantı güncelleme.
 */

#include "queue.h"

Queue* queue_olustur(void) {
    Queue* q = (Queue*)calloc(1, sizeof(Queue));
    if (!q) return NULL;
    /* on = arka = NULL, boyut = 0 (calloc ile) */
    return q;
}

void queue_yok_et(Queue* q) {
    if (!q) return;
    QueueNode* gecici = q->on;
    while (gecici) {
        QueueNode* silinecek = gecici;
        gecici = gecici->sonraki;
        free(silinecek);
    }
    free(q);
}

/*
 * Enqueue: Yeni düğümü 'arka' pointer'a ekler - O(1).
 * Kuyruk boşsa hem 'on' hem 'arka' yeni düğümü gösterir.
 */
int randevu_al(Queue* q,
               const char* tc, const char* ad, const char* soyad,
               const char* poliklinik, const char* randevu_saati) {
    if (!q || !tc) return -1;

    QueueNode* yeni = (QueueNode*)malloc(sizeof(QueueNode));
    if (!yeni) return -1;

    strncpy(yeni->tc,           tc,           11); yeni->tc[11]           = '\0';
    strncpy(yeni->ad,           ad,           49); yeni->ad[49]           = '\0';
    strncpy(yeni->soyad,        soyad,        49); yeni->soyad[49]        = '\0';
    strncpy(yeni->poliklinik,   poliklinik,   49); yeni->poliklinik[49]   = '\0';
    strncpy(yeni->randevu_saati,randevu_saati, 9); yeni->randevu_saati[9] = '\0';
    yeni->eklenme_zamani = time(NULL);
    yeni->sonraki = NULL;

    if (q->arka) {
        q->arka->sonraki = yeni; /* Mevcut son düğümün arkasına ekle */
    } else {
        q->on = yeni; /* Boş kuyruk: ilk eleman */
    }
    q->arka = yeni;
    q->boyut++;
    return 0;
}

/*
 * Dequeue: 'on' pointer'dan çıkarır - O(1).
 * Kuyruk tek elemanlıysa 'arka' da NULL yapılır.
 * Çağıran free() ile sonucu serbest bırakmalıdır.
 */
QueueNode* siradaki_hasta_cagir(Queue* q) {
    if (!q || !q->on) return NULL;

    QueueNode* cikarilan = q->on;
    q->on = q->on->sonraki;
    if (!q->on) q->arka = NULL; /* Kuyruk boşaldı */
    q->boyut--;
    cikarilan->sonraki = NULL;
    return cikarilan;
}

/*
 * Kuyruğun anlık görüntüsünü dizi olarak döndürür.
 * Orijinal linked list yapısını bozmaz - sadece kopyalar.
 * Çağıran free() etmeli.
 */
QueueNode* kuyruk_durumu_goster(Queue* q, int* sayi) {
    if (!q || !sayi) return NULL;
    *sayi = q->boyut;
    if (*sayi == 0) return NULL;

    QueueNode* dizi = (QueueNode*)malloc(sizeof(QueueNode) * (*sayi));
    if (!dizi) return NULL;

    QueueNode* gecici = q->on;
    for (int i = 0; i < *sayi && gecici; i++) {
        dizi[i] = *gecici;
        dizi[i].sonraki = NULL; /* Kopyadaki pointer'ları geçersiz kıl */
        gecici = gecici->sonraki;
    }
    return dizi;
}

/*
 * TC'ye göre randevu iptali - O(n) arama, O(1) silme.
 * onceki pointer ile tek geçişte düğüm bulunur ve zincirden çıkarılır.
 */
int randevu_iptal(Queue* q, const char* tc) {
    if (!q || !tc) return -1;

    QueueNode* onceki = NULL;
    QueueNode* gecici = q->on;

    while (gecici) {
        if (strcmp(gecici->tc, tc) == 0) {
            if (onceki) {
                onceki->sonraki = gecici->sonraki;
            } else {
                q->on = gecici->sonraki; /* Baştan silme */
            }
            if (gecici == q->arka) {
                q->arka = onceki; /* Son elemanı sildik */
            }
            free(gecici);
            q->boyut--;
            return 0;
        }
        onceki = gecici;
        gecici = gecici->sonraki;
    }
    return -1; /* Bulunamadı */
}
