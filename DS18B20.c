#define DEV_ADDR_LCD 0x70 // I2C address of PCF8574A extender

sbit sclPin at P3.B0; // I2C serial clock line
sbit sdaPin at P3.B1; // I2C serial data line
sbit DQ at P3.B2; // DS18B20 temprature sensor pin
char backLight;

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

void writeToI2C(char mAddr, char mByte)
{
  startI2C();
  writeToSlave(mAddr);
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

void delay(int mTime)
{
  int k;
  for (k = 0; k < mTime; k++); // Delay in milliseconds
}
 
char resetSensor()
{
  DQ = 0;
  delay(29);
  DQ = 1;
  delay(28); // Wait for sensor to write 0 on DQ line
  return (DQ);
}

char getBit()
{
  char k;
  DQ = 0; // pull DQ low to start timeslot
  DQ = 1; // then return high
  for (k = 0; k < 3; k++); // delay 15us from start of timeslot
  return (DQ); // return value of DQ line
}

void setBit(char mByte)
{
  DQ = 0; // pull DQ low to start timeslot
  
  if (mByte)
    DQ = 1; // return DQ high if write 1
  else
    DQ = 0;
    
  delay(5); // hold value for remainder of timeslot
  DQ = 1;
}// Delay provides 16us per loop, plus 24us. Therefore delay(5) = 104us

char getByte()
{
  char k, mByte = 0;
    for (k = 0; k < 8; k++)
    {
      if(getBit())
        mByte |= 0x01 << k; // reads byte in, one byte at a time and then shifts it left
        
      delay(6); // wait for rest of timeslot
    }
  return (mByte);
}

void putByte(char mByte)
{
  char k, kt;
    for (k = 0; k < 8; k++) // writes byte, one bit at a time
    {
     kt = mByte >> k; // shifts val right 'i' spaces
     kt &= 0x01; // copy that bit to temp
     setBit(kt); // write bit in temp into
    }
  delay(5);
}

char readTemperature()
{
  char mArr[9];
  char nByte, mByte;
  char k, xByte, yByte;
  
  resetSensor();
  putByte(0xCC); // Skip ROM
  putByte(0x44); // Start termperature conversion
  
  delay(5);
  
  resetSensor();
  putByte(0xCC); // Skip ROM
  putByte(0xBE); // Read scratch pad
  
  for (k = 0; k < 9; k++)
      mArr[k] = getByte();

  nByte = mArr[0]; // Temperature LSB
  mByte = mArr[1]; // Temperature MSB
  k = nByte | mByte; // Merging MSB and LSB
  xByte = (k & 0xF0) >> 4; // Correcting upper nibble of byte
  yByte = (k & 0x0F) << 4; // Correcting lower nibble of byte
  k = xByte | yByte; // Merging both nibbles
  return (k);
}

void main() 
{
  char xByte, yByte;
  setBackLight(1); // Turning ON backlight
  initDisplay(); // Initializing LCD for 4-bit mode
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