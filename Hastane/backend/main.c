/*
 * main.c
 * Hastane Randevu ve Hasta Öncelik Sistemi - HTTP Sunucu
 *
 * POSIX socket API ile minimal HTTP/1.1 sunucu implementasyonu.
 * Statik dosyaları ../frontend/ dizininden servis eder.
 * Her bağlantı için yeni bir pthread oluşturulur.
 * Tüm global veri yapıları tek bir mutex ile korunur.
 *
 * Derleme: make
 * Çalıştırma: ./hastane_server 8080
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdarg.h>

#include "hash_table.h"
#include "heap.h"
#include "queue.h"
#include "stack.h"
#include "tree.h"

/* ======================== Sabitler ======================== */
#define MAX_ISTEK_BOYUTU   65536
#define MAX_YANIT_BOYUTU   (1 << 21)  /* 2MB - büyük hasta listeleri için */
#define MAX_BASLIK_BOYUTU  1024

/* ======================== Global veri yapıları ======================== */
static HashTable*  hasta_tablosu   = NULL;
static MinHeap*    triaj_heap      = NULL;
static Queue*      randevu_kuyruğu = NULL;
static Stack*      islem_gecmisi   = NULL;
static AgacYapisi* poliklinik_agaci = NULL;

/*
 * Global mutex: tüm veri yapılarını thread'ler arası yarış koşullarından korur.
 * Tek mutex basit ama güvenli; üretimde her yapı için ayrı mutex daha verimli olur.
 */
static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ======================== Yardımcı JSON fonksiyonları ======================== */

/* JSON string değerlerini kaçış karakterleriyle güvenli yazar */
static int json_yaz_string(char* buf, int pos, int max, const char* str) {
    if (!str) str = "";
    buf[pos++] = '"';
    for (int i = 0; str[i] && pos < max - 4; i++) {
        if (str[i] == '"')       { buf[pos++] = '\\'; buf[pos++] = '"'; }
        else if (str[i] == '\\') { buf[pos++] = '\\'; buf[pos++] = '\\'; }
        else if (str[i] == '\n') { buf[pos++] = '\\'; buf[pos++] = 'n'; }
        else if (str[i] == '\r') { buf[pos++] = '\\'; buf[pos++] = 'r'; }
        else buf[pos++] = str[i];
    }
    buf[pos++] = '"';
    return pos;
}

/* JSON body'sinden string değer çeker */
static void json_string_al(const char* json, const char* key, char* dst, int dst_size) {
    dst[0] = '\0';
    char arama[64];
    snprintf(arama, sizeof(arama), "\"%s\"", key);
    const char* p = strstr(json, arama);
    if (!p) return;
    p += strlen(arama);
    while (*p == ':' || *p == ' ') p++;
    if (*p != '"') return;
    p++; /* açılış tırnağını atla */
    int i = 0;
    while (*p && *p != '"' && i < dst_size - 1) {
        if (*p == '\\' && *(p+1)) { p++; } /* basit kaçış - sadece atla */
        dst[i++] = *p++;
    }
    dst[i] = '\0';
}

/* JSON body'sinden integer değer çeker */
static int json_int_al(const char* json, const char* key) {
    char arama[64];
    snprintf(arama, sizeof(arama), "\"%s\"", key);
    const char* p = strstr(json, arama);
    if (!p) return 0;
    p += strlen(arama);
    while (*p == ':' || *p == ' ') p++;
    return atoi(p);
}

/* Query string'den değer çeker: "?tc=12345" → "12345" */
static void query_deger_al(const char* query, const char* key, char* dst, int dst_size) {
    dst[0] = '\0';
    char arama[64];
    snprintf(arama, sizeof(arama), "%s=", key);
    const char* p = strstr(query, arama);
    if (!p) return;
    p += strlen(arama);
    int i = 0;
    while (*p && *p != '&' && i < dst_size - 1) {
        dst[i++] = *p++;
    }
    dst[i] = '\0';
}

/* URL decode: %XX ve + karakterlerini çözer */
static void url_decode(char* dst, const char* src, int dst_size) {
    int i = 0, j = 0;
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            char hex[3] = {src[i+1], src[i+2], '\0'};
            dst[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
}

/* ======================== HTTP istek ayrıştırma ======================== */

typedef struct {
    char method[10];
    char path[512];
    char query[512];
    char body[32768];
} HttpIstek;

/*
 * Ham HTTP isteğini ayrıştırır: method, path, query string, body.
 * İstek formatı: "METHOD /path?query HTTP/1.1\r\n...\r\n\r\nbody"
 */
static void istek_ayristir(const char* ham, HttpIstek* istek) {
    memset(istek, 0, sizeof(*istek));

    const char* p = ham;
    int i = 0;
    /* Method */
    while (*p && *p != ' ' && i < 9) istek->method[i++] = *p++;
    istek->method[i] = '\0';
    if (*p == ' ') p++;

    /* Path + query */
    char tam_yol[512] = {0};
    i = 0;
    while (*p && *p != ' ' && i < 511) tam_yol[i++] = *p++;
    tam_yol[i] = '\0';

    char* soru = strchr(tam_yol, '?');
    if (soru) {
        int uzunluk = (int)(soru - tam_yol);
        strncpy(istek->path, tam_yol, uzunluk);
        istek->path[uzunluk] = '\0';
        strncpy(istek->query, soru + 1, 511);
    } else {
        strncpy(istek->path, tam_yol, 511);
    }

    /* Body: \r\n\r\n'den sonrası */
    const char* govde_basi = strstr(ham, "\r\n\r\n");
    if (govde_basi) {
        govde_basi += 4;
        strncpy(istek->body, govde_basi, sizeof(istek->body) - 1);
    }
}

/* ======================== HTTP yanıt gönderme ======================== */

static void yanit_gonder(int fd, int durum_kodu, const char* icerik_tipi,
                          const char* govde, size_t govde_uzunlugu) {
    const char* durum_metni;
    switch (durum_kodu) {
        case 200: durum_metni = "OK";                    break;
        case 201: durum_metni = "Created";               break;
        case 400: durum_metni = "Bad Request";           break;
        case 404: durum_metni = "Not Found";             break;
        case 409: durum_metni = "Conflict";              break;
        case 500: durum_metni = "Internal Server Error"; break;
        default:  durum_metni = "OK";
    }

    char baslik[MAX_BASLIK_BOYUTU];
    int baslik_uzunlugu = snprintf(baslik, sizeof(baslik),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n"
        "\r\n",
        durum_kodu, durum_metni, icerik_tipi, govde_uzunlugu);

    write(fd, baslik, baslik_uzunlugu);
    if (govde && govde_uzunlugu > 0) {
        write(fd, govde, govde_uzunlugu);
    }
}

static void json_yanit(int fd, int durum, const char* json) {
    yanit_gonder(fd, durum, "application/json", json, strlen(json));
}

static void basari_json(int fd, const char* mesaj) {
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"basari\":true,\"mesaj\":\"%s\"}", mesaj);
    json_yanit(fd, 200, buf);
}

