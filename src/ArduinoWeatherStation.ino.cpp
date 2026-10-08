# 1 "/var/folders/32/539xn9y15y55dj9chf6r7kl40000gn/T/tmp90s2rkv4"
#include <Arduino.h>
# 1 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"

#include "version.h"





#ifdef ENABLE_HARDWARE_SIMULATION
# 20 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
#endif



#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/eeprom.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Math.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <utility/W5100.h>


#include <Time.h>
#include <TimeLib.h>
#include <DS3232RTC.h>
#include <SdFat.h>
#include <SparkFunBME280.h>
#include <Adafruit_INA219_5A.h>


#include "pins.h"
#include "Marshall.h"
#include "wxSerial.h"
#include "renogy.h"






const String wxVersion = VERSION_ID;
const bool enableEthDump2Serial = false;
const String startupMessage = "UM Weather Station (ver" VERSION_ID ")";
const byte wifiStartupDelay = 50;
const int EthStartupDelay = 5000;
int minutesBeforeSunrise = 70;
int minutesAfterSunset = 30;
const unsigned int waitTimeIncomingClient = 8;
byte uploadRetryNum = 1;
float battery_ma_offset = 30.0;



#if BATTERY_TYPE == 'F'
  float battery_critical_voltage = 13.0;
#elif BATTERY_TYPE == 'A'
  float battery_critical_voltage = 11.8;
#endif






#define logOneLine(line) Serial.println(line);
#define logOneLine2(line,base) Serial.println(line,base);
#define logSome(line) Serial.print(line);


Adafruit_INA219_5A ina219a_solar(ina219a_solar_HWaddr);
Adafruit_INA219_5A ina219b_battery(ina219b_battery_HWaddr);
BME280 bme280a;
BME280 bme280b;
# 121 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
const int eeHardwareId = 75;
const int eeWatchdog = 76;
const int eeWatchdogTime = 77;
const int eeKeepUbiOn = 81;


const int eeWatchdogCounter = 92;
const int eeNightPBootCounter = 94;
const int eeTimeZone = 95;
const int eeCamStatus = 96;
const int eeMinutesBeforeSunrise = 100;
const int eeMinutesAfterSunset = 101;
const int eeVoltsLowestSeen = 102;
const int eeVoltsLowestDay = 103;
const int eeUploadRetryNum = 104;

byte hardwareId = 255;
String hwVersionForUpload;

unsigned int eeUIntTemp = 0;
byte eeByteTemp = 0;
char eeCharTemp = 0;

struct structCamStatus {
  bool SouthDesireOn : 1;
  bool NorthDesireOn : 1;
  bool BrainDesireOn : 1;
  bool badWeather : 1;
  bool godMode : 1;
  byte padding : 3;
};

structCamStatus camStatus;
# 181 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
const byte rtcWindSpeed = 0x0B;



struct RTCmem {
  byte windSpeed : 4;
  unsigned int batt_mAh : 11;
  unsigned int batt_lastminute : 9;
};





unsigned long lastSecond;
unsigned long loopCounter;
unsigned long loopDelta;
unsigned long msTemp;
unsigned long usTemp;
byte seconds;
byte minutes;
byte minutes_10m;
byte minutes_5m;
byte hours;
int days;
int sunrise;
int sunset;
int minutesToday;
byte telnetSeconds = 0;
byte sunriseDay = 0;
byte lastRealMinute;
bool justBooted = true;
bool justRestarted = true;
bool ethEnabled = false;
bool wifiEnabled = false;
bool keepUbiquitiOn = false;
bool pauseSolar = false;
byte pauseSolarMinutes = 2;
byte resumeSolarMinutes = 3;
float pauseSolarChargeCurrent;
unsigned long wifiStartTime = 0;
time_t pauseSolarStartTime;
time_t resumeSolarStartTime;
time_t uploadPending = 0;
time_t reportWatchdog = 0;
time_t recentTime = 0;
time_t lastCrashTime = 0;
time_t lastUploadTime = 0;
time_t ntp_time_temp;
time_t rtc_time_temp;
bool rtc_got_update_from_ntp = false;
const char charComma = ',';

bool telnet_at_startup = false;
bool rtc_available = true;
char versionDateBuf[] = VERSION_DATE;
  const int version_year = atoi(strtok(versionDateBuf, "/"));


bool shut_down_flag = false;
bool battery_critical = false;
bool night_time = false;

long lastWindCheck = 0;
int minuteWindClicks = 0;
volatile long lastWindIRQ = 0;
volatile byte windClicks = 0;
# 278 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
float windSpeedAvg = 0;
float windSpeedMinuteSum = 0;
byte windSpeedMinuteCount = 0;

#define WIND_DIR_AVG_SIZE 60
#define VOLTAGE_AVG_SIZE 10
int winddiravg[WIND_DIR_AVG_SIZE];
float windgust_10m[10];
float windgust_5m[5];
int windgustdirection_10m[10];
int windgustdirection_5m[5];



int winddir = 0;
unsigned int winddirRaw = 0;
float windspeedmph = 0;
float windgustmph = 0;
int windgustdir = 0;
int winddir_avg2m = 0;
float windgustmph_10m = 0;
float windgustmph_5m = 0;
float windmax_1m = 0;
float windmin_1m = 0;
int windgustdir_10m = 0;
int windgustdir_5m = 0;
String strWindDir = "ERR";

float humidityOutside = 0.0;
float humidityInside = 0.0;
float tempf = 0;


float pressure = 0;
float oldPressure = 0;
float pres5min[5];


float batt_lvl = 11.8;
float light_lvl = 455;

float battDrainmA = 0;
int battDrainMinutes = 0;



const float camDrainMaThreshold = -120.0f;
const int camDrainMinutesToShutoff = 15;
const float camDrainmAShutoff = -18000.0f;
const float camDrainmAAllowTurnOn = -3000.0f;
const int camDrainMinutesForLowV = 10;
#if BATTERY_TYPE == 'F'
  const float camLowVoltageShutoff = 13.15f;
#else
  const float camLowVoltageShutoff = 12.5f;
#endif
const float camSolarMaWeak = 250.0f;
const int camSolarDeficitMinutes = 12;
const int camGodModeStartHour = 10;


const int ina219a_solar_MMAcount = 512;
float ina219a_solar_volts;
float ina219a_solar_ma;
float ina219a_solar_MMAcurrentSum;
float ina219a_solar_MMAcurrentAvg;
float ina219a_solar_MMAvoltSum = 14.0*ina219a_solar_MMAcount ;
float ina219a_solar_MMAvoltAvg;


const int ina219b_battery_MMAcount = 512;
float ina219b_battery_volts;
float ina219b_battery_ma;
float ina219b_battery_MMAcurrentSum;
float ina219b_battery_MMAcurrentAvg;
float ina219b_battery_MMAvoltSum = 14.0*ina219b_battery_MMAcount;
float ina219b_battery_MMAvoltAvg;


float shuntvoltage = 0;
float busvoltage = 0;
float current_mA = 0;
float loadvoltage = 0;
float ina219_MMAtemp;
float voltsLowestSeen = 20.0;
unsigned int ina219a_solar_MMAmillis = 0;
unsigned int ina219a_solar_MMAloops = 0;
# 377 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
struct wxCache_struct {
  byte gust : 4;
  byte wd : 4;
  byte pres1 : 8;
  byte temp2 : 8;
  byte vBatt : 8;
  unsigned int aBatt : 10;
  byte ws : 6;
  byte humidIn : 5;
  byte sent : 1;

};

time_t wxCache_time;
byte wxCache_count;
byte wxCache_lastSaved;
byte wxCache_lastSent;
byte wxSendInterval = 5;
#define WX_CACHE_MAX 60

wxCache_struct wxCache[WX_CACHE_MAX];



String wxStringCache[10];
byte wxCacheSlotMinute[10];
byte wxCacheSlotHour[10];
const byte wxCacheSlotInvalid = 255;
String tempWeatherString;
String returnStatus;
# 417 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
IPAddress ip(192, 168, IPq3, IPWX);
IPAddress dnsServer(1, 1, 1, 1);
IPAddress gateway(192, 168, IPq3, IPgw);
IPAddress subnet(255, 255, 255, 0);
EthernetClient client;
EthernetClient incomingClient;
EthernetServer server(23537);
EthernetUDP Udp;




unsigned long ethLastMillis = 0;
unsigned long elapsedMillis = 0;
const int ETH_TIMEOUT = 6000;
byte ethTimeouts = 0;
byte ethConnFails = 0;
int ethLastFailureCode = 0;
uint8_t ethSockStatus[MAX_SOCK_NUM];
int timeZone;
int timeZoneTemp;
int timeZoneDefault = -8;
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[ NTP_PACKET_SIZE ];

unsigned long msNTPrequest;
# 456 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
#define sdErr(msg) sd.errorPrint(F(msg))
SdFat sd;
SdFile file;
unsigned int sdPosition;
const uint8_t chipSelect = 4;
# 487 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
void wspeedIRQ();
void enableWatchdog();
void yield();
void setup();
void loop();
void saveWeatherToCache(const String& weatherString);
void finishBootAndCacheWeather();
bool isValidWeatherString(const String& weatherString);
bool shouldUploadCacheSlot(byte slot, byte expectedMinute, byte expectedHour, const String& weatherString);
static byte expectedHourForMinute(byte uploadMinute, byte expectedMinute);
static void tryUploadCacheSlot(byte slot, byte expectedMinute, byte expectedHour, byte& uploadStatus);
void uploadCachedWeather(byte uploadMinute, byte& uploadStatus);
static void printUploadPutLine(const char* charPut, int length);
static int parseHttpStatusCode(const char* response, size_t len);
static void printUploadFailureBody(byte status, int detail);
byte uploadWeather(String WeatherString);
String getWeatherString();
void put_windspeed(byte m, byte windSpeed);
byte get_windspeed(byte m);
void put_windgust(byte m, byte windGust);
byte get_windgust(byte m);
void put_winddir(byte m, int wd);
int get_winddir(byte m);
void put_pres1(byte m, float p);
float get_pres1(byte m);
void put_temp2f(byte m, float t);
float get_temp2f(byte m);
void put_temp2c(byte m, float c);
float get_temp2c(byte m);
void put_humidIn(byte m, float h);
float get_humidIn(byte m);
void put_vBatt(byte m, float v);
float get_vBatt(byte m);
void put_aBatt(byte m, float a);
int get_aBatt(byte m);
void sdLogData(char *fileName, String logData);
void sdReadFileToSerial(char *fileName);
void sdReadFileToSocket(char *fileName);
void sdDateTime(uint16_t* sd_date, uint16_t* sd_time);
void wxLogBlank();
void wxLogRule();
void wxLogSection(const __FlashStringHelper* title);
void wxLogTag(const __FlashStringHelper* tag, const __FlashStringHelper* msg);
void wxLogTag(const __FlashStringHelper* tag, const String& msg);
void wxLogTag(const __FlashStringHelper* tag, const char* msg);
void wxLogTagNum(const __FlashStringHelper* tag, const __FlashStringHelper* label, long value);
void wxLogTagFloat(const __FlashStringHelper* tag, const __FlashStringHelper* label, float value, uint8_t decimals);
bool resolveCssServerIp();
IPAddress getCssServerIp();
bool checkEthIncomingData();
void setKeepUbiquitiOn(bool v);
void ethernetPowerOn();
void ethernetPowerOff();
void enableEthernet(bool quiet);
void resetEthernet();
static uint8_t athenaEepromByte(uint16_t addr);
static void printAthenaIpv4(uint16_t start);
static void printAthenaMac(uint16_t start);
static bool athenaIpv4Matches(uint16_t start, IPAddress addr);
static bool athenaMacMatches(uint16_t start, const byte* expected);
void printAthenaNetEeprom();
void disableEthernet(bool quiet);
void enableWifi(bool quiet);
void waitForWifi();
void disableWifi(bool quiet);
void PrintSpiPinMode();
uint8_t getPinMode(uint8_t pin);
void getRiseSet();
boolean CheckDST();
int getTimeZone(void);
time_t getNtpTime();
void sendNtpPacket(IPAddress &address);
void compareRTCwithNTP();
bool isTimeValid(time_t time_to_check);
bool setArduinoTimeWithNtp();
String getDateWithZeros();
String getDateWithZerosNoSeparator();
String getTimeWithZeros();
String strMinutesToHHMM(int M);
String time_t_to_datetime_string(time_t tt);
String makeStationDefinesSuffix();
size_t makeUploadWeatherPut(char* buf, size_t bufSize, const String& wxString, byte uploadRetries);
void accumulateWindSpeedSample(float speed);
void finalizeWindSpeedMinute();
void delayWithWdt(unsigned long ms);
float get_wind_speed();
int get_wind_direction();
void ShowSockStatus();
int freeRam ();
void printDigits(int digits);
void handleSerial();
void enableSolar();
void disableSolar();
void enableCamBrain();
void disableCamBrain();
void enableCamNorth();
void disableCamNorth();
void enableCamSouth();
void disableCamSouth();
void writeHardwareId(byte id);
void initializeEEPROM();
void goToSleep();
void calcWeather();
void printWeather();
float ws85ParseVolts(const String &val);
void ws85ResetFrame();
void ws85MarkFrameReady();
void ws85PrintReading();
void ws85PrintReading();
void ws85ParseLine(const String &line);
void ws85Init();
void ws85Poll();
bool ws85Fresh(unsigned long maxAgeMs);
bool ws85ConsumeFrame();
float ws85SpeedMph();
float ws85GustMph();
int ws85Direction();
float ws85TempC();
float ws85RainMm();
float ws85CapVoltage();
float ws85BatVoltage();
void ws85LogVoltage();
void ws85LogVoltageAtBoot();
#line 487 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
void wspeedIRQ()

{
    if (millis() - lastWindIRQ > 25)
    {
        lastWindIRQ = millis();
        windClicks++;
    }
}
# 511 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
ISR(WDT_vect)
{


    EEPROM.get(eeWatchdogTime, lastCrashTime);
    if (recentTime > lastCrashTime + 600) {
      EEPROM.put(eeWatchdog, 1);
      EEPROM.put(eeWatchdogTime, recentTime);
    }


    EEPROM.get(eeWatchdogCounter, eeUIntTemp);
    EEPROM.put(eeWatchdogCounter, eeUIntTemp + 1);
# 532 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
    while(true);
}


void enableWatchdog()
{
  cli();
  MCUSR &= ~(1<<WDRF);
  wdt_reset();
  WDTCSR |= (1<<WDCE) | (1<<WDE);
  WDTCSR = (~(1<<WDP1) & ~(1<<WDP2)) | ((1<<WDE) | (1<<WDIE) | (1<<WDP3) | (1<<WDP0));
  sei();
}



