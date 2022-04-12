//Connections:
//Serial <-> Arduino PC
//Serial 1 <-> ESP Module 
//Serial 2 <-> Bluetooth Module

//Including Libraries
#include <SPI.h> //Library for SPI communications
#include "SdFat.h" //Library for SD Card module
#include <SoftwareSerial.h>//Library for UART Serial communications.
#include <virtuabotixRTC.h>//Library for real time module.  
#include <dht.h>//DHT library
#include <SFE_BMP180.h>//Barometric sensor
#include <Wire.h>//Library from arduino used for I2C communications.
#include <Adafruit_Sensor.h>//Adafruits main library
#include <Adafruit_LSM303_U.h>//Library for IMU
#include <Adafruit_L3GD20_U.h>//Library for IMU
#include <Adafruit_9DOF.h>//Library for IMU
// You will need to create an SFE_BMP180 object, here called "pressure":
#define ALTITUDE 1655.0 // Altitude of SparkFun's HQ in Boulder, CO. in meters
#define SD_FAT_TYPE 0
#define LED_PIN 13 //LED pin for similarity checking.
#define DHT11_PIN 9//Pin per temperatur edhe humidity.
// SDCARD_SS_PIN is defined for the built-in SD on some boards.
#ifndef SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SS;
#else  // SDCARD_SS_PIN
// Assume built-in SD is used.
const uint8_t SD_CS_PIN = SDCARD_SS_PIN;
#endif  // SDCARD_SS_PIN

// Try to select the best SD card configuration.
#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI)
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI)
#endif  // HAS_SDIO_CLASS

#if SD_FAT_TYPE == 0
SdFat sd;
File dir;
File file;
#elif SD_FAT_TYPE == 1
SdFat32 sd;
File32 dir;
File32 file;
#elif SD_FAT_TYPE == 2
SdExFat sd;
ExFile dir;
ExFile file;
#elif SD_FAT_TYPE == 3
SdFs sd;
FsFile dir;
FsFile file;
#else  // SD_FAT_TYPE
#error invalid SD_FAT_TYPE
#endif  // SD_FAT_TYPE
//------------------------------------------------------------------------------
// Store error strings in flash to save RAM.
#define error(s) sd.errorHalt(&Serial, F(s))
// ------------------------ Static Config Variables ----------------
//File myFile;//Declare the File object for SD Card
virtuabotixRTC myRTC(49, 48, 8);//Real time shield object.
SFE_BMP180 pressure;//Barometric pressure objeect
dht DHT;//DHT sensor library object
Adafruit_LSM303_Accel_Unified accel = Adafruit_LSM303_Accel_Unified(30301);//Unique identifier for accelerometer
Adafruit_LSM303_Mag_Unified   mag   = Adafruit_LSM303_Mag_Unified(30302);//Unique identifier for magnetometer
const String Wifi_SSID = "MEO-8C0021-5G"; //SSID of the wifi
const String Wifi_Password = "e1959f0ce"; //Password
const String IP_RemoteServer = "92.205.12.117";//IP Address of the server
const String domainName = "www.arduinocommunication.com";//Domain of the server
const String PHP_FilePath = "/test/";
bool ESP_Debugging = true;//If true, then this variable will set the ESP data flow dump to serial.
bool BTS_Debugging = true;//if true, then this variable will set the Bluetooth data flow dump to serial
int DeviceID = 1;
char line[40]; //for sd card reading.
// ----------------------------------------------------------------
// ---------------- Dynamical Variables ----------------------------
bool InternetConnection = false;//Global
bool LED_Status = false;//Led for similar data.
bool PastFilesStored = false;//Global, used to tell if there are any files in the sd card, so it iterates 3 times to send data and delete them.
String Bluetooth_Serial_Data = "";
//Variables for bluetooth communications
bool SecondDevice_InternetConenction = true;//It firstly assumes there is connection
int CommunicationState = 0;// 0 for idle, 1 for packet sent waiting for ack the other party has connection, 2 for no internet connection on the other parties, 3 for other party no connection he is waiting ack. 
String Ackd_FILENAME = "";//The acked file name.
String ToBeSent_Packet_name = "";//Packet to be sent
String Remote_PacketData = "";
String Remote_PacketName = "";
//In case you dont have internet
String PacketName_OWN_SDCARD = "";//This is the file name being sent to the other device that has internet conn
//In case you have internet
String PacketName_REMOTE = "";
String PacketData = "";
//Sensor Variables
float MagnetometerX = 0;
float MagnetometerY= 0;
float MagnetometerZ= 0;
float GyroX= 0;
float GyroY= 0;
float GyroZ= 0;
float Temperature= 0;
float Humidity= 0;
float AccelerometerX= 0;
float AccelerometerY= 0;
float AccelerometerZ= 0;
String TimeMoment = "";
String DateMoment = "";
String SD_CurrentFile = "";
int LED_Iterations = 0;
// ----------------------------------------------------------------

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Booting up.. "); //
  pinMode(LED_PIN, OUTPUT);
  Boot();//The device starts boots up and test each module for proper connection (Bluetooth, )
}

