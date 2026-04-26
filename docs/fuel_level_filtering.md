# Polttoainetason suodatuslogiikka — HALMET ESP32 SignalK Gateway

## Taustaa

Fridan polttoainetankki: **400 L**, poikkileikkausala ~1 m².

Marssinopeus: **7 solmua**, kulutus **~1 L/meripeninkulmaa** → 7 L/h → pinnan alenema **7 mm/h**.

Yksittäinen litra vastaa 1 mm pinnanalenemaa. VDO-anturin mittaustarkkuus on ±2–3 % koko alueesta, mikä vastaa useita senttimetrejä — eli yksittäinen litra on täysin mittauskohinan sisällä.

Lainehdinnan aiheuttama pinnanvaihtelu purjeveneellä on helposti ±50–100 mm muutamassa sekunnissa.
**Signaali/kohina-suhde yksittäisessä näytteessä on luokkaa 1:50 000.**

Tämän vuoksi suodatus voi ja sen täytyy olla erittäin hidas ja voimakkaasti tasoittava.

---

## Suodatusparametrit

| Parametri | Arvo | Perustelu |
|---|---|---|
| Näytteenottoväli | 2 s | Riittää, muutos on 0,002 mm/s |
| Mediaani-ikkuna | 120 näytettä | 4 min — kattaa lainehdinnan täysin |
| EMA alpha | 0.005 | Aikavakio τ ≈ 400 s ≈ 6,7 min |
| EMA 3×τ reagointiaika | ~20 min | Pinnanmuutos 20 min ajossa: ~2,3 mm — ok |

**Muistinkulutus:** 120 × 4 bytes = 480 tavua (triviaali ESP32:lla).

---

## Käynnistymislogiikka

Järjestelmä toimii kolmessa vaiheessa käynnistyksen jälkeen:

**Vaihe 1 (0–4 min):** Mediaani-ikkuna täyttyy. Lähetetään raaka ohm-arvo suoraan SignalK:lle — data alkaa virrata välittömästi käynnistyksestä. Kohinaa näkyy jonkin verran.

**Vaihe 2 (4 min):** Mediaani-ikkuna täynnä ensimmäistä kertaa. EMA alustetaan ensimmäisellä mediaaniarvolla — ei nollasta, jotta ei synny pitkää lämpenemisaikaa.

**Vaihe 3 (4 min →):** Normaali suodatettu toiminta: mediaani → EMA → SignalK. Käyrä tasoittuu pehmeästi ilman hyppäystä.

---

## Toteutus (VDOProcessor)

```cpp
// VDOProcessor.h
static constexpr int   MEDIAN_WINDOW = 120;
static constexpr float EMA_ALPHA     = 0.005f;

float _samples[MEDIAN_WINDOW];  // pyörivä puskuri
int   _sample_idx   = 0;
int   _sample_count = 0;
float _ema          = 0.0f;
bool  _ema_initialized = false;

float computeMedian();  // kopioi + lajittelee, ei muokkaa _samples-taulukkoa
```

```cpp
// VDOProcessor.cpp
void VDOProcessor::updateLevel(float ohms) {
  // Tallenna näyte pyörivään puskuriin
  _samples[_sample_idx] = ohms;
  _sample_idx = (_sample_idx + 1) % MEDIAN_WINDOW;
  if (_sample_count < MEDIAN_WINDOW) _sample_count++;

  if (_sample_count < MEDIAN_WINDOW) {
    // Vaihe 1: ikkuna ei täynnä — lähetetään raaka arvo
    _delta.fuel_level_ratio = fillRatio(ohms);
    return;
  }

  float median = computeMedian();

  // Vaihe 2: EMA-alustus ensimmäisellä mediaaniarvolla
  if (!_ema_initialized) {
    _ema = median;
    _ema_initialized = true;
  }

  // Vaihe 3: normaali suodatus
  _ema = EMA_ALPHA * median + (1.0f - EMA_ALPHA) * _ema;
  _delta.fuel_level_ratio = fillRatio(_ema);
}
```

### fillRatio — vastus → täyttösuhde

```cpp
// VDO European sender: 10 Ω = täysi, 180 Ω = tyhjä
static constexpr float VDO_OHMS_FULL  = 10.0f;
static constexpr float VDO_OHMS_EMPTY = 180.0f;

float VDOProcessor::fillRatio(float ohms) {
  float ratio = 1.0f - (ohms - VDO_OHMS_FULL) / (VDO_OHMS_EMPTY - VDO_OHMS_FULL);
  if (ratio < 0.0f) ratio = 0.0f;
  if (ratio > 1.0f) ratio = 1.0f;
  return ratio;
}
```

### computeMedian

```cpp
float VDOProcessor::computeMedian() {
  float buf[MEDIAN_WINDOW];
  memcpy(buf, _samples, sizeof(buf));
  std::sort(buf, buf + MEDIAN_WINDOW);
  return buf[MEDIAN_WINDOW / 2];
}
```

---

## SignalK-polut

| Polku | Arvo | Yksikkö |
|---|---|---|
| `tanks.fuel.0.currentLevel` | fillRatio (0.0–1.0) | dimensioton |
| `tanks.fuel.0.capacity` | 0.4 | m³ |

Kapasiteetti lähetetään staattisena arvona SignalK-yhteyden muodostumisen yhteydessä, ei joka delta-syklissä.
