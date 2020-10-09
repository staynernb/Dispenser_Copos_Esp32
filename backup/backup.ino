// Envio to cel       
//characteristicTX->setValue("UAU");
//characteristicTX->notify();

#include <Wire.h>                //Biblioteca para I2C
#include <ESP32Servo.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "BluetoothSerial.h"
#include <WiFi.h>
//#include <WiFiServer.h>
#include <HTTPClient.h>
#include "ArduinoJson.h"
#include "EEPROM.h"

HTTPClient http;
char json[400] = "{\"ssid\":\"thots\",\"pass\":\"asd123\"}"; 
StaticJsonDocument<200> doc;

// Constantes
#define SERVICE_UUID           "ab0828b1-198e-4351-b779-901fa0e0371e" // UART service UUID
#define CHARACTERISTIC_UUID_RX "4ac8a682-9736-4e5d-932b-e9b31405049c"
#define CHARACTERISTIC_UUID_TX "0972EF8C-7613-4075-AD52-756F33D4DA91"
#define EEPROM_SIZE 128

// Parâmetros ajustáveis 
#define TEMPO_POSICIONAR_MANGUEIRA 2000
//#define TEMPO_BOMBEAMENTO 10000
#define TEMPO_ESPERAR_COPO 5000
#define TEMPO_LIBERAR_COPO 1000

int TEMPO_BOMBEAMENTO = 8000;

// Variáveis
Servo servo_Mangueira;   // Objeto do servo que controla a mangueira
BLECharacteristic *characteristicTX; // Objeto que permite enviar dados para o cliente Bluetooth
BluetoothSerial SerialBT;
//WiFiServer web_server(80);    // Set web server port number to 80


bool flag_dispositivo_conectado = false; // Indica que um dispositivo bluetooth foi conectado
bool flag_comando_inicio = 0;  // Indica que o comando Bluetooth foi recebido e agora está no meio do processo.
bool flag_cancelar_processo = 0; // Indica que o processo deve ser cancelado e voltar para espera de um novo comando.
bool flag_comando_reinicio = 0;  // Indica que o processo estava travado sem copo na bandeja, e foi recebido o comando pelo bluetooth para iniciar.

int angulo_fim_mangueira = 90;     //posição do servo mangueira apontado para o copo em graus
int angulo_inicio_mangueira = 0;   //posição do servo mangueira inicial em graus

unsigned long tempo_Bombeado = 0;
int qtd_copos_repo = 0;
int qtd_litros_repo = 0;
int qtd_finalizados = 0;

//String ssids_array[50];
String client_wifi_ssid;
String client_wifi_password;
// Variable to store the HTTP request
String header;

const int ssid_Addr = 0;     // Endereços da memória rom para ssid e senha da wifi.
const int pass_Addr = 30;

enum wifi_setup_stages {NONE, SCAN_START, SCAN_COMPLETE, SSID_ENTERED, WAIT_PASS, PASS_ENTERED, WAIT_CONNECT, LOGIN_FAILED};
enum wifi_setup_stages wifi_stage = NONE;

// PINS
int pin_Sensor_copo_repositorio = 10;  //(INSERIR PINO de conexão com o sensor de infravermelho do repositorio);
int pin_Sensor_copo_bandeja = 13;  //(INSERIR PINO de conexão com o sensor de infravermelho da bandeja);
int pin_Servo_mangueira = 12;    //(INSERIR PINO de conexão com o servo mangueira);
int pin_Motor_copo = 2;    //(INSERIR PINO de conexão com o drive do motor que solta um copo);
int pin_Motor_copo_gnd = 16;    //(INSERIR PINO de conexão com o drive do motor que solta um copo gnd);
int pin_Motor_liquido = 14;    //(INSERIR PINO de conexão com o drive do motor bomba que despeja o líquido);
int pin_Motor_liquido_gnd = 15;    //(INSERIR PINO de conexão com o drive do motor bomba que despeja o líquido gnd);

//Funções prototipos
void checar_disparo();

//callback para receber os eventos de conexão de dispositivos
class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      flag_dispositivo_conectado = true;
    };

    void onDisconnect(BLEServer* pServer) {
      flag_dispositivo_conectado = false;
    }
};

void wifi_task(){
  String ssid;
  String password;
  
  ssid = read_string_rom(ssid_Addr);
  password = read_string_rom(pass_Addr);
  Serial.println(ssid);
  Serial.println(password);
  
  if(ssid.length() > 0 && password.length() > 0){
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("password: ");
    Serial.println(password);
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.print(" Conectando Wifi ");
    while(WiFi.status() != WL_CONNECTED){
      Serial.print(".");
      delay(300);
    }
    Serial.print("Conectado com o IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();
    // WEB SErver
    //web_server.begin();
  }
  else{
    Serial.println("There is nothing");
  }
  
  
}
class CharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *characteristic) {
      //retorna ponteiro para o registrador contendo o valor atual da caracteristica
      std::string rxValue = characteristic->getValue(); 
      
