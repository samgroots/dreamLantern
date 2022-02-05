

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <ArduinoJson.h>

#define COLOR_ORDER GRB
#define LED_PIN 12

#define decayProb_1 50
#define decayProb_hue 50
#define decayConst_1 980
#define decayConst_hue 800
#define baseHue_1 50
#define hueAmplitude_1 10
#define sineTau 3000
#define flickerHue 10
#define hueMaxVar 500
#define maxVal 100
#define outerVal 80

int co2 = 400; // starting value for CO2 levels

//MQTT topics
const char* TOPIC_0 = "lantern/lant0"; // change to airquality base topic 
//const char* TOPIC_1 = "lantern/lant1";
//const char* TOPIC_2 = "lantern/lant2";
//const char* TOPIC_3 = "lantern/lant3";
//const char* TOPIC_4 = "lantern/lant4";
const char* TOPIC_AQ = "airquality/#"; //subscribe to AQ topic on local raspberry pi (change hardware number to your own)

#define LED_A 24
#define LED_B 48
#define LED_C 64
#define LED_D 72
#define LED_COUNT 72

uint8_t pixelHue[LED_COUNT];
uint8_t pixelVal[LED_COUNT];
float pixelFraction[LED_COUNT];
int hueModifier[LED_COUNT];
//const float blendSpeed = 2; // variable used for smooth blending - comment out if not using

CRGBArray<LED_COUNT> leds;

//starting parameters
uint16_t PARAMETERS[]= {baseHue_1, hueAmplitude_1, sineTau, flickerHue, hueMaxVar};
#define numTopics 5
uint16_t PARAMETERS_old[numTopics];

//////////////////////////////////
////////// WiFi details go in here
const char* ssid = "TOES";
const char* password = "saxophone";
const char* mqtt_server = "airquality.local";
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50) 
char msg[MSG_BUFFER_SIZE];
int value = 0;

////////////////////////////////////
/// GETTING ONTO THE WIFI

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

///////////////////////////////////////////////////////////////////////////
//// MQTT callback - this function runs whenever a subscribed topic changes
///////////////////////////////////////////////////////////////////////////
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

//  for (int i = 0; i < length; i++) {
//    Serial.print((char)payload[i]);
//  }
//  Serial.println();

//convert payload to a number, if it is one...
//
//    payload[length] = '\0'; // this line terminates the payload with a null code
//    String s = String((char*)payload);
//    int payloadValue = s.toInt();
  
    // Compare 'topic' i.e. recently updated value 
    // with the name of each subscribed topic
    // and change the respective Parameter:
  
      if (strcmp(topic, TOPIC_0) == 0){
        payload[length] = '\0'; // this line terminates the payload with a null code
        String s = String((char*)payload);
        int payloadValue = s.toInt();
        PARAMETERS[0] = payloadValue;
        Serial.print("Parameter ");
        Serial.print(TOPIC_0);
        Serial.print(" changed to ");
        Serial.println(PARAMETERS[0]);
      }
//      else if(strcmp(topic, TOPIC_1) == 0){
//        PARAMETERS[1] = payloadValue;
//        Serial.print("Parameter ");
//        Serial.print(TOPIC_1);
//        Serial.print(" changed to ");
//        Serial.println(PARAMETERS[1]);      
//      }
//      else if(strcmp(topic, TOPIC_2) == 0){
//        PARAMETERS[2] = payloadValue;
//        Serial.print("Parameter ");
//        Serial.print(TOPIC_2);
//        Serial.print(" changed to ");
//        Serial.println(PARAMETERS[2]);      
//      }
//      else if(strcmp(topic, TOPIC_3) == 0){
//        PARAMETERS[3] = payloadValue;
//        Serial.print("Parameter ");
//        Serial.print(TOPIC_3);
//        Serial.print(" changed to ");
//        Serial.println(PARAMETERS[3]);      
//      }
//      else if(strcmp(topic, TOPIC_4) == 0){
//        PARAMETERS[4] = payloadValue;
//        Serial.print("Parameter ");
//        Serial.print(TOPIC_4);
//        Serial.print(" changed to ");
//        Serial.println(PARAMETERS[4]);      
//      }
      else {
        Serial.print("payload is a JSON. Length = ");
        Serial.println(length);
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload, length);

        // Test if parsing succeeds.
        if (error) { 
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }
        co2 = doc["co2"]["co2"];
        Serial.print("CO2 level updated: ");
        Serial.println(co2);
      }
  }

