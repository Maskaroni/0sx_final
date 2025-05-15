#include "Alarm.h"
#include "ViseurAutomatique.h"
#include "DHT.h"

#include <LCD_I2C.h>
#include <HCSR04.h>
#include <U8g2lib.h>
#include <WiFiEspAT.h>
#include <PubSubClient.h>

#define HAS_SECRETS 1
#define AT_BAUD_RATE 115200
#define DHTPIN 6
#define DHTTYPE DHT11
#define TRIGGER_PIN 11
#define ECHO_PIN 12
#define p1 2
#define p2 3
#define p3 4
#define p4 5
#define rPin 8
#define gPin 7
#define bPin 9
#define alarmPin 10
#define LED_BUILTIN 13

#if HAS_SECRETS
#include "arduino_secrets.h"
const char ssid[] = SECRET_SSID;
const char pass[] = SECRET_PASS;
#else
const char ssid[] = "TechniquesInformatique-Etudiant";  // network SSID (name)
const char pass[] = "shawi123";                         // network password (use for WPA, or use as key for WEP)
#endif
#define DEVICE_NAME "deviceMiL"
#define MQTT_PORT 1883
#define MQTT_USER "etdshawi"
#define MQTT_PASS "shawi123"
const char* mqttServer = "216.128.180.194";

WiFiClient espClient;
PubSubClient client(espClient);
int status = WL_IDLE_STATUS;

LCD_I2C lcd(0x27, 16, 2);
HCSR04 hc(TRIGGER_PIN, ECHO_PIN);

#define DIN_PIN 26
#define CLK_PIN 22
#define CS_PIN 24
U8G2_MAX7219_8X8_F_4W_SW_SPI u8g2(U8G2_R0, CLK_PIN, DIN_PIN, CS_PIN, U8X8_PIN_NONE, U8X8_PIN_NONE);

DHT dht(DHTPIN, DHTTYPE);

unsigned long currentTime = 0;

float distance = 0;
float humidity = 0;
float temperature = 0;
const int zero = 0;
float maxNear = 30;
float maxFar = 60;
const float left = 10.0;
const float right = 170.0;
int alarmTreshold = 15;
bool changeAlarmState = false;
bool isMatrixOn = false;
int whichIcon = 0;
String fullCmd = "";
String cmd = "";
String arg1 = "";
String arg2 = "";

Alarm alarm(rPin, gPin, bPin, alarmPin, &distance);
ViseurAutomatique motor(p1, p2, p3, p4, &distance);

//SETUP AND LOOP
void setup() {                                                                                  //setup
  Serial.begin(AT_BAUD_RATE);

  dht.begin();

  lcd.begin(AT_BAUD_RATE);
  lcd.backlight();

  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  initiationAlarm();
  initiationMotor();

  u8g2.begin();
  u8g2.setContrast(7);
  u8g2.clearBuffer();
  u8g2.sendBuffer();

  initiationMqtt();

  Serial.println("Setup completed!");

  lcd.print("2486739");
  lcd.setCursor(0, 2);
  lcd.print("MUAHAHAH");
  Serial.println("The screen has displayed my student's and lab number already!");
  delay(2000);

  screen();
}

void loop() {                                                                                   //loop
  currentTime = millis();
  client.loop();
  getDistance(currentTime);

  alarm.update();
  motor.update();
  checkIfShouldBeSent();
  refreshScreen();
  dhtTask(currentTime);
  processCommands(currentTime);
}

//INITIATION//////////////////////////////////////////////////////////////////////////////////////
void initiationAlarm() {                                                                        //initiationAlarm
  alarm.setColourA(255, 0, 0);
  alarm.setColourB(0, 0, 255);
  alarm.setDistance(alarmTreshold);
  alarm.setTimeout(3000);
  alarm.setVariationTiming(70);
}

void initiationMotor() {                                                                        //initiationMotor
  motor.setPasParTour(2048);
  motor.setAngleMin(left);
  motor.setAngleMax(right);
  motor.setDistanceMinSuivi(maxNear);
  motor.setDistanceMaxSuivi(maxFar);
  motor.activer();
}

