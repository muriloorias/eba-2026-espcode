# Agrinho Eba 2026: firmware do robô (ESP32)

Este é o firmware do robô do projeto Agrinho Eba 2026, um robô inspirado no TARS, de *Interestelar*, que funciona como assistente controlado por inteligência artificial e demonstra conceitos físicos como torque e equilíbrio.

## Por que ele é importante

O firmware é o "corpo" do robô. É ele que roda no ESP32 e lida com o mundo físico: lê os sensores, aciona os motores e, futuramente, a tela, o microfone e o som. É aqui que os conceitos de torque e equilíbrio aparecem na prática: o robô mede a própria inclinação e usa a força dos motores para se manter nivelado. Sem ele, a inteligência da API não teria como se mover nem interagir.

## O que faz hoje

- Lê a inclinação nos eixos X e Y com o sensor MPU6050 (I2C nos pinos 25 e 33).
- Liga os motores (pinos 4, 5, 18 e 19) até o robô ficar nivelado e desliga quando ele está dentro da margem de tolerância.
- Mostra os ângulos no Monitor Serial (115200 baud).

## Tecnologias

- ESP32 (placa DOIT DevKit V1), framework Arduino
- PlatformIO
- Bibliotecas Adafruit MPU6050 e Adafruit BMP280

## Como gravar no ESP32

Com o PlatformIO instalado (por exemplo, a extensão do VS Code):

```bash
pio run --target upload
pio device monitor
```

## Repositórios do projeto

- **eba-2026-espcode** (este): o corpo do robô.
- [agrinho-eba-2026-api](https://github.com/muriloorias/agrinho-eba-2026-api): a API de IA, que é a inteligência do robô.
