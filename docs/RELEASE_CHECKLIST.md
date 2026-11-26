# Checklist de aceite — 1.0.0-rc1

**Candidata à validação. Não aprovada para uso diário.** `[x]` significa
verificação executada nesta entrega; código escrito sem teste no destino não
recebe aprovação física por inferência.

## Desenvolvimento / evidência no host

- [x] Base v0.5 identificada e SHA dos arquivos/árvores/commit conferidos.
- [x] Branch local feature/native-tv-release-v10 criada sobre 35d8502.
- [x] Testes originais portáveis executados antes das mudanças: 4/4.
- [x] Núcleo C++17 compilado e testado por CMake/CTest no Windows.
- [x] Parser/metadados/UTF-8/limites/duplicatas/listas vazias e inválidas testados.
- [x] Grupos, paginação/navegação e fila de reprodução/favoritos testados.
- [x] Favoritos/settings: persistência, reorder, token rotativo, corrupção e escrita atômica testados.
- [x] Estados, deadlines, retry/backoff/cancelamento testados com relógio injetável.
- [x] Input normalizado: edges, reconexão/reset, histerese e wrap de relógio testados.
- [x] Recarga de 20 mil canais em worker testada.
- [x] Testes do servidor legado preservados: 9/9.
- [x] Documentação criada/sincronizada com implementação e limitações.
- [x] Nenhum merge no main ou alteração automática de serviços/instalação aprovada no Pi realizada.
- [x] Publicação da branch de finalização autorizada pelo usuário; playlist privada excluída da entrega.

## Build / integração Linux

- [x] Aplicação completa ARM64 compilada após hotfix X11 (build retomado; log fornecido pelo usuário).
- [x] Build completo em diretório ARM64 isolado com SDL2/GStreamer 1.26,
  incluindo Spotify; log de compilação integral fornecido pelo usuário em 25/11/2026.
- [x] CTest completo Linux (12/12 PASS, 4,46 s; log fornecido pelo usuário).
- [x] Ownership real de GStreamer com CRITICAL fatal aprovado na RC1 (teste #11).
- [x] Player real com fakesinks: WAV → PLAYING/pausa/retomada/stop; falhas/requisições obsoletas/shutdown (teste #12).
- [x] Sintaxe Bash e simulação instalação/atualização/rollback/desinstalação aprovadas (log do Pi fornecido pelo usuário, teste #10, 1,31 s).
- [ ] Pacote ARM64 com executável/logs/checksums gerado e conferido.

## Matriz física mínima na TV

- [x] Log SDL confirma renderização interna e saída 1920×1080 e reconhecimento do PS4 (não confirma navegação/vídeo/áudio).
- [ ] Conferir modelo real do Raspberry pelo device tree.
- [ ] Abrir UI e confirmar saída/renderização 1920×1080 sem sobreposição de textos.
- [ ] DualShock navega, confirma, volta, retorna ao início e abre ajuda.
- [ ] Desconectar/reconectar Bluetooth e repetir ações sem duplicação.
- [x] Carregar playlist real privada: 398 canais, 0 inválidos, 0 duplicados, sem truncamento; confirmado pelo usuário.
- [ ] Navegar por grupos/páginas da playlist real e validar todos os comandos físicos.
- [ ] Abrir canal compatível: vídeo fisicamente visível.
- [ ] Stream com trilha: áudio audível no dispositivo selecionado.
- [ ] Volume/mute/restauração audíveis e persistidos após reabrir.
- [ ] L2/R2 trocam só na lista/grupo de origem, inclusive Favoritos.
- [ ] ○ interrompe abertura, buffering, retry e vídeo; menu retorna.
- [ ] Reabrir outro canal após stop/falha.
- [ ] Favoritar, fechar, reabrir e recarregar lista: favoritos mantidos.
- [ ] Simular URL indisponível/DNS inválido/403/404 em catálogo de teste privado.
- [ ] Cancelar reconexão e confirmar ausência de travamento no SDL.
- [ ] OSD aparece/desaparece sem corromper vídeo; fallback --no-osd utilizável.
- [ ] Encerrar por controle/Ctrl+Q/SIGINT sem GStreamer-CRITICAL.
- [ ] Registrar memória/temperatura/throttling em amostra curta, sem estresse.
- [ ] Confirmar que DietPi/LightDM/XFCE/X11/SSH/Bluetooth/rede continuam operando.
- [ ] Executar instalação opcional somente após revisão; confirmar rollback/desinstalação em diretório de teste.

## Aprovação

- [ ] Todos os itens obrigatórios acima atendidos e resultados registrados em TEST_REPORT.md.
- [ ] Limitações de alimentação/desempenho aceitas com medições reais.
- [ ] Investigar travamentos relatados; subtensão é hipótese, não diagnóstico exclusivo comprovado.
- [ ] Release 1.0 final aprovada. Até lá, manter o identificador RC1.

## Adicional Spotify — build no Pi aprovado; aceite físico pendente

- [x] Base RC1 preservada; nenhum merge no main ou substituição da instalação aprovada pelo agente.
- [x] Testes portáveis existentes executados antes da alteração: 9/9 PASS.
- [x] Testes offline Spotify e worker/protocolo/navegação C++ executados.
- [x] Credencial real ausente de fonte, testes, exemplos e arquivos Git.
- [x] Configuração privada e instruções de instalação/execução/rollback preparadas.
- [x] Build ARM64 da interface alterada e CTest completo, incluindo processo POSIX:
  15/15 PASS, 12,26 s, conforme log do usuário.
- [x] UI inicializa e reporta renderização interna/saída 1920×1080 no log SDL.
- [x] Permissões POSIX 0700/0600 em fixtures de teste aprovadas na suíte Python.
- [x] Configuração da chave por prompt oculto confirmada pelo log do usuário.
- [x] Raspberry TV selecionado no Spotify Connect, conforme captura do usuário.
- [ ] Permissões dos arquivos reais, versão do cliente Soloist e áudio HDMI audível verificados no Pi.
- [ ] OAuth real opcional, playlists da conta e dispositivo real.
- [ ] Regressão física IPTV/Full HD/DS4/OSD após esta integração.

## Correção posterior Spotify / ausência de som

- [x] Foto fornecida confirma painel APLICATIVOS na TV e indicador PS4 conectado.
- [x] Registrar pausa relatada e ausência de som confirmada pelo usuário.
- [x] Corrigir alternância pausa/retomada e cabeçalho desatualizado; testes no host.
- [x] Testar preferência de saída isolada no processo e preservação da chave.
- [x] Preparar diagnóstico de saídas/mute/fluxos sem credenciais.
- [ ] Obter diagnóstico real e identificar a saída usada pelo Soloist.
- [ ] Compilar e executar os 15 CTest no Pi após esta correção.
- [ ] Confirmar música audível, pausa/retomada, faixas e volume na TV.

## Ícone de abertura manual — 26/11/2026

- [x] Preparar ícone próprio, launcher XFCE com Terminal=false e ponte Windows.
- [x] Validar sintaxe do instalador Windows e payload Python sem conexão SSH.
- [x] Gerar e reler o arquivo .lnk na pasta de entrega com destino/ícone corretos.
- [ ] Instalar na área de trabalho Windows pela sessão normal do usuário
  (a gravação direta do agente recebeu Acesso negado).
- [ ] Primeiro uso no Pi: ícone visível, abertura do app e prevenção de duplicata.

## Consolidação do pacote central — 26/11/2026

- [x] Reunir TV, integração Spotify, atalhos e diagnóstico no mesmo código-fonte.
- [x] Incluir todos os arquivos do atalho no layout de instalação CMake/ARM64.
- [x] Build limpo portável e 11/11 CTest aprovados novamente antes da publicação.
- [x] Suíte Python completa aprovada no host, com o teste POSIX pendente.
- [x] Empacotador recusa checkout com alterações não commitadas.
- [x] Gerar ZIP/TAR central: arquivos relidos e 96 hashes de fontes conferidos.
- [x] Auditar os blobs dos commits Spotify/atalhos e recusar credenciais reais.
- [x] Usuário autorizou publicar a integração Spotify e o pacote central no Git.
- [x] Publicação confirmada no GitHub nas duas branches, com árvores de arquivos
  idênticas às auditadas e main preservado.
- [x] CI Linux do pacote central: aplicação completa compilada, 15/15 CTest e
  60/60 testes Python aprovados no commit 64aa777.
- [ ] Novo build completo ARM64 e confirmação física do áudio Spotify.

## Soloist pelo ícone — 26/11/2026

- [x] Cliente configurado inicia pelo ícone em sessão independente do terminal.
- [x] Implementar reutilização, locks de instância, duas recuperações e cooldown.
- [x] Implementar parada apenas do grupo gerenciado com identidade de PID verificada.
- [x] Adaptador mantém o protocolo do binário existente e evita recompilação no Pi.
- [x] Transporte Windows comprimido conferido; nenhum segredo é transportado.
- [x] Host: 12/12 CTest e 70 casos Python, com skips POSIX explicitamente registrados.
- [x] Linux CI: 16/16 CTest e 70/70 testes Python, incluindo os seis casos de
  processos/supervisão, aprovados após a correção no commit 237ddc1.
- [x] Preparar atualização do instalador existente sem mudar o binário aprovado.
- [ ] Aplicar launcher atualizado e confirmar no Raspberry após fechar SSH.
- [ ] Confirmar música audível na saída correta.