static void hata_json(int fd, int kod, const char* mesaj) {
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"basari\":false,\"mesaj\":\"%s\"}", mesaj);
    json_yanit(fd, kod, buf);
}

/* ======================== Statik dosya servisi ======================== */

static void statik_dosya_gonder(int fd, const char* dosya_yolu, const char* icerik_tipi) {
    FILE* dosya = fopen(dosya_yolu, "rb");
    if (!dosya) {
        hata_json(fd, 404, "Dosya bulunamadi");
        return;
    }

    fseek(dosya, 0, SEEK_END);
    long boyut = ftell(dosya);
    fseek(dosya, 0, SEEK_SET);

    if (boyut <= 0 || boyut > MAX_YANIT_BOYUTU) {
        fclose(dosya);
        hata_json(fd, 500, "Dosya boyutu hatasi");
        return;
    }

    char* icerik = (char*)malloc(boyut + 1);
    if (!icerik) { fclose(dosya); hata_json(fd, 500, "Bellek hatasi"); return; }

    size_t okunan = fread(icerik, 1, boyut, dosya);
    fclose(dosya);
    icerik[okunan] = '\0';

    yanit_gonder(fd, 200, icerik_tipi, icerik, okunan);
    free(icerik);
}

/* ======================== Hasta JSON yardımcısı ======================== */

static int hasta_json_yaz(char* buf, int pos, int max, const Hasta* h) {
    pos += snprintf(buf + pos, max - pos, "{\"tc\":");
    pos = json_yaz_string(buf, pos, max, h->tc);
    pos += snprintf(buf + pos, max - pos, ",\"ad\":");
    pos = json_yaz_string(buf, pos, max, h->ad);
    pos += snprintf(buf + pos, max - pos, ",\"soyad\":");
    pos = json_yaz_string(buf, pos, max, h->soyad);
    pos += snprintf(buf + pos, max - pos, ",\"yas\":%d,\"poliklinik\":", h->yas);
    pos = json_yaz_string(buf, pos, max, h->poliklinik);
    pos += snprintf(buf + pos, max - pos, ",\"telefon\":");
    pos = json_yaz_string(buf, pos, max, h->telefon);
    pos += snprintf(buf + pos, max - pos, "}");
    return pos;
}

/* Hasta verilerini undo_data JSON formatına dönüştürür */
static void hasta_undo_json_yaz(char* buf, int boyut, const Hasta* h) {
    snprintf(buf, boyut,
        "{\"tc\":\"%s\",\"ad\":\"%s\",\"soyad\":\"%s\","
        "\"yas\":%d,\"poliklinik\":\"%s\",\"telefon\":\"%s\"}",
        h->tc, h->ad, h->soyad, h->yas, h->poliklinik, h->telefon);
}

/* ======================== API İşleyicileri ======================== */

/* GET /api/dashboard */
static void islem_dashboard(int fd) {
    pthread_mutex_lock(&global_mutex);

    int hasta_sayi   = hasta_tablosu ? hasta_tablosu->toplam_hasta : 0;
    int kuyruk_sayi  = randevu_kuyruğu ? randevu_kuyruğu->boyut : 0;
    int triaj_sayi   = triaj_heap ? triaj_heap->boyut : 0;

    /* Bugünkü işlem sayısını say */
    time_t simdi = time(NULL);
    struct tm* bugun = localtime(&simdi);
    int bugun_sayi = 0;
    if (islem_gecmisi) {
        StackNode* n = islem_gecmisi->tepe;
        while (n) {
            struct tm* islem_tm = localtime(&n->zaman);
            if (islem_tm->tm_mday == bugun->tm_mday &&
                islem_tm->tm_mon  == bugun->tm_mon  &&
                islem_tm->tm_year == bugun->tm_year) {
                bugun_sayi++;
            }
            n = n->sonraki;
        }
    }

    /* Kuyruk listesi */
    int qsayi = 0;
    QueueNode* qkopya = kuyruk_durumu_goster(randevu_kuyruğu, &qsayi);

    /* Triaj listesi */
    int tsayi = 0;
    TriajHasta* tkopya = triaj_listesi_goster(triaj_heap, &tsayi);

    pthread_mutex_unlock(&global_mutex);

    char* buf = (char*)malloc(MAX_YANIT_BOYUTU);
    if (!buf) { hata_json(fd, 500, "Bellek hatasi"); return; }

    int pos = 0;
    pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos,
        "{\"toplam_hasta\":%d,\"kuyruk_sayi\":%d,"
        "\"triaj_sayi\":%d,\"bugun_islem\":%d,",
        hasta_sayi, kuyruk_sayi, triaj_sayi, bugun_sayi);

    /* Normal kuyruk */
    pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos, "\"kuyruk\":[");
    for (int i = 0; i < qsayi; i++) {
        if (i > 0) buf[pos++] = ',';
        pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos,
            "{\"sira\":%d,\"tc\":\"%s\",\"ad\":\"%s\",\"soyad\":\"%s\","
            "\"poliklinik\":\"%s\",\"saat\":\"%s\"}",
            i + 1, qkopya[i].tc, qkopya[i].ad, qkopya[i].soyad,
            qkopya[i].poliklinik, qkopya[i].randevu_saati);
    }
    pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos, "],");

    /* Triaj kuyruğu */
    pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos, "\"triaj\":[");
    for (int i = 0; i < tsayi; i++) {
        if (i > 0) buf[pos++] = ',';
        pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos,
            "{\"tc\":\"%s\",\"ad\":\"%s\",\"soyad\":\"%s\",\"skor\":%d}",
            tkopya[i].tc, tkopya[i].ad, tkopya[i].soyad, tkopya[i].aciliyet_skoru);
    }
    pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos, "]}");
    buf[pos] = '\0';

    free(qkopya);
    free(tkopya);
    json_yanit(fd, 200, buf);
    free(buf);
}

