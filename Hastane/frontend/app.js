/* ============================================================
 * app.js — Hastane Yönetim Sistemi Frontend
 * Fetch API ile C backend iletişimi ve DOM yönetimi.
 * ============================================================ */

const API = '';

/* ─────────────────────────────────────────────────────
   TOAST BİLDİRİMLERİ
───────────────────────────────────────────────────── */

function bildir(mesaj, tip = 'basari') {
    const ikonlar = { basari: '✅', hata: '❌', bilgi: 'ℹ️' };
    const kap = document.getElementById('bildirim-kap');
    const toast = document.createElement('div');
    toast.className = `toast toast-${tip}`;
    toast.innerHTML = `<span class="toast-ikon">${ikonlar[tip] || 'ℹ️'}</span>
                       <span class="toast-metin">${htmlKac(mesaj)}</span>`;
    kap.appendChild(toast);
    setTimeout(() => {
        toast.classList.add('cikis');
        setTimeout(() => toast.remove(), 300);
    }, 3200);
}

/* ─────────────────────────────────────────────────────
   API YARDIMCI
───────────────────────────────────────────────────── */

async function apiFetch(url, secenekler = {}) {
    try {
        const r = await fetch(API + url, {
            headers: { 'Content-Type': 'application/json' },
            ...secenekler
        });
        const veri = await r.json().catch(() => ({}));
        return { tamam: r.ok, durum: r.status, veri };
    } catch {
        bildir('Sunucuya bağlanılamıyor — sunucunun çalıştığından emin olun.', 'hata');
        return { tamam: false, veri: {} };
    }
}

/* ─────────────────────────────────────────────────────
   NAVİGASYON
───────────────────────────────────────────────────── */

const BOLUMLER = ['dashboard', 'hastalar', 'randevular', 'triaj', 'poliklinikler', 'gecmis'];
const SAYFA_ADLARI = {
    dashboard: 'Dashboard',
    hastalar: 'Hasta Yönetimi',
    randevular: 'Randevu Kuyruğu',
    triaj: 'Acil Triaj',
    poliklinikler: 'Poliklinik Hiyerarşisi',
    gecmis: 'İşlem Geçmişi'
};

let dashInterval = null;

function bolumGoster(ad) {
    BOLUMLER.forEach(b => {
        document.getElementById(`bolum-${b}`).classList.toggle('gizli', b !== ad);
    });
    document.querySelectorAll('.nav-link').forEach(l =>
        l.classList.toggle('aktif', l.dataset.bolum === ad));

    const sayfaAdi = document.getElementById('sayfa-adi');
    if (sayfaAdi) sayfaAdi.textContent = SAYFA_ADLARI[ad] || ad;

    if (ad === 'dashboard') {
        dashboardYukle();
        if (!dashInterval) dashInterval = setInterval(dashboardYukle, 5000);
    } else {
        clearInterval(dashInterval); dashInterval = null;
    }

    if (ad === 'hastalar')      { hastalariYukle(); poliklinikleriDoldur('h-poliklinik'); poliklinikleriDoldur('g-poliklinik'); }
    if (ad === 'randevular')    { randevuKuyruguYukle(); poliklinikleriDoldur('r-poliklinik'); }
    if (ad === 'triaj')         triajListesiYukle();
    if (ad === 'poliklinikler') agaciYukle();
    if (ad === 'gecmis')        gecmisYukle();
}

/* ─────────────────────────────────────────────────────
   DASHBOARD
───────────────────────────────────────────────────── */

async function dashboardYukle() {
    const { tamam, veri } = await apiFetch('/api/dashboard');
    if (!tamam) return;

    /* Grafikleri paralel yükle (dashboard açıkken) */
    istatistikleriYukle();

    animasyonluSayi('toplam-hasta', veri.toplam_hasta ?? 0);
    animasyonluSayi('kuyruk-sayi',  veri.kuyruk_sayi  ?? 0);
    animasyonluSayi('triaj-sayi',   veri.triaj_sayi   ?? 0);
    animasyonluSayi('bugun-islem',  veri.bugun_islem  ?? 0);

    /* Triaj rozetini güncelle */
    const triajDeg = document.getElementById('triaj-degisim');
    if (triajDeg && veri.triaj_sayi > 0) {
        triajDeg.className = 'kart-degisim degisim-azaldi';
        triajDeg.textContent = `${veri.triaj_sayi} Bekliyor`;
    }

    /* Normal kuyruk tablosu */
    const qTablo = document.getElementById('dash-kuyruk');
    qTablo.innerHTML = (!veri.kuyruk || veri.kuyruk.length === 0)
        ? '<tr><td colspan="4" class="bos-mesaj">📭 Kuyruk boş</td></tr>'
        : veri.kuyruk.map(h =>
            `<tr>
              <td><span class="sira-no">${h.sira}</span></td>
              <td><strong>${htmlKac(h.ad)}</strong> ${htmlKac(h.soyad)}</td>
              <td><span class="rozet rozet-teal">${htmlKac(h.poliklinik)}</span></td>
              <td style="color:#475569;font-weight:500;">${htmlKac(h.saat)}</td>
             </tr>`
        ).join('');

    /* Triaj tablosu */
    const tTablo = document.getElementById('dash-triaj');
    if (!veri.triaj || veri.triaj.length === 0) {
        tTablo.innerHTML = '<tr><td colspan="3" class="bos-mesaj">📭 Triaj boş</td></tr>';
    } else {
        const sirali = [...veri.triaj].sort((a, b) => a.skor - b.skor);
        tTablo.innerHTML = sirali.map(h =>
            `<tr class="${triajSatirSinifi(h.skor)}">
              <td>${triajSkorKutu(h.skor)}</td>
              <td><strong>${htmlKac(h.ad)}</strong> ${htmlKac(h.soyad)}</td>
              <td>${triajRozet(h.skor)}</td>
             </tr>`
        ).join('');
    }
}