void yield()
{
  wdt_reset();
}
# 563 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
void setup()
{
  Serial.begin(115200);
  wxLogSection(F("BOOT"));
  Serial.print(startupMessage);
  Serial.print(F(" git="));
  Serial.println(VERSION_COMMIT);
  Serial.print(F("  started at "));
  Serial.print(millis());
  Serial.println(F(" ms"));

  wxLogSection(F("CONFIG"));
  Serial.print(F("  owner=")); Serial.println(wxOwner);
  Serial.print(F("  hardwareVersion=")); Serial.println(hardwareVersion);
  Serial.print(F("  batteryType='"));
  Serial.print((char)BATTERY_TYPE);
  Serial.println(F("' ('F' = LiFePo, 'A' = AGM)"));
  Serial.print(F("  dataSubfolder=")); Serial.println(wxBetaText);
  Serial.print(F("  webSite=")); Serial.println(wxSiteName);
  Serial.print(F("  uploadPath=")); Serial.println(wxUploadPath);
  Serial.print(F("  ip=192.168.")); Serial.print(IPq3, DEC); Serial.print(F(".")); Serial.println(IPWX, DEC);
  Serial.print(F("  gateway=192.168.")); Serial.print(IPq3, DEC); Serial.print(F(".")); Serial.println(IPgw, DEC);
  Serial.print(F("  mac="));
  for (byte i = 0; i < 6; i++) {
    if (mac[i] < 16) Serial.print(F("0"));
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(F(":"));
  }
  Serial.println();
  Serial.print(F("  ntpServer="));
  Serial.print(timeServer[0], DEC); Serial.print(F("."));
  Serial.print(timeServer[1], DEC); Serial.print(F("."));
  Serial.print(timeServer[2], DEC); Serial.print(F("."));
  Serial.println(timeServer[3], DEC);
  Serial.print(F("  localPort=")); Serial.println(localPort);
#ifdef ANEMO_WS85
  Serial.println(F("  ANEMO_WS85"));
#endif
#ifdef DONT_SLEEP
  Serial.println(F("  DONT_SLEEP"));
#endif
#ifdef BENCH_MODE
  Serial.println(F("  BENCH_MODE"));
#endif
#ifdef ENABLE_HARDWARE_SIMULATION
  Serial.println(F("  ENABLE_HARDWARE_SIMULATION"));
#endif
  if (uploadRetryNum > 0) {
    Serial.print(F("  uploadRetryNum=")); Serial.println(uploadRetryNum);
  }

  printAthenaNetEeprom();



  enableWatchdog();


  initializeEEPROM();
  Serial.print(F("  hardwareId="));
  if (hardwareId == 255) {
    Serial.println(F("(unset)"));
  } else {
    Serial.println(hardwareId);
  }
  Serial.print(F("  hwVersionForUpload="));
  Serial.println(hwVersionForUpload);

  pinMode(STAT1, OUTPUT);

#ifndef ANEMO_WS85
  pinMode(WSPEED, INPUT);
#endif

#ifdef ANEMO_WS85
  ws85Init();
  Serial.print(F("WS85 wind sensor on Serial1 @ "));
  Serial.print(WS85_BAUD);
  Serial.println(F(" baud"));
#endif

  pinMode(PIN_CamNorth_POWER, OUTPUT);
  pinMode(PIN_CamSouth_POWER, OUTPUT);
  pinMode(PIN_CamBrain_POWER, OUTPUT);

  disableEthernet(true);

  wxLogSection(F("SENSORS"));

  Serial.print(F("  INA219a solar: ")); usTemp = micros();
  ina219a_solar.begin();
  ina219a_solar.setCalibration_32V_5A();
  Serial.print(micros() - usTemp); Serial.println("us.");

  Serial.print(F("  INA219b battery: ")); usTemp = micros();
  ina219b_battery.begin();
  ina219b_battery.setCalibration_16V_5A();
  Serial.print(micros() - usTemp); Serial.println("us.");


  Serial.print(F("  BME280a external: status 0x")); usTemp = micros();
  bme280a.settings.commInterface = I2C_MODE;
  bme280a.settings.I2CAddress = bme280a_HWaddr;
  bme280a.settings.runMode = 3;
  bme280a.settings.tempOverSample = 1;
  bme280a.settings.pressOverSample = 1;
  bme280a.settings.humidOverSample = 1;
  Serial.print(bme280a.begin(), HEX);
  Serial.print(", took "); Serial.print(micros() - usTemp); Serial.println("us.");


  Serial.print(F("  BME280b internal: status 0x")); usTemp = micros();
  bme280b.settings.commInterface = I2C_MODE;
  bme280b.settings.I2CAddress = bme280b_HWaddr;
  bme280b.settings.runMode = 3;
  bme280b.settings.tempOverSample = 1;
  bme280b.settings.pressOverSample = 1;
  bme280b.settings.humidOverSample = 1;
  Serial.print(bme280b.begin(), HEX);
  Serial.print(F(", took ")); Serial.print(micros() - usTemp); Serial.println(F("us."));

  renogyInit();
  Serial.println(F("  Renogy CC on Serial2 @ 9600 baud, modbus addr 255"));

  seconds = 0;
  lastSecond = millis();



  #if !defined(SIMULATE_WIND_SPEED) && !defined(ANEMO_WS85)
    attachInterrupt(digitalPinToInterrupt(WSPEED), wspeedIRQ, FALLING);
  #endif




  interrupts();

  Serial.print(F("[BOOT] ready at "));
  Serial.print(millis());
  Serial.println(F(" ms"));

  loopCounter = 0;
  loopDelta = 0;

  for (byte i = 0; i < 10; i++) {
    wxStringCache[i] = "";
    wxCacheSlotMinute[i] = wxCacheSlotInvalid;
    wxCacheSlotHour[i] = wxCacheSlotInvalid;
  }





  if (EEPROM.read(eeWatchdog)) {




    EEPROM.get(eeWatchdogTime, reportWatchdog);


    Serial.println();
    Serial.print(F("Watchdog caused the last reboot, at: "));
    Serial.print(year(reportWatchdog));
    Serial.print("/");
    Serial.print(month(reportWatchdog));
    Serial.print("/");
    Serial.print(day(reportWatchdog));
    Serial.print(charComma);
    Serial.print(hour(reportWatchdog));
    Serial.print(":");
    Serial.print(minute(reportWatchdog));
    Serial.print(":");
    Serial.print(second(reportWatchdog));
    Serial.print(" ("); Serial.print(reportWatchdog); Serial.print(")");
    Serial.println();

    if (reportWatchdog > 4294000000) {

      Serial.println("Resetting eeprom reset-time to zero because it was too high");
      EEPROM.put(eeWatchdogTime, 0.0F);
    }
  }


  wdt_reset();

  #ifndef SIMULATE_RTC

  wxLogSection(F("TIME"));
  timeZone = - (int)EEPROM.read(eeTimeZone);
  Serial.print(F("  timezone EEPROM: ")); Serial.println(timeZone);
  if ((timeZone < -8) or (timeZone > -7)){
    timeZone = timeZoneDefault;
    Serial.print("EEPROM time zone not valid. Using default time zone = "); Serial.println(timeZone);
    EEPROM.put(eeTimeZone, (byte)(-timeZone));
  }

  rtc_time_temp = RTC.get();
  Serial.print(F("  RTC: ")); Serial.println(time_t_to_datetime_string(rtc_time_temp));

  if ((rtc_time_temp > 0) and (isTimeValid(rtc_time_temp))) {

    rtc_available = true;
    wxLogTag(F("TIME"), F("using RTC; NTP sync on first upload"));
    setSyncProvider([](){return RTC.get();});
    setSyncInterval(300);
    recentTime = now();
  } else {

    rtc_available = false;
    Serial.println(F("RTC malfunction! Using NTP instead. Starting Wifi to contact NTP server..."));
    enableWifi();
    waitForWifi();
    enableEthernet();
    if(!setArduinoTimeWithNtp()){
      Serial.println("Fatal error! No time information from any source! Weather station will be available via telnet for 10 minutes, then shut down.");
   for (int i = 0; i < 600; i++){
        wdt_reset();
        if (checkEthIncomingData()) break;
        delay(1000);
      }
   goToSleep();
    }
    Serial.print("Time succesfully retrieved via NTP: "); Serial.print(getDateWithZeros()); Serial.print(" "); Serial.println(getTimeWithZeros());

    RTC.set(getNtpTime());

  }

  timeZone = getTimeZone();
  timeZoneTemp = - (int)EEPROM.read(eeTimeZone);
  EEPROM.update(eeTimeZone, (byte)(-timeZone));
  if (timeZone != timeZoneTemp){
    Serial.print("Change in time zone deteced, from "); Serial.print(timeZoneTemp); Serial.print(" to "); Serial.println(timeZone);
    if (rtc_available) {
      rtc_time_temp = RTC.get();
      Serial.print("Adusting RTC:");
      if (timeZone == -7){
        Serial.println("Subtracting one hour.");
        rtc_time_temp -= 3600;
      }
      if (timeZone == -8) {
        Serial.println("Adding one hour.");
        rtc_time_temp += 3600;
      }
      RTC.set(rtc_time_temp);
    } else {
        setArduinoTimeWithNtp();
    }
  }
  #else
    setTime(SIMULATE_RTC);
    Serial.println("Simulating RTC. Simulated time is:");
    Serial.print(getDateWithZeros()); Serial.print(" "); Serial.println(getTimeWithZeros());
    Serial.println();
  #endif


#ifndef BENCH_MODE
  disableEthernet();
  disableWifi();
#endif


  Serial.println(F("  sun times:"));
  getRiseSet();







#ifndef BENCH_MODE

  minutesToday = hour() * 60 + minute();
  if ((minutesToday < sunrise - minutesBeforeSunrise)
  or (minutesToday > sunset + minutesAfterSunset)){
    if (wxDontSleep) {
      Serial.println(F("[BOOT] night (skipping sleep)"));
    } else {
      Serial.print(F("[BOOT] night - sleeping until "));
      Serial.print((sunrise - minutesBeforeSunrise) / 60); Serial.print(F(":")); Serial.println((sunrise - minutesBeforeSunrise) % 60);
    }
    goToSleep();
  }
#endif





#ifdef ANEMO_WS85
  windSpeedAvg = 0;
#elif !defined(SIMULATE_RTC)
  windSpeedAvg = RTC.readRTC(rtcWindSpeed);
#else
  windSpeedAvg = 10.0;
#endif

#ifdef BENCH_MODE
  enableEthernet();
#endif


  #ifdef TELNET_AT_STARTUP
    telnet_at_startup = true;
    Serial.println("TELNET_AT_STARTUP flag is set.");
  #else
    pinMode(PIN_TELNET_AT_STARTUP, INPUT);
    digitalWrite(PIN_TELNET_AT_STARTUP, HIGH);
    delay(50);
    telnet_at_startup = !digitalRead(PIN_TELNET_AT_STARTUP);
    if (telnet_at_startup) Serial.println("PIN_TELNET_AT_STARTUP is set (pulled to GND).");
  #endif

#ifdef ANEMO_WS85
  ws85LogVoltageAtBoot();
#endif

  if (telnet_at_startup){
    Serial.println("Standard measurement loop will be suspended.");
    enableWifi();
    waitForWifi();
    enableEthernet();
    while (true){
      Serial.println("Telnet client enabled. Waiting for incoming Telnet data...");
      while (true){
        wdt_reset();
        if (checkEthIncomingData()) break;
        delay(1000);
      }
    }
  }
}
# 915 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
void loop()
{

#ifdef ANEMO_WS85
  ws85Poll();
#endif

  elapsedMillis = millis() - lastSecond;


  if( elapsedMillis >= 1000 ) {




    lastSecond += 1000 * (elapsedMillis / 1000);
    seconds += (int)(elapsedMillis / 1000);
    wdt_reset();

    if (!justBooted && lastRealMinute != minute()) {
      finalizeWindSpeedMinute();
    }


#ifdef ANEMO_WS85
    if (ws85ConsumeFrame()) {
      float currentSpeed = ws85SpeedMph();
      float currentGust = ws85GustMph();
      int currentDirection = ws85Direction();

      windspeedmph = currentSpeed;

      accumulateWindSpeedSample(currentSpeed);

      if (currentGust > windgust_10m[minutes_10m]) {
        windgust_10m[minutes_10m] = currentGust;
        windgustdirection_10m[minutes_10m] = currentDirection;
      }
      if (currentGust > windgust_5m[minutes_5m]) {
        windgust_5m[minutes_5m] = currentGust;
        windgustdirection_5m[minutes_5m] = currentDirection;
      }
      if (currentGust > windgustmph) {
        windgustmph = currentGust;
        windgustdir = currentDirection;
      }
    }
#else
    float currentSpeed;
    currentSpeed = get_wind_speed();

    windspeedmph = currentSpeed;
    int currentDirection = get_wind_direction();

    accumulateWindSpeedSample(currentSpeed);


    if(currentSpeed > windgust_10m[minutes_10m])
    {
        windgust_10m[minutes_10m] = currentSpeed;
        windgustdirection_10m[minutes_10m] = currentDirection;
    }
    if(currentSpeed > windgust_5m[minutes_5m])
    {
        windgust_5m[minutes_5m] = currentSpeed;
        windgustdirection_5m[minutes_5m] = currentDirection;
    }


    if(currentSpeed > windgustmph)
    {
        windgustmph = currentSpeed;
        windgustdir = currentDirection;
    }
#endif




#ifndef BENCH_MODE
    if (wifiStartTime) {
      if (not (int((millis() - wifiStartTime) / 1000) % 10)) {
        Serial.print(F("[NET] wifi starting... "));
        Serial.print((millis() - wifiStartTime) / 1000,10);
        Serial.println(F("s"));
      }
      if ((millis() - wifiStartTime) / 1000 > wifiStartupDelay) {
        wxLogTag(F("NET"), F("wifi ready"));
        wifiStartTime = 0;
        wifiEnabled = true;
      }
    }
#endif



    if(seconds > 59)
    {
      seconds = seconds % 60;
      if(++minutes > 59)
      {
        minutes = 0;
        if(++hours > 23)
        {
          hours = 0;
          ++days;
        }
      }
      if(++minutes_10m > 9) minutes_10m = 0;
      if(++minutes_5m > 4) minutes_5m = 0;



      windgust_10m[minutes_10m] = 0;
      windgust_5m[minutes_5m] = 0;


      if (hour() == 11 and minute() >= 49) {
        enableWifi();
        enableEthernet();
      }
# 1044 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
      bool shouldAutoEnableCams = false;
      if (camStatus.godMode) {

        shouldAutoEnableCams = (hour() >= camGodModeStartHour)
          and not (camStatus.badWeather);
      } else {

        shouldAutoEnableCams = (minutesToday > sunrise) and (minutesToday < sunset - 120)
         and (((ina219a_solar_ma > 500) and (ina219a_solar_volts > 14))
          or (ina219a_solar_volts > 17.5))
         and not (camStatus.badWeather)
         and (battDrainmA > camDrainmAAllowTurnOn);
      }

      if (shouldAutoEnableCams) {
        static bool camPowerMsgShown = false;
        static bool camGodModeMsgShown = false;
        if (camStatus.godMode) {
          if (!camGodModeMsgShown) {
            wxLogTag(F("CAM"), F("god mode - cameras + continuous WiFi (10am until sleep)"));
            camGodModeMsgShown = true;
          }
          camPowerMsgShown = false;
        } else if (!camPowerMsgShown) {
          wxLogTag(F("CAM"), F("daytime power OK - cameras + continuous WiFi"));
          camPowerMsgShown = true;
          camGodModeMsgShown = false;
        }
        keepUbiquitiOn = true;
        EEPROM.update(eeKeepUbiOn, true);
        enableWifi();
        if (humidityInside < 80) {

          enableCamNorth();
        } else {
          wxLogTag(F("CAM"), F("north skipped (high humidity)"));
        }
        enableCamSouth();
      }


      if (not camStatus.godMode) {

        bool seriousDrainMinute = (ina219b_battery_ma < camDrainMaThreshold);
        bool weakSolar = (ina219a_solar_ma < camSolarMaWeak);
        bool charging = ((ina219b_battery_ma > 50) or (ina219a_solar_volts > 16));

        if (seriousDrainMinute) {
          if (battDrainMinutes < 0) { battDrainMinutes = 0; }
          battDrainMinutes += 1;

          bool shutoffCams = false;
          if (battDrainMinutes >= camDrainMinutesToShutoff) shutoffCams = true;
          if (battDrainmA < camDrainmAShutoff) shutoffCams = true;
          if ((ina219b_battery_volts < camLowVoltageShutoff) and (battDrainMinutes >= camDrainMinutesForLowV)) shutoffCams = true;
          if (weakSolar and (battDrainMinutes >= camSolarDeficitMinutes)) shutoffCams = true;

          if (shutoffCams) {
            wxLogTag(F("CAM"), F("off - excessive battery drain"));
            disableCamSouth();
            disableCamNorth();
            disableCamBrain();
            keepUbiquitiOn = false;
            EEPROM.update(eeKeepUbiOn, false);
          }
        } else if (charging) {
          if (battDrainMinutes > 0) { battDrainMinutes = 0; }
          battDrainMinutes -= 1;
        } else {

          if (battDrainMinutes > 0) { battDrainMinutes -= 1; }
        }
      }
      battDrainmA += ina219b_battery_ma;




      if (not camStatus.godMode
      and (hour() == 18)
      and ((minute() > 25) or (minute() < 30)) ) {
        keepUbiquitiOn = false;
        EEPROM.update(eeKeepUbiOn, false);
        disableCamNorth();
        disableCamSouth();
        disableCamBrain();
      }


#ifndef BENCH_MODE





      shut_down_flag = false;


      minutesToday = hour() * 60 + minute();
      if ((minutesToday < sunrise - minutesBeforeSunrise)
      or (minutesToday > sunset + minutesAfterSunset)){
        shut_down_flag = true;
      }


      if (ina219b_battery_volts < battery_critical_voltage) {
        shut_down_flag = true;
      }

      Serial.print(F("[STATUS] "));
      Serial.print(hour()); Serial.print(F(":"));
      if (minute() < 10) Serial.print(F("0"));
      Serial.print(minute());
      if (shut_down_flag) {
        Serial.print(F(" night"));
        if (wxDontSleep) Serial.print(F(" (skipping sleep)"));
      } else {
        Serial.print(F(" day"));
      }
      Serial.print(F("  batt="));
      Serial.print(ina219b_battery_volts, 2);
      Serial.print(F("V"));
      if (shut_down_flag && ina219b_battery_volts < battery_critical_voltage) {
        Serial.print(F(" LOW"));
      }
      if (shut_down_flag && (minutesToday < sunrise - minutesBeforeSunrise)) {
        Serial.print(F("  day@"));
        Serial.print((sunrise - minutesBeforeSunrise) / 60); Serial.print(F(":"));
        Serial.print((sunrise - minutesBeforeSunrise) % 60);
      } else if (!shut_down_flag) {
        Serial.print(F("  night@"));
        Serial.print((sunset + minutesAfterSunset) / 60); Serial.print(F(":"));
        Serial.print((sunset + minutesAfterSunset) % 60);
      }
      Serial.println();


      if (shut_down_flag) {
        goToSleep();
      }
#endif
# 1194 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
      if ((ina219b_battery_ma > 2500) and ( (resumeSolarStartTime + resumeSolarMinutes * 60) <= now() )) {


        Serial.print(F("[SOLAR] pause charge "));
        Serial.print(ina219b_battery_ma, 0);
        Serial.println(F(" mA > 2500"));
        pauseSolar = true;
        pauseSolarChargeCurrent = ina219b_battery_ma;
        pauseSolarStartTime = now();
        disableSolar();

      } else {


        if (pauseSolar) {
          if ( (pauseSolarStartTime + pauseSolarMinutes * 60) <= now() ) {

            wxLogTag(F("SOLAR"), F("resume charging"));
            pauseSolarChargeCurrent = 0;
            pauseSolar = false;
            enableSolar();
            resumeSolarStartTime = now();
          }
        }
      }


    }



    calcWeather();
    byte uploadStatus = 0;

    if (justBooted) {

#ifdef BENCH_MODE
      if (seconds > 15) {
        finishBootAndCacheWeather();
        justRestarted = false;
      }
#else
      if ((seconds > 15) and (timeStatus() == timeSet)) finishBootAndCacheWeather();
#endif
    } else {
      if (lastRealMinute != minute()) {
        lastRealMinute = minute();
        tempWeatherString = getWeatherString();
        Serial.print(F("[WX] "));
        Serial.println(tempWeatherString);
        ina219a_solar_MMAloops = 0;

        bool uploadThisMinute = (minute() % 5 == 0) and (millis() > 180000);
        if (uploadThisMinute) {

          saveWeatherToCache(tempWeatherString);
        }

#ifndef BENCH_MODE

        if ((minute() % 5 == 4) and (not uploadPending)) {
          uploadPending = true;
          enableWifi();
        }
        if (uploadThisMinute) {

          uploadPending = true;
          wxLogSection(F("UPLOAD"));
          enableEthernet();
          if (ethEnabled){
            msTemp = millis();
            bool wasJustRestarted = justRestarted;
            uploadCachedWeather(minute(), uploadStatus);
            if (wasJustRestarted) {
              justRestarted = false;
            }


   if (uploadStatus==0){
    if (reportWatchdog) {
     wxLogTag(F("UP"), F("watchdog EEPROM flag cleared"));
     reportWatchdog = 0;
     EEPROM.update(eeWatchdog, 0);
    }
    ethLastFailureCode = 0;
   }


            #ifndef SIMULATE_RTC
              if (rtc_available) {
                if (!rtc_got_update_from_ntp){
                  ntp_time_temp = getNtpTime();
                  if (isTimeValid(ntp_time_temp)){
                    RTC.set(ntp_time_temp);
                    wxLogTag(F("NTP"), F("RTC updated"));
                    rtc_got_update_from_ntp = true;
                    setSyncProvider([](){return RTC.get();});
                  } else{
                    Serial.println("RTC update via NTP failed! Trying again during next data upload.");
                  }
                }
              } else {
                setArduinoTimeWithNtp();
              }
            #endif


            wxLogTag(F("NET"), F("telnet listen (8s)"));
            for (unsigned int i = 0; i <= waitTimeIncomingClient; i++){
              checkEthIncomingData();
              wdt_reset();
              delay(1000);
            }
          } else {
            ethConnFails++;
          }
          disableWifi();
          disableEthernet();
          wxLogRule();
          uploadPending = false;
        }
#else
        if (uploadThisMinute) {
          if (not ethEnabled) enableEthernet();
          if (ethEnabled) {
            msTemp = millis();
            bool wasJustRestarted = justRestarted;
            uploadCachedWeather(minute(), uploadStatus);
            if (wasJustRestarted) {
              justRestarted = false;
            }

            if (uploadStatus==0) {
              if (reportWatchdog) {
                Serial.println(F("  Clearing watchdog EEPROM flag"));
                reportWatchdog = 0;
                EEPROM.update(eeWatchdog, 0);
              }
              ethLastFailureCode = 0;
            }

            #ifndef SIMULATE_RTC
              if (rtc_available) {
                if (!rtc_got_update_from_ntp) {
                  ntp_time_temp = getNtpTime();
                  if (isTimeValid(ntp_time_temp)) {
                    RTC.set(ntp_time_temp);
                    wxLogTag(F("NTP"), F("RTC updated"));
                    rtc_got_update_from_ntp = true;
                    setSyncProvider([](){return RTC.get();});
                  } else {
                    Serial.println("RTC update via NTP failed! Trying again during next data upload.");
                  }
                }
              } else {
                setArduinoTimeWithNtp();
              }
            #endif

            wxLogTag(F("NET"), F("telnet listen (8s)"));
            for (unsigned int i = 0; i <= waitTimeIncomingClient; i++) {
              checkEthIncomingData();
              wdt_reset();
              delay(1000);
            }
          } else {
            ethConnFails++;
          }
        }
#endif
#ifdef ANEMO_WS85
        if (uploadThisMinute) {
          ws85Poll();
          ws85LogVoltage();
        }
#endif
        if (!uploadThisMinute) {
          saveWeatherToCache(tempWeatherString);
        }

      }
    }
# 1385 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
    renogyPoll();
  }



  if (ina219a_solar_MMAmillis + 12 < millis()) {

    float ina219a_solar_polarity = 1.0;

    shuntvoltage = ina219a_solar.getShuntVoltage_mV();
    busvoltage = ina219a_solar.getBusVoltage_V();
    ina219_MMAtemp = busvoltage + (shuntvoltage / 1000.0);

    if (ina219_MMAtemp < 0.0) {
      ina219_MMAtemp = ina219_MMAtemp * -1.0;
      ina219a_solar_polarity = -1.0;
    }

    ina219a_solar_MMAvoltSum -= ina219a_solar_MMAvoltAvg;
    ina219a_solar_MMAvoltSum += ina219_MMAtemp;
    ina219a_solar_MMAvoltAvg = ina219a_solar_MMAvoltSum / ina219a_solar_MMAcount;
    #ifdef SIMULATE_INA219A_SOLAR_VOLTS
    ina219a_solar_volts = SIMULATE_INA219A_SOLAR_VOLTS;
    #else
    ina219a_solar_volts = ina219a_solar_MMAvoltAvg;
    #endif

    ina219_MMAtemp = ina219a_solar.getCurrent_mA() * ina219a_solar_polarity;
    ina219a_solar_MMAcurrentSum -= ina219a_solar_MMAcurrentAvg;
    ina219a_solar_MMAcurrentSum += ina219_MMAtemp;
    ina219a_solar_MMAcurrentAvg = ina219a_solar_MMAcurrentSum / ina219a_solar_MMAcount;
    #ifdef SIMULATE_INA219A_SOLAR_MA
    ina219a_solar_ma = SIMULATE_INA219A_SOLAR_MA;
    #else
    ina219a_solar_ma = ina219a_solar_MMAcurrentAvg;
    #endif

    ina219a_solar_MMAloops++;


    shuntvoltage = ina219b_battery.getShuntVoltage_mV();
    busvoltage = ina219b_battery.getBusVoltage_V();
    ina219_MMAtemp = busvoltage + (shuntvoltage / 1000.0);

    ina219b_battery_MMAvoltSum -= ina219b_battery_MMAvoltAvg;
    ina219b_battery_MMAvoltSum += ina219_MMAtemp;
    ina219b_battery_MMAvoltAvg = ina219b_battery_MMAvoltSum / ina219b_battery_MMAcount;
    #ifdef SIMULATE_INA219B_BATTERY_VOLTS
    ina219b_battery_volts = SIMULATE_INA219B_BATTERY_VOLTS;
    #else
    ina219b_battery_volts = ina219b_battery_MMAvoltAvg;
    #endif
    if ( (ina219b_battery_volts < voltsLowestSeen)
     and (ina219b_battery_volts > 2.0) ) {
      voltsLowestSeen = ina219b_battery_volts;
    }


    ina219_MMAtemp = ina219b_battery.getCurrent_mA() + battery_ma_offset;
    ina219b_battery_MMAcurrentSum -= ina219b_battery_MMAcurrentAvg;
    ina219b_battery_MMAcurrentSum += ina219_MMAtemp;
    ina219b_battery_MMAcurrentAvg = ina219b_battery_MMAcurrentSum / ina219b_battery_MMAcount;
    #ifdef SIMULATE_INA219B_BATTERY_MA
    ina219b_battery_ma = SIMULATE_INA219B_BATTERY_MA;
    #else
    ina219b_battery_ma = ina219b_battery_MMAcurrentAvg ;
    #endif
  }


  loopCounter++;

}
# 1469 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
void saveWeatherToCache(const String& weatherString) {
  byte slot = minute() % 10;
  wxStringCache[slot] = weatherString;
  wxCacheSlotMinute[slot] = minute();
  wxCacheSlotHour[slot] = hour();
  wxCache_lastSaved = minute();
}

