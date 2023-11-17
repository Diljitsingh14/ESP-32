//------------------------------------------------------------------------------------------------------------------------
//
// Title: SD Card Wav Player
//
// Description:
//    Simple example to demonstrate the fundementals of playing WAV files (digitised sound) from an SD Card via the I2S
//    interface of the ESP32. Plays WAV file from SD card. To keep this simple the WAV must be stereo and 16bit samples.
//    The Samples Per second can be anything. On the SD Card the wav file must be in root and called wavfile.wav
//    Libraries are available to play WAV's on ESP32, this code does not use these so that we can see what is happening.
//    This is part 3 in a tutorial series on using I2S on ESP32. See the accompanying web page (which will also include
//    a tutorial video).
//
// Boring copyright/usage information:
//    (c) XTronical, www.xtronical.com
//    Use as you wish for personal or monatary gain, or to rule the world (if that sort of thing spins your bottle)
//    However you use it, no warrenty is provided etc. etc. It is not listed as fit for any purpose you perceive
//    It may damage your house, steal your lover, drink your beers and more.
//
//    http://www.xtronical.com/i2s-ep3
//
//------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------
//
// Includes

#include "SD.h"         // SD Card library, usually part of the standard install
#include "driver/i2s.h" // Library of I2S routines, comes with ESP32 standard install

//------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------
// Defines

//    SD Card
#define SD_CS 5 // SD Card chip select

//    I2S
#define I2S_DOUT 26 // i2S Data out oin
#define I2S_BCLK 27 // Bit clock
#define I2S_LRC 14  // Left/Right clock, also known as Frame clock or word select
#define I2S_NUM -1  // i2s port number

// Wav File reading
#define NUM_BYTES_TO_READ_FROM_FILE 1024 // How many bytes to read from wav file at a time

//------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------
// structures and also variables
//  I2S configuration

static const i2s_config_t i2s_config =
    {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 44100, // Note, this will be changed later
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // high interrupt priority
        .dma_buf_count = 8,                       // 8 buffers
        .dma_buf_len = 64,                        // 64 bytes per buffer, so 8K of buffer space
        .use_apll = 0,
        .tx_desc_auto_clear = true,
        .fixed_mclk = -1};

// These are the physical wiring connections to our I2S decoder board/chip from the esp32, there are other connections
// required for the chips mentioned at the top (but not to the ESP32), please visit the page mentioned at the top for
// further information regarding these other connections.

static const i2s_pin_config_t pin_config =
    {
        .bck_io_num = I2S_BCLK,          // The bit clock connectiom, goes to pin 27 of ESP32
        .ws_io_num = I2S_LRC,            // Word select, also known as word select or left right clock
        .data_out_num = I2S_DOUT,        // Data out from the ESP32, connect to DIN on 38357A
        .data_in_num = I2S_PIN_NO_CHANGE // we are not interested in I2S data into the ESP32
};

struct WavHeader_Struct
{
  //   RIFF Section
  char RIFFSectionID[4]; // Letters "RIFF"
  uint32_t Size;         // Size of entire file less 8
  char RiffFormat[4];    // Letters "WAVE"

  //   Format Section
  char FormatSectionID[4]; // letters "fmt"
  uint32_t FormatSize;     // Size of format section less 8
  uint16_t FormatID;       // 1=uncompressed PCM
  uint16_t NumChannels;    // 1=mono,2=stereo
  uint32_t SampleRate;     // 44100, 16000, 8000 etc.
  uint32_t ByteRate;       // =SampleRate * Channels * (BitsPerSample/8)
  uint16_t BlockAlign;     // =Channels * (BitsPerSample/8)
  uint16_t BitsPerSample;  // 8,16,24 or 32

  // Data Section
  char DataSectionID[4]; // The letters "data"
  uint32_t DataSize;     // Size of the data that follows
} WavHeader;
//------------------------------------------------------------------------------------------------------------------------

//  Global Variables/objects

File WavFile;                                // Object for root of SD card directory
static const i2s_port_t i2s_num = I2S_NUM_0; // i2s port number

