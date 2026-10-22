/*
 * 
 * Functions for handling time and some network related duties. Like NTP.
 * 
 */
#include <Dns.h>

extern time_t uploadPending;

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
// Telnet and Serial share the same single-character command set (Stream interface).
void handleRemoteCommand(char c, Stream &io, bool &quitSession) {
  char fileName[13];
  String strFileName;
  int rtcSetStatus;
  unsigned long RTCSetTimeoutStart;
  bool RTCSetTimeoutExceeded;
  time_t RTCSett;
  tmElements_t RTCSettm;

          switch (c) {
            case 'q':
            case 'Q': // for quit/Quit
              quitSession = true;
              break;
            
            case 'R': // for Reset
              wdt_reset();
              Serial.println(F("---===---===--- Client hit R and [enter], which causes the reboot ---===---===---"));
              io.println(F("R and Enter detected. Rebooting and disconnecting."));
              wdt_disable();              // kill interference
              wdt_enable(WDTO_15MS);      // set WD reset timeout to 15msec
              while (true);                  // wait for WD to happen
              break;                      // End of 'R'eset

            case 'M':                     // 'M'anually enable (no-timeout) TFTP mode and make a Reset to trigger TFTP mode
              wdt_reset();                // prevent timeout for now
              Serial.println(F("---===---===--- Client hit M and [enter], which sets the TFTP EEPROM flag and issues a WD reset to trigger TFTP mode---===---===---"));
              io.println(F("M and Enter detected. Seeting flag, rebooting and disconnecting."));
              EEPROM.write(02,0xDD);      // enableUpdateMode, set the TFTP mode via flag (0xDD) in EEPROM location 02
              delay(100);                 // strangely must wait here for it to work
              wdt_disable();              // kill interference
              wdt_enable(WDTO_15MS);      // set WD reset timeout to 15msec
              while (true);               // wait 15msec for WD to happen, Mega will be in TFTP mode after reset
              break;                      // End of 'M'anual TFTP + reset
              
            case 'C': // for Cache, dump what cached data we have
              io.println(F("C and Enter detected. Dumping what we have cached in memory. Be sure to disconnect."));
              for (byte i=0; i < 10; i++) {
                Serial.println(wxStringCache[i]);
                io.print(i); io.print(": ");
                io.println(wxStringCache[i]);
              }

              break; // End of 'C'ache
              
            case 'D': // for Dump today's file to this socket.
              io.println(F("D and Enter detected. Dumping today's data file to this socket. No reboot (hopefully)"));
              strFileName = getDateWithZerosNoSeparator() + ".dat";
              strFileName.toCharArray(fileName, 13);
              
              sdReadFileToSocket(fileName, io);
              
              io.println(F("---- Done dumping file"));
              Serial.println(F("Done dumping file to stream."));

              break; // End of 'D'ump

            case 'G':
            case 'g': // for Get Camera Snapshot (a stretch, but I'm running out of letters)
              io.print(F("<-- g detected. Getting a camera snapshot. "));

              break; // End of 'g'et Camera Snapshot
              
            case 'U': // for Toggle Ubiquiti keep-on (vs 5 minute Save & Send all day)
              io.print(F("U detected, toggling Ubiquity to: "));
              if (EEPROM.read(eeKeepUbiOn)) {
                io.println(F("turn off, only on once every 5 minutes."));
                keepUbiquitiOn = false;
                EEPROM.update(eeKeepUbiOn, false);
              } else {
                io.println(F("stay on ALL DAY. Still turns off at night."));
                keepUbiquitiOn = true;
                EEPROM.update(eeKeepUbiOn, true);
              }
              break; // End of 'U'biquiti toggle
              
            case 'B': // for turn Brain Box camera on
              if (camStatus.BrainDesireOn) {
                io.print(F("B detected, BrainBox camera was ON, turning off BrainBox camera..."));
                disableCamBrain();
              } else {
                io.print(F("B detected, BrainBox camera was OFF, turning on BrainBox camera..."));
                enableCamBrain();
              }
              io.println(F(" Done!"));
              break;
              
            case 'H': // Set time of RTC.
              io.println(F("H detected. Set RTC by providing timestamp."));
              RTCSetTimeoutStart = millis();
              RTCSetTimeoutExceeded = false;
              io.println(F("Expected format: year,month,day,hour,minute,second"));
              io.println(F("Where: year has four digits, month is 1-12, day is 1-31, hour is 0-23, minute and second are 0-59."));
              io.println(F("Type anything else to abort. Timeout is 300 seconds."));
              while(!io.available()){ //wait for user input resetting the watchdog_timer()
                wdt_reset(); //reset watchdog while wating
                msTemp = millis(); //Reset general client connection timeout while waiting.
                delay(100);
                if(millis()-RTCSetTimeoutStart > 300000){ 
                  io.println(F("Timeout for RTC setting exceeded. Send 'H' to retry."));
                  RTCSetTimeoutExceeded = true;
                  break;
                }
              }
              if (RTCSetTimeoutExceeded) break;
              if (io.available() < 12) io.println(F("Error: Timestamp too short! Send 'H' again to retry."));
              else {
                int y = io.parseInt();
                if (y < 1000) io.println(F("Error: Year must be > 1000!"));
                else {
                  RTCSettm.Year = CalendarYrToTm(y);
                  RTCSettm.Month = io.parseInt();
                  RTCSettm.Day = io.parseInt();
                  RTCSettm.Hour = io.parseInt();
                  RTCSettm.Minute = io.parseInt();
                  RTCSettm.Second = io.parseInt();
                  RTCSett = makeTime(RTCSettm);
                  RTC.set(RTCSett);
                  setTime(RTCSett);
                  io.println(F("Setting RTC succesful! New time is:"));
                  io.print(getDateWithZeros()); io.print(" "); io.println(getTimeWithZeros());
                  while (io.available() > 0) io.read(); //flush buffer
                }
              }
              msTemp = millis(); //Reset general client connection timeout.
              break;
              
            case 'S': // for turn South camera on (formerly called Hang Glider launch camera)
              if (camStatus.SouthDesireOn) {
                io.print(F("S detected, South camera was ON, turning off South camera..."));
                disableCamSouth();
              } else {
                io.print(F("S detected, South camera was OFF, turning on South camera..."));
                enableCamSouth();
              }
              io.println(F(" Done!"));
              break;

            case 'P':
            case 'N': // for turn North camera on (formerly called Paraglider Launch camera)
              if (camStatus.NorthDesireOn) {
                io.print(F("N detected, North camera was ON, turning off North camera..."));
                disableCamNorth();
              } else {
                io.print(F("N detected, North camera was OFF, turning on North camera..."));
                enableCamNorth();
              }
              io.println(F(" Done!"));
              break;

            case 'X': // Bad Weather forecast, don't auto-on the cameras in the morning.
              if(camStatus.badWeather) {
                io.print(F("X detected, Bad Weather bit is now set FALSE so cameras will auto-start tomorrow morning... "));
                camStatus.badWeather = false;
              } else {
                io.print(F("X detected, Bad Weather bit is now set TRUE, so cameras will NOT auto-start tomorrow morning... "));
                camStatus.badWeather = true;
              }
              EEPROM.put(eeCamStatus, camStatus);
              io.println(F(" done!"));
              break;

            case 'O': // God mode: from 10am until night sleep, no solar/battery checks.
              if (camStatus.godMode) {
                io.print(F("O detected, god mode OFF — cameras use solar/battery logic again... "));
                camStatus.godMode = false;
              } else {
                io.print(F("O detected, god mode ON — cameras from 10am until sleep, ignoring solar/battery... "));
                camStatus.godMode = true;
              }
              EEPROM.put(eeCamStatus, camStatus);
              io.println(F(" done!"));
              break;
              
            case 'A': // for turn ALL cameras on
              io.print(F("A detected, turning on all cameras... "));
              enableCamBrain();
              enableCamSouth();
              enableCamNorth();
              io.println(F(" - DONE."));
              break;

            case 'a': // for turn ALL cameras off
              io.print(F("a detected, turning OFF all cameras... "));
              disableCamBrain();
              disableCamSouth();
              disableCamNorth();
              io.println(F(" - DONE."));
              break;


            case 'J': // Force an NTP check to update the RTC
              io.print(F("J detected, forcing an NTP check, time is: "));
              io.println(getTimeWithZeros());
              returnStatus = "";
              ensureNetworkForNtp(io);
              wdt_reset();
              compareRTCwithNTP();
              wdt_reset();
              // io.print(F("NTP time, raw: "));
              // io.println(String(getNtpTime()));
              // io.print(F("RTC time, raw: "));
              // io.println(String(RTC.get()));
              io.println();
              io.print(F("NTP check finished. Time is: "));
              io.println(getTimeWithZeros());
              io.println(returnStatus);
              break;
              
            case 'T':   // Add one hour to Time
              io.println(F("H detected. Set RTC by providing timestamp."));
              io.print(F("T detected, adding one hour to RTC time for Daylight Saving update. Current time is: "));
              io.println(getTimeWithZeros());
              rtcSetStatus = RTC.set(now() + 3600);
              io.print(F("RTC is now set, return status: "));
              io.print(rtcSetStatus);
              io.print(F(" time is "));
              io.println(getTimeWithZeros());
              io.println();
              io.println(F("---===  Note!! It takes 10 minutes for the system time to update after RTC fix!! ===------"));
              break;
              
            case 't':   // Subtract one hour from Time
              io.print(F("t detected, subtracting one hour from time for Daylight Saving update. Current time is: "));
              io.println(getTimeWithZeros());
              rtcSetStatus = RTC.set(now() - 3600);
              io.print(F("RTC is now set, return status: "));
              io.print(rtcSetStatus);
              io.print(F(" time is "));
              io.println(getTimeWithZeros());
              io.println();
              io.println(F("---===  Note!! It takes 10 minutes for system time to update after RTC fix!! ===------"));
              break;

            case 'W': // adjust "wake up" time 15 minutes earlier
              io.print(F("W detected, adding 15 minutes to morning wake-up time... "));
              minutesBeforeSunrise = minutesBeforeSunrise + 15;
              if (minutesBeforeSunrise > 120) { minutesBeforeSunrise = 120; }
              eeCharTemp = minutesBeforeSunrise;
              EEPROM.put(eeMinutesBeforeSunrise, eeCharTemp);
              io.print(F(" - DONE, now waking up "));
              io.print(minutesBeforeSunrise);
              io.println(F(" minutes before sunrise."));
              break;

            case 'w': // adjust "wake up" time 15 minutes later
              io.print(F("w detected, subtracting 15 minutes from morning wake-up time... "));
              minutesBeforeSunrise = minutesBeforeSunrise - 15;
              if (minutesBeforeSunrise < -120) { minutesBeforeSunrise = -120; }
              eeCharTemp = minutesBeforeSunrise;
              EEPROM.put(eeMinutesBeforeSunrise, eeCharTemp);
              io.print(F(" - DONE, now waking up "));
              io.print(minutesBeforeSunrise);
              io.println(F(" minutes before sunrise."));
              break;

            case 'Z': // adjust sleep (ZZZzzz) time 15 minutes later
              io.print(F("S detected, adding 15 minutes to night go-to-sleep time... "));
              minutesAfterSunset = minutesAfterSunset + 15;
              if (minutesAfterSunset > 120) { minutesAfterSunset = 120; }
              eeCharTemp = minutesAfterSunset;
              EEPROM.put(eeMinutesAfterSunset, eeCharTemp);
              io.print(F(" - DONE, now going to sleep "));
              io.print(minutesAfterSunset);
              io.println(F(" minutes after sunset."));
              break;

            case 'z': // adjust sleep (ZZZzzz) time 15 minutes earlier
              io.print(F("s detected, subtracting 15 minutes from night go-to-sleep time... "));
              minutesAfterSunset = minutesAfterSunset - 15;
              if (minutesAfterSunset < -120) { minutesAfterSunset = -120; }
              eeCharTemp = minutesAfterSunset;
              EEPROM.put(eeMinutesAfterSunset, eeCharTemp);
              io.print(F(" - DONE, now going to sleep "));
              io.print(minutesAfterSunset);
              io.println(F(" minutes after sunset."));
              break;

            case 'Y': // add one extra upload retrY
              io.print(F("Y detected, adding one upload retry... "));
              if (uploadRetryNum < 5) { uploadRetryNum++; }
              EEPROM.update(eeUploadRetryNum, uploadRetryNum);
              io.print(F(" - DONE, "));
              io.print(uploadRetryNum);
              io.print(F(" extra retries ("));
              io.print(uploadRetryNum + 1);
              io.println(F(" total PUT attempts)."));
              break;

            case 'y': // subtract one extra upload retrY
              io.print(F("y detected, subtracting one upload retry... "));
              if (uploadRetryNum > 0) { uploadRetryNum--; }
              EEPROM.update(eeUploadRetryNum, uploadRetryNum);
              io.print(F(" - DONE, "));
              io.print(uploadRetryNum);
              io.print(F(" extra retries ("));
              io.print(uploadRetryNum + 1);
              io.println(F(" total PUT attempts)."));
              break;
              
            case '?':   // HELP
              // print help
              io.println(F("Options:"));
              io.println(F("q or Q: Quit. Disconnects telnet session."));
              io.println(F("M: Enter Athena TFTP update mode and reboot."));
              io.println(F("R: Reset unit (watchdog reset)."));
              io.println(F("C: Cache, print cached weather lines."));
              io.println(F("D: Dump SD file for today (only valid if SD card exists)."));
              io.println(F("U: Toggle ubiquiti between Always On and Off except for send every 5 minutes. Always off at night regardless."));
              io.println(F("H: Set time and date of RTC."));
              io.println(F("B: Camera viewing Brain."));
              io.println(F("S: Camera viewing South."));
              io.println(F("N: Camera viewing North."));
              io.println(F("A: Turn ON all Cameras (Brain Box, South, North)."));
              io.println(F("a: Turn OFF all Cameras (Brain Box, South, North)."));
              io.println(F("X: Bad Weather forecast, disable AUTO TURN ON for cameras. Can still be manually turned on."));
              io.println(F("O: God mode (10am until sleep, no solar/battery checks). Toggle."));
              io.println(F("J: Force an NTP check to see if the RTC should be updated."));
              io.println(F("T: Add one hour to RTC clock, for DST end in Fall. Takes 10 minutes to take effect!!"));
              io.println(F("t: Subtract one hour from RTC clock, for DST begin in Spring. Takes 10 minutes to take effect!!"));
              io.println(F("W: Add 15 minutes to morning wake time, wakes up earlier."));
              io.println(F("w: Subtract 15 minutes from morning wake time, wake up later."));
              io.println(F("Z: Add 15 minutes to night go-to-sleep (Zzzz) time, stay up later."));
              io.println(F("z: Subtract 15 minutes from night go-to-sleep (Zzzz) time, go to sleep earlier."));
              io.println(F("Y: Add one extra upload retry after PUT failure."));
              io.println(F("y: Subtract one extra upload retry after PUT failure."));
              io.println("");
              io.print(F("  Minutes before Sunrise: ")); io.println(minutesBeforeSunrise);
              io.print(F("  Minutes after Sunset:   ")); io.println(minutesAfterSunset);
              io.print(F("  Upload retries:         ")); io.print(uploadRetryNum);
              io.print(F(" extra (")); io.print(uploadRetryNum + 1); io.println(F(" total PUT attempts)"));
              io.print(F("  camStatus EEPROM value: ")); io.println(EEPROM.read(eeCamStatus));
              io.print(F("  Bad Weather bit:        ")); io.println(camStatus.badWeather);
              io.print(F("  God mode:               ")); io.println(camStatus.godMode);
              io.print(F("  CamSouth desired on:    ")); io.println(camStatus.SouthDesireOn);
              io.print(F("  CamNorth desired on:    ")); io.println(camStatus.NorthDesireOn);
              io.print(F("  CamBrain desired on:    ")); io.println(camStatus.BrainDesireOn);
              io.print(F("  Ubiquiti Keep on:       ")); io.println(EEPROM.read(eeKeepUbiOn));
              io.print(F("  Solar   V: "));              io.print(String(ina219a_solar_volts, 2));
                io.print(F(", mA: "));                     io.println(String(ina219a_solar_ma, 0));
              io.print(F("  Battery V: "));              io.print(String(ina219b_battery_volts, 2));
                io.print(F(", mA: "));                     io.println(String(ina219b_battery_ma, 0));
              io.print(F("  Lowest batt voltage since wake:  "));
              if (voltsLowestSeenValid) {
                io.println(String(voltsLowestSeen, 2));
              } else {
                io.println(F("n/a"));
              }
              io.print(F("  Lowest batt voltage today:       "));
              {
                byte voltsStored = EEPROM.read(eeVoltsLowestSeen);
                if (eeVoltsLowestStoredIsValid(voltsStored)) {
                  io.println(String(voltsStored / 10.0, 1));
                } else {
                  io.println(F("n/a"));
                }
              }
              io.print(F("  Lowest batt voltage day:  "));         io.println(String(EEPROM.read(eeVoltsLowestDay)));
              io.print(F("  Battery drain minutes: "));  io.print(String(battDrainMinutes));
                io.print(F(", mAm: "));                    io.print(String(battDrainmA));
                io.print(F(", mAh = "));                   io.println(String(battDrainmA / 60));
              io.print(F("  INA219 reads last min: "));  io.print(ina219_avgReadHz, 2);
              io.print(F(" Hz, this min so far: "));   io.println(String(ina219_sampleCount));
              io.print(F("  Temp internal: "));          io.println(String(bme280b.readTempC()));
              io.print(F("  Humidity int:  "));          io.println(String(bme280b.readFloatHumidity()));
              io.print(F("  Sunrise / Wake: "));
                io.print(strMinutesToHHMM(sunrise));
                io.print(" - "); io.print(minutesBeforeSunrise); io.print(" minutes = ");
                io.println(strMinutesToHHMM(sunrise - minutesBeforeSunrise));
              io.print(F("  Sunset / Sleep: "));
                io.print(strMinutesToHHMM(sunset));
                io.print(" + "); io.print(minutesAfterSunset); io.print(" minutes = ");
                io.println(strMinutesToHHMM(sunset + minutesAfterSunset));
              io.println();
              io.print(F("  Current time: "));           io.println(getTimeWithZeros());
              io.println();
              io.print(F("  Free ram:      "));          io.println(String(freeRam()));
              io.println(startupMessage);
              io.println();
              break;
          }

}

static bool telnetWelcomeSent = false;
static bool telnetPleaseWaitSent = false;
static bool telnetHoldCommands = false;
static bool telnetAcceptedThisCycle = false;

static bool telnetNetReady() {
#ifdef BENCH_MODE
  return ethEnabled;
#else
  return ethEnabled && wifiEnabled;
#endif
}

// Accept local telnet on the W5500 during the upload window even if WiFi is still starting.
static bool telnetAcceptReady() {
  if (!ethEnabled) {
    return false;
  }
#ifdef BENCH_MODE
  return true;
#else
  if (telnetHoldCommands || telnetAcceptedThisCycle || uploadPending) {
    return true;
  }
  return wifiEnabled;
#endif
}

void setTelnetHoldCommands(bool hold) {
  telnetHoldCommands = hold;
}

static void telnetDropStaleClient() {
  if (telnetHoldCommands || telnetAcceptedThisCycle) {
    return;
  }
  if (incomingClient && !incomingClient.connected() && !incomingClient.available()) {
    incomingClient.stop();
    telnetWelcomeSent = false;
    telnetPleaseWaitSent = false;
  }
}

static void telnetSendPleaseWait() {
  incomingClient.println(F("Connected - please wait while uploads finish."));
  telnetPleaseWaitSent = true;
}

