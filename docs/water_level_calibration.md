# Makeavesitankin kalibrointi — HALMET ESP32 SignalK Gateway

## Taustaa

Fridan makeavesitankki: **80 L**, muodoltaan **ylösalaisin oleva pyramidi** — se levenee ylöspäin.

Polttoainetankki on säännöllisen muotoinen, joten `VDOProcessor` muuntaa vastuksen täyttöasteeksi kahden pisteen lineaarisella sovituksella (3 Ω = tyhjä, 180 Ω = täysi). Makeavesitankissa tämä ei toimi: poikkileikkausala vaihtelee korkeuden mukaan, joten sama vastuksen muutos vastaa eri litramäärää tankin eri kohdissa.

Siksi `WaterProcessor` käyttää **mitattua kalibrointitaulukkoa**: taulukossa on yksi vastuslukema jokaista 2,5 litran porrasta kohden, ja ohjelma interpoloi lineaarisesti pisteiden välillä. Mitattu ohmimäärä litraa kohden **pienenee** täytön myötä (~2,92 Ω/L välillä 0–30 L, ~1,73 Ω/L välillä 30–55 L), mikä on juuri sitä mitä ylöspäin levenevä tankki tarkoittaa.

Anturi: resistiivinen lähetin nimellisalueella **0–180 Ω**, HALMETin analogiatulo **A2** (ADS1115 kanava 1), **CCS-jumpperi A2:lle asennettuna**.

---

## Tankin geometria ja anturin rajat

Mittaukset 21.8.2026 (täyttö 0→80 L, sen jälkeen valutus 80→0 L) paljastivat kolme asiaa, jotka on ymmärrettävä ennen kuin taulukkoon koskee.

### Lähetin on porrastettu, ei portaaton

Kaikki kahdeksan mitattua pistettä osuvat sarjaan `R = 0,8 + 14,53·k`, missä k on kokonaisluku, ±0,4 Ω tarkkuudella:

| k | 0 | 2 | 4 | 6 | 7 | 8 | 9 | 10 | 12 |
|---|---|---|---|---|---|---|---|---|---|
| Sovite Ω | 0,8 | 29,9 | 58,9 | 88,0 | 102,5 | 117,0 | 131,6 | 146,1 | 175,2 |
| Mitattu Ω | 0,8 | 30,0 | 59,3 | 88,3 | 102,8 | 117,2 | 131,6 | 146,1 | 174,8 |

Kyseessä on lankakierretty vastuskortti liukukoskettimella: se hyppää kontaktilta toiselle ~14,5 Ω askelin. **Anturin tilavuusresoluutio on siten ~5 L tankin alaosassa ja ~10 L yläosassa.** Sitä tarkempaa kalibrointia ei kannata tavoitella, eikä 0,1 Ω:n eroilla ole merkitystä.

**174,8 Ω (k = 12) on kortin ylin kontakti.** Sitä korkeampaa lukemaa ei tule koskaan, mikä sopii yhteen nimellisen 0–180 Ω luokituksen kanssa.

### Kaksi tasannetta, eri syistä — älä sekoita niitä

**174,8 Ω pysyy vakiona 80 litrasta 57,5 litraan.** Tämä on yllä mainittu ylin kontakti: tankin koko yläkolmannes näkyy samana lukemana. Rivejä 57,5–80 L **ei siis voi mitata**, ja ne on johdettu laskennallisesti: suora mitatusta pisteestä (57,5 L, 146,1 Ω) lähettimen nimelliseen maksimiin (80 L, 180,0 Ω). Se vaatii kaltevuuden 1,507 Ω/L, joka on *pienempi* kuin sen alapuolella mitattu 1,732 Ω/L — täsmälleen mitä yhä leveneväksi jatkuva tankki edellyttää. Oletus 0–180 Ω lähettimestä siis tarkistuu mitattua muotoa vasten sen sijaan että se olisi oletettu sisään.

**146,1 Ω on ilmataskun kattoraja.** 57,5 litran kohdalla pinta laskee sen luukun tasolle, johon anturi on kiinnitetty. Luukun alle muodostuu ilmatasku, joka estää kohoa nousemasta, ja lukema hyppää 174,8 → 146,1 ilman että tilavuus muuttuu lainkaan.

