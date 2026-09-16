# Validação desta entrega

## Executado

Ambiente Linux, GCC 13.3.0, modo C++23, 24 verificações da lógica sem dependências Windows:

```bash
g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize=address,undefined -g tests/CoreTests.cpp -o /tmp/voxel-core-tests
ASAN_OPTIONS=detect_leaks=0 /tmp/voxel-core-tests
```

Resultado: **24 checks passed**, sem diagnóstico de AddressSanitizer ou UndefinedBehaviorSanitizer. A primeira execução com LeakSanitizer habilitado falhou por limitação do ambiente sob ptrace; a detecção de vazamentos foi desabilitada na execução reportada. Não se afirma que vazamentos foram testados.

Cobertura: geração determinística local; limites de leitura/escrita; atualização na fronteira das regiões; serialização e rejeição de corrupção, versão desconhecida, truncamento, bytes adicionais e materiais inválidos; raios em eixos positivos/negativos, alcance e direção nula; colisão, gravidade, salto, barreira e voo.

Os testes usam verificações explícitas em vez de `assert`, portanto não desaparecem com NDEBUG.

Arquivos XML de projeto/propriedades/manifesto também foram verificados quanto à sintaxe. Isso não equivale a validação semântica pelo MSBuild.

## Não executado

- Compilação/linkedição MSVC, Resource Compiler e Manifest Tool.
- Compilação dos shaders pelo D3DCompile e execução da janela.
- Testes visuais, de desempenho, hardware, drivers, instalação ou assinatura.
- Execução dos testes automatizados no Windows.

Não há EXE validado nem imagem de gameplay certificada no pacote.

## Roteiro de aceitação no Windows

1. Execute `tools/build.ps1` em Debug e Release. CoreTests deve imprimir `24 checks passed` nas duas configurações.
2. Inicie o jogo; confirme terreno, texturas e mira. Clique, olhe em todas as direções e teste materiais 1–5.
3. Ande contra paredes, pule e alterne voo. Confirme que não consegue colocar bloco no próprio corpo.
4. Edite perto de x=15/16 ou z=15/16 e confirme que faces na fronteira atualizam.
5. Pressione Esc, clique novamente, faça Alt+Tab e feche pelo X; o cursor deve sempre ser liberado.
6. Redimensione, minimize/restaure e mova entre monitores com DPI diferentes. Não deve haver erro de back buffer nem distorção da proporção.
7. Edite, pressione F5, feche e reabra: a edição deve persistir. Verifique o save em LocalAppData.
8. Com o jogo fechado, faça cópia do save e corrompa somente uma cópia de teste no local de leitura. A inicialização deve reportar erro e preservar os bytes inválidos. Restaure o backup.
9. Teste falha de salvamento num perfil de teste com permissões restritas, preservando os dados reais. Fechar deve ser cancelado e informar a falha.
10. Observe o Output com a camada de debug D3D11 instalada. Resolva erros de recursos e pipeline antes de declarar a versão validada.
11. Execute Release numa máquina de teste sem Visual Studio, como usuário padrão.

Registre versão do Windows, GPU, driver, MSVC, SDK, configuração e resultados. Só então substitua o status “não executado” por evidência específica.
