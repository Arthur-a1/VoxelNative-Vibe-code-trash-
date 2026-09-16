# Qualidade, limites e caminho para produção

**A entrega é um protótipo estruturado.** “Todas as melhores práticas” não é uma propriedade verificável de qualquer aplicação: decisões precisam de requisitos, orçamento de desempenho, plataformas e evidência de testes. O uso de Win32 e C++ recente não torna automaticamente um jogo comercial.

## Decisões adotadas

| Área | Implementação e motivo |
|---|---|
| Dependências | Somente biblioteca padrão e APIs/headers Microsoft; reduz instalação e código de terceiros |
| Propriedade de recursos | RAII para arquivos, mutex, memória COM; `ComPtr` para GPU; `unique_ptr` para renderer |
| Estado | Estado por App, sem singleton de motor |
| Exceções | Erros propagados em C++; contidos nas fronteiras com callback e ponto de entrada |
| Entrada | Raw Input; perda de foco limpa teclas e solta cursor |
| Simulação | Passo fixo, limite de acumulação, colisão AABB por eixo |
| Mundo | Dados compactos de um byte, regiões alteradas, exclusão de faces internas |
| Persistência | Formato versionado com bytes explícitos, validação antes de modificar o mundo |
| Escrita | Arquivo temporário no mesmo diretório, flush e substituição; sem promessa absoluta diante de falha de hardware |
| Integridade | Checksum não criptográfico; tamanho e materiais validados |
| Privilégios | Sem administrador; saves em LocalAppData |
| Build | Debug/Release x64; símbolos; proteções básicas do compilador/linker |
| Testabilidade | Núcleo independente de Win32; testes não dependem de `assert` e funcionam em Release |

Um mutex nomeado evita dois escritores na mesma sessão do Windows. Isso não implementa coordenação entre sessões remotas simultâneas do mesmo usuário. Não há journal, backup rotativo nem restauração automática de `.tmp`.

## Limitações funcionais explícitas

- Não há crafting, inventário com quantidades, criaturas, combate, água, física de fluidos, iluminação por blocos, ciclo dia/noite, biomas extensos, multiplayer, som ou mods.
- O mapa é finito; regiões só subdividem o meshing, não implementam streaming de mundo infinito.
- A geração usa seno/cosseno e coordenadas fixas, sem seleção de seed. Não há garantia de bit-identidade entre implementações matemáticas de compiladores diferentes.
- A copa usa grama, sem transparência. Todas as faces de cada bloco usam o mesmo tile. São texturas originais simples, sem mipmaps ou pipeline artístico.
- A interface é mínima: título da janela, mira e diálogo de ajuda. Não há menu, hotbar gráfica, remapeamento ou suporte a gamepad.
- A colisão é simples, sem step-up, agachamento, plataformas móveis ou swept collision geral. O teste de ausência de atravessamento vale para velocidades e passo fixo implementados.
- O chão não pode ser removido pela interface, mas o decoder não impõe regras de gameplay ao conteúdo do save.
- Ao abrir o jogo, o jogador reaparece acima do centro; posição e câmera não são persistidas.

## Limitações técnicas

- Meshing síncrono pode causar picos de frame. Não há greedy meshing, buffers indexados, frustum/occlusion culling ou jobs. Backface culling está desativado: uma otimização futura deve primeiro validar winding de todas as faces.
- O shader é compilado na inicialização, não no build. O debug layer depende dos componentes opcionais do Windows.
- Remoção/reset de dispositivo durante execução é reportado como erro fatal. Ainda não há reconstrução automática dos recursos. Alterações desde o último save podem se perder nesse caso.
- Log simples, sem rotação, horário estruturado, crash dump ou pipeline de diagnóstico. Sem telemetria.
- Não há assinatura digital, instalador, atualizador, licença/EULA do produto ou suporte ao cliente.
- Configuração SDK `10.0` e `/std:c++latest` favorecem abertura local, mas não fornecem build reproduzível entre máquinas. Para releases, fixe ambos e registre a versão real do MSVC.
- Parte Win32/D3D11 não foi compilada ou executada no ambiente de entrega. Testes do núcleo não validam APIs, manifesto, shaders ou qualidade visual.

## Critérios recomendados antes de uma venda

1. **Definir produto:** lista de funcionalidades, regras de gameplay, requisitos mínimos e objetivo de FPS/memória. Estabelecer o que “clone” significa sem reutilizar assets de terceiros.
2. **Build Windows:** compilar Debug e Release em máquina limpa, corrigir avisos `/W4`, ligar `/WX` no código do projeto e revisar análise estática. Fixar MSVC, SDK e opções numa integração contínua Windows.
3. **GPU e desempenho:** validar Intel/AMD/NVIDIA, notebook com duas GPUs, WARP, diferentes resoluções/DPI e monitores. Medir tempos CPU/GPU e percentis de frame; não prometer FPS sem medições.
4. **Ciclo de vida:** recuperar device loss, suspensão/retorno, minimização, troca de monitor e mudança de resolução sem perda de mundo ou cursor preso.
5. **Persistência:** backups, migração, limites de tamanho, testes de falta de espaço/permissão, interrupção durante gravação e múltiplos processos/sessões. Fuzzing do parser conforme o formato evoluir.
6. **Jogabilidade e acessibilidade:** menus, opções salvas, remapeamento, escala de UI, sensibilidade/FOV, leitores de tela onde aplicável, feedback e tutorial.
7. **Entrega:** recursos e shaders empacotados, ícone, metadados finais, instalador/desinstalador, assinatura, notas de versão e símbolos arquivados. Testar como usuário padrão numa máquina sem Visual Studio.
8. **Operação:** política de suporte, tratamento de falhas e atualizações. Se adicionar serviços/rede, incluir autenticação, validação no servidor e gestão de dados adequada ao produto.

Esses itens são trabalho futuro, não recursos implicitamente implementados. O pacote não inclui uma licença final de distribuição do produto. Defina-a antes de publicar e inventarie qualquer asset ou dependência que venha a adicionar.