void loop() {
    //Code to run forever.
    CheckInternet_World();//Check Internet.

    if (InternetConnection){
       //What to do if device has internet.
       Serial.println("Sending data to the server");
       SendDataToServer_INTERNET_AVAILABLE();//Sends to server.
       
    }else{
        //What to do if device has no internet.
        Serial.println("NO INTERNET !!");
        INTERNET_UNAVAILABLE();
    }

    if (LED_Status) {
        digitalWrite(LED_PIN, HIGH);
        LED_Iterations++;
        if (LED_Iterations>3){
            digitalWrite(LED_PIN, LOW);
            LED_Iterations =0 ;
            LED_Status = false;
        }
    }

    delay(1000);//Default Delay.
}

//The upper part of the algorithm, what the device does in case there is internet.
//Main KEY functions.
void SendDataToServer_INTERNET_AVAILABLE(){
    SensorReadOut(); //Update sensor values
    UpdateTime(); //Update time.
    String Final_Path = PHP_FilePath + "index.php?d="+String(DeviceID)+"&time=" + TimeMoment + "&date="+DateMoment+"&mX="+String(MagnetometerX)+"&mY="+String(MagnetometerY)+"&mZ="+String(MagnetometerZ)+"&gX="+String(GyroX)+"&gY="+String(GyroY)+"&gZ="+String(GyroZ)+"&t="+String(Temperature)+"&h=" + String(Humidity)+ "&aX="+String(AccelerometerX)+"&aY="+String(AccelerometerY)+"&aZ="+String(AccelerometerZ);
    String ApplicationLayerPacket = "GET "+Final_Path+" HTTP/1.1\r\nHost: "+domainName+"\r\nConnection: close\r\nUser-Agent: IoT device 1.0\r\n\r\n";//Build the HTTP packet.
    int Length = ApplicationLayerPacket.length();//Get the length for the network layer headers
    sendData_ESP("AT+CIPSTART=4,\"TCP\",\""+IP_RemoteServer+"\",80\r\n",3000,ESP_Debugging);//Initiate TCP connection to the server
    sendData_ESP("AT+CIPSEND=4," + String(ApplicationLayerPacket.length() + 4) + "\r\n",4000,ESP_Debugging);//Open sender for the HTTP packet
    sendData_ESP(ApplicationLayerPacket,2500,ESP_Debugging);//Send the HTTP packet
    sendData_ESP("AT+CIPCLOSE\r\n",1200,ESP_Debugging);//Close the TCP connection.
    //Data Sent to the server. Now check if the file past.txt exists, and read to check if there are any past data sets.
    ListingFiles();
    Serial.println("Are there files:");
    Serial.println(PastFilesStored);
    if (PastFilesStored){
      for (int i = 0; i < 3; i++) {//Loop to check for 3 files inside sd card, end them and delete them.
        FixText(true);//This will fix the file name.
        String InsideData = ReadTargetFile();//Read the data inside the file.
        Serial.println("Inside Data:"+ReadTargetFile());//Print the data.
        String Final_Path = PHP_FilePath + "index.php?sd="+InsideData;
        String ApplicationLayerPacket = "GET "+Final_Path+" HTTP/1.1\r\nHost: "+domainName+"\r\nConnection: close\r\nUser-Agent: IoT device 1.0\r\n\r\n";//Build the HTTP packet.
        int Length = ApplicationLayerPacket.length();//Get the length for the network layer headers
        sendData_ESP("AT+CIPSTART=4,\"TCP\",\""+IP_RemoteServer+"\",80\r\n",3000,ESP_Debugging);//Initiate TCP connection to the server
        sendData_ESP("AT+CIPSEND=4," + String(ApplicationLayerPacket.length() + 4) + "\r\n",4000,ESP_Debugging);//Open sender for the HTTP packet
        sendData_ESP(ApplicationLayerPacket,2500,ESP_Debugging);//Send the HTTP packet
        sendData_ESP("AT+CIPCLOSE\r\n",1200,ESP_Debugging);//Close the TCP connection.
        sd.remove(SD_CurrentFile);//Delete the file.
        ListingFiles();//Recheck if there is more files inside the sd card.
      }
   }

    if (LED_Status){
      digitalWrite(LED_PIN,HIGH);
    }else{
      digitalWrite(LED_PIN,LOW);
    }

   //If the second device has written something in the bluetooth. 
   if (Serial2.available() > 0) {
       Bluetooth_Serial_Data = ""; Bluetooth_Serial_Data = BTSManager(true, "", 1000);//Read bluetooth data.
       Serial.println();
       Serial.println("Other Party DaTA: ");
       Serial.println(Bluetooth_Serial_Data);
       Serial.println("");
       BTSParseSerial();//Parse the bluetooth data, and do following our BTS Application Layer protocol.
   }

    if (ToBeSent_Packet_name != "") {ToBeSent_Packet_name="";}
    //End of function.
}

