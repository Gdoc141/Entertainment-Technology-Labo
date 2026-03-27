# Entertainment-Technology-Labo
Labo Opdrachten Entertainment Technology. Student Hogeschool Vives Brugge
---

## Opdracht 1: USB MIDI Keyboard met MCP23S17 & 4x4 Matrix

### Projectbeschrijving
Implementatie van een **USB MIDI keyboard** met 16 toetsen op een STM32H533 (Nucleo bord).
- **Hardware**: MCP23S17 SPI I/O expander + 4×4 keypad matrix
- **Communicatie**: USB MIDI Device over TinyUSB
- **Doel**: Toetsen indrukken → MIDI Note On/Off signalen sturen naar PC

### Hardware Setup

#### Gebruikte Componenten
- **Microcontroller**: STM32H533RETx (Nucleo H533RE)
- **I/O Expander**: MCP23S17 (SPI chip, 16 GPIO pinnnen)
- **Keypad**: 4×4 matrix (16 toetsen)

#### Pinnen Connecties

**MCP23S17 → STM32H533 (SPI)**
| MCP Pin | Functie | STM32 Pin | Naam    |
|---------|---------|-----------|---------|
| 9       | VDD     | 3V3       | 3V3     |
| 10      | VSS     | GND       | GND     |
| 12      | SCK     | D13       | SCK     |
| 13      | SI      | D11       | MOSI    |
| 14      | SO      | D12       | MISO    |
| 11      | CS      | D10       | SS      |
| 15      | A0      | GND       | Adres   |
| 16      | A1      | GND       | Adres   |
| 17      | A2      | GND       | Adres   |

**MCP23S17 GPIO → Keypad Matrix**
```
Kolommen (Inputs):
  GPA0 → C1 (Kolom 1)
  GPA1 → C2 (Kolom 2)
  GPA2 → C3 (Kolom 3)
  GPA3 → C4 (Kolom 4)

Rijen (Outputs):
  GPB0 → R1 (Rij 1)
  GPB1 → R2 (Rij 2)
  GPB2 → R3 (Rij 3)
  GPB3 → R4 (Rij 4)
```

### MIDI Noten Mapping

**Toets Layout (4×4 matrix)**
```
┌─────┬─────┬─────┬─────┐
│ C4  │ D4  │ E4  │ F4  │  Rij 1
│ (60)│ (62)│ (64)│ (65)│
├─────┼─────┼─────┼─────┤
│ G4  │ A4  │ B4  │ C5  │  Rij 2
│ (67)│ (69)│ (71)│ (72)│
├─────┼─────┼─────┼─────┤
│ D5  │ E5  │ F5  │ G5  │  Rij 3
│ (74)│ (76)│ (77)│ (79)│
├─────┼─────┼─────┼─────┤
│ A5  │ B5  │ C6  │ D6  │  Rij 4
│ (81)│ (83)│ (84)│ (86)│
└─────┴─────┴─────┴─────┘
```
(MIDI nummers tussen haakjes)

### Implementatie Details

#### SPI Communicatie
- **Baudrate**: ~3 MHz (SPI_BAUDRATEPRESCALER_16)
- **Mode**: SPI Master, Mode 0 (CPOL=0, CPHA=0)
- **Datasize**: 8-bit
- **CS**: Software controlled (PA10)

#### Matrix Scanning Algoritme
1. **Rij-voor-rij scanning**:
   - Drive rij 0 laag, lees kolommen → `keypad_state[0]`
   - Drive rij 1 laag, lees kolommen → `keypad_state[1]`
   - Drive rij 2 laag, lees kolommen → `keypad_state[2]`
   - Drive rij 3 laag, lees kolommen → `keypad_state[3]`
   - Alle rijen terug hoog

2. **Debouncing**: 20ms scan interval
3. **Detectie**: Vergelijk vorige state met huidige state
   - Bit 0→1: Toets losgelaten → Note Off
   - Bit 1→0: Toets ingedrukt → Note On

#### MIDI USB Protocol
- **Cable Number**: 0
- **Channel**: 0 (Channel 1)
- **Velocity**: 127 (maximum) bij Note On
- **Status Bytes**:
  - Note On: `0x90 | channel`
  - Note Off: `0x80 | channel`

### Software Structuur

**Initializatie sequence:**
```
main()
  ├─ HAL_Init()
  ├─ SystemClock_Config()
  ├─ MX_GPIO_Init()          // GPIO + CS instellingen
  ├─ MX_SPI1_Init()          // SPI1 configuratie
  ├─ MX_USB_Init()           // USB Device stack
  ├─ mcp23s17_init()         // MCP I/O expander setup
  └─ tusb_init()             // TinyUSB stack
```