/* POST /api/hasta */
static void islem_hasta_ekle(int fd, const char* body) {
    char tc[12]={0}, ad[50]={0}, soyad[50]={0}, poliklinik[50]={0}, telefon[15]={0};
    int  yas = 0;

    json_string_al(body, "tc",          tc,          12);
    json_string_al(body, "ad",          ad,          50);
    json_string_al(body, "soyad",       soyad,       50);
    json_string_al(body, "poliklinik",  poliklinik,  50);
    json_string_al(body, "telefon",     telefon,     15);
    yas = json_int_al(body, "yas");

    if (tc[0] == '\0' || ad[0] == '\0') {
        hata_json(fd, 400, "TC ve ad zorunludur"); return;
    }

    pthread_mutex_lock(&global_mutex);
    int sonuc = hasta_ekle(hasta_tablosu, tc, ad, soyad, yas, poliklinik, telefon);

    if (sonuc == 0) {
        char aciklama[200], undo[512];
        snprintf(aciklama, sizeof(aciklama), "%s %s hasta olarak kaydedildi", ad, soyad);
        /* Undo: TC'yi silerek geri alınır */
        snprintf(undo, sizeof(undo), "{\"tc\":\"%s\"}", tc);
        islem_kaydet(islem_gecmisi, "HASTA_EKLE", tc, aciklama, undo);
    }
    pthread_mutex_unlock(&global_mutex);

    if (sonuc == 0)   basari_json(fd, "Hasta basariyla eklendi");
    else if (sonuc == -2) hata_json(fd, 409, "Bu TC ile hasta zaten kayitli");
    else              hata_json(fd, 500, "Hasta eklenemedi");
}

/* GET /api/hasta?tc=... */
static void islem_hasta_ara(int fd, const char* query) {
    char tc[12] = {0};
    query_deger_al(query, "tc", tc, 12);
    if (tc[0] == '\0') { hata_json(fd, 400, "TC parametresi eksik"); return; }

    pthread_mutex_lock(&global_mutex);
    Hasta* h = hasta_ara(hasta_tablosu, tc);
    pthread_mutex_unlock(&global_mutex);

    if (!h) { hata_json(fd, 404, "Hasta bulunamadi"); return; }

    char buf[512];
    int pos = hasta_json_yaz(buf, 0, sizeof(buf), h);
    buf[pos] = '\0';
    json_yanit(fd, 200, buf);
}

/* GET /api/hastalar */
static void islem_hastalar_listele(int fd) {
    pthread_mutex_lock(&global_mutex);
    int sayi = 0;
    Hasta* liste = tum_hastalari_listele(hasta_tablosu, &sayi);
    pthread_mutex_unlock(&global_mutex);

    char* buf = (char*)malloc(MAX_YANIT_BOYUTU);
    if (!buf) { free(liste); hata_json(fd, 500, "Bellek hatasi"); return; }

    int pos = 0;
    buf[pos++] = '[';
    for (int i = 0; i < sayi; i++) {
        if (i > 0) buf[pos++] = ',';
        pos = hasta_json_yaz(buf, pos, MAX_YANIT_BOYUTU - 2, &liste[i]);
    }
    buf[pos++] = ']';
    buf[pos]   = '\0';

    free(liste);
    json_yanit(fd, 200, buf);
    free(buf);
}

/* PUT /api/hasta/:tc */
static void islem_hasta_guncelle(int fd, const char* tc_param, const char* body) {
    char tc[12]={0}, ad[50]={0}, soyad[50]={0}, poliklinik[50]={0}, telefon[15]={0};
    int  yas = 0;

    url_decode(tc, tc_param, 12);
    json_string_al(body, "ad",         ad,         50);
    json_string_al(body, "soyad",      soyad,      50);
    json_string_al(body, "poliklinik", poliklinik, 50);
    json_string_al(body, "telefon",    telefon,    15);
    yas = json_int_al(body, "yas");

    pthread_mutex_lock(&global_mutex);
    /* Geri alma için eski veriyi kaydet */
    Hasta* eski = hasta_ara(hasta_tablosu, tc);
    char undo[512] = {0};
    if (eski) hasta_undo_json_yaz(undo, sizeof(undo), eski);

    int sonuc = hasta_guncelle(hasta_tablosu, tc, ad, soyad, yas, poliklinik, telefon);
    if (sonuc == 0) {
        char aciklama[200];
        snprintf(aciklama, sizeof(aciklama), "TC %s guncellemesi yapildi", tc);
        islem_kaydet(islem_gecmisi, "HASTA_GUNCELLE", tc, aciklama, undo);
    }
    pthread_mutex_unlock(&global_mutex);

    if (sonuc == 0) basari_json(fd, "Hasta guncellendi");
    else            hata_json(fd, 404, "Hasta bulunamadi");
}

/* DELETE /api/hasta/:tc */
static void islem_hasta_sil(int fd, const char* tc_param) {
    char tc[12] = {0};
    url_decode(tc, tc_param, 12);

    pthread_mutex_lock(&global_mutex);
    /* Silmeden önce veriyi kaydet (geri alma için) */
    Hasta* h = hasta_ara(hasta_tablosu, tc);
    char undo[512] = {0};
    if (h) hasta_undo_json_yaz(undo, sizeof(undo), h);

    int sonuc = hasta_sil(hasta_tablosu, tc);
    if (sonuc == 0) {
        char aciklama[200];
        snprintf(aciklama, sizeof(aciklama), "TC %s hastasi silindi", tc);
        islem_kaydet(islem_gecmisi, "HASTA_SIL", tc, aciklama, undo);
    }
    pthread_mutex_unlock(&global_mutex);

    if (sonuc == 0) basari_json(fd, "Hasta silindi");
    else            hata_json(fd, 404, "Hasta bulunamadi");
}

