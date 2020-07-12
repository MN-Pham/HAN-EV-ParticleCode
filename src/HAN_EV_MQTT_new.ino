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

String UIDtagCharger1;
String UIDtagCharger2;

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
EVUser EVUserlist[SIZEOFUSERLIST];
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

void getUsers(String input) {
	//input is not used anymore!
	client.publish("HANevse/getUsers", "get");
	
}

void getUsers_callback(byte* payload, unsigned int length) {
	String data;
    int from = 0;
    int to = 0;
    
    char p[length + 1];
    memcpy(p, payload, length);
    
    p[length] = NULL;

    charToString(p, data);
    for (int i=0; i<SIZEOFUSERLIST; i++) {
        //Read User ID
        while (data[to]!='%') {
            to++;
        }
        EVUserlist[i].Id = (data.substring(from, to)).toInt();
        to++;
        from = to;
        //Read CarBrand
        while (data[to]!='%') {
            to++;
        }
        EVUserlist[i].CarBrand = data.substring(from, to);
        to++;
        from = to;
        //Read CarType
        while (data[to]!='%') {
            to++;
        }
        EVUserlist[i].CarType = data.substring(from, to);
        to++;
        from = to;
        //Read CarYear
        while (data[to]!='%') {
            to++;
        }
        EVUserlist[i].CarYear = (data.substring(from, to)).toInt();
        to++;
        from = to;
        //Read Owner
        while (data[to]!='%') {
            to++;
        }
        EVUserlist[i].Owner = data.substring(from, to);
        to++;
        from = to;
        //Read BatteryCapacity
        while (data[to]!='%') {
            to++;
        }
        EVUserlist[i].BatteryCapacity = (data.substring(from, to)).toFloat();
        to++;
        from = to;
        //Read UIDtag
        while (data[to]!='%') {
            to++;
        }
        EVUserlist[i].UIDtag = data.substring(from, to);
        to++;
        from = to;
        //Read PendingCharger
        while (data[to]!='%') {
            to++;
        }
        EVUserlist[i].PendingCharger = (data.substring(from, to)).toInt();
        to++;
        from = to;
        //Read StartTime
        while (data[to]!='%') {
            to++;
        }
        EVUserlist[i].StartTime = atol((data.substring(from, to)).c_str());
        to++;
        from = to;
    }
    //Testing purpose only
    //DEBUGPORT.println(EVUserlist[1].Owner);
    time_t time = Time.now();
    //DEBUGPORT.println(time);
    DEBUGPORT.print("MQTT>\tReceive EV user list from broker at: ");
    DEBUGPORT.println(Time.format(time, TIME_FORMAT_DEFAULT));
}

void getUpdate_callback(byte* payload, unsigned int length) {
	String data;
	int userId;
	int SocketId;
	unsigned long Stime;
    int from = 0;
    int to = 0;
    
    char p[length + 1];
    memcpy(p, payload, length);
    
    p[length] = NULL;
    
    charToString(p, data);
	
	//Read User Id
    while (data[to]!='%') {
        to++;
    }
    userId = (data.substring(from, to)).toInt();
    to++;
    from = to;
	//Read Socket ID
    while (data[to]!='%') {
        to++;
    }
    SocketId = (data.substring(from, to)).toInt();
    to++;
    from = to;
	//Read start time
    while (data[to]!='%') {
        to++;
    }
    Stime = atol((data.substring(from, to)).c_str());
    to++;
    from = to;

	for(int i=0; i<SIZEOFUSERLIST; i++) {
		if (EVUserlist[i].Id == userId) {
			EVUserlist[i].PendingCharger = SocketId;
			EVUserlist[i].StartTime = Stime;
			break;
		}
	}
	time_t time = Time.now();
    //DEBUGPORT.println(time);
    DEBUGPORT.print("MQTT>\tUpdate EV user list from broker at: ");
    DEBUGPORT.println(Time.format(time, TIME_FORMAT_DEFAULT));
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
        maxCurrentC1_test((int)(EMeterData[2].PhaseCurrent[0])); //Emeter3, I1
    }
    else if (activeCharger()==2) {
        maxCurrentC2_test((int)(EMeterData[2].PhaseCurrent[0])); //Emeter3, I1
    }
    else {
        maxCurrentC1_test((int)(EMeterData[2].PhaseCurrent[0]/2)); //Emeter3, I1
        maxCurrentC2_test((int)(EMeterData[2].PhaseCurrent[0]/2)); //Emeter3, I1
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    test = "99";
	if (strcmp(topic, "HANevse/UserList")==0) {
	    test = "1";
		getUsers_callback(payload, length);
	}
	else if (strcmp(topic, "HANevse/UpdateUser")==0) {
	    test = "2";
		getUpdate_callback(payload, length);
	}
	if (strcmp(topic, "HANevse/EnergyMeter")==0) {
	    test = "3";
	    getMeasure_callback(payload, length);
	}
	time_t time = Time.now();
    //DEBUGPORT.println(time);
    DEBUGPORT.print("MQTT>\tCallback function is called at: ");
    DEBUGPORT.println(Time.format(time, TIME_FORMAT_DEFAULT));
}