//The lower part of the algorithm, what the device does in case there is no internet.
//Main KEY functions.
void INTERNET_UNAVAILABLE(){
    SensorReadOut(); //Update sensor values
    UpdateTime(); //Update time.  
    //Create the packet format and store it in the sd card with a random file name.
    String dataPacket = "..." + TimeMoment + "," + DateMoment + "," + String(MagnetometerX)+","+String(MagnetometerY)+","+String(MagnetometerZ)+","+String(GyroX)+","+String(GyroY)+","+String(GyroZ)+","+String(Temperature)+"," + String(Humidity)+ ","+String(AccelerometerX)+","+String(AccelerometerY)+","+String(AccelerometerZ) + "," + String(DeviceID);
    randomSeed(analogRead(A0));
    int RandFile = random(0, 99999);
    if (sd.exists(String(RandFile)+".txt")){//Used for protection.
        sd.remove(String(RandFile)+".txt");//overwrite old file..
    }
    file = sd.open(String(RandFile)+".txt", FILE_WRITE);
    file.print(dataPacket);//Print file 
    file.close();

      //If there is communication available.
    if (Serial2.available() > 0) {
      Bluetooth_Serial_Data = ""; Bluetooth_Serial_Data = BTSManager(true, "", 500);
      BTSParseSerial();
    }
    //SD_CurrentFile
    if (ToBeSent_Packet_name == "") {
      //This automatically means that have a slot (In the case of no internet) to send another packet from our sd card.
       ListingFiles();//Gets a next file in the sd card to be sent.
       FixText(true);//This will fix the file name.
       String InsideData = ReadTargetFile();//Read the data inside the file.
       ToBeSent_Packet_name = SD_CurrentFile;//Assign this variables so we dont flood the variables.
       FixText(false);//This will fix the file name.
       String Packetization = InsideData+"%"+SD_CurrentFile;
       //String ByteAddr_1 = Packetization.substring(0,63);
       //String ByteAddr_2 = Packetization.substring(63,Packetization.length());
       //Serial2.print(Packetization);//Send packet to the remote device.
       //delay(200);
       Serial2.println(Packetization);//Send packet to the remote device.
       Serial.println("Packet Sent " + Packetization);
    }
    delay(5000);//Default larger delay.
}