/* ─────────────────────────────────────────────────────
   VERİ YAPISI ANALİTİĞİ — GRAFİKLER
───────────────────────────────────────────────────── */

async function istatistikleriYukle() {
    const { tamam, veri } = await apiFetch('/api/istatistikler');
    if (!tamam) return;

    renderHashTableGauge(veri.hash_table);
    renderPoliklinikChart(veri.poliklinik_dagilimi);
    renderTriajSkorChart(veri.triaj_skorlar);
    renderDolulukCubuklar(veri.heap_boyut, veri.heap_kapasite, veri.stack_boyut);
}

/* ── Hash Table Gauge ──
 * SVG daire grafiği: yük faktörünü görsel olarak gösterir.
 * Yük faktörü = toplam_hasta / HASH_TABLE_BOYUTU
 * 0.1–0.7 arası ideal aralık (az çakışma, az bellek israfı).
 */
function renderHashTableGauge(ht) {
    if (!ht) return;
    const ark   = document.getElementById('gauge-ht-ark');
    const pctEl = document.getElementById('gauge-ht-pct');
    if (!ark || !pctEl) return;

    /* Çember çevresi: 2π×44 = 276.46 */
    const CEVRE = 2 * Math.PI * 44;
    const oran  = Math.min(ht.yuk_faktoru, 1);
    const dolu  = oran * CEVRE;

    /* Rengi yük faktörüne göre ayarla */
    let renk = 'url(#tealGrad)';
    if (oran > 0.75) renk = '#F59E0B';
    if (oran > 0.90) renk = '#EF4444';
    ark.setAttribute('stroke', renk);
    ark.setAttribute('stroke-dasharray', `${dolu.toFixed(1)} ${(CEVRE - dolu).toFixed(1)}`);
    pctEl.textContent = (oran * 100).toFixed(1) + '%';

    /* Bilgi satırları */
    const s = (id, val) => { const e = document.getElementById(id); if (e) e.textContent = val; };
    s('ht-bucket-dolu',  ht.dolu_bucket?.toLocaleString('tr'));
    s('ht-bucket-top',   ht.toplam_bucket?.toLocaleString('tr'));
    s('ht-max-zincir',   ht.max_zincir);
    s('ht-toplam',       ht.toplam_hasta?.toLocaleString('tr'));

    /* Histogram: zincir uzunluk dağılımı */
    renderHistogram(ht.zincir_hist);
}

/* ── Bucket Histogram ──
 * Hash table'daki zincir uzunluklarının dağılımını gösterir.
 * Çoğu bucket 0 veya 1 uzunlukta olmalı → iyi hash dağılımı.
 * Uzun zincirler hash fonksiyonunun zayıf dağıldığını gösterir.
 */
function renderHistogram(hist) {
    const kap = document.getElementById('ht-histogram');
    if (!kap || !hist) return;

    const maksimum = Math.max(...hist, 1);
    const etiketler = ['0','1','2','3','4','5','6','7','8','9+'];

    /* Renk: 0=gri, 1=teal, 2+=turuncu/kırmızı */
    const renkler = [
        '#E2E8F0', '#14B8A6', '#2DD4BF', '#F59E0B',
        '#F97316', '#EF4444', '#DC2626', '#B91C1C',
        '#991B1B', '#7F1D1D'
    ];

    kap.innerHTML = hist.map((sayi, i) => {
        const yuzde = (sayi / maksimum) * 100;
        const renk  = renkler[Math.min(i, renkler.length - 1)];
        const tip   = `Uzunluk ${etiketler[i]}: ${sayi.toLocaleString('tr')} bucket`;
        return `<div class="hist-kolon">
            <div class="hist-bar" style="height:${Math.max(yuzde, 2)}%;background:${renk};" data-tip="${htmlKac(tip)}"></div>
            <div class="hist-etiket">${etiketler[i]}</div>
        </div>`;
    }).join('');
}

/* ── Poliklinik Yatay Bar Chart ──
 * Hash table'daki tüm hastalar poliklinik alanına göre gruplandırılır.
 * Bu O(n) bir tarama işlemidir ve her poliklinikteki hasta sayısını gösterir.
 */
function renderPoliklinikChart(dagilim) {
    const kap = document.getElementById('pol-dagilim-chart');
    if (!kap || !dagilim || dagilim.length === 0) {
        if (kap) kap.innerHTML = '<div class="bos-mesaj">Veri yok</div>';
        return;
    }

    const maksimum = Math.max(...dagilim.map(d => d.sayi), 1);

    /* Her poliklinik için renk tonu (teal paleti) */
    const toner = (i, top) => {
        const t = i / Math.max(top - 1, 1);
        const r = Math.round(13  + t * (45 - 13));
        const g = Math.round(148 + t * (212 - 148));
        const b = Math.round(136 + t * (166 - 136));
        return `rgb(${r},${g},${b})`;
    };

    kap.innerHTML = dagilim.map((d, i) => {
        const pct  = (d.sayi / maksimum) * 100;
        const renk = toner(i, dagilim.length);
        return `<div class="bar-satir">
            <div class="bar-etiket" title="${htmlKac(d.ad)}">${htmlKac(d.ad)}</div>
            <div class="bar-arka">
              <div class="bar-dolu" style="width:${pct}%;background:linear-gradient(90deg,${renk}99,${renk});"></div>
            </div>
            <div class="bar-sayi">${d.sayi}</div>
        </div>`;
    }).join('');
}

