🌦️ Estação Meteorológica com Interface Web
Este projeto tem como objetivo o desenvolvimento de uma estação meteorológica embarcada, utilizando sensores conectados via I²C, com visualização local e remota dos dados ambientais, além de alertas visuais e sonoros baseados em limites configuráveis.

📋 Descrição Geral
O sistema é capaz de:

📡 Capturar em tempo real dados de temperatura, umidade relativa e pressão atmosférica.

🖥️ Exibir valores no display OLED SSD1306 conectado ao sistema embarcado.

🌐 Servir uma interface web via Wi-Fi, permitindo o acompanhamento remoto dos valores e configuração de limites.

🚨 Emitir alertas visuais e sonoros quando os limites são ultrapassados (LED RGB, matriz de LEDs e buzzer).

🧠 Sensores Utilizados
AHT20: Temperatura e umidade relativa

BMP280: Pressão atmosférica e temperatura

💻 Interface Web
A interface HTML/CSS/JS:

Mostra valores atuais (temperatura, umidade, pressão)

Permite definir limites mínimo e máximo para os sensores

Exibe gráficos simples (linha ou barras)

Atualiza valores dinamicamente usando AJAX (JSON)

Responsiva e compatível com celulares e PCs

🔧 Funcionalidades da Interface:
Campos para configurar limites dos sensores

Botão de atualização dos valores

Indicadores gráficos dos dados em tempo real

Feedback visual de sucesso (ex: “Limites atualizados!”)

🚨 Recursos Visuais e Interativos
O projeto utiliza diversos recursos da plataforma BitDogLab, incluindo:

📺 Display OLED: Exibição local de dados e status da rede

🟩 Matriz de LEDs: Representação visual de faixas de valores

🔴 LED RGB: Indicação de status do sistema

🔊 Buzzer: Alerta sonoro ao ultrapassar os limites definidos

🟢 Push Buttons: Entrada para ajuste físico de configurações no sistema

Todos os recursos foram implementados com tratamento de interrupções e lógica de debounce para maior precisão e estabilidade.

🛠️ Tecnologias e Ferramentas
C/C++ com SDK do Raspberry Pi Pico

HTML, CSS e JavaScript (AJAX)

Plataforma BitDogLab

Servidor embutido no firmware para servir páginas via Wi-Fi

Gráficos em JavaScript

📦 Estrutura do Projeto
bash
Copiar
Editar
📁 src/
├── main.c                  # Lógica principal
├── sensores/               # Códigos dos sensores AHT20 e BMP280
├── display/                # Controle do OLED SSD1306
├── interface_web/          # HTML, CSS e JavaScript
└── recursos/               # Controle de LEDs, buzzer e matriz
📷 Demonstração

Exemplo da interface web com leitura de umidade e campos de limite configuráveis.

✅ Requisitos Atendidos
✅ Captação de temperatura, umidade e pressão

✅ Exibição local e remota dos dados

✅ Interface Web funcional e responsiva

✅ Limites configuráveis com feedback

✅ Gráficos simples para visualização

✅ Alertas sonoros e visuais

✅ Uso completo dos recursos da plataforma BitDogLab

👨‍🔧 Como Executar
Faça o build e grave o firmware no Raspberry Pi Pico com o código fornecido.

Conecte o dispositivo à sua rede Wi-Fi.

Acesse a interface web via IP fornecido no OLED.

Configure os limites desejados na interface.