void Boot() {//The boot up function, called at the very beginning
  delay(2000);//This delay is for in case the user wants to configure the time of the real time module.
  Serial1.begin(115200);//Open UART connection to the ESP module
  Serial2.begin(9600);//Open UART connection to the HC-05 bluetooth module
  //---------------------- Sd Card initiator ----------------------
  if (!sd.begin(SD_CS_PIN)) {
    Serial.println("initialization failed - SD CARD ERROR! PLEASE MAKE SURE SD CARD MODULE IS WORKING IN ORDER TO CONTINUE WITH THE BOOT.");
  }
  //---------------------------------------------------------------
  
  //----------------------Real time module initiator ---------------------
  myRTC.setDS1302Time(00, 30, 12, 6, 10, 6, 2021); 
  //seconds, minutes, hours, day of the week, day of the month, month, year (uncomment to update time.)
  //---------------------------------------------------------------

  //---------------------- ESP initiator ----------------------
      ConnectToWifi();
      CheckInternet_World();
  //---------------------------------------------------------------

 if (!sd.begin(SD_CONFIG)) {
    Serial.println("Error SD CARD");
  }

  //---------------------- BAROMETRIC SENSOR BOOT UP --------------
    if (pressure.begin())
        Serial.println("BMP180 init success");
    else
    {
        Serial.println("BMP180 init fail\n\n");
    }
  //--------------------------------------------------------------

  //----------------- Adafruits sensors boot up --------------------

  //----------------------------------------------------------------

  Serial.println("--Booting finished successfully");
}



//---------------- SENSOR READOUT ------------------------------------------------------------------------------------------------------------------------------------
//All functions for Sensors go inside this region
void SensorReadOut(){
    // DHT Temperature And Humidity:
      int chk = DHT.read11(DHT11_PIN);
      Temperature= DHT.temperature;
      Humidity= DHT.humidity;
    //-----------------------------

    //Barometric sensor
       char status;
       double T,P,p0,a;
       status = pressure.startPressure(3);
        if (status != 0)
        {
          delay(status);
          status = pressure.getPressure(P,T);
          if (status != 0)
          {
              // Print out the measurement:
              GyroX= P; //REPRESENTS PRESSURE
              p0 = pressure.sealevel(P,ALTITUDE); // we're at 1655 meters (Boulder, CO)
              GyroY= p0;//REPRESENTS SEALEVEL
              a = pressure.altitude(P,p0);
              GyroZ= a;//REPRESENTS ALTITUDE
          }
        } 
        
    //----------------------------

    //--------------------- Adafruits sensors -----------------
    //sensors_event_t event;
    // mag.getEvent(&event);
    MagnetometerX = 0;
    MagnetometerY = 0;
    MagnetometerZ = 0; 
   // accel.getEvent(&event);
    AccelerometerX= 0;
    AccelerometerY= 0;
    AccelerometerZ= 0;
    //--------------------------------------------------------
}
//End functions for Sensors.
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------



//---------------- ESP NETWORKING Utilities --------------------------------------------------------------------------------------------------------------------------

//The following function will try to connect the esp to WIFI.
void ConnectToWifi(){
        //The following commands are necessary to preperely configure ESP to connect to the wifi.
      delay(3000);
      sendData_ESP("AT+CWMODE=1\r\n",1000,ESP_Debugging);
          delay(3000);
      sendData_ESP("AT+CWJAP=\""+Wifi_SSID+"\",\""+Wifi_Password+"\"\r\n",6000,ESP_Debugging);
          delay(3000);
      sendData_ESP("AT+CIPMODE=0\r\n",2000,ESP_Debugging);
          delay(3000);
      sendData_ESP("AT+CIPMUX=1\r\n",2000,ESP_Debugging);
          delay(2000);
}

