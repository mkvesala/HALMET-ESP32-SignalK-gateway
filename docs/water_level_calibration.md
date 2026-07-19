# Makeavesitankin kalibrointi — HALMET ESP32 SignalK Gateway

## Taustaa

Fridan makeavesitankki: **100 L**, **epäsäännöllisen muotoinen**.

Polttoainetankki on säännöllisen muotoinen, joten `VDOProcessor` muuntaa vastuksen täyttöasteeksi kahden pisteen lineaarisella sovituksella (3 Ω = tyhjä, 180 Ω = täysi). Makeavesitankissa tämä ei toimi: poikkileikkausala vaihtelee korkeuden mukaan, joten sama vastuksen muutos vastaa eri litramäärää tankin eri kohdissa.

Siksi `WaterProcessor` käyttää **mitattua kalibrointitaulukkoa**: tankki täytetään 10 %:n portaissa ja kussakin portaassa kirjataan anturin tuottama vastus. Ohjelma interpoloi lineaarisesti mittauspisteiden välillä.

Anturi: resistiivinen 0–190 Ω lähetin, HALMETin analogiatulo **A2** (ADS1115 kanava 1), **CCS-jumpperi A2:lle asennettuna**.

---

## Miksi taulukko tallentaa vain vastuksen

Taulukossa on yksi luku riviä kohden — täyttöaste on **rivin indeksi**, ei erillinen sarake:

```cpp
static constexpr float OHMS[CAL_POINTS] = {
      0.0f,   //   0 %  —   0 L  (empty)
     19.0f,   //  10 %  —  10 L
     ...
```

Näin sarakkeita ei voi vahingossa saada epätahtiin keskenään, ja jokaisen 10 %:n täytön jälkeen muokattavana on täsmälleen yksi luku sillä rivillä, jonka kommentti sen nimeää.

Mukana toimitetut arvot ovat **paikkamerkki** — tasainen 0→190 Ω ramppi. Firmware toimii siis lineaarisena kuten polttoainepuoli, kunnes tankki on kalibroitu. Se ei koskaan lähde liikkeelle rikkinäisenä, ja jokainen rivi on ilmiselvästi pyöreä luku joka odottaa korvaamista.

---

## Kalibroinnin esivalmistelut

1. Kytke anturi A2:een ja asenna **CCS-jumpperi A2:lle**.
2. Poista kommentti riviltä `HALMETApplication::handleWaterRead()`:

   ```cpp
   //Serial.printf("[WATER] %.1f ohm\n", ohms);   // uncomment for calibration
   ```
3. Käännä ja lataa firmware, avaa sarjamonitori **115200 baudia**.
4. Lukeman pitäisi tulostua ~2 s välein.

> **Vene on oltava suorassa ja paikallaan koko kalibroinnin ajan.** Kallistuma ja lainehdinta ovat juuri se häiriö, jonka suodatin on olemassa poistamaan — ja juuri se, joka pilaa mittaukset.

---

## Mittausproseduuri

**Vaihe 0 — tyhjä.** Tyhjennä tankki kokonaan. Odota 60 s. Lukema värähtelee muutaman kymmenesosan ohmin verran: kirjaa **värähtelyn silmämääräinen keskikohta**, älä yksittäistä näytettä. Kirjoita luku riville `0 %`.

**Vaiheet 1–10 — 10 litraa kerrallaan.** Lisää tasan 10 L kalibroidulla mitalla tai virtausmittarilla. Odota **vähintään 60 s**, jotta pinta tasaantuu ja uimuri asettuu. Kirjaa värähtelyn keskikohta vastaavalle riville. Toista riville `100 %` asti.

> Tilavuuden tarkkuus on tässä tärkeämpää kuin ohmilukeman tarkkuus. Kaadon 10 %:n virhe jättää käyrään pysyvän mutkan, kun taas lukeman kohina keskiarvoistuu joka tapauksessa pois ajonaikaisessa suodatuksessa.

**Vaihe 11 — `MAX_OHMS`.** Aseta `WaterSensor.h`:ssä `MAX_OHMS` noin **kaksinkertaiseksi täyden tankin lukemaan** nähden. Oletusarvo 400 Ω on varovainen lähtökohta; jos täysi tankki näyttää esim. 190 Ω, sopiva arvo on ~380 Ω.

**Vaihe 12 — syötä ja käännä.** Muokkaa vain `WaterCal::OHMS`-taulukon ensimmäistä saraketta. Käännä uudelleen.

