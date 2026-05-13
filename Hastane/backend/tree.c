/*
 * tree.c
 * Poliklinik hiyerarşi modülü implementasyonu.
 * Veri Yapısı: N-ary Tree (her düğümün en fazla MAX_COCUK çocuğu var)
 *
 * Düğüm bulma: DFS (Derinlik Önce Arama) - O(n), n = toplam düğüm sayısı.
 * Ekleme: Ebeveyn bulunduktan sonra O(1) diziye ekleme.
 * Silme: Ebeveyn'in cocuklar dizisinden çıkarma + alt ağacı özyinelemeli sil.
 * JSON: Özyinelemeli oluşturma, her düğüm için tek geçiş.
 *
 * Başlangıç ağacı prompt'taki hastane yapısıyla önceden doldurulur.
 */

#include "tree.h"
#include <stdarg.h>

/* Yeni ağaç düğümü oluşturur */
static TreeNode* dugum_olustur(const char* ad, TreeNode* ebeveyn) {
    TreeNode* dugum = (TreeNode*)calloc(1, sizeof(TreeNode));
    if (!dugum) return NULL;
    strncpy(dugum->ad, ad, MAX_AD_UZUNLUGU - 1);
    dugum->ad[MAX_AD_UZUNLUGU - 1] = '\0';
    dugum->ebeveyn = ebeveyn;
    dugum->cocuk_sayisi = 0;
    return dugum;
}

/* Bir düğüme çocuk ekler; ebeveyn'i de ayarlar */
static int cocuk_ekle(TreeNode* ebeveyn, TreeNode* cocuk) {
    if (!ebeveyn || !cocuk || ebeveyn->cocuk_sayisi >= MAX_COCUK) return -1;
    ebeveyn->cocuklar[ebeveyn->cocuk_sayisi++] = cocuk;
    cocuk->ebeveyn = ebeveyn;
    return 0;
}

/*
 * Prompt'taki hastane yapısını kurar.
 * Hastane → Dahiliye (Kardiyoloji, Nöroloji, Gastroenteroloji)
 *          → Cerrahi  (Genel Cerrahi, Ortopedi)
 *          → Acil     (Travma, Kardiyak Acil)
 */
AgacYapisi* agac_olustur(void) {
    AgacYapisi* agac = (AgacYapisi*)malloc(sizeof(AgacYapisi));
    if (!agac) return NULL;

    TreeNode* hastane = dugum_olustur("Hastane", NULL);
    if (!hastane) { free(agac); return NULL; }
    agac->kok = hastane;

    /* Ana birimler */
    TreeNode* dahiliye = dugum_olustur("Dahiliye", hastane);
    TreeNode* cerrahi  = dugum_olustur("Cerrahi",  hastane);
    TreeNode* acil     = dugum_olustur("Acil",     hastane);
    if (!dahiliye || !cerrahi || !acil) goto hata;
    cocuk_ekle(hastane, dahiliye);
    cocuk_ekle(hastane, cerrahi);
    cocuk_ekle(hastane, acil);

    /* Dahiliye alt birimleri */
    TreeNode* kardiyoloji     = dugum_olustur("Kardiyoloji",      dahiliye);
    TreeNode* noroloji        = dugum_olustur("Noroloji",         dahiliye);
    TreeNode* gastroenteroloji = dugum_olustur("Gastroenteroloji", dahiliye);
    if (!kardiyoloji || !noroloji || !gastroenteroloji) goto hata;
    cocuk_ekle(dahiliye, kardiyoloji);
    cocuk_ekle(dahiliye, noroloji);
    cocuk_ekle(dahiliye, gastroenteroloji);

    /* Cerrahi alt birimleri */
    TreeNode* genel_cerrahi = dugum_olustur("Genel Cerrahi", cerrahi);
    TreeNode* ortopedi      = dugum_olustur("Ortopedi",      cerrahi);
    if (!genel_cerrahi || !ortopedi) goto hata;
    cocuk_ekle(cerrahi, genel_cerrahi);
    cocuk_ekle(cerrahi, ortopedi);

    /* Acil alt birimleri */
    TreeNode* travma        = dugum_olustur("Travma",        acil);
    TreeNode* kardiyak_acil = dugum_olustur("Kardiyak Acil", acil);
    if (!travma || !kardiyak_acil) goto hata;
    cocuk_ekle(acil, travma);
    cocuk_ekle(acil, kardiyak_acil);

    return agac;

hata:
    agac_yok_et(agac);
    return NULL;
}

