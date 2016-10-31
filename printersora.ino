

/**
  Printesora.ino
  Name: Printesora
  Purpose: Send images/text stored on SD card to thermal printer.

  @author Erick Martinez
  @version 1.0 2016/10/25
*/

#include <SD.h>
#include <SoftwareSerial.h>
#include <Adafruit_Thermal.h>

// Pins
const int LED_PIN             = 3;  // To status LED (hardware PWM pin)
const int SD_CARD_SELECT_LINE = 4;  // Card Select line for Arduino Ethernet
const int RX_PIN              = 5;  // Arduino receive   GREEN WIRE   labeled TX on printer
const int TX_PIN              = 6;  // Arduino transmit  YELLOW WIRE  labeled RX on printer
const int PRINTER_GROUND      = 7;  // Printer connection: black wire

// Settings
const String SETTINGS_FILE = "/settings.txt";

// Default Settings
struct Settings {
  int     charset;
  int     codePage;
  int     lineHeight;
  int     heatTime;
  int     dotPrintTime;
  int     dotFeedTime;
  String  fontSize;
  String  jobs;

  void init() {
    charset = CHARSET_USA;
    codePage = CODEPAGE_CP437;
    lineHeight = 28;
    heatTime = 120;
    dotPrintTime = 30000;
    dotFeedTime = 2100;
    fontSize = "S";
    jobs = "/jobs/";
  }

} configuration;

// Default Document
struct Document {
  int     imageWidth;
  int     imageHeight;
  String  image;
  String  bodyText;

  void init() {
    imageWidth = 0;
    imageHeight = 0;
    image = "";
    bodyText = "";
  }

} document;

// Create printer
SoftwareSerial mySerial(RX_PIN, TX_PIN); // Declare SoftwareSerial obj first
Adafruit_Thermal printer(&mySerial);     // Pass addr to printer constructor



/**
   Setup
*/
void setup() {

  Serial.begin(9600);

  // Start Led
  analogWrite(LED_PIN, 255);
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);   // turn the LED on (HIGH is the voltage level)

  // Init SD
  if (!SD.begin(SD_CARD_SELECT_LINE))
  {
    Serial.println("No SD Card Found");
    return;
  }

  // Load settings from SD Card
  configuration.init();
  readSDSettings(SETTINGS_FILE);

  // Init printer from settings and document values
  // This line is for compatibility with the Adafruit IotP project pack,
  // which uses pin 7 as a spare grounding point.  You only need this if
  // wired up the same way (w/3-pin header into pins 5/6/7):
  pinMode(PRINTER_GROUND, OUTPUT);
  digitalWrite(PRINTER_GROUND, LOW);
  // NOTE: SOME PRINTERS NEED 9600 BAUD instead of 19200, check test page.
  mySerial.begin(19200);  // Initialize SoftwareSerial

  printFromDirectory(configuration.jobs);

  printer.sleep();      // Tell printer to sleep
  delay(3000L);         // Sleep for 3 seconds
  printer.wake();       // MUST wake() before printing again, even if reset
  printer.setDefault(); // Restore printer to defaults

  Serial.println("Done init");

}

void loop() {

}

/**
   Set printer parameters
   to be call before each printout as parameters may change
   from document to document
*/
void setPrinterParameters() {
  printer.begin(configuration.heatTime);      // default 120
  printer.setSize(configuration.fontSize.charAt(0));
  printer.setLineHeight(configuration.lineHeight);
  printer.setCharset(configuration.charset);
  printer.setCodePage(configuration.codePage);
}
/**
   Void
*/
void printFromDirectory(String directoryName) {

  if (SD.exists(directoryName)) {
    File dir = SD.open(directoryName);
    if (dir) {
      dir.rewindDirectory();
      while (true) {
        File entry =  dir.openNextFile();
        if (! entry) {
          // no more files
          dir.rewindDirectory();
          Serial.println("No files left on " + directoryName);
          break;
        }
        if (! entry.isDirectory()) {
          // Print document
          Serial.print("Printing document: ");
          Serial.println(entry.name());
          Serial.println(entry.name());

          document.init();
          readSettingsFromFile(entry);
          setPrinterParameters();

          printImage(document.image, document.imageWidth, document.imageHeight);
          printText(document.bodyText);
        }
        entry.close();
      }
    }
    dir.close();
    Serial.println("Done with directory " + directoryName);
  }
  printer.feed(2);

}

/**
   Print image
   Opens a file stream and passes it directly to the printer's printBitmap
*/
boolean printImage(String filename, int imageWidth, int imageHeigth) {
  if (SD.exists(filename)) {
    File imageFile = SD.open(filename);
    if (imageFile) {
      Serial.println("Printing image from file: " + filename);
      printer.feed(1);
      while (imageFile.available()) {
        printer.printBitmap(imageWidth, imageHeigth, dynamic_cast<Stream*>(&imageFile));
      }
      imageFile.close();
      return true;
    }
  }
  return false;
}

/**
   Opens a file stream with the text and passes it to the printer's print method
*/
boolean printText (String filename) {
  if (SD.exists(filename)) {
    File dataFile = SD.open(filename);
    if (dataFile) {
      Serial.println("Printing text from file: " + filename);

      printer.feed(1);
      // Assume first line is the title
      printer.boldOn();
      printer.justify('C');
      printer.setSize('M');
      printer.println(dataFile.readStringUntil(10)); 
      printer.setSize(configuration.fontSize.charAt(0));
      printer.justify('L');
      printer.boldOff();
      while (dataFile.available()) {
        printer.println(dataFile.readStringUntil(10));
      }
      dataFile.close();
      return true;
    }
  }
  return false;
}

/**
   Reads attributes from the passed file on the SD and
   sets the values on global variables.
*/
void readSDSettings(String filename) {
  char character;
  String settingName;
  String settingValue;

  if (SD.exists(filename)) {
    File myFile;
    myFile = SD.open(filename);
    readSettingsFromFile(myFile);
    myFile.close();

  } else {
    // if the file didn't open, print an error:
    Serial.println("Error opening: " + filename);
  }
}

/**
   Reads attributes from the passed file on the SD and
   sets the values on global variables.
*/
void readSettingsFromFile(File document) {
  char character;
  String settingName;
  String settingValue;

  if (document) {

    while (document.available()) {
      character = document.read();
      while ((document.available()) && (character != '[')) {
        character = document.read();
      }
      character = document.read();
      while ((document.available()) && (character != '=')) {
        settingName = settingName + character;
        character = document.read();
      }
      character = document.read();
      while ((document.available()) && (character != ']')) {
        settingValue = settingValue + character;
        character = document.read();
      }

      if (character == ']') {
        // Apply the value to the parameter
        Serial.println("Applying setting: " + settingName + "=" + settingValue);
        applySetting(settingName, settingValue);
        // Reset Strings
        settingName = "";
        settingValue = "";
      }
    }
  }
}


/* Apply the value to the parameter by searching for the parameter name
  Using String.toInt(); for Integers
  toFloat(string); for Float
  toBoolean(string); for Boolean
  toLong(string); for Long
*/
void applySetting(String settingName, String settingValue) {

  // Configuration attributes
  if (settingName == "font_size") {
    configuration.fontSize = settingValue;
  } else if (settingName == "jobs") {
    configuration.jobs = settingValue;
  } else if (settingName == "line_height") {
    configuration.lineHeight = settingValue.toInt();
  } else if (settingName == "charset") {
    configuration.charset = settingValue.toInt();
  } else if (settingName == "codepage") {
    configuration.codePage = settingValue.toInt();
  } else if (settingName == "heat_time") {
    configuration.heatTime = settingValue.toInt();
  }

  // Document attributes
  if (settingName == "image") {
    document.image = settingValue;
  } else if (settingName == "body_text") {
    document.bodyText = settingValue;
  } else if (settingName == "width") {
    document.imageWidth = settingValue.toInt();
  } else if (settingName == "height") {
    document.imageHeight = settingValue.toInt();
  }
}


