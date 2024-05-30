#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <AT24Cxx.h>
#include "RTClib.h"

#define i2c_address 0x50

//inicializa estrutura de gravação
AT24Cxx eep(i2c_address, 32);
int address = 0;
// byte { ano, mes, dia, hora, min, seg, temp, umid, press, alt, bat, d1, d2, d3, d4, d5}
byte logData[16] = { };

// inicia estrutura de tempo (t)
RTC_DS1307 rtc;
#define PressaoaNivelDoMar_HPA 1013.25
const static char *DiasdaSemana[] = { "Domingo", "Segunda", "Terca", "Quarta", "Quinta", "Sexta", "Sabado" };
const char DEGREE_SYMBOL[] = { 0xB0, '\0' };
char str[7];
DateTime now;

//Inicializa sensor THPA
Adafruit_BME280 bme;

//Inicializa Display
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

//Inicializa medidor da bateria
const int pinBateria = A0;
const float maxBateria = 5.0;  //teste com bateria de 9v
const float minBateria = 4.5;  //teste com bateria de 9v

// declara variáveis de atualização
unsigned long tanterior_RTC;
unsigned long tanterior_Sensores;
unsigned long tanterior_log;

void setup(void) {
  Serial.begin(9600);
  //Display
  u8g2.begin();
  //Sensor BME280
  bme.begin(0x76);
  //RTC
  if (!rtc.begin()) {                         // SE O RTC NÃO FOR INICIALIZADO, FAZ
    Serial.println("DS1307 não encontrado");  //IMPRIME O TEXTO NO MONITOR SERIAL
    while (1)
      ;  //SEMPRE ENTRE NO LOOP
  }
  if (!rtc.isrunning()) {               //SE RTC NÃO ESTIVER SENDO EXECUTADO, FAZ
    Serial.println("DS1307 rodando!");  //IMPRIME O TEXTO NO MONITOR SERIAL
    //REMOVA O COMENTÁRIO DE UMA DAS LINHAS ABAIXO PARA INSERIR AS INFORMAÇÕES ATUALIZADAS EM SEU RTC
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  //CAPTURA A DATA E HORA EM QUE O SKETCH É COMPILADO
    //rtc.adjust(DateTime(2018, 7, 5, 15, 33, 15)); //(ANO), (MÊS), (DIA), (HORA),
  }
  if (rtc.readnvram(40) == 77){
    address = rtc.readnvram(40);
  }

  //Serial.println(rtc.readnvram(40));
  atualiza_Sensores();
  atualiza_RTC();
  salvaLog();
}

void loop(void) {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'r') {
      Serial.println("lendo: ");
      lista_Gravacao();
    } else if (c == 'w') {
      Serial.println("gravando: ");
      salvaLog();
    }
  }
  u8g2.firstPage();
  do {
    u8g2.clearBuffer();
    char buf[8];
    u8g2.drawLine(0, 12, 128, 12);
    u8g2.setFont(u8g2_font_spleen6x12_mf);
    u8g2.drawStr(2, 8, DiasdaSemana[now.dayOfTheWeek()]);
    sprintf(buf, "%02d", get_bateria(logData[10]));
    u8g2.drawStr(46, 8, buf);  //bateria
    u8g2.drawStr(64, 8, "%");
    sprintf(buf, "%02d/%02d/%d", logData[2], logData[1], logData[0]);
    u8g2.drawStr(80, 8, buf);
    u8g2.drawFrame(2, 52, 80, 8);
    u8g2.setFont(u8g2_font_spleen16x32_mn);
    //imprime a hora
    sprintf(buf, "%02d", logData[3]);
    u8g2.drawStr(2, 42, buf);
    //imprime a cada segundo o :
    if (logData[5] % 2 == 0) {
      u8g2.drawStr(32, 40, ":");
    }
    //minutos
    sprintf(buf, "%02d", logData[4]);
    u8g2.drawStr(46, 42, buf);
    //imprime barra de progresso dos segundos
    u8g2.drawBox(2, 52, map(logData[5], 0, 60, 1, 80), 8);
    //barra divisória
    u8g2.drawVLine(84, 16, 48);
    //temperatura
    u8g2.setFont(u8g2_font_spleen6x12_mf);
    u8g2.drawStr(90, 24, dtostrf(get_temperatura(logData[6]), 4, 1, str));
    u8g2.drawUTF8(116, 24, DEGREE_SYMBOL);
    u8g2.drawStr(122, 24, "C");
    //umidade
    u8g2.drawStr(96, 38, dtostrf(get_umidade(logData[7]), 3, 0, str));
    u8g2.drawStr(122, 38, "%");
    //pressao
    u8g2.drawStr(90, 50, dtostrf(get_pressao(logData[8]), 4, 0, str));
    u8g2.drawStr(122, 50, "h");
    //altitude
    u8g2.drawStr(90, 64, dtostrf(get_altit(logData[9]), 4, 0, str));
    u8g2.drawStr(122, 64, "m");
    u8g2.sendBuffer();
  } while (u8g2.nextPage());

  // pegar valor de millis() atual
  unsigned long atual = millis();
  // Atualiza RTC - 1s
  if (atual - tanterior_RTC > 1000) {
    atualiza_RTC();
/*    
    Serial.print("Entrei na atualização do tempo...");
    Serial.print("Data: ");                          //IMPRIME O TEXTO NO MONITOR SERIAL
    Serial.print(logData[2], DEC);                   //IMPRIME NO MONITOR SERIAL O DIA
    Serial.print('/');                               //IMPRIME O CARACTERE NO MONITOR SERIAL
    Serial.print(logData[1], DEC);                   //IMPRIME NO MONITOR SERIAL O MÊS
    Serial.print('/');                               //IMPRIME O CARACTERE NO MONITOR SERIAL
    Serial.print(logData[0], DEC);                   //IMPRIME NO MONITOR SERIAL O ANO
    Serial.print(" / Dia: ");                        //IMPRIME O TEXTO NA SERIAL
    Serial.print(DiasdaSemana[now.dayOfTheWeek()]);  //IMPRIME NO MONITOR SERIAL O DIA
    Serial.print(" / Horas: ");                      //IMPRIME O TEXTO NA SERIAL
    Serial.print(logData[3], DEC);                   //IMPRIME NO MONITOR SERIAL A HORA
    Serial.print(':');                               //IMPRIME O CARACTERE NO MONITOR SERIAL
    Serial.print(logData[4], DEC);                   //IMPRIME NO MONITOR SERIAL OS MINUTOS
    Serial.print(':');                               //IMPRIME O CARACTERE NO MONITOR SERIAL
    Serial.print(logData[5], DEC);                   //IMPRIME NO MONITOR SERIAL OS SEGUNDOS
    Serial.println();                                //QUEBRA DE LINHA NA SERIAL
*/    
    tanterior_RTC = atual;
  }
  // Atualiza SENSORES - 1min
  if (atual - tanterior_Sensores > 60000) {
    atualiza_Sensores();
/*    
    Serial.println("Entrei na atualização dos sensores...");
    Serial.print(get_umidade(logData[7]));
    Serial.println(F(" %"));
    Serial.print(get_temperatura(logData[6]));
    Serial.println(F(" *C"));
    Serial.print(get_pressao(logData[8]));
    Serial.println(F(" hPa"));
    Serial.print(get_altit(logData[9]));
    Serial.println(F(" m"));
    Serial.print(get_bateria(logData[10]));
    Serial.println(F(" bat"));
*/    
    tanterior_Sensores = atual;
  }
  // Atualiza LOG - 1h
  if (atual - tanterior_log > 3600000) {
    //Serial.print("Entrei na atualização do log...Endereço: ");
    salvaLog();
    //Serial.println(address);
    tanterior_log = atual;
  }
  /*
  Serial.print("Data: ");                          //IMPRIME O TEXTO NO MONITOR SERIAL
  Serial.print(logData[2], DEC);                    //IMPRIME NO MONITOR SERIAL O DIA
  Serial.print('/');                               //IMPRIME O CARACTERE NO MONITOR SERIAL
  Serial.print(logData[1], DEC);                  //IMPRIME NO MONITOR SERIAL O MÊS
  Serial.print('/');                               //IMPRIME O CARACTERE NO MONITOR SERIAL
  Serial.print(logData[0], DEC);                   //IMPRIME NO MONITOR SERIAL O ANO
  Serial.print(" / Dia: ");                        //IMPRIME O TEXTO NA SERIAL
  Serial.print(DiasdaSemana[now.dayOfTheWeek()]);  //IMPRIME NO MONITOR SERIAL O DIA
  Serial.print(" / Horas: ");                      //IMPRIME O TEXTO NA SERIAL
  Serial.print(logData[3], DEC);                   //IMPRIME NO MONITOR SERIAL A HORA
  Serial.print(':');                               //IMPRIME O CARACTERE NO MONITOR SERIAL
  Serial.print(logData[4], DEC);                 //IMPRIME NO MONITOR SERIAL OS MINUTOS
  Serial.print(':');                               //IMPRIME O CARACTERE NO MONITOR SERIAL
  Serial.print(logData[5], DEC);                 //IMPRIME NO MONITOR SERIAL OS SEGUNDOS
  Serial.println();                                //QUEBRA DE LINHA NA SERIAL
  Serial.print(get_umidade(logData[7]));
  Serial.println(F(" %"));
  Serial.print(get_temperatura(logData[6]));
  Serial.println(F(" *C"));
  Serial.print(get_pressao(logData[8]));
  Serial.println(F(" hPa"));
  Serial.print(get_altit(logData[9]));
  Serial.println(F(" m"));
  Serial.print(get_bateria(logData[10]));
  Serial.println(F(" bat"));
*/
}

//Função que lista todas as gravações, desde o endereço 0
//Copy/paste para excel
void lista_Gravacao() {
  for (int a = 0; a < 64; a += 16) {
    Serial.print(a);
    //Serial.print(";");
    for (byte i = 0; i < sizeof(logData); i++) {
      Serial.print(";");
      Serial.print(eep.read(a + i), DEC);
    }
    Serial.println();
  }
}

//função de gravação do log (16 bytes de logData)
void salvaLog() {
  //Serial.println(sizeof(logData));
  for (byte i = 0; i < sizeof(logData); i++) {
    eep.write(address, logData[i]);
    //Serial.print(data_address);
    address++;
    if (address == eep.length()) {
      address = 0;
    }
  }
  rtc.writenvram(40, address);
  rtc.writenvram(46, 77);
}

void atualiza_RTC(void) {
  now = rtc.now();  //CHAMADA DE ATUALIZACAO
  logData[0] = now.year() - 2000;
  logData[1] = now.month();
  logData[2] = now.day();
  logData[3] = now.hour();
  logData[4] = now.minute();
  logData[5] = now.second();
}

void atualiza_Sensores(void) {
  logData[6] = set_temperatura();
  logData[7] = set_umidade();
  logData[8] = set_pressao();
  logData[9] = set_altit(PressaoaNivelDoMar_HPA);
  logData[10] = set_bateria();
}


byte set_temperatura(void) {
  //reduzindo de 0 a 255 (1 byte)
  return byte(bme.readTemperature()*5);
}

float get_temperatura(byte temp) {
  return (temp / 5);
}

byte set_umidade(void) {
  //umidade do sensor pelo datasheet 0 a 100%
  float umid = bme.readHumidity();
  //reduzindo de 0 a 255 (1 byte)
  return map(umid, 0.0, 100.0, 0, 255);
}

long get_umidade(byte umid) {
  //faz a tansformação de byte em float
  //existe uma perda na acurácia de
  return map(umid, 0, 255, 0.0, 100.0);
}

byte set_pressao(void) {
  //umidade do sensor pelo datasheet 0 a 100%
  float press = bme.readPressure() / 100.0F;
  //reduzindo de 0 a 255 (1 byte)
  return map(press, 300.0, 1100.0, 0, 255);
}

long get_pressao(byte press) {
  //faz a tansformação de byte em float
  //existe uma perda na acurácia de
  return map(press, 0, 255, 300, 1100);
}

byte set_altit(long HPA_NIVEL_MAR) {
  //umidade do sensor pelo datasheet 0 a 100%
  float altit = bme.readAltitude(HPA_NIVEL_MAR);
  //reduzindo de 0 a 255 (1 byte)
  return map(altit, 6000, 0, 255, 0);
}

long get_altit(byte altit) {
  //faz a tansformação de byte em float
  //existe uma perda na acurácia de
  return map(altit, 255, 0, 6000, 0);
}

byte set_bateria(void) {
  //bateria pela ponte no pinBateria
  //Serial.println(bateria);
  //reduzindo de 0 a 255 (1 byte)
  return map(analogRead(pinBateria), 0, 1023, 0, 255);
}

long get_bateria(byte bateria) {
  //faz a tansformação de byte em float, percentual, de acordo com parametros max e min
  int medBateria = map(bateria, 0, 255, 0, 1023) * (0.0049);  //analog pin resolução voltagem = 5/1024
  return (1 - ((maxBateria - medBateria) / (maxBateria - minBateria))) * 100;
}