void finishBootAndCacheWeather() {
  justBooted = false;
  lastRealMinute = minute();
  finalizeWindSpeedMinute();
  tempWeatherString = getWeatherString();
  Serial.print(F("[WX] "));
  Serial.println(tempWeatherString);
  saveWeatherToCache(tempWeatherString);
}

bool isValidWeatherString(const String& weatherString) {
  if (weatherString.length() < 12) return false;
  if (weatherString.charAt(2) != ':') return false;
  if (weatherString.indexOf(',') < 0) return false;
  return true;
}

bool shouldUploadCacheSlot(byte slot, byte expectedMinute, byte expectedHour, const String& weatherString) {
  if (!isValidWeatherString(weatherString)) return false;
  if (wxCacheSlotMinute[slot] == wxCacheSlotInvalid) return false;
  if (wxCacheSlotHour[slot] == wxCacheSlotInvalid) return false;
  if (wxCacheSlotMinute[slot] != expectedMinute) return false;
  if (wxCacheSlotHour[slot] != expectedHour) return false;
  return true;
}

static byte expectedHourForMinute(byte uploadMinute, byte expectedMinute) {
  byte expectedHour = hour();
  if (expectedMinute > uploadMinute) expectedHour = (expectedHour + 23) % 24;
  return expectedHour;
}

static void tryUploadCacheSlot(byte slot, byte expectedMinute, byte expectedHour, byte& uploadStatus) {
  wdt_reset();
  if (shouldUploadCacheSlot(slot, expectedMinute, expectedHour, wxStringCache[slot])) {
    uploadStatus = uploadWeather(wxStringCache[slot]);
  }
}

void uploadCachedWeather(byte uploadMinute, byte& uploadStatus) {


  for (byte k = 0; k < 5; k++) {
    byte expectedMinute;
    byte slot;
    byte expectedHour;
    if (uploadMinute % 10 == 0) {
      expectedMinute = (byte)(((int)uploadMinute - 4 + k + 60) % 60);
      slot = expectedMinute % 10;
      expectedHour = expectedHourForMinute(uploadMinute, expectedMinute);
    } else {
      expectedMinute = uploadMinute - 4 + k;
      slot = 1 + k;
      expectedHour = hour();
    }
    tryUploadCacheSlot(slot, expectedMinute, expectedHour, uploadStatus);
  }
}

static void printUploadPutLine(const char* charPut, int length) {
  Serial.print(F("[UP] "));
  for (int i = 0; i < length; i++) {
    char c = charPut[i];
    if (c == '\r' || c == '\n') Serial.print(' ');
    else Serial.print(c);
  }
  Serial.println();
}

static int parseHttpStatusCode(const char* response, size_t len) {
  for (size_t i = 0; i + 8 < len; i++) {
    if (response[i] != 'H' || response[i + 1] != 'T' ||
        response[i + 2] != 'T' || response[i + 3] != 'P' || response[i + 4] != '/') {
      continue;
    }
    const char* p = response + i + 5;
    while (p < response + len && *p != ' ') {
      p++;
    }
    if (p >= response + len || *p != ' ') {
      return -1;
    }
    p++;
    int code = 0;
    bool gotDigit = false;
    while (p < response + len && *p >= '0' && *p <= '9') {
      code = code * 10 + (*p - '0');
      gotDigit = true;
      p++;
    }
    return gotDigit ? code : -1;
  }
  return -1;
}

static void printUploadFailureBody(byte status, int detail) {
  Serial.print(F("FAILED ("));
  Serial.print(status);
  Serial.print(F("): "));
  switch (status) {
    case 50: Serial.println(F("network not ready")); break;
    case 51: Serial.println(F("invalid weather string")); break;
    case 52: Serial.println(F("PUT build failed (empty w or buffer)")); break;
    case 201: Serial.println(F("DNS lookup failed")); break;
    case 200: Serial.println(F("TCP connection failed")); break;
    case 202:
      Serial.print(F("PUT write incomplete ("));
      Serial.print(detail);
      Serial.println(F(" bytes sent)"));
      break;
    case 203: Serial.println(F("no HTTP response from server")); break;
    case 204:
      Serial.print(F("HTTP error "));
      Serial.println(detail);
      break;
    case 205: Serial.println(F("unrecognized HTTP response")); break;
    default: Serial.println(F("unknown")); break;
  }
}

static void printUploadAttemptFailure(byte attempt, byte status, int detail = 0) {
  Serial.print(F("[UP] attempt "));
  Serial.print(attempt);
  Serial.print(' ');
  printUploadFailureBody(status, detail);
}

static void printUploadResult(byte status, int detail = 0) {
  if (status == 0) {
    Serial.print(F("[UP] SUCCESS"));
    if (detail > 0) {
      Serial.print(F(" HTTP "));
      Serial.print(detail);
    }
    Serial.println();
    return;
  }
  Serial.print(F("[UP] "));
  printUploadFailureBody(status, detail);
}