/* ── Triaj Skor Dikey Bar Chart ──
 * Min-Heap'teki her aciliyet skorundan (1–10) kaç hasta olduğunu gösterir.
 * Kırmızı(1-3) = kritik, Sarı(4-6) = orta, Yeşil(7-10) = stabil.
 * Heap sıralı değil → tüm elemanları tarayarak sayım yapılır O(n).
 */
function renderTriajSkorChart(skorlar) {
    const kap = document.getElementById('triaj-skor-chart');
    if (!kap || !skorlar) return;

    const maksimum = Math.max(...skorlar.map(s => s.sayi), 1);
    const MIN_H    = 4;   /* Minimum görünür bar yüksekliği px */
    const MAX_H    = 90;  /* Maksimum bar yüksekliği px */

    kap.innerHTML = skorlar.map(s => {
        const yuzde = (s.sayi / maksimum);
        const yuk   = MIN_H + yuzde * (MAX_H - MIN_H);
        let sinif   = s.skor <= 3 ? 'kritik' : s.skor <= 6 ? 'orta' : 'stabil';

        return `<div class="dikey-kolon">
            <div class="dikey-sayi">${s.sayi > 0 ? s.sayi : ''}</div>
            <div class="dikey-bar ${sinif}" style="height:${yuk}px;"
                 title="Skor ${s.skor}: ${s.sayi} hasta"></div>
            <div class="dikey-skor">${s.skor}</div>
        </div>`;
    }).join('');
}

/* ── Doluluk Çubukları ──
 * Min-Heap ve Stack'in mevcut kapasitesini yüzde olarak gösterir.
 */
function renderDolulukCubuklar(heapBoyut, heapKapasite, stackBoyut) {
    const heapPct = heapKapasite > 0 ? (heapBoyut / heapKapasite) * 100 : 0;
    const stackPct = Math.min((stackBoyut / 500) * 100, 100);

    const hp = document.getElementById('heap-progress');
    const sp = document.getElementById('stack-progress');
    const hpt = document.getElementById('heap-pct');
    const spt = document.getElementById('stack-pct');

    if (hp)  hp.style.width  = heapPct.toFixed(1) + '%';
    if (sp)  sp.style.width  = stackPct.toFixed(1) + '%';
    if (hpt) hpt.textContent = `${heapBoyut}/${heapKapasite}`;
    if (spt) spt.textContent = `${stackBoyut}/500`;
}

/* ─────────────────────────────────────────────────────
   HASTA YÖNETİMİ
───────────────────────────────────────────────────── */

async function hastaEkle(olay) {
    olay.preventDefault();
    const veri = {
        tc:         belgeAl('h-tc'),
        ad:         belgeAl('h-ad'),
        soyad:      belgeAl('h-soyad'),
        yas:        parseInt(belgeAl('h-yas')) || 0,
        poliklinik: belgeAl('h-poliklinik'),
        telefon:    belgeAl('h-telefon')
    };
    const { tamam, veri: y } = await apiFetch('/api/hasta', {
        method: 'POST', body: JSON.stringify(veri)
    });
    if (tamam) {
        bildir('Hasta başarıyla kaydedildi.');
        document.getElementById('hasta-form').reset();
        hastalariYukle();
    } else bildir(y.mesaj || 'Kayıt başarısız.', 'hata');
}

let buAnkiHasta = null;

async function hastaAra() {
    const tc = belgeAl('ara-tc').trim();
    if (!tc) { bildir('TC giriniz.', 'hata'); return; }
    const { tamam, veri } = await apiFetch(`/api/hasta?tc=${tc}`);
    const sonucDiv = document.getElementById('arama-sonuc');
    if (tamam) {
        buAnkiHasta = veri;
        const ilkHarf = (veri.ad || '?').charAt(0).toUpperCase();
        document.getElementById('hasta-avatar-harf').textContent = ilkHarf;
        document.getElementById('hasta-tam-ad').textContent = `${veri.ad} ${veri.soyad}`;
        document.getElementById('hasta-tc-goster').textContent = veri.tc;
        document.getElementById('hasta-bilgi-sat').innerHTML = `
            <div class="bilgi-parcasi"><div class="bilgi-etiket">Yaş</div><div class="bilgi-deger">${veri.yas}</div></div>
            <div class="bilgi-parcasi"><div class="bilgi-etiket">Poliklinik</div><div class="bilgi-deger">${htmlKac(veri.poliklinik)}</div></div>
            <div class="bilgi-parcasi"><div class="bilgi-etiket">Telefon</div><div class="bilgi-deger">${htmlKac(veri.telefon)}</div></div>`;
        sonucDiv.classList.remove('gizli');
    } else {
        buAnkiHasta = null;
        sonucDiv.classList.add('gizli');
        bildir('Hasta bulunamadı.', 'hata');
    }
}

function hastaGuncelleForm() {
    if (!buAnkiHasta) return;
    const panel = document.getElementById('guncelle-panel');
    panel.classList.remove('gizli');
    belgeAta('g-tc',         buAnkiHasta.tc);
    belgeAta('g-ad',         buAnkiHasta.ad);
    belgeAta('g-soyad',      buAnkiHasta.soyad);
    belgeAta('g-yas',        buAnkiHasta.yas);
    belgeAta('g-telefon',    buAnkiHasta.telefon);
    belgeAta('g-poliklinik', buAnkiHasta.poliklinik);
    panel.scrollIntoView({ behavior: 'smooth' });
}

