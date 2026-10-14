#include <Arduino.h>

HardwareSerial ModbusSerial(2);

#define MODBUS_RX 16
#define MODBUS_TX 17
#define SLAVE_ID 1

uint16_t regs[1000] = {0};

String inputBuffer;


// ============================================================
// Modbus RTU timing
// 9600 baud, 8N1
//
// 1 символ ≈ 1.04 ms
// 3.5 символа ≈ 3.65 ms
//
// Берём 5 ms с запасом.
// ============================================================

#define MODBUS_FRAME_GAP_MS 5


// ============================================================
// CRC16 Modbus
// ============================================================

uint16_t modbusCRC(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;

    for (uint16_t pos = 0; pos < length; pos++)
    {
        crc ^= data[pos];

        for (uint8_t i = 0; i < 8; i++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }

    return crc;
}


// ============================================================
// Print hex
// ============================================================

void printHex(const uint8_t *data, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++)
    {
        if (data[i] < 0x10)
            Serial.print("0");

        Serial.print(data[i], HEX);

        if (i < length - 1)
            Serial.print(" ");
    }

    Serial.println();
}


// ============================================================
// PC command interface
// ============================================================

void processCommand(String command)
{
    command.trim();

    if (command.length() == 0)
        return;

    Serial.print("> ");
    Serial.println(command);


    // --------------------------------------------------------
    // GET
    // --------------------------------------------------------

    if (command.startsWith("GET "))
    {
        int address = command.substring(4).toInt();

        if (address < 0 || address >= 1000)
        {
            Serial.println("ERROR: invalid address");
            return;
        }

        Serial.printf(
            "REG %d = %u (0x%04X)\n",
            address,
            regs[address],
            regs[address]
        );

        return;
    }


    // --------------------------------------------------------
    // SET
    // --------------------------------------------------------

    if (command.startsWith("SET "))
    {
        int firstSpace = command.indexOf(' ');
        int secondSpace = command.indexOf(' ', firstSpace + 1);

        if (secondSpace < 0)
        {
            Serial.println("ERROR: SET <address> <value>");
            return;
        }

        int address = command.substring(
            firstSpace + 1,
            secondSpace
        ).toInt();

        long value = command.substring(
            secondSpace + 1
        ).toInt();

        if (address < 0 || address >= 1000)
        {
            Serial.println("ERROR: invalid address");
            return;
        }

        if (value < 0 || value > 65535)
        {
            Serial.println("ERROR: value must be 0..65535");
            return;
        }

        regs[address] = (uint16_t)value;

        Serial.printf(
            "SET %d = %u (0x%04X)\n",
            address,
            regs[address],
            regs[address]
        );

        return;
    }


    // --------------------------------------------------------
    // LIST
    // --------------------------------------------------------

    if (command == "LIST")
    {
        for (int i = 0; i < 1000; i++)
        {
            if (regs[i] != 0)
            {
                Serial.printf(
                    "%d = %u (0x%04X)\n",
                    i,
                    regs[i],
                    regs[i]
                );
            }
        }

        return;
    }


    // --------------------------------------------------------
    // HELP
    // --------------------------------------------------------

    Serial.println("Commands:");
    Serial.println("GET <address>");
    Serial.println("SET <address> <value>");
    Serial.println("LIST");
}


// ============================================================
// Send Modbus response
// ============================================================

void sendModbusResponse(uint8_t *data, uint16_t length)
{
    uint16_t crc = modbusCRC(data, length);

    data[length++] = crc & 0xFF;
    data[length++] = (crc >> 8) & 0xFF;

    Serial.print("MODBUS TX: ");
    printHex(data, length);

    ModbusSerial.write(data, length);
    ModbusSerial.flush();
}


// ============================================================
// Process complete Modbus frame
// ============================================================

void processModbusFrame(uint8_t *buffer, uint16_t length)
{
    if (length < 8)
        return;


    // --------------------------------------------------------
    // Slave address
    // --------------------------------------------------------

    if (buffer[0] != SLAVE_ID)
        return;


    // --------------------------------------------------------
    // CRC
    // --------------------------------------------------------

    uint16_t receivedCRC =
        buffer[length - 2] |
        ((uint16_t)buffer[length - 1] << 8);

    uint16_t calculatedCRC =
        modbusCRC(buffer, length - 2);

    if (receivedCRC != calculatedCRC)
    {
        Serial.print("MODBUS CRC ERROR: ");
        printHex(buffer, length);

        return;
    }


    // --------------------------------------------------------
    // Valid frame
    // --------------------------------------------------------

    Serial.print("MODBUS RX: ");
    printHex(buffer, length);


    // --------------------------------------------------------
    // Function 03
    // Read Holding Registers
    // --------------------------------------------------------

    if (buffer[1] == 0x03)
    {
        uint16_t startAddress =
            ((uint16_t)buffer[2] << 8) |
            buffer[3];

        uint16_t quantity =
            ((uint16_t)buffer[4] << 8) |
            buffer[5];


        Serial.printf(
            "MODBUS READ addr=%u qty=%u\n",
            startAddress,
            quantity
        );


        // Modbus limit
        if (quantity == 0 || quantity > 125)
        {
            Serial.println("MODBUS ERROR: invalid quantity");
            return;
        }


        // Register array limit
        if ((uint32_t)startAddress + quantity > 1000)
        {
            Serial.println("MODBUS ERROR: address out of range");
            return;
        }


        uint8_t response[256];

        response[0] = SLAVE_ID;
        response[1] = 0x03;
        response[2] = quantity * 2;


        for (uint16_t i = 0; i < quantity; i++)
        {
            uint16_t value =
                regs[startAddress + i];

            response[3 + i * 2] =
                value >> 8;

            response[4 + i * 2] =
                value & 0xFF;
        }


        sendModbusResponse(
            response,
            3 + quantity * 2
        );

        return;
    }


    // --------------------------------------------------------
    // Unsupported function
    // --------------------------------------------------------

    Serial.printf(
        "MODBUS: unsupported function 0x%02X\n",
        buffer[1]
    );
}


