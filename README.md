# ANENJI 11kW Hybrid Inverter Modbus Emulator

A Modbus emulator for the ANENJI 11kW hybrid inverter, designed to simulate inverter telemetry and operating data for testing, development, and Home Assistant integration.

The project allows developers to emulate Modbus responses without requiring a physical inverter, making it possible to test data acquisition, sensor configuration, and monitoring integrations using simulated inverter data.

## Features

- Emulates Modbus communication with an ANENJI 11kW hybrid inverter.
- Provides simulated inverter telemetry for testing and development.
- Supports testing Modbus register parsing and data mapping.
- Helps develop and troubleshoot Home Assistant integrations without direct access to the inverter hardware.
- Can be extended with historical datasets to reproduce inverter readings over a selected time period.

## Use Cases

- Developing and testing inverter monitoring integrations.
- Validating Modbus register mappings and data conversion.
- Testing ESPHome-based data acquisition.
- Replaying recorded inverter data for Home Assistant history and dashboard testing.
- Debugging communication and sensor-processing logic in a controlled environment.

## Disclaimer

This is an independent software project and is not affiliated with or endorsed by ANENJI. The accuracy of simulated values depends on the implemented register mappings and the data supplied to the emulator.
