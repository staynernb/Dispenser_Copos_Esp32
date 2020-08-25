#include <ESP32Servo.h>

// Variáveis
Servo servo_Mangueira;   // Objeto do servo que controla a mangueira
bool flag_comando_inicio = 0;  // Indica que o comando Bluetooth foi recebido e agora está no meio do processo.
bool flag_cancelar_processo = 0; // Indica que o processo deve ser cancelado e voltar para espera de um novo comando.
bool flag_comando_reinicio = 0;  // Indica que o processo estava travado sem copo na bandeja, e foi recebido o comando pelo bluetooth para iniciar.


// PINS
int pin_Sensor_copo_repositorio = 1;  //(INSERIR PINO de conexão com o sensor de infravermelho do repositorio);
int pin_Sensor_copo_bandeja = 1;  //(INSERIR PINO de conexão com o sensor de infravermelho da bandeja);


// SETUP
void setup() {
  Serial.begin(9600);
}


// LOOP
void loop() {

   if(flag_comando_inicio == 1){
      
      if(Verifica_copos_repositorio() == 0){
          Serial.println("Sem COpos");
      }
      
      else{
          Liberar_copo_bandeja();
          
          if(flag_cancelar_processo == 1){
              Serial.println("Informar que houve problemas de liberação e copos");
              flag_comando_inicio = 0;
          }
          
          else{
              Posicionar_mangueira_fim();
              if(Verifica_copos_bandeja() != 1){
                
                  Posicionar_mangueira_inicio();

                  Aguardar_copo_ou_reinicio();
                  
              }
              if(flag_cancelar_processo == 1){
                  Serial.println("Copo sumiu da bandeja");
                  flag_comando_inicio = 0;
              }
              else if(flag_comando_reinicio == 1){
                  Serial.println("Começar com novo copo");
                  flag_comando_inicio = 1;
              }
              else {
                  if(Verifica_copos_bandeja() != 1){
                      flag_comando_inicio = 0;
                  }
                  else{
                      Bombear_liquido();
                  }
              }
                  
          }
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
  
    Serial.println("Copo verificado na bandeja");
    //return ~digitalRead(pin_Sensor_copo_bandeja);
    return 1;   
}

void Liberar_copo_bandeja(){
  
    Serial.println("Acionar Motor DC para liberar um copo");
    int tempoInicio = millis();
    
    while(Verifica_copos_bandeja() !=1){
        if( millis() - tempoInicio >= 5000){
            flag_cancelar_processo = 1;
            break;
        }
    }
}

void Posicionar_mangueira_fim(){

    Serial.println("Posicionando Mangueira");
    delay(1000);
    Serial.println("Mangueira posicionada fim");
}

void Posicionar_mangueira_inicio(){

    Serial.println("Posicionando Mangueira");
    delay(1000);
    Serial.println("Mangueira posicionada inicio");
}


void Aguardar_copo_ou_reinicio(){
    Serial.println("Aguardando copo");
    int tempoInicio = millis();
    
    while(Verifica_copos_bandeja() != 1){
        if( millis() - tempoInicio >= 30000){
            flag_cancelar_processo = 1;
            break;
        }
        else{
            if( flag_comando_reinicio == 1)  // Lembrar de codificar essa parte na interrupção
                break;
        }
    }
}


void Bombear_liquido(){
    Serial.println("Bombeando liquido");
    int tempoInicio = millis();
    while(Verifica_copos_bandeja() == 1 && (millis() - tempoInicio < 100)){
        //Ligar motor
    }
    Serial.println("Bombeamento Finalizado");
}