// ============================================================
// Modbus receiver
// ============================================================

void processModbus()
{
    static uint8_t buffer[256];
    static uint16_t length = 0;

    static uint32_t lastByteTime = 0;


    // --------------------------------------------------------
    // Receive bytes
    // --------------------------------------------------------

    while (ModbusSerial.available())
    {
        uint8_t byte = ModbusSerial.read();

        if (length < sizeof(buffer))
        {
            buffer[length++] = byte;
        }
        else
        {
            // Overflow — discard current frame
            length = 0;
        }

        lastByteTime = millis();
    }


    // --------------------------------------------------------
    // Detect end of RTU frame
    //
    // Frame is complete after silence >= 5 ms.
    // --------------------------------------------------------

    if (length > 0 &&
        (millis() - lastByteTime >= MODBUS_FRAME_GAP_MS))
    {
        processModbusFrame(buffer, length);

        length = 0;
    }
}


// ============================================================
// Registers
// ============================================================

void initRegisters()
{
    // Status
    regs[198] = 97;

    // Operation mode = Mains
    regs[201] = 2;

    // Grid frequency = 50.01 Hz
    regs[203] = 5001;


    // Inverter temperature
    regs[231] = 33;


    // Output current = 4.7 A
    regs[252] = 47;

    // Output frequency = 50.01 Hz
    regs[253] = 5001;

    // Active power
    regs[254] = 944;

    // Apparent power
    regs[255] = 1081;

    // Load %
    regs[256] = 9;


    // Battery voltage = 52.7 V
    regs[277] = 527;

    // Battery current = -20.3 A
    regs[278] = (uint16_t)-203;

    // Battery power = -1069 W
    regs[279] = (uint16_t)-1069;

    // SOC
    regs[280] = 63;

    // DCDC temperature
    regs[281] = 33;


    // Grid voltage = 230.0 V
    regs[338] = 2300;


    // Load power
    regs[349] = 944;


    // --------------------------------------------------------
    // PV1
    // --------------------------------------------------------

    // Voltage = 210.0 V
    regs[351] = 2100;

    // Current = 13.5 A
    regs[352] = 135;

    // Power = 2835 W
    regs[353] = 2835;


    // --------------------------------------------------------
    // PV2
    // --------------------------------------------------------

    regs[389] = 0;
    regs[390] = 0;
    regs[391] = 0;


    // --------------------------------------------------------
    // Energy
    // --------------------------------------------------------

    // Daily energy = 12.50 kWh
    regs[702] = 1250;


    // Total energy = 12,719,100
    //
    // 32-bit U_DWORD:
    //
    // register 703 = high word
    // register 704 = low word
    //

    uint32_t totalEnergy = 12719100UL;

    regs[703] =
        (uint16_t)(totalEnergy >> 16);

    regs[704] =
        (uint16_t)(totalEnergy & 0xFFFF);
}


// ============================================================
// Setup
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("==============================");
    Serial.println("ANENJI Modbus Slave Emulator");
    Serial.println("==============================");

    initRegisters();


    // --------------------------------------------------------
    // Modbus UART
    // --------------------------------------------------------

    ModbusSerial.begin(
        9600,
        SERIAL_8N1,
        MODBUS_RX,
        MODBUS_TX
    );


    Serial.println("PC UART:     115200");
    Serial.println("Modbus UART: 9600 8N1");
    Serial.println("Slave ID:    1");
    Serial.println("Frame gap:   5 ms");
    Serial.println();
    Serial.println("Ready.");
    Serial.println();
}


// ============================================================
// Main loop
// ============================================================

void loop()
{
    // --------------------------------------------------------
    // PC commands
    // --------------------------------------------------------

    while (Serial.available())
    {
        char c = Serial.read();

        if (c == '\r' || c == '\n')
        {
            if (inputBuffer.length() > 0)
            {
                processCommand(inputBuffer);
                inputBuffer = "";
            }
        }
        else
        {
            if (inputBuffer.length() < 100)
                inputBuffer += c;
        }
    }


    // --------------------------------------------------------
    // Modbus
    // --------------------------------------------------------

    processModbus();
}