async function hastaGuncelle(olay) {
    olay.preventDefault();
    const tc = belgeAl('g-tc');
    const veri = {
        ad: belgeAl('g-ad'), soyad: belgeAl('g-soyad'),
        yas: parseInt(belgeAl('g-yas')) || 0,
        poliklinik: belgeAl('g-poliklinik'), telefon: belgeAl('g-telefon')
    };
    const { tamam, veri: y } = await apiFetch(`/api/hasta/${tc}`, {
        method: 'PUT', body: JSON.stringify(veri)
    });
    if (tamam) { bildir('Hasta güncellendi.'); guncelleIptal(); hastalariYukle(); }
    else bildir(y.mesaj || 'Güncelleme başarısız.', 'hata');
}

function guncelleIptal() {
    document.getElementById('guncelle-panel').classList.add('gizli');
}

async function hastaSilOnay() {
    if (!buAnkiHasta) return;
    if (!confirm(`${buAnkiHasta.ad} ${buAnkiHasta.soyad} (TC: ${buAnkiHasta.tc}) silinsin mi?`)) return;
    await hastaSil(buAnkiHasta.tc);
}

async function hastaSil(tc) {
    const { tamam, veri } = await apiFetch(`/api/hasta/${tc}`, { method: 'DELETE' });
    if (tamam) {
        bildir('Hasta silindi.');
        document.getElementById('arama-sonuc').classList.add('gizli');
        buAnkiHasta = null;
        hastalariYukle();
    } else bildir(veri.mesaj || 'Silme başarısız.', 'hata');
}

async function hastalariYukle() {
    const { tamam, veri } = await apiFetch('/api/hastalar');
    const tablo = document.getElementById('hasta-listesi');
    if (!tamam || !Array.isArray(veri) || veri.length === 0) {
        tablo.innerHTML = `<tr><td colspan="7" class="bos-mesaj">📭 ${tamam ? 'Kayıtlı hasta yok' : 'Yüklenemedi'}</td></tr>`;
        return;
    }
    tablo.innerHTML = veri.map(h =>
        `<tr>
          <td><span class="tc-kodu">${htmlKac(h.tc)}</span></td>
          <td><strong>${htmlKac(h.ad)}</strong></td>
          <td>${htmlKac(h.soyad)}</td>
          <td><span class="rozet rozet-gri">${h.yas}</span></td>
          <td><span class="rozet rozet-teal">${htmlKac(h.poliklinik)}</span></td>
          <td style="color:#64748b;font-size:13px;">${htmlKac(h.telefon)}</td>
          <td>
            <button class="buton buton-ikon buton-gri" title="Güncelle" onclick="hastaListedenGuncelle('${htmlKac(h.tc)}')">✏️</button>
            <button class="buton buton-ikon buton-kirmizi" style="margin-left:4px" title="Sil"
                    onclick="hastaListedenSil('${htmlKac(h.tc)}','${htmlKac(h.ad)} ${htmlKac(h.soyad)}')">🗑️</button>
          </td>
         </tr>`
    ).join('');
}

async function hastaListedenGuncelle(tc) {
    const { tamam, veri } = await apiFetch(`/api/hasta?tc=${tc}`);
    if (!tamam) { bildir('Hasta bulunamadı.', 'hata'); return; }
    buAnkiHasta = veri;
    hastaGuncelleForm();
}

async function hastaListedenSil(tc, adSoyad) {
    if (!confirm(`${adSoyad} (TC: ${tc}) silinsin mi?`)) return;
    await hastaSil(tc);
}

/* ─────────────────────────────────────────────────────
   RANDEVU KUYRUĞU
───────────────────────────────────────────────────── */

async function randevuEkle(olay) {
    olay.preventDefault();
    const veri = {
        tc: belgeAl('r-tc'), ad: belgeAl('r-ad'), soyad: belgeAl('r-soyad'),
        poliklinik: belgeAl('r-poliklinik'), saat: belgeAl('r-saat')
    };
    const { tamam, veri: y } = await apiFetch('/api/randevu', {
        method: 'POST', body: JSON.stringify(veri)
    });
    if (tamam) {
        bildir('Randevu kuyruğa eklendi.');
        document.getElementById('randevu-form').reset();
        randevuKuyruguYukle();
    } else bildir(y.mesaj || 'Randevu alınamadı.', 'hata');
}

async function siradakiCagir() {
    const { tamam, veri } = await apiFetch('/api/randevu/cagir', { method: 'POST' });
    if (tamam && veri.hasta) {
        bildir(`${veri.hasta.ad} ${veri.hasta.soyad} çağrıldı — ${veri.hasta.poliklinik}`, 'bilgi');
        randevuKuyruguYukle();
    } else bildir(veri.mesaj || 'Kuyrukta hasta yok.', 'hata');
}

async function randevuIptal() {
    const tc = belgeAl('iptal-tc').trim();
    if (!tc) { bildir('TC giriniz.', 'hata'); return; }
    const { tamam, veri } = await apiFetch(`/api/randevu/${tc}`, { method: 'DELETE' });
    if (tamam) {
        bildir('Randevu iptal edildi.');
        belgeAta('iptal-tc', '');
        randevuKuyruguYukle();
    } else bildir(veri.mesaj || 'Randevu bulunamadı.', 'hata');
}