      //verifica se existe dados (tamanho maior que zero)
      if (rxValue.length() > 0) {
        // Do stuff based on the command received
        if (rxValue.find("L1") != -1) { 
          Serial.print("Liberando copo");
          if (!flag_comando_inicio) {
            flag_comando_inicio = 1;
            Serial.println("flag comando = 1");
          } else {
            Serial.println("flag comando reinicio = 1");
            flag_comando_reinicio = 1;
          }
        }
        if (rxValue.find("config") != -1) { 
           Serial.print("Configuração");
           wifi_stage = SCAN_START;
        }
        if (rxValue.find("wifi") != -1) { 
           Serial.print("Wifi Task");
           wifi_task();
        }
        if( wifi_stage == SCAN_COMPLETE) {
          client_wifi_ssid = rxValue.data();
          Serial.println(client_wifi_ssid);
          write_string_rom(ssid_Addr,client_wifi_ssid);
        }
        if( wifi_stage == SCAN_COMPLETE) {
          client_wifi_ssid = rxValue.data();
          Serial.println(client_wifi_ssid);
          wifi_stage = SSID_ENTERED;
        }
        if (wifi_stage == WAIT_PASS){
          client_wifi_password = rxValue.data();
          Serial.println(rxValue.data());
          write_string_rom(pass_Addr,client_wifi_password);
          wifi_stage = PASS_ENTERED;
        }
      }
    }
};

// SETUP
void setup() {

  Serial.begin(115200);
    
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  servo_Mangueira.setPeriodHertz(50);
  servo_Mangueira.attach(pin_Servo_mangueira, 500, 2400);

  digitalWrite(pin_Motor_liquido_gnd, LOW);
  digitalWrite(pin_Motor_copo_gnd, LOW);
  digitalWrite(pin_Motor_liquido_gnd, LOW);

  pinMode(13,INPUT);
  pinMode(34,INPUT);
  pinMode(35,INPUT);

  /************ BLUETOOTH *****************/
  // Criar o dispositivo BLE
  BLEDevice::init("ESP32-BLE"); // nome do dispositivo bluetooth
  
  // Criar e configurar o servidor BLE
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  
  // Criar o serviço BLE
  BLEService *service = server->createService(SERVICE_UUID);
  
  // Configurar características de transmissão dos dados
  characteristicTX = service->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  characteristicTX->addDescriptor(new BLE2902());

  // Configurar características de recebimento dos dados
  BLECharacteristic *characteristic = service->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  characteristic->setCallbacks(new CharacteristicCallbacks());
  
  // Iniciar serviço BLE
  service->start();
  server->getAdvertising()->start();

  // EEPROM
  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("failed to initialise EEPROM"); delay(1000000);
  }
  wifi_task();
  const size_t capacity = JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(8) + 146;
  DynamicJsonDocument json(capacity);
}

// LOOP
void loop() {
   
   //Serial.println(client_wifi_password);
   
   if(flag_dispositivo_conectado){
     if(wifi_stage == SCAN_START){
          Serial.println("Escaneando wifis");
          wifi_stage = SCAN_COMPLETE;
          Serial.println("Digite sua id");
     }
     if(wifi_stage == SSID_ENTERED){
          Serial.println("Digite sua senha");
          wifi_stage = WAIT_PASS;
     }
     if(wifi_stage == PASS_ENTERED){
          Serial.println("CONECTANDO");
          wifi_stage = NONE;
     }
   }
   else{
   }
   if (flag_comando_inicio == 1){
      if (Verifica_copos_repositorio() == 0){
          Serial.println("Sem COpos");
          flag_comando_inicio = 0;
      } 
      else {
          Liberar_copo_bandeja();
          if (flag_cancelar_processo == 1){
              Serial.println("Informar que houve problemas de liberação e copos");
              flag_comando_inicio = 0;
              flag_cancelar_processo = 0;
          }
          else {
              qtd_copos_repo = qtd_copos_repo - 1;
              Posicionar_mangueira_fim();
              if (Verifica_copos_bandeja() != 1){
                  Posicionar_mangueira_inicio();
                  Aguardar_copo_ou_reinicio();
              }
  
              if (flag_cancelar_processo == 1){
                  Serial.println("Copo sumiu da bandeja");
                  flag_comando_inicio = 0;
                  flag_cancelar_processo = 0;
              }
              else if (flag_comando_reinicio == 1){
                  Serial.println("Começar com novo copo");
                  flag_comando_inicio = 1;
                  flag_comando_reinicio = 0;
              }
              else {
                  Posicionar_mangueira_fim();
                  Bombear_liquido();
                  Posicionar_mangueira_inicio();
                  qtd_finalizados += 1;
                  Serial.println("flag comando no final : ");
                  Serial.print(flag_comando_inicio);
              }
          }
      }
   }  
   else{
    if(WiFi.status() == WL_CONNECTED){
      listen_server();
      delay(200);
    }
    else{
      wifi_task();
    }
   }
}