byte uploadWeather(String WeatherString)
{
  if (!isValidWeatherString(WeatherString)) {
    printUploadResult(51);
    return 51;
  }

  Serial.print(F("[UP] freeRam="));
  Serial.println(freeRam());

  String WeatherString2;
  WeatherString2 = WeatherString;
  if (justRestarted) {
    static bool stationDefinesSent = false;
    if (!stationDefinesSent) {
      String definesSuffix = makeStationDefinesSuffix();
      WeatherString2 += definesSuffix;
      Serial.print(F("[UP] +defines "));
      Serial.println(definesSuffix);
      stationDefinesSent = true;
    }
  }




  const size_t uploadBufSize = 512;
  char charPut[uploadBufSize];







#ifdef BENCH_MODE
  if (!ethEnabled) {
    printUploadResult(50);
    return 50;
  }
#else
  if (!wifiEnabled) {
    printUploadResult(50);
    return 50;
  }

  if (digitalRead(PIN_UBIQUITI_POWER) == UBIQUITI_OFF){
    printUploadResult(50);
    return 50;
  }

  if (digitalRead(PIN_ETH_POWER) == ETH_OFF){
    printUploadResult(50);
    return 50;
  }
#endif


  byte uploadStatus = 90;




  client.stop();
  while (client.available()) {
    wdt_reset();
    char c = client.read();
    if (enableEthDump2Serial) Serial.write(c);
  }

  uploadStatus = 100;

  int clientConnectStatus = 0;
  int writeDetail = 0;
  int httpStatusCode = 0;
  uploadStatus = 200;
  size_t strPutLength = 0;

  for (byte attempt = 0; attempt <= uploadRetryNum; attempt++) {
    if (attempt > 0) {
      Serial.print(F("[UP] retry "));
      Serial.print(attempt);
      Serial.print(F("/"));
      Serial.println(uploadRetryNum);
      client.stop();
      while (client.available()) {
        wdt_reset();
        client.read();
      }
      delayWithWdt(500);
    }

    strPutLength = makeUploadWeatherPut(charPut, uploadBufSize, WeatherString2, attempt);
    if (strPutLength == 0) {
      uploadStatus = 52;
      printUploadAttemptFailure(attempt, uploadStatus);
      continue;
    }
    printUploadPutLine(charPut, (int)strPutLength);

    client.setTimeout(600);
    wdt_reset();
    if (!resolveCssServerIp()) {
      clientConnectStatus = 0;
      uploadStatus = 201;
      client.stop();
      printUploadAttemptFailure(attempt, uploadStatus);
      continue;
    }
    clientConnectStatus = client.connect(getCssServerIp(), 80);
    wdt_reset();
    if (!clientConnectStatus) {
      ethLastFailureCode = clientConnectStatus;
      client.stop();
      uploadStatus = 200;
      printUploadAttemptFailure(attempt, uploadStatus);
      continue;
    }

    if (enableEthDump2Serial) { Serial.write(charPut, strPutLength); }
    size_t written = client.write(charPut, strPutLength);
    wdt_reset();
    if (written != (size_t)strPutLength) {
      client.stop();
      writeDetail = (int)written;
      uploadStatus = 202;
      printUploadAttemptFailure(attempt, uploadStatus, writeDetail);
      continue;
    }

    ethLastMillis = millis();

    uint32_t flushStart = millis();
    while (millis() - flushStart < 3000) {
      wdt_reset();
      if (client.availableForWrite() >= (int)W5100.SSIZE) break;
      delay(10);
    }
    delayWithWdt(200);

    char responseBuf[160];
    size_t responseLen = 0;
    bool statusLineComplete = false;
    uint32_t readStart = millis();
    while (millis() - readStart < 2500) {
      wdt_reset();
      while (client.available()) {
        ethLastMillis = millis();
        char c = client.read();
        if (enableEthDump2Serial) Serial.print(c);
        if (responseLen < sizeof(responseBuf) - 1) {
          responseBuf[responseLen++] = c;
          if (c == '\n') {
            statusLineComplete = true;
          }
        }
      }
      if (statusLineComplete) {
        break;
      }
      if (!client.connected() && !client.available()) {
        break;
      }
      delay(10);
    }
    responseBuf[responseLen] = '\0';

    if (responseLen == 0) {
      client.stop();
      uploadStatus = 203;
      printUploadAttemptFailure(attempt, uploadStatus);
      break;
    }

    httpStatusCode = parseHttpStatusCode(responseBuf, responseLen);
    if (httpStatusCode < 0) {
      client.stop();
      uploadStatus = 205;
      Serial.print(F("[UP] response "));
      printUploadPutLine(responseBuf, (int)responseLen);
      printUploadAttemptFailure(attempt, uploadStatus);
      continue;
    }
    if (httpStatusCode < 200 || httpStatusCode >= 300) {
      client.stop();
      writeDetail = httpStatusCode;
      uploadStatus = 204;
      printUploadAttemptFailure(attempt, uploadStatus, writeDetail);
      continue;
    }

    uploadStatus = 0;
    ethConnFails = 0;
    ethTimeouts = 0;
    break;
  }

  if (uploadStatus != 0) {
    ethConnFails++;
    if (uploadStatus == 204) {
      ethLastFailureCode = writeDetail;
    } else {
      ethLastFailureCode = uploadStatus;
    }
  }

  wdt_reset();
  if (uploadStatus == 0) {
    printUploadResult(uploadStatus, httpStatusCode);
  }
  return uploadStatus;
}


String getWeatherString() {
  String weatherString = "";
  byte wxMinute = minute();
  wxCache_lastSaved = wxMinute;

  renogyFinalizeMinute();


  float temperature2temp = bme280b.readTempC();
  humidityInside = bme280b.readFloatHumidity();
  float pres1temp = bme280b.readFloatPressure() / 100.0;



  if (hour() < 10) weatherString += String('0');
  weatherString += String(hour());
  weatherString += ":";
  if (minute() < 10) weatherString += String('0');
  weatherString += String(minute());


  weatherString += String(charComma);
  weatherString += String(month());
  weatherString += String(("/"));
  weatherString += String(day());
  weatherString += String(("/"));
  weatherString += String(year());


  weatherString += String(charComma);
  if (windSpeedAvg < 9.95) weatherString += String('0');
  weatherString += String(windSpeedAvg, 1);
  put_windspeed(wxMinute, windSpeedAvg);
#ifndef ANEMO_WS85

  RTC.writeRTC(rtcWindSpeed, int(windSpeedAvg + 0.5));
#endif


  weatherString += String(charComma);
  if (windgustmph_5m < 9.5) weatherString += String('0');
  weatherString += String(windgustmph_5m, 0);
  put_windgust(wxMinute, windgustmph_5m);


  weatherString += String(charComma);
  if ((winddir < 10 ) and (winddir >= 0)) weatherString += String("0");
  if ((winddir < 100) and (winddir >= 0)) weatherString += String("0");
  weatherString += String(winddir);
  put_winddir(wxMinute, winddir);


  weatherString += String(charComma);



  weatherString += String(charComma);




  weatherString += String(charComma);
  weatherString += String(renogy_solar_volts, 1);
  put_pres1(wxMinute, pres1temp);


  weatherString += String(charComma);
  weatherString += String(renogy_solar_amps, 2);


  weatherString += String(charComma);
  weatherString += String(wxOwner);




  weatherString += String(charComma);
  weatherString += String(wxVersion);


  weatherString += String(charComma);
  weatherString += String(BATTERY_TYPE);
  weatherString += hwVersionForUpload;


  weatherString += String(charComma);
  weatherString += String(temperature2temp, 2);
  put_temp2c(wxMinute, temperature2temp);


  weatherString += String(charComma);
  weatherString += String(humidityInside, 0);
  put_humidIn(wxMinute, humidityInside);


  weatherString += String(charComma);
  if (ina219a_solar_ma < 0) {

    weatherString += String("-");
    if (ina219a_solar_ma > -9.5) weatherString += "0";
    if (ina219a_solar_ma > -99.5) weatherString += "0";
    weatherString += String(ina219a_solar_ma * -1, 0);
  } else {

    if (ina219a_solar_ma < 999.5) weatherString += "0";
    if (ina219a_solar_ma < 99.5) weatherString += "0";
    if (ina219a_solar_ma < 9.5) weatherString += "0";
    weatherString += String(ina219a_solar_ma, 0);
  }


  weatherString += String(charComma);
  weatherString += String(ina219a_solar_volts, 1);



  weatherString += String(charComma);
  if (ina219b_battery_ma < 0) {

    weatherString += String("-");
    if (ina219b_battery_ma > -9.5) weatherString += "0";
    if (ina219b_battery_ma > -99.5) weatherString += "0";
    weatherString += String(ina219b_battery_ma * -1, 0);
  } else {

    if (ina219b_battery_ma < 999.5) weatherString += "0";
    if (ina219b_battery_ma < 99.5) weatherString += "0";
    if (ina219b_battery_ma < 9.5) weatherString += "0";
    weatherString += String(ina219b_battery_ma, 0);
  }
  put_aBatt(wxMinute, ina219b_battery_ma);


  weatherString += String(charComma);
  weatherString += String(ina219b_battery_volts, 2);
  put_vBatt(wxMinute, ina219b_battery_volts);


  weatherString += String(charComma);


  if (wxOwner == "L") {
    if ((hours + days * 24) < 10) weatherString += String('0');
    weatherString += String(hours + (days * 24));
    weatherString += String(":");
    if (minutes < 10) weatherString += String('0');
    weatherString += String(minutes);
  } else {

    if ((hours * 60) + minutes < 10) weatherString += String('0');
    weatherString += String((hours * 60) + minutes);
  }
  weatherString += String(":");
  if (seconds < 10) weatherString += String('0');
  weatherString += String(seconds);


  weatherString += String(charComma);
  if (keepUbiquitiOn) { weatherString += String("U"); }
  if (camStatus.SouthDesireOn) { weatherString += String("S"); }
  if (camStatus.NorthDesireOn) { weatherString += String("N"); }
  if (camStatus.BrainDesireOn) { weatherString += String("B"); }
  if (camStatus.badWeather) { weatherString += String("X"); }
  if (camStatus.godMode) { weatherString += String("O"); }
  if (telnetSeconds) {
    weatherString += String("T=");
    weatherString += String(telnetSeconds);
    telnetSeconds = 0;
  }


  weatherString += String(charComma);
  if (battDrainMinutes > 0) {
    weatherString += String(battDrainMinutes);
  }


  weatherString += String(charComma);
  weatherString += String(battDrainmA / 60.0, 1);




  if (wifiEnabled) {
    bool reportSockets = false;
    for (int i = 0; i < MAX_SOCK_NUM; i++) {
      if ((ethSockStatus[i] > 0) and (ethSockStatus[i] != 0x14)) reportSockets = true;
    }
    if (reportSockets) {
      weatherString += String(charComma);
      for (int i = 0; i < MAX_SOCK_NUM; i++) {
        if (ethSockStatus[i] < 17) weatherString += String("0");
        weatherString += String(ethSockStatus[i], 16);
      }
    }
  }

  if (pauseSolarChargeCurrent) {
    weatherString += String(charComma);
    weatherString += String("S-");
    weatherString += String(pauseSolarChargeCurrent, 0);
  }


  if (ethConnFails) {
    weatherString += String(F(",EthConnFails="));
    weatherString += String(ethConnFails);
  }
  if (ethTimeouts) {
    weatherString += String(F(",Timeouts="));
    weatherString += String(ethTimeouts);
  }
  if (ethLastFailureCode) {
    weatherString += String(F(",failCode="));
    weatherString += String(ethLastFailureCode);
  }
  if(reportWatchdog) {
    weatherString += ",W-";
    weatherString += String(year(reportWatchdog));
    weatherString += String(("/"));
    weatherString += String(month(reportWatchdog));
    weatherString += String(("/"));
    weatherString += String(day(reportWatchdog));
    weatherString += String("@");
    weatherString += String(hour(reportWatchdog));
    weatherString += String((":"));
    weatherString += String(minute(reportWatchdog));
  }


  if(justRestarted) {
    weatherString += ",R";
# 2079 "/Users/tavis/code/awx-pio/src/ArduinoWeatherStation.ino"
  }

  weatherString.replace(" ", "");
  return weatherString;
}
# 1 "/Users/tavis/code/awx-pio/src/wxCache.ino"



void put_windspeed(byte m, byte windSpeed) {
  m = m % 60;
  if (windSpeed > 63) windSpeed = 63;
  if (windSpeed <= 0) windSpeed = 0;
  wxCache[m].ws = windSpeed;
}

byte get_windspeed(byte m) {
  m = m % 60;
  return wxCache[m].ws;
}




void put_windgust(byte m, byte windGust) {
  m = m % 60;
  windGust = windGust - wxCache[m].ws;
  windGust = round(windGust / 2);
  wxCache[m].gust = windGust;
}


byte get_windgust(byte m) {
  byte tempGust;
  m = m % 60;
  tempGust = wxCache[m].gust * 2;
  tempGust = tempGust + wxCache[m].ws;
  return tempGust;
}




void put_winddir(byte m, int wd) {
  m = m % 60;
  if (wd < 0) wd = 0;
  if (wd > 359) wd = 359;
  wxCache[m].wd = (int)(wd / 22.5);
}

int get_winddir(byte m) {
  m = m % 60;
  return round(wxCache[m].wd * 22.5);
}




void put_pres1(byte m, float p) {
  byte pres1;
  m = m % 60;
  if (p < 900) {
    pres1 = 0;
  } else if (p > 1155) {
    pres1 = 255;
  } else {
    pres1 = round(p - 900);
  }

  wxCache[m].pres1 = pres1;
}

float get_pres1(byte m) {
  m = m % 60;
  return (float)(900 + wxCache[m].pres1);
}



void put_temp2f(byte m, float t) {
  m = m % 60;
  if (t > 180) t = 180;
  if (t < 0) t = 0;
  wxCache[m].temp2 = t * 1.4;
}

float get_temp2f(byte m) {
  m = m % 60;
  return wxCache[m].temp2 / 1.4;
}



void put_temp2c(byte m, float c) {
  m = m % 60;
  float f = c * 1.8 + 32.0;
  put_temp2f(m, f);
}

float get_temp2c(byte m) {
  m = m % 60;
  return (get_temp2f(m) - 32.0) / 1.8;
}



void put_humidIn(byte m, float h) {
  m = m % 60;
  if (h > 100) h = 100;
  if (h < 0) h = 0;
  h = h / 3.23;
  wxCache[m].humidIn = round(h);
}
float get_humidIn(byte m) {
  m = m % 60;
  return (wxCache[m].humidIn * 3.23);
}




void put_vBatt(byte m, float v) {
  m = m % 60;

  if (v < 0) v = v * -1.0;
  if (v < 10) v = 10;
  if (v > 15) v = 15;
  v = v - 10;
  v = v * 50;
  wxCache[m].vBatt = (byte)round(v);
}

float get_vBatt(byte m) {
  m = m % 60;
  float v = wxCache[m].vBatt;
  return v / 50 + 10;
}



void put_aBatt(byte m, float a) {
  m = m % 60;
  if (a > 3550) a = 3550;
  if (a < -500) a = -500;
  a = a + 500.0;
  wxCache[m].aBatt = round(a / 4.0);
}

int get_aBatt(byte m) {
  m = m % 60;
  return (int)wxCache[m].aBatt * 4 - 500;
}




void sdLogData(char *fileName, String logData) {

  msTemp = millis();
  usTemp = micros();
  Serial.print(F("[SD] write ")); Serial.println(fileName);

  bool sdEnabledEthernet = false;
  if (not ethEnabled) {
    sdEnabledEthernet = true;
    enableEthernet();
  }


  if (!file.open(fileName, O_CREAT | O_WRITE | O_APPEND)) {
    sdErr("file.open");
  }
  file.dateTimeCallback(sdDateTime);


  wdt_reset();
  file.print(logData);
  file.println();


  if (!file.sync() || file.getWriteError()) {
    sdErr("write error");
  }

  sdPosition = file.curPosition();
  file.close();

  if (sdEnabledEthernet) disableEthernet();

  Serial.print(F("[SD] done "));
  Serial.print(millis() - msTemp);
  Serial.print(F(" ms  pos "));
  Serial.println(sdPosition);

}

void sdReadFileToSerial(char *fileName) {

  msTemp = millis();
  wdt_reset();

  Serial.print("Dumping SD file to serial: "); Serial.println(fileName);


  if (!file.open(fileName, O_READ)) {
    sdErr("file.open for read");
  } else {
    Serial.println("File opened, begin dumping...");

    while (file.available()) {
      Serial.write(file.read());
      wdt_reset();
    }

    file.close();
  }


  file.close();

  Serial.print("  --- Done reading "); Serial.print(fileName); Serial.print(", took ");
  Serial.print(millis() - msTemp); Serial.println("ms.");

}


