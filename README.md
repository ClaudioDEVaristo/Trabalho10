🌦️ Estação Meteorológica com Interface Web
Este projeto tem como objetivo o desenvolvimento de uma estação meteorológica embarcada, utilizando sensores conectados via I²C, com visualização local e remota dos dados ambientais, além de alertas visuais e sonoros baseados em limites configuráveis.

📋 Descrição Geral
O sistema é capaz de:

📡 Capturar em tempo real dados de temperatura, umidade relativa e pressão atmosférica.

🌐 Servir uma interface web via Wi-Fi, permitindo o acompanhamento remoto dos valores e configuração de limites.

🚨 Emitir alertas visuais e sonoros quando os limites são ultrapassados (LED RGB, matriz de LEDs e buzzer).

🧠 Sensores Utilizados
AHT20: Temperatura e umidade relativa

BMP280: Pressão atmosférica e temperatura

💻 Interface Web
A interface HTML/CSS/JS:

Mostra valores atuais (temperatura, umidade, pressão)

Permite definir limites mínimo e máximo para os sensores

Atualiza valores dinamicamente usando AJAX (JSON)

Responsiva e compatível com celulares e PCs

🔧 Funcionalidades da Interface:
Campos para configurar limites dos sensores

Botão de atualização dos valores

Feedback visual de sucesso (ex: “Limites atualizados!”)

🚨 Recursos Visuais e Interativos
O projeto utiliza diversos recursos da plataforma BitDogLab, incluindo:

🟩 Matriz de LEDs: Representação visual de faixas de valores

🔴 LED RGB: Indicação de status do sistema

🔊 Buzzer: Alerta sonoro ao ultrapassar os limites definidos

🛠️ Tecnologias e Ferramentas
C/C++ com SDK do Raspberry Pi Pico

HTML, CSS e JavaScript (AJAX)

Plataforma BitDogLab

Servidor embutido no firmware para servir páginas via Wi-Fi