async function randevuKuyruguYukle() {
    const { tamam, veri } = await apiFetch('/api/randevu/kuyruk');
    const tablo = document.getElementById('randevu-listesi');
    if (!tamam || !Array.isArray(veri) || veri.length === 0) {
        tablo.innerHTML = '<tr><td colspan="5" class="bos-mesaj">📭 Kuyruk boş</td></tr>';
        return;
    }
    tablo.innerHTML = veri.map(h =>
        `<tr>
          <td><span class="sira-no">${h.sira}</span></td>
          <td><span class="tc-kodu">${htmlKac(h.tc)}</span></td>
          <td><strong>${htmlKac(h.ad)}</strong> ${htmlKac(h.soyad)}</td>
          <td><span class="rozet rozet-teal">${htmlKac(h.poliklinik)}</span></td>
          <td style="font-weight:600;color:#0D9488;">${htmlKac(h.saat)}</td>
         </tr>`
    ).join('');
}

/* ─────────────────────────────────────────────────────
   ACİL TRİAJ
───────────────────────────────────────────────────── */

function skorGuncelle(deger) {
    const n = parseInt(deger);
    const el = document.getElementById('skor-suat-el');
    const et = document.getElementById('skor-etiket-el');
    el.textContent = n;
    if (n <= 3) {
        el.className = 'skor-suat skor-kritik';
        et.textContent = 'Kritik Aciliyet';
        et.className = 'skor-etiket text-kirmizi';
    } else if (n <= 6) {
        el.className = 'skor-suat skor-orta';
        et.textContent = 'Orta Aciliyet';
        et.className = 'skor-etiket text-amber';
    } else {
        el.className = 'skor-suat skor-stabil';
        et.textContent = 'Düşük Aciliyet';
        et.className = 'skor-etiket text-yesil';
    }
}

async function triajEkle(olay) {
    olay.preventDefault();
    const veri = {
        tc: belgeAl('t-tc'), ad: belgeAl('t-ad'), soyad: belgeAl('t-soyad'),
        skor: parseInt(belgeAl('t-skor'))
    };
    const { tamam, veri: y } = await apiFetch('/api/triaj', {
        method: 'POST', body: JSON.stringify(veri)
    });
    if (tamam) {
        bildir('Triaja eklendi.');
        document.getElementById('triaj-form').reset();
        document.getElementById('t-skor').value = '5';
        skorGuncelle(5);
        triajListesiYukle();
    } else bildir(y.mesaj || 'Triaja eklenemedi.', 'hata');
}

async function enAcilCagir() {
    const { tamam, veri } = await apiFetch('/api/triaj/cagir', { method: 'POST' });
    if (tamam && veri.hasta) {
        bildir(`En acil hasta çağrıldı: ${veri.hasta.ad} ${veri.hasta.soyad} (Skor: ${veri.hasta.skor})`, 'bilgi');
        triajListesiYukle();
    } else bildir(veri.mesaj || 'Triajda hasta yok.', 'hata');
}

async function triajListesiYukle() {
    const { tamam, veri } = await apiFetch('/api/triaj/kuyruk');
    const tablo = document.getElementById('triaj-listesi');
    if (!tamam || !Array.isArray(veri) || veri.length === 0) {
        tablo.innerHTML = '<tr><td colspan="4" class="bos-mesaj">📭 Triaj boş</td></tr>';
        return;
    }
    const sirali = [...veri].sort((a, b) => a.skor - b.skor);
    tablo.innerHTML = sirali.map(h =>
        `<tr class="${triajSatirSinifi(h.skor)}">
          <td>${triajSkorKutu(h.skor)}</td>
          <td><span class="tc-kodu">${htmlKac(h.tc)}</span></td>
          <td><strong>${htmlKac(h.ad)}</strong> ${htmlKac(h.soyad)}</td>
          <td>${triajRozet(h.skor)}</td>
         </tr>`
    ).join('');
}

/* ─────────────────────────────────────────────────────
   POLİKLİNİKLER
───────────────────────────────────────────────────── */

async function agaciYukle() {
    const { tamam, veri } = await apiFetch('/api/poliklinikler');
    const kap = document.getElementById('agac-gorunum');
    if (!tamam) { kap.innerHTML = '<div class="bos-mesaj">❌ Yüklenemedi</div>'; return; }
    kap.innerHTML = '';
    kap.appendChild(dugumOlustur(veri, true));

    /* SVG görselleştirmesini de güncelle */
    renderAgacSVG(veri);
}

/* ─────────────────────────────────────────────────────
   N-ARY TREE SVG GÖRSELLEŞTİRMESİ
   ──────────────────────────────────────────────────
   Algoritma:
   1. yaprakSay(n)   → her düğümün altındaki yaprak sayısı (bottom-up)
   2. konumAta(n)    → x = seviye * adım, y = yaprak ağırlıklı merkez (top-down)
   3. SVG oluştur    → bezier kenarlar (önce), düğüm kutular (sonra)

   Bezier kontrol noktaları: cx = (x1+x2)/2 ile simetrik S-eğrisi
   Her düğüm/kenar animasyon gecikmesi seviye × indeks ile hesaplanır.
───────────────────────────────────────────────────── */