void sdReadFileToSocket(char *fileName) {

  msTemp = millis();
  wdt_reset();

  Serial.print("Dumping SD file to ethernet socket: "); Serial.println(fileName);

  bool sdEnabledEthernet = false;
  if (not ethEnabled) {
    sdEnabledEthernet = true;
    enableEthernet();
  }


  if (!file.open(fileName, O_READ)) {
    sdErr("file.open for read");
  } else {
    Serial.println("File opened, begin dumping...");

    while (file.available()) {
      wdt_reset();
      incomingClient.write(file.read());
    }

    file.close();
  }


  file.close();

  if (sdEnabledEthernet) disableEthernet();

  Serial.print("  --- Done reading "); Serial.print(fileName); Serial.print(", took ");
  Serial.print(millis() - msTemp); Serial.println("ms.");

}


void sdDateTime(uint16_t* sd_date, uint16_t* sd_time) {
  uint16_t sd_year;
  uint8_t sd_month, sd_day, sd_hour, sd_minute, sd_second;



  sd_year = year();
  sd_month = month();
  sd_day = day();
  sd_hour = hour();
  sd_minute = minute();
  sd_second = second();


  *sd_date = FAT_DATE(sd_year, sd_month, sd_day);

  *sd_time = FAT_TIME(sd_hour, sd_minute, sd_second);
}
# 1 "/Users/tavis/code/awx-pio/src/wxLog.ino"
#include "wxSerial.h"

void wxLogBlank() {
  Serial.println();
}

void wxLogRule() {
  Serial.println(F("----------------------------------------"));
}

void wxLogSection(const __FlashStringHelper* title) {
  Serial.println();
  Serial.println(title);
  wxLogRule();
}

void wxLogTag(const __FlashStringHelper* tag, const __FlashStringHelper* msg) {
  Serial.print(F("["));
  Serial.print(tag);
  Serial.print(F("] "));
  Serial.println(msg);
}

void wxLogTag(const __FlashStringHelper* tag, const String& msg) {
  Serial.print(F("["));
  Serial.print(tag);
  Serial.print(F("] "));
  Serial.println(msg);
}

void wxLogTag(const __FlashStringHelper* tag, const char* msg) {
  Serial.print(F("["));
  Serial.print(tag);
  Serial.print(F("] "));
  Serial.println(msg);
}

void wxLogTagNum(const __FlashStringHelper* tag, const __FlashStringHelper* label, long value) {
  Serial.print(F("["));
  Serial.print(tag);
  Serial.print(F("] "));
  Serial.print(label);
  Serial.println(value);
}

void wxLogTagFloat(const __FlashStringHelper* tag, const __FlashStringHelper* label, float value, uint8_t decimals) {
  Serial.print(F("["));
  Serial.print(tag);
  Serial.print(F("] "));
  Serial.print(label);
  Serial.println(value, decimals);
}
# 1 "/Users/tavis/code/awx-pio/src/wxTimeNet.ino"





#include <Dns.h>

static IPAddress cssServerIp(0, 0, 0, 0);
static bool cssServerDnsValid = false;

bool resolveCssServerIp() {
  if (cssServerDnsValid) {
    return true;
  }
  if (!ethEnabled) {
    return false;
  }

  DNSClient dns;
  dns.begin(Ethernet.dnsServerIP());
  IPAddress resolved;
  if (!dns.getHostByName(wxSiteName.c_str(), resolved)) {
    return false;
  }

  cssServerIp = resolved;
  cssServerDnsValid = true;
  return true;
}

IPAddress getCssServerIp() {
  return cssServerIp;
}



bool checkEthIncomingData() {
#ifdef BENCH_MODE
  if (!ethEnabled) return false;
#else
  if (!wifiEnabled) return false;
#endif
    usTemp = millis();
    bool timeoutWarningGiven = false;
    bool boolQuitSession = false;

    unsigned long RTCSetTimeoutStart;
    bool RTCSetTimeoutExceeded;
    time_t RTCSett;
    tmElements_t RTCSettm;
    wdt_reset();
    incomingClient = server.available();
    if (incomingClient) {
      wxLogTag(F("NET"), F("telnet client connected"));
      incomingClient.println(F("Hi there, it's me, the Marshall weather station! Send '?' for help."));
      while (incomingClient.connected()) {


        if (incomingClient.available()) {
          msTemp = millis();
          char c = incomingClient.read();
          while(incomingClient.available()) incomingClient.read();
          char fileName[13];
          String strFileName;
          int rtcSetStatus;




          switch (c) {
            case 'q':
            case 'Q':
              boolQuitSession = true;
              break;

            case 'R':
              wdt_reset();
              Serial.println(F("---===---===--- Client hit R and [enter], which causes the reboot ---===---===---"));
              incomingClient.println(F("R and Enter detected. Rebooting and disconnecting."));
              wdt_disable();
              wdt_enable(WDTO_15MS);
              while (true);
              break;

            case 'M':
              wdt_reset();
              Serial.println(F("---===---===--- Client hit M and [enter], which sets the TFTP EEPROM flag and issues a WD reset to trigger TFTP mode---===---===---"));
              incomingClient.println(F("M and Enter detected. Seeting flag, rebooting and disconnecting."));
              EEPROM.write(02,0xDD);
              delay(100);
              wdt_disable();
              wdt_enable(WDTO_15MS);
              while (true);
              break;

            case 'C':
              incomingClient.println(F("C and Enter detected. Dumping what we have cached in memory. Be sure to disconnect."));
              for (byte i=0; i < 10; i++) {
                Serial.println(wxStringCache[i]);
                incomingClient.print(i); incomingClient.print(": ");
                incomingClient.println(wxStringCache[i]);
              }

              break;

            case 'D':
              incomingClient.println(F("D and Enter detected. Dumping today's data file to this socket. No reboot (hopefully)"));
              strFileName = getDateWithZerosNoSeparator() + ".dat";
              strFileName.toCharArray(fileName, 13);

              sdReadFileToSocket(fileName);

              incomingClient.println(F("---- Done dumping file"));
              Serial.println(F("Done dumping file to Ethernet."));

              break;

            case 'G':
            case 'g':
              incomingClient.print(F("<-- g detected. Getting a camera snapshot. "));

              break;

            case 'U':
              incomingClient.print(F("U detected, toggling Ubiquity to: "));
              if (EEPROM.read(eeKeepUbiOn)) {
                incomingClient.println(F("turn off, only on once every 5 minutes."));
                keepUbiquitiOn = false;
                EEPROM.update(eeKeepUbiOn, false);
              } else {
                incomingClient.println(F("stay on ALL DAY. Still turns off at night."));
                keepUbiquitiOn = true;
                EEPROM.update(eeKeepUbiOn, true);
              }
              break;

            case 'B':
              if (camStatus.BrainDesireOn) {
                incomingClient.print(F("B detected, BrainBox camera was ON, turning off BrainBox camera..."));
                disableCamBrain();
              } else {
                incomingClient.print(F("B detected, BrainBox camera was OFF, turning on BrainBox camera..."));
                enableCamBrain();
              }
              incomingClient.println(F(" Done!"));
              break;

            case 'H':
              incomingClient.println(F("H detected. Set RTC by providing timestamp."));
              RTCSetTimeoutStart = millis();
              RTCSetTimeoutExceeded = false;
              incomingClient.println(F("Expected format: year,month,day,hour,minute,second"));
              incomingClient.println(F("Where: year has four digits, month is 1-12, day is 1-31, hour is 0-23, minute and second are 0-59."));
              incomingClient.println(F("Type anything else to abort. Timeout is 300 seconds."));
              while(!incomingClient.available()){
                wdt_reset();
                msTemp = millis();
                delay(100);
                if(millis()-RTCSetTimeoutStart > 300000){
                  incomingClient.println(F("Timeout for RTC setting exceeded. Send 'H' to retry."));
                  RTCSetTimeoutExceeded = true;
                  break;
                }
              }
              if (RTCSetTimeoutExceeded) break;
              if (incomingClient.available() < 12) incomingClient.println(F("Error: Timestamp too short! Send 'H' again to retry."));
              else {
                int y = incomingClient.parseInt();
                if (y < 1000) incomingClient.println(F("Error: Year must be > 1000!"));
                else {
                  RTCSettm.Year = CalendarYrToTm(y);
                  RTCSettm.Month = incomingClient.parseInt();
                  RTCSettm.Day = incomingClient.parseInt();
                  RTCSettm.Hour = incomingClient.parseInt();
                  RTCSettm.Minute = incomingClient.parseInt();
                  RTCSettm.Second = incomingClient.parseInt();
                  RTCSett = makeTime(RTCSettm);
                  RTC.set(RTCSett);
                  setTime(RTCSett);
                  incomingClient.println(F("Setting RTC succesful! New time is:"));
                  incomingClient.print(getDateWithZeros()); incomingClient.print(" "); incomingClient.println(getTimeWithZeros());
                  while (incomingClient.available() > 0) incomingClient.read();
                }
              }
              msTemp = millis();
              break;

            case 'S':
              if (camStatus.SouthDesireOn) {
                incomingClient.print(F("S detected, South camera was ON, turning off South camera..."));
                disableCamSouth();
              } else {
                incomingClient.print(F("S detected, South camera was OFF, turning on South camera..."));
                enableCamSouth();
              }
              incomingClient.println(F(" Done!"));
              break;

            case 'P':
            case 'N':
              if (camStatus.NorthDesireOn) {
                incomingClient.print(F("N detected, North camera was ON, turning off North camera..."));
                disableCamNorth();
              } else {
                incomingClient.print(F("N detected, North camera was OFF, turning on North camera..."));
                enableCamNorth();
              }
              incomingClient.println(F(" Done!"));
              break;

            case 'X':
              if(camStatus.badWeather) {
                incomingClient.print(F("X detected, Bad Weather bit is now set FALSE so cameras will auto-start tomorrow morning... "));
                camStatus.badWeather = false;
              } else {
                incomingClient.print(F("X detected, Bad Weather bit is now set TRUE, so cameras will NOT auto-start tomorrow morning... "));
                camStatus.badWeather = true;
              }
              EEPROM.put(eeCamStatus, camStatus);
              incomingClient.println(F(" done!"));
              break;

            case 'O':
              if (camStatus.godMode) {
                incomingClient.print(F("O detected, god mode OFF — cameras use solar/battery logic again... "));
                camStatus.godMode = false;
              } else {
                incomingClient.print(F("O detected, god mode ON — cameras from 10am until sleep, ignoring solar/battery... "));
                camStatus.godMode = true;
              }
              EEPROM.put(eeCamStatus, camStatus);
              incomingClient.println(F(" done!"));
              break;

            case 'A':
              incomingClient.print(F("A detected, turning on all cameras... "));
              enableCamBrain();
              enableCamSouth();
              enableCamNorth();
              incomingClient.println(F(" - DONE."));
              break;

            case 'a':
              incomingClient.print(F("a detected, turning OFF all cameras... "));
              disableCamBrain();
              disableCamSouth();
              disableCamNorth();
              incomingClient.println(F(" - DONE."));
              break;


            case 'J':
              incomingClient.print(F("J detected, forcing an NTP check, time is: "));
              incomingClient.println(getTimeWithZeros());
              returnStatus = "";
              compareRTCwithNTP();




              incomingClient.println();
              incomingClient.print(F("NTP check finished. Time is: "));
              incomingClient.println(getTimeWithZeros());
              incomingClient.println(returnStatus);
              break;

            case 'T':
              incomingClient.println(F("H detected. Set RTC by providing timestamp."));
              incomingClient.print(F("T detected, adding one hour to RTC time for Daylight Saving update. Current time is: "));
              incomingClient.println(getTimeWithZeros());
              rtcSetStatus = RTC.set(now() + 3600);
              incomingClient.print(F("RTC is now set, return status: "));
              incomingClient.print(rtcSetStatus);
              incomingClient.print(F(" time is "));
              incomingClient.println(getTimeWithZeros());
              incomingClient.println();
              incomingClient.println(F("---===  Note!! It takes 10 minutes for the system time to update after RTC fix!! ===------"));
              break;

            case 't':
              incomingClient.print(F("t detected, subtracting one hour from time for Daylight Saving update. Current time is: "));
              incomingClient.println(getTimeWithZeros());
              rtcSetStatus = RTC.set(now() - 3600);
              incomingClient.print(F("RTC is now set, return status: "));
              incomingClient.print(rtcSetStatus);
              incomingClient.print(F(" time is "));
              incomingClient.println(getTimeWithZeros());
              incomingClient.println();
              incomingClient.println(F("---===  Note!! It takes 10 minutes for system time to update after RTC fix!! ===------"));
              break;

            case 'W':
              incomingClient.print(F("W detected, adding 15 minutes to morning wake-up time... "));
              minutesBeforeSunrise = minutesBeforeSunrise + 15;
              if (minutesBeforeSunrise > 120) { minutesBeforeSunrise = 120; }
              eeCharTemp = minutesBeforeSunrise;
              EEPROM.put(eeMinutesBeforeSunrise, eeCharTemp);
              incomingClient.print(F(" - DONE, now waking up "));
              incomingClient.print(minutesBeforeSunrise);
              incomingClient.println(F(" minutes before sunrise."));
              break;

            case 'w':
              incomingClient.print(F("w detected, subtracting 15 minutes from morning wake-up time... "));
              minutesBeforeSunrise = minutesBeforeSunrise - 15;
              if (minutesBeforeSunrise < -120) { minutesBeforeSunrise = -120; }
              eeCharTemp = minutesBeforeSunrise;
              EEPROM.put(eeMinutesBeforeSunrise, eeCharTemp);
              incomingClient.print(F(" - DONE, now waking up "));
              incomingClient.print(minutesBeforeSunrise);
              incomingClient.println(F(" minutes before sunrise."));
              break;

            case 'Z':
              incomingClient.print(F("S detected, adding 15 minutes to night go-to-sleep time... "));
              minutesAfterSunset = minutesAfterSunset + 15;
              if (minutesAfterSunset > 120) { minutesAfterSunset = 120; }
              eeCharTemp = minutesAfterSunset;
              EEPROM.put(eeMinutesAfterSunset, eeCharTemp);
              incomingClient.print(F(" - DONE, now going to sleep "));
              incomingClient.print(minutesAfterSunset);
              incomingClient.println(F(" minutes after sunset."));
              break;

            case 'z':
              incomingClient.print(F("s detected, subtracting 15 minutes from night go-to-sleep time... "));
              minutesAfterSunset = minutesAfterSunset - 15;
              if (minutesAfterSunset < -120) { minutesAfterSunset = -120; }
              eeCharTemp = minutesAfterSunset;
              EEPROM.put(eeMinutesAfterSunset, eeCharTemp);
              incomingClient.print(F(" - DONE, now going to sleep "));
              incomingClient.print(minutesAfterSunset);
              incomingClient.println(F(" minutes after sunset."));
              break;

            case 'Y':
              incomingClient.print(F("Y detected, adding one upload retry... "));
              if (uploadRetryNum < 5) { uploadRetryNum++; }
              EEPROM.update(eeUploadRetryNum, uploadRetryNum);
              incomingClient.print(F(" - DONE, "));
              incomingClient.print(uploadRetryNum);
              incomingClient.print(F(" extra retries ("));
              incomingClient.print(uploadRetryNum + 1);
              incomingClient.println(F(" total PUT attempts)."));
              break;

            case 'y':
              incomingClient.print(F("y detected, subtracting one upload retry... "));
              if (uploadRetryNum > 0) { uploadRetryNum--; }
              EEPROM.update(eeUploadRetryNum, uploadRetryNum);
              incomingClient.print(F(" - DONE, "));
              incomingClient.print(uploadRetryNum);
              incomingClient.print(F(" extra retries ("));
              incomingClient.print(uploadRetryNum + 1);
              incomingClient.println(F(" total PUT attempts)."));
              break;

            case '?':

              incomingClient.println(F("Options:"));
              incomingClient.println(F("q or Q: Quit. Disconnects telnet session."));
              incomingClient.println(F("R: Reset unit (watchdog reset)."));
              incomingClient.println(F("C: Cache, print cached weather lines."));
              incomingClient.println(F("D: Dump SD file for today (only valid if SD card exists)."));
              incomingClient.println(F("U: Toggle ubiquiti between Always On and Off except for send every 5 minutes. Always off at night regardless."));
              incomingClient.println(F("H: Set time and date of RTC."));
              incomingClient.println(F("B: Camera viewing Brain."));
              incomingClient.println(F("S: Camera viewing South."));
              incomingClient.println(F("N: Camera viewing North."));
              incomingClient.println(F("A: Turn ON all Cameras (Brain Box, South, North)."));
              incomingClient.println(F("a: Turn OFF all Cameras (Brain Box, South, North)."));
              incomingClient.println(F("X: Bad Weather forecast, disable AUTO TURN ON for cameras. Can still be manually turned on."));
              incomingClient.println(F("O: God mode (10am until sleep, no solar/battery checks). Toggle."));
              incomingClient.println(F("J: Force an NTP check to see if the RTC should be updated."));
              incomingClient.println(F("T: Add one hour to RTC clock, for DST end in Fall. Takes 10 minutes to take effect!!"));
              incomingClient.println(F("t: Subtract one hour from RTC clock, for DST begin in Spring. Takes 10 minutes to take effect!!"));
              incomingClient.println(F("W: Add 15 minutes to morning wake time, wakes up earlier."));
              incomingClient.println(F("w: Subtract 15 minutes from morning wake time, wake up later."));
              incomingClient.println(F("Z: Add 15 minutes to night go-to-sleep (Zzzz) time, stay up later."));
              incomingClient.println(F("z: Subtract 15 minutes from night go-to-sleep (Zzzz) time, go to sleep earlier."));
              incomingClient.println(F("Y: Add one extra upload retry after PUT failure."));
              incomingClient.println(F("y: Subtract one extra upload retry after PUT failure."));
              incomingClient.println("");
              incomingClient.print(F("  Minutes before Sunrise: ")); incomingClient.println(minutesBeforeSunrise);
              incomingClient.print(F("  Minutes after Sunset:   ")); incomingClient.println(minutesAfterSunset);
              incomingClient.print(F("  Upload retries:         ")); incomingClient.print(uploadRetryNum);
              incomingClient.print(F(" extra (")); incomingClient.print(uploadRetryNum + 1); incomingClient.println(F(" total PUT attempts)"));
              incomingClient.print(F("  camStatus EEPROM value: ")); incomingClient.println(EEPROM.read(eeCamStatus));
              incomingClient.print(F("  Bad Weather bit:        ")); incomingClient.println(camStatus.badWeather);
              incomingClient.print(F("  God mode:               ")); incomingClient.println(camStatus.godMode);
              incomingClient.print(F("  CamSouth desired on:    ")); incomingClient.println(camStatus.SouthDesireOn);
              incomingClient.print(F("  CamNorth desired on:    ")); incomingClient.println(camStatus.NorthDesireOn);
              incomingClient.print(F("  CamBrain desired on:    ")); incomingClient.println(camStatus.BrainDesireOn);
              incomingClient.print(F("  Ubiquiti Keep on:       ")); incomingClient.println(EEPROM.read(eeKeepUbiOn));
              incomingClient.print(F("  Solar   V: ")); incomingClient.print(String(ina219a_solar_volts, 2));
                incomingClient.print(F(", mA: ")); incomingClient.println(String(ina219a_solar_ma, 0));
              incomingClient.print(F("  Battery V: ")); incomingClient.print(String(ina219b_battery_volts, 2));
                incomingClient.print(F(", mA: ")); incomingClient.println(String(ina219b_battery_ma, 0));
              incomingClient.print(F("  Lowest batt voltage since boot:  ")); incomingClient.println(String(voltsLowestSeen, 2));
              incomingClient.print(F("  Lowest batt voltage last sleep:  ")); incomingClient.println(String(EEPROM.read(eeVoltsLowestSeen) / 10.0, 1));
              incomingClient.print(F("  Lowest batt voltage day:  ")); incomingClient.println(String(EEPROM.read(eeVoltsLowestDay)));
              incomingClient.print(F("  Battery drain minutes: ")); incomingClient.print(String(battDrainMinutes));
                incomingClient.print(F(", mAm: ")); incomingClient.print(String(battDrainmA));
                incomingClient.print(F(", mAh = ")); incomingClient.println(String(battDrainmA / 60));
              incomingClient.print(F("  ina219_MMA loops: ")); incomingClient.println(String(ina219a_solar_MMAloops));
              incomingClient.print(F("  Temp internal: ")); incomingClient.println(String(bme280b.readTempC()));
              incomingClient.print(F("  Humidity int:  ")); incomingClient.println(String(bme280b.readFloatHumidity()));
              incomingClient.print(F("  Sunrise / Wake: "));
                incomingClient.print(strMinutesToHHMM(sunrise));
                incomingClient.print(" - "); incomingClient.print(minutesBeforeSunrise); incomingClient.print(" minutes = ");
                incomingClient.println(strMinutesToHHMM(sunrise - minutesBeforeSunrise));
              incomingClient.print(F("  Sunset / Sleep: "));
                incomingClient.print(strMinutesToHHMM(sunset));
                incomingClient.print(" + "); incomingClient.print(minutesAfterSunset); incomingClient.print(" minutes = ");
                incomingClient.println(strMinutesToHHMM(sunset + minutesAfterSunset));
              incomingClient.println();
              incomingClient.print(F("  Current time: ")); incomingClient.println(getTimeWithZeros());
              incomingClient.println();
              incomingClient.print(F("  Free ram:      ")); incomingClient.println(String(freeRam()));
              incomingClient.println(startupMessage);
              incomingClient.println();
              break;
          }
        }
        wdt_reset();
        if (boolQuitSession) {
          incomingClient.println("Session quit, DISCONNECTING.");
          break;
        }
        if ((millis() - msTemp) > 20000) {
          incomingClient.println("Idle timeout reached, DISCONNECTING.");
          break;
        } else if ((millis() - msTemp) > 10000) {

          if (not timeoutWarningGiven) {
            incomingClient.println("Idle timeout in 10 seconds....");
            timeoutWarningGiven = true;
          }
        } else if (timeoutWarningGiven) {

          timeoutWarningGiven = false;
        }

      }
      delay(3);
      incomingClient.println();
      incomingClient.print(getTimeWithZeros());
      incomingClient.println(": Ending Session. Goodbye!");
      delay(10);
      incomingClient.stop();
      Serial.print(F("[NET] telnet disconnected ("));
      Serial.print(millis() - usTemp);
      Serial.println(F(" ms)"));
      telnetSeconds = (millis() - usTemp) / 1000;
      return true;
    }
  return false;
}