- **Käännös onnistuu** → taulukko on monotoninen ja askeleet ≥ 2 Ω. Valmista. Kommentoi `printf` takaisin ja lataa firmware.
- **Käännös epäonnistuu** `tableIsValid`-assertioon → kaksi peräkkäistä riviä on yhtä suuria, väärinpäin tai alle 2 Ω etäisyydellä. Katso §Vianetsintä.

**Vaihe 13 — tarkistus.** Varmista SignalK-palvelimen data browserista että `tanks.freshWater.0.currentLevel` on täydellä tankilla lähellä 1.0 ja `tanks.freshWater.0.capacity` on 0.1.

---

## Vianetsintä: käännös kaatuu taulukkoon

```
static assertion failed: WaterCal::OHMS is not strictly increasing by at least MIN_STEP_OHMS.
```

Tämä on **tarkoituksellinen käännösvirhe**, ei bugi. Vaihtoehto — arvojen hiljainen siistiminen ohjelmallisesti — piilottaisi virheellisen mittauksen pysyvästi ja tekisi mittarista huomaamattomasti väärän. Kääntäjä pakottaa päätöksen sille, jolla on kanisteri kädessä.

`MIN_STEP_OHMS = 2.0f` on tässä tärkeämpi kuin pelkkä monotonisuus: 0,1 Ω:n askel kuvaisi anturin kohinan kokonaiseksi 10 %:n hyppäykseksi mittarissa. ADS1115:n resoluutio on 0,1875 Ω/LSB, joten 2 Ω ≈ 11 LSB.

Kolme kelvollista ratkaisua, paremmuusjärjestyksessä:

1. **Mittaa kyseinen porras uudelleen.** Todennäköisin syy on liian lyhyt tasaantumisaika tai epätarkka kaato.
2. **Keskiarvoista ristiriitaiset lukemat** ja erota rivit `MIN_STEP_OHMS`:n verran toisistaan.
3. **Hyväksy anturin litteä kohta.** Jos uimurivarsi osuu väliseinään, tankki on aidosti erottelukyvytön sillä välillä. Erota rivit käsin — tietoisena siitä, että mittari arvaa tuolla alueella.

---

## Suodatusparametrit — miksi nopeampi kuin polttoaineella

| Parametri | Vesi | Polttoaine | Perustelu |
|---|---|---|---|
| `MEDIAN_WINDOW` | 60 (~2 min) | 120 (~4 min) | Kattaa lainehdinnan (sekunteja) yhtä hyvin, mutta puolittaa käynnistyksen sokean jakson |
| `EMA_ALPHA` | 0.02 (τ ≈ 100 s) | 0.005 (τ ≈ 400 s) | Vedenkulutus on purskeista, ei jatkuvaa |

Polttoaineen erittäin hidas suodatus on perusteltu `docs/fuel_level_filtering.md`:ssä: kulutus on ~7 L/h eli 7 mm/h pinnanalenema, ja signaali/kohina-suhde yksittäisessä näytteessä on luokkaa 1:50 000.

**Vesi on toisenlainen ongelma.** Suihku tai tiskaus vie 10–20 L eli 10–20 % tankista muutamassa minuutissa. Polttoainesuodattimen ~20 minuutin vasteaika tarkoittaisi, että mittari näyttäisi yhä täyttä pitkään sen jälkeen kun viidennes tankista on käytetty — käyttökelvoton lukema sille, joka miettii pitääkö vettä täydentää.

Lainehdinnan amplitudi on kuitenkin samaa luokkaa, joten mediaani-ikkunan on yhä katettava se.

---

## Käynnistyskäyttäytyminen

Kylmäkäynnistyksen jälkeen lukema on **suodattamaton ensimmäiset ~2 minuuttia** (vaihe 1), minkä jälkeen se asettuu ~5 minuutin kuluessa. **Pieni porras 2 minuutin kohdalla on normaali**, ei vika: siinä siirrytään raakalukemasta mediaani+EMA-suodatettuun arvoon.

---

## Tiheämpi tai harvempi taulukko

Jos tankki vaatii paremman resoluution, muuta `CAL_POINTS` ja lisää rivejä — `RATIO_STEP` mukautuu automaattisesti. Ainoa ehdoton vaatimus on että **täyttöportaat ovat tasavälisiä**, koska täyttöaste johdetaan rivin indeksistä. Epätasaiset portaat vaatisivat toisen sarakkeen eikä tämä toteutus tue niitä.