//------------------------------------------------------------------------------------------------------------------------

void setup()
{
  Serial.begin(115200); // Used for info/debug
  SDCardInit();
  i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
  i2s_set_pin(i2s_num, &pin_config);
  // get the wav file from the SD card
  WavFile = SD.open("/heavenly.wav"); // Open the wav file
  if (WavFile == false)
    Serial.println("Could not open 'heavenly.wav'");
  else
  {
    WavFile.read((byte *)&WavHeader, 44);                  // Read in the WAV header, which is first 44 bytes of the file.
                                                           // We have to typecast to bytes for the "read" function
    DumpWAVHeader(&WavHeader);                             // Dump the header data to serial, optional!
    if (ValidWavData(&WavHeader))                          // optional if your sure the WAV file will be valid.
      i2s_set_sample_rates(i2s_num, WavHeader.SampleRate); // set sample rate
  }
}

void loop()
{
  PlayWav(); // Have to keep calling this to keep the wav file playing

  // Your normal code to do your task can go here
}

void PlayWav()
{
  static bool ReadingFile = true;                   // True if reading file from SD. false if filling I2S buffer
  static byte Samples[NUM_BYTES_TO_READ_FROM_FILE]; // Memory allocated to store the data read in from the wav file
  static uint16_t BytesRead;                        // Num bytes actually read from the wav file which will either be
                                                    // NUM_BYTES_TO_READ_FROM_FILE or less than this if we are very
                                                    // near the end of the file. i.e. we can't read beyond the file.

  if (ReadingFile) // Read next chunk of data in from file if needed
  {
    BytesRead = ReadFile(Samples); // Read data into our memory buffer, return num bytes read in
    ReadingFile = false;           // Switch to sending the buffer to the I2S
  }
  else
    ReadingFile = FillI2SBuffer(Samples, BytesRead); // We keep calling this routine until it returns true, at which point
                                                     // this will swap us back to Reading the next block of data from the file.
                                                     // Reading true means it has managed to push all the data to the I2S
                                                     // Handler, false means there still more to do and you should call this
                                                     // routine again and again until it returns true.
}

uint16_t ReadFile(byte *Samples)
{
  static uint32_t BytesReadSoFar = 0; // Number of bytes read from file so far
  uint16_t BytesToRead;               // Number of bytes to read from the file

  if (BytesReadSoFar + NUM_BYTES_TO_READ_FROM_FILE > WavHeader.DataSize) // If next read will go past the end then adjust the
    BytesToRead = WavHeader.DataSize - BytesReadSoFar;                   // amount to read to whatever is remaining to read
  else
    BytesToRead = NUM_BYTES_TO_READ_FROM_FILE; // Default to max to read

  WavFile.read(Samples, BytesToRead); // Read in the bytes from the file
  BytesReadSoFar += BytesToRead;      // Update the total bytes red in so far

  if (BytesReadSoFar >= WavHeader.DataSize) // Have we read in all the data?
  {
    WavFile.seek(44);   // Reset to start of wav data
    BytesReadSoFar = 0; // Clear to no bytes read in so far
  }
  return BytesToRead; // return the number of bytes read into buffer
}

bool FillI2SBuffer(byte *Samples, uint16_t BytesInBuffer)
{
  // Writes bytes to buffer, returns true if all bytes sent else false, keeps track itself of how many left
  // to write, so just keep calling this routine until returns true to know they've all been written, then
  // you can re-fill the buffer

  size_t BytesWritten;           // Returned by the I2S write routine,
  static uint16_t BufferIdx = 0; // Current pos of buffer to output next
  uint8_t *DataPtr;              // Point to next data to send to I2S
  uint16_t BytesToSend;          // Number of bytes to send to I2S

  // To make the code eaier to understand I'm using to variables to some calculations, normally I'd write this calcs
  // directly into the line of code where they belong, but this make it easier to understand what's happening

  DataPtr = Samples + BufferIdx;                              // Set address to next byte in buffer to send out
  BytesToSend = BytesInBuffer - BufferIdx;                    // This is amount to send (total less what we've already sent)
  i2s_write(i2s_num, DataPtr, BytesToSend, &BytesWritten, 1); // Send the bytes, wait 1 RTOS tick to complete
  BufferIdx += BytesWritten;                                  // increasue by number of bytes actually written

  if (BufferIdx >= BytesInBuffer)
  {
    // sent out all bytes in buffer, reset and return true to indicate this
    BufferIdx = 0;
    return true;
  }
  else
    return false; // Still more data to send to I2S so return false to indicate this
}