/* POST /api/randevu */
static void islem_randevu_ekle(int fd, const char* body) {
    char tc[12]={0}, ad[50]={0}, soyad[50]={0}, poliklinik[50]={0}, saat[10]={0};

    json_string_al(body, "tc",          tc,         12);
    json_string_al(body, "ad",          ad,         50);
    json_string_al(body, "soyad",       soyad,      50);
    json_string_al(body, "poliklinik",  poliklinik, 50);
    json_string_al(body, "saat",        saat,       10);

    if (tc[0] == '\0') { hata_json(fd, 400, "TC zorunludur"); return; }

    pthread_mutex_lock(&global_mutex);
    int sonuc = randevu_al(randevu_kuyruğu, tc, ad, soyad, poliklinik, saat);
    if (sonuc == 0) {
        char aciklama[200], undo[64];
        snprintf(aciklama, sizeof(aciklama), "%s %s randevu aldi (%s)", ad, soyad, poliklinik);
        snprintf(undo, sizeof(undo), "{\"tc\":\"%s\"}", tc);
        islem_kaydet(islem_gecmisi, "RANDEVU_AL", tc, aciklama, undo);
    }
    pthread_mutex_unlock(&global_mutex);

    if (sonuc == 0) basari_json(fd, "Randevu alindi");
    else            hata_json(fd, 500, "Randevu alinamadi");
}

/* GET /api/randevu/kuyruk */
static void islem_randevu_kuyruk(int fd) {
    pthread_mutex_lock(&global_mutex);
    int sayi = 0;
    QueueNode* liste = kuyruk_durumu_goster(randevu_kuyruğu, &sayi);
    pthread_mutex_unlock(&global_mutex);

    char* buf = (char*)malloc(MAX_YANIT_BOYUTU);
    if (!buf) { free(liste); hata_json(fd, 500, "Bellek hatasi"); return; }

    int pos = 0;
    buf[pos++] = '[';
    for (int i = 0; i < sayi; i++) {
        if (i > 0) buf[pos++] = ',';
        pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos,
            "{\"sira\":%d,\"tc\":\"%s\",\"ad\":\"%s\",\"soyad\":\"%s\","
            "\"poliklinik\":\"%s\",\"saat\":\"%s\"}",
            i + 1, liste[i].tc, liste[i].ad, liste[i].soyad,
            liste[i].poliklinik, liste[i].randevu_saati);
    }
    buf[pos++] = ']';
    buf[pos]   = '\0';

    free(liste);
    json_yanit(fd, 200, buf);
    free(buf);
}

/* POST /api/randevu/cagir */
static void islem_randevu_cagir(int fd) {
    pthread_mutex_lock(&global_mutex);
    QueueNode* hasta = siradaki_hasta_cagir(randevu_kuyruğu);
    if (hasta) {
        char aciklama[200];
        snprintf(aciklama, sizeof(aciklama), "%s %s cagrildi (%s)",
                 hasta->ad, hasta->soyad, hasta->poliklinik);
        islem_kaydet(islem_gecmisi, "RANDEVU_CAGIR", hasta->tc, aciklama, "");
    }
    pthread_mutex_unlock(&global_mutex);

    if (!hasta) { hata_json(fd, 404, "Kuyrukta hasta yok"); return; }

    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"basari\":true,\"hasta\":{\"tc\":\"%s\",\"ad\":\"%s\","
        "\"soyad\":\"%s\",\"poliklinik\":\"%s\",\"saat\":\"%s\"}}",
        hasta->tc, hasta->ad, hasta->soyad, hasta->poliklinik, hasta->randevu_saati);
    free(hasta);
    json_yanit(fd, 200, buf);
}

/* DELETE /api/randevu/:tc */
static void islem_randevu_iptal(int fd, const char* tc_param) {
    char tc[12] = {0};
    url_decode(tc, tc_param, 12);

    pthread_mutex_lock(&global_mutex);
    int sonuc = randevu_iptal(randevu_kuyruğu, tc);
    if (sonuc == 0) {
        char aciklama[100];
        snprintf(aciklama, sizeof(aciklama), "TC %s randevusu iptal edildi", tc);
        islem_kaydet(islem_gecmisi, "RANDEVU_IPTAL", tc, aciklama, "");
    }
    pthread_mutex_unlock(&global_mutex);

    if (sonuc == 0) basari_json(fd, "Randevu iptal edildi");
    else            hata_json(fd, 404, "Randevu bulunamadi");
}

/* POST /api/triaj */
static void islem_triaj_ekle(int fd, const char* body) {
    char tc[12]={0}, ad[50]={0}, soyad[50]={0};
    int  skor = 0;

    json_string_al(body, "tc",    tc,    12);
    json_string_al(body, "ad",    ad,    50);
    json_string_al(body, "soyad", soyad, 50);
    skor = json_int_al(body, "skor");

    if (tc[0] == '\0' || skor < 1 || skor > 10) {
        hata_json(fd, 400, "TC ve gecerli skor (1-10) zorunludur"); return;
    }

    pthread_mutex_lock(&global_mutex);
    int sonuc = triaj_ekle(triaj_heap, tc, ad, soyad, skor);
    if (sonuc == 0) {
        char aciklama[200], undo[64];
        snprintf(aciklama, sizeof(aciklama), "%s %s triaja eklendi (skor=%d)", ad, soyad, skor);
        snprintf(undo, sizeof(undo), "{\"tc\":\"%s\"}", tc);
        islem_kaydet(islem_gecmisi, "TRIAJ_EKLE", tc, aciklama, undo);
    }
    pthread_mutex_unlock(&global_mutex);

    if (sonuc == 0)  basari_json(fd, "Triaja eklendi");
    else if (sonuc == -2) hata_json(fd, 500, "Triaj kapasitesi dolu");
    else             hata_json(fd, 400, "Gecersiz skor");
}

/* GET /api/triaj/kuyruk */
static void islem_triaj_kuyruk(int fd) {
    pthread_mutex_lock(&global_mutex);
    int sayi = 0;
    TriajHasta* liste = triaj_listesi_goster(triaj_heap, &sayi);
    pthread_mutex_unlock(&global_mutex);

    char* buf = (char*)malloc(MAX_YANIT_BOYUTU);
    if (!buf) { free(liste); hata_json(fd, 500, "Bellek hatasi"); return; }

    int pos = 0;
    buf[pos++] = '[';
    for (int i = 0; i < sayi; i++) {
        if (i > 0) buf[pos++] = ',';
        pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos,
            "{\"tc\":\"%s\",\"ad\":\"%s\",\"soyad\":\"%s\",\"skor\":%d}",
            liste[i].tc, liste[i].ad, liste[i].soyad, liste[i].aciliyet_skoru);
    }
    buf[pos++] = ']';
    buf[pos]   = '\0';

    free(liste);
    json_yanit(fd, 200, buf);
    free(buf);
}

