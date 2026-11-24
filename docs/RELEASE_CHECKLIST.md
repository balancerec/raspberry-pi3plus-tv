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
- [x] Nenhum push, merge no main, alteração em serviço ou instalação no Pi realizada.

## Build / integração Linux ainda necessários

- [ ] Build completo limpo ARM64 com SDL2/GStreamer 1.26.
- [ ] CTest completo Linux (12 testes): núcleo, scripts e dois testes GStreamer.
- [ ] Ownership real de GStreamer com CRITICAL fatal aprovado na RC1.
- [ ] Player real: WAV → PLAYING/pausa/retomada/stop; falhas/requisições obsoletas/shutdown.
- [ ] Sintaxe Bash e simulação instalação/atualização/rollback/desinstalação aprovadas.
- [ ] Pacote ARM64 com executável/logs/checksums gerado e conferido.

## Matriz física mínima na TV

- [ ] Conferir modelo real do Raspberry pelo device tree.
- [ ] Abrir UI e confirmar saída/renderização 1920×1080 sem sobreposição de textos.
- [ ] DualShock navega, confirma, volta, retorna ao início e abre ajuda.
- [ ] Desconectar/reconectar Bluetooth e repetir ações sem duplicação.
- [ ] Carregar playlist real privada e navegar por grupos/páginas.
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
- [ ] Release 1.0 final aprovada. Até lá, manter o identificador RC1.
