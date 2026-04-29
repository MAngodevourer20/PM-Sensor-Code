#include "Adafruit_BME680.h"
#include <Adafruit_Sensor.h>
#include <PMS5003.h> //!!!!! HAD TO FIX THIS LIBRARY A BIT CAUSE OF A TYPO THE PN500 and PN5000 values were the same but i fixed it so now it has both
#include <SPI.h>
#include <Wire.h>
#include <hd44780.h>                       // main hd44780 header
#include <hd44780ioClass/hd44780_I2Cexp.h> // i2c expander i/o class header

constexpr int RX3 = 0;
constexpr int TX3 = 1;
constexpr int SEALEVELPRESSURE_HPA = 1004;
constexpr int LCD_COLS = 16;
constexpr int LCD_ROWS = 2;
volatile int lcdscroll = 0;
constexpr int butpin = D6;
volatile int butstate = 0;
int prevbutstate = 0;
float prevbut=0;
float sleeptimer = 300000;//20000;
float readingtimer = 0;

namespace {
hd44780_I2Cexp
    lcd; // declare lcd object: auto locate & auto config expander chip
GuL::PMS5003 pms(Serial2);
Adafruit_BME680 bme; // I2C

int pm1 = 0;
int pm2_5 = 0;
int pm10 = 0;
int pn300 = 0;
int pn500 = 0;
int pn1000 = 0;
int pn2500 = 0;
int pn5000 = 0;
int pn10000 = 0;
float temperature = 0.00;
float pressure = 0.00;
float RH = 0.00;
float gas_resistance = 0.00;
float altitude = 0.00;
} // namespace

std::string outputFormat = "PM1 (STD) \t= % 6d µg/µ3 \n"
                           "PM2.5 (STD) \t= % 6d µg/µ3 \n"
                           "PM10 (STD) \t= % 6d µg/µ3 \n"
                           "PM1 (ATM) \t= % 6d µg/µ3 \n"
                           "PM2.5 (ATM) \t= % 6d µg/µ3 \n"
                           "PM10 (ATM) \t= % 6d µg/µ3 \n"
                           "\n"
                           "PN300 \t= % 6d #\\0.1 l \n"
                           "PN500 \t= % 6d #\\0.1 l \n"
                           "PN1000 \t= % 6d #\\0.1 l \n"
                           "PN2500 \t= % 6d #\\0.1 l \n"
                           "PN5000 \t= % 6d #\\0.1 l \n"
                           "PN10000 \t= % 6d #\\0.1 l \n"
                           "\n";

void IRAM_ATTR handleipress() {
  butstate=1;
}


void setup() {
  Wire.begin();

  Serial.begin(115200);

  Serial2.begin(9600, SERIAL_8N1, RX3, TX3);
  pms.setToPassiveReporting();

  pinMode(butpin, INPUT);
  attachInterrupt(digitalPinToInterrupt(butpin), handleipress, FALLING); // Trigger on falling edge

  if (!bme.begin(0x76, &Wire)) {
    Serial.println(F("Could not find a valid BME680 sensor, check wiring!"));
    delay(5000);
  }

  // Set up bme680 oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

  // initialize LCD with number of columns and rows:
  // hd44780 returns a status from begin() that can be used
  // to determine if initalization failed.
  // the actual status codes are defined in <hd44780.h>
  // See the values RV_XXXX
  //
  // looking at the return status from begin() is optional
  // it is being done here to provide feedback should there be an issue
  //
  // note:
  //	begin() will automatically turn on the backlight
  //
  int status = lcd.begin(LCD_COLS, LCD_ROWS);
  if (status) // non zero status means it was unsuccesful
  {
    // hd44780 has a fatalError() routine that blinks an led if possible
    // begin() failed so blink error code using the onboard LED if possible
    hd44780::fatalError(status); // does not return
  }

  // initalization was successful, the backlight should be on now
}



void Serialprintvalues() {
  Serial.println("pm1    = " + String(pm1));
  Serial.println("pm2.5  = " + String(pm2_5));
  Serial.println("pm10   = " + String(pm10));
  Serial.println("pn300  = " + String(pn300));
  Serial.println("pn500  = " + String(pn500));
  Serial.println("pn1000 = " + String(pn1000));
  Serial.println("pn2500 = " + String(pn2500));
  Serial.println("pn5000 = " + String(pn5000));
  Serial.println("pn10000= " + String(pn10000));

  Serial.print(F("Temperature      = "));
  Serial.print(temperature);
  Serial.println(F(" *C"));

  Serial.print(F("Pressure         = "));
  Serial.print(pressure);
  Serial.println(F(" hPa"));

  Serial.print(F("Humidity         = "));
  Serial.print(RH);
  Serial.println(F(" %"));

  Serial.print(F("Gas resistance   = "));
  Serial.print(gas_resistance);
  Serial.println(F(" KOhms"));

  Serial.print(F("Approx. Altitude = "));
  Serial.print(altitude);
  Serial.println(F(" m"));
}

void lcdprintvalue(int scroll = 0) {
  String labels[] = {"PM1 (Standard)",   "PM2.5 (Standard)", "PM10 (Standard)",
                     "Particles >300nm", "Particles >500nm", "Particles >1um",
                     "Particles >2.5um", "Particles >5um",   "Particles >10um",
                     "Temperature",      "Pressure",         "Humidity (RH)",
                     "Gas Resistance",   "Altitude"

  };
  String units[] = {"ug/m3", "ug/m3", "ug/m3", "/0.1L", "/0.1L",
                    "/0.1L", "/0.1L", "/0.1L", "/0.1L", "C",
                    "hPa",   "%",     "Ohms",  "Metres"};
  float values[] = {pm1,      pm2_5,  pm10,           pn300,   pn500,
                    pn1000,   pn2500, pn5000,         pn10000, temperature,
                    pressure, RH,     gas_resistance, altitude};

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(labels[scroll]);
  lcd.setCursor(0, 1);
  lcd.print(values[scroll]);
  lcd.setCursor(String(values[scroll]).length() + 1, 1);
  lcd.print(units[scroll]);
  // lcd.print(pn300);

}

void takeReadings(){
// BME680
  unsigned long endTime = bme.beginReading();

  if (endTime == 0) {
    Serial.println(F("Failed to begin reading :("));
    return;
  }

  // Serial.print(F("Reading started at "));
  // Serial.print(millis());
  // Serial.print(F(" and will finish at "));
  // Serial.println(endTime);
  // Serial.println(F("You can do other work during BME680 measurement."));
  delay(50); // This represents parallel work.
  // There's no need to delay() until millis() >= endTime: bme.endReading()
  // takes care of that. It's okay for parallel work to take longer than
  // BME680's measurement time.

  // Obtain measurement results from BME680. Note that this operation isn't
  // instantaneous even if milli() >= endTime due to I2C/SPI latency.
  if (!bme.endReading()) {
    Serial.println(F("Failed to complete BME680 reading :("));
    return;
  }

  temperature = bme.temperature;
  pressure = bme.pressure / 100.0;
  RH = bme.humidity;
  gas_resistance = bme.gas_resistance / 1000.0;
  altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
  // Serial.print(F("Reading completed at "));
  // Serial.println(millis());

  //   PMS5003
  pms.poll();
  delay(20);
  pms.read();

  pm1 = pms.getPM1_STD();
  pm2_5 = pms.getPM2_5_STD();
  pm10 = pms.getPM10_STD();
  pn300 = pms.getCntBeyond300nm();
  pn500 = pms.getCntBeyond500nm();
  pn1000 = pms.getCntBeyond1000nm();
  pn2500 = pms.getCntBeyond2500nm();
  pn5000 = pms.getCntBeyond5000nm();
  pn10000 = pms.getCntBeyond10000nm();
}

void loop() {
  

  //// DISPLAY
  //Serialprintvalues();

  // Print a message to the LCD

  if (millis() > sleeptimer) {
      lcd.off();
      delay(20);
 //     lcd.on();
//      delay(20);

  }
  //int g = digitalRead(butpin);
  //Serial.println(g);

  //if (butstate == 1 and digitalRead(butpin)==0 and millis()-prevbut>500){
  if (butstate == 1 and prevbutstate==0 and millis()-prevbut>200){
    lcd.on();
    delay(100);
    sleeptimer=millis()+300000;//20000;
    //Serial.println("butpress!!!");
    prevbutstate=butstate;
    prevbutstate=1;
    prevbut=millis();
    lcdscroll+=1;
    if (lcdscroll > 13) {
      lcdscroll = 0;
    }
    lcdprintvalue(lcdscroll);

  }
if (digitalRead(butpin)==1){
  butstate=0;
}
if (butstate==0 and prevbutstate==1){
prevbutstate=0;
}
if (readingtimer < millis() ){
  takeReadings();
  readingtimer=millis()+5000;
  //lcdprintvalue(lcdscroll);
  if (millis() < sleeptimer){
    lcdprintvalue(lcdscroll);
  }
}





  





}