// FUNÇÔES 

bool Verifica_copos_repositorio(){
  
    Serial.println("Copo verificado no repositorio");
    //return ~digitalRead(pin_Sensor_copo_repositorio);
    return 1;   
}

bool Verifica_copos_bandeja(){
    bool aux;
    Serial.println("Copo verificado na bandeja: ");
    aux =  !digitalRead(pin_Sensor_copo_bandeja);
    Serial.print(aux);
    return aux;
}

void Liberar_copo_bandeja(){
  
    Serial.println("Acionar Motor DC para liberar um copo");
    //digitalWrite(pin_Motor_copo, HIGH);
    //delay(tempo_Motor_ligado_copo);
    //digitalWrite(pin_Motor_copo, LOW);
    delay(TEMPO_LIBERAR_COPO);
    unsigned long tempoInicio = millis();
    
    while(Verifica_copos_bandeja() !=1){
        if( millis() - tempoInicio >= TEMPO_ESPERAR_COPO){
            flag_cancelar_processo = 1;
            break;
        }
    }
}

void Posicionar_mangueira_fim(){

    Serial.println("Posicionando Mangueira");
    servo_Mangueira.write(angulo_fim_mangueira);
    delay(TEMPO_POSICIONAR_MANGUEIRA);
    Serial.println("Mangueira posicionada fim");
}

void Posicionar_mangueira_inicio(){

    Serial.println("Posicionando Mangueira");
    servo_Mangueira.write(angulo_inicio_mangueira);
    delay(TEMPO_POSICIONAR_MANGUEIRA);
    Serial.println("Mangueira posicionada inicio");
}


void Aguardar_copo_ou_reinicio(){
    Serial.println("Aguardando copo");
    unsigned long tempoInicio = millis();
    
    while(Verifica_copos_bandeja() != 1){
        if( (millis() - tempoInicio) >= TEMPO_ESPERAR_COPO){   //Lembrar de colocar a cast
            flag_cancelar_processo = 1;
            break;
        }
        else{
            if( flag_comando_reinicio == 1)  
                break;
        }
    }
}

void Bombear_liquido(){
    Serial.println("Bombeando liquido");
    unsigned long tempoAnterior = millis();
    Serial.print("tempo Inicio: ");
    Serial.println(tempoAnterior);
    bool continuar = 1;
    while( continuar == 1 ){
      
        if( Verifica_copos_bandeja() != 1){
            continuar = 0;
            Posicionar_mangueira_inicio();
            Serial.println("Desligar motor");
            //digitalWrite(pin_Motor_liquido, LOW);
            Aguardar_copo_ou_reinicio();
            
            if(flag_cancelar_processo == 1){
              flag_cancelar_processo = 0;
              flag_comando_inicio = 0;
              break;
            }
            else if(flag_comando_reinicio == 1){
              flag_comando_inicio = 1;
              break;
            }
            else{
              Posicionar_mangueira_fim();
              continuar = 1;
            }
            tempoAnterior = millis();
            
        }
        tempo_Bombeado = tempo_Bombeado + (millis() - tempoAnterior);
        tempoAnterior = millis();
        if( tempo_Bombeado > TEMPO_BOMBEAMENTO){
           continuar = 0;
           Serial.print("continuar : ");
           Serial.println(continuar);
           tempo_Bombeado = 0;
           
        }
        
        //digitalWrite(pin_Motor_liquido, HIGH);
        
    }
    if(flag_comando_reinicio==1){
      flag_comando_reinicio = 0;
      flag_comando_inicio = 1;
    }
    else{
      flag_comando_inicio = 0;
    }
    //digitalWrite(pin_Motor_liquido, LOW);
    Serial.println("Bombeamento Finalizado");
}



void write_string_rom(int add, String data){
  int _size = data.length();
  int i;
  for(i=0 ; i<_size; i++){
    EEPROM.write(add+i,data[i]);
  }
  EEPROM.write(add+_size,'\0');
  EEPROM.commit();
}

String read_string_rom(int add){
  char data[100];
  int len =0;
  unsigned char k;
  k = EEPROM.read(add);
  while( k != '\0'){
    k = EEPROM.read(add+len);
    data[len] = k;
    len++;
  }
  data[len] = '\0';
  Serial.println(String(data));
  return String(data);
}