//The following function sends and retrives the response of AT Commands of ESP
String sendData_ESP(String command, const int timeout, boolean debug) 
{
    String response = "";
    Serial1.print(command); 
    long int time = millis();   
    while( (time+timeout) > millis())
    {
          while(Serial1.available())
          {
                char c = Serial1.read(); 
                if (c=='|') {
                  LED_Status = true;
                }
                response+=c;
          }
      }    
      if(debug)
      {
           Serial.print(response);
      }  

    return response;
}

//The following algorithm extracts the IPv4 assigned IP address from the AT+CIFSR command response
String GetIP(){
      String Final = "Erri";
      String data = sendData_ESP("AT+CIFSR\r\n",500,false);
      int cc = data.length();
      char mc[255]; 
      data.toCharArray(mc, cc);
     int index_first_apostrophe = 0; 
      while (mc[index_first_apostrophe] != '"'){   
        if (mc[index_first_apostrophe+1]=='"'){
          index_first_apostrophe++;
          break;
        }
        index_first_apostrophe++;
      }
      int index_last_apostrophe = index_first_apostrophe+1;
      while (mc[index_last_apostrophe] != '"') {
        if (mc[index_last_apostrophe+1]=='"'){
          index_last_apostrophe++;
          break;
        }
        index_last_apostrophe++;
      }
      Final="";
      index_first_apostrophe++;
      while (index_first_apostrophe != index_last_apostrophe){
        Final += mc[index_first_apostrophe];
        index_first_apostrophe++;
      }
      return Final;
}