function renderAgacSVG(kok) {
    const svg = document.getElementById('agac-svg');
    if (!svg || !kok) return;

    /* ── Sabitler ── */
    const NW = 152;   /* düğüm kutu genişliği */
    const NH = 36;    /* düğüm kutu yüksekliği */
    const HG = 72;    /* seviyeler arası yatay boşluk */
    const VU = 56;    /* yaprak başına dikey birim (px) */
    const MX = 28;    /* sol kenar boşluğu */
    const MY = 48;    /* üst boşluk (seviye etiketleri için) */
    const NR = 10;    /* kutu köşe yarıçapı */

    /* Seviyeye göre görsel tema */
    const TEMALAR = [
        { dolu: '#0F172A', metin: '#ffffff', kenar: 'none',    golge: '0 4px 12px rgba(15,23,42,.40)' },
        { dolu: '#0D9488', metin: '#ffffff', kenar: 'none',    golge: '0 4px 12px rgba(13,148,136,.35)' },
        { dolu: '#F0FDFA', metin: '#0D6B63', kenar: '#14B8A6', golge: '0 2px 8px rgba(13,148,136,.15)' },
    ];

    /* ── ADIM 1: Yaprak sayısı hesapla (bottom-up DFS) ── */
    function yaprakSay(dugum) {
        if (!dugum.cocuklar || dugum.cocuklar.length === 0) {
            return (dugum._yaprak = 1);
        }
        return (dugum._yaprak = dugum.cocuklar.reduce((s, c) => s + yaprakSay(c), 0));
    }
    yaprakSay(kok);

    /* ── ADIM 2: Koordinat ata (top-down) ──
     * x: seviye × (NW + HG) + sol boşluk
     * y: alt ağacın ilk ve son çocuğunun merkez noktaları ortası
     */
    function konumAta(dugum, seviye, yBaslangic) {
        dugum._seviye = seviye;
        dugum._x = MX + seviye * (NW + HG);

        if (!dugum.cocuklar || dugum.cocuklar.length === 0) {
            dugum._cy = yBaslangic + VU / 2;
            dugum._y  = dugum._cy - NH / 2;
            return yBaslangic + VU;
        }

        let suanY = yBaslangic;
        dugum.cocuklar.forEach(c => { suanY = konumAta(c, seviye + 1, suanY); });

        /* Ebeveyn y'si: ilk ve son çocuğun cy ortası */
        dugum._cy = (dugum.cocuklar[0]._cy + dugum.cocuklar[dugum.cocuklar.length - 1]._cy) / 2;
        dugum._y  = dugum._cy - NH / 2;
        return suanY;
    }
    konumAta(kok, 0, MY);

    /* ── Boyut hesapla ── */
    let maxSeviye = 0, toplamDugum = 0, yaprakSayisi = 0, toplamKenar = 0;
    const tumDugumler = [];

    (function topla(d) {
        tumDugumler.push(d);
        toplamDugum++;
        if (d._seviye > maxSeviye) maxSeviye = d._seviye;
        if (!d.cocuklar || !d.cocuklar.length) yaprakSayisi++;
        if (d.cocuklar) {
            toplamKenar += d.cocuklar.length;
            d.cocuklar.forEach(topla);
        }
    })(kok);

    const svgW = MX + (maxSeviye + 1) * (NW + HG) - HG + MX + 10;
    const svgH = kok._yaprak * VU + MY + 20;

    svg.setAttribute('viewBox', `0 0 ${svgW} ${svgH}`);
    svg.setAttribute('width',  svgW);
    svg.setAttribute('height', svgH);
    svg.style.minWidth = svgW + 'px';

    /* ── Parçaları topla ── */
    const kenarlar = [];
    const dugumler = [];

    (function topla2(dugum, derinlik) {
        dugumler.push({ dugum, derinlik });
        if (dugum.cocuklar) {
            dugum.cocuklar.forEach((c, ci) => {
                kenarlar.push({ kaynak: dugum, hedef: c, idx: kenarlar.length });
                topla2(c, derinlik + 1);
            });
        }
    })(kok, 0);

    /* ── ADIM 3: SVG oluştur ── */

    /* Seviye arka plan çizgileri */
    const seviyeCizgileri = Array.from({ length: maxSeviye + 1 }, (_, i) => {
        const x = MX + i * (NW + HG) + NW / 2;
        return `<line x1="${x}" y1="${MY - 22}" x2="${x}" y2="${svgH - 10}"
                      class="agac-seviye-cizgi"/>`;
    }).join('');

    /* Seviye etiketleri */
    const seviyeEtiketleri = ['Kök', 'Seviye 1', 'Seviye 2', 'Seviye 3'].slice(0, maxSeviye + 1);
    const seviyeMetinler = Array.from({ length: maxSeviye + 1 }, (_, i) => {
        const x   = MX + i * (NW + HG) + NW / 2;
        const etk = seviyeEtiketleri[i] || `S${i}`;
        return `<text x="${x}" y="${MY - 8}" text-anchor="middle"
                      font-size="10" font-weight="700" fill="#94A3B8"
                      text-transform="uppercase" letter-spacing="1"
                      font-family="Inter,sans-serif">${etk.toUpperCase()}</text>`;
    }).join('');

    /* Kenar SVG'leri — bezier S-eğrisi */
    const kenarSVG = kenarlar.map(k => {
        const x1  = k.kaynak._x + NW;
        const y1  = k.kaynak._cy;
        const x2  = k.hedef._x;
        const y2  = k.hedef._cy;
        const cxM = (x1 + x2) / 2;         /* yatay orta nokta */
        const renk = k.kaynak._seviye === 0 ? '#99F6E4' : '#CBD5E1';
        const gecikme = k.idx * 60;

        return `<path class="agac-edge"
                      d="M ${x1} ${y1} C ${cxM} ${y1}, ${cxM} ${y2}, ${x2} ${y2}"
                      stroke="${renk}"
                      style="animation-delay:${gecikme}ms;"/>`;
    }).join('\n');

    /* ── Düğüm SVG'leri ──
     * HATA DÜZELTMESİ: CSS "transform:scale()" ile SVG "transform=translate(x,y)"
     * aynı <g> elementinde çakışınca tarayıcı CSS'i önceliklendiriyor ve
     * SVG translate sıfırlanıyor → tüm düğümler (0,0)'a yığılıyordu.
     *
     * Çözüm — İki katmanlı <g>:
     *   Dış <g transform="translate(x,y)">   SVG attribute → konum (CSS dokunmaz)
     *   İç <g class="agac-node-g">           CSS animation → sadece animasyon
     */
    const dugumSVG = dugumler.map(({ dugum: d }, i) => {
        const tema    = TEMALAR[Math.min(d._seviye, TEMALAR.length - 1)];
        const yaprak  = !d.cocuklar || !d.cocuklar.length;
        const gecikme = (d._seviye * 3 + i) * 50;
        const stroke  = tema.kenar !== 'none' ? `stroke="${tema.kenar}" stroke-width="1.5"` : '';
        const fontW   = d._seviye === 0 ? '700' : '600';
        const fontSize= d._seviye === 0 ? '13'  : '12';
        const noktaSVG = yaprak
            ? `<circle cx="${NW - 13}" cy="${NH / 2}" r="3.5"
                        fill="${tema.kenar !== 'none' ? tema.kenar : '#2DD4BF'}" opacity="0.8"/>`
            : '';

        return `<g transform="translate(${d._x.toFixed(1)},${d._y.toFixed(1)})">
            <g class="agac-node-g" style="animation-delay:${gecikme}ms">
                <rect class="agac-node-rect" width="${NW}" height="${NH}" rx="${NR}"
                      fill="${tema.dolu}" ${stroke} filter="url(#nd-golge)"/>
                ${noktaSVG}
                <text x="${NW / 2}" y="${NH / 2 + 4.5}" text-anchor="middle"
                      fill="${tema.metin}" font-size="${fontSize}" font-weight="${fontW}"
                      font-family="Inter,sans-serif">${htmlKac(d.ad)}</text>
            </g>
        </g>`;
    }).join('\n');

    /* SVG çıktısı — <defs> en üstte, filtreler burada tanımlı */
    svg.innerHTML = `
        <defs>
            <filter id="nd-golge" x="-15%" y="-20%" width="130%" height="140%">
                <feDropShadow dx="0" dy="2" stdDeviation="3.5" flood-color="#00000025"/>
            </filter>
        </defs>
        <g class="kilavuzlar">${seviyeCizgileri}</g>
        <g class="etiketler">${seviyeMetinler}</g>
        <g class="kenarlar">${kenarSVG}</g>
        <g class="dugumler">${dugumSVG}</g>`;

    /* İstatistik şeridini güncelle */
    const s = (id, v) => { const e = document.getElementById(id); if (e) e.textContent = v; };
    s('agac-dugum-sayi', toplamDugum);
    s('agac-kenar-sayi', toplamKenar);
    s('agac-derinlik',   maxSeviye);
    s('agac-yaprak',     yaprakSayisi);
}

