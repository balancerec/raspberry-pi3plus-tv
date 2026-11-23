# Raspberry TV — módulo isolado (v1.0)

Extraído do **raspberry-spi-lcd-nrf-system** fornecido pelo usuário. A interface de TV (`tv.html`, `tv.css`, `tv.js`) foi preservada visualmente, com ajustes mínimos nas rotas locais e na consulta de métricas para funcionar **sem o restante do projeto original**.

## O que foi isolado

- **Interface Raspberry TV:** página inicial, cartões de aplicativos, atalhos Netflix/YouTube/Prime Video/Disney+/Spotify/navegador, relógio, navegação pelo teclado e **Gamepad API** do navegador.
- **Servidor Python independente:** somente leitura, biblioteca padrão Python 3.10+, `/tv`, `/info` e `/api/status`.
- **Status local:** nome do equipamento, IP, uso de CPU quando disponível, temperatura em sistemas Linux com sensor compatível.
- **Painel local simplificado:** `/info` substitui o painel administrativo do projeto original e apresenta apenas métricas de leitura.

## O que NÃO faz parte deste pacote

- Gerenciamento SPI, NRF24L01, display ST7735R, GPIO, pareamento Bluetooth, mouse HDMI virtual, configuração de Wi-Fi e APIs administrativas.
- Servidor original na porta 8080, serviço `raspberry-display.service`, scripts sudo e event loop `main.py`.
- Entrada via `evdev` no backend: controle PS4 permanece disponível **apenas se o navegador expuser o controle à Gamepad API**. D-pad, botão principal, voltar e home podem depender do mapeamento do navegador/controlador. Use teclado como alternativa.
- **Não há player IPTV, M3U nem EPG nesta versão.** Os cartões atuais abrem sites externos. Serviços DRM (Netflix, Disney+, Prime Video) podem não reproduzir vídeo no navegador Firefox do Pi 3; atalhos não garantem compatibilidade com codecs/DRM.

## Segurança e convivência com a aplicação original

- Porta **8090** (a original usa **8080**). Nenhum processo do sistema atual é encerrado.
- Bind padrão **127.0.0.1** (somente acesso local), sem API de escrita ou comandos de administração.
- Use `--host 0.0.0.0` somente na rede doméstica confiável. O servidor utiliza HTTP sem autenticação e **não deve ser exposto à internet**.
- O pacote não modifica login gráfico, LightDM, autostart, rede, Tailscale ou serviços do DietPi.
- **Evite abrir outro Firefox em modo quiosque simultaneamente** com o Firefox atual, devido à RAM limitada do Raspberry Pi 3.

## Executar no Windows (PowerShell)

Descompacte o ZIP e, dentro da pasta `raspberry-tv-isolada`:

```powershell
py -3 .\server.py
```

Ou execute `powershell -ExecutionPolicy Bypass -File .\scripts\run-windows.ps1` se a política local impedir a execução do script. No navegador:

- <http://127.0.0.1:8090/tv> — interface de TV
- <http://127.0.0.1:8090/info> — informações de leitura

Não é necessário instalar pacotes Python adicionais.

## Executar no Raspberry Pi / DietPi sem modificar a instalação

Do PowerShell do Windows, na pasta onde o ZIP foi baixado:

```powershell
scp .\raspberry-tv-isolada.zip balancerec@192.168.100.17:/home/balancerec/
ssh balancerec@192.168.100.17
```

No terminal SSH:

```bash
cd /home/balancerec
unzip -q raspberry-tv-isolada.zip
cd raspberry-tv-isolada
python3 server.py --host 0.0.0.0 --port 8090
```

Abra do Windows **na mesma rede local**:

`http://192.168.100.17:8090/tv`

**Alternativa com bind local e túnel SSH:** rode `python3 server.py` no Raspberry. Em outra janela PowerShell use `ssh -L 8090:127.0.0.1:8090 balancerec@192.168.100.17` e abra `http://127.0.0.1:8090/tv` no Windows. Garanta que não haja um servidor local ocupando a porta 8090.

Para interromper a aplicação de teste, pressione `Ctrl+C` na sessão do servidor. A aplicação principal continuará funcionando na porta 8080.

## Serviço opcional systemd (não instalado automaticamente)

O exemplo `systemd/raspberry-tv@.service` assume que os arquivos estão em `/home/<usuario>/raspberry-tv-isolada` e usa porta 8090 em localhost. Após validar manualmente, instale **se desejar**:

```bash
sudo cp systemd/raspberry-tv@.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now raspberry-tv@balancerec.service
sudo systemctl status raspberry-tv@balancerec.service --no-pager
```

Desativar esse serviço **não afeta** `raspberry-display.service`:

```bash
sudo systemctl disable --now raspberry-tv@balancerec.service
```

## Estrutura

```text
raspberry-tv-isolada/
├── server.py                   # Servidor HTTP independente (sem dependências externas)
├── static/
│   ├── tv.html                # Interface de TV extraída (adaptada)
│   ├── tv.css                 # CSS original preservado
│   ├── tv.js                  # Navegação local + Gamepad API + status
│   ├── info.html              # Painel mínimo de leitura (novo)
│   ├── info.js
│   └── info.css
├── scripts/
│   ├── run-windows.ps1
│   └── run-linux.sh
├── systemd/
│   └── raspberry-tv@.service  # Opcional
├── tests/
│   └── test_server.py
├── SHA256SUMS.txt
└── README.md
```

## Verificações

```bash
python3 -m unittest discover -s tests -v
python3 -m py_compile server.py
```

O arquivo `SHA256SUMS.txt` inclui hashes dos arquivos do pacote (exceto o próprio manifesto), para conferência de integridade.

## Critérios de aceitação

- A página `/tv` carrega os estilos e scripts sem depender de SPI/NRF/PS4.
- A página `/info` exibe métricas do sistema quando disponíveis.
- As setas, Enter, Esc e Home navegam sem depender de hardware específico.
- `/api/status` é um endpoint de leitura; POST é recusado.
- A aplicação original em `:8080` continua intocada.