void updateUser(int UserId, int SocketId, unsigned long StartTime){
    String Body = "";
    Body = String(UserId) + "%" + String(SocketId) + "%" + String(StartTime)+"%"; 
    
    for(int i=0;i<3;i++) {
        if(client.publish("HANevse/UpdateUser", Body)) {
            break;
        }
    }
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

int forceUIDsoc1(String UID) {
    return testUser("K"+UID,1+CHARGEROFFSET);  
}

int forceUIDsoc2(String UID) {
    return testUser("K"+UID,2+CHARGEROFFSET);  
}

int authUserId(String Id, int socket) {
    //parse id string to int
    int UserId = Id.toInt();
    
    //see if anyone is charging here and stop that
    unauthSocket(String(socket,DEC));
    
    //now find new user 
    int k;
    for(k=0;k<SIZEOFUSERLIST;k++)
    {
      if(EVUserlist[k].Id == UserId)
      {
          break;
      }
    }
    
    //and take a new user
    EVUserlist[k].PendingCharger=socket+CHARGEROFFSET;
    EVUserlist[k].StartTime=Time.now();
    //Authorized=true;
    if (socket==1)
    {
        digitalWrite(AUTHENTICATION_CAR1,HIGH);
        LatestStartTime[0]=Time.now();
        blinkRFIDled(1,1);
    }
    if (socket==2)
    {
        digitalWrite(AUTHENTICATION_CAR2,HIGH);
        //digitalWrite(D7,HIGH);
        LatestStartTime[1]=Time.now();
        blinkRFIDled(2,1);
    }
    updateUser(EVUserlist[k].Id,EVUserlist[k].PendingCharger,EVUserlist[k].StartTime);
    DEBUGPORT.println("Authorized access charger"+String(EVUserlist[k].PendingCharger,DEC)+": "+EVUserlist[k].Owner+" "+EVUserlist[k].CarBrand+" "+String(EVUserlist[k].CarYear)+" with "+String(EVUserlist[k].BatteryCapacity,1)+" kWh @time "+String(EVUserlist[k].StartTime));
    DEBUGPORT.println(String(EVUserlist[k].Id)+"&pendingcharger="+String(EVUserlist[k].PendingCharger)+"&starttime="+String(EVUserlist[k].StartTime));
    DEBUGPORT.println();
    //return testUser("K"+UID,1+CHARGEROFFSET);      
    return socket;
}

int authUserSoc1(String Id) {
    return authUserId(Id,1);
}

int authUserSoc2(String Id) {
    return authUserId(Id,2);
}

int unauthSocket(String input) {
    //parse input string to int
    int socket = input.toInt() % 2;
    if (socket == 0) { socket = 2;}
    //DEBUGPORT.println("unauthorizeSocket called with argument: "+String(socket));
    //loop over the userlist and find the corresponding user
    int k;
    for(k=0;k<SIZEOFUSERLIST;k++)
    {
      if(EVUserlist[k].PendingCharger == socket)
      {
          break;
      }
    }
    // stop charging
    //DEBUGPORT.println("STOP CHARGEING");
    //DEBUGPORT.print("Stop new charge because charger " + String(EVUserlist[k].PendingCharger) +"==" + String(Charger) +" starttime in EVuserlist " + String(EVUserlist[k].StartTime) + " now= " + String( Time.now()));
    EVUserlist[k].PendingCharger=0;
    EVUserlist[k].StartTime = Time.now();
    //Authorized=true;
    if (socket==1)
    {
        digitalWrite(AUTHENTICATION_CAR1,LOW);
    }
    //(Charger==2+CHARGEROFFSET)
    //take socket==0 because of modulo 2 arithmetic
    if (socket==2 || socket == 0)
    {
        digitalWrite(AUTHENTICATION_CAR2,LOW);
        //digitalWrite(D7,LOW);
    }
    blinkRFIDled(socket,4);
    //DEBUGPORT.println("EVuserlist[k].StartTime set to "+String(EVUserlist[k].StartTime)); 
    updateUser(EVUserlist[k].Id,EVUserlist[k].PendingCharger,EVUserlist[k].StartTime);
    //DEBUGPORT.println(String(EVUserlist[k].Id)+"&pendingcharger="+String(EVUserlist[k].PendingCharger)+"&starttime="+String(EVUserlist[k].StartTime));
    return socket;         
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

bool testUser(String content, int Charger) {
    bool Authorized=false;
    DEBUGPORT.println();
    DEBUGPORT.print("Message : ");

    content.toUpperCase();
  //bool Authorized_Charger1=false;
    for (int k=0;k<SIZEOFUSERLIST;k++)
    {
      DEBUGPORT.println(k, DEC);
      if (content.substring(1) == EVUserlist[k].UIDtag) //Remove?????
      {
        if (EVUserlist[k].PendingCharger==0) //if the user is not charging
        {
            //see if someone else is charging here
            bool ChargerIsOccupied=false;
            for(int j=0;j<SIZEOFUSERLIST;j++)
            {
                if (EVUserlist[j].PendingCharger == Charger)
                {
                    ChargerIsOccupied=true; 
                }
            }
            //if charger is not occupied, take new user
            if(!ChargerIsOccupied && (EVUserlist[k].StartTime + 20 < Time.now()))
            {
                DEBUGPORT.println("Start new charge because charger occupied: " + String(ChargerIsOccupied) + " starttime in EVuserlist " + String(EVUserlist[k].StartTime) + " now= " + String( Time.now()));
                EVUserlist[k].PendingCharger=Charger;
                EVUserlist[k].StartTime=Time.now();
                Authorized=true;
                if (Charger==1+CHARGEROFFSET)
                {
                    digitalWrite(AUTHENTICATION_CAR1,HIGH);
                    LatestStartTime[0]=Time.now();
                    blinkRFIDled(1,1);
                }
                if (Charger==2+CHARGEROFFSET)
                {
                    digitalWrite(AUTHENTICATION_CAR2,HIGH);
                    //digitalWrite(D7,HIGH);
                    LatestStartTime[1]=Time.now();
                    blinkRFIDled(2,1);
                }
                updateUser(EVUserlist[k].Id,EVUserlist[k].PendingCharger,EVUserlist[k].StartTime);
                DEBUGPORT.println("Authorized access charger"+String(EVUserlist[k].PendingCharger,DEC)+": "+EVUserlist[k].Owner+" "+EVUserlist[k].CarBrand+" "+String(EVUserlist[k].CarYear)+" with "+String(EVUserlist[k].BatteryCapacity,1)+" kWh @time "+String(EVUserlist[k].StartTime));
                DEBUGPORT.println(String(EVUserlist[k].Id)+"&pendingcharger="+String(EVUserlist[k].PendingCharger)+"&starttime="+String(EVUserlist[k].StartTime));
                DEBUGPORT.println();
                //delay(3000);
            }
            else
            {
                DEBUGPORT.println("Charger is occupied or you did a second try to start charging within 20sec.");
                
                if(!ChargerIsOccupied)
                {
                    //
                    blinkRFIDled(Charger-CHARGEROFFSET,2);
                }
                else
                {
                    blinkRFIDled(Charger-CHARGEROFFSET,3);
                }
            }
        }
        else
        {
            DEBUGPORT.println("You are allready charging at charger "+String(EVUserlist[k].PendingCharger,DEC));
            if(EVUserlist[k].PendingCharger == Charger && (EVUserlist[k].StartTime + 20 < Time.now()))
            {
                //you swiped the card on the charger where you are charging--> stop charging
                DEBUGPORT.println("STOP CHARGEING");
                DEBUGPORT.print("Stop new charge because charger " + String(EVUserlist[k].PendingCharger) +"==" + String(Charger) +" starttime in EVuserlist " + String(EVUserlist[k].StartTime) + " now= " + String( Time.now()));
                EVUserlist[k].PendingCharger=0;
                EVUserlist[k].StartTime = Time.now();
                Authorized=true;
                if (Charger==1+CHARGEROFFSET)
                {
                    digitalWrite(AUTHENTICATION_CAR1,LOW);
                }
                if (Charger==2+CHARGEROFFSET)
                {
                    digitalWrite(AUTHENTICATION_CAR2,LOW);
                    //digitalWrite(D7,LOW);
                }
                blinkRFIDled(Charger-CHARGEROFFSET,4);
                DEBUGPORT.println("EVuserlist[k].StartTime set to "+String(EVUserlist[k].StartTime)); 
                updateUser(EVUserlist[k].Id,EVUserlist[k].PendingCharger,EVUserlist[k].StartTime);
                DEBUGPORT.println(String(EVUserlist[k].Id)+"&pendingcharger="+String(EVUserlist[k].PendingCharger)+"&starttime="+String(EVUserlist[k].StartTime));
                
            }
            else
            {
                if(EVUserlist[k].PendingCharger == Charger)
                {
                    blinkRFIDled(Charger-CHARGEROFFSET,5);
                }
                else
                {
                    blinkRFIDled(Charger-CHARGEROFFSET,6);
                }
            }
        }

      }
     
     else   {
        DEBUGPORT.println(" Try next user");
        //delay(3000);
      }
      if(EVUserlist[k].CarYear==0 || Authorized==true)
      {
          blinkRFIDled(Charger-CHARGEROFFSET,7);
          break;
      }
    } 
    return Authorized;
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

int funcName(String extra) {
    return 0;
}

void reconnect(void) {
    while (!client.isConnected()) {
        DEBUGPORT.print("MQTT>\tConnecting to MQTT broker...");
        if (client.connect("EV-Photon1")) {
            DEBUGPORT.println("MQTT>\tConnected");
            //client.subscribe("HANevse/#", client.QOS2);
            client.subscribe("HANevse/EnergyMeter", client.QOS2);
            client.subscribe("HANevse/UpdateUser", client.QOS2);
            client.subscribe("HANevse/UserList", client.QOS2);
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
    //Particle.function("funcKey", funcName);
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
    
    for(int i=0;i<SIZEOFUSERLIST;i++)
    {
       EVUserlist[i]={0,"","",0,"",0,"",0,0};  
    }
    //Particle.process();
    //resetOlimex("");
    //Particle.process();
    
	//Particle.function("getUsers",getUsers);
	Particle.function("switchTest",switchTest);
    Particle.function("maxCurrentC1",maxCurrentC1);
    Particle.function("maxCurrentC2",maxCurrentC2);
    //Particle.function("forceUIDsoc1",forceUIDsoc1); //Remove because cannot check the database anymore
    //Particle.function("forceUIDsoc2",forceUIDsoc2); //Remove because cannot check the database anymore
    Particle.function("resetOlimex",resetOlimex);
    Particle.function("progModeOlmx",progModeOlmx);
    //Particle.function("unauthSocket",unauthSocket); //Remove because cannot check the database anymore
    Particle.function("resetParticl",resetParticl);
    //Particle.function("authUserSoc1",authUserSoc1); //Remove because cannot check the database anymore
    //Particle.function("authUserSoc2",authUserSoc2); //Remove because cannot check the database anymore
    //Particle.function("AuthPinsHigh",AuthPinsHigh);
    //Particle.function("AuthPinsLow",AuthPinsLow);
    Particle.function("WifiSignal",WifiSignal);
    Particle.function("initRFID",initRFID);
    //Particle.variable("EVListStr", EVListStr); //Remove because cannot check the database anymore
    Particle.variable("currentStr",currentStr);
    Particle.variable("ShareVar",ShareVar);
    //Particle.variable("Current", Current_Str);
    Particle.variable("Topic", test);
    Particle.process();
	
	getUsers("");

    //EVUserlist[0]={0,"Nissan","Leaf", 2011, "Yuri", 21.2, "97 2D 39 5D",0,0};
    //EVUserlist[1]={1,"Tesla","model S", 2016, "Trung", 85.0, "04 4E 79 EA FA 4D 80",0,0};
    //EVUserlist[0]={"Nissan Leaf", 2011, "Yuri", 21.2, "97 2D 39 5D",0,0};
    //EVUserlist[1]={"Tesla model S", 2016, "Trung", 85.0, "04 4E 79 EA FA 4D 80",0,0};
    //DEBUGPORT.println(EVUserlist[0].Owner);
    //DEBUGPORT.println(EVUserlist[1].Owner);
    //DEBUGPORT.println(EVUserlist[2].Owner);         
    //DEBUGPORT.println(EVUserlist[3].Owner);
	
	RGB.control(true);
    Time.zone(1); //Dutch time zone
    
    //NO NEED, the photon will connect to the broker
    //client.connect("EV-Photon1");//Connect to the broker
    
    //if (client.isConnected()) {
        //test = "3";
    //    client.subscribe("UserList");
        //test = "4";
	//	client.subscribe("updateUser");
    //    RGB.color(0, 255, 0); //Green led
    //}
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
        //unsigned long temptime = EVUserlist[0].StartTime%10;
        //char buffer[15];
        //sprintf(buffer,"%ld", EVUserlist[0].StartTime);
        //DEBUGPORT.print(buffer);
        //DEBUGPORT.println(" "+String(temptime));
		//DEBUGPORT.print("Userlist>\t");DEBUGPORT.print(EVUserlist[0].Id);DEBUGPORT.print("-");DEBUGPORT.print(EVUserlist[0].Owner);DEBUGPORT.print(EVUserlist[0].UIDtag);DEBUGPORT.print("-");DEBUGPORT.print(EVUserlist[0].PendingCharger);DEBUGPORT.print("-");DEBUGPORT.println(EVUserlist[0].StartTime,DEC);
		//DEBUGPORT.print("Userlist>\t");DEBUGPORT.print(EVUserlist[1].Owner);DEBUGPORT.print(EVUserlist[1].UIDtag);DEBUGPORT.print("-");DEBUGPORT.print(EVUserlist[1].PendingCharger);DEBUGPORT.print("-");DEBUGPORT.println(String(EVUserlist[1].StartTime));
		//DEBUGPORT.print("Userlist>\t");DEBUGPORT.print(EVUserlist[2].Owner);DEBUGPORT.print(EVUserlist[2].UIDtag);DEBUGPORT.print("-");DEBUGPORT.print(EVUserlist[2].PendingCharger);DEBUGPORT.print("-");DEBUGPORT.println(String(EVUserlist[2].StartTime));
		counter = 0;
		DEBUGPORT.println("LatestStartTime>\t"+String(LatestStartTime[0])+", "+String(LatestStartTime[1]));
		DEBUGPORT.println(String(Current[1][0]+ Current[1][1]+ Current[1][2]));
		
		//EVListStr = 
		//EVUserlist[0].Owner+" "+String(EVUserlist[0].PendingCharger)+" "+String(EVUserlist[0].StartTime)+"; "+
		//EVUserlist[1].Owner+" "+String(EVUserlist[1].PendingCharger)+" "+String(EVUserlist[1].StartTime)+"; "+
		//EVUserlist[2].Owner+" "+String(EVUserlist[2].PendingCharger)+" "+String(EVUserlist[2].StartTime)+"; "+
		//EVUserlist[3].Owner+" "+String(EVUserlist[3].PendingCharger)+" "+String(EVUserlist[3].StartTime)+"; "+
		//EVUserlist[4].Owner+" "+String(EVUserlist[4].PendingCharger)+" "+String(EVUserlist[4].StartTime)+"; "+
		//EVUserlist[5].Owner+" "+String(EVUserlist[5].PendingCharger)+" "+String(EVUserlist[5].StartTime)+"; "+
		//EVUserlist[6].Owner+" "+String(EVUserlist[6].PendingCharger)+" "+String(EVUserlist[6].StartTime)+"; "+
		//EVUserlist[7].Owner+" "+String(EVUserlist[7].PendingCharger)+" "+String(EVUserlist[7].StartTime)+"; "+
		//EVUserlist[8].Owner+" "+String(EVUserlist[8].PendingCharger)+" "+String(EVUserlist[8].StartTime)+"; "+
		//EVUserlist[9].Owner+" "+String(EVUserlist[9].PendingCharger)+" "+String(EVUserlist[9].StartTime)+"; "+
		//EVUserlist[10].Owner+" "+String(EVUserlist[10].PendingCharger)+" "+String(EVUserlist[10].StartTime)+"; "+
		//EVUserlist[11].Owner+" "+String(EVUserlist[11].PendingCharger)+" "+String(EVUserlist[11].StartTime)+"; "+
		//EVUserlist[12].Owner+" "+String(EVUserlist[12].PendingCharger)+" "+String(EVUserlist[12].StartTime)+"; "+
		//EVUserlist[13].Owner+" "+String(EVUserlist[13].PendingCharger)+" "+String(EVUserlist[13].StartTime)+"; "+
		//EVUserlist[14].Owner+" "+String(EVUserlist[14].PendingCharger)+" "+String(EVUserlist[14].StartTime)+"; "+
		//EVUserlist[15].Owner+" "+String(EVUserlist[15].PendingCharger)+" "+String(EVUserlist[15].StartTime)+"; "+
		//EVUserlist[16].Owner+" "+String(EVUserlist[16].PendingCharger)+" "+String(EVUserlist[16].StartTime)+"; "+
		//EVUserlist[17].Owner+" "+String(EVUserlist[17].PendingCharger)+" "+String(EVUserlist[17].StartTime)+"; "+
		//EVUserlist[18].Owner+" "+String(EVUserlist[18].PendingCharger)+" "+String(EVUserlist[18].StartTime)+"; "+
		//EVUserlist[19].Owner+" "+String(EVUserlist[19].PendingCharger)+" "+String(EVUserlist[19].StartTime)+"; "+
		//EVUserlist[20].Owner+" "+String(EVUserlist[20].PendingCharger)+" "+String(EVUserlist[20].StartTime)+"; ";
		/*
		String EVStr = "";
		for (int i = 0; i<SIZEOFUSERLIST; i++) {
		    EVStr = EVStr + String(EVUserlist[0].Id)+" "+EVUserlist[0].Owner+" "+String(EVUserlist[0].PendingCharger)+" "+String(EVUserlist[0].StartTime)+"; ";
		}
		EVListStr = EVStr;
        */
    }
    counter++;
		
    // store new measurement value if it is received correctly from energymeter (via the Olimex).
    if(millis()>nextTime[handledCharger] && (Charger==1+CHARGEROFFSET || Charger==2+CHARGEROFFSET)) 
    {
        Particle.process();
        //getUserIdAtSocket(Charger)
        int tempCharger = Charger;
        Charger = handledCharger + 1;
        if(getUserIdAtSocket(Charger) != "00") //Replace Charger+CHARGEROFFSET by Charger?
        {
            add_Measurement(PhaseVoltage[Charger-1][0], PhaseVoltage[Charger-1][1], PhaseVoltage[Charger-1][2], Current[Charger-1][0], Current[Charger-1][1], Current[Charger-1][2], Power[Charger-1][0]+Power[Charger-1][1]+Power[Charger-1][2], Energy[Charger-1], Frequency[Charger-1], Time.now(), Charger, getUserIdAtSocket(Charger));
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
        /*
        for(int j=0;j<SIZEOFUSERLIST;j++)
        {
            if (EVUserlist[j].PendingCharger == 1+CHARGEROFFSET)
            {
                Particle.process();
                DEBUGPORT.println("user1 at index: "+String(j,DEC));
                EVUserlist[j].PendingCharger=0;
                //EVUserlist[j].StartTime = Time.now();//if you didnot connect within 20s, you are allowed to checkin immediately
                //updateUser(EVUserlist[j].Id,EVUserlist[j].PendingCharger,EVUserlist[j].StartTime);
                break;
            }
            
        }
        */
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
        /*
        for(int j=0;j<SIZEOFUSERLIST;j++)
        {
            if (EVUserlist[j].PendingCharger == 2+CHARGEROFFSET)
            {
                Particle.process();
                DEBUGPORT.println("user2 at index: "+String(j,DEC));
                EVUserlist[j].PendingCharger=0;
                //EVUserlist[j].StartTime = Time.now(); //if you didnot connect within 20s, you are allowed to checkin immediately
                updateUser(EVUserlist[j].Id,EVUserlist[j].PendingCharger,EVUserlist[j].StartTime);
                DEBUGPORT.println(String(EVUserlist[j].Id)+"&pendingcharger="+String(EVUserlist[j].PendingCharger)+"&starttime="+String(EVUserlist[j].StartTime));
                break;
            }
            
        }
        */
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
    handledCharger = !handledCharger;
}