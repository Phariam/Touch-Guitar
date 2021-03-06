/*
   This example plays every .WAV file it finds on the SD card in a loop
*/
#include <WaveHC.h>
#include <WaveUtil.h>

//CAP

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_CAP1188.h>

// Reset Pin is used for I2C or SPI
#define CAP1188_RESET  11

// CS pin is used for software or hardware SPI
#define CAP1188_CS  10

// These are defined for software SPI, for hardware SPI, check your
// board's SPI pins in the Arduino documentation
#define CAP1188_MOSI  11
#define CAP1188_MISO  12
#define CAP1188_CLK  13

Adafruit_CAP1188 cap = Adafruit_CAP1188(CAP1188_RESET);

//CAP

SdReader card;    // This object holds the information for the card
FatVolume vol;    // This holds the information for the partition on the card
FatReader root;   // This holds the information for the volumes root directory
WaveHC wave;      // This is the only wave (audio) object, since we will only play one at a time
FatReader f;      // This holds the information for the file we're playing

uint8_t dirLevel; // indent level for file/dir names    (for prettyprinting)
dir_t dirBuf;     // buffer for directory reads


/*
   Define macro to put error messages in flash memory
*/
#define error(msg) error_P(PSTR(msg))

// Function definitions (we define them here, but the code is below)
void play(FatReader &dir);

//////////////////////////////////// SETUP
void setup() {
  Serial.begin(9600);           // set up Serial library at 9600 bps for debugging

  //CAP

  Serial.println("CAP1188 test!");

  // Initialize the sensor, if using i2c you can pass in the i2c address
  // if (!cap.begin(0x28)) {
  if (!cap.begin()) {
    Serial.println("CAP1188 not found");
    while (1);
  }
  Serial.println("CAP1188 found!");

  //CAP


  putstring_nl("\nWave test!");  // say we woke up!

  putstring("Free RAM: ");       // This can help with debugging, running out of RAM is bad
  Serial.println(FreeRam());

  //  if (!card.init(true)) { //play with 4 MHz spi if 8MHz isn't working for you
  if (!card.init()) {         //play with 8 MHz spi (default faster!)
    error("Card init. failed!");  // Something went wrong, lets print out why
  }

  // enable optimize read - some cards may timeout. Disable if you're having problems
  card.partialBlockRead(true);

  // Now we will look for a FAT partition!
  uint8_t part;
  for (part = 0; part < 5; part++) {   // we have up to 5 slots to look in
    if (vol.init(card, part))
      break;                           // we found one, lets bail
  }
  if (part == 5) {                     // if we ended up not finding one  :(
    error("No valid FAT partition!");  // Something went wrong, lets print out why
  }

  // Lets tell the user about what we found
  putstring("Using partition ");
  Serial.print(part, DEC);
  putstring(", type is FAT");
  Serial.println(vol.fatType(), DEC);     // FAT16 or FAT32?

  // Try to open the root directory
  if (!root.openRoot(vol)) {
    error("Can't open root dir!");      // Something went wrong,
  }

  // Whew! We got past the tough parts.
  putstring_nl("Files found (* = fragmented):");

  // Print out all of the files in all the directories.
  root.ls(LS_R | LS_FLAG_FRAGMENTED);

  pinMode(A5, INPUT);
}

//THIS IS WHERE THE MAGIC HAPPENS


// The volume can be set from 0 to 12. A volume value of 0 is maximum, and 12 is silence. Anything higher than 12 will be the same as 12.

int volumeval = 12; // This means the volume is on its lowest point. You hear nothing. 

long oldmillis = 0;
int volumedelay = 200; //This is where you define how long the WAV-file keeps playing before it fades out again.

void loop() {

  uint8_t touched = cap.touched();

  if (touched == 0) { //If a string is NOT touched, then decrease volume
    if (millis() > oldmillis + volumedelay && volumeval < 12)
    {
      volumeval++; //This decreases the volume. The logic is a little strange, but the higher this variable is, the softer the sound is.
      oldmillis = millis();
    }
  }


  for (uint8_t i = 0; i < 8; i++) {
    if (touched & (1 << i)) { //If a string is touched, then max volume.
      volumeval = 0;
      oldmillis = millis();
    }
  }
  Serial.println();
  delay(50);

  if (!wave.isplaying)
  {
    playfile("NONSTOP.WAV"); //REPLACE NONSTOP.WAV WITH THE NAME OF YOUR FILE. 
  }

  wave.volume = volumeval;

  //END OF THE MAGIC. You're done here!
  
}

void playfile(char *name) {
  // see if the wave object is currently doing something
  if (wave.isplaying) {// already playing something, so stop it!
    wave.stop(); // stop it
  }
  // look in the root directory and open the file
  if (!f.open(root, name)) {
    putstring("Couldn't open file "); Serial.print(name); return;
  }
  else
  {
    Serial.println("gelukt");
  }
  // OK read the file and turn it into a wave object
  if (!wave.create(f)) {
    putstring_nl("Not a valid WAV"); return;
  }



  // ok time to play! start playback
  wave.play();
}

/////////////////////////////////// HELPERS
/*
   print error message and halt
*/
void error_P(const char *str) {
  PgmPrint("Error: ");
  SerialPrint_P(str);
  sdErrorCheck();
  while (1);
}
/*
   print error message and halt if SD I/O error, great for debugging!
*/
void sdErrorCheck(void) {
  if (!card.errorCode()) return;
  PgmPrint("\r\nSD I/O error: ");
  Serial.print(card.errorCode(), HEX);
  PgmPrint(", ");
  Serial.println(card.errorData(), HEX);
  while (1);
}
/*
   play recursively - possible stack overflow if subdirectories too nested
*/
void play(FatReader &dir) {
  FatReader file;
  while (dir.readDir(dirBuf) > 0) {    // Read every file in the directory one at a time

    // Skip it if not a subdirectory and not a .WAV file
    if (!DIR_IS_SUBDIR(dirBuf)
        && strncmp_P((char *)&dirBuf.name[8], PSTR("WAV"), 3)) {
      continue;
    }

    Serial.println();            // clear out a new line

    for (uint8_t i = 0; i < dirLevel; i++) {
      Serial.write(' ');       // this is for prettyprinting, put spaces in front
    }
    if (!file.open(vol, dirBuf)) {        // open the file in the directory
      error("file.open failed");          // something went wrong
    }

    if (file.isDir()) {                   // check if we opened a new directory
      putstring("Subdir: ");
      printEntryName(dirBuf);
      Serial.println();
      dirLevel += 2;                      // add more spaces
      // play files in subdirectory
      play(file);                         // recursive!
      dirLevel -= 2;
    }
    else {
      // Aha! we found a file that isnt a directory
      putstring("Playing ");
      printEntryName(dirBuf);              // print it out
      if (!wave.create(file)) {            // Figure out, is it a WAV proper?
        putstring(" Not a valid WAV");     // ok skip it
      } else {
        Serial.println();                  // Hooray it IS a WAV proper!
        wave.play();                       // make some noise!

        uint8_t n = 0;
        while (wave.isplaying) {// playing occurs in interrupts, so we print dots in realtime
          putstring(".");
          if (!(++n % 32))Serial.println();
          delay(100);
        }
        sdErrorCheck();                    // everything OK?
        // if (wave.errors)Serial.println(wave.errors);     // wave decoding errors
      }
    }
  }
}
