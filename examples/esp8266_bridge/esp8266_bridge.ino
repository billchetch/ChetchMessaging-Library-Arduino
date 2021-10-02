#include <ChetchNetworkAPI.h>
#include <ChetchWifiManager.h>

//Normally only need to change these
#define TRACE false
#define HOSTNAME "oblong3" //this identifies the board on the network so needs to be changed on a per board basis
#define PORT 8088 //doesn't need to be changed

//Network credentials
/*
#define SSID "Bulan Baru Internet"
#define PASSWORD "bulanbaru"
#define NETWORK_SERVICE_URL "http://192.168.1.188:8001/api/service"
*/

#define SSID "Bulan Baru"
#define PASSWORD "bbcharters"
#define NETWORK_SERVICE_URL "http://192.168.2.100:8001/api/service"

using namespace Chetch;

WiFiServer server(PORT);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  WifiManager::trace = TRACE;
  WifiManager::connect(HOSTNAME, SSID, PASSWORD);
  
  //now register with network service
  delay(1000);
  NetworkAPI::trace = TRACE;
  NetworkAPI::registerService(NETWORK_SERVICE_URL, HOSTNAME, PORT);
  
  //start server for TCP connections
  server.begin();
  if(TRACE){
    Serial.print("Connect to this device @: ");
    Serial.print(WiFi.localIP());
    Serial.print(":");
    Serial.println(PORT);
  }
  
}

void loop() {
  // put your main code here, to run repeatedly:
  WiFiClient client = server.available();
  
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
