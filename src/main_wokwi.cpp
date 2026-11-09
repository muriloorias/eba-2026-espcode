// Copia de main.cpp adaptada para o simulador Wokwi (compile com: pio run -e wokwi)
#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
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

//API de IA (repo eba-2026-api): POST /ai/action com {"situation": "..."}
//responde {"action": "...", "why": "...", "requiresPosConfirm": true/false}
//troque pela URL do seu deploy (Render) ou pelo IP do PC rodando a API (ex: "http://192.168.0.10:8080/ai/action")
#define API_URL "https://eba-2026-api.onrender.com/ai/action"
#define API_TIMEOUT 60000      //o Render gratuito pode demorar ate ~50s para acordar (ms)
#define INTERVALO_DECISAO 3000 //tempo minimo entre duas perguntas para a API (ms)

//sensores de proximidade (HC-SR04)
#define DISTANCIA_MAXIMA 400 //alcance do HC-SR04 (cm); sem eco = nada ate essa distancia
#define DISTANCIA_SEGURA 20  //abaixo disso (cm) o robo nao anda na direcao do obstaculo
#define TIMEOUT_ECO 25000    //tempo maximo esperando o eco (us), ~4m ida e volta

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
bool modoAutomatico = true; //true = a API decide os movimentos; "m" no serial liga/desliga
unsigned long ultimaDecisao = 0;
String ultimaAcao = "parar";
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

//mede a distancia em cm de um HC-SR04; sem eco devolve DISTANCIA_MAXIMA (caminho livre)
long medirDistancia(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  unsigned long duracao = pulseIn(echo, HIGH, TIMEOUT_ECO);
  if (duracao == 0) return DISTANCIA_MAXIMA;
  long cm = duracao / 58; //velocidade do som: ~58us por cm (ida e volta)
  return min(cm, (long)DISTANCIA_MAXIMA);
}

struct Distancias {
  long frente;
  long atras;
  long esquerda;
  long direita;
};

Distancias lerSensores() {
  Distancias d;
  d.frente = medirDistancia(trifront, echofront);
  d.atras = medirDistancia(trigback, echoback);
  d.esquerda = medirDistancia(trigesq, eshoesq);
  d.direita = medirDistancia(trigdir, echodir);
  return d;
}

//texto enviado para a IA no campo "situation"
String descreverSituacao(const Distancias &d) {
  String s = "Leitura dos sensores de proximidade do robo (distancia ate o obstaculo mais proximo, em cm; ";
  s += String(DISTANCIA_MAXIMA) + " = livre): ";
  s += "frente=" + String(d.frente);
  s += ", atras=" + String(d.atras);
  s += ", esquerda=" + String(d.esquerda);
  s += ", direita=" + String(d.direita);
  s += ". Abaixo de " + String(DISTANCIA_SEGURA) + " cm o robo bate se andar naquela direcao. ";
  s += "Ultima acao executada: " + ultimaAcao + ". ";
  s += "Escolha o proximo movimento para explorar o ambiente sem bater.";
  return s;
}

//faz o POST na API e devolve a acao escolhida (vazio se der erro)
String pedirAcao(const Distancias &d) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("API: sem WiFi, nao da para perguntar");
    return "";
  }

  JsonDocument corpo;
  corpo["situation"] = descreverSituacao(d);
  String json;
  serializeJson(corpo, json);

  //https precisa do cliente seguro; setInsecure pula a verificacao do certificado (suficiente para o projeto)
  WiFiClientSecure clienteSeguro;
  WiFiClient cliente;
  HTTPClient http;
  bool https = String(API_URL).startsWith("https");
  if (https) {
    clienteSeguro.setInsecure();
    http.begin(clienteSeguro, API_URL);
  } else {
    http.begin(cliente, API_URL);
  }
  http.setTimeout(API_TIMEOUT);
  http.addHeader("Content-Type", "application/json");

  Serial.print("API: enviando ");
  Serial.println(json);
  int status = http.POST(json);
  String resposta = http.getString();
  http.end();

  if (status != 200) {
    Serial.print("API: erro ");
    Serial.print(status);
    Serial.print(" ");
    Serial.println(status < 0 ? HTTPClient::errorToString(status) : resposta);
    return "";
  }

  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, resposta);
  if (erro) {
    Serial.print("API: resposta invalida (");
    Serial.print(erro.c_str());
    Serial.print("): ");
    Serial.println(resposta);
    return "";
  }

  String acao = doc["action"] | "";
  Serial.print("API: acao = ");
  Serial.print(acao);
  Serial.print(" | motivo: ");
  Serial.println(doc["why"] | "");
  return acao;
}

//executa a acao da API; recusa andar contra um obstaculo mesmo se a IA mandar
void executarAcao(String acao, const Distancias &d) {
  if (acao == "manter/continuar_como_esta") {
    acao = ultimaAcao; //repete o ultimo movimento
  }

  if (acao == "andar") {
    if (d.frente < DISTANCIA_SEGURA) {
      Serial.println("obstaculo na frente, nao vou andar");
      acao = "parar";
    } else {
      frente();
    }
  } else if (acao == "recuar") {
    if (d.atras < DISTANCIA_SEGURA) {
      Serial.println("obstaculo atras, nao vou recuar");
      acao = "parar";
    } else {
      atras();
    }
  } else if (acao == "virar_esquerda") {
    esquerda();
  } else if (acao == "virar_direita") {
    direita();
  } else if (acao == "parar") {
    parar();
  } else {
    Serial.print("acao desconhecida: ");
    Serial.println(acao);
    acao = "parar";
  }

  if (acao == "parar") parar();
  ultimaAcao = acao;
}

//modo automatico: quando o robo esta parado, le os sensores, pergunta para a API e executa
void decidirPelaApi() {
  if (!modoAutomatico || emMovimento) return;
  if (millis() - ultimaDecisao < INTERVALO_DECISAO) return;

  Distancias d = lerSensores();
  Serial.printf("sensores (cm): frente=%ld atras=%ld esquerda=%ld direita=%ld\n",
                d.frente, d.atras, d.esquerda, d.direita);

  String acao = pedirAcao(d);
  if (acao.length() > 0) {
    executarAcao(acao, d);
  }
  //conta o intervalo a partir do fim da resposta, que pode demorar
  ultimaDecisao = millis();
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
  Serial.println("m = liga/desliga o modo automatico (a API decide os movimentos pelos sensores)");
}

void loop() {
  verificarWifi();
  verificarMovimento();
  decidirPelaApi();

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
      case 'm':
        modoAutomatico = !modoAutomatico;
        parar();
        Serial.println(modoAutomatico ? "modo automatico ligado" : "modo automatico desligado (so comandos do serial)");
        break;
    }
  }
}
