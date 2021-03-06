// This #include statement was automatically added by the Particle IDE.
#include <MQTT.h>

// This #include statement was automatically added by the Particle IDE.
#include <MFRC522.h>

#include "Comandparser.h"

//#include "RFIDfunctions.h"
//extern void setupread();
extern int readSerialOlimex();
extern bool readRFIDCard(int Charger);
extern float Current[2][3];
extern float Power[2][3];
extern float PhaseVoltage[2][3];
extern float LineVoltage[2][3];
extern float Energy[2];
extern float Frequency[2];
extern float CurrentList[20];
extern int numberOfZeroReadings[2];
void reconnect(void);
void callback(char* topic, byte* payload, unsigned int length);
void charToString(const char in[], String &out);

String UIDtagCharger1="No ID";
String UIDtagCharger2="No ID";

#define CHARGEROFFSET 0 //use 0 for socket 1 and 2, use 2 for socket 3 and 4, etc.
#define DEBUGPORT Serial
#define SIZEOFUSERLIST 2
#define NUMBEROFMETERS 5
//SYSTEM_MODE(SEMI_AUTOMATIC);

#define SS_PIN_CHARGER1 A1
#define SS_PIN_CHARGER2 A2
#define RST_PIN A0
//Additional UART port not possible on D1,D2??
#define EXTRA_DIGITAL_BREAKOUT_1 D0 // Not used yet
#define EXTRA_DIGITAL_BREAKOUT_2 D1 // Not used yet
#define EXTRA_DIGITAL_BREAKOUT_3 D3 // Not used yet
#define WAKEUP_OLIMEX D2 //needed for in system programming (ISP)
#define RESET_OLIMEX D4 //needed for in system programming (ISP)
#define PILOT_FEEDBACK_CAR_1 A6 // To read the feedback signal from EV shield in paralel with the Olimex
#define PILOT_FEEDBACK_CAR_2 A7 // To read the feedback signal from EV shield in paralel with the Olimex
#define AUTHENTICATION_CAR1 D5 //Enable car 1 --> to Olimex A2
#define AUTHENTICATION_CAR2 D6 //Enable car 2 --> to Olimex A3
#define EXTRA D7 // No function yet --> to Olimex A4
//#define SERVER "80.113.19.23:8080"
//"hannl-lmrt-particle-api.azurewebsites.net"

STARTUP(WiFi.selectAntenna(ANT_EXTERNAL)); // selects the u.FL antenna
//SYSTEM_THREAD(ENABLED);

//MQTT setting
//byte server[] = {192,168,43,249};
//MQTT client(server, 1883, callback);
MQTT client("broker.hivemq.com", 1883, MQTT_DEFAULT_KEEPALIVE, callback, 512);
//char ID[] = "11111";

String test = "0";

int counter=11;
MFRC522 mfrc522_Charger1(SS_PIN_CHARGER1, RST_PIN);   // Create MFRC522 instance.
MFRC522 mfrc522_Charger2(SS_PIN_CHARGER2, RST_PIN);   // Create MFRC522 instance.
unsigned long LatestStartTime[2]={0,0};
bool handledCharger=0;
String ShareVar;
//String Current_Str="0";
bool TESTCASE=false;

struct EMeter {
    float PhaseVoltage[3];
    float PhaseCurrent[3];
    float PhasePower[3];
    float Frequency;
    unsigned long Time;
};

struct EVUser {
    int Id;
    String CarBrand;
    String CarType;
    int CarYear;
    String Owner;
    float BatteryCapacity;
    String UIDtag;
    int PendingCharger;
    unsigned long StartTime;
};

EMeter EMeterData[NUMBEROFMETERS];
String EVListStr="";
String currentStr="";
unsigned int nextTime[2] = {30000,30000};    // Next time to contact the server

void charToString(const char in[], String &out) {
    byte index = 0;
    const char *pointer = in;
    out = "";

    while (*pointer++) {
      out.concat(in[index++]);
      }
}

int resetOlimex(String input) {
    digitalWrite(RESET_OLIMEX, LOW);
    delay(500);
    digitalWrite(RESET_OLIMEX, HIGH);
    return 1;
}

int WifiSignal(String input) {
    return WiFi.RSSI();
}

int resetParticl(String input) {
    System.reset();
}