/* POST /api/triaj/cagir */
static void islem_triaj_cagir(int fd) {
    pthread_mutex_lock(&global_mutex);
    TriajHasta* hasta = en_acil_hastay_al(triaj_heap);
    if (hasta) {
        char aciklama[200];
        snprintf(aciklama, sizeof(aciklama), "%s %s triajdan cagrildi (skor=%d)",
                 hasta->ad, hasta->soyad, hasta->aciliyet_skoru);
        islem_kaydet(islem_gecmisi, "TRIAJ_CAGIR", hasta->tc, aciklama, "");
    }
    pthread_mutex_unlock(&global_mutex);

    if (!hasta) { hata_json(fd, 404, "Triajda hasta yok"); return; }

    char buf[400];
    snprintf(buf, sizeof(buf),
        "{\"basari\":true,\"hasta\":{\"tc\":\"%s\",\"ad\":\"%s\","
        "\"soyad\":\"%s\",\"skor\":%d}}",
        hasta->tc, hasta->ad, hasta->soyad, hasta->aciliyet_skoru);
    free(hasta);
    json_yanit(fd, 200, buf);
}

/* GET /api/poliklinikler */
static void islem_poliklinikler(int fd) {
    pthread_mutex_lock(&global_mutex);
    char* json = agac_json_olustur(poliklinik_agaci);
    pthread_mutex_unlock(&global_mutex);

    if (!json) { hata_json(fd, 500, "Agac olusturulamadi"); return; }
    json_yanit(fd, 200, json);
    free(json);
}

/* POST /api/poliklinik */
static void islem_poliklinik_ekle(int fd, const char* body) {
    char ebeveyn[100]={0}, yeni[100]={0};
    json_string_al(body, "ebeveyn", ebeveyn, 100);
    json_string_al(body, "ad",     yeni,    100);

    if (ebeveyn[0] == '\0' || yeni[0] == '\0') {
        hata_json(fd, 400, "ebeveyn ve ad zorunludur"); return;
    }

    pthread_mutex_lock(&global_mutex);
    int sonuc = poliklinik_ekle(poliklinik_agaci, ebeveyn, yeni);
    if (sonuc == 0) {
        char aciklama[200];
        snprintf(aciklama, sizeof(aciklama), "%s altina %s eklendi", ebeveyn, yeni);
        islem_kaydet(islem_gecmisi, "POLIKLINIK_EKLE", "", aciklama, yeni);
    }
    pthread_mutex_unlock(&global_mutex);

    if (sonuc == 0)  basari_json(fd, "Poliklinik eklendi");
    else if (sonuc == -2) hata_json(fd, 404, "Ebeveyn poliklinik bulunamadi");
    else if (sonuc == -4) hata_json(fd, 409, "Bu isimde poliklinik zaten var");
    else             hata_json(fd, 500, "Eklenemedi");
}

/* DELETE /api/poliklinik/:ad */
static void islem_poliklinik_sil(int fd, const char* ad_param) {
    char ad[100] = {0};
    url_decode(ad, ad_param, 100);

    pthread_mutex_lock(&global_mutex);
    int sonuc = poliklinik_sil(poliklinik_agaci, ad);
    if (sonuc == 0) {
        char aciklama[200];
        snprintf(aciklama, sizeof(aciklama), "%s polikilnigi silindi", ad);
        islem_kaydet(islem_gecmisi, "POLIKLINIK_SIL", "", aciklama, ad);
    }
    pthread_mutex_unlock(&global_mutex);

    if (sonuc == 0)  basari_json(fd, "Poliklinik silindi");
    else if (sonuc == -2) hata_json(fd, 400, "Kok dugum silinemez");
    else             hata_json(fd, 404, "Poliklinik bulunamadi");
}

/* GET /api/gecmis */
static void islem_gecmis(int fd) {
    pthread_mutex_lock(&global_mutex);
    int sayi = 0;
    StackNode* liste = gecmis_goster(islem_gecmisi, 50, &sayi);
    pthread_mutex_unlock(&global_mutex);

    char* buf = (char*)malloc(MAX_YANIT_BOYUTU);
    if (!buf) { free(liste); hata_json(fd, 500, "Bellek hatasi"); return; }

    int pos = 0;
    buf[pos++] = '[';
    for (int i = 0; i < sayi; i++) {
        if (i > 0) buf[pos++] = ',';
        char zaman_str[32];
        struct tm* tm_info = localtime(&liste[i].zaman);
        strftime(zaman_str, sizeof(zaman_str), "%Y-%m-%d %H:%M:%S", tm_info);

        pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos,
            "{\"tip\":\"%s\",\"tc\":\"%s\",\"aciklama\":\"%s\",\"zaman\":\"%s\"}",
            liste[i].islem_tipi, liste[i].tc, liste[i].aciklama, zaman_str);
    }
    buf[pos++] = ']';
    buf[pos]   = '\0';

    free(liste);
    json_yanit(fd, 200, buf);
    free(buf);
}