/* Özyinelemeli düğüm silme */
static void dugum_yok_et(TreeNode* dugum) {
    if (!dugum) return;
    for (int i = 0; i < dugum->cocuk_sayisi; i++) {
        dugum_yok_et(dugum->cocuklar[i]);
    }
    free(dugum);
}

void agac_yok_et(AgacYapisi* agac) {
    if (!agac) return;
    dugum_yok_et(agac->kok);
    free(agac);
}

/*
 * DFS ile ada göre düğüm arama - O(n).
 * İteratif (stack kullanmadan) özyinelemeli implementasyon.
 */
TreeNode* dugum_bul(TreeNode* kok, const char* ad) {
    if (!kok || !ad) return NULL;
    if (strcmp(kok->ad, ad) == 0) return kok;

    for (int i = 0; i < kok->cocuk_sayisi; i++) {
        TreeNode* bulunan = dugum_bul(kok->cocuklar[i], ad);
        if (bulunan) return bulunan;
    }
    return NULL;
}

int poliklinik_ekle(AgacYapisi* agac, const char* ebeveyn_adi, const char* yeni_ad) {
    if (!agac || !ebeveyn_adi || !yeni_ad) return -1;

    TreeNode* ebeveyn = dugum_bul(agac->kok, ebeveyn_adi);
    if (!ebeveyn) return -2; /* Ebeveyn bulunamadı */
    if (ebeveyn->cocuk_sayisi >= MAX_COCUK) return -3; /* Kapasite dolu */

    /* Aynı isimde düğüm var mı? */
    if (dugum_bul(agac->kok, yeni_ad)) return -4;

    TreeNode* yeni = dugum_olustur(yeni_ad, ebeveyn);
    if (!yeni) return -1;
    cocuk_ekle(ebeveyn, yeni);
    return 0;
}

int poliklinik_sil(AgacYapisi* agac, const char* ad) {
    if (!agac || !ad) return -1;
    if (strcmp(ad, "Hastane") == 0) return -2; /* Kök silinemez */

    TreeNode* hedef = dugum_bul(agac->kok, ad);
    if (!hedef) return -1;

    TreeNode* ebeveyn = hedef->ebeveyn;
    if (!ebeveyn) return -2;

    /* Ebeveyn'in cocuklar dizisinden çıkar (boşluğu kapat) */
    for (int i = 0; i < ebeveyn->cocuk_sayisi; i++) {
        if (ebeveyn->cocuklar[i] == hedef) {
            /* Son eleman ile doldur ve sayıyı azalt */
            ebeveyn->cocuklar[i] = ebeveyn->cocuklar[ebeveyn->cocuk_sayisi - 1];
            ebeveyn->cocuk_sayisi--;
            break;
        }
    }

    /* Alt ağacı özyinelemeli sil */
    dugum_yok_et(hedef);
    return 0;
}

/* JSON string'ine güvenli karakter ekler (alıntı ve ters eğik çizgi kaçar) */
static void json_string_ekle(char* buf, int* pos, int max, const char* str) {
    for (int i = 0; str[i] && *pos < max - 2; i++) {
        if (str[i] == '"' || str[i] == '\\') {
            buf[(*pos)++] = '\\';
        }
        buf[(*pos)++] = str[i];
    }
}

/*
 * Düğümü ve alt ağacını özyinelemeli olarak JSON'a dönüştürür.
 * Çıktı: {"ad":"...", "cocuklar":[...]}
 */
static void dugum_json(TreeNode* dugum, char* buf, int* pos, int max) {
    if (!dugum || *pos >= max - 50) return;

    *pos += snprintf(buf + *pos, max - *pos, "{\"ad\":\"");
    json_string_ekle(buf, pos, max, dugum->ad);
    *pos += snprintf(buf + *pos, max - *pos, "\",\"cocuklar\":[");

    for (int i = 0; i < dugum->cocuk_sayisi; i++) {
        if (i > 0) buf[(*pos)++] = ',';
        dugum_json(dugum->cocuklar[i], buf, pos, max);
    }
    *pos += snprintf(buf + *pos, max - *pos, "]}");
}

/*
 * Tüm ağacı JSON string olarak döndürür.
 * Çağıran free() ile serbest bırakmalıdır.
 */
char* agac_json_olustur(AgacYapisi* agac) {
    if (!agac || !agac->kok) return NULL;

    char* buf = (char*)malloc(AGAC_JSON_BOYUT);
    if (!buf) return NULL;

    int pos = 0;
    dugum_json(agac->kok, buf, &pos, AGAC_JSON_BOYUT);
    buf[pos] = '\0';
    return buf;
}