void initiationMqtt() {
  wifiInit();
  client.setServer(mqttServer, MQTT_PORT);
  client.setCallback(mqttEvent);
  if(!client.connect(DEVICE_NAME, MQTT_USER, MQTT_PASS)) {
    Serial.println("Incapable de se connecter sur le serveur MQTT");
    Serial.print("client.state : ");
    Serial.println(client.state());
  } else{
    Serial.println("Connecté sur le serveur MQTT");
  }
  client.subscribe("etd/15/motor", 0);
  if (client.subscribe("etd/15/motor", 0)) {
    Serial.println("Abonnement à 'motor' complété.");
  } else {
    Serial.println("Erreur. Abonnement à 'motor' impossible.");
  }

  client.subscribe("etd/15/color", 0);
  if (client.subscribe("etd/15/color", 0)) {
    Serial.println("Abonnement à 'color' complété.");
  } else {
    Serial.println("Erreur. Abonnement à 'color' impossible.");
  }
}

//WIFI////////////////////////////////////////////////////////////////////////////////////////////
void wifiInit() {
  Serial1.begin(AT_BAUD_RATE);
  WiFi.init(&Serial1);
  
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("La communication avec le module WiFi a échoué!");

    while (true) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(50);
    }
  }
  
  Serial.println("En attente de connexion au WiFi");
  while (status != WL_CONNECTED) {
    Serial.print('.');
    status = WiFi.begin(ssid, pass);
  }
  Serial.println();

  IPAddress ip = WiFi.localIP();
  Serial.println();
  Serial.println("Connecté au réseau WiFi.");
  Serial.print("Adresse : ");
  Serial.println(ip);

  printWifiStatus();
}

void printWifiStatus() {                                                                        //printWifiStatus

  // imprimez le SSID du réseau auquel vous êtes connecté:
  char ssid[33];
  WiFi.SSID(ssid);
  Serial.print("SSID: ");
  Serial.println(ssid);

  // imprimez le BSSID du réseau auquel vous êtes connecté:
  uint8_t bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC: ");
  printMacAddress(mac);

  // imprimez l'adresse IP de votre carte:
  IPAddress ip = WiFi.localIP();
  Serial.print("Adresse IP: ");
  Serial.println(ip);

  // imprimez la force du signal reçu:
  long rssi = WiFi.RSSI();
  Serial.print("force du signal (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void printMacAddress(byte mac[]) {                                                              //printMacAddress
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}

bool reconnect() {                                                                              //reconnect
  bool result = client.connect(DEVICE_NAME, MQTT_USER, MQTT_PASS);
  if(!result) {
    Serial.println("Incapable de se connecter sur le serveur MQTT");
  }
  return result;
}

void mqttEvent(char* topic, byte* payload, unsigned int length) {                               //mqttEvent
  Serial.print("Message reçu [");
  Serial.print(topic);
  Serial.print("] : ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  String msg = String((char*)payload);
  msg.trim();
  String command = "";

  int firstStep = msg.indexOf(':');
  int finalStep = msg.indexOf('}', firstStep + 1);

  if (strcmp(topic, "etd/15/motor") == 0) {
    command = msg.substring(firstStep + 1, finalStep);
    int motorState = command.toInt();
    switchMotor(motorState);
  }
  else if (strcmp(topic, "etd/15/color") == 0) {
    command = msg.substring(firstStep + 3, finalStep - 1);
    changeColor(command);
  }
  else {
    Serial.println("Not applicable.");
  }
}

//WIFI SEND///////////////////////////////////////////////////////////////////////////////////////
void checkIfShouldBeSent() {                                                                    //checkIfShouldBeSent
  static float angle = 0;

  if (!(distance < maxNear) && !(distance > maxFar)) {
    angle = motor.getAngle();
  }
  sendInfoWifi(angle);
}

void sendInfoWifi(float angle) {                                                                //sendInfoWifi
  static unsigned long lastTime = 0;
  const unsigned int rate = 2500;
  static bool firstTime = true;

  static char message[200] = "";
  static char stringDist[6];
  static char stringAngle[6];
  static char stringTemp[6];
  static char stringHum[6];
  dtostrf(distance, 3, 1, stringDist);
  if (motor.getEtatTexte() != "INACTIF") {
    dtostrf(angle, 3, 1, stringAngle);
  }
  dtostrf(temperature, 3, 1, stringTemp);
  dtostrf(humidity, 3, 1, stringHum);

  if (firstTime) {
    sprintf(message, "{\"motor\":1,\"color\":\"#000000\"}");
    client.publish("etd/15/data", message);
    firstTime = false;
  }

  sendLinesLcd(stringDist, stringAngle, message);
  sprintf(message, "{\"uptime\":%lu}", (currentTime/1000));
  client.publish("etd/15/data", message);

  if (currentTime - lastTime < rate) {return;}
  lastTime = currentTime;

  sprintf(message, "{\"name\":\"Milanne Lacerte\",\"number\":\"2486739\",\"dist\":%s,\"angle\":%s,\"temp\":%s,\"hum\":%s}", stringDist, stringAngle, stringTemp, stringHum);
  client.publish("etd/15/data", message);
}

void sendLinesLcd(char* stringDist, char* stringAngle, char* message) {                         //sendLinesLcd
  static unsigned long lastTime = 0;
  const unsigned int rate = 1100;

  char lcdDist[13] = "";
  char lcdAngle[13] = "";
  sprintf(lcdDist, "Dist: %s cm", stringDist);
  if (motor.getEtatTexte() == "INACTIF") {
    sprintf(lcdAngle, "INACTIF");
  }
  else {
    if (!(distance < maxNear) && !(distance > maxFar)) {
      sprintf(lcdAngle, "Obj: %s deg", stringAngle);
    }
    else if (distance < maxNear) {
      sprintf(lcdAngle, "Obj: Trop pret");
    }
    else {
      sprintf(lcdAngle, "Obj: Trop loin");
    }
  }

  if (currentTime - lastTime < rate) return;
  lastTime = currentTime;

  sprintf(message, "{\"line1\":\"%s\",\"line2\":\"%s\"}", lcdDist, lcdAngle);
  client.publish("etd/15/data", message);
}

void switchMotor(int state) {                                                                   //switchMotor
  switch (state) {
    case 0:
      motor.desactiver();
      break;
    case 1:
      motor.activer();
      break;
    default:
      Serial.println("État demandé au moteur non-existant");
      break;
  }
}

void changeColor(String hexColor) {
  if (hexColor != "000000") {
  String color1 = hexColor.substring(0, 2);
  String color2 = hexColor.substring(2, 4);
  String color3 = hexColor.substring(4, 6);

  int r = (int)strtol(color1.c_str(), NULL, 16);
  int g = (int)strtol(color2.c_str(), NULL, 16);
  int b = (int)strtol(color3.c_str(), NULL, 16);

  static char newColor[30] = "";
  sprintf(newColor, "Couleur changée pour r:%d, g:%d, b:%d", r, g, b);
  Serial.println(newColor);

  alarm.setColourA(r, g, b);
  alarm.setColourB(r, g, b);
  }
}

//COMMANDS////////////////////////////////////////////////////////////////////////////////////////
void processCommands(unsigned long currentTime) {                                               //processCommands
  enum Icon {NO_ICON, CHECK, ERROR, LIMIT_ROSSED};
  Icon _whichIcon = NO_ICON;
  bool isWrong = false;

  displayIcon(currentTime, whichIcon);

  if (Serial.available() > 0) {
    fullCmd = Serial.readStringUntil('\n');
    Serial.println("Arduino a compris: '" + fullCmd + "'");
    analyseCommand();

    if (cmd == "gDist" || cmd == "g_dist") {
      Serial.println(distance);
      isMatrixOn = true;
      whichIcon = CHECK;
    }
    else if (cmd == "cfg") {
      if (arg1 == "alm") {
        alarmTreshold = arg2.toInt();
        alarm.setDistance(alarmTreshold);
        isMatrixOn = true;
        whichIcon = CHECK;
      }
      else if (arg1 == "lim_inf" || arg1 == "lim_sup") {
        isMatrixOn = true;
        whichIcon = processLimitsMotor();
      }
      else {isWrong = true;}
    }
    else {isWrong = true;}


    if (isWrong) {
      Serial.println("Ce n'est pas une commande valide, recommencez.");
      isMatrixOn = true;
      whichIcon = ERROR;
    }
  }
}

void analyseCommand() {                                                                         //analyseCommand
  cmd = "";
  arg1 = "";
  arg2 = "";

  int firstStep = fullCmd.indexOf(';');
  int secondStep = fullCmd.indexOf(';', firstStep + 1);

  if (firstStep == -1) {
    cmd = fullCmd;
    return;
  }

  cmd = fullCmd.substring(0, firstStep);

  if (secondStep != -1) {
    arg1 = fullCmd.substring(firstStep + 1, secondStep);
    arg2 = fullCmd.substring(secondStep + 1);
  } else {
    arg1 = fullCmd.substring(firstStep + 1);
  }
}

int processLimitsMotor() {                                                                      //processLimitsMotor
  int pastLimit;
  int icon;

  if (arg1 == "lim_inf") {
    pastLimit = maxNear;

    maxNear = arg2.toInt();

    icon = 1;

    if (maxNear >= maxFar) {
    Serial.println("  ERREUR! \n Limite inférieure plus grande que limite supérieure.");
    maxNear = pastLimit;
    icon = 3;
    }
  }
  else if (arg1 == "lim_sup") {
    pastLimit = maxFar;

    maxFar = arg2.toInt();

    icon = 1;

    if (maxNear >= maxFar) {
    Serial.println("  ERREUR! \n Limite inférieure plus grande que limite supérieure.");
    maxFar = pastLimit;
    icon = 3;
    }
  }
  else {
    Serial.println("Ce n'est pas une commande valide, recommencez.");
    icon = 2;
  }

  motor.setDistanceMinSuivi(maxNear);
  motor.setDistanceMaxSuivi(maxFar);
  return icon;
}

void displayIcon(unsigned long ct, int whichOne) {                                              //displayIcon
  unsigned static long timer;
  static bool isTimerStarted = false;

  if (!isTimerStarted && isMatrixOn) {
    u8g2.clearBuffer();
    switch (whichOne) {
      case 1:
        u8g2.drawLine(6, 7, 1, 2);
        u8g2.drawLine(2, 1, 3, 0);
        break;
      case 2:
        u8g2.drawLine(0, 0, 7, 7);
        u8g2.drawLine(7, 0, 0, 7);
        break;
      case 3:
        u8g2.drawCircle(4, 3, 3);
        u8g2.drawLine(7, 0, 0, 7);
        break;
    }
    u8g2.sendBuffer(); 
    timer = ct + 3000;
    isTimerStarted = true;
  }
  if (ct >= timer) {
    isMatrixOn = false;
    isTimerStarted = false;
    u8g2.clearBuffer();
    u8g2.sendBuffer();
  }
}

//REFRESH, DISTANCE AND TEMPERATURE+HUMIDITY//////////////////////////////////////////////////////
void refreshScreen() {                                                                          //refreshScreen
  unsigned static long timer;
  static bool isTimerStarted = false;

  if (!isTimerStarted) {
    timer = currentTime;
    timer += 100;
    isTimerStarted = true;
  }
  if (currentTime >= timer) {
    screen();
    isTimerStarted = false;
  }
}

void getDistance(unsigned long ct) {                                                            //getDistance
  unsigned static long timer;
  static bool isTimerDone = false;
  static int lastDistance = zero;
  static int tempDistance;

  if (!isTimerDone) {
    timer = ct + 50;
    isTimerDone = true;
  }

  if (ct >= timer) {
    tempDistance = hc.dist();
    if (tempDistance != zero) {
      distance = tempDistance;
      lastDistance = distance;
    }
    else {
      distance = lastDistance;
    }
    isTimerDone = false;
  }
}

void dhtTask(unsigned long now) {
  static unsigned long lastTime = 0;
  const int rate = 2000;
  
  if (now - lastTime >= rate) { 
    lastTime = now;
    
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    float f = dht.readTemperature(true);

    if (isnan(humidity) || isnan(temperature) || isnan(f)) {
      Serial.println(F("Echec de lecture du DHT!"));
      return;
    }
  }
}

//SCREEN DISPLAY CALLED BY REFRESH_SCREEN/////////////////////////////////////////////////////////
void screen() {                                                                                 //Screen
  lcd.clear();

  lcd.print("Dist: ");
  lcd.print((int)distance);
  lcd.print(" cm");

  lcd.setCursor(0, 2);
  if (motor.getEtatTexte() == "INACTIF") {
    lcd.print("INACTIF");
  }
  else {
    lcd.print("Obj: ");
    if (!(distance < maxNear) && !(distance > maxFar)) {
      lcd.print((int)motor.getAngle());
      lcd.print(" deg");
    }
    else if (distance < maxNear) {
      lcd.print("Trop pret");
    }
    else {
      lcd.print("Trop loin");
    }
  }
}