/* POST /api/gecmis/geri-al */
static void islem_geri_al(int fd) {
    pthread_mutex_lock(&global_mutex);
    StackNode* islem = son_islemi_geri_al(islem_gecmisi);

    if (!islem) {
        pthread_mutex_unlock(&global_mutex);
        hata_json(fd, 404, "Geri alinacak islem yok");
        return;
    }

    char mesaj[300] = {0};

    /* İşlem tipine göre geri alma mantığı */
    if (strcmp(islem->islem_tipi, "HASTA_EKLE") == 0) {
        /* Eklenen hastayı sil */
        hasta_sil(hasta_tablosu, islem->tc);
        snprintf(mesaj, sizeof(mesaj), "HASTA_EKLE geri alindi: TC %s silindi", islem->tc);
    }
    else if (strcmp(islem->islem_tipi, "HASTA_SIL") == 0) {
        /* Silinen hastayı geri yükle */
        char tc[12]={0}, ad[50]={0}, soyad[50]={0}, pol[50]={0}, tel[15]={0};
        int yas = 0;
        json_string_al(islem->undo_data, "tc",         tc,  12);
        json_string_al(islem->undo_data, "ad",         ad,  50);
        json_string_al(islem->undo_data, "soyad",      soyad, 50);
        json_string_al(islem->undo_data, "poliklinik", pol, 50);
        json_string_al(islem->undo_data, "telefon",    tel, 15);
        yas = json_int_al(islem->undo_data, "yas");
        hasta_ekle(hasta_tablosu, tc, ad, soyad, yas, pol, tel);
        snprintf(mesaj, sizeof(mesaj), "HASTA_SIL geri alindi: TC %s geri yuklendi", islem->tc);
    }
    else if (strcmp(islem->islem_tipi, "HASTA_GUNCELLE") == 0) {
        /* Güncelleme öncesi veriyi geri yükle */
        char tc[12]={0}, ad[50]={0}, soyad[50]={0}, pol[50]={0}, tel[15]={0};
        int yas = 0;
        json_string_al(islem->undo_data, "tc",         tc,  12);
        json_string_al(islem->undo_data, "ad",         ad,  50);
        json_string_al(islem->undo_data, "soyad",      soyad, 50);
        json_string_al(islem->undo_data, "poliklinik", pol, 50);
        json_string_al(islem->undo_data, "telefon",    tel, 15);
        yas = json_int_al(islem->undo_data, "yas");
        hasta_guncelle(hasta_tablosu, tc, ad, soyad, yas, pol, tel);
        snprintf(mesaj, sizeof(mesaj), "HASTA_GUNCELLE geri alindi: TC %s eski veriye donduruldu", islem->tc);
    }
    else if (strcmp(islem->islem_tipi, "RANDEVU_AL") == 0) {
        randevu_iptal(randevu_kuyruğu, islem->tc);
        snprintf(mesaj, sizeof(mesaj), "RANDEVU_AL geri alindi: TC %s randevusu iptal edildi", islem->tc);
    }
    else if (strcmp(islem->islem_tipi, "POLIKLINIK_EKLE") == 0) {
        poliklinik_sil(poliklinik_agaci, islem->undo_data);
        snprintf(mesaj, sizeof(mesaj), "POLIKLINIK_EKLE geri alindi: %s silindi", islem->undo_data);
    }
    else {
        snprintf(mesaj, sizeof(mesaj), "%s islemi geri alindi (kayit silindi)", islem->islem_tipi);
    }

    pthread_mutex_unlock(&global_mutex);

    free(islem);

    char buf[400];
    snprintf(buf, sizeof(buf), "{\"basari\":true,\"mesaj\":\"%s\"}", mesaj);
    json_yanit(fd, 200, buf);
}

/* GET /api/istatistikler
 * Hash Table bucket dağılımı, poliklinik başına hasta sayısı,
 * triaj skor dağılımı ve veri yapısı doluluk bilgilerini döndürür.
 * Tüm hesaplamalar O(HASH_TABLE_BOYUTU + n) zaman ile tamamlanır.
 */
static void islem_istatistikler(int fd) {
    pthread_mutex_lock(&global_mutex);

    /* ── Hash Table Analizi ──
     * Tüm bucket'ları gezerek zincir uzunluk dağılımını hesapla.
     * Bu bilgi hash fonksiyonunun ne kadar düzgün dağıttığını gösterir.
     * Uzun zincirler = çakışma artışı = arama performansı düşer.
     */
    int dolu_bucket   = 0;
    int max_zincir    = 0;
    /* Zincir uzunluk histogram: index=uzunluk (9+ birlikte) */
    int zincir_hist[10] = {0};

    for (int i = 0; i < HASH_TABLE_BOYUTU; i++) {
        int uzunluk = 0;
        HastaNode* n = hasta_tablosu->buckets[i];
        while (n) { uzunluk++; n = n->sonraki; }
        if (uzunluk > 0) dolu_bucket++;
        if (uzunluk > max_zincir) max_zincir = uzunluk;
        int idx = (uzunluk < 9) ? uzunluk : 9;
        zincir_hist[idx]++;
    }
    float yuk_faktoru = (HASH_TABLE_BOYUTU > 0)
        ? (float)hasta_tablosu->toplam_hasta / HASH_TABLE_BOYUTU : 0.0f;

    /* ── Poliklinik Başına Hasta Sayısı ──
     * Hash table'ı tek geçişte tara, poliklinik adlarını grupla.
     * Bu O(n + HASH_TABLE_BOYUTU) işlemdir.
     */
#define MAX_POL_ISTAT 60
    char pol_ad[MAX_POL_ISTAT][52];
    int  pol_cnt[MAX_POL_ISTAT];
    int  pol_top = 0;

    for (int i = 0; i < HASH_TABLE_BOYUTU; i++) {
        HastaNode* n = hasta_tablosu->buckets[i];
        while (n) {
            int bulundu = 0;
            for (int j = 0; j < pol_top; j++) {
                if (strcmp(pol_ad[j], n->hasta.poliklinik) == 0) {
                    pol_cnt[j]++; bulundu = 1; break;
                }
            }
            if (!bulundu && pol_top < MAX_POL_ISTAT) {
                strncpy(pol_ad[pol_top], n->hasta.poliklinik, 51);
                pol_ad[pol_top][51] = '\0';
                pol_cnt[pol_top++] = 1;
            }
            n = n->sonraki;
        }
    }

    /* Basit insertion sort: fazla sayıya göre büyükten küçüğe */
    for (int i = 1; i < pol_top; i++) {
        int key = pol_cnt[i]; char key_ad[52];
        strncpy(key_ad, pol_ad[i], 51); key_ad[51] = '\0';
        int j = i - 1;
        while (j >= 0 && pol_cnt[j] < key) {
            pol_cnt[j+1] = pol_cnt[j];
            strncpy(pol_ad[j+1], pol_ad[j], 51);
            j--;
        }
        pol_cnt[j+1] = key;
        strncpy(pol_ad[j+1], key_ad, 51);
    }

    /* ── Triaj Skor Dağılımı ──
     * Min-Heap'teki tüm elemanları ziyaret et, skor gruplarını say.
     * Heap sıralı değil, tüm elemanları gezmek O(n).
     */
    int skor_dag[11] = {0}; /* indeks 1-10 */
    for (int i = 0; i < triaj_heap->boyut; i++) {
        int s = triaj_heap->hastalar[i].aciliyet_skoru;
        if (s >= 1 && s <= 10) skor_dag[s]++;
    }

    int heap_boyut   = triaj_heap->boyut;
    int stack_boyut  = islem_gecmisi->boyut;
    int kuyruk_boyut = randevu_kuyruğu->boyut;

    pthread_mutex_unlock(&global_mutex);

    /* ── JSON Çıktısı ── */
    char* buf = (char*)malloc(MAX_YANIT_BOYUTU);
    if (!buf) { hata_json(fd, 500, "Bellek hatasi"); return; }
    int pos = 0;

    /* Hash table istatistikleri */
    pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos,
        "{\"hash_table\":{"
        "\"toplam_hasta\":%d,\"dolu_bucket\":%d,\"toplam_bucket\":%d,"
        "\"yuk_faktoru\":%.4f,\"max_zincir\":%d,\"zincir_hist\":[",
        hasta_tablosu->toplam_hasta, dolu_bucket, HASH_TABLE_BOYUTU,
        yuk_faktoru, max_zincir);
    for (int i = 0; i < 10; i++) {
        if (i > 0) buf[pos++] = ',';
        pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos, "%d", zincir_hist[i]);
    }
    pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos, "]},");

    /* Poliklinik dağılımı (max 10 göster) */
    int goster = (pol_top < 10) ? pol_top : 10;
    pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos, "\"poliklinik_dagilimi\":[");
    for (int i = 0; i < goster; i++) {
        if (i > 0) buf[pos++] = ',';
        pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos,
            "{\"ad\":\"%s\",\"sayi\":%d}", pol_ad[i], pol_cnt[i]);
    }
    pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos, "],");

    /* Triaj skor dağılımı (skor 1–10) */
    pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos, "\"triaj_skorlar\":[");
    for (int s = 1; s <= 10; s++) {
        if (s > 1) buf[pos++] = ',';
        pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos,
            "{\"skor\":%d,\"sayi\":%d}", s, skor_dag[s]);
    }
    pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos, "],");

    /* Veri yapısı kapasiteleri */
    pos += snprintf(buf + pos, MAX_YANIT_BOYUTU - pos,
        "\"heap_boyut\":%d,\"heap_kapasite\":%d,"
        "\"stack_boyut\":%d,\"kuyruk_boyut\":%d}",
        heap_boyut, MAX_TRIAJ_HASTA, stack_boyut, kuyruk_boyut);
    buf[pos] = '\0';

    json_yanit(fd, 200, buf);
    free(buf);
}