Tätä porrasta **ei korjata ohjelmallisesti.** Jumissa oleva koho lukee aina liian vähän, koska ilmataskussa siihen ei kohdistu nostetta — vika kaatuu siis turvalliseen suuntaan. Mikään suodatin ei voi palauttaa tasoa, jolle koho ei koskaan noussut. Huippupitosuodatin (peak hold) korjaisi alilukeman mutta rikkoisi normaalin kulutusseurannan, koska taso laskee aidosti koko ajan. **Todellinen korjaus on mekaaninen:** pieni ilmareikä luukkulevyn korkeimpaan kohtaan, tai tankin huohotinlinjan otto tankin todelliseen lakipisteeseen.

### Mitä tästä seuraa mittarille

Alue **57,5–80 L raportoituu vakiona 76,6 litrana**, koska lähetin antaa koko alueella saman 174,8 Ω. Kun tankissa on todellisuudessa 58 L, mittari näyttää 76,6 L eli **yliarvio on pahimmillaan ~19 L**. Kun taso lopulta ylittää portaan, lukema putoaa kerralla 57,5 litraan.

Alue **0–57,5 L on mitattu** ja tarkka lähettimen ~5 L porrasresoluution rajoissa. Se on se osa, jolla vedenriitto oikeasti ratkeaa.

---

## Miksi taulukko tallentaa vain vastuksen

Taulukossa on yksi luku riviä kohden — **litramäärä on rivin indeksi**, ei erillinen sarake. Rivi *i* vastaa `i × CAL_STEP_L` litraa:

```cpp
static constexpr float OHMS[CAL_POINTS] = {
      0.8f,   //   0.0 L  (m) empty
      8.1f,   //   2.5 L  (i)
     15.4f,   //   5.0 L  (i)
     ...
```

Näin sarakkeita ei voi vahingossa saada epätahtiin keskenään, ja jokaisen mittauksen jälkeen muokattavana on täsmälleen yksi luku sillä rivillä, jonka kommentti sen nimeää.

Rivin lopun merkintä kertoo arvon alkuperän:

| Merkki | Merkitys |
|---|---|
| `(m)` | Mitattu veneessä |
| `(i)` | Lineaarinen interpolaatio kahden mitatun rivin välillä |
| `(t)` | Laskennallisesti johdettu — vain alue 57,5–80 L, ks. §Tankin geometria |

**Huomaa että täyttöaste ei ole enää sama asia kuin rivin indeksi.** Suhde `currentLevel` lasketaan aina tankin fyysistä 80 litran kapasiteettia vasten, joten se saavuttaa 1,0 vasta lähettimen nimellisellä maksimilla 180 Ω. Käytännössä se ei ylitä **~0,957**, koska 174,8 Ω on korkein lukema jonka lähetin pystyy antamaan.

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
mediaani→EMA-suodatettu arvo (τ ≈ 50 s) — sama, jolla mittarikin toimii. Se korvaa
värähtelyn silmämääräisen keskikohdan arvioinnin: kun `samples` näyttää `60/60` ja
`filt` on lakannut liikkumasta, arvo on valmis kirjattavaksi.

> **Varaa aikaa noin tunti.** Sama aikavakio, joka tekee `filt`-arvosta luotettavan,
> tekee siitä myös hitaan: jokaisen kaadon tai valutuksen jälkeen se tarvitsee ~2,5 min
> asettuakseen (perustelu vaiheessa 2). Kymmenkunta askelta on siis noin 25 min pelkkää odottelua.
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

**Mittaa valuttamalla, älä täyttämällä.** Alkuperäinen menettely (täytä 10 L kerrallaan tyhjästä) toimii alueella 0–40 L, mutta ei kerro mitään yläpäästä: ilmatasku ehtii muodostua luukun alle jo täytön aikana ja pitää kohoa alhaalla, jolloin lukema jää liian matalaksi ilman että mikään tarkistus huomaa sitä. Valuttaminen täydestä tankista antaa saman käyrän ilman tätä ansaa, koska pinta on koko ajan laskemassa kohti kohoa eikä nousemassa sitä kohti.

**Vaihe 0 — täysi.** Täytä tankki täyteen. **Keinuta venettä**, kunnes lukema nousee arvoon 174,8 Ω — se on merkki siitä että luukun alle jäänyt ilmatasku on purkautunut. Odota, kunnes `samples` näyttää `60/60` (noin **2 min** käynnistyksestä).

Tämä ensimmäinen piste on nopea: kun ikkuna täyttyy, EMA alustetaan suoraan ensimmäiseen mediaaniin (vaihe 2), joten se ei ryömi paikalleen vaan on heti oikein. Seuraavat pisteet eivät ole.

**Vaihe 1 — löydä porras.** Valuta 5 L kerrallaan mitattuun astiaan. Keinuta venettä **ennen** jokaista asettumisjaksoa, älä sen jälkeen. Lukema pysyy arvossa 174,8 Ω useita askeleita — se on lähettimen ylin kontakti, ei mittausvirhe. Litramäärä, jolla lukema **ensimmäisen kerran putoaa alle 174,8 Ω**, on luukun alapinnan taso; nykyisellä tankilla se on 57,5 L ja lukema putoaa siinä suoraan arvoon 146,1 Ω.

**Vaihe 2 — kirjaa käyrä portaan alapuolelta.** Portaan alapuolella ilmataskua ei enää synny, joten keinuttamista ei tarvita ja yksi lukema riittää. Jatka 5 L askelin tyhjään asti. Odota jokaisen valutuksen jälkeen **noin 2,5 minuuttia** ja kirjaa `filt` sitä litramäärää vastaavalle riville.

> **Kirjaa mittauksesi litroina, älä prosentteina.** Taulukon rivi *i* on `i × CAL_STEP_L` litraa. Jos mittauspisteesi eivät osu nykyiseen 2,5 L ruudukkoon, valitse `CAL_STEP_L` niin että ne osuvat — ks. §Tiheämpi tai harvempi taulukko. Rivit mittauspisteiden välissä täytetään lineaarisella interpolaatiolla ja merkitään `(i)`.

> **Miksi 2,5 min eikä minuutti.** Pinta tasaantuu ja uimuri asettuu minuutissa, mutta `filt` on EMA aikavakiolla τ ≈ 50 s, ja se seuraa askelta eksponentiaalisesti. Lähettimen 14,5 Ω kontaktiaskeleesta on 60 s kohdalla vielä **30 % jäljellä** (e^−1,2 ≈ 0,30), 2 min kohdalla 9 %, ja vasta ~2,5 min kohdalla alle 5 % eli alle 0,7 Ω. Minuutin odotuksella jokainen rivi jäisi systemaattisesti liian alas — ja koska virhe on samansuuntainen joka rivillä, se ei näy taulukon monotonisuustarkistuksessa vaan tuottaa pysyvästi väärän mutta täysin uskottavan näköisen käyrän.
>
> Käytännössä: odota kunnes `filt` on lakannut liikkumasta desimaalitasolla. Se on luotettavampi merkki kuin kello.

> Sivun alalaidassa oleva `WaterCal::OHMS`-taulukko merkitsee `>`-merkillä sen välin, jolla nykyinen lukema on. Näet siis suoraan, mille riville olet mittaamassa arvoa ja mitä siinä nyt lukee. Huomaa että merkki seuraa `filt`-arvoa, joten se siirtyy vasta suodattimen perässä.

> Tilavuuden tarkkuus on tässä tärkeämpää kuin ohmilukeman tarkkuus. Kaadon 10 %:n virhe jättää käyrään pysyvän mutkan, kun taas lukeman kohina keskiarvoistuu joka tapauksessa pois ajonaikaisessa suodatuksessa.

**Vaihe 3 — `MAX_OHMS`.** Tarkista että `WaterSensor.h`:n `MAX_OHMS` on selvästi taulukon ylimmän rivin yläpuolella mutta kaukana avoimen piirin lukemasta. Avoin piiri ajaa tulon rajalle (6,144 V / 1 mA ≈ 6144 Ω), joten kynnyksen tarkka arvo ei ole kriittinen. Nykyinen 400 Ω toimii taulukon 180 Ω maksimin kanssa sellaisenaan. Voimassa oleva arvo näkyy kalibrointisivulla rivillä `water open-circuit limit`.

**Vaihe 4 — syötä ja käännä.** Muokkaa vain `WaterCal::OHMS`-taulukon ensimmäistä saraketta. Käännä uudelleen.

- **Käännös onnistuu** → taulukko on monotoninen ja askeleet ≥ 2 Ω. Valmista. Lataa firmware ja aseta `WEB_UI_ENABLED = false`, jos et halua kalibrointisivua jäävän tuotantobuildiin.
- **Käännös epäonnistuu** `tableIsValid`-assertioon → kaksi peräkkäistä riviä on yhtä suuria, väärinpäin tai alle 2 Ω etäisyydellä. Katso §Vianetsintä.

**Vaihe 5 — tarkistus.** Varmista SignalK-palvelimen data browserista että `tanks.freshWater.0.capacity` on **0.08** ja että `tanks.freshWater.0.currentLevel` vastaa taulukon odotusarvoa:

| Lukema | litraa | `currentLevel` |
|---|---|---|
| 0,8 Ω | 0,0 | 0,000 |
| 102,8 Ω | 40,0 | 0,500 |
| 146,1 Ω | 57,5 | 0,719 |
| 174,8 Ω | 76,6 | 0,957 |

**`currentLevel` ei siis saavuta arvoa 1,0 täydelläkään tankilla** — 174,8 Ω on korkein lukema jonka lähetin antaa. Tämä ei ole vika. Kalibrointisivun rivi `water vol:` näyttää saman litroina, ja merkintä `SENDER CEILING` kertoo että lukema on kattorajalla eikä erottele väliä 57,5–80 L.

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

> **Keino 3 kelpaa vain aitoon lähettimen tasanteeseen.** Jos tasanne johtuu ilmataskusta — tunnusmerkki: sama lukema toistuu useilla eri litramäärillä ja hyppää ylöspäin kun venettä keinuttaa — rivien erottelu käsin leipoo mittausvirheen taulukkoon pysyvästi. Mittaa uudelleen valuttamalla ja keinuttamalla, tai lyhennä taulukko mitatulle alueelle. Ks. §Tankin geometria ja anturin rajat.

---

## Suodatusparametrit — miksi nopeampi kuin polttoaineella

| Parametri | Vesi | Polttoaine | Perustelu |
|---|---|---|---|
| `MEDIAN_WINDOW` | 60 (~2 min) | 120 (~4 min) | Kattaa lainehdinnan (sekunteja) yhtä hyvin, mutta puolittaa käynnistyksen sokean jakson |
| `EMA_ALPHA` | 0.04 (τ ≈ 50 s) | 0.005 (τ ≈ 400 s) | Vedenkulutus on purskeista, ei jatkuvaa |

Polttoaineen erittäin hidas suodatus on perusteltu `docs/fuel_level_filtering.md`:ssä: kulutus on ~7 L/h eli 7 mm/h pinnanalenema, ja signaali/kohina-suhde yksittäisessä näytteessä on luokkaa 1:50 000.

**Vesi on toisenlainen ongelma.** Suihku tai tiskaus vie 10–20 L eli 10–20 % tankista muutamassa minuutissa. Polttoainesuodattimen ~20 minuutin vasteaika tarkoittaisi, että mittari näyttäisi yhä täyttä pitkään sen jälkeen kun viidennes tankista on käytetty — käyttökelvoton lukema sille, joka miettii pitääkö vettä täydentää.

Lainehdinnan amplitudi on kuitenkin samaa luokkaa, joten mediaani-ikkunan on yhä katettava se.

---

## Käynnistyskäyttäytyminen

Kylmäkäynnistyksen jälkeen lukema on **suodattamaton ensimmäiset ~2 minuuttia** (vaihe 1), minkä jälkeen se asettuu ~2,5 minuutin kuluessa. **Pieni porras 2 minuutin kohdalla on normaali**, ei vika: siinä siirrytään raakalukemasta mediaani+EMA-suodatettuun arvoon.

---

## Tiheämpi tai harvempi taulukko

Jos tankki vaatii paremman resoluution, muuta `CAL_POINTS` ja `CAL_STEP_L` ja lisää rivejä — `CAL_MAX_L` mukautuu automaattisesti. Ainoa ehdoton vaatimus on että **portaat ovat tasavälisiä**, koska litramäärä johdetaan rivin indeksistä. Epätasaiset portaat vaatisivat toisen sarakkeen eikä tämä toteutus tue niitä.

Nykyinen askel on **2,5 L**, koska se on karkein ruudukko jolla kaikki mitatut pisteet — myös portaan kohta 57,5 L — osuvat tasan omalle rivilleen. Karkeampi 5 L ruudukko siirtäisi ilmataskun kattorajan 146,1 Ω noin 1,5 litraa väärään paikkaan, ja se on arvo jossa lukema seisoo aina kun taskua on.

Kaksi tarkistusta pitää huolen siitä ettei ruudukon muutos mene ohi:

- `tableIsValid` vaatii että jokainen askel kasvaa vähintään `MIN_STEP_OHMS` (2 Ω) verran. Tiheämpi ruudukko tarkoittaa pienempiä ohmiaskelia, joten liian tiheä taulukko kaatuu käännöksessä. Nykyisen taulukon pienin askel on 3,6 Ω.
- `CAL_MAX_L <= TANK_CAPACITY_L` estää taulukkoa ulottumasta tankin fyysisen tilavuuden yli.
