#include <ChetchNetworkAPI.h>
#include <ChetchWifiManager.h>
#include <ChetchStreamFlowController.h>
#include <ChetchADM.h>


//Normally only need to change these
#define TRACE true
#define HOSTNAME "oblong3" //this identifies the board on the network so needs to be changed on a per board basis
#define PORT 8090 //IMPORTANT: this should be unique with the HOSTNAME as well (starting at 8088 and going up) ... this is to prevent confusion if using dynamic dns

//Network credentials
/*
#define SSID "Bulan Baru Internet"
#define PASSWORD "bulanbaru"
#define NETWORK_SERVICE_URL "http://192.168.1.188:8001/api/service"
*/

/*#define SSID "Bulan Baru"
#define PASSWORD "bbcharters"
#define NETWORK_SERVICE_URL "http://192.168.2.100:8001/api/service"*/

#define SSID "Bulan Baru KM"
#define PASSWORD "bbcharters"
#define NETWORK_SERVICE_URL "http://192.168.2.100:8001/api/service"

using namespace Chetch;

WiFiServer server(PORT);
WiFiClient client;
bool serviceRegistered = false;

void onWifiEvent(WifiManager::WifiEvent event){
  switch(event){
    case WifiManager::WifiEvent::CONNECTED:
      break;
      
    case WifiManager::WifiEvent::GOT_IP:
      break;
      
    case WifiManager::WifiEvent::DISCONNECTED:
      sendCommand(&Serial, ArduinoDeviceManager::RESET_ADM_COMMAND);
      
      if(TRACE){
        Serial.println("Disconnected... sending Reset ADM command byte to Arduino");
      }
      break;
  }
}

void sendCommand(Stream* stream, byte cmd){
  stream->write(StreamFlowController::COMMAND_BYTE);
  stream->write(cmd);
}

void sendEvent(Stream* stream, byte evt){
  stream->write(StreamFlowController::EVENT_BYTE);
  stream->write(evt);
}

bool registerService(){
  while(!WifiManager::hasIP()){
    delay(500);
    if(TRACE)Serial.println("Waiting for IP...");
  }
  NetworkAPI::trace = TRACE;
  serviceRegistered = NetworkAPI::registerService(NETWORK_SERVICE_URL, HOSTNAME, PORT);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(100);

  //this will connect to the network (or timeout if provided)
  WifiManager::trace = TRACE;
  WifiManager::connect(HOSTNAME, SSID, PASSWORD, onWifiEvent);
  delay(100);
  
  //now we register service ... firstly it waits for an IP to be available and then we set Network api with an infinte timeout ae 
  //we need the service to register for any client to be able to connect (i.e. this is all useless unless it regisers)
  registerService();
  delay(100);

  //start up server
  server.begin();
  delay(100);
}

void loop() {
  //Ideally we'd respond to a lack of IP (resulting from a disconnect event) in the event handler however that was causing
  //an exception so we check here
  if(!WifiManager::hasIP()){
    //registerService will wait until an IP is available before continuing
    registerService();
  }
  
  // put your main code here, to run repeatedly:
  client = server.available(); 
  if (client) {
    client.setNoDelay(true);
    while(client.connected()){ 
      //read from client (wifi) and forward to serial (arduino)
      while(client.available()){
        Serial.write(client.read());
      }

      yield();
      
      //read from serial (arduino) and forward to client (wifi)
      while(Serial.available()){
        client.write(Serial.read());
      }
      
      yield();
    } //end client connected loop
  } //end check if client instance created 
}