void setKeepUbiquitiOn(bool v) {
    keepUbiquitiOn = v;
    EEPROM.update(eeKeepUbiOn, v);
}


void ethernetPowerOn(){

  pinMode (PIN_ETH_RESET, OUTPUT);
  digitalWrite (PIN_ETH_RESET, ETH_RESET);
  pinMode (PIN_ETH_POWER, OUTPUT);
  digitalWrite (PIN_ETH_POWER, ETH_ON);
  delay(100);
  digitalWrite (PIN_ETH_RESET, ETH_NORESET);
  delay(100);

  pinMode (SCK, OUTPUT);
  pinMode (MISO, INPUT);
  pinMode (MOSI, OUTPUT);
  digitalWrite (MISO, HIGH);
  pinMode (SS, OUTPUT);
  digitalWrite (SS, HIGH);
  Ethernet.init (SS);
  delay(100);
  wdt_reset();
}



void ethernetPowerOff(){

  SPI.end();
  pinMode (MOSI, INPUT);
  pinMode (MISO, INPUT);
  pinMode (SCK, INPUT);
  pinMode (SS, INPUT);

  pinMode (PIN_ETH_RESET, OUTPUT);
  digitalWrite (PIN_ETH_RESET, ETH_RESET);
  pinMode (PIN_ETH_POWER, OUTPUT);
  digitalWrite (PIN_ETH_POWER, ETH_OFF);
}



void enableEthernet(bool quiet) {

  if (ethEnabled) return;
  wdt_reset();
  if (!quiet) wxLogTag(F("NET"), F("enabling ethernet..."));
  ethernetPowerOn();
  Ethernet.begin(mac, ip, dnsServer, gateway, subnet);
  delayWithWdt(EthStartupDelay);
  wdt_reset();
  if (!quiet) {
    Serial.print(F("[NET] ethernet up  IP "));
    Serial.println(Ethernet.localIP());
  }
  if (resolveCssServerIp()) {
    if (!quiet) {
      Serial.print(F("[NET] DNS "));
      Serial.print(wxSiteName);
      Serial.print(F(" -> "));
      Serial.println(cssServerIp);
    }
  } else if (!quiet) {
    wxLogTag(F("NET"), F("DNS lookup failed (will retry on upload)"));
  }
  W5100.setRetransmissionTime(0x07D0);
  W5100.setRetransmissionCount(4);
  server.begin();
  delayWithWdt(100);
  ethEnabled = true;
  wdt_reset();
}

void resetEthernet(){
  wdt_reset();
  Serial.print("Resetting ethernet card...");
  pinMode(PIN_ETH_RESET, OUTPUT);
  digitalWrite(PIN_ETH_RESET, ETH_RESET);
  delay(100);
  digitalWrite(PIN_ETH_RESET, ETH_NORESET);
  delay(100);
  pinMode(SS, OUTPUT);
  digitalWrite (SS, HIGH);
  Ethernet.init (SS) ;
  delay(100);
  Ethernet.begin(mac, ip, dnsServer, gateway, subnet);
  Serial.println("Ethernet.begin executed. Wating for 5 sec");
  delayWithWdt(5000);
  wdt_reset();
  W5100.setRetransmissionTime(0x07D0);
  W5100.setRetransmissionCount(4);
  Serial.println("Done.");
}


static uint8_t athenaEepromByte(uint16_t addr) {
  return eeprom_read_byte((const uint8_t*)(uintptr_t)addr);
}

static void printAthenaIpv4(uint16_t start) {
  Serial.print(athenaEepromByte(start));
  Serial.print(F("."));
  Serial.print(athenaEepromByte(start + 1));
  Serial.print(F("."));
  Serial.print(athenaEepromByte(start + 2));
  Serial.print(F("."));
  Serial.print(athenaEepromByte(start + 3));
}

static void printAthenaMac(uint16_t start) {
  for (uint8_t i = 0; i < 6; i++) {
    if (i) {
      Serial.print(F(":"));
    }
    uint8_t octet = athenaEepromByte(start + i);
    if (octet < 16) {
      Serial.print(F("0"));
    }
    Serial.print(octet, HEX);
  }
}

static bool athenaIpv4Matches(uint16_t start, IPAddress addr) {
  for (uint8_t i = 0; i < 4; i++) {
    if (athenaEepromByte(start + i) != addr[i]) {
      return false;
    }
  }
  return true;
}

static bool athenaMacMatches(uint16_t start, const byte* expected) {
  for (uint8_t i = 0; i < 6; i++) {
    if (athenaEepromByte(start + i) != expected[i]) {
      return false;
    }
  }
  return true;
}

void printAthenaNetEeprom() {
  const bool netSigSet =
      athenaEepromByte(3) == 0x55 && athenaEepromByte(4) == 0xAA;
  const bool portSigSet = athenaEepromByte(23) == 0xBB;
  const uint16_t tftpPort =
      athenaEepromByte(24) | (static_cast<uint16_t>(athenaEepromByte(25)) << 8);

  wxLogSection(F("ATHENA EEPROM"));
  Serial.print(F("  imgStat=0x"));
  Serial.println(athenaEepromByte(2), HEX);
  Serial.print(F("  netSig="));
  Serial.println(netSigSet ? F("set") : F("unset"));
  Serial.print(F("  gw="));
  printAthenaIpv4(5);
  Serial.print(athenaIpv4Matches(5, gateway) ? F(" (matches firmware)") : F(" (MISMATCH firmware)"));
  Serial.println();
  Serial.print(F("  sn="));
  printAthenaIpv4(9);
  Serial.print(athenaIpv4Matches(9, subnet) ? F(" (matches firmware)") : F(" (MISMATCH firmware)"));
  Serial.println();
  Serial.print(F("  mac="));
  printAthenaMac(13);
  Serial.print(athenaMacMatches(13, mac) ? F(" (matches firmware)") : F(" (MISMATCH firmware)"));
  Serial.println();
  Serial.print(F("  ip="));
  printAthenaIpv4(19);
  Serial.print(athenaIpv4Matches(19, ip) ? F(" (matches firmware)") : F(" (MISMATCH firmware)"));
  Serial.println();
  Serial.print(F("  portSig="));
  Serial.print(portSigSet ? F("set") : F("unset"));
  Serial.print(F("  tftpPort="));
  Serial.println(tftpPort);
  Serial.print(F("  ethCsPin="));
  Serial.print(athenaEepromByte(68));
  Serial.print(F("  ethResetPin="));
  Serial.println(athenaEepromByte(69));

  Serial.print(F("  raw[5..25]="));
  for (uint8_t addr = 5; addr <= 25; addr++) {
    if (addr != 5) {
      Serial.print(F(" "));
    }
    Serial.print(addr);
    Serial.print(F("="));
    uint8_t value = athenaEepromByte(addr);
    if (value < 16) {
      Serial.print(F("0"));
    }
    Serial.print(value, HEX);
  }
  Serial.println();
}


void disableEthernet(bool quiet) {
  if (!quiet) {
    Serial.print(F("[NET] ethernet off  "));
    Serial.println(getTimeWithZeros());
  }

  if (hour() == 11 and minute() > 48) {

    if (!quiet) wxLogTag(F("NET"), F("ethernet held on (11:50-12:00)"));
    return;
  }

  incomingClient.stop();
  client.stop();

  ethernetPowerOff();

  ethEnabled = false;
}

void enableWifi(bool quiet) {

#ifdef BENCH_MODE
  if (!quiet) wxLogTag(F("NET"), F("wifi skipped (BENCH_MODE)"));
  return;
#endif

  if (wifiEnabled) return;
  if (wifiStartTime) return;

  if (!quiet) {
    Serial.print(F("[NET] enabling wifi ("));
    Serial.print(wifiStartupDelay);
    Serial.println(F("s delay)"));
  }

  pinMode(PIN_UBIQUITI_POWER, OUTPUT);
  digitalWrite(PIN_UBIQUITI_POWER, UBIQUITI_ON);
  wifiStartTime = millis();

}

void waitForWifi() {
  while(true){
    wdt_reset();
    if (not (int((millis() - wifiStartTime) / 1000) % 10)) {
      Serial.print(F("[NET] wifi starting... "));
      Serial.print((millis() - wifiStartTime) / 1000,10);
      Serial.println(F("s"));
    }
    if ((millis() - wifiStartTime) / 1000 > wifiStartupDelay) {
      wxLogTag(F("NET"), F("wifi ready"));
      wifiStartTime = 0;
      wifiEnabled = true;
      break;
    }
    delay(1000);
  }
}

void disableWifi(bool quiet) {

  if (keepUbiquitiOn) {
    if (!quiet) wxLogTag(F("NET"), F("wifi held on (keepUbiquitiOn)"));
    return;
  }

  if (wifiStartTime) {
    return;
  }

  if (hour() == 11 and minute() > 48) {
    if (!quiet) wxLogTag(F("NET"), F("wifi held on (11:50-12:00)"));
    return;
  }

#ifdef TENMINUTEDAY
  if (!quiet) wxLogTag(F("NET"), F("wifi held on (TENMINUTEDAY)"));
  return;
#endif


  if (!quiet) {
    Serial.print(F("[NET] wifi off  "));
    Serial.println(getTimeWithZeros());
  }
  pinMode(PIN_UBIQUITI_POWER, OUTPUT);
  digitalWrite(PIN_UBIQUITI_POWER, UBIQUITI_OFF);
  wifiEnabled = false;
  wifiStartTime = 0;

}



void PrintSpiPinMode() {
  return;
  Serial.print( "I: " ); Serial.print(INPUT);
  Serial.print(", O: " ); Serial.print(OUTPUT);
  Serial.print(", Ip: "); Serial.println(INPUT_PULLUP);
  Serial.print("MOSI: "); Serial.println(getPinMode(MOSI));
  Serial.print("MISO: "); Serial.println(getPinMode(MISO));
  Serial.print("SCK : "); Serial.println(getPinMode(SCK ));
  Serial.print("SS  : "); Serial.println(getPinMode(SS ));
  Serial.println();
}

