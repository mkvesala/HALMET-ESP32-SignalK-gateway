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
2. Varmista että `HALMETApplication.h`:ssä on `WEB_UI_ENABLED = true` (oletus).
3. Käännä ja lataa firmware, odota että WiFi-yhteys nousee.
4. Avaa selaimessa `http://<WIFI_STATIC_IP>/` — puhelin riittää, ja se on veneessä
   käytännöllisempi kuin kannettava USB-kaapelin päässä.

Sivu näyttää molemmat tankit ja päivittyy sekunnin välein:

```
                ohms    filt      mV    adc   ratio
Water  A2      123.4   121.8   123.4    658   0.641
Fuel   A1       87.1    86.9    87.1    465   0.474

water:     60/60 samples, updated 0.4 s ago
```

**`filt`-sarake on se luku, joka taulukkoon kirjataan.** Se on laitteen oma
mediaani→EMA-suodatettu arvo (τ ≈ 100 s) — sama, jolla mittarikin toimii. Se korvaa
värähtelyn silmämääräisen keskikohdan arvioinnin: kun `samples` näyttää `60/60` ja
`filt` on lakannut liikkumasta, arvo on valmis kirjattavaksi.

> **Varaa aikaa noin tunti.** Sama aikavakio, joka tekee `filt`-arvosta luotettavan,
> tekee siitä myös hitaan: jokaisen 10 L kaadon jälkeen se tarvitsee ~5 min asettuakseen
> (perustelu vaiheissa 1–10). Kymmenen askelta on siis noin 50 min pelkkää odottelua.
> Tämä ei ole työvaihe jonka voi kiirehtiä läpi — liian aikaisin kirjattu lukema tuottaa
> pysyvästi väärän käyrän, joka ei näy missään tarkistuksessa.

> **Vene on oltava suorassa ja paikallaan koko kalibroinnin ajan.** Kallistuma ja lainehdinta ovat juuri se häiriö, jonka suodatin on olemassa poistamaan — ja juuri se, joka pilaa mittaukset.

> **Sivu toimii vain aluksen WiFin kautta.** `handleWebUI()` on portitettu
> `WifiState::CONNECTED`-ehdolla, eikä laitteen oma SoftAP kelpaa varareitiksi: se on
> piilotettu ja deauthaa jokaisen liittyjän välittömästi. mDNS:ää ei ole, joten
> osoite on aina `secrets.h`:n `WIFI_STATIC_IP`. Jos WiFi ei ole käytettävissä, katso
> §Varareitti: sarjamonitori.

---

## Mittausproseduuri

**Vaihe 0 — tyhjä.** Tyhjennä tankki kokonaan. Odota, kunnes `samples` näyttää `60/60` — noin **2 min** käynnistyksestä. Kirjaa `filt`-lukema riville `0 %`.

Tämä ensimmäinen piste on nopea: kun ikkuna täyttyy, EMA alustetaan suoraan ensimmäiseen mediaaniin (vaihe 2), joten se ei ryömi paikalleen vaan on heti oikein. Seuraavat pisteet eivät ole.

**Vaiheet 1–10 — 10 litraa kerrallaan.** Lisää tasan 10 L kalibroidulla mitalla tai virtausmittarilla. Odota **noin 5 minuuttia**. Kirjaa `filt` vastaavalle riville. Toista riville `100 %` asti.

> **Miksi 5 min eikä minuutti.** Pinta tasaantuu ja uimuri asettuu minuutissa, mutta `filt` on EMA aikavakiolla τ ≈ 100 s, ja se seuraa askelta eksponentiaalisesti. 19 Ω:n askeleesta on 60 s kohdalla vielä **55 % jäljellä** (e^−0.6 ≈ 0.55), 2 min kohdalla 30 %, ja vasta ~5 min kohdalla alle 5 % eli alle 1 Ω. Minuutin odotuksella jokainen rivi jäisi systemaattisesti liian alas — ja koska virhe on samansuuntainen joka rivillä, se ei näy taulukon monotonisuustarkistuksessa vaan tuottaa pysyvästi väärän mutta täysin uskottavan näköisen käyrän.
>
> Käytännössä: odota kunnes `filt` on lakannut liikkumasta desimaalitasolla. Se on luotettavampi merkki kuin kello.

> Sivun alalaidassa oleva `WaterCal::OHMS`-taulukko merkitsee `>`-merkillä sen välin, jolla nykyinen lukema on. Näet siis suoraan, mille riville olet mittaamassa arvoa ja mitä siinä nyt lukee. Huomaa että merkki seuraa `filt`-arvoa, joten se siirtyy vasta suodattimen perässä.

> Tilavuuden tarkkuus on tässä tärkeämpää kuin ohmilukeman tarkkuus. Kaadon 10 %:n virhe jättää käyrään pysyvän mutkan, kun taas lukeman kohina keskiarvoistuu joka tapauksessa pois ajonaikaisessa suodatuksessa.

**Vaihe 11 — `MAX_OHMS`.** Aseta `WaterSensor.h`:ssä `MAX_OHMS` noin **kaksinkertaiseksi täyden tankin lukemaan** nähden. Oletusarvo 400 Ω on varovainen lähtökohta; jos täysi tankki näyttää esim. 190 Ω, sopiva arvo on ~380 Ω. Voimassa oleva arvo näkyy kalibrointisivulla rivillä `water open-circuit limit`.

**Vaihe 12 — syötä ja käännä.** Muokkaa vain `WaterCal::OHMS`-taulukon ensimmäistä saraketta. Käännä uudelleen.

- **Käännös onnistuu** → taulukko on monotoninen ja askeleet ≥ 2 Ω. Valmista. Lataa firmware ja aseta `WEB_UI_ENABLED = false`, jos et halua kalibrointisivua jäävän tuotantobuildiin.
- **Käännös epäonnistuu** `tableIsValid`-assertioon → kaksi peräkkäistä riviä on yhtä suuria, väärinpäin tai alle 2 Ω etäisyydellä. Katso §Vianetsintä.

**Vaihe 13 — tarkistus.** Varmista SignalK-palvelimen data browserista että `tanks.freshWater.0.currentLevel` on täydellä tankilla lähellä 1.0 ja `tanks.freshWater.0.capacity` on 0.1.

---

## Varareitti: sarjamonitori

Jos WiFiä ei ole saatavilla, kalibrointisivu ei ole tavoitettavissa. Vanha reitti toimii
edelleen: poista kommentti riviltä `HALMETApplication::handleWaterRead()`:

```cpp
//Serial.printf("[WATER] %.1f ohm\n", ohms);   // uncomment for calibration
```

Käännä, lataa ja avaa sarjamonitori **115200 baudia**; lukema tulostuu ~2 s välein.
Huomaa että tämä tulostaa **suodattamattoman** raakalukeman — silloin joudut arvioimaan
värähtelyn keskikohdan silmämääräisesti 60 s ajalta, mikä on juuri se työvaihe jonka
kalibrointisivun `filt`-sarake poistaa.

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
