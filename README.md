# ANENJI 11kW Modbus Emulator

A Modbus RTU slave emulator for the ANENJI 11kW hybrid inverter, running on an ESP32. It allows developers to test Modbus register reads and build monitoring integrations without connecting to a physical inverter.

## Features

- Modbus RTU slave with device address `1`.
- Serial communication at `9600 baud, 8N1`.
- Support for function `0x03` — Read Holding Registers.
- Runtime register configuration through the USB serial interface.
- HEX logging of received Modbus requests and transmitted responses.
- Preconfigured test data for battery, PV, grid, power, and energy registers.

## Hardware and Communication Settings

| Parameter | Value |
|---|---|
| Modbus UART | `HardwareSerial(2)` |
| RX pin | GPIO16 |
| TX pin | GPIO17 |
| Baud rate | 9600 |
| Frame format | 8N1 |
| Slave ID | 1 |
| Command interface | USB Serial, 115200 baud |

**Important:** GPIO16 and GPIO17 are logic-level UART pins. A suitable transceiver may be required when connecting to a physical RS-232 or RS-485 interface. Never connect RS-232/RS-485 signal lines directly to ESP32 GPIO pins.

## Serial Commands

Enter commands through the Serial Monitor at `115200 baud`. Configure the line ending as `Newline` or `Both NL & CR`.

| Command | Description |
|---|---|
| `GET <address>` | Read a register value |
| `SET <address> <value>` | Set a register value |
| `LIST` | Display all non-zero registers |
| `HELP` | Display available commands |

### Examples

Set the battery state of charge to 75%:

```text
SET 280 75
```

Response:

```text
SET 280 = 75 (0x004B)
```

Read the register:

```text
GET 280
```

Response:

```text
REG 280 = 75 (0x004B)
```

Set the battery voltage to 53.2 V. In this emulator, the value is represented in tenths of a volt:

```text
SET 277 532
```

## Modbus RTU Example

Read one holding register starting at address `277`, which contains the battery voltage.

Request:

```text
01 03 01 15 00 01 94 37
```

Field breakdown:

- `01` — Slave address.
- `03` — Read Holding Registers.
- `01 15` — Starting register address, `277`.
- `00 01` — Number of registers to read.
- `94 37` — Modbus CRC16, low byte first.

If register `277` contains `527`, the emulator returns:

```text
01 03 02 02 0F F8 7D
```

The data bytes `02 0F` represent the decimal value `527`, corresponding to `52.7 V`.

## Default Test Registers

| Register | Parameter | Default value |
|---:|---|---:|
| 198 | Status | 97 |
| 201 | Operating mode | 2 |
| 203 | Grid frequency | 50.01 Hz |
| 231 | Inverter temperature | 33 |
| 252 | Output current | 4.7 A |
| 254 | Active power | 944 W |
| 277 | Battery voltage | 52.7 V |
| 278 | Battery current | −20.3 A |
| 280 | Battery SOC | 63% |
| 338 | Grid voltage | 230 V |
| 351 | PV1 voltage | 210 V |
| 352 | PV1 current | 13.5 A |
| 353 | PV1 power | 2835 W |
| 702 | Daily energy | 12.50 kWh |
| 703–704 | Total energy, 32-bit value | 12,719,100 |

## Limitations

- Only Modbus function `0x03` is implemented.
- Register values are stored in RAM and are reset when the device restarts.
- The default values are manually configured test data, not live measurements.
- Register addresses, scaling factors, and data types reflect the current emulator implementation and should be verified against the register map of the target inverter.

## Disclaimer

This is an independent software project and is not affiliated with or endorsed by ANENJI. It is intended for development, testing, and educational purposes.