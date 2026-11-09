// Copia de main.cpp adaptada para o simulador Wokwi (compile com: pio run -e wokwi)
#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Servo.h>
//servos
#define direito 15
#define esquerdo 14
//sensores de proximidade
//direito
#define trigdir 2
#define echodir 4
//esquerdo
#define trigesq 13
#define eshoesq 12
//frente
#define trifront 32
#define echofront 21
//atras
#define trigback 27
#define echoback 16

//wifi: versao para o simulador Wokwi, usa a rede virtual "Wokwi-GUEST" (sem senha)
//nao precisa do include/rede.h
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_SENHA ""
#define WIFI_CANAL 6         //canal fixo da rede do Wokwi, conecta mais rapido
#define WIFI_TIMEOUT 15000   //tempo maximo tentando conectar (ms)
#define WIFI_RECONEXAO 10000 //intervalo entre tentativas de reconexao (ms)

//servos de rotacao continua: 90 = parado, 0 e 180 = velocidade maxima em sentidos opostos
//como os servos ficam espelhados no chassi, um gira ao contrario do outro para andar reto
#define PARADO 90
#define VELOCIDADE 90 //0 a 90

//todo movimento para sozinho depois de um tempo
#define DURACAO_PADRAO 1000 //tempo de cada movimento quando nao e informado (ms)
#define DURACAO_MAXIMA 5000 //nenhum movimento passa deste tempo (ms)

Servo servoDireito;
Servo servoEsquerdo;

unsigned long ultimaTentativaWifi = 0;
bool wifiConectado = false;
bool emMovimento = false;
unsigned long inicioMovimento = 0;
unsigned long duracaoMovimento = 0;

void conectarWifi() {
  Serial.print("Conectando ao WiFi ");
  Serial.print(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_SENHA, WIFI_CANAL);

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < WIFI_TIMEOUT) {
    delay(250);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" falhou, tentando de novo mais tarde");
  }
  wifiConectado = WiFi.status() == WL_CONNECTED;
  ultimaTentativaWifi = millis();
}

//reconecta sem travar o loop, para nao atrasar a parada dos movimentos
void verificarWifi() {
  bool conectado = WiFi.status() == WL_CONNECTED;
  if (conectado && !wifiConectado) {
    Serial.print("WiFi conectado, IP: ");
    Serial.println(WiFi.localIP());
  } else if (!conectado && wifiConectado) {
    Serial.println("WiFi desconectado");
  }
  wifiConectado = conectado;

  if (!conectado && millis() - ultimaTentativaWifi > WIFI_RECONEXAO) {
    Serial.println("Tentando reconectar ao WiFi...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_SENHA, WIFI_CANAL);
    ultimaTentativaWifi = millis();
  }
}

void parar() {
  servoDireito.write(PARADO);
  servoEsquerdo.write(PARADO);
  emMovimento = false;
}

//liga os servos e marca quando o movimento deve acabar (no maximo DURACAO_MAXIMA)
void mover(int velDireito, int velEsquerdo, unsigned long duracao) {
  if (duracao == 0) return;
  servoDireito.write(PARADO + velDireito);
  servoEsquerdo.write(PARADO + velEsquerdo);
  inicioMovimento = millis();
  duracaoMovimento = min(duracao, (unsigned long)DURACAO_MAXIMA);
  emMovimento = true;
}

//chamada no loop: para os servos quando o tempo do movimento acaba
void verificarMovimento() {
  if (emMovimento && millis() - inicioMovimento >= duracaoMovimento) {
    parar();
    Serial.println("movimento terminou");
  }
}

void frente(unsigned long duracao = DURACAO_PADRAO) {
  mover(-VELOCIDADE, VELOCIDADE, duracao);
}

void atras(unsigned long duracao = DURACAO_PADRAO) {
  mover(VELOCIDADE, -VELOCIDADE, duracao);
}

//gira no proprio eixo: as duas rodas no mesmo sentido de rotacao
void esquerda(unsigned long duracao = DURACAO_PADRAO) {
  mover(-VELOCIDADE, -VELOCIDADE, duracao);
}

void direita(unsigned long duracao = DURACAO_PADRAO) {
  mover(VELOCIDADE, VELOCIDADE, duracao);
}

void setup() {
  Serial.begin(115200);

  servoDireito.setPeriodHertz(50);
  servoEsquerdo.setPeriodHertz(50);
  servoDireito.attach(direito, 500, 2400);
  servoEsquerdo.attach(esquerdo, 500, 2400);
  parar();

  pinMode(trigdir, OUTPUT);
  pinMode(echodir, INPUT);
  pinMode(trigesq, OUTPUT);
  pinMode(eshoesq, INPUT);
  pinMode(trifront, OUTPUT);
  pinMode(echofront, INPUT);
  pinMode(trigback, OUTPUT);
  pinMode(echoback, INPUT);

  conectarWifi();

  Serial.setTimeout(50);
  Serial.println("Comandos pelo monitor serial: w = frente, s = atras, a = esquerda, d = direita, x = parar");
  Serial.println("Tempo opcional em ms depois da letra, ex: w2000 (maximo 5000)");
}

void loop() {
  verificarWifi();
  verificarMovimento();

  if (Serial.available()) {
    char comando = Serial.read();
    unsigned long duracao = DURACAO_PADRAO;
    if (isDigit(Serial.peek())) {
      duracao = Serial.parseInt();
    }
    switch (comando) {
      case 'w': frente(duracao);   Serial.println("frente");   break;
      case 's': atras(duracao);    Serial.println("atras");    break;
      case 'a': esquerda(duracao); Serial.println("esquerda"); break;
      case 'd': direita(duracao);  Serial.println("direita");  break;
      case 'x': parar();           Serial.println("parado");   break;
    }
  }
}
