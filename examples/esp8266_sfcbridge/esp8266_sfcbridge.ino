#include <ChetchNetworkAPI.h>
#include <ChetchWifiManager.h>
#include <ChetchSFCBridge.h>
#include <ChetchADM.h>

//Normally only need to change these
#define TRACE false
#define HOSTNAME "oblong3" //this identifies the board on the network so needs to be changed on a per board basis
#define PORT 8088 //doesn't need to be changed

//SFC bridge values (normally don't need to change) .. I = internal i.e. on the board, X = external e.g. wifi
#define I_LOCAL_UART_BUFFER 64
#define I_REMOTE_UART_BUFFER 64
#define I_RECEIVE_BUFFER 2*I_LOCAL_UART_BUFFER
#define I_SEND_BUFFER 2*I_LOCAL_UART_BUFFER
#define I_CTS_TIMEOUT 2000
#define X_LOCAL_UART_BUFFER I_LOCAL_UART_BUFFER //should be the same because it's a bridge
#define X_REMOTE_UART_BUFFER 64
#define X_RECEIVE_BUFFER 2*X_LOCAL_UART_BUFFER
#define X_SEND_BUFFER 2*X_LOCAL_UART_BUFFER
#define X_CTS_TIMEOUT 2000

//Network credentials
#define SSID "Bulan Baru Internet"
#define PASSWORD "xxxxxx"
#define NETWORK_SERVICE_URL "http://192.168.1.188:8001/api/service"

using namespace Chetch;

StreamFlowController xStream(X_LOCAL_UART_BUFFER, X_REMOTE_UART_BUFFER, X_RECEIVE_BUFFER, X_SEND_BUFFER);
StreamFlowController iStream(I_LOCAL_UART_BUFFER, I_LOCAL_UART_BUFFER, I_RECEIVE_BUFFER, I_SEND_BUFFER);

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

  iStream.setCTSTimeout(I_CTS_TIMEOUT);
  xStream.setCTSTimeout(X_CTS_TIMEOUT); 
  SFCBridge::init(&iStream, &xStream, ArduinoDeviceManager::getMaxFrameSize());
  SFCBridge::beginIStream(&Serial);
}

void loop() {
  // put your main code here, to run repeatedly:
  WiFiClient client = server.available();
  
  if (client) {
    client.setNoDelay(true);
    SFCBridge::beginXStream(&client);
    while(client.connected()){ 
      
      SFCBridge::loop();
      
      yield();
    } //end client connected loop
  } //end check if client instance created 
}
