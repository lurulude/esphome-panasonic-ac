# Panasonic CZ25 CN-CNT protocol notes

These notes describe observations from a Panasonic CZ25-series unit connected through CN-CNT and ESPHome 2026.8.2.

They are intentionally conservative: values are marked as observed only when captured from the tested unit. Some state names are still provisional and may be model-dependent.

## Packet layout used here

The normal poll response is 35 bytes including header and checksum.

Relevant full-packet byte indices:

| Byte | Current interpretation |
| --- | --- |
| 2 | selected mode + power state |
| 3 | target temperature x2 |
| 12 | physical/internal operational state |
| 13 | internal control/reference temperature x2 |
| 14 | defrost (`0x02` observed/documented as defrost) |
| 18 | primary indoor/current temperature; likely intake/return-air/control temperature |
| 19 | outside temperature |
| 21 | secondary indoor temperature field; exact physical meaning still unknown on CZ25 |
| 22 | alternate outside temperature |
| 28-29 | little-endian raw outdoor/power-related value |
| 30 | current-like value; `b30 / 5` tracks current strongly |
| 31-33 | rotating/multiplexed status fields; exact meaning unknown |

## Selected mode byte (`b2`)

Observed selected-mode values with the unit on:

| b2 | Selected mode |
| --- | --- |
| `0x04` | AUTO / HEAT_COOL |
| `0x24` | DRY |
| `0x34` | COOL |
| `0x44` | HEAT |
| `0x64` | FAN_ONLY |

The selected mode changes immediately after a command. Byte 12 can continue to report the previous physical state for several seconds, so b2 and b12 must not be treated as the same thing.

## Operational state byte (`b12`)

### Observed on this CZ25

| Mode/context | b12 | Meaning |
| --- | --- | --- |
| AUTO | `0x00` | AUTO idle/base |
| AUTO | `0x0C` | AUTO cooling-side run |
| AUTO | `0x20` | AUTO heating-side idle |
| AUTO | `0x2C` | AUTO heating-side run |
| DRY | `0x20` | DRY idle |
| DRY | `0x28` | DRY start |
| DRY | `0x2C` | DRY run |
| COOL | `0x30` | COOL idle |
| COOL | `0x38` | COOL start |
| COOL | `0x3C` | COOL run |
| HEAT | `0x40` | HEAT idle |
| HEAT | `0x48` | HEAT start |
| HEAT | `0x4C` | HEAT run |
| FAN_ONLY | `0x60` | fan-only state |

### Predicted from the state pattern, not yet captured on this unit

| Context | b12 | Provisional meaning |
| --- | --- | --- |
| AUTO cooling side | `0x04` | transition |
| AUTO cooling side | `0x08` | start |
| AUTO heating side | `0x24` | transition |
| AUTO heating side | `0x28` | start |
| DRY | `0x24` | transition |
| COOL | `0x34` | transition |
| HEAT | `0x44` | transition |

The broad pattern appears to use low-nibble state values:

- `x0`: idle/base
- `x4`: transition
- `x8`: start
- `xC`: run

This pattern is strongly supported for COOL and HEAT and partly supported for DRY/AUTO, but not every combination has been captured.

## Important transition behavior

Selected mode changes before the physical state machine catches up.

Examples captured on the unit:

- HEAT -> DRY: b2 changes to `0x24` immediately while b12 remains `0x4C` (HEAT_RUN) for the immediate response; roughly one poll later b12 becomes `0x20` (DRY_IDLE).
- DRY -> COOL: b2 changes to `0x34` immediately while b12 remains `0x20`; roughly one poll later b12 becomes `0x30` (COOL_IDLE).

For this reason operational-state labels should be based primarily on b12, with mode context used only where the same b12 family is demonstrably mode-dependent (notably AUTO vs DRY for the 0x2x family).

## Climate action

The upstream component derives `climate.action` from selected mode and current/target temperatures with a 2 C tolerance. Captures show that this can disagree with the actual physical state.

The CZ25 branch therefore derives action from b12 when the state is known:

- HEAT start/run -> HEATING
- COOL start/run -> COOLING
- DRY start/run -> DRYING
- corresponding idle states -> IDLE
- FAN `0x60` -> FAN
- AUTO cooling-side active states -> COOLING
- AUTO heating-side active states -> HEATING

Unknown states fall back to the upstream behavior.

## Compressor running

Currently treated as running for observed start/run states:

- `0x0C`
- `0x28`
- `0x2C`
- `0x38`
- `0x3C`
- `0x48`
- `0x4C`

Idle/base states are treated as not running. Transition values (`x4`) are not yet confirmed on this unit.

## Temperature fields

### b18

b18 is the primary temperature used by the existing component as current temperature when supported. On this CZ25 it behaves consistently with Panasonic's indoor/intake/control temperature, but should not be assumed to equal a room-center reference thermometer.

### b21

b21 is a separate temperature field. On this unit it often differs from b18 by 0-1 C. The data collected so far does not justify naming it as the indoor coil/pipe temperature, so it remains deliberately neutral as `temperature_b21`.

### b13

b13 is encoded in 0.5 C increments (`b13 / 2`). It follows target-temperature changes with delay and behaves like an internal control/reference value rather than a physical sensor.

Examples observed in HEAT:

- target 24.0 C -> reference 25.0 C
- target 23.0 C -> reference initially remains 25.0 C
- target 21.5 C -> reference 24.0 C
- target 20.5 C -> reference 22.5 C then 21.5 C
- target 18.5 C -> reference eventually 19.5 C

## Power/current fields

The branch exposes both the upstream value and the raw fields for comparison.

- raw outdoor/power field: `b28 + 256*b29`
- current-like field: `b30 / 5 A`
- upstream legacy power calculation: `(b28 + 256*b29) - b30`

On the tested unit b30/5 tracks load very strongly, but the exact electrical meaning should remain provisional until calibrated against an external power/current meter.

## Multiplex bytes 31-33

The tested unit repeatedly cycles combinations such as:

- `80:68:70`
- `C0:00:00`
- `C1:32:13`

The exact meaning is unknown. The CZ25 branch exposes the triplet as `status_multiplex` to make further capture analysis easier.

## Debug entities

The current CZ25 branch can expose:

- decoded operational state
- raw b12
- raw selected mode b2
- compressor-running binary state
- b18 intake/current temperature
- b21 secondary temperature
- b13/2 control reference
- b28/b29 raw outdoor power value
- b30/5 current-like value
- b31:b32:b33 multiplex triplet

These entities are intended to make further reverse engineering possible without repeatedly changing the parser.