function dugumOlustur(dugum, kok = false) {
    const div = document.createElement('div');
    div.className = 'agac-dugum' + (kok ? ' agac-kok' : '');

    const icerik = document.createElement('div');
    icerik.className = 'agac-icerik';

    const ok = document.createElement('span');
    ok.className = 'agac-ok';
    ok.innerHTML = dugum.cocuklar && dugum.cocuklar.length ? '▶' : '&nbsp;';

    const ikonEl = document.createElement('span');
    ikonEl.className = 'agac-ikon';
    ikonEl.textContent = kok ? '🏥' : (dugum.cocuklar && dugum.cocuklar.length ? '📁' : '🏷️');

    const adEl = document.createElement('span');
    adEl.className = 'agac-ad';
    adEl.textContent = dugum.ad;

    icerik.appendChild(ok);
    icerik.appendChild(ikonEl);
    icerik.appendChild(adEl);
    div.appendChild(icerik);

    if (dugum.cocuklar && dugum.cocuklar.length) {
        const cocuklarDiv = document.createElement('div');
        cocuklarDiv.className = 'agac-cocuklar';
        dugum.cocuklar.forEach(c => cocuklarDiv.appendChild(dugumOlustur(c)));
        div.appendChild(cocuklarDiv);
        ok.classList.add('acik');

        icerik.onclick = () => {
            const acik = cocuklarDiv.style.display !== 'none';
            cocuklarDiv.style.display = acik ? 'none' : 'block';
            ok.classList.toggle('acik', !acik);
        };
    }
    return div;
}

async function poliklinikEkle(olay) {
    olay.preventDefault();
    const veri = { ebeveyn: belgeAl('p-ebeveyn'), ad: belgeAl('p-ad') };
    const { tamam, veri: y } = await apiFetch('/api/poliklinik', {
        method: 'POST', body: JSON.stringify(veri)
    });
    if (tamam) {
        bildir('Poliklinik eklendi.');
        document.getElementById('poliklinik-form').reset();
        agaciYukle();
        ['h-poliklinik','r-poliklinik','g-poliklinik'].forEach(poliklinikleriDoldur);
    } else bildir(y.mesaj || 'Eklenemedi.', 'hata');
}

async function poliklinikSil() {
    const ad = belgeAl('p-sil-ad').trim();
    if (!ad) { bildir('Birim adı giriniz.', 'hata'); return; }
    if (!confirm(`"${ad}" ve tüm alt birimleri silinecek. Onaylıyor musunuz?`)) return;
    const { tamam, veri } = await apiFetch(`/api/poliklinik/${encodeURIComponent(ad)}`, { method: 'DELETE' });
    if (tamam) {
        bildir('Poliklinik silindi.');
        belgeAta('p-sil-ad', '');
        agaciYukle();
        ['h-poliklinik','r-poliklinik','g-poliklinik'].forEach(poliklinikleriDoldur);
    } else bildir(veri.mesaj || 'Silinemedi.', 'hata');
}