int progModeOlmx(String input) {
    digitalWrite(WAKEUP_OLIMEX, HIGH);
    delay(500);
    resetOlimex("");
    delay(500);
    digitalWrite(WAKEUP_OLIMEX, LOW);
    return 1;
}

void blinkRFIDled(int charger,int action) {
    //action=1  succesfull start new charge (charger is free and last stoped session > 20 sec ago)
    //action=2  charger is free, but you allready swiped the card in the last 20 sec (second swipe within 20sec)
    //action=3  charger is occupied by annother user
    //action=4  succesfull stop this charge session
    //action=5  you just started a charge on this charger, but have another consecutive RFID read/swipe within 20 seconds
    //action=6  you are allready charging at another charger
    //action=7  succesfull RFID read, but you are not in the userlist
    
    digitalWrite(D7,HIGH);
    delay(100);
    digitalWrite(D7,LOW);
    return;
}

int activeCharger() {
    int number = 0;
    for (int i=0; i<3; i++) {
        if (Current[0][i] != 0.0) {
            number += 1;
            break;
        }
    }
    
    for (int i=0; i<3; i++) {
        if (Current[1][i] != 0.0) {
            number += 2;
            break;
        }
    }
    
    return number;
}

int switchTest(String valueString) {
    if (valueString == "true") {
        TESTCASE = true;
        return 1;
    }
    if (valueString == "false") {
        TESTCASE = false;
        return 0;
    }
}

int maxCurrentC1(String setPointStr) {
    unsigned int setPoint = setPointStr.toInt();
    byte olimexMessage[4] = {0xFE,1,setPoint,0xFF};
    if (!TESTCASE) {
        Serial1.write(olimexMessage,4);
        DEBUGPORT.println("maxCurrentC1>\tNew setpoint set at "+String(setPoint)+" Amps.");
        return 0;
    }
    return 1;
}

int maxCurrentC2(String setPointStr) {
    unsigned int setPoint = setPointStr.toInt();
    byte olimexMessage[4] = {0xFE,2,setPoint,0xFF};
    if (!TESTCASE) {
        Serial1.write(olimexMessage,4);
        DEBUGPORT.println("maxCurrentC2>\tNew setpoint set at "+String(setPoint)+" Amps.");
        return 0;
    }
    return 1;
}

int maxCurrentC1_test(int setPoint) {
    byte olimexMessage[4] = {0xFE,1,setPoint,0xFF};
    if (TESTCASE) {
        Serial1.write(olimexMessage,4);
        DEBUGPORT.println("maxCurrentC1>\tNew setpoint set at "+String(setPoint)+" Amps.");
        return 0;
    }
    return 1;
}

int maxCurrentC2_test(int setPoint) {
    byte olimexMessage[4] = {0xFE,2,setPoint,0xFF};
    if (TESTCASE) {
        Serial1.write(olimexMessage,4);
        DEBUGPORT.println("maxCurrentC1>\tNew setpoint set at "+String(setPoint)+" Amps.");
        return 0;
    }
    return 1;
}
/*
int AuthPinsHigh(String input)
{
    digitalWrite(AUTHENTICATION_CAR1, HIGH); //digitalWrite(D1,LOW);
    digitalWrite(AUTHENTICATION_CAR2, HIGH);//digitalWrite(D2,LOW);
    //digitalWrite(D7,HIGH);
    delay(10000);
    return 1;
}

int AuthPinsLow(String input)
{
    digitalWrite(AUTHENTICATION_CAR1, LOW); //digitalWrite(D1,LOW);
    digitalWrite(AUTHENTICATION_CAR2, LOW);//digitalWrite(D2,LOW);
    //digitalWrite(D7,LOW);
    delay(10000);
    return 1;
}*/

String getUserIdAtSocket(int socket) {
    if (socket == 1+CHARGEROFFSET)
        return UIDtagCharger1;
    if (socket == 2+CHARGEROFFSET)
        return UIDtagCharger2;
    return "00";
}