void SDCardInit()
{
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH); // SD card chips select, must use GPIO 5 (ESP32 SS)
  if (!SD.begin(SD_CS))
  {
    Serial.println("Error talking to SD card!");
    while (true)
      ; // end program
  }
}

bool ValidWavData(WavHeader_Struct *Wav)
{

  if (memcmp(Wav->RIFFSectionID, "RIFF", 4) != 0)
  {
    Serial.print("Invalid data - Not RIFF format");
    return false;
  }
  if (memcmp(Wav->RiffFormat, "WAVE", 4) != 0)
  {
    Serial.print("Invalid data - Not Wave file");
    return false;
  }
  if (memcmp(Wav->FormatSectionID, "fmt", 3) != 0)
  {
    Serial.print("Invalid data - No format section found");
    return false;
  }
  if (memcmp(Wav->DataSectionID, "data", 4) != 0)
  {
    Serial.print("Invalid data - data section not found");
    return false;
  }
  if (Wav->FormatID != 1)
  {
    Serial.print("Invalid data - format Id must be 1");
    return false;
  }
  if (Wav->FormatSize != 16)
  {
    Serial.print("Invalid data - format section size must be 16.");
    return false;
  }
  if ((Wav->NumChannels != 1) & (Wav->NumChannels != 2))
  {
    Serial.print("Invalid data - only mono or stereo permitted.");
    return false;
  }
  if (Wav->SampleRate > 48000)
  {
    Serial.print("Invalid data - Sample rate cannot be greater than 48000");
    return false;
  }
  if ((Wav->BitsPerSample != 8) & (Wav->BitsPerSample != 16))
  {
    Serial.print("Invalid data - Only 8 or 16 bits per sample permitted.");
    return false;
  }
  return true;
}

void DumpWAVHeader(WavHeader_Struct *Wav)
{
  if (memcmp(Wav->RIFFSectionID, "RIFF", 4) != 0)
  {
    Serial.print("Not a RIFF format file - ");
    PrintData(Wav->RIFFSectionID, 4);
    return;
  }
  if (memcmp(Wav->RiffFormat, "WAVE", 4) != 0)
  {
    Serial.print("Not a WAVE file - ");
    PrintData(Wav->RiffFormat, 4);
    return;
  }
  if (memcmp(Wav->FormatSectionID, "fmt", 3) != 0)
  {
    Serial.print("fmt ID not present - ");
    PrintData(Wav->FormatSectionID, 3);
    return;
  }
  if (memcmp(Wav->DataSectionID, "data", 4) != 0)
  {
    Serial.print("data ID not present - ");
    PrintData(Wav->DataSectionID, 4);
    return;
  }
  // All looks good, dump the data
  Serial.print("Total size :");
  Serial.println(Wav->Size);
  Serial.print("Format section size :");
  Serial.println(Wav->FormatSize);
  Serial.print("Wave format :");
  Serial.println(Wav->FormatID);
  Serial.print("Channels :");
  Serial.println(Wav->NumChannels);
  Serial.print("Sample Rate :");
  Serial.println(Wav->SampleRate);
  Serial.print("Byte Rate :");
  Serial.println(Wav->ByteRate);
  Serial.print("Block Align :");
  Serial.println(Wav->BlockAlign);
  Serial.print("Bits Per Sample :");
  Serial.println(Wav->BitsPerSample);
  Serial.print("Data Size :");
  Serial.println(Wav->DataSize);
}

void PrintData(const char *Data, uint8_t NumBytes)
{
  for (uint8_t i = 0; i < NumBytes; i++)
    Serial.print(Data[i]);
  Serial.println();
}