#define UNKNOWN_PIN 0xFF
uint8_t getPinMode(uint8_t pin)
{
  uint8_t bit = digitalPinToBitMask(pin);
  uint8_t port = digitalPinToPort(pin);


  if (NOT_A_PIN == port) return UNKNOWN_PIN;


  if (0 == bit) return UNKNOWN_PIN;


  if (bit & (bit - 1)) return UNKNOWN_PIN;

  volatile uint8_t *reg, *out;
  reg = portModeRegister(port);
  out = portOutputRegister(port);

  if (*reg & bit)
    return OUTPUT;
  else if (*out & bit)
    return INPUT_PULLUP;
  else
    return INPUT;
}




void getRiseSet()
{



  float common = cos( ((month()-1)*30.5+day() + 8 ) / 58.091554);
  sunrise = 349.5 + 66.5 * common;
  sunset = 1078.5 - 69.5 * common;
  if (CheckDST()) {
    sunrise = sunrise + 60;
    sunset = sunset + 60;
  }
  sunriseDay = day();
  Serial.print(F("  sunrise "));
  Serial.print(sunrise / 60); Serial.print(F(":")); Serial.println(sunrise % 60);
  Serial.print(F("  sunset  "));
  Serial.print(sunset / 60); Serial.print(F(":")); Serial.println(sunset % 60);
}

boolean CheckDST(){






    if (month() < 3 || month() > 11) { return false; }

    if (month() > 3 && month() < 11) { return true; }
    int previousSunday = day() - weekday();

    if (month() == 3) { return previousSunday >= 8; }


    return previousSunday <= 0;
}

int getTimeZone(void) {
      if (CheckDST()) {
        return -7;
      } else {
        return -8;
      }
}
# 852 "/Users/tavis/code/awx-pio/src/wxTimeNet.ino"
time_t getNtpTime()
{
  Udp.begin(localPort);
  while (Udp.parsePacket() > 0) ;
  wxLogTag(F("NTP"), F("request"));
  sendNtpPacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    wdt_reset();
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      wxLogTag(F("NTP"), F("response"));
      Udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900;

      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.print(F("After waiting "));
  Serial.print(millis() - beginWait);
  Serial.println(F("ms, No NTP Response :-("));
  Udp.stop();
  return 0;
}


void sendNtpPacket(IPAddress &address)
{

  memset(packetBuffer, 0, NTP_PACKET_SIZE);


  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;

  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;


  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}


void compareRTCwithNTP() {


  unsigned int diffNTPRTC = 0;

  if (CheckDST()) {
    timeZone = -7;
  } else {
    timeZone = -8;
  }

  returnStatus += "TZ: ";
  returnStatus += String(timeZone);

  time_t timeRTC = RTC.get();
  time_t timeNTP = getNtpTime();
  if ((timeRTC == 0) or (timeNTP == 0)) {

    diffNTPRTC = 0;
    returnStatus += " RTC or NTP was 0. ";
  } else if (timeNTP > timeRTC) {
    diffNTPRTC = timeNTP - timeRTC;
  } else {
    diffNTPRTC = timeRTC - timeNTP;
  }
  if (diffNTPRTC > 0) {
    returnStatus += F("NTP and RTC differ by ");
    returnStatus += String(diffNTPRTC);
    if (diffNTPRTC == 1) Serial.print(" second.");
    if (diffNTPRTC > 1) Serial.print(" seconds.");
    if (diffNTPRTC > 5) {
      Serial.print("RTC time is ");
      Serial.print(timeRTC);
      Serial.print(", setting RTC to ");
      Serial.print(timeNTP);
      byte rtcSetStatus;
      rtcSetStatus = RTC.set(timeNTP);
      if (rtcSetStatus) {
        Serial.print(" FAILED. Error code: ");
        Serial.print(rtcSetStatus);
        returnStatus += F(" - Failed! RTC status: ");
        returnStatus += String(rtcSetStatus);
      } else {
        Serial.print(" done.");
        setTime(timeNTP);
        returnStatus += F("Done, RTC updated.");
      }
    }
    Serial.println();
  }
}


bool isTimeValid(time_t time_to_check){

  int ttc_year = year(time_to_check);
  if ((ttc_year >= version_year-1) and (ttc_year <= version_year + 5)) return true;
  return false;
}


bool setArduinoTimeWithNtp(){

  time_t _ntp_time = getNtpTime();
  if (_ntp_time == 0) {
    Serial.println("Setting of Arduino time from NTP failed, because NTP not available!");
    return false;
  }
  if (!isTimeValid(_ntp_time)) {
    Serial.println("Setting of Arduino time from NTP failed, because NTP not valid! NTP time is not consistent with software version date.");
    return false;
  }
  setTime(_ntp_time);
  return true;
}


String getDateWithZeros() {

    String S;

    if(year() < 10) S += "0";
    S += String(year());
    S += "/";
    if(month() < 10) S += "0";
    S += String(month());
    S += "/";
    if(day() < 10) S += "0";
    S += String(day());

    return S;

}

String getDateWithZerosNoSeparator() {

    String S;

    if(year() < 10) S += "0";
    S += String(year());
    if(month() < 10) S += "0";
    S += String(month());
    if(day() < 10) S += "0";
    S += String(day());

    return S;

}

String getTimeWithZeros() {

    String S;

    if(hour() < 10) S += "0";
    S += String(hour());
    S += ":";
    if(minute() < 10) S += "0";
    S += String(minute());
    S += ":";
    if(second() < 10) S += "0";
    S += String(second());

    return S;

}


String strMinutesToHHMM(int M) {

  String S;

  if (M / 60 < 10) S += "0";
  S += String(M / 60);
  S += ":";

  if (M % 60 < 10) S += "0";
  S += String(M % 60);

  return S;
}

String time_t_to_datetime_string(time_t tt){
  String dts = (String)year(tt) + "/" + (String)month(tt) + "/" + (String)day(tt) + " ";
  dts += (String)hour(tt) + ":" + (String)minute(tt) + ":" + (String)second(tt);
  return dts;
}


String makeStationDefinesSuffix() {
  String s;
  s += F(",");
  s += wxBetaCode;
  s += F(",");
  s += IPq3;
  s += F(".");
  s += IPWX;
  s += F(":");
  s += IPgw;
  return s;
}



size_t makeUploadWeatherPut(char* buf, size_t bufSize, const String& wxString, byte uploadRetries) {
  if (!buf || bufSize < 64 || wxString.length() == 0) return 0;

  String strPut;
  strPut.reserve(96 + wxString.length() + (uploadRetries > 0 ? 16 : 0));
  strPut += F("PUT ");
  strPut += wxUploadPath;
  strPut += F("?sub=");
  strPut += wxBetaText;
  strPut += F("&w=");
  strPut += wxString;
  if (uploadRetries > 0) {
    strPut += F(",UploadRetries=");
    strPut += String(uploadRetries);
  }
  strPut += F(" HTTP/1.1\r\nHost: ");
  strPut += wxSiteName;
  strPut += F("\r\nConnection: close\r\n\r\n");

  size_t len = strPut.length();
  if (len == 0 || len >= bufSize) return 0;
  strPut.toCharArray(buf, bufSize);
  return len;
}
# 1 "/Users/tavis/code/awx-pio/src/wxUtilities.ino"







void accumulateWindSpeedSample(float speed) {
  windSpeedMinuteSum += speed;
  windSpeedMinuteCount++;
}

void finalizeWindSpeedMinute() {
  if (windSpeedMinuteCount > 0) {
    windSpeedAvg = windSpeedMinuteSum / windSpeedMinuteCount;
  }
  windSpeedMinuteSum = 0;
  windSpeedMinuteCount = 0;
}


void delayWithWdt(unsigned long ms)
{
  unsigned long start = millis();
  while (millis() - start < ms) {
    wdt_reset();
    delay(100);
  }
}


float get_wind_speed()
{
    #ifdef SIMULATE_WIND_SPEED
      return(SIMULATE_WIND_SPEED);
    #elif defined(ANEMO_WS85)
    if (ws85Fresh(30000)) {
      return ws85SpeedMph();
    }
    return windSpeedAvg;
    #else
    float deltaTime = millis() - lastWindCheck;
    deltaTime /= 1000.0;
    float windSpeed = (float)windClicks / deltaTime;

    minuteWindClicks += windClicks;
    windClicks = 0;
    lastWindCheck = millis();
    windSpeed *= 1.492;

    return(windSpeed);
    #endif
}


int get_wind_direction()
{
    #ifdef ANEMO_WS85
    if (ws85Fresh(30000)) {
      winddirRaw = ws85Direction();
      return ws85Direction();
    }
    return winddir;
    #else
    unsigned int adc;

    # ifndef SIMULATE_WIND_DIRECTION
      adc = analogRead(WDIR);
    #else
      adc = SIMULATE_WIND_DIRECTION;
    #endif
    winddirRaw = adc;





    if (adc < 81 ) { strWindDir = "ERL"; return (-10); }
    else if (adc < 186 ) { strWindDir = "ESE"; return (113); }
    else if (adc < 215 ) { strWindDir = "ENE"; return (68); }
    else if (adc < 258 ) { strWindDir = "E"; return (90); }
    else if (adc < 341 ) { strWindDir = "SSE"; return (158); }
    else if (adc < 436 ) { strWindDir = "SE"; return (135); }
    else if (adc < 508 ) { strWindDir = "SSW"; return (203); }
    else if (adc < 600 ) { strWindDir = "S"; return (180); }
    else if (adc < 689 ) { strWindDir = "NNE"; return (23); }
    else if (adc < 766 ) { strWindDir = "NE"; return (45); }
    else if (adc < 827 ) { strWindDir = "WSW"; return (248); }
    else if (adc < 859 ) { strWindDir = "SW"; return (225); }
    else if (adc < 902 ) { strWindDir = "NNW"; return (338); }
    else if (adc < 934 ) { strWindDir = "N"; return (0); }
    else if (adc < 957 ) { strWindDir = "WNW"; return (293); }
    else if (adc < 982 ) { strWindDir = "NW"; return (315); }
    else if (adc < 1008) { strWindDir = "W"; return (270); }
    else { strWindDir = "ERH"; return (-20); }
    return (-30);
    #endif
}



void ShowSockStatus()
{
  if (not ethEnabled) return;

  for (int i = 0; i < MAX_SOCK_NUM; i++) {
    Serial.print("Socket#");
    Serial.print(i);
    uint8_t s = W5100.readSnSR(i);
    ethSockStatus[i] = s;
    Serial.print(":0x");
    Serial.print(s,16);
    Serial.print(" ");
    Serial.print(W5100.readSnPORT(i));
    Serial.print(" D:");
    uint8_t dip[4];
    W5100.readSnDIPR(i, dip);
    for (int j=0; j<4; j++) {
      Serial.print(dip[j],10);
      if (j<3) Serial.print(".");
    }
    Serial.print("(");
    Serial.print(W5100.readSnDPORT(i));
    Serial.println(")");
  }
}


int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

void printDigits(int digits){

  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}



void handleSerial() {

  while (Serial.available()) {
    Serial.read();
  }
}





void enableSolar() {
  Serial.print(F("[SOLAR] on  "));
  Serial.println(getTimeWithZeros());
  pinMode(PIN_SOLAR_POWER, INPUT);
  digitalWrite(PIN_SOLAR_POWER, SOLAR_CONNECTED);

}


void disableSolar() {
  Serial.print(F("[SOLAR] off "));
  Serial.println(getTimeWithZeros());
  pinMode(PIN_SOLAR_POWER, OUTPUT);
  digitalWrite(PIN_SOLAR_POWER, SOLAR_DISCONNECTED);

}



void enableCamBrain() {
  if (camStatus.BrainDesireOn == false) {
    wxLogTag(F("CAM"), F("brain on"));
    pinMode(PIN_CamBrain_POWER, INPUT_PULLUP);
    delay (2000);
    pinMode(PIN_CamBrain_POWER, OUTPUT);
    digitalWrite(PIN_CamBrain_POWER, CamBrain_ON);
    camStatus.BrainDesireOn = true;
    EEPROM.put(eeCamStatus, camStatus);
  }
}
void disableCamBrain() {
  wxLogTag(F("CAM"), F("brain off"));
  digitalWrite(PIN_CamBrain_POWER, CamBrain_OFF);
  camStatus.BrainDesireOn = false;
  EEPROM.put(eeCamStatus, camStatus);
}

void enableCamNorth() {
  if (camStatus.NorthDesireOn == false) {
    wxLogTag(F("CAM"), F("north on"));
    pinMode(PIN_CamNorth_POWER, INPUT_PULLUP);
    delay (2000);
    pinMode(PIN_CamNorth_POWER, OUTPUT);
    camStatus.NorthDesireOn = true;
    digitalWrite(PIN_CamNorth_POWER, CamNorth_ON);
    EEPROM.put(eeCamStatus, camStatus);
  }
}
void disableCamNorth() {
  wxLogTag(F("CAM"), F("north off"));
  camStatus.NorthDesireOn = false;
  digitalWrite(PIN_CamNorth_POWER, CamNorth_OFF);
  EEPROM.put(eeCamStatus, camStatus);
}

void enableCamSouth() {
  if (camStatus.SouthDesireOn == false) {
    wxLogTag(F("CAM"), F("south on"));
    pinMode(PIN_CamSouth_POWER, INPUT_PULLUP);
    delay (2000);
    pinMode(PIN_CamSouth_POWER, OUTPUT);
    digitalWrite(PIN_CamSouth_POWER, CamSouth_ON);
    camStatus.SouthDesireOn = true;
    EEPROM.put(eeCamStatus, camStatus);
  }
}
void disableCamSouth() {
  wxLogTag(F("CAM"), F("south off"));
  digitalWrite(PIN_CamSouth_POWER, CamSouth_OFF);
  camStatus.SouthDesireOn = false;
  EEPROM.put(eeCamStatus, camStatus);
}


void writeHardwareId(byte id) {
  EEPROM.update(eeHardwareId, 0);
  EEPROM.update(eeHardwareId, id);
  hardwareId = id;
}



void initializeEEPROM() {







  if (EEPROM.read(eeKeepUbiOn) == 255) {
    Serial.println(F("EEPROM eeKeepUbiOn was 255, is this a new Arduino? Setting to false (0)."));
    EEPROM.update(eeKeepUbiOn, false);
  }

  if (EEPROM.read(eeCamStatus) == 255) {
    Serial.println(F("EEPROM eeCamStatus was 255, is this a new Arduino? Setting to false (0)."));
    EEPROM.update(eeCamStatus, false);
  }

  if (EEPROM.read(eeMinutesBeforeSunrise) == 255) {
    Serial.println(F("EEPROM eeMinutesBeforeSunrise was 255, is this a new Arduino? Setting to 30."));
    EEPROM.put(eeMinutesBeforeSunrise, char(minutesBeforeSunrise));
  }
  if (EEPROM.read(eeMinutesAfterSunset) == 255) {
    Serial.println(F("EEPROM eeMinutesAfterSunset was 255, is this a new Arduino? Setting to 30."));
    EEPROM.put(eeMinutesAfterSunset, char(minutesAfterSunset));
  }

  if (EEPROM.read(eeUploadRetryNum) == 255) {
    Serial.println(F("EEPROM eeUploadRetryNum was 255, is this a new Arduino? Setting to 1."));
    EEPROM.update(eeUploadRetryNum, uploadRetryNum);
  }

  hardwareId = EEPROM.read(eeHardwareId);
  const byte buildHwId = (byte)hardwareVersion.toInt();
  if (hardwareId == 255) {
    Serial.print(F("EEPROM eeHardwareId unset, writing HW_VERSION="));
    Serial.println(buildHwId);
    writeHardwareId(buildHwId);
  } else if (hardwareId != buildHwId) {
    Serial.print(F("WARNING: EEPROM hardwareId="));
    Serial.print(hardwareId);
    Serial.print(F(" differs from build HW_VERSION="));
    Serial.print(buildHwId);
    Serial.println(F("; using EEPROM value for uploads"));
  }

  if (hardwareId == 255) {
    hwVersionForUpload = hardwareVersion;
  } else {
    hwVersionForUpload = String(hardwareId);
  }


  if (EEPROM.read(eeKeepUbiOn)) {
    keepUbiquitiOn = true;
  }
# 302 "/Users/tavis/code/awx-pio/src/wxUtilities.ino"
  EEPROM.get(eeCamStatus, camStatus);
  if (camStatus.NorthDesireOn) { camStatus.NorthDesireOn = false; enableCamNorth(); }
  if (camStatus.SouthDesireOn) { camStatus.SouthDesireOn = false; enableCamSouth(); }
  if (camStatus.BrainDesireOn) { camStatus.BrainDesireOn = false; enableCamBrain(); }


  EEPROM.get(eeMinutesBeforeSunrise, eeCharTemp);
  minutesBeforeSunrise = eeCharTemp;
  EEPROM.get(eeMinutesAfterSunset, eeCharTemp);
  minutesAfterSunset = eeCharTemp;
  uploadRetryNum = EEPROM.read(eeUploadRetryNum);

#ifdef BENCH_MODE
  Serial.println(F("BENCH_MODE - staying awake, clearing keep-Ubiquiti EEPROM flag."));
  keepUbiquitiOn = false;
  if (EEPROM.read(eeKeepUbiOn)) EEPROM.update(eeKeepUbiOn, false);
#endif
}


