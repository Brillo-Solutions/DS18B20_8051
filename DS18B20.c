#define DEV_ADDR_LCD 0x70 // I2C address of PCF8574A extender

sbit sclPin at P3.B0; // I2C serial clock line
sbit sdaPin at P3.B1; // I2C serial data line
sbit DQ at P3.B2; // DS18B20 thermometer data line

char backLight;
char codeROM[8], mArr[9];

void startI2C() // I2C start condition
{
  sdaPin = 1;
  sclPin = 1;
  sdaPin = 0;
  sclPin = 0;
}

void stopI2C() // I2C stop condition
{
  sclPin = 0;
  sdaPin = 0;
  sclPin = 1;
  sdaPin = 1;
}

void writeToSlave(char mByte) // Writing byte to I2C slave
{
 char k;
 for(k = 0; k <= 8; k++)
 {
   sdaPin = mByte & 0x80 ? 1 : 0;
   sclPin = 1;
   sclPin = 0;
   mByte <<= 1;
 }
}

void writeToI2C(char codeROM, char mByte)
{
  startI2C();
  writeToSlave(codeROM);
  writeToSlave(mByte);
  stopI2C();
}

void sendToDisplay(char mByte)
{
  writeToI2C(DEV_ADDR_LCD, mByte);
}

void brkInstByte(char mByte)
{
  char nByte;
  nByte = mByte & 0xF0;
  sendToDisplay(nByte | (backLight | 0x04));
  delay_ms(1);
  sendToDisplay(nByte | (backLight | 0x00));
  nByte = ((mByte << 4) & 0xF0);
  sendToDisplay(nByte | (backLight | 0x04));
  delay_ms(1);
  sendToDisplay(nByte | (backLight | 0x00));
}

void brkDataByte(char mByte)
{
  char nByte;
  nByte = mByte & 0xF0;
  sendToDisplay(nByte | (backLight | 0x05));
  delay_ms(1);
  sendToDisplay(nByte | (backLight | 0x01));
  nByte = ((mByte << 4) & 0xF0);
  sendToDisplay(nByte | (backLight | 0x05));
  delay_ms(1);
  sendToDisplay(nByte | (backLight | 0x01));
}

void putChar(char mByte, char rn, char cp)
{
  char rowAddr;

  if (rn == 1)
    rowAddr = 0x80; // First row of 16x2 LCD
  else
    rowAddr = 0xC0; // Second row of 16x2 LCD

  brkInstByte(rowAddr + (cp - 1)); // Send 8-bit LCD commands in 4-bit mode
  brkDataByte(mByte); // Send 8-bit LCD data in 4-bit mode
}

void initDisplay()
{
  char k, mArr[4] = {0x02, 0x28, 0x01, 0x0C}; // LCD 4-bit mode initialization commands
  for (k = 0; k < 4; k++)
    brkInstByte(mArr[k]); // Send 8-bit LCD commands in 4-bit mode
}

void setBackLight(int mBool)
{
  if (mBool)
    backLight = 0x08; // Turn ON backlight of LCD
  else
    backLight = 0x00; // Turn OFF backlight of LCD
}

char resetMeter() // Reset thermometer and detect presense pulse
{
  DQ = 0;
  delay_us(480); // Reset pulse delay
  DQ = 1;
  delay_us(112); // Wait for sensor to write presence (bit 0) on DQ line
                 // Meter normally shows its presence within 60 - 240us (max 480us)
  return (DQ);
}

char getBit()
{
  char k;
  DQ = 0; // Pull DQ low to start timeslot
  DQ = 1; // Then return high
  delay_us(15); // Bit read timeslot
  return (DQ); // return value of DQ line
}

void setBit(char mByte)
{
  DQ = 0; // Pull DQ low to start timeslot

  if (mByte)
    DQ = 1; // Return DQ high if write 1

  delay_us(60); // Bit write timeslot
  DQ = 1;
}

char getByte()
{
  char k, mByte = 0;
    for (k = 0; k < 8; k++)
    {
      if(getBit())
        mByte |= 0x01 << k; // Read one byte at a time then shift left

      delay_us(45); // Bit read additional timeslot
    }
  return (mByte);
}

void putByte(char mByte)
{
  char k, kt;
    for (k = 0; k < 8; k++) // Write byte as one bit at a time
    {
      kt = mByte >> k; // Shift byte to right for k spaces
      kt &= 0x01; // Copy that bit to temporary variable
      setBit(kt); // Write temporary bit
    }
}

void matchROM()
{
  char k;
  if (resetMeter() == 0)
  {
    putByte(0x55); // Match ROM, comapre 64-bit ROM code with meter's code
    for (k = 0; k < 8; k++)
      putByte(codeROM[k]);
  }
}

void skipROM()
{
  if (resetMeter() == 0)
    putByte(0xCC); // Skip ROM, dont read 64-bit ROM code in case of one meter
}

void readScratchPad()
{
  char k;
  putByte(0xBE); // Read 9-byte scratchpad data including CRC
  for (k = 0; k < 9; k++)
    mArr[k] = getByte();
}

void convTemperature()
{
  putByte(0x44); // Start temperature conversion to be placed on scratchpad
}

char readTemperature()
{
  char nByte, mByte;

//  matchROM(); // Call this function only if multiple meters are on bus
  skipROM(); // Call this function only if single meter is on bus
  convTemperature();

//  matchROM();
  skipROM(); // Call this function only if single meter is on bus
  readScratchPad();
  
  mByte = ((mArr[0] | mArr[1]) & 0xF0) >> 4; // Correcting upper nibble of byte
  nByte = ((mArr[0] | mArr[1]) & 0x0F) << 4; // Correcting lower nibble of byte
  return (mByte | nByte);
}

void readROM()
{
  char k;
  if (resetMeter() == 0)
  {
    putByte(0x33); // Read ROM
    for (k = 0; k < 8; k++)
      codeROM[k] = getByte();
  }
}

void main()
{
  char k, xByte, yByte;
  setBackLight(1); // Turning ON backlight
  initDisplay(); // Initializing LCD for 4-bit mode
  readROM(); // Read 64-bit unique ROM code
  for (;;)
  {
    xByte = yByte = readTemperature();

    xByte = (xByte / 10 * 16) + (xByte % 10); // Converting Hex. to Decimal
    putChar(((xByte >> 4) | 0x30), 1, 7);
    putChar(((xByte & 0x0F) | 0x30), 1, 8);
    putChar(223, 1, 9); // Degree symbol decimal code is 223 and hex. is 0xDF
    putChar('C', 1, 10);

    yByte = (yByte * 9 / 5) + 32; // Converting celcius to fahrenheit
    yByte = (yByte / 10 * 16) + (yByte % 10);
    putChar(((yByte >> 4) | 0x30), 2, 7);
    putChar(((yByte & 0x0F) | 0x30), 2, 8);
    putChar(223, 2, 9); // Degree symbol decimal code is 223 and hex. is 0xDF
    putChar('F', 2, 10);
  }
}