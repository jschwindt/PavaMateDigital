#if defined(__AVR_ATtiny85__)
  #define LED             0  // pin 5
  #define RELAY           1  // pin 6
  #define ANALOG_IN       A1 // pin 7
  #define debug(str)
  #define debugln(str)
#else
  #define SERIAL_ENABLE
  #define LED             13
  #define RELAY           12
  #define ANALOG_IN       A0
  #define debug(str)      Serial.print(str)
  #define debugln(str)    Serial.println(str)
#endif

#define BLINK_SLOW_LEVEL  250
#define TEMP_60           450
#define TEMP_80           600
#define TEMP_85           640
#define BLINK_FAST_LEVEL  650
#define BLINK_SLOW_TIME   750
#define BLINK_FAST_TIME   33
#define OFF               0
#define ON                1

///////////////////////////////////////////////////////////////////////////////////////////////////////
// TIMERS HANDLING
unsigned long currentMillis;
#define update_millis()       currentMillis = millis()
#define start_timer(timer)    timer = currentMillis

unsigned int elapsed_millis(unsigned long initialTime) {
  return currentMillis - initialTime;
}
unsigned long timerLed;
unsigned long timerTic;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// TIC HANDLING
#define TIC_TIME         200
#define TIC_SECS(secs)   (secs * (1000 / TIC_TIME))
unsigned int tic;
#define update_tic()     tic++
#define start_tic(timer) timer = tic

unsigned int elapsed_secs(unsigned int initialTime) {
  return tic - initialTime;
}
unsigned int ticRelayTimer;
unsigned int relayTime;

///////////////////////////////////////////////////////////////////////////////////////////////////////
int analogData, analogData1, analogData2;
int blinkTime;
enum State { KEEP_WARM, HEATING, HEATING_END } state;

///////////////////////////////////////////////////////////////////////////////////////////////////////
/// Lee el valor analógico correspondiente a la temperatura y lo promedia con los 2 valores anteriores

void readTemp() {
  int ad = analogRead(ANALOG_IN);
  analogData = (ad + analogData1 + analogData2) / 3;
  analogData1 = analogData2;
  analogData2 = ad;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
/// Initial setup

void setup() {
  pinMode(LED, OUTPUT);
  digitalWrite(LED, OFF);
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, OFF);
  state = KEEP_WARM;
  tic = 0;
  readTemp(); readTemp(); readTemp();
  #if defined(SERIAL_ENABLE)
    Serial.begin(9600);
  #endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
/// Actualiza el status del led de la siguiente forma:
///   state == KEEP_WARM => led ON (el agua llegó a la temperatura)
///   analogData > BLINK_FAST_LEVEL => led blink at 10hz
///   analogData en el medio, calclua el timer de manera que se acelere al aumentar analogData
///   analogData < BLINK_SLOW_LEVEL => LED blink at 1hz

void update_led() {
  if (state == KEEP_WARM) {
    digitalWrite(LED, ON);
    return;
  }
  if (analogData > BLINK_FAST_LEVEL) {
    blinkTime = BLINK_FAST_TIME;
  }
  else if (analogData < BLINK_SLOW_LEVEL ) {
    blinkTime = BLINK_SLOW_TIME;
  }
  else {
    blinkTime = BLINK_SLOW_TIME - ((unsigned long)analogData - BLINK_SLOW_LEVEL) * (BLINK_SLOW_TIME - BLINK_FAST_TIME) / (BLINK_FAST_LEVEL - BLINK_SLOW_LEVEL);
  }
  if (elapsed_millis(timerLed) > blinkTime) {
    start_timer(timerLed);
    digitalWrite(LED, !digitalRead(LED));
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
/// STATE: KEEP_WARM

void handle_keep_warm() {
  if (analogData < TEMP_80) {
    start_tic(ticRelayTimer);
    digitalWrite(RELAY, ON);
    relayTime = TIC_SECS(8);
    state = HEATING;
    debug("HEATING for "); debugln(relayTime);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
/// STATE: HEATING

void handle_heating() {
  if (analogData > TEMP_85) {
    digitalWrite(RELAY, OFF);
    state = KEEP_WARM;
    debugln("Goto KEEP_WARM 1");
  }
  else if (elapsed_secs(ticRelayTimer) > relayTime) {
    start_tic(ticRelayTimer);
    if (analogData < TEMP_60) {
      relayTime = TIC_SECS(8);
      debug("STILL HEATING for "); debugln(relayTime);
    }
    else {
      digitalWrite(RELAY, OFF);
      state = HEATING_END;
      debugln("Goto HEATING_END");
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
/// STATE: HEATING_END

void handle_heating_end() {
  if (analogData > TEMP_85) {
    state = KEEP_WARM;
    debugln("Goto KEEP_WARM 2");
  }
  else if (elapsed_secs(ticRelayTimer) > TIC_SECS(10)) {
    start_tic(ticRelayTimer);
    digitalWrite(RELAY, ON);
    relayTime = TIC_SECS(8);
    state = HEATING;
    debug("BACK TO HEATING for "); debugln(relayTime);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
/// Main loop

void loop() {
  update_millis();

  update_led();

  if (elapsed_millis(timerTic) > TIC_TIME) {
    start_timer(timerTic);
    update_tic();

    readTemp();

    switch (state) {
      case KEEP_WARM:   handle_keep_warm();   break;
      case HEATING:     handle_heating();     break;
      case HEATING_END: handle_heating_end(); break;
    }

    debug("AD = ");  debug(analogData);
    debug(" S = ");  debug(state);
    debug(" RO = "); debugln(digitalRead(RELAY));
  }
}