/////////////////////////
// reconnect to the WiFi

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish("test", "hello world");
      // ... and resubscribe

      //subscribe to all topics
        client.subscribe(TOPIC_0);
        //client.subscribe(TOPIC_1);
        //client.subscribe(TOPIC_2);
        //client.subscribe(TOPIC_3);
        //client.subscribe(TOPIC_4);
        client.subscribe(TOPIC_AQ);

        Serial.print("Subscribed to ");
        Serial.print(TOPIC_0);
//        Serial.print(", ");
//        Serial.print(TOPIC_1);
//        Serial.print(", ");
//        Serial.print(TOPIC_2);
//        Serial.print(", ");
//        Serial.print(TOPIC_3);
//        Serial.print(", ");
//        Serial.print(TOPIC_4);
        Serial.print(", ");
        Serial.println(TOPIC_AQ);
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


////////////////////////////////////////////////////////////
//// Do some maths to fill a global float array 'pixelFraction' 
//// for each separate ring / subset of the LEDs
//// - this enables a lot of the animations

void fillPixelFractions(uint16_t startLED, uint16_t stopLED){
  int numLedsInString = stopLED - startLED;
  float number = float(numLedsInString);
  for(int i = startLED; i < stopLED; i++)
  {
     pixelFraction[i] = (i+1-startLED)/number;
  }
}


///////////////////////////////////////////////////////////
/// Sine wave pulse

void sineWaveHue(uint8_t firstLED, uint8_t lastLED, uint16_t centralVal, uint16_t amplitude, uint16_t timePeriod){
  uint16_t timeNow = (millis() % timePeriod);
  float timeNowFraction = float(timeNow) / float(timePeriod);
  for(int i = firstLED; i < lastLED; i++){
    pixelHue[i] = int(amplitude * sin(TWO_PI * (timeNowFraction + pixelFraction[i])) + centralVal);
    //pixelVal[i] = 255;
  }
}

///////////////////////////////////////////////////////////
/// Random noise flicker!

void randomNoise(uint16_t firstLED, uint16_t lastLED, uint16_t strength, bool gradient){
    int pixelMod[LED_COUNT];
    for (int i = firstLED; i< lastLED; i++){
      pixelMod[i] = random(strength);
      int halfStr = int(strength/2);
      if(gradient == false){
        pixelHue[i] = pixelHue[i] - halfStr + int(pixelMod[i]);
      }
      else{
        pixelHue[i] = pixelHue[i] - halfStr + int(pixelMod[i]*pixelFraction[i]);
      }
    }
}


///////////////////////////////////////////////////////////
/// Exponential decay brightness flicker!

void flickerDecayVal(int firstLED, int lastLED, uint16_t decayConst, uint16_t decayProb, uint16_t brightness){
  for(int i = firstLED; i < lastLED; i++){
    int decayValRandom = random(1000);
    if(decayValRandom < decayProb){
      pixelVal[i] = brightness;
    }
    else {
      pixelVal[i] = int(float(pixelVal[i])*float(decayConst)/1000);
    }
  }  
}

//////////////////////////////////////////////////////////
///// Exponential decay colour modifier (flicker) - generates a 'hue modifier' value for the relevant LED
///// NOTE creates a value for hueModifier[i] that needs to be divided by 1000 before adding to hueValue[i]

void flickerDecayHue(int firstLED, int lastLED, uint16_t decayConst, uint16_t decayProb, uint16_t variance){
  for(int i = firstLED; i < lastLED; i++){
    int decayRandom = random(1000); // with a probability of 'decayProb / 1000':
    if(decayRandom < decayProb){
      hueModifier[i] = random(variance) - int(float(variance)/2); //add (or subtract) a random number
    }
    else {
      hueModifier[i] = int(hueModifier[i]*float(decayConst)/1000); //and otherwise reduce the modifier
    }
  }
}

void stringHue(byte firstLED, byte lastLED, uint16_t huenum){
  for(int i = firstLED; i < lastLED; i++){
     pixelHue[i] = huenum;
  }
}

void stringVal(byte firstLED, byte lastLED, uint16_t valnum){
  for(int i = firstLED; i < lastLED; i++){
     pixelVal[i] = valnum;
  }
}