**Main loop:**
```
while(1) {
  tud_task()      → USB device task
  led_blinking_task()  → LED feedback
  keypad_task()        → Toetsenbord scanning + MIDI
  midi_task()          → USB MIDI traffic handling
}
```

### Functie Overzicht

| Functie | Beschrijving |
|---------|-------------|
| `mcp23s17_init()` | Initialiseert MCP23S17 (GPIOA inputs, GPIOB outputs) |
| `mcp23s17_read_reg(reg)` | Leest 8-bit register via SPI |
| `mcp23s17_write_reg(reg, val)` | Schrijft 8-bit register via SPI |
| `keypad_scan()` | Scant matrix (drive rij, lees kolommen) |
| `keypad_task()` | Detecteert indrukken/loslaten, stuurt MIDI |
| `midi_task()` | Verwerkt inkomende USB MIDI traffic |

### Testen

**Vereisten:**
- STM32H533RE Nucleo bord
- MCP23S17 chip + breadboard
- 4×4 keypad matrix
- Micro USB kabel
- PC met MIDI software (MIDI-OX, Ableton, etc.)

**Stappen:**
1. Sluit hardware aan volgens pinnen schema
2. Compile en flash project naar STM32
3. Plug USB in PC
4. MIDI-toets verschijnt in MIDI software
5. Druk toetsen in → MIDI noten verschijnen

### Opmerkingen

- **Pull-up weerstanden**: Kolommen hebben interne pull-ups (MCP23S17 GPPUA register)
- **Scan timing**: 2ms delay per rij voor stabiele readings
- **Debouncing**: Vereist dat toets state 20ms stabiel is (scan interval)
- **Stroomverbruik**: MCP23S17 operates op 3V3 (from STM32 regulator)

---

## Opdracht 3: Huidige Status (Matrix + Meerdere Potentiometers)

### Doel van de huidige firmware
- USB MIDI via TinyUSB
- 4x4 matrix via MCP23S17 (SPI bit-bang)
- 2 potentiometers via ADC + DMA + TIM6 trigger

### Wat momenteel in de code zit
- Matrix scanning en MIDI Note On/Off zijn terug geïntegreerd.
- Potentiometerverwerking draait parallel met matrixverwerking.
- TinyUSB interrupt en USB init staan actief.
- LED2 wordt niet gebruikt voor blinkfeedback omdat PA5 gedeeld wordt met matrix-SCK.

### Pinout overzicht voor deze gecombineerde setup

#### Matrix / MCP23S17 (SPI bit-bang)
- PA5 -> SCK
- PA6 <- MISO (SO van MCP23S17)
- PA7 -> MOSI (SI van MCP23S17)
- PA10 -> CS

#### Potentiometers
- Pot 1 wiper -> PA0 (ADC1_IN0)
- Pot 2 wiper -> PA1 (ADC1_IN1)
- Buitenste pinnen van beide potmeters -> 3V3 en GND

### MIDI gedrag
- Matrix toetsen sturen MIDI notes volgens note map (Note On/Off).
- Potmeters sturen Control Change:
  - Pot 1 -> CC16
  - Pot 2 -> CC17

### Belangrijke CubeMX aandachtspunten
- ADC1 scan mode aan, aantal conversies = 2.
- Rank 1 = Channel 0, Rank 2 = Channel 1.
- DMA destination increment moet aan staan.
- USB moet als Device geconfigureerd zijn.
- Let op pinconflicten: PA5/PA6/PA7/PA10 reserveren voor matrix-SPI.

### Testchecklist (thuis op eigen bord)
- Controleer eerst USB enumeratie in Device Manager en MIDI software.
- Test matrix:
  - Druk meerdere toetsen
  - Controleer Note On bij indrukken, Note Off bij loslaten
- Test potmeters:
  - Draai Pot 1 en check CC16
  - Draai Pot 2 en check CC17
- Controleer of matrix en potmeters tegelijk stabiel blijven werken.

### Troubleshooting kort
- Alleen potmeters werken, matrix niet:
  - Meestal SPI/pin issue (PA5/PA6/PA7/PA10) of MCP23S17 voeding/GND.
- USB device niet zichtbaar:
  - Controleer USB interrupt handler, TinyUSB include paths en middleware paden.
- Na CubeMX regenerate werkt iets niet meer:
  - Controleer of custom code nog in USER CODE blokken staat.