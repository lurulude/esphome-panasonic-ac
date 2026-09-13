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
| 14 | defrost (`0x02` documented/observed elsewhere as defrost) |
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

The strongest current model is that b12 describes the **physical operating family**, largely independently of selected mode b2.

This became clear in AUTO/HEAT_COOL captures: with `b2 = 0x04`, the unit has directly reported HEAT states (`0x40/0x44/0x4C`) and COOL states (`0x38/0x3C`). AUTO therefore selects among the same physical state families rather than using a separate dedicated heat/cool family.

### Observed on this CZ25

| Physical family | b12 | Meaning |
| --- | --- | --- |
| AUTO/neutral | `0x00` | AUTO idle / neutral wait state |
| DRY | `0x20` | DRY idle |
| DRY | `0x28` | DRY start |
| DRY | `0x2C` | DRY run |
| COOL | `0x30` | COOL idle |
| COOL | `0x38` | COOL start |
| COOL | `0x3C` | COOL run |
| HEAT | `0x40` | HEAT idle |
| HEAT | `0x44` | HEAT transition |
| HEAT | `0x48` | HEAT start |
| HEAT | `0x4C` | HEAT run |
| FAN | `0x60` | fan-only state |

`0x44 = HEAT_TRANS` was captured during a real AUTO heating shutdown sequence:

`HEAT_RUN (0x4C) -> HEAT_TRANS (0x44) -> HEAT_IDLE (0x40)`

The COOL startup sequence in AUTO was captured as:

`AUTO_IDLE (0x00) -> COOL_START (0x38) -> COOL_RUN (0x3C)`

The 5-second polling interval did not capture `0x34` in that startup, so COOL_TRANS remains predicted rather than confirmed on this unit.

### Observed 0x0x AUTO-family values with unresolved meaning

Earlier captures have shown `0x0C` while selected mode was AUTO. Newer captures prove that ordinary AUTO cooling uses the standard COOL family (`0x38/0x3C`), so `0x0C` must **not** be called AUTO_COOL_RUN.

Current conservative labels are:

| b12 | Label | Status |
| --- | --- | --- |
| `0x04` | AUTO_TRANS_0x04 | predicted from low-nibble pattern, not captured |
| `0x08` | AUTO_START_0x08 | predicted from low-nibble pattern, not captured |
| `0x0C` | AUTO_RUN_0x0C | observed, physical heat/cool direction unresolved |

### Predicted from the state pattern, not yet captured on this unit

| Family | b12 | Provisional meaning |
| --- | --- | --- |
| DRY | `0x24` | transition |
| COOL | `0x34` | transition |

The broad low-nibble pattern is now strongly supported:

- `x0`: idle/base
- `x4`: transition
- `x8`: start
- `xC`: run

This pattern is directly supported by HEAT (`40/44/48/4C`) and by COOL (`38/3C`, with `30` observed separately). DRY has `20/28/2C` observed, with `24` still missing.

## AUTO / HEAT_COOL behavior

AUTO should be thought of as a **selector of physical families**, not a separate physical operating family.

A captured sequence with current temperature around 24 C and target reduced from 23.5 C downward showed:

1. `b2 = 0x04` AUTO while `b12 = 0x4C` HEAT_RUN.
2. Lowering the target produced `0x44` HEAT_TRANS.
3. Next poll produced `0x40` HEAT_IDLE.
4. After further target reduction the unit moved to `0x00` AUTO_IDLE/neutral.
5. It remained in `0x00` for multiple polls.
6. With target 16.5 C and room around 22 C, it entered `0x38` COOL_START.
7. It then entered `0x3C` COOL_RUN while `b2` remained `0x04` AUTO.

This is the clearest evidence so far that b12 is the authoritative physical-action field.

## Important transition behavior

Selected mode b2 changes before the physical state machine b12 catches up.

Examples captured on the unit:

- HEAT -> DRY: b2 changes to `0x24` immediately while b12 remains `0x4C` HEAT_RUN for the immediate response; roughly one poll later b12 becomes `0x20` DRY_IDLE.
- DRY -> COOL: b2 changes to `0x34` immediately while b12 remains `0x20` DRY_IDLE; roughly one poll later b12 becomes `0x30` COOL_IDLE.
- AUTO target reduction while physically heating: b2 remains `0x04`, while b12 progresses through `0x4C -> 0x44 -> 0x40 -> 0x00 -> 0x38 -> 0x3C` as the unit stops heating, waits, and starts cooling.

For this reason operational-state labels and action should be derived primarily from b12, without relabelling b12 merely because the selected mode in b2 has changed.

## Climate action

The upstream component derives `climate.action` from selected mode and current/target temperatures with a 2 C tolerance. Captures show that this can disagree with the actual physical state.

The CZ25 branch now derives action from b12 when the physical family is known:

- HEAT start/run (`0x48/0x4C`) -> HEATING
- COOL start/run (`0x38/0x3C`) -> COOLING
- DRY start/run (`0x28/0x2C`) -> DRYING
- idle (`x0`) and transition (`x4`) states -> IDLE
- FAN `0x60` -> FAN
- AUTO neutral `0x00` -> IDLE

Unknown 0x0x AUTO states fall back to the upstream behavior because their physical heat/cool direction has not yet been established.

## Compressor running

The binary compressor-running estimate is derived from the low-nibble phase pattern:

- start (`x8`) -> running
- run (`xC`) -> running
- idle (`x0`) -> not running
- transition (`x4`) -> not running

Observed start/run values include:

- `0x28`, `0x2C`
- `0x38`, `0x3C`
- `0x48`, `0x4C`

`0x0C` is also treated as compressor-running because it was observed as an active AUTO-family state, but its heat/cool direction remains unresolved.

## Temperature fields

### b18

b18 is the primary temperature used by the existing component as current temperature when supported. On this CZ25 it behaves consistently with Panasonic's indoor/intake/control temperature, but should not be assumed to equal a room-center reference thermometer.

### b21

b21 is a separate temperature field. On this unit it often differs from b18 by 0-1 C. The data collected so far does not justify naming it as the indoor coil/pipe temperature, so it remains deliberately neutral as `temperature_b21`.

### b13

b13 is encoded in 0.5 C increments (`b13 / 2`). It follows target-temperature changes with delay and behaves like an internal control/reference value rather than a physical sensor.

Examples observed include:

- target 24.0 C -> reference 25.0 C
- target 23.0 C -> reference initially remains 25.0 C
- target 21.5 C -> reference 24.0 C
- target 20.5 C -> reference 22.5 C then 21.5 C
- target 18.5 C -> reference eventually 19.5 C

During AUTO cooling startup, the reference also moved independently of b18/b21, reinforcing that it is a control target rather than a sensor.

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