void getMeasure_callback(byte* payload, unsigned int length) {
    String data;
    int from = 0;
    int to = 0;
    
    char p[length + 1];
    memcpy(p, payload, length);
    
    p[length] = NULL;
    charToString(p, data);
    for(int i=0; i<NUMBEROFMETERS; i++) {
        //Read Phase Voltage
        for(int j=0; j<3; j++) {
            while (data[to]!='%') {
                to++;
            }
            EMeterData[i].PhaseVoltage[j] = (data.substring(from, to)).toFloat();
            to++;
            from = to;
        }
        //Read Phase Current
        for(int j=0; j<3; j++) {
            while (data[to]!='%') {
                to++;
            }
            EMeterData[i].PhaseCurrent[j] = (data.substring(from, to)).toFloat();
            to++;
            from = to;
        }
        //Read Phase Power
        for(int j=0; j<3; j++) {
            while (data[to]!='%') {
                to++;
            }
            EMeterData[i].PhasePower[j] = (data.substring(from, to)).toFloat();
            to++;
            from = to;
        }
        //Read Frequency
        while (data[to]!='%') {
            to++;
        }
        EMeterData[i].Frequency = (data.substring(from, to)).toFloat();
        to++;
        from = to;
        //Read StartTime
        while (data[to]!='%') {
            to++;
        }
        EMeterData[i].Time = atol((data.substring(from, to)).c_str());
        to++;
        from = to;
    }
    time_t time = Time.now();
    //DEBUGPORT.println(time);
    DEBUGPORT.print("MQTT>\tReceive energy meter data from broker at: ");
    DEBUGPORT.println(Time.format(time, TIME_FORMAT_DEFAULT));
    
    //Current_Str = String((int)(EMeterData[2].PhaseCurrent[0]));
    
    //Send current to OLIMEX
    /*
    if (AUTHENTICATION_CAR1) {
        if (AUTHENTICATION_CAR2) {
            maxCurrentC1_test((int)(EMeterData[2].PhaseCurrent[0]/2)); //Emeter3, I1
            maxCurrentC2_test((int)(EMeterData[2].PhaseCurrent[0]/2)); //Emeter3, I1
        }
        else
            maxCurrentC1_test((int)(EMeterData[2].PhaseCurrent[0])); //Emeter3, I1
    }
    else {
        if (AUTHENTICATION_CAR2) {
            maxCurrentC2_test((int)(EMeterData[2].PhaseCurrent[0])); //Emeter3, I1
        }
    }
    */
    if (activeCharger()==1) {
        maxCurrentC1_test((int)(EMeterData[2].PhaseCurrent[0]+EMeterData[2].PhaseCurrent[1]+EMeterData[2].PhaseCurrent[2])); //Emeter3, I1
    }
    else if (activeCharger()==2) {
        maxCurrentC2_test((int)(EMeterData[2].PhaseCurrent[0]+EMeterData[2].PhaseCurrent[1]+EMeterData[2].PhaseCurrent[2])); //Emeter3, I1
    }
    else {
        maxCurrentC1_test((int)((EMeterData[2].PhaseCurrent[0]+EMeterData[2].PhaseCurrent[1]+EMeterData[2].PhaseCurrent[2])/2)); //Emeter3, I1
        maxCurrentC2_test((int)((EMeterData[2].PhaseCurrent[0]+EMeterData[2].PhaseCurrent[1]+EMeterData[2].PhaseCurrent[2])/2)); //Emeter3, I1
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    test = "99";
	if (strcmp(topic, "HANevse/EnergyMeter")==0) {
	    test = "1";
	    getMeasure_callback(payload, length);
	}
	time_t time = Time.now();
    //DEBUGPORT.println(time);
    DEBUGPORT.print("MQTT>\tCallback function is called at: ");
    DEBUGPORT.println(Time.format(time, TIME_FORMAT_DEFAULT));
}

void add_Measurement(float phaseVoltageL1, float phaseVoltageL2, float phaseVoltageL3, float currentL1, float currentL2, float currentL3,  float Power, float Energy, float Frequency, unsigned long Timestamp, int socketId=0, String userId="00") {
	String socketStr = "";
	String userStr = "";
	if(socketId != 0) {
		socketStr = "%" + String(socketId);
	}
	if(userId != "00") {
		userStr = "%" + userId;
	}
	String Body = String(phaseVoltageL1, 2) + "%" + String(phaseVoltageL2, 2) + "%" + String(phaseVoltageL3, 2) + "%" + String(currentL1, 2) + "%" + String(currentL2, 2) + "%" + String(currentL3, 2) + "%" + String(Power, 2) + "%" + String(Energy, 2) + "%" + String(Frequency, 2) + "%" + String(Timestamp) + socketStr + userStr + "%";
	
	for(int i=0; i<3; i++) {
		if(client.publish("HANevse/photonMeasure", Body)) {
			break;
		}
	}
}

int initRFID(String input) {
    //additional config for debugging RFID readers
    pinMode(SS_PIN_CHARGER1, OUTPUT);
	digitalWrite(SS_PIN_CHARGER1, HIGH);
	pinMode(SS_PIN_CHARGER2, OUTPUT);
	digitalWrite(SS_PIN_CHARGER2, HIGH);
  
    SPI.begin(D0);      // Initiate  SPI bus
    //Particle.process();
    delay(50);
    mfrc522_Charger1.PCD_Init();   // Initiate MFRC522
    delay(500);
    mfrc522_Charger2.PCD_Init();   // Initiate MFRC522
    ////mfrc522_Charger1.PCD_SetAntennaGain(mfrc522.RxGain_max);
    mfrc522_Charger1.PCD_SetAntennaGain(mfrc522_Charger1.RxGain_max);
    mfrc522_Charger2.PCD_SetAntennaGain(mfrc522_Charger2.RxGain_max);
    
    DEBUGPORT.println("Approximate your card to the reader...");
    DEBUGPORT.println();    
    return 1;
}

bool readRFIDCard(int Charger) {
   // DEBUGPORT.print("readCard>\t");
    bool Authorized=true;
    if(Charger==1+CHARGEROFFSET)
    {
      // Look for new cards
        if ( ! mfrc522_Charger1.PICC_IsNewCardPresent()) 
        {
            return false;
        }
        // Select one of the cards
        if ( ! mfrc522_Charger1.PICC_ReadCardSerial()) 
        {
            return false;
        }
  
        //Show UID on serial monitor
        DEBUGPORT.print("readCard>\tUID tag on charger1:");
        String content= "";
        byte letter;
        for (byte i = 0; i < mfrc522_Charger1.uid.size; i++) 
        {
            DEBUGPORT.print(mfrc522_Charger1.uid.uidByte[i] < 0x10 ? " 0" : " ");
            DEBUGPORT.print(mfrc522_Charger1.uid.uidByte[i], HEX);
            content.concat(String(mfrc522_Charger1.uid.uidByte[i] < 0x10 ? " 0" : " "));
            content.concat(String(mfrc522_Charger1.uid.uidByte[i], HEX));
        }
        //Authorized=testUser(content,Charger);
        UIDtagCharger1=content.substring(1);
    }
    if(Charger==2+CHARGEROFFSET)
    {
    
        // Look for new cards
        if ( ! mfrc522_Charger2.PICC_IsNewCardPresent()) 
        {
            return false;
        }
        // Select one of the cards
        if ( ! mfrc522_Charger2.PICC_ReadCardSerial()) 
        {
            return false;
        }
        //DEBUGPORT.println("Read something on charger2");
        //Show UID on serial monitor
        DEBUGPORT.print("readCard>\tUID tag on charger2:");
        String content= "";
        byte letter;
        for (byte i = 0; i < mfrc522_Charger2.uid.size; i++) 
        {
            DEBUGPORT.print(mfrc522_Charger2.uid.uidByte[i] < 0x10 ? " 0" : " ");
            DEBUGPORT.print(mfrc522_Charger2.uid.uidByte[i], HEX);
            content.concat(String(mfrc522_Charger2.uid.uidByte[i] < 0x10 ? " 0" : " "));
            content.concat(String(mfrc522_Charger2.uid.uidByte[i], HEX));
        }
        //Authorized=testUser(content,Charger);
        UIDtagCharger2=content.substring(1);
    }
    DEBUGPORT.println("");
    return Authorized;
}

void reconnect(void) {
    while (!client.isConnected()) {
        DEBUGPORT.print("MQTT>\tConnecting to MQTT broker...");
        if (client.connect("EV-Photon1")) {
            DEBUGPORT.println("MQTT>\tConnected");
            //client.subscribe("HANevse/#", client.QOS2);
            client.subscribe("HANevse/EnergyMeter", client.QOS2);
        }
        else {
            DEBUGPORT.println("MQTT>\tConnection failed");
            DEBUGPORT.println("MQTT>\tRetrying...");
            delay(5000);
        }
    }
}

void setup() {
    DEBUGPORT.begin(115200); 
    Serial1.begin(9600);
    //DEBUGPORT.println(Voltage,5);
    //DEBUGPORT.println(String(Voltage,5));
    
    waitUntil(Particle.connected);
    
    pinMode(AUTHENTICATION_CAR1, OUTPUT); //pinMode(D1, OUTPUT); //Charger1_Authorized
    pinMode(AUTHENTICATION_CAR2, OUTPUT); //pinMode(D2, OUTPUT); //Charger2_Authorized
    pinMode(PILOT_FEEDBACK_CAR_1,INPUT);
    pinMode(PILOT_FEEDBACK_CAR_2,INPUT);
    pinMode(WAKEUP_OLIMEX, OUTPUT);
    pinMode(RESET_OLIMEX, OUTPUT);
    pinMode(D7, OUTPUT);
    
    digitalWrite(AUTHENTICATION_CAR1, LOW); //digitalWrite(D1,LOW);
    digitalWrite(AUTHENTICATION_CAR2, LOW);//digitalWrite(D2,LOW);
    digitalWrite(WAKEUP_OLIMEX, LOW);
    digitalWrite(RESET_OLIMEX, HIGH);
    digitalWrite(D7, LOW);
    
    initRFID("");
    
    //Particle.process();
    //resetOlimex("");
    //Particle.process();

	Particle.function("switchTest",switchTest);
    Particle.function("maxCurrentC1",maxCurrentC1);
    Particle.function("maxCurrentC2",maxCurrentC2);
    Particle.function("resetOlimex",resetOlimex);
    Particle.function("progModeOlmx",progModeOlmx);
    Particle.function("resetParticl",resetParticl);
    //Particle.function("AuthPinsHigh",AuthPinsHigh);
    //Particle.function("AuthPinsLow",AuthPinsLow);
    Particle.function("WifiSignal",WifiSignal);
    Particle.function("initRFID",initRFID);
    Particle.variable("currentStr",currentStr);
    Particle.variable("ShareVar",ShareVar);
    //Particle.variable("Current", Current_Str);
    Particle.variable("Topic", test);
    Particle.process();
	
	RGB.control(true);
    Time.zone(1); //Dutch time zone
}

void loop() {
    //Check the connection to the MQTT broker
    if (client.isConnected()) {
        client.loop();
    }
    else reconnect();
    
    Particle.process();
    //currentStr = String(Current[0][0],1)+" "+String( Current[0][1],1)+" "+String(Current[0][2],1)+" "+String(Current[1][0],1)+" "+String( Current[1][1],1)+" "+String(Current[1][2],1)+" "+String(Frequency[0],2);
    currentStr = String(Current[0][0],1)+" "+String( PhaseVoltage[0][1],1)+" "+String(LineVoltage[0][2],1)+" "+String(Power[1][0],1)+" "+String( Energy[1],1)+" "+String(Current[1][2],1)+" "+String(Frequency[0],2);
    //currentStr=String(Current[1][2],1)+" "+currentStr.substring(0, max(200, currentStr.length()))
    //currentStr = String(CurrentList[0],1)+" "+String(CurrentList[1],1)+" "+String(CurrentList[2],1)+" "+String(CurrentList[3],1)+" "+String(CurrentList[4],1)+" "+String(CurrentList[5],1)+" "+String(CurrentList[6],1)+" "+String(CurrentList[7],1)+" "+String(CurrentList[8],1)+" "+String(CurrentList[9],1)+" "+String(CurrentList[10],1)+" "+String(CurrentList[11],1)+" "+String(CurrentList[12],1)+" "+String(CurrentList[13],1)+" "+String(CurrentList[14],1)+" "+String(CurrentList[15],1)+" "+String(CurrentList[16],1)+" "+String(CurrentList[17],1)+" "+String(CurrentList[18],1)+" "+String(CurrentList[19],1);
    if (Particle.connected() == false) {
        Particle.connect();
    }
    //int Charger =1; 
    int Charger = readSerialOlimex()+CHARGEROFFSET;
    Particle.process();
    if(counter>10){
		counter = 0;
		DEBUGPORT.println("LatestStartTime>\t"+String(LatestStartTime[0])+", "+String(LatestStartTime[1]));
		DEBUGPORT.println(String(Current[1][0]+ Current[1][1]+ Current[1][2]));
    }
    counter++;
		
    // store new measurement value if it is received correctly from energymeter (via the Olimex).
    if(millis()>nextTime[handledCharger] && (Charger==1+CHARGEROFFSET || Charger==2+CHARGEROFFSET)) 
    {
        Particle.process();
        //getUserIdAtSocket(Charger)
        int tempCharger = Charger;
        Charger = handledCharger + 1;
        if(((activeCharger()==Charger) || (activeCharger() == 3)) && (getUserIdAtSocket(Charger)!="00"))
        {
            //getUserIdAtSocket(Charger+CHARGEROFFSET);
            add_Measurement(PhaseVoltage[Charger-1][0], PhaseVoltage[Charger-1][1], PhaseVoltage[Charger-1][2], Current[Charger-1][0], Current[Charger-1][1], Current[Charger-1][2], Power[Charger-1][0]+Power[Charger-1][1]+Power[Charger-1][2], Energy[Charger-1], Frequency[Charger-1], Time.now(), Charger+CHARGEROFFSET, getUserIdAtSocket(Charger+CHARGEROFFSET));
        }
        Charger = tempCharger;
        nextTime[handledCharger] = millis() + 30000; //every 30 sec
    }
    
    //run loop very often to check new RFID cards
    Particle.process();
    bool Authorized_Charger1=readRFIDCard(1+CHARGEROFFSET);
    bool Authorized_Charger2=readRFIDCard(2+CHARGEROFFSET);
    
    //DEBUGPORT.println(Current[0][0]+ Current[0][1]+ Current[0][2],4);
    //DEBUGPORT.println(String(LatestStartTime[0]+60));
    //DEBUGPORT.println(String(Time.now()));
    //DEBUGPORT.println((LatestStartTime[0] + 60 < Time.now()),DEC);
    //if ((LatestStartTime[0] + 60 < Time.now()) && (Current[0][0]+ Current[0][1]+ Current[0][2]) < 1)
    //if (((numberOfZeroReadings[0]>10 && (LatestStartTime[0] + 60 < Time.now()))|| ((Time.now()<LatestStartTime[0] + 70)&&(LatestStartTime[0] + 60 < Time.now()))) && (Current[0][0]+ Current[0][1]+ Current[0][2]) < 1)
    if( ((numberOfZeroReadings[0]>10)||(LatestStartTime[0] + 70 > Time.now()) )&& (LatestStartTime[0] + 60 < Time.now()) && (Current[0][0]+ Current[0][1]+ Current[0][2]) < 1)
    {   
        //timeout with current almost zero
        DEBUGPORT.println("Timeout charger"+String(CHARGEROFFSET+1));
        digitalWrite(AUTHENTICATION_CAR1,LOW);
        LatestStartTime[0]=2147483548;
    }
    //DEBUGPORT.println(Current[1][0]+ Current[1][1]+ Current[1][2],4);
    //DEBUGPORT.println(String(LatestStartTime[1]+60));
    //DEBUGPORT.println(String(Time.now()));
    //DEBUGPORT.println((LatestStartTime[1] + 60 < Time.now()),DEC);
    if( ((numberOfZeroReadings[1]>10)||(LatestStartTime[1] + 70 > Time.now()) )&& (LatestStartTime[1] + 60 < Time.now()) && (Current[1][0]+ Current[1][1]+ Current[1][2]) < 1)
    {
        //timeout with current almost zero
        DEBUGPORT.println("Timeout charger"+String(CHARGEROFFSET+2));
        digitalWrite(AUTHENTICATION_CAR2,LOW);
        //digitalWrite(D7,LOW);
        LatestStartTime[1]=2147483548;
        //DEBUGPORT.println("Timeout charger2");
    }
    delay(100);
	//DEBUGPORT.println(Current[0],2);
	//DEBUGPORT.println(Current[1],2);
	//DEBUGPORT.println(Time.now());
	//DEBUGPORT.println(LatestStartTime[0]);
	//DEBUGPORT.println(LatestStartTime[1]);
    //toggle digital pins if an Authorized user swiped the RFID card   
    //if (Authorized_Charger1 || Authorized_Charger2)
    //{
    //  digitalWrite(D1,Authorized_Charger1);
    //  digitalWrite(D2,Authorized_Charger2);
    //  delay(3000);
    //  digitalWrite(D1,LOW);
    //  digitalWrite(D2,LOW);
    //}
    //else
    //{
    //  delay(100);
    //}
    
    //Reset the UIDtag if there is no car charging
    if ((activeCharger()!=1)&&(activeCharger()!=3))
        UIDtagCharger1="No ID";
    if ((activeCharger()!=2)&&(activeCharger()!=3))
        UIDtagCharger2="No ID";
            
    handledCharger = !handledCharger;
}