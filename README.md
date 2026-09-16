# VoxelNative — sandbox de blocos nativo do Windows

Projeto educacional original inspirado na mecânica de construir e remover blocos.
**Versão 0.1: base de jogo, não um clone completo nem um produto comercial certificado.**
Não usa código, nome de produto, texturas ou outros arquivos do Minecraft.

## Comece aqui

1. Instale o Visual Studio 2026 e a carga **Desenvolvimento para desktop com C++**.
2. No instalador, inclua MSVC x64/x86 e um Windows 10/11 SDK recente.
3. Extraia **todo** este ZIP para uma pasta gravável. Não execute de dentro do ZIP.
4. Abra `VoxelNative.sln` no Visual Studio.
5. Selecione **Debug | x64**. Clique com o botão direito no projeto **VoxelNative** e escolha **Definir como projeto de inicialização**.
6. Use **Compilar > Compilar Solução** (`Ctrl+Shift+B`) e depois `F5`.
7. Clique na área do jogo para capturar o mouse. Pressione `F1` para ajuda.
8. Para uma versão otimizada, selecione **Release | x64** e compile novamente.

Executável: `bin\x64\Debug\VoxelNative.exe` ou `bin\x64\Release\VoxelNative.exe`.
Não há executável pré-compilado no pacote: a parte Windows ainda precisa ser compilada e validada no seu PC.

Se usar Visual Studio 2022 atualizado, altere **Conjunto de Ferramentas da Plataforma** para **v143** nos DOIS projetos, em Todas as Configurações. A configuração entregue usa **v145**, para VS 2026. Se aparecer MSB8020, o toolset selecionado não está instalado.

## O que está implementado

- Janela Win32 Unicode, tratamento de DPI por monitor, redimensionamento e perda de foco.
- Direct3D 11, swap chain flip-discard, profundidade, VSync e alternativa de renderização por software WARP se a criação do dispositivo de hardware falhar.
- Mundo finito de 48 × 32 × 48 blocos, nove regiões de 16 × 32 × 16 e reconstrução das regiões afetadas por edições.
- Faces internas omitidas; luz direcional simplificada por face e neblina de distância.
- Cinco texturas pixeladas originais de 16 × 16, geradas na inicialização, em um array de texturas: grama, terra, pedra, madeira e areia.
- Árvores estilizadas: tronco de madeira e copa usando o material grama.
- Câmera em primeira pessoa, Raw Input, colisão, gravidade, salto e voo com colisão.
- Remoção/colocação por raio DDA de até sete unidades; bloqueio de colocação dentro do jogador.
- Mira central. Material atual e modo aparecem no título da janela.
- Salvamento manual, a cada 30 segundos se alterado enquanto ativo, e ao fechar. Fechar é cancelado se salvar falhar.
- Formato binário versionado, tamanho fixo, validação de materiais e checksum; escrita temporária seguida de substituição.
- RAII, ponteiros COM gerenciados, erros com contexto, log e 24 verificações automatizadas da lógica.

## Controles

| Ação | Entrada |
|---|---|
| Capturar mouse | Clique esquerdo |
| Olhar | Mouse |
| Mover | W, A, S, D |
| Pular / subir voando | Espaço |
| Descer voando | Ctrl |
| Alternar caminhada/voo | F |
| Remover bloco | Botão esquerdo, após capturar |
| Colocar bloco | Botão direito |
| Grama / terra / pedra / madeira / areia | 1 / 2 / 3 / 4 / 5 |
| Salvar | F5 |
| Liberar mouse e pausar simulação | Esc |
| Mostrar ajuda | F1 |
| Sair | Fechar a janela / Alt+F4 |

O personagem começa acima do centro do mundo e cai ao capturar o mouse. Não há dano de queda.
O limite inferior é protegido contra remoção; bordas laterais bloqueiam o jogador.
O salvamento guarda os blocos, não posição, orientação, teclas ou material selecionado.

## Documentação

- `docs/GUIA-WIN32.md`: tutorial para quem nunca usou a Windows API e leitura guiada do código.
- `docs/PROPRIEDADES.md`: propriedades entregues, bibliotecas e como configurar do zero.
- `docs/QUALIDADE-E-LIMITES.md`: arquitetura, testes, limitações e trabalho necessário para produção.
- `docs/VALIDACAO.md`: evidência das verificações executadas e roteiro para Windows.
- `docs/FONTES.md`: referências oficiais consultadas.

## Compilar sem navegar pela interface

No PowerShell, dentro da pasta extraída:

```powershell
.\tools\build.ps1 -Configuration Debug
.\tools\build.ps1 -Configuration Release
.\tools\build.ps1 -Configuration Debug -Analyze
```

Para VS 2022: acrescente `-Toolset v143`. Se a política de execução impedir scripts, use a interface do Visual Studio ou execute em **Developer PowerShell for VS**:

```powershell
msbuild .\VoxelNative.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
.\bin\x64\Release\CoreTests.exe
```

O script não instala dependências, não muda políticas do sistema e não exige administrador.

## Dados e recuperação

Os dados ficam em `%LOCALAPPDATA%\VoxelNative\world.vxn`; erros em `errors.log` na mesma pasta.
Cole `%LOCALAPPDATA%\VoxelNative` na barra do Explorer para abrir.

Para iniciar outro mundo, feche o jogo e **renomeie** `world.vxn` para `world-backup.vxn`.
Se um save estiver inválido, o jogo interrompe a inicialização para preservá-lo. Faça uma cópia antes de qualquer reparo.
O checksum detecta corrupção acidental, não é autenticação criptográfica. Não há nuvem, rede ou telemetria.

## Padrão C++ e dependências

A configuração usa `/std:c++latest`, que habilita o conjunto mais recente **implementado pelo MSVC instalado**, incluindo recursos em desenvolvimento. Isso não significa implementação completa de C++26 nem estabilidade ABI entre toolsets.
O código privilegia recursos consolidados (RAII, `std::span`, `std::optional`, `std::array`, `std::unique_ptr`) em vez de exigir recursos experimentais sem necessidade. Ele não é uma demonstração de recursos exclusivos de C++26.
Para produção, fixe versões do compilador/SDK e o modo de linguagem após a validação.

Somente biblioteca padrão C++, Win32, Direct3D/DXGI, DirectXMath e WRL disponibilizados pela Microsoft. Sem SDL, GLFW, OpenGL, Vulkan, Boost, motor externo, NuGet ou vcpkg. Direct3D faz parte das APIs nativas do Windows; GDI sozinho não é uma base adequada para este tipo de renderização 3D.

Alvo inicial: Windows 10/11 x64, com Direct3D feature level 11_0 ou WARP. Isso é um alvo técnico, não uma matriz de compatibilidade já certificada. A adoção de Win32 não garante funcionamento em todas as versões do Windows ou todos os drivers.
