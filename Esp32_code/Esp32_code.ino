#include <ESP32Servo.h>

// Variáveis
Servo servo_Mangueira;   // Objeto do servo que controla a mangueira

bool flag_comando_inicio = 0;  // Indica que o comando Bluetooth foi recebido e agora está no meio do processo.
bool flag_cancelar_processo = 0; // Indica que o processo deve ser cancelado e voltar para espera de um novo comando.
bool flag_comando_reinicio = 0;  // Indica que o processo estava travado sem copo na bandeja, e foi recebido o comando pelo bluetooth para iniciar.

int angulo_fim_mangueira;     //posição do servo mangueira apontado para o copo em graus
int angulo_inicio_mangueira;   //posição do servo mangueira inicial em graus

unsigned long tempo_Bombeado;

// PINS
int pin_Sensor_copo_repositorio = 1;  //(INSERIR PINO de conexão com o sensor de infravermelho do repositorio);
int pin_Sensor_copo_bandeja = 1;  //(INSERIR PINO de conexão com o sensor de infravermelho da bandeja);
int pin_Servo_mangueira = 1;    //(INSERIR PINO de conexão com o servo mangueira);
int pin_Motor_copo = 1;    //(INSERIR PINO de conexão com o drive do motor que solta um copo);
int pin_Motor_copo_gnd = 12;    //(INSERIR PINO de conexão com o drive do motor que solta um copo gnd);
int pin_Motor_liquido = 14;    //(INSERIR PINO de conexão com o drive do motor bomba que despeja o líquido);
int pin_Motor_liquido_gnd = 15;    //(INSERIR PINO de conexão com o drive do motor bomba que despeja o líquido gnd);

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
}


// LOOP
void loop() {
   
   flag_comando_inicio = digitalRead(13);
   if(flag_comando_inicio == 1){
      
      if(Verifica_copos_repositorio() == 0){
          Serial.println("Sem COpos");
          flag_comando_inicio = 0;
      }
      
      else{
          Liberar_copo_bandeja();
          
          if(flag_cancelar_processo == 1){
              Serial.println("Informar que houve problemas de liberação e copos");
              flag_comando_inicio = 0;
              flag_cancelar_processo = 0;
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
                  flag_cancelar_processo = 0;
              }
              else if(flag_comando_reinicio == 1){
                  Serial.println("Começar com novo copo");
                  flag_comando_inicio = 1;
                  flag_comando_reinicio = 0;
              }
              else {
                  Bombear_liquido();
              }                  
          }
      }      
   }

   
}

// FUNÇÔES 

bool Verifica_copos_repositorio(){
  
    Serial.println("Copo verificado no repositorio");
    //return ~digitalRead(pin_Sensor_copo_repositorio);
    return digitalRead(34);   
}

bool Verifica_copos_bandeja(){
  
    Serial.println("Copo verificado na bandeja");
    //return ~digitalRead(pin_Sensor_copo_bandeja);
    return digitalRead(35);   
}

void Liberar_copo_bandeja(){
  
    Serial.println("Acionar Motor DC para liberar um copo");
    //digitalWrite(pin_Motor_copo, HIGH);
    //delay(tempo_Motor_ligado_copo);
    //digitalWrite(pin_Motor_copo, LOW);
    delay(10000);
    unsigned long tempoInicio = millis();
    
    while(Verifica_copos_bandeja() !=1){
        if( millis() - tempoInicio >= 2000){
            flag_cancelar_processo = 1;
            break;
        }
    }
}

void Posicionar_mangueira_fim(){

    Serial.println("Posicionando Mangueira");
    //servo_Mangueira.write(angulo_fim_mangueira);
    delay(10000);
    Serial.println("Mangueira posicionada fim");
}

void Posicionar_mangueira_inicio(){

    Serial.println("Posicionando Mangueira");
    //servo_Mangueira.write(angulo_inicio_mangueira);
    delay(10000);
    Serial.println("Mangueira posicionada inicio");
}


void Aguardar_copo_ou_reinicio(){
    Serial.println("Aguardando copo");
    unsigned long tempoInicio = millis();
    
    while(Verifica_copos_bandeja() != 1){
        if( (millis() - tempoInicio) >= 30000){   //Lembrar de colocar a cast
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
    unsigned long tempoInicio = millis();
    bool continuar = 1;
    while( continuar == 1 ){
      
        if( Verifica_copos_bandeja() != 1){
            continuar = 0;
            Posicionar_mangueira_inicio();
            Aguardar_copo_ou_reinicio();
            
            if(flag_cancelar_processo == 1){
              flag_cancelar_processo = 0;
              flag_comando_inicio = 0;
              break;
            }
            else if(flag_comando_reinicio == 1){
              flag_comando_inicio = 1;
              flag_comando_reinicio = 0;
              break;
            }
            else{
              continuar = 1;
            }
            
        }
        tempo_Bombeado = tempo_Bombeado + (millis() - tempoInicio);
        
        if( tempo_Bombeado > 10000){
           continuar = 0;
           tempo_Bombeado = 0;
           
        }

        //digitalWrite(pin_Motor_liquido, HIGH);
        
    }
    //digitalWrite(pin_Motor_liquido, LOW);
    Serial.println("Bombeamento Finalizado");
}
