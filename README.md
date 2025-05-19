<div align="center">
    <img src="https://moodle.embarcatech.cepedi.org.br/pluginfile.php/1/theme_moove/logo/1733422525/Group%20658.png" alt="Logo Embarcatech" height="100">
</div>

<br>

# Estação de Monitoramento de Enchentes

## Sumário

- [Descrição](#descrição)
- [Funcionalidades Implementadas](#funcionalidades-implementadas)
- [Ferramentas utilizadas](#ferramentas-utilizadas)
- [Objetivos](#objetivos)
- [Instruções de uso](#instruções-de-uso)
- [Vídeo de apresentação](#vídeo-de-apresentação)
- [Aluno e desenvolvedor do projeto](#aluno-e-desenvolvedor-do-projeto)
- [Licensa](#licença)

## Descrição

Este projeto implementa uma estação de monitoramento de enchentes, utilizando o microcontrolador RP2040 e o sistema operacional em tempo real FreeRTOS. O sistema é composto por matriz de LEDs, LED RGB, display SSD1306, buzzer e Joystick, e foi desenvolvido exclusivamente com tarefas e filas do FreeRTOS. Os eixos do Joystick simulam sensores de volume de chuva e nível de água, e o sistema entra em modo alerta quando esses valores aumentam acima do limite estipulado, o LED RGB, a matriz de LEDs, o Display OLED e o buzzer mudam de comportamento a depender do modo normal ou alerta.

## Funcionalidades Implementadas

1. Joystick

   - Os eixos X e Y do Joystick simulam sensores de volume de chuva e nível de água.
   - Foi desenvolvida uma fórmula que transforma os valores lidos de ADC para uma escala de porcentagem de volume e nível:

<div align="center">
    <img width = "60%" src = "https://github.com/user-attachments/assets/3dd5a485-e257-489b-b98d-e9d0894ee723">
</div>

   - O modo alerta é ativado quando o volume de chuva é maior ou igual a 80% ou quando o nível de água é maior ou igual a 70%.
   - As informações são enviadas para filas do FreeRTOS.

2. Display OLED

   - Recebe os valores das filas.
   - Exibe informações do modo atual, do volume de chuva e do nível de água.

3. LED RGB

   - Recebe o valor do modo pela fila.
   - O LED RGB exibe a cor verde no modo normal e a cor vermelha no modo alerta.

4. Matriz de LEDs RGB

   - Recebe o valor do modo pela fila.
   - Exibe um “N” na cor verde no modo normal e uma “!” na cor vermelha no modo alerta.
  
5. Buzzers

   - Recebe o valor do modo pela fila.
   - Emitem um sinal sonoro intermitente quando o modo alerta está ativado.
  
## Ferramentas utilizadas

- **Ferramenta educacional BitDogLab (versão 6.3)**: Placa de desenvolvimento utilizada para programar o microcontrolador.
- **Microcontrolador Raspberry Pi Pico W**: Responsável pela execução das tarefas do FreeRTOS e pelo controle de periféricos.
- **Joystick**: Utilizado para simular os sensores de volume de chuva e nível de água.
- **Display OLED SSD1306**: Exibe as informações de modo, volume e nível.
- **LED RGB**: Altera de cor de acordo com o modo.
- **Matriz de LEDs RGB**: Exibe símbolos diferentes de acordo com o modo.
- **Buzzers**: Emite sinal sonoro no modo alerta.
- **Visual Studio Code (VS Code)**: IDE utilizada para o desenvolvimento do código com integração ao Pico SDK.
- **Pico SDK**: Kit de desenvolvimento de software utilizado para programar o Raspberry Pi Pico W em linguagem C.
- **FreeRTOS**: Sistema operacional em tempo real, utilizado para gerenciar as multitarefas e as filas desenvolvidas.

## Objetivos

1. Aplicar os conceitos aprendidos sobre filas no FreeRTOS.
2. Implementar uma estação de monitoramento de enchentes com alertas visuais e sonoros.
3. Implementar um sistema acessível.

## Instruções de uso

1. **Clonar o Repositório**:

```bash
git clone https://github.com/bigodinhojf/Embarcatech_F2T4_estacao.git
```

2. **Compilar e Carregar o Código**:
   No VS Code, configure o ambiente e compile o projeto com os comandos:

```bash	
cmake -G Ninja ..
ninja
```

3. **Interação com o Sistema**:
   - Conecte a placa ao computador.
   - Clique em run usando a extensão do raspberry pi pico.
   - Movimente o Joystick para variar os valores de volume de chuva e nível de água.
   - Observe os diferentes alertas visuais e sonoros.

## Vídeo de apresentação

O vídeo apresentando o projeto pode ser assistido [clicando aqui](https://youtu.be/BgiW1ZC8qdg).

## Aluno e desenvolvedor do projeto

<a href="https://github.com/bigodinhojf">
        <img src="https://github.com/bigodinhojf.png" width="150px;" alt="João Felipe"/><br>
        <sub>
          <b>João Felipe</b>
        </sub>
</a>

## Licença

Este projeto está licenciado sob a licença MIT.