//THe following function will ping google to see if there is a connection, and will change the global 
//Dynamical variable to true or false depending.
void CheckInternet_World(){
    String PingGoogle = sendData_ESP("AT+PING=\"www.google.com\"\r\n",3000,true);
    if (PingGoogle.indexOf("ERROR") > 0) {
      InternetConnection = false;
    }else if (PingGoogle.indexOf("OK")>0){
      InternetConnection = true;
    }else{
        InternetConnection = false;
    }
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


//---------------------------------------------------- BLUETOOTH HC-05 UTILITIES ----------------------------------------------------------------------------------------------
//The following algorihtm is used to read from the bluetooth connection, 
String BTSManager(bool ReadOnly, String command, const int timeout) 
{
    String response = "";
    if (!ReadOnly){
      Serial2.println(command);
    }
    long int time = millis();   
    while( (time+timeout) > millis())
    {
          while(Serial2.available())
          {
                char c = Serial2.read(); 
                response+=c;
          }
      }    
      if(BTS_Debugging)
      {
           Serial.print(response);
      }  
    return response;
}

//This is a key functions to realise the communication between the two devices. It understands the state of the both systems. Updates integer values to the communication state variable
void BTSParseSerial() {
  Serial.print("OKAY SO WE GOT:");
  Serial.println(Bluetooth_Serial_Data);
    if (Bluetooth_Serial_Data.indexOf("#") > -1) {
      //Ack packet received
       SecondDevice_InternetConenction = true;//This means that the second device has internet connection.
      if (InternetConnection){
          //Random ACK received for a file name probably send previously, make sure to erase that file from sd card.
          //GetFileName_FromACK();      
          sd.remove(ToBeSent_Packet_name);//Delete the file from sd card (because it is already sent to the server).
          Ackd_FILENAME = "";//Flush the variable.
          ToBeSent_Packet_name = "";
          Serial.println("Im here 1");
      }else{
          //Acknowledgment received from a packet you sent earlier (probably).
          // GetFileName_FromACK();
          sd.remove(ToBeSent_Packet_name);//Delete the file from sd card 
          ToBeSent_Packet_name = "";
          Ackd_FILENAME = "";//Flush the variable.
          Serial.println("Im here 2");
      }
    }else if (Bluetooth_Serial_Data.indexOf(".") >= 0){
      //Second device has no internet, packet received
      SecondDevice_InternetConenction = false;
      if (InternetConnection){
          //If you have internet then you must take the packet send it to the server and send an ACK via bluetooth
          GetPacketData();
          String Final_Path = PHP_FilePath + "index.php?sd="+Remote_PacketData;
          String ApplicationLayerPacket = "GET "+Final_Path+" HTTP/1.1\r\nHost: "+domainName+"\r\nConnection: close\r\nUser-Agent: IoT device 1.0\r\n\r\n";//Build the HTTP packet.
          int Length = ApplicationLayerPacket.length();//Get the length for the network layer headers
          sendData_ESP("AT+CIPSTART=4,\"TCP\",\""+IP_RemoteServer+"\",80\r\n",3000,ESP_Debugging);//Initiate TCP connection to the server
          sendData_ESP("AT+CIPSEND=4," + String(ApplicationLayerPacket.length() + 4) + "\r\n",4000,ESP_Debugging);//Open sender for the HTTP packet
          sendData_ESP(ApplicationLayerPacket,1500,ESP_Debugging);//Send the HTTP packet
          sendData_ESP("AT+CIPCLOSE\r\n",1000,ESP_Debugging);//Close the TCP connection.
          Serial2.println("###");//Send ACK to the remote device that the packet got received.
          Remote_PacketName = "";//Flush the variables for the next packet..
          Remote_PacketData = "";//FLush the variables for the next packet.
                   Serial.println("Im here 3");
      }else{
          //If you have no internet aswell, you assume that the second device has no internet aswell, and you drop the packet. 
          Serial2.flush();//Drop anything else. Or assume there is no blueototh connection.
                   Serial.println("Im here 4");
      }
    }else{
        Serial2.flush();//Drop anything else. Or assume there is no blueototh connection.
                 Serial.println("Im here 5");
    }
}

void GetFileName_FromACK() {
    Bluetooth_Serial_Data.replace("#", "");
    Ackd_FILENAME = Bluetooth_Serial_Data;
    Ackd_FILENAME += ".txt";
}

//This functions extracts packet data. Remote Packet construction:    ...DATA%PACKETNAME     Remote_PacketData
void GetPacketData(){
    Bluetooth_Serial_Data.replace(".", "");
    String temp = Bluetooth_Serial_Data;
    int ind = temp.indexOf("%");
    Remote_PacketData = temp.substring(0,ind);
    int maxLen = temp.length();
    Remote_PacketName = temp.substring((ind+1),maxLen);
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------- REAL TIME SHIELD MODULE ---------------------------------------------------------------------------------------------------

//This function will update the real time, and store them into two string DateMoment, TimeMoment.
void UpdateTime() {
   myRTC.updateTime();//Update time from the real time module.
   DateMoment = String(myRTC.dayofmonth) + "/" + String(myRTC.month) + "/" + String(myRTC.year);
   TimeMoment = String(myRTC.hours) + ":" + String(myRTC.minutes) + ":" + String(myRTC.seconds); 
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------- SD CARD MANAGER ---------------------------------------------------------------------------------------------------------------
void FixText(bool AddTxt) {
  String custom = SD_CurrentFile;
  int indx = custom.indexOf(".");
  String Extracted = custom.substring(0,indx);
  if (AddTxt) {
    Extracted +=".txt";
  }
  SD_CurrentFile = Extracted;
}

String ReadTargetFile(){
  file = sd.open(SD_CurrentFile);
  String output = "";
  if (file) {
    while (file.available()) {
        char c = file.read();
        output +=c;
    }
    // close the file:
  file.close();
  return output;
  }
  return "";
}

void ListingFiles(){   
   if (!dir.open("/")){
      error("dir.open failed");
    } 
   char Name[40];
   String c;
   PastFilesStored = false;
   while (file.openNext(&dir, O_RDONLY)) {
      file.getName(Name, 40);
      for (int i = 0;i<40;i++){
        c+=Name[i];
      }
       for (int i = 0;i<40;i++){
        Name[i]=0x00;
      }
    Serial.println();
    file.close();
    if (c.indexOf("Information")>0){
       c="";
      PastFilesStored = false;
      ;
    }else{
      PastFilesStored = true;
      SD_CurrentFile = c;
      c="";
      break;
    }
  }
 dir.close();

}
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