void goToSleep(){



#ifdef DONT_SLEEP
  return;
#endif

  keepUbiquitiOn = false;
  EEPROM.update(eeKeepUbiOn, false);
  if (camStatus.godMode) {
    camStatus.godMode = false;
    EEPROM.put(eeCamStatus, camStatus);
  }
  disableWifi();
  disableEthernet();
  disableCamBrain();
  disableCamNorth();
  disableCamSouth();
# 350 "/Users/tavis/code/awx-pio/src/wxUtilities.ino"
  EEPROM.get(eeVoltsLowestDay, eeByteTemp);
  if (day() == eeByteTemp) {
    EEPROM.get(eeVoltsLowestSeen, eeByteTemp);
    if ((byte)voltsLowestSeen < eeByteTemp) {
      EEPROM.put(eeVoltsLowestSeen, (byte)(voltsLowestSeen * 10.0));
    }
  } else {
    EEPROM.put(eeVoltsLowestSeen, (byte)(voltsLowestSeen * 10.0));
    EEPROM.put(eeVoltsLowestDay, (byte)day());
  }
  ina219a_solar.enterPowerSave();
  ina219b_battery.enterPowerSave();



  uint8_t valuea = bme280a.readRegister(BME280_CTRL_MEAS_REG);
  valuea = (valuea & 0xFC) + 0x01;
  bme280a.writeRegister(BME280_CTRL_MEAS_REG, valuea);
  uint8_t valueb = bme280b.readRegister(BME280_CTRL_MEAS_REG);
  valueb = (valueb & 0xFC) + 0x01;
  bme280b.writeRegister(BME280_CTRL_MEAS_REG, valueb);


  delay(10);




  wxLogSection(F("SLEEP"));
  wxLogTag(F("SLEEP"), F("powering down"));
  Serial.flush();
  Serial.end();

  cli();


  delay(500);
  PORTF &= ~_BV (7) & ~_BV (6) & ~_BV (4) &~_BV (2) & ~_BV (1) & ~_BV (0);


  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(A2, OUTPUT);
  pinMode(A3, OUTPUT);
  pinMode(A4, OUTPUT);
  pinMode(A5, OUTPUT);
  pinMode(A6, OUTPUT);
  pinMode(A7, OUTPUT);
  pinMode(A8, OUTPUT);
  pinMode(A9, OUTPUT);
  pinMode(A10, OUTPUT);
  pinMode(A11, OUTPUT);
  pinMode(A12, OUTPUT);
  pinMode(A13, OUTPUT);
  pinMode(A14, OUTPUT);
  pinMode(A15, OUTPUT);

  digitalWrite(A0, LOW);
  digitalWrite(A1, LOW);
  digitalWrite(A2, LOW);
  digitalWrite(A3, LOW);
  digitalWrite(A4, LOW);
  digitalWrite(A5, LOW);
  digitalWrite(A6, LOW);
  digitalWrite(A7, LOW);
  digitalWrite(A8, LOW);
  digitalWrite(A9, LOW);
  digitalWrite(A10, LOW);
  digitalWrite(A11, LOW);
  digitalWrite(A12, LOW);
  digitalWrite(A13, LOW);
  digitalWrite(A14, LOW);
  digitalWrite(A15, LOW);
  pinMode(WSPEED, INPUT);
  pinMode(WDIR, INPUT);


  Wire.end();
  pinMode(SCL, INPUT);
  pinMode(SDA, INPUT);


  for (int i = 0; i <= 53; i++) {
    pinMode(i, OUTPUT);
    if (PIN_ETH_POWER == i) {digitalWrite(i, HIGH);}
    else if (PIN_UBIQUITI_POWER == i) {digitalWrite(i, HIGH);}
    else {digitalWrite(i, LOW);}
  }

  cli();

  ADCSRA = 0;
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  power_all_disable();
  power_adc_disable();
  power_spi_disable();
  power_usart0_disable();
  power_usart2_disable();
  power_timer1_disable();
  power_timer2_disable();
  power_timer3_disable();
  power_timer4_disable();
  power_timer5_disable();
  power_twi_disable();

  sleep_mode();

}





void calcWeather()
{

    winddir = get_wind_direction();
# 489 "/Users/tavis/code/awx-pio/src/wxUtilities.ino"
    long sum = winddiravg[0];
    int D = winddiravg[0];
    for(int i = 1 ; i < WIND_DIR_AVG_SIZE ; i++)
    {
        int delta = winddiravg[i] - D;

        if(delta < -180)
            D += delta + 360;
        else if(delta > 180)
            D += delta - 360;
        else
            D += delta;

        sum += D;
    }
    winddir_avg2m = sum / WIND_DIR_AVG_SIZE;
    if(winddir_avg2m >= 360) winddir_avg2m -= 360;
    if(winddir_avg2m < 0) winddir_avg2m += 360;




    windgustmph_10m = 0;
    windgustdir_10m = 0;

    for(int i = 0; i < 10 ; i++)
    {
        if(windgust_10m[i] > windgustmph_10m)
        {
            windgustmph_10m = windgust_10m[i];
            windgustdir_10m = windgustdirection_10m[i];
        }
    }




    windgustmph_5m = 0;
    windgustdir_5m = 0;

    for(int i = 0; i < 5 ; i++)
    {
        if(windgust_5m[i] > windgustmph_5m)
        {
            windgustmph_5m = windgust_5m[i];
            windgustdir_5m = windgustdirection_5m[i];
        }
    }







}






void printWeather()
{

  timeStatus();


  printDigits(hour());
  Serial.print((":"));
  printDigits(minute());


  Serial.print(charComma);
  Serial.print(month());
  Serial.print(("/"));
  Serial.print(day());
  Serial.print(("/"));
  Serial.print(year());


  Serial.print(charComma);
  if (windSpeedAvg < 10) Serial.print('0');
  Serial.print(windSpeedAvg, 2);


  Serial.print(charComma);
  if (windgustmph_5m < 10) Serial.print('0');
  Serial.print(windgustmph_5m, 1);


  Serial.print(charComma);
  if (winddir == 0 ) Serial.print(" ");
  if (winddir < 100) Serial.print(" ");
  Serial.print(winddir);






  Serial.print(charComma);
  Serial.print(tempf, 1);


  Serial.print(charComma);
  Serial.print(humidityOutside, 1);


  Serial.print(charComma);
  Serial.print(pressure / 100.0, 1);


  Serial.print(charComma);
  if (pressure >= oldPressure) Serial.print(" ");
  Serial.print((pressure - oldPressure) / 100.0, 2);




  Serial.print(charComma);
  Serial.print(0.0, 2);
# 619 "/Users/tavis/code/awx-pio/src/wxUtilities.ino"
  Serial.print((","));
  if (windspeedmph < 10) Serial.print('0');
  Serial.print(windspeedmph, 1);


  Serial.print(charComma);

  Serial.print(RTC.temperature() / 4);



  Serial.print(charComma);
  Serial.print(ina219a_solar_ma, 2);
  Serial.print("mA");


  Serial.print(charComma);
  Serial.print(batt_lvl, 2);


  Serial.print(charComma);
  Serial.print(ina219a_solar_volts,2);


  Serial.print(charComma);
  Serial.print(light_lvl, 2);


  Serial.print(charComma);
  Serial.print(days);
  Serial.print(".");
  printDigits(hours);
  Serial.print(":");
  printDigits(minutes);
  Serial.print(":");
  printDigits(seconds);


  time_t rtcTime = RTC.get();
  Serial.print(charComma);
  Serial.print("rtc:");
  Serial.print(year(rtcTime));
  Serial.print("/");
  Serial.print(month(rtcTime));
  Serial.print("/");
  Serial.print(day(rtcTime));
  Serial.print(charComma);
  Serial.print(hour(rtcTime));
  Serial.print(":");
  Serial.print(minute(rtcTime));
  Serial.print(":");
  Serial.print(second(rtcTime));
# 696 "/Users/tavis/code/awx-pio/src/wxUtilities.ino"
  Serial.print(charComma);
  Serial.print(freeRam());


  Serial.print(charComma);
  Serial.print(winddirRaw);


  Serial.print(charComma);
  Serial.print(ina219a_solar_volts);
  Serial.print(charComma);
  Serial.print(ina219a_solar_ma);


  Serial.println();

}
# 1 "/Users/tavis/code/awx-pio/src/wxWS85.ino"





#ifdef ANEMO_WS85

#define WS85_SERIAL Serial1


static int ws85Dir = 0;
static float ws85SpeedMps = 0;
static float ws85GustMps = 0;
static float ws85TempCelsius = 0;
static float ws85RainMillimeters = 0;
static float ws85CapVolts = 0;
static float ws85BatVolts = 0;
static bool ws85Valid = false;
static unsigned long ws85LastMs = 0;
static bool ws85HaveDir = false;
static bool ws85HaveSpeed = false;
static bool ws85HaveGust = false;
static bool ws85HaveTemp = false;
static bool ws85HaveRain = false;
static bool ws85HaveCapV = false;
static bool ws85HaveBatV = false;
static bool ws85FrameReady = false;

float ws85ParseVolts(const String &val) {
  String s = val;
  s.trim();
  if (s.endsWith("V") || s.endsWith("v")) {
    s.remove(s.length() - 1);
    s.trim();
  }
  return s.toFloat();
}

void ws85ResetFrame() {
  ws85HaveDir = false;
  ws85HaveSpeed = false;
  ws85HaveGust = false;
  ws85HaveTemp = false;
  ws85HaveRain = false;
  ws85HaveCapV = false;
  ws85HaveBatV = false;
}

void ws85MarkFrameReady() {
  if (!ws85HaveDir || !ws85HaveSpeed) return;
  ws85FrameReady = true;
}

#ifdef WS85_SERIAL_LOG
void ws85PrintReading() {
  if (!ws85HaveDir || !ws85HaveSpeed) return;

  Serial.print(F("[WS85] dir="));
  Serial.print(ws85Dir);
  Serial.print(F(" deg  speed="));
  Serial.print(ws85SpeedMps, 1);
  Serial.print(F(" m/s ("));
  Serial.print(ws85SpeedMps * 2.23694f, 1);
  Serial.print(F(" mph)"));
  Serial.print(F("  gust="));
  if (ws85HaveGust) {
    Serial.print(ws85GustMps, 1);
    Serial.print(F(" m/s ("));
    Serial.print(ws85GustMps * 2.23694f, 1);
    Serial.print(F(" mph)"));
  } else {
    Serial.print(F("n/a"));
  }
  if (ws85HaveTemp) {
    Serial.print(F("  temp="));
    Serial.print(ws85TempCelsius, 1);
    Serial.print(F(" C"));
  }
  if (ws85HaveRain) {
    Serial.print(F("  rain="));
    Serial.print(ws85RainMillimeters, 1);
    Serial.print(F(" mm"));
  }
  if (ws85HaveCapV) {
    Serial.print(F("  cap="));
    Serial.print(ws85CapVolts, 2);
    Serial.print(F(" V"));
  }
  if (ws85HaveBatV) {
    Serial.print(F("  bat="));
    Serial.print(ws85BatVolts, 2);
    Serial.print(F(" V"));
  }
  Serial.println();

  ws85MarkFrameReady();
}
#else
void ws85PrintReading() {
  ws85MarkFrameReady();
}
#endif

void ws85ParseLine(const String &line) {
  if (line.startsWith("==========")) {
    if (line.indexOf("WS85") >= 0) {
      ws85ResetFrame();
    } else {
      ws85PrintReading();
    }
    return;
  }

  int eq = line.indexOf('=');
  if (eq < 0) return;

  String key = line.substring(0, eq);
  key.trim();
  String val = line.substring(eq + 1);
  val.trim();

  if (key == "WindDir") {
    ws85Dir = val.toInt();
    ws85Valid = true;
    ws85LastMs = millis();
    ws85HaveDir = true;
  } else if (key == "WindSpeed") {
    ws85SpeedMps = val.toFloat();
    ws85HaveSpeed = true;
  } else if (key == "WindGust") {
    ws85GustMps = val.toFloat();
    ws85HaveGust = true;
  } else if (key == "GXTS04Temp") {
    ws85TempCelsius = val.toFloat();
    ws85HaveTemp = true;
  } else if (key == "Rain") {
    ws85RainMillimeters = val.toFloat();
    ws85HaveRain = true;
  } else if (key == "CapVoltage") {
    ws85CapVolts = ws85ParseVolts(val);
    ws85HaveCapV = true;
  } else if (key == "BatVoltage") {
    ws85BatVolts = ws85ParseVolts(val);
    ws85HaveBatV = true;
  }
}

void ws85Init() {
  WS85_SERIAL.begin(WS85_BAUD);
}

void ws85Poll() {
  static String line;

  while (WS85_SERIAL.available()) {
    char c = WS85_SERIAL.read();
    if (c == '\r') continue;

    if (c == '\n') {
      if (line.length() > 0) {
        ws85ParseLine(line);
      }
      line = "";
    } else if (line.length() < 80) {
      line += c;
    }
  }
}

bool ws85Fresh(unsigned long maxAgeMs) {
  return ws85Valid && ((millis() - ws85LastMs) < maxAgeMs);
}

bool ws85ConsumeFrame() {
  if (!ws85FrameReady) return false;
  ws85FrameReady = false;
  return true;
}

float ws85SpeedMph() {
  return ws85SpeedMps * 2.23694f;
}

float ws85GustMph() {
  if (ws85HaveGust) {
    return ws85GustMps * 2.23694f;
  }
  return ws85SpeedMph();
}

int ws85Direction() {
  return ws85Dir;
}

float ws85TempC() {
  return ws85TempCelsius;
}

float ws85RainMm() {
  return ws85RainMillimeters;
}

float ws85CapVoltage() {
  return ws85CapVolts;
}

float ws85BatVoltage() {
  return ws85BatVolts;
}

void ws85LogVoltage() {
  Serial.print(F("[WS85] "));
  if (ws85HaveCapV) {
    Serial.print(F("cap="));
    Serial.print(ws85CapVolts, 2);
    Serial.print(F(" V"));
  } else {
    Serial.print(F("cap=n/a"));
  }
  Serial.print(F("  "));
  if (ws85HaveBatV) {
    Serial.print(F("bat="));
    Serial.print(ws85BatVolts, 2);
    Serial.print(F(" V"));
  } else {
    Serial.print(F("bat=n/a"));
  }
  Serial.println();
}

void ws85LogVoltageAtBoot() {
  unsigned long t0 = millis();
  while (millis() - t0 < 3000) {
    ws85Poll();
    wdt_reset();
    if (ws85HaveCapV || ws85HaveBatV) break;
  }
  ws85LogVoltage();
}

#endif