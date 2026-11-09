// Modelo das configuracoes de rede.
// Copie este arquivo para "rede.h" (na mesma pasta) e preencha com a sua rede.
// O rede.h esta no .gitignore, entao a senha nao vai para o repositorio.
#pragma once

//o ambiente "wokwi" do platformio.ini define WOKWI e usa a rede virtual do simulador
#ifdef WOKWI
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_SENHA ""
#define WIFI_CANAL 6 //canal fixo da rede do Wokwi, conecta mais rapido
#else
#define WIFI_SSID "NOME_DA_SUA_REDE"
#define WIFI_SENHA "SENHA_DA_SUA_REDE"
#define WIFI_CANAL 0 //0 = procura em todos os canais
#endif

#define WIFI_TIMEOUT 15000   //tempo maximo tentando conectar (ms)
#define WIFI_RECONEXAO 10000 //intervalo entre tentativas de reconexao (ms)