void result_get_disparo(String msg){
    //memset(json,0,sizeof(json));
    msg.toCharArray(json, 400);
    Serial.println(msg);
    deserializeJson(doc, json);
    JsonObject ticker = doc["ticker"];
      
    long flag = doc["ativar"]; 
    long tempo = doc["tempo"]; 

    if(flag == 1){
      flag_comando_inicio = 1;
      TEMPO_BOMBEAMENTO = tempo*1000;
    }
    
}

void result_get_parametros(String msg){
    //memset(json,0,sizeof(json));
    msg.toCharArray(json, 400);
    Serial.println(msg);
    deserializeJson(doc, json);
    JsonObject ticker = doc["ticker"];
      
    long qtd_copos = doc["qtd_copos"]; 
    long litros = doc["litros"]; 
    long repo = doc["repo"];

    if(repo == 1){
      qtd_copos_repo = qtd_copos;
      qtd_litros_repo = litros;
    }
      
}
void result_get_wifi(String msg){
    //memset(json,0,sizeof(json));
    msg.toCharArray(json, 400);
    Serial.println(msg);
    deserializeJson(doc, json);
    JsonObject ticker = doc["ticker"];
      
    const char* ssid = doc["ssid"]; 
    const char* pass = doc["pass"]; 
    long wifi_change = doc["wifi_change"];

    String ssid_wifi = ssid;
    String pass_wifi = pass;
    
    int i = 0;
    Serial.println(ssid_wifi.c_str());
    if(wifi_change == 1){
      //WiFi.disconnect(true);
      WiFi.begin(ssid_wifi.c_str(), pass_wifi.c_str());
      
      while(WiFi.status() != WL_CONNECTED){
        Serial.print(".");
        delay(300);
        i++;
        if(i >= 8)
          //WiFi.disconnect(true);
          wifi_task();
          break;
      }
      if( i< 8){
        write_string_rom(ssid_Addr,ssid_wifi);
        write_string_rom(pass_Addr,pass_wifi);
      }
    }
}

void post_data_server(){
 
  Serial.println("Posting JSON data to server...");
  // Your Domain name with URL path or IP address with path
  http.begin("https://thoths.cc/api.php?acao=cadastrar_esp");
  // Specify content-type header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  String httpRequestData = "mac=123&qtd_copos="+String(qtd_copos_repo)+"&qtd_litros="+String(qtd_litros_repo)+"&qtd_finalizados="+String(qtd_finalizados);
  Serial.println(httpRequestData);
  // Send HTTP POST request 
  int httpResponseCode = http.POST(httpRequestData);
    
  Serial.println(httpResponseCode);
  String payload = http.getString();
  //Serial.println(httpCode);
  Serial.println(payload);
     
  http.end();
}

void listen_server(){
  //3 - iniciamos a URL alvo, pega a resposta e finaliza a conexão
    http.begin("https://thoths.cc/api.php?acao=checar_disparo");
    int httpCode = http.GET();
    if (httpCode > 0) { //Maior que 0, tem resposta a ser lida
        String payload = http.getString();
        result_get_disparo(payload);
    }
    else {
        Serial.println("Falha na requisição");
    }
    http.end();
    
    http.begin("https://thoths.cc/api.php?acao=obter_wifi");
    int httpCode1 = http.GET();
    if (httpCode1 > 0) { //Maior que 0, tem resposta a ser lida
        String payload2 = http.getString();
        result_get_wifi(payload2); 
    }
    else {
        Serial.println("Falha na requisição");
    }
    http.end();

    http.begin("https://thoths.cc/api.php?acao=obter_paramentros");
    int httpCode2 = http.GET();
    if (httpCode2 > 0) { //Maior que 0, tem resposta a ser lida
        String payload3 = http.getString();
        result_get_parametros(payload3); 
    }
    else {
        Serial.println("Falha na requisição");
    }
    http.end();

    post_data_server();
}
/*void web_server_loop(){
  // Current time
  long currentTime = millis();
  // Previous time
  unsigned long previousTime = 0; 
  // Define timeout time in milliseconds (example: 2000ms = 2s)
  const long timeoutTime = 2000;
  WiFiClient client = web_server.available();   // Listen for incoming clients
  if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the GPIOs on and off
            if (header.indexOf("GET /on") >= 0) {
              Serial.println("GPIO 26 on");
              Serial.print("Liberando copo");
              if (!flag_comando_inicio) {
                flag_comando_inicio = 1;
                Serial.println("flag comando = 1");
              } else {
                Serial.println("flag comando reinicio = 1");
                flag_comando_reinicio = 1;
              }
            } else if (header.indexOf("GET /26/off") >= 0) {
              Serial.println("GPIO 26 off");
              digitalWrite(26, LOW);
            }
          break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}*/
