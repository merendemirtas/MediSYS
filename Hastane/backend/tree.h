/*
 * tree.h
 * Poliklinik hiyerarşi modülü - hastane bölüm ağacı.
 * Veri Yapısı: N-ary Tree (her düğümün en fazla MAX_COCUK çocuğu olabilir)
 * Neden N-ary Tree: Hiyerarşik bölüm yapısını doğal olarak temsil eder.
 * Ebeveyn pointer: silme ve yukarı yönlü gezinme için O(1) erişim.
 */

#ifndef TREE_H
#define TREE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COCUK       20
#define MAX_AD_UZUNLUGU 100
#define AGAC_JSON_BOYUT 8192

/*
 * Ağaç düğümü.
 * cocuklar: sabit boyutlu pointer dizisi (N-ary tree için)
 * ebeveyn: silme sırasında O(1) ile üst düğüme erişim sağlar
 */
typedef struct TreeNode {
    char            ad[MAX_AD_UZUNLUGU];
    struct TreeNode* cocuklar[MAX_COCUK];
    int              cocuk_sayisi;
    struct TreeNode* ebeveyn;
} TreeNode;

typedef struct {
    TreeNode* kok;
} AgacYapisi;

AgacYapisi* agac_olustur(void);
void        agac_yok_et(AgacYapisi* agac);

/* Ağaçta isme göre düğüm bulur; DFS ile O(n) arama */
TreeNode* dugum_bul(TreeNode* kok, const char* ad);

int poliklinik_ekle(AgacYapisi* agac,
                    const char* ebeveyn_adi, const char* yeni_ad);
int poliklinik_sil(AgacYapisi* agac, const char* ad);

/* Tüm ağacı JSON string olarak döndürür; çağıran free() etmeli */
char* agac_json_olustur(AgacYapisi* agac);

#endif /* TREE_H */