async function poliklinikleriDoldur(selectId) {
    const { tamam, veri } = await apiFetch('/api/poliklinikler');
    if (!tamam) return;
    const secim = document.getElementById(selectId);
    if (!secim) return;
    const mevcut = secim.value;
    const isimler = poliklinikIsimleri(veri, []);
    secim.innerHTML = '<option value="">— Seçiniz —</option>' +
        isimler.map(ad => `<option value="${htmlKac(ad)}"${ad === mevcut ? ' selected' : ''}>${htmlKac(ad)}</option>`).join('');
}

function poliklinikIsimleri(dugum, liste) {
    if (!dugum) return liste;
    if (dugum.ad !== 'Hastane') liste.push(dugum.ad);
    if (dugum.cocuklar) dugum.cocuklar.forEach(c => poliklinikIsimleri(c, liste));
    return liste;
}

/* ─────────────────────────────────────────────────────
   İŞLEM GEÇMİŞİ
───────────────────────────────────────────────────── */

async function gecmisYukle() {
    const { tamam, veri } = await apiFetch('/api/gecmis');
    const tablo = document.getElementById('gecmis-listesi');
    if (!tamam || !Array.isArray(veri) || veri.length === 0) {
        tablo.innerHTML = '<tr><td colspan="4" class="bos-mesaj">📭 Geçmiş boş</td></tr>';
        return;
    }
    tablo.innerHTML = veri.map(i =>
        `<tr>
          <td style="white-space:nowrap;font-size:12px;color:#64748b;">${htmlKac(i.zaman)}</td>
          <td>${islemRozet(i.tip)}</td>
          <td><span class="tc-kodu">${htmlKac(i.tc)}</span></td>
          <td style="font-size:13px;">${htmlKac(i.aciklama)}</td>
         </tr>`
    ).join('');
}

async function geriAl() {
    const { tamam, veri } = await apiFetch('/api/gecmis/geri-al', { method: 'POST' });
    if (tamam) {
        bildir(veri.mesaj || 'İşlem geri alındı.', 'bilgi');
        gecmisYukle(); hastalariYukle(); randevuKuyruguYukle(); triajListesiYukle();
    } else bildir(veri.mesaj || 'Geri alınacak işlem yok.', 'hata');
}

/* ─────────────────────────────────────────────────────
   YARDIMCI FONKSİYONLAR
───────────────────────────────────────────────────── */

function belgeAl(id) { const el = document.getElementById(id); return el ? el.value : ''; }
function belgeAta(id, v) { const el = document.getElementById(id); if (el) el.value = v; }

function htmlKac(str) {
    if (str === null || str === undefined) return '';
    return String(str)
        .replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

function triajSatirSinifi(skor) {
    if (skor <= 3) return 'satir-kritik';
    if (skor <= 6) return 'satir-orta';
    return 'satir-stabil';
}

function triajSkorKutu(skor) {
    return `<span class="skor-kutu skor-${skor}">${skor}</span>`;
}

function triajRozet(skor) {
    if (skor <= 3) return '<span class="rozet rozet-kirmizi">KRİTİK</span>';
    if (skor <= 6) return '<span class="rozet rozet-amber">ORTA</span>';
    return '<span class="rozet rozet-yesil">STABİL</span>';
}

function islemRozet(tip) {
    const renk = {
        'HASTA_EKLE': 'rozet-yesil', 'HASTA_SIL': 'rozet-kirmizi',
        'HASTA_GUNCELLE': 'rozet-teal', 'RANDEVU_AL': 'rozet-teal',
        'RANDEVU_CAGIR': 'rozet-mor', 'RANDEVU_IPTAL': 'rozet-kirmizi',
        'TRIAJ_EKLE': 'rozet-amber', 'TRIAJ_CAGIR': 'rozet-mor',
        'POLIKLINIK_EKLE': 'rozet-yesil', 'POLIKLINIK_SIL': 'rozet-kirmizi'
    };
    const s = renk[tip] || 'rozet-gri';
    return `<span class="rozet ${s}">${htmlKac(tip.replace('_', ' '))}</span>`;
}

/* Sayı animasyonu */
function animasyonluSayi(id, hedef) {
    const el = document.getElementById(id);
    if (!el) return;
    const mevcut = parseInt(el.textContent) || 0;
    if (mevcut === hedef) return;
    const fark = hedef - mevcut;
    const adim = fark > 0 ? Math.ceil(fark / 12) : Math.floor(fark / 12);
    let sayac = mevcut;
    const id2 = setInterval(() => {
        sayac += adim;
        if ((fark > 0 && sayac >= hedef) || (fark < 0 && sayac <= hedef)) {
            el.textContent = hedef;
            clearInterval(id2);
        } else el.textContent = sayac;
    }, 35);
}

/* Tarih/saat göstergesi */
function tarihGoster() {
    const el = document.getElementById('tarih-goster');
    if (!el) return;
    const now = new Date();
    el.textContent = now.toLocaleDateString('tr-TR', { weekday: 'long', year: 'numeric', month: 'long', day: 'numeric' });
}

/* ─────────────────────────────────────────────────────
   BAŞLANGIÇ
───────────────────────────────────────────────────── */

document.addEventListener('DOMContentLoaded', () => {
    tarihGoster();
    setInterval(tarihGoster, 60000);
    bolumGoster('dashboard');
});