void setup() { 
  
  Serial.begin(115200);
  
  // initialise the WiFi and the MQTT client
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  // initialise the LEDs
  FastLED.addLeds<WS2812,LED_PIN, COLOR_ORDER>(leds, LED_COUNT); 
  delay(50);

  //Set starting parameters
  for(int i = 0; i < numTopics; i++){
    PARAMETERS_old[i] = PARAMETERS[i];
  }
    
  //LED string is off to begin with
  stringHue(0, LED_COUNT, 0);
  stringVal(0, LED_COUNT, 0);

  // pixelFractions is a float array - this function fills it with values
  // that vary from 0 at the beginning of the LED string to 1 at the end.
  // This allows LED animations to be smoothly graded over the string.
  fillPixelFractions(0, LED_A);
  fillPixelFractions(LED_A, LED_B);
  fillPixelFractions(LED_B, LED_C);
  fillPixelFractions(LED_C, LED_COUNT);
}

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
///// main loop
void loop(){ 

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  for (int i=0; i < numTopics; i++){
    PARAMETERS_old[i] = PARAMETERS[i]; //comment out if using blend function
  }

  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////
  ///////ANIMATION FUNCTIONS//////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////
  //Animation reference
  //sineWaveHue(firstLED, lastLED, centralVal, amplitude, timePeriod)
  //flickerDecayHue(firstLED, lastLED, decayConst, decayProb, variance)
  //flickerDecayVal(firstLED, lastLED, decayConst, decayProb, maxVal)
  ////////////////////////////////////////////////////////////////////////////  
  /////// FIRST STRING: pointing down - outside AQ
  
  sineWaveHue(0, LED_A, PARAMETERS_old[0], PARAMETERS_old[1], PARAMETERS_old[2]); 
  stringVal(0, LED_A, maxVal);
  flickerDecayHue(0, LED_A, decayConst_hue, decayProb_hue, PARAMETERS_old[4]);
  //Serial.println(hueModifier[0]);  // debug line


  ////////////////////////////////////////////////////////////////////////////
  /////// SECOND STRING: pointing up - indoor AQ - main sinewave pulse 

  sineWaveHue(LED_A, LED_B, PARAMETERS_old[0], PARAMETERS_old[1], PARAMETERS_old[2]);
  stringVal(LED_A, LED_B, maxVal);

  /////// THIRD STRING: pointing up - indoor AQ - second sinewave pulse? flicker decay??
  
  flickerDecayVal(LED_B, LED_C, decayConst_1, decayProb_1, maxVal);
  stringHue(LED_B, LED_C, PARAMETERS_old[3]);
  randomNoise(LED_B, LED_C, 10, false);

  /////// FOURTH STRING: pointing up - indoor AQ - flicker

  flickerDecayVal(LED_C, LED_D, decayConst_1, decayProb_1, maxVal);
  stringHue(LED_C, LED_D, PARAMETERS_old[3]);

  /////////////////////////////////////////////////////////////////////////////
  // load the LED values into the FastLED function, and send it all to the LEDs
  
  for(int i = 0; i < LED_COUNT; i++) {
    if (hueModifier[i] == 0){   
    leds[i] = CHSV(pixelHue[i],255,pixelVal[i]); //nb. add hueModifier value
    }
    else {
    int modifier = int(hueModifier[i] / 100);
    leds[i] = CHSV(pixelHue[i] + modifier,255,pixelVal[i]); //nb. add hueModifier value
    }
  }
  FastLED.show();
  delay(10);
}


///////////////////////////////////////////////////////////
/// random WALK flicker!
//
//void randomWalkHue(uint16_t firstLED, uint16_t lastLED, uint16_t strength, bool graded){
//    int pixelMod[LED_COUNT];
//    for (int i = firstLED; i< lastLED; i++){
//      pixelMod[i] = random(strength);
//      int halfStr = int(strength/2);
//      if(graded == false){
//        pixelHue[i] = pixelHue[i] - halfStr + int(pixelMod[i]);
//      }
//      else{
//        pixelHue[i] = pixelHue[i] - halfStr + int(pixelMod[i]*pixelFraction[i]);
//      }
//    }
//}



//SOME CODE TO VARY THE PARAMETERS SMOOTHLY - work on this later.
//
//    if (PARAMETERS[i] != PARAMETERS_old[i]){
//      if (PARAMETERS[i] > PARAMETERS_old[i]){
//          if(100*(PARAMETERS[i] / PARAMETERS_old[i]-1) < 1){
//            PARAMETERS_old[i] = PARAMETERS[i]; // if there's less than 1% difference, make them identical.
//          }
//          else{
//            PARAMETERS_old[i] = uint16_t(PARAMETERS[i] - (blendSpeed*((PARAMETERS[i] / PARAMETERS_old[i])-1))); 
//          }
//      }
//      else{ //here, new is a smaller value than old...
//        if(100*(PARAMETERS_old[i] / PARAMETERS[i]-1) < 1){
//            PARAMETERS_old[i] = PARAMETERS[i]; // if there's less than 1% difference, make them identical.
//          }
//          else{
//            PARAMETERS_old[i] = uint16_t(PARAMETERS[i] - (blendSpeed*((PARAMETERS_old[i] / PARAMETERS[i])-1))); 
//          }
//      }
//  }