/* ======================== İstek yönlendirme ======================== */

static void istegi_isle(int fd, HttpIstek* istek) {
    /* OPTIONS preflight isteğini yanıtla */
    if (strcmp(istek->method, "OPTIONS") == 0) {
        yanit_gonder(fd, 200, "text/plain", "", 0);
        return;
    }

    /* Statik dosyalar */
    if (strcmp(istek->path, "/") == 0 || strcmp(istek->path, "/index.html") == 0) {
        statik_dosya_gonder(fd, "../frontend/index.html", "text/html");
        return;
    }
    if (strcmp(istek->path, "/style.css") == 0) {
        statik_dosya_gonder(fd, "../frontend/style.css", "text/css");
        return;
    }
    if (strcmp(istek->path, "/app.js") == 0) {
        statik_dosya_gonder(fd, "../frontend/app.js", "application/javascript");
        return;
    }

    /* API rotaları - "/api/" önekini kontrol et */
    if (strncmp(istek->path, "/api/", 5) != 0) {
        hata_json(fd, 404, "Sayfa bulunamadi");
        return;
    }

    const char* api_yol = istek->path + 5; /* "/api/" sonrası */

    /* Path segmentlerini ayır */
    char seg1[100]={0}, seg2[200]={0};
    const char* bolme = strchr(api_yol, '/');
    if (bolme) {
        int uzunluk = (int)(bolme - api_yol);
        if (uzunluk >= (int)sizeof(seg1)) uzunluk = (int)sizeof(seg1) - 1;
        strncpy(seg1, api_yol, uzunluk);
        strncpy(seg2, bolme + 1, sizeof(seg2) - 1);
    } else {
        strncpy(seg1, api_yol, sizeof(seg1) - 1);
    }

    const char* m = istek->method;

    /* Dashboard */
    if (strcmp(seg1, "dashboard") == 0 && strcmp(m, "GET") == 0) {
        islem_dashboard(fd);
    }
    /* Hasta CRUD */
    else if (strcmp(seg1, "hasta") == 0) {
        if (strcmp(m, "POST") == 0)                             islem_hasta_ekle(fd, istek->body);
        else if (strcmp(m, "GET") == 0 && istek->query[0])     islem_hasta_ara(fd, istek->query);
        else if (strcmp(m, "PUT") == 0 && seg2[0])             islem_hasta_guncelle(fd, seg2, istek->body);
        else if (strcmp(m, "DELETE") == 0 && seg2[0])          islem_hasta_sil(fd, seg2);
        else hata_json(fd, 400, "Gecersiz istek");
    }
    else if (strcmp(seg1, "hastalar") == 0 && strcmp(m, "GET") == 0) {
        islem_hastalar_listele(fd);
    }
    /* Randevu */
    else if (strcmp(seg1, "randevu") == 0) {
        if (strcmp(m, "POST") == 0 && strcmp(seg2, "cagir") == 0)  islem_randevu_cagir(fd);
        else if (strcmp(m, "GET") == 0 && strcmp(seg2, "kuyruk") == 0) islem_randevu_kuyruk(fd);
        else if (strcmp(m, "POST") == 0 && seg2[0] == '\0')         islem_randevu_ekle(fd, istek->body);
        else if (strcmp(m, "DELETE") == 0 && seg2[0])               islem_randevu_iptal(fd, seg2);
        else hata_json(fd, 400, "Gecersiz istek");
    }
    /* Triaj */
    else if (strcmp(seg1, "triaj") == 0) {
        if (strcmp(m, "POST") == 0 && strcmp(seg2, "cagir") == 0)  islem_triaj_cagir(fd);
        else if (strcmp(m, "GET") == 0 && strcmp(seg2, "kuyruk") == 0) islem_triaj_kuyruk(fd);
        else if (strcmp(m, "POST") == 0 && seg2[0] == '\0')         islem_triaj_ekle(fd, istek->body);
        else hata_json(fd, 400, "Gecersiz istek");
    }
    /* Poliklinikler */
    else if (strcmp(seg1, "poliklinikler") == 0 && strcmp(m, "GET") == 0) {
        islem_poliklinikler(fd);
    }
    else if (strcmp(seg1, "poliklinik") == 0) {
        if (strcmp(m, "POST") == 0)                    islem_poliklinik_ekle(fd, istek->body);
        else if (strcmp(m, "DELETE") == 0 && seg2[0])  islem_poliklinik_sil(fd, seg2);
        else hata_json(fd, 400, "Gecersiz istek");
    }
    /* İstatistikler */
    else if (strcmp(seg1, "istatistikler") == 0 && strcmp(m, "GET") == 0) {
        islem_istatistikler(fd);
    }
    /* Geçmiş */
    else if (strcmp(seg1, "gecmis") == 0) {
        if (strcmp(m, "GET") == 0 && seg2[0] == '\0')              islem_gecmis(fd);
        else if (strcmp(m, "POST") == 0 && strcmp(seg2, "geri-al") == 0) islem_geri_al(fd);
        else hata_json(fd, 400, "Gecersiz istek");
    }
    else {
        hata_json(fd, 404, "Endpoint bulunamadi");
    }
}

/* ======================== Thread işleyicisi ======================== */

typedef struct {
    int fd;
} BaglantiArg;

static void* baglanti_isle(void* arg) {
    BaglantiArg* ba = (BaglantiArg*)arg;
    int fd = ba->fd;
    free(ba);

    pthread_detach(pthread_self()); /* Kaynakları otomatik serbest bırak */

    char* tampon = (char*)malloc(MAX_ISTEK_BOYUTU);
    if (!tampon) { close(fd); return NULL; }

    /* İsteği oku (büyük body'ler için döngü gerekebilir) */
    ssize_t toplam = 0;
    size_t baslik_sonu = 0;
    int icerik_uzunlugu = 0;

    while (toplam < MAX_ISTEK_BOYUTU - 1) {
        ssize_t okunan = read(fd, tampon + toplam, MAX_ISTEK_BOYUTU - toplam - 1);
        if (okunan <= 0) break;
        toplam += okunan;
        tampon[toplam] = '\0';

        if (!baslik_sonu) {
            char* son = strstr(tampon, "\r\n\r\n");
            if (son) {
                baslik_sonu = (size_t)(son - tampon) + 4;
                char* cl = strstr(tampon, "Content-Length: ");
                if (cl) icerik_uzunlugu = atoi(cl + 16);
            }
        }
        if (baslik_sonu && (size_t)toplam >= baslik_sonu + (size_t)icerik_uzunlugu) break;
    }

    if (toplam > 0) {
        HttpIstek istek;
        istek_ayristir(tampon, &istek);
        istegi_isle(fd, &istek);
    }

    free(tampon);
    close(fd);
    return NULL;
}

/* ======================== Veri yükleme ======================== */

/* hastalar.txt'den başlangıç verilerini yükler */
static void veri_yukle(const char* dosya_yolu) {
    FILE* f = fopen(dosya_yolu, "r");
    if (!f) return;

    char satir[200];
    int yuklenen = 0;
    /* Format: TC,AD,SOYAD,YAS,POLİKLİNİK,TELEFON */
    while (fgets(satir, sizeof(satir), f)) {
        if (satir[0] == '#' || satir[0] == '\n') continue;
        char tc[12]={0}, ad[50]={0}, soyad[50]={0}, pol[50]={0}, tel[15]={0};
        int  yas = 0;
        if (sscanf(satir, "%11[^,],%49[^,],%49[^,],%d,%49[^,],%14[^\n]",
                   tc, ad, soyad, &yas, pol, tel) == 6) {
            hasta_ekle(hasta_tablosu, tc, ad, soyad, yas, pol, tel);
            yuklenen++;
        }
    }
    fclose(f);
    printf("[Bilgi] %d hasta yuklendi: %s\n", yuklenen, dosya_yolu);
}

/* ======================== main ======================== */

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc >= 2) port = atoi(argv[1]);

    /* Veri yapılarını başlat */
    hasta_tablosu    = hash_table_olustur();
    triaj_heap       = heap_olustur();
    randevu_kuyruğu  = queue_olustur();
    islem_gecmisi    = stack_olustur();
    poliklinik_agaci = agac_olustur();

    if (!hasta_tablosu || !triaj_heap || !randevu_kuyruğu ||
        !islem_gecmisi || !poliklinik_agaci) {
        fprintf(stderr, "HATA: Veri yapilari baslatılamadi\n");
        return 1;
    }

    /* Başlangıç verilerini yükle */
    veri_yukle("../data/hastalar.txt");

    /* TCP sunucu soketi oluştur */
    int sunucu_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sunucu_fd < 0) {
        perror("socket");
        return 1;
    }

    /* SO_REUSEADDR: sunucu yeniden başlatıldığında aynı port kullanılabilsin */
    int seccenek = 1;
    setsockopt(sunucu_fd, SOL_SOCKET, SO_REUSEADDR, &seccenek, sizeof(seccenek));

    struct sockaddr_in adres;
    memset(&adres, 0, sizeof(adres));
    adres.sin_family      = AF_INET;
    adres.sin_addr.s_addr = INADDR_ANY;
    adres.sin_port        = htons((uint16_t)port);

    if (bind(sunucu_fd, (struct sockaddr*)&adres, sizeof(adres)) < 0) {
        perror("bind");
        close(sunucu_fd);
        return 1;
    }

    if (listen(sunucu_fd, 10) < 0) {
        perror("listen");
        close(sunucu_fd);
        return 1;
    }

    printf("========================================\n");
    printf("  Hastane Sistemi Sunucu Baslatildi\n");
    printf("  Port    : %d\n", port);
    printf("  Adres   : http://localhost:%d\n", port);
    printf("========================================\n");

    /* Ana kabul döngüsü */
    while (1) {
        struct sockaddr_in istemci_adresi;
        socklen_t istemci_uzunlugu = sizeof(istemci_adresi);
        int istemci_fd = accept(sunucu_fd, (struct sockaddr*)&istemci_adresi,
                                &istemci_uzunlugu);
        if (istemci_fd < 0) {
            if (errno == EINTR) continue; /* Sinyal ile kesintiye uğradı */
            perror("accept");
            continue;
        }

        /* Her bağlantı için yeni thread */
        BaglantiArg* arg = (BaglantiArg*)malloc(sizeof(BaglantiArg));
        if (!arg) { close(istemci_fd); continue; }
        arg->fd = istemci_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, baglanti_isle, arg) != 0) {
            free(arg);
            close(istemci_fd);
        }
    }

    /* Temizlik (normalde buraya ulaşılmaz) */
    close(sunucu_fd);
    hash_table_yok_et(hasta_tablosu);
    heap_yok_et(triaj_heap);
    queue_yok_et(randevu_kuyruğu);
    stack_yok_et(islem_gecmisi);
    agac_yok_et(poliklinik_agaci);
    pthread_mutex_destroy(&global_mutex);
    return 0;
}
