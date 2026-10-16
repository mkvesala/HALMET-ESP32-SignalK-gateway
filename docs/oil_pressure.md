# Öljynpaineen passiivinen mittaus — HALMET ESP32 SignalK Gateway

## Taustaa

Fridan moottorin öljynpaine näytetään VDO cockpit -mittarilla (Ø 52 mm, **0–5 bar**). Anturi on tyyppiä *negative earth with warning contact*: liitännät **S** (mittaussignaali) ja **WK** (varoituskosketin), anturin runko maadoittuu moottorilohkoon. Koko mittariston syöttö on **12.3 V** DC-DC-muuntimelta.

HALMET lukee painetiedon **passiivisesti** analogiatulosta **A3** (ADS1115 kanava 2) ilman CCS-jumpperia, rinnakkain mittarin kanssa. Analoginen mittari toimii täsmälleen kuten ennen. WK-johdinta ei käytetä tässä.

Lähetettävät tiedot:

| Kohde | Polku / viesti | Yksikkö | Väli |
|---|---|---|---|
| SignalK | `propulsion.0.oilPressure` | Pa | ~1 s (`SK_OIL_TX_MS` 1013 ms) |
| ESP-NOW | `HALMET_OIL_DELTA = 10`, `HALMETOilDelta.oil_pressure_pa` | Pa | ~3 s (`ESPNOW_TX_MS`) |

---

## Mittausperiaate

### Miksi ei CCS-resistanssimittausta

VDO-mittarissa on ratiomittari (*System Ke*, kolme kelaa ja kääntyvä magneetti). Mittari itse syöttää virran S-johtimen kautta anturiin. Jos A3:lle asennettaisiin CCS-jumpperi, HALMETin vakiovirtalähde syöttäisi lisävirtaa samaan solmuun ja mittarin näyttämä vääristyisi. Siksi A3 mittaa vain jännitettä.

### Thevenin-malli

Mittarin sisäinen piiri (kelat, kompensointi- ja pudotusvastus) on lineaarinen resistiivinen verkko, jota syötetään vakiojännitteellä. S-navasta katsottuna se on siis tarkalleen jännitelähde `V_OC` sarjassa resistanssin `R_TH` kanssa:

```
V = V_OC · R / (R + R_TH)      =>      R = R_TH · V / (V_OC − V)
```

`V_OC` (S-navan jännite anturi irti) ja `R_TH` mitataan kerran käyttöönotossa **HALMETin omilla lukemilla samasta mittauspisteestä**. Kaava on silloin puhdas suhde, joten seuraavat kumoutuvat:

- HALMETin tulojakajan (33.3/3.3, Hat Labs -esimerkkifirmwaren mukaan) tarkka arvo
- suojaava sarjavastus HALMET-johtimessa
- S-johtimen ja maapaluun resistanssi
- HALMET-tulon kuormitus (~33 kΩ rinnan anturin kanssa → alle 0.6 % virhe täydellä asteikolla)

Ehto: 12.3 V syöttö on vakaa. DC-DC-muunnin takaa sen. Suoraan akusta syötetyllä mittaristolla `V_OC` vaihtelisi latausjännitteen mukana.

### Anturikäyrä

VDO cockpit -käsikirja (TU00-0755-0000002, luku 6.5), mittari 0–5 bar:

| bar | 0 | 0.5 | 1 | 1.5 | 2 | 2.5 | 3 | 3.5 | 4 | 4.5 | 5 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Ω | 10 | 30 | 48 | 65 | 82 | 99 | 116 | 134 | 151 | 168 | 184 |

Taulukko on `OilCal::OHMS` (`OilProcessor.h`). Rivi-indeksi kertoo paineen (`i * 0.5 bar`), ja taulukon pisteiden välillä interpoloidaan lineaarisesti. Alle 10 Ω tulkitaan 0 bariksi ja yli 184 Ω 5 bariksi, eli taulukon ulkopuolelle ei ekstrapoloida. Monotonisuus tarkistetaan käännösaikana `static_assert`-lausekkeella.

### Suodatus ja viat

- Luku 499 ms välein → mediaani 5 näytteestä → EMA α=0.3. Suodatus on tarkoituksella nopea, koska öljynpaine muuttuu paljon nopeammin kuin tankin pinta.
- **Hylätään:** `V ≥ V_OC` (anturi tai S-johdin poikki), `R < 3 Ω` (S oikosulussa maihin tai mittaristo ilman virtaa), `R > 300 Ω`.
- **Vanhenemissuoja:** jos viimeisin hyväksytty näyte on yli 5 s vanha, arvoa ei lähetetä SignalKiin eikä ESP-NOW:lla. Vanhentunut "paine OK" -lukema on vaarallisempi kuin puuttuva tieto.
- **Kalibroimaton malli:** niin kauan kuin `OilCal::CALIBRATED = false`, kalibrointisivulla näkyvät jännite, ohmit ja bar-arvo, mutta mitään ei lähetetä.

---

## Kytkentä

### Mistä jännite otetaan: mittarin S-navasta piggyback-liittimellä

**Suositus:** piggyback-lattaliitin (6.3 mm, "satulaliitin") **mittarin takana olevaan S-napaan**.