// Accept TCP while Ethernet is up; does not read commands or send welcome.
void pollTelnetAcceptIfUploadWindow() {
  if (!ethEnabled) {
    return;
  }
  if (uploadPending || telnetHoldCommands || telnetAcceptedThisCycle) {
    pollTelnetAccept();
  }
}

void pollTelnetAccept() {
  if (!telnetAcceptReady()) {
    return;
  }
  telnetDropStaleClient();
  if (incomingClient.connected() || incomingClient.available()) {
    return;
  }
  // available() only returns a client after RX data; accept() returns ESTABLISHED idle connects.
  EthernetClient newClient = server.accept();
  if (newClient) {
    incomingClient = newClient;
    telnetWelcomeSent = false;
    telnetPleaseWaitSent = false;
    telnetAcceptedThisCycle = true;
    wxLogTag(F("NET"), F("telnet client connected"));
    if (telnetHoldCommands) {
      wxLogTag(F("NET"), F("telnet held until upload finishes"));
    }
    telnetSendPleaseWait();
  }
}

static void telnetSendWelcome() {
  incomingClient.println(F("Hi there, it's me, the Marshall weather station! Send '?' for help."));
  telnetWelcomeSent = true;
  msTemp = millis();
  wxLogTag(F("NET"), F("telnet ready for commands"));
}

