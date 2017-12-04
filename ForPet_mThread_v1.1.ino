#include <mthread.h>
#include <SPI.h>
#include <Phpoc.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include "hx711.h"

#define servoPin 5
#define hx_DOUT 3
#define hx_CLK 2
#define buzz 8
#define led 9
#define threshold 800

char server_name[] = "116.33.27.63";
char mac[32];
long power;
Hx711 scale(hx_DOUT,hx_CLK);

class httpThread : public Thread{
public:
  httpThread();
  void Motor();
  void Weight();
protected:
  bool loop();
private:
  PhpocClient client;
  PhpocClient client2;
  String msg;
  int weight;
};

httpThread::httpThread(){
  msg = "";
  weight = scale.getGram();
  Motor();
  Weight();
}

void httpThread::Motor(){
  if(client.connect(server_name, 217)){
    Serial.println("Connected to server");
    client.print("GET /motor/");
    client.print(mac);
    client.println(" HTTP/1.1");
    client.println();
  }else{
    Serial.println("connection failed");
  }
}

void httpThread::Weight(){
  if(client2.connect(server_name, 217)){
    Serial.println("Sending data to server");
    client2.print("GET /weight/");
    client2.print(weight);
    client2.print("/");
    client2.print(mac);
    client2.println(" HTTP/1.1");
    client2.println();
  }else{
    Serial.println("connection failed");
  }
}

bool httpThread::loop(){
  if(kill_flag)
    return false;
  int i = 0;
  while(client.available())
  {
    char c = client.read();
    if(i<9){
      if(c=='\n'){
        i++;
      }
    }else{
      if(c=='[' || c==']'){
      }else{
        msg += c;
      }
    }
  }
  while(client2.available()){
    client2.read();
  }

  if(!client.connected() && !client2.connected()){
    Serial.println(msg);
    StaticJsonBuffer<200> buf;
    JsonObject& motor = buf.parseObject(msg);
    power = motor["motor"];
    Serial.println(power);

    client.stop();
    client2.stop();
    msg = "";
    weight = scale.getGram();
    Serial.println(weight);
    sleep(50);
    Motor();
    Weight();
  }
  return true;
}

class servoThread : public Thread{
public:
  servoThread();
protected:
  bool loop();
private:
  Servo servo;
};

servoThread::servoThread(){
  servo.attach(servoPin);
  servo.write(0);
}

bool servoThread::loop(){
  if(kill_flag)
    return false;
  if(servo.read() == 90){
    servo.write(0);
  }
  if(power == 1){
    servo.write(90);
    power = 0;
  }
  sleep(1);
  return true;
}

class volumeThread : public Thread{
public:
  volumeThread();
protected:
  bool loop();
private:
  int volume;
};

volumeThread::volumeThread(){
  
}

bool volumeThread::loop(){
  if(kill_flag)
    return false;

  volume = analogRead(A0);
  //Serial.println(volume);

  digitalWrite(led,LOW);
  if(volume >= threshold){
    digitalWrite(led,HIGH);
    tone(buzz,volume-800,100);
  }
  sleep_milli(500);
  return true;
}

//Phpoc_쉴드 MAC Address 확인 메소드
int get_mac_address(char *str, int buf_size){
  if(Phpoc.command(F("net1 get hwaddr"))<0){
    return 0;
  }else{
    if(Phpoc.read((uint8_t*)str,buf_size)>0){
      return 6;
    }else{
      return 0;
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  while(!Serial)
    ;
  Phpoc.begin(PF_LOG_SPI | PF_LOG_NET);
  get_mac_address(mac,32);
  Serial.print("Phpoc macAddress: ");
  Serial.println(mac);
  pinMode(buzz,OUTPUT);
  pinMode(led,OUTPUT);
  
  main_thread_list->add_thread(new volumeThread());
  main_thread_list->add_thread(new httpThread());
  main_thread_list->add_thread(new servoThread());
}