| Vaihtoehto | Arvio |
|---|---|
| **Mittarin S-napa, piggyback** | ✅ Paras. HALMET näkee täsmälleen saman jännitteen kuin mittari, eli niiden näyttämät seuraavat toisiaan. HALMETin GND ja mittarin ⏚ ovat samassa paneelimaassa. Johdinta ei katkaista, joten kytkennän saa purettua. Ei moottoritilan lämpöä eikä tärinää. |
| Anturin S-napa moottorilla | ⚠️ Vain jos HALMET on moottoritilassa. Moottorilohkon ja HALMETin maan välinen potentiaaliero (käynnistin- ja laturivirrat) näkyy suoraan mittausvirheenä. Lisäksi napa on kuuma ja tärisevä. |
| Haaroitus S-johtimesta (T-tap, scotchlok) | ❌ Huonoin. Rikkoo eristeen, altistaa korroosiolle ja on vaikea palauttaa. |

### Kytkentäohje

```
Mittarin S-napa ──┬── (alkuperäinen S-johdin anturille, koskematon)
                  │
            piggyback-liitin
                  │
             1 kΩ 0.25 W  (kutistesukassa heti liittimen jälkeen)
                  │
                  └──────────────────────── HALMET A3   (EI CCS-jumpperia)

Mittarin ⏚ / paneelin maakisko ──────────── HALMET GND (analogiliittimen GND)
```

1. **A3: ei CCS-jumpperia.** A1:n ja A2:n jumpperit pysyvät paikallaan.
2. 1 kΩ sarjavastus suojaa mittaripiiriä: jos HALMET-johdin hankautuu maihin, mittari ei putoa nollaan. Kalibrointi kumoaa vastuksen vaikutuksen. Vastuksen voi jättää pois, mutta kalibrointi on tehtävä samalla kytkennällä, jota käytetään lopullisesti.
3. Johdin 0.5–0.75 mm², kierretty pari GND:n kanssa, pois laturin ja käynnistimen kaapeleiden vierestä.
4. Mittarin +, ⏚ ja anturin S/WK-johdotus jätetään ennalleen. WK jää tässä kytkemättä.
5. **Maaero:** mittaa yleismittarilla moottori käynnissä jännite HALMET GND ↔ mittarin ⏚. Sen pitää olla alle 20 mV.

---

## Kalibrointi (kerran käyttöönotossa)

Tarvitset yleismittarin ja tunnetut vastukset, esimerkiksi 47 Ω, 100 Ω ja 150 Ω. Mittaa vastusten todelliset arvot yleismittarilla.

1. Kytke HALMET kuten yllä. Aseta `HALMETApplication::WEB_UI_ENABLED = true`, käännä ja lataa. Avaa `http://<HALMET-IP>/`.
2. Kytke mittariston virta päälle. **Moottori pysyy sammuksissa.** Rivi `Oil A3` näyttää jännitteen.
3. **V_OC:** irrota S-johdin anturin navalta. Mittari nousee täyteen asteikkoon. Kirjaa kalibrointisivun jännite = `V_OC`.
4. **R_TH:** kytke S-johtimen päästä 100 Ω vastus moottorin runkoon (anturin tilalle). Kirjaa jännite `V`. Laske:
   ```
   R_TH = R · (V_OC − V) / V
   ```
   Mittarin pitäisi näyttää noin 2.5 bar (99 Ω = 2.5 bar). Samalla tarkistuu, että mittari ja taulukko vastaavat toisiaan.
5. Kirjaa `V_OC` ja `R_TH` `OilProcessor.h`:n `OilCal`-nimiavaruuteen. Aseta `CALIBRATED = true`, käännä ja lataa.
6. **Tarkistus:** kytke vuorotellen 47 Ω (~1 bar) ja 150 Ω (~4 bar). Kalibrointisivun bar-arvon ja mittarin näyttämän pitää olla ±0.1 bar sisällä.
7. Kytke S-johdin takaisin anturiin. Moottori sammuksissa: ~0 bar. Moottori käynnissä: arvo seuraa mittaria.
8. Aseta `WEB_UI_ENABLED = false`, käännä ja lataa tuotantoversio.

Jos WiFi ei ole käytettävissä, `handleOilRead()`:ssa on kommentoitu `Serial.printf`, joka tulostaa jännitteen sarjamonitoriin.

### Mitatut arvot

| Päivä | V_OC [V] | R_TH [Ω] | Tarkistus 47 Ω | Tarkistus 150 Ω | Huom. |
|---|---|---|---|---|---|
| — | — | — | — | — | ei vielä mitattu |

---

## Sivuhuomio: CCS-virta

Hat Labs -esimerkkifirmwaren mukaan analogiatulojen jakaja on 33.3/3.3 ≈ 10.09. Tämä selittää, miksi CCS-virta "mitattiin" 1 mA:ksi: todellinen virta on 10 mA (kuten dokumentaatio sanoo), ja se jaetaan 10:1-jakajalla, joten `R = V_adc / 1 mA` antaa oikean resistanssin. A1/A2-koodiin ei kosketa, koska tulos on oikea.