// Run command loop for an accepted client. Welcome is sent here when commands are allowed.
bool serviceTelnetSession() {
  if (!telnetNetReady()) {
    return false;
  }
  if (!incomingClient.connected() && !incomingClient.available()) {
    return false;
  }

  usTemp = millis();
  bool timeoutWarningGiven = false;
  bool boolQuitSession = false;
  wdt_reset();

  if (!telnetWelcomeSent) {
    telnetSendWelcome();
  }

  while (incomingClient.connected() || incomingClient.available()) {
    if (incomingClient.available()) {
      msTemp = millis();
      char c = incomingClient.read();
      while (incomingClient.available()) {
        char extra = incomingClient.read();
        if (extra != '\r' && extra != '\n') {
          // ignore stray line endings only; other bytes are not a second command here
        }
      }
      handleRemoteCommand(c, incomingClient, boolQuitSession);
    }
    wdt_reset();
    if (boolQuitSession) {
      incomingClient.println("Session quit, DISCONNECTING.");
      break;
    }
    if ((millis() - msTemp) > telnetIdleTimeoutMs) {
      incomingClient.println("Idle timeout reached, DISCONNECTING.");
      break;
    } else if ((millis() - msTemp) > telnetIdleWarningMs) {
      if (not timeoutWarningGiven) {
        incomingClient.println("Idle timeout in 20 seconds....");
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
  telnetWelcomeSent = false;
  telnetPleaseWaitSent = false;
  telnetAcceptedThisCycle = false;
  Serial.print(F("[NET] telnet disconnected ("));
  Serial.print(millis() - usTemp);
  Serial.println(F(" ms)"));
  telnetSeconds = (millis() - usTemp) / 1000;
  return true;
}

/* Accept (if needed) and service one telnet session. Deferred while telnetHoldCommands is set. */
bool checkEthIncomingData() {
  if (!telnetNetReady()) {
    return false;
  }
  wdt_reset();
  pollTelnetAccept();
  if (telnetHoldCommands) {
    return false;
  }
  return serviceTelnetSession();
}

void finishTelnetAfterUpload() {
  server.begin(); // NTP UDP may have recycled sockets; keep TCP telnet listening.
  telnetHoldCommands = false;
  if (telnetAcceptedThisCycle) {
    uint32_t until = millis() + telnetIdleTimeoutMs;
    while (millis() < until) {
      if (incomingClient.connected() || incomingClient.available()) {
        serviceTelnetSession();
        return;
      }
      pollTelnetAccept();
      wdt_reset();
      delay(50);
    }
    wxLogTag(F("NET"), F("telnet client lost before command prompt"));
    incomingClient.stop();
    telnetAcceptedThisCycle = false;
    telnetPleaseWaitSent = false;
    return;
  }
  Serial.print(F("[NET] telnet listen ("));
  Serial.print(waitTimeIncomingClient);
  Serial.println(F("s)"));
  uint32_t listenUntil = millis() + (unsigned long)waitTimeIncomingClient * 1000UL;
  while (millis() < listenUntil) {
    pollTelnetAccept();
    if (incomingClient.connected() || incomingClient.available()) {
      telnetAcceptedThisCycle = true;
      serviceTelnetSession();
      return;
    }
    wdt_reset();
    delay(50);
  }
}

void setKeepUbiquitiOn(bool v) {
    keepUbiquitiOn = v;
    EEPROM.update(eeKeepUbiOn, v);
}

// Activate power supply and SPI bus of Ethernet card. Nothing else! 
void ethernetPowerOn(){
  // Turn shield on:
  pinMode       (PIN_ETH_RESET, OUTPUT);          // prepare ETH reset control pin
  digitalWrite  (PIN_ETH_RESET, ETH_RESET);       // turn ETH reset on, transition to power with reset active  
  pinMode       (PIN_ETH_POWER, OUTPUT);          // prepare ETH power control pin
  digitalWrite  (PIN_ETH_POWER, ETH_ON);          // turn ETH shield on
  delay(100);
  digitalWrite  (PIN_ETH_RESET, ETH_NORESET);     // turn ETH reset off
  delay(100);
  // Activate SPI bus:
  pinMode       (SCK,   OUTPUT);
  pinMode       (MISO,  INPUT);
  pinMode       (MOSI,  OUTPUT);
  digitalWrite  (MISO,  HIGH);                    // SPI pullup
  pinMode       (SS,    OUTPUT);
  digitalWrite  (SS,    HIGH);
  Ethernet.init (SS);                             // This pin ChipSelects the W5500 board on the SPI bus
  delay(100);                                     // jjjjj? delay needed?? from 1500
  wdt_reset();
}


// Deactivate power supply and SPI bus of Ethernet card. Nothing else! 
void ethernetPowerOff(){
  // Deactivate SPI bus:
  SPI.end(); 
  pinMode       (MOSI,  INPUT);                   // prevents leakage through W5500 ESD diodes
  pinMode       (MISO,  INPUT);                   // prevents leakage through ESD diodes
  pinMode       (SCK,   INPUT);                   // prevents leakage through ESD diodes
  pinMode       (SS,    INPUT);                   // prevents leakage through ESD diodes
  // Turn shield off:
  pinMode       (PIN_ETH_RESET, OUTPUT);          // prepare ETH reset control pin
  digitalWrite  (PIN_ETH_RESET, ETH_RESET);       // turn on (and keep on) ETH reset for smoother power transition 
  pinMode       (PIN_ETH_POWER, OUTPUT);          // prepare ETH power control pin
  digitalWrite  (PIN_ETH_POWER, ETH_OFF);         // turn ETH shield off
}


// Turns on power for components needed for network connectivity
void enableEthernet(bool quiet) {
  // DON'T KEEP ENABLING ONCE IT'S ALREADY ENABLED! It's wasteful. Also it makes an endless loop if the 30 seconds crosses the "second zero" boundary.
  if (ethEnabled) return;
  wdt_reset();
  if (!quiet) wxLogTag(F("NET"), F("enabling ethernet..."));
  ethernetPowerOn();                              // Activate power supply and SPI bus
  Ethernet.begin(mac, ip, dnsServer, gateway, subnet); // Eth must be initialized after each power up
  delayWithWdt(EthStartupDelay);                  // must wait at least @ 5000ms; feed WD during wait
  wdt_reset();
  if (!quiet) {
    Serial.print(F("[NET] ethernet up  IP "));
    Serial.println(Ethernet.localIP());
  }
  if (resolveCssServerIp() && !quiet) {
    Serial.print(F("[NET] DNS "));
    Serial.print(wxSiteName);
    Serial.print(F(" -> "));
    Serial.println(cssServerIp);
  }
  W5100.setRetransmissionTime(0x07D0);            // reduce wait
  W5100.setRetransmissionCount(4);
  server.begin();                                 //jjjjj? why is that here?
  delayWithWdt(100);                              //jjjjj down from 2500
  ethEnabled = true;
  wdt_reset();
}

void resetEthernet(){ // Resets the ethernet shield. Delays incorporated! Takes about 5.3 seconds.
  wdt_reset(); // keep WD away
  Serial.print("Resetting ethernet card...");
  pinMode(PIN_ETH_RESET, OUTPUT);                 // prepares ETH reset control pin
  digitalWrite(PIN_ETH_RESET, ETH_RESET);         // reset W5500
  delay(100);                                     // for 100ms
  digitalWrite(PIN_ETH_RESET, ETH_NORESET);       // turn off reset, let W5500 boot up
  delay(100);                                     // for 100ms
  pinMode(SS,   OUTPUT);
  digitalWrite (SS, HIGH);                        // select master mode for SPI bus
  Ethernet.init (SS) ;                            // This pin ChipSelects the W5500 board on the SPI bus
  delay(100);                                     // jjjjj? is this time delay needed at all? check source?
  Ethernet.begin(mac, ip, dnsServer, gateway, subnet); // Eth must be initialized after each power up
  Serial.println("Ethernet.begin executed. Wating for 5 sec"); 
  delayWithWdt(5000);                           // must wait at least 5000ms; feed WD during wait
  wdt_reset();
  W5100.setRetransmissionTime(0x07D0);            // reduce wait
  W5100.setRetransmissionCount(4);                // reduce retries
  Serial.println("Done.");
}

// Athena bootloader network block (internal EEPROM bytes 0-69).
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

// Turns off power for network components to save power
void disableEthernet(bool quiet) {
  if (!quiet) {
    Serial.print(F("[NET] ethernet off  "));
    Serial.println(getTimeWithZeros());
  }
  
  if (hour() == 11 and minute() > 48) {
    // Leave it all on for ~10 minutes, once a day. Just-in-case.
    if (!quiet) wxLogTag(F("NET"), F("ethernet held on (11:50-12:00)"));
    return;
  }
  
  incomingClient.stop();
  client.stop();
  telnetWelcomeSent = false;
  telnetPleaseWaitSent = false;
  telnetHoldCommands = false;
  telnetAcceptedThisCycle = false;

  ethernetPowerOff();                             // Turn off Ethernet shield

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

  pinMode(PIN_UBIQUITI_POWER, OUTPUT);                 // prepares Ubiquiti power control pin
  digitalWrite(PIN_UBIQUITI_POWER, UBIQUITI_ON);  // turns Ubiquiti on
  wifiStartTime = millis();
  // Rest of enabling happens in loop() after wifiStartupDelay seconds (50s from :10 -> :00).
}

void waitForWifi() {
  while(true){
    wdt_reset();  
    if (not (int((millis() - wifiStartTime) / 1000) % 10)) { 
      Serial.print(F("[NET] wifi starting... "));
      Serial.print((millis() - wifiStartTime) / 1000,10);
      Serial.println(F("s"));
    }
    if ((millis() - wifiStartTime) / 1000 >= wifiStartupDelay) {
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
  pinMode(PIN_UBIQUITI_POWER, OUTPUT);                  // prepares Ubiquiti power control pin
  digitalWrite(PIN_UBIQUITI_POWER, UBIQUITI_OFF);  // turns Ubiquiti off
  wifiEnabled = false;
  wifiStartTime = 0;

}


//Prints to serial the pinmode of SPI pins. For debugging obviously.
void PrintSpiPinMode() {
  return; // DISABLED !! DISABLED !! DISABLED
  Serial.print(  "I: " ); Serial.print(INPUT);
  Serial.print(", O: " ); Serial.print(OUTPUT);
  Serial.print(", Ip: "); Serial.println(INPUT_PULLUP);
  Serial.print("MOSI: "); Serial.println(getPinMode(MOSI));
  Serial.print("MISO: "); Serial.println(getPinMode(MISO));
  Serial.print("SCK : "); Serial.println(getPinMode(SCK ));
  Serial.print("SS  : "); Serial.println(getPinMode(SS  ));
  Serial.println();
}

#define UNKNOWN_PIN 0xFF
uint8_t getPinMode(uint8_t pin)
{
  uint8_t bit = digitalPinToBitMask(pin);
  uint8_t port = digitalPinToPort(pin);

  // I don't see an option for mega to return this, but whatever...
  if (NOT_A_PIN == port) return UNKNOWN_PIN;

  // Is there a bit we can check?
  if (0 == bit) return UNKNOWN_PIN;

  // Is there only a single bit set?
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
  // Calculate sunrise and sunset, based on Los Angeles. Close enough.
  // Calculated in minutes of the day. E.g. sunrise at 6 am -> sunrise = 360
  // From http://forum.arduino.cc/index.php/topic,66426.msg487457.html#msg487457
  float common = cos( ((month()-1)*30.5+day() + 8 ) / 58.091554);
  sunrise = 349.5 + 66.5 * common;
  sunset =  1078.5 - 69.5 * common;
  if (CheckDST()) {
    sunrise = sunrise + 60;
    sunset = sunset + 60;
  }
  sunriseDay = day();
  Serial.print(F("  sunrise "));
  Serial.print(sunrise / 60); Serial.print(F(":")); Serial.println(sunrise % 60);
  Serial.print(F("  sunset  "));
  Serial.print(sunset  / 60); Serial.print(F(":")); Serial.println(sunset  % 60);
}

boolean CheckDST(){
/*
Starts: Second Sunday in March
Ends: First Sunday in November
From: http://forum.arduino.cc/index.php?topic=66426.15
*/     
    //January, february, and december are out.
    if (month() < 3 || month() > 11) { return false; }
    //April to October are in
    if (month() > 3 && month() < 11) { return true; }
    int previousSunday = day() - weekday();
    //In march, we are DST if our previous sunday was on or after the 8th.
    if (month() == 3) { return previousSunday >= 8; }
    //In november we must be before the first sunday to be dst.
    //That means the previous sunday must be before the 1st.
    return previousSunday <= 0;
}

int getTimeZone(void) {
      if (CheckDST()) {
        return -7;
      } else {
        return -8;
      }
}

 
// Bring up WiFi/Ethernet before NTP (same prerequisites as upload). Feeds watchdog during long waits.
static void ensureNetworkForNtp(Stream &io) {
  wdt_reset();
  if (ethEnabled) {
    return;
  }
#ifndef BENCH_MODE
  if (!wifiEnabled) {
    if (!wifiStartTime) {
      io.println(F("Enabling WiFi for NTP (up to 50s)..."));
      enableWifi(false);
    }
    waitForWifi();
  }
#endif
  if (!ethEnabled) {
    io.println(F("Enabling Ethernet for NTP..."));
    enableEthernet(false);
  }
  wdt_reset();
}

/*-------- NTP code ----------*/
/*****************************************************************************
  _______   _____   __  __   ______         __    _   _   _______   _____  
 |__   __| |_   _| |  \/  | |  ____|       / /   | \ | | |__   __| |  __ \ 
    | |      | |   | \  / | | |__         / /    |  \| |    | |    | |__) |
    | |      | |   | |\/| | |  __|       / /     | . ` |    | |    |  ___/ 
    | |     _| |_  | |  | | | |____     / /      | |\  |    | |    | |     
    |_|    |_____| |_|  |_| |______|   /_/       |_| \_|    |_|    |_|     

 *****************************************************************************
 */

time_t getNtpTime(bool quiet)
{
  wdt_reset();
  Udp.begin(localPort);
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  if (!quiet) {
    wxLogTag(F("NTP"), F("request"));
  }
  sendNtpPacket(timeServer);
  wdt_reset();
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    wdt_reset();
    pollTelnetAccept();
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      if (!quiet) {
        wxLogTag(F("NTP"), F("response"));
      }
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
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
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address (DNS lookup version, see IPAddress() version below)
void sendNtpPacket(IPAddress &address)
{
  wdt_reset();
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
  wdt_reset();
}


void logNtpDrift(time_t ntp, time_t rtc, time_t nowBefore, const __FlashStringHelper* suffix) {
  Serial.print(F("[NTP] NTP-RTC "));
  Serial.print((long)ntp - (long)rtc);
  Serial.print(F("s  RTC-now "));
  Serial.print((long)rtc - (long)nowBefore);
  Serial.print(F("s"));
  if (suffix) {
    Serial.print(F(", "));
    Serial.println(suffix);
  } else {
    Serial.println();
  }
}

void compareRTCwithNTP() {
  wdt_reset();

  if (CheckDST()) {
    timeZone = -7;
  } else {
    timeZone = -8;
  }

  returnStatus += "TZ: ";
  returnStatus += String(timeZone);

  time_t nowBefore = now();
  time_t timeRTC = RTC.get();
  time_t timeNTP = getNtpTime(true);
  if ((timeRTC == 0) or (timeNTP == 0)) {
    returnStatus += " RTC or NTP was 0. ";
    return;
  }

  long ntpRtcDrift = (long)timeNTP - (long)timeRTC;
  long rtcNowDrift = (long)timeRTC - (long)nowBefore;
  returnStatus += F(" NTP-RTC ");
  returnStatus += String(ntpRtcDrift);
  returnStatus += F("s RTC-now ");
  returnStatus += String(rtcNowDrift);
  returnStatus += F("s.");

  unsigned int absNtpRtc = (ntpRtcDrift < 0) ? (unsigned int)(-ntpRtcDrift) : (unsigned int)ntpRtcDrift;
  if (absNtpRtc > 5) {
    wdt_reset();
    byte rtcSetStatus = RTC.set(timeNTP);
    wdt_reset();
    if (rtcSetStatus) {
      logNtpDrift(timeNTP, timeRTC, nowBefore, F("RTC update FAILED"));
      returnStatus += F(" RTC set failed: ");
      returnStatus += String(rtcSetStatus);
    } else {
      setTime(timeNTP);
      logNtpDrift(timeNTP, timeRTC, nowBefore, F("RTC updated"));
      returnStatus += F(" RTC updated.");
    }
  } else {
    logNtpDrift(timeNTP, timeRTC, nowBefore, F("RTC unchanged"));
    returnStatus += F(" RTC unchanged.");
  }
}


bool isTimeValid(time_t time_to_check){
  // Compares time_to_check year to the year of the Software Version Date. If it is more than 1 year before or more then 5 years after, time is not valid!
  int ttc_year = year(time_to_check);
  if ((ttc_year >= version_year-1) and (ttc_year <= version_year + 5)) return true;
  return false;
}


bool setArduinoTimeWithNtp(){
  // gets time from NTP server but checks validiy before setting Arduino time with it. Returns success as boolean.
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

// Minutes after midnight when the station will sleep (sunset + post-sunset offset).
int wxSleepMinutesAfterMidnight() {
  return sunset + minutesAfterSunset;
}

// True when this minute's weather line should be tagged before the next sleep.
bool wxIsLastUploadBeforeSleep() {
#ifndef BENCH_MODE
#ifdef DONT_SLEEP
  if (wxDontSleep) return false;
#endif
  if (ina219b_battery_volts < battery_critical_voltage) {
    return true;
  }
  int m = hour() * 60 + minute();
  int sleepAt = wxSleepMinutesAfterMidnight();
  if (m > sleepAt) {
    return false;
  }
  int lastUpload = (sleepAt / 5) * 5;
  return m == lastUpload;
#else
  return false;
#endif
}

// Given minutes-after-midnight (like int sunset or int sunrise), turn that into military HH:MM with padded zeros
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

// Compact station-config suffix for the first upload after boot (sub, ip, gw, q3).
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

// Build the HTTP PUT into buf using readable String += assembly.
// Returns PUT length, or 0 if weather is empty or buf is too small.
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
