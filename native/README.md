# Aplicação nativa

O ponto de entrada atualizado é o [README principal](../README.md).
Build: `cmake -S native -B build-native -DCMAKE_BUILD_TYPE=Release`, a partir
da raiz; depois `cmake --build build-native --parallel 1` e
`ctest --test-dir build-native --output-on-failure`.

`BUILD_NATIVE_APP=OFF` compila somente o núcleo/testes portáveis. Esse modo não
compila nem valida a aplicação SDL/GStreamer. A RC1 exige também o build ON no
Raspberry antes da aprovação. Documentos v0.x em docs são registros históricos,
não o estado de validação da RC1.
