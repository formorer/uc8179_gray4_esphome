# 4 Graustufen auf dem reTerminal E1001 — Anleitung

Diese Anleitung richtet sich an Nutzer eines **Seeed reTerminal E1001**, die
das 7.5"-ePaper statt in Schwarz/Weiß mit **4 Graustufen** (Weiß, Hellgrau,
Dunkelgrau, Schwarz) betreiben wollen. Grundlage ist die ESPHome-Komponente
`uc8179_gray4` aus diesem Repository.

## Voraussetzungen

- reTerminal E1001 + USB-C-Kabel (fürs erste Flashen)
- [ESPHome](https://esphome.io/guides/installing_esphome) ≥ 2025.x auf dem
  Rechner (`pip install esphome` oder Homebrew)
- Grundkenntnisse ESPHome (YAML, `esphome run`)

## Schritt 1: Komponente einbinden

Im eigenen YAML reicht (kein Klonen nötig — ESPHome lädt die Komponente beim
Kompilieren selbst von GitHub):

```yaml
external_components:
  - source: github://formorer/uc8179_gray4_esphome
    components: [ uc8179_gray4 ]
```

Alternativ das Repo klonen und den Ordner `components/` neben die eigene
YAML-Datei legen, dann:

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [ uc8179_gray4 ]
```

## Schritt 2: Display-Block ersetzen

In der bestehenden E1001-Konfiguration (z.B. aus dem
[Seeed-Wiki](https://wiki.seeedstudio.com/reterminal_e10xx_with_esphome/))
den `display:`-Block mit `platform: waveshare_epaper` / `model: 7.50inv2`
durch diesen ersetzen — der `spi:`-Block (clk GPIO7, mosi GPIO9) bleibt gleich:

```yaml
display:
  - platform: uc8179_gray4
    id: eink_display
    cs_pin: GPIO10
    dc_pin: GPIO11
    reset_pin: GPIO12
    busy_pin:
      number: GPIO13
      mode:
        input: true
        pullup: true
    lut_mode: otp        # bei komischen Graustufen: "custom" probieren
    update_interval: never
    lambda: |-
      it.filled_rectangle(0,   0, 800, 120, Color(0, 0, 0));
      it.filled_rectangle(0, 120, 800, 120, Color(85, 85, 85));
      it.filled_rectangle(0, 240, 800, 120, Color(170, 170, 170));
      it.filled_rectangle(0, 360, 800, 120, Color(255, 255, 255));
```

**Achtung, häufigster Fehler:** In der Seeed-Vorlage ist der Busy-Pin
`inverted: true`. Bei `uc8179_gray4` **kein** `inverted: true` setzen — der
Treiber wertet das Active-low-Signal selbst aus. Wer das Flag drin lässt,
bekommt Timeouts ("Timeout while waiting for the display to become idle").

Ein vollständiges, lauffähiges Minimalbeispiel liegt in
[example-e1001.yaml](example-e1001.yaml).

## Schritt 3: Flashen & testen

```sh
esphome run example-e1001.yaml   # erstes Mal per USB, danach geht OTA
```

Der Streifentest (Lambda oben) muss 4 klar unterscheidbare Flächen zeigen:
Schwarz → Dunkelgrau → Hellgrau → Weiß. Der Graustufen-Refresh ist ein
Full-Refresh: Er dauert einige Sekunden und blitzt dabei — das ist normal.

Sehen die Stufen falsch, invertiert oder verwaschen aus: `lut_mode` auf den
jeweils anderen Wert stellen (`otp` ↔ `custom`). Es gibt zwei Panel-Chargen
mit unterschiedlichen Waveforms; welcher Modus passt, sieht man am Ergebnis.

## Schritt 4: Eigene Bilder

`Color(r,g,b)` in Lambdas wird per Helligkeit auf die 4 Stufen gerundet
(Schwellen 64/128/192). Fotos vorher passend dithern:

```sh
python3 tools/dither4.py foto.jpg images/foto.png
```

(braucht Pillow: `pip install pillow`; Hochformat-Quellen mit `--rotate`)

Dann im YAML einbinden und zeichnen:

```yaml
image:
  - file: images/foto.png
    id: mein_foto
    type: GRAYSCALE
```

```yaml
    lambda: |-
      it.image(0, 0, id(mein_foto));
```

## Grenzen (v1)

- **Nur Full-Refresh** im Graustufen-Modus — kein Partial-Update, jede
  Aktualisierung blitzt und dauert ein paar Sekunden. Für batteriebetriebene
  Dashboards mit seltenen Updates (Deep Sleep) ideal, für "lebende" Anzeigen
  nicht.
- Framebuffer braucht 96 kB RAM (auf dem ESP32-S3 des E1001 unkritisch).
- Auto-Clear von ESPHome füllt Schwarz; Lambdas, die nicht die ganze Fläche
  zeichnen, sollten mit `it.fill(Color(255,255,255));` beginnen.
- Getestet auf dem baugleichen XIAO ePaper Driver Board (gleicher
  UC8179-Controller, gleiches 800x480-Panel); auf dem E1001 selbst sollte es
  identisch laufen — Rückmeldung willkommen.
