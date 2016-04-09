#include <SPI.h>
#include <SD.h>

//#define LOGGING

#define DATA_OUT  19  // READ
#define DATA_IN   18  // WRITE
#define MOTOR_IN  21
#define SENSE_OUT 16
#define RAM_CS    9
#define BUTTON    7
#define SD_CS     2

#define WINDOW_SIZE 1024

#define WRSR  1
#define WRITE 2
#define READ  3

#define LONG_PRESS  500
#define SHORT_PRESS 20

#define s8 signed char
#define s16 signed short
#define s32 signed int
#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int

SPISettings RAMSPISettings(20000000, MSBFIRST, SPI_MODE0); 

typedef struct TapHdr_s
{
  char sig[12];
  u8 version;
  u8 dummy[3];
  u8 size[4];
} TapHdr;

//Header
TapHdr hdr;

//LZ stuff
u8 window[WINDOW_SIZE];
u16 head;
u16 offset;
u8 length;

File tape;
u32 filePos;
bool bDoneLoad;

void LZLikeReset()
{
  head=0;
  length=0;
  filePos=0;
}

u8 LZLikeDecode()
{
  if (length==0)
  {
    u8 a=SPI.transfer(0);
    u8 b=SPI.transfer(0);
    filePos+=2;
    if (a==0)
    {
      window[(head++)&(WINDOW_SIZE-1)]=b;
      return b;
    }
    else
    {
      offset=head-1-(b+((a>>6)<<8))+WINDOW_SIZE;
      length=(a&0x3F)+1;
      offset+=length-1;
    }
  }
  length--;
  u8 v=window[(offset-length)&(WINDOW_SIZE-1)];
  window[(head++)&(WINDOW_SIZE-1)]=v;
  return v;
}

void ReadToRAM(u32 fileOffset)
{
  u8 buf[1024];
  u32 address=0;
  tape.seek(fileOffset);
  while (tape.available() && address<128*1024)
  {
    for (u32 i=0; i<sizeof(buf); i++)
      buf[i]=tape.read();
    SPI.beginTransaction(RAMSPISettings);
    digitalWrite(RAM_CS, LOW);
    SPI.transfer(WRITE); // Write
    SPI.transfer((address>>16)&0xFF); // Top address
    SPI.transfer((address>>8)&0xFF); // Mid address
    SPI.transfer(address&0xFF); // Low address
    for (u32 i=0; i<sizeof(buf); i++)
      SPI.transfer(buf[i]); // Data
    digitalWrite(RAM_CS, HIGH);
    SPI.endTransaction();
    address+=sizeof(buf);
  }
  bDoneLoad=!tape.available();
#ifdef LOGGING
  Serial.println(bDoneLoad?"Done load":"Done loading a chunk");
#endif
}

void ReadMore()
{
  if (!bDoneLoad)
  {
#ifdef LOGGING
    Serial.println("Reading a bit more");
#endif
    digitalWrite(RAM_CS, HIGH);
    SPI.endTransaction();
    ReadToRAM(filePos);
    SPI.beginTransaction(RAMSPISettings);
    digitalWrite(RAM_CS, LOW);
    SPI.transfer(READ);
    SPI.transfer(0);  // Top address
    SPI.transfer(0); // Mid address
    SPI.transfer(0);  // Low address
  }
}

void PlayTape()
{
  u32 size=hdr.size[0];
  size+=hdr.size[1]<<8;
  size+=hdr.size[2]<<16;
  size+=hdr.size[3]<<24;
  digitalWrite(DATA_OUT, LOW);
  while (size--)
  {
#ifndef LOGGING
    // Wait for motor start signal
    while (digitalRead(MOTOR_IN)==LOW);
#endif
    u32 start=micros();
    u32 delay=8*LZLikeDecode();
    if (delay==0)
    {
      if (hdr.version==1)
      {
        delay=LZLikeDecode();
        delay|=LZLikeDecode()<<8;
        delay|=LZLikeDecode()<<16;
        if (size<3)
          return;
        size-=3;
      }
      else
      {
        delay=1000000; // Some arbitary long time
      }
      if (delay>=1000000)  // If a second gap or more and not fully loaded to memory yet then load fresh chunk from SD
        ReadMore();
    }
    while (micros()<start+delay/2);
    digitalWrite(DATA_OUT, HIGH);
    while (micros()<start+delay);
    digitalWrite(DATA_OUT, LOW);
  }
}

int LoadAndCheckHeader()
{
  u8 *raw=(u8*)&hdr;
  LZLikeReset();
  for (int i=0; i<(int)sizeof(TapHdr); i++)
  {
    raw[i]=LZLikeDecode();
  }
  if (strncmp(hdr.sig, "C64-TAPE-RAW", sizeof(hdr.sig))!=0)
  {
    return 1;
  }
  if (hdr.version>1)
  {
    return 2;
  }
  return 0;
}

bool isLongButtonPress()
{
  while (true)
  {
    while (digitalRead(BUTTON)==HIGH);
    u32 startTime=millis();
    while (millis()-startTime<LONG_PRESS && digitalRead(BUTTON)==LOW);
    if (millis()-startTime>=LONG_PRESS)
      return true;
    if (millis()-startTime>SHORT_PRESS)
      return false;
  }
}

void LoadTape()
{
  SPI.beginTransaction(RAMSPISettings);
  digitalWrite(RAM_CS, LOW);
  SPI.transfer(READ);
  SPI.transfer(0);  // Top address
  SPI.transfer(0); // Mid address
  SPI.transfer(0);  // Low address
  if (LoadAndCheckHeader()==0)
  {
    if (isLongButtonPress())
    {
#ifdef LOGGING
      Serial.println("Playing tape");
#endif
      digitalWrite(SENSE_OUT, LOW);
      PlayTape();
      digitalWrite(SENSE_OUT, HIGH);
    }
  }
  digitalWrite(RAM_CS, HIGH);
  SPI.endTransaction();
}

void LoadToRAM()
{
  while (true)
  {
    File dir = SD.open("/");
    while (true)
    {
      tape = dir.openNextFile();
      if (!tape)
        break;
      if (tape.isDirectory())
        continue;
#ifdef LOGGING
      Serial.print(tape.name());
      Serial.println(": About to read");
#endif
      ReadToRAM(0);
      LoadTape();
      tape.close();
    }
    dir.close();
  }
}

void InitialiseRAM()
{
    SPI.beginTransaction(RAMSPISettings);
    digitalWrite(RAM_CS, LOW);
    SPI.transfer(WRSR); // Write status
    SPI.transfer(0x40); // Sequential mode
    digitalWrite(RAM_CS, HIGH);
    SPI.endTransaction();
}

void setup() 
{
  pinMode(DATA_OUT, OUTPUT);
  pinMode(DATA_IN, INPUT);
  pinMode(MOTOR_IN, INPUT);
  pinMode(SENSE_OUT, OUTPUT);
  pinMode(RAM_CS, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  digitalWrite(SENSE_OUT, HIGH);
  digitalWrite(DATA_OUT, LOW);
  digitalWrite(RAM_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
#ifdef LOGGING
  Serial.begin(9600);
  while (!Serial);  // Uncomment to debug
#endif
  SPI.begin();
  SD.begin(SD_CS);
  InitialiseRAM();
  LoadToRAM();
}

void loop() 
{
  // Do nothing
}
