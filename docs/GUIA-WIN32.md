# Aprendendo Win32 por este projeto

Win32 é o conjunto de APIs nativas que permite pedir serviços ao Windows: criar uma janela, receber teclado, abrir arquivos e muito mais. Apesar do nome, funciona em aplicações de 64 bits. Direct3D é a API gráfica usada aqui; ela é parte da plataforma Windows, mas tem conceitos próprios de GPU.

C++ é a linguagem. O Windows SDK fornece os headers, bibliotecas de importação e ferramentas. O Visual Studio organiza, compila e depura. O compilador não desenha a janela por você: seu código pede isso ao sistema.

## 1. Como o programa começa

Em `src/Main.cpp`, encontre:

```cpp
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
```

O subsistema **Windows** faz o runtime do C++ encaminhar a execução para esse ponto depois de inicializar o programa. Não configure um entry point manual no linker, porque isso pode pular a inicialização do runtime.

- `HINSTANCE` identifica o módulo executável.
- `PWSTR` é um ponteiro para texto UTF-16 mutável; aqui os argumentos não são usados.
- `show` informa como exibir a janela inicialmente.
- `WINAPI` expressa a convenção de chamada da API. Não é uma classe.

O `try/catch` externo transforma falhas C++ em log e mensagem legível. Não deixa uma exceção C++ escapar de uma callback do Windows.

## 2. Handles não são objetos C++ comuns

`HWND` identifica uma janela; `HANDLE` identifica diversos recursos do sistema. Eles são valores opacos. Não use `delete` neles.

| Recurso | Quem cria | Quem libera |
|---|---|---|
| Janela | `CreateWindowExW` | `DestroyWindow` |
| Arquivo/mutex | `CreateFileW` / `CreateMutexW` | `CloseHandle` |
| Memória de Known Folder | `SHGetKnownFolderPath` | `CoTaskMemFree` |
| Interface Direct3D | Métodos de criação | `Release`, via `ComPtr` |
| `std::vector` | Construtor C++ | Destrutor C++ |

`FileHandle` em `Storage.hpp` chama `CloseHandle` no destrutor e proíbe cópia, evitando fechamento duplo. Isso é **RAII**: aquisição e liberação acompanham a vida de um objeto. `ComPtr` aplica a mesma ideia ao contador de referências das interfaces COM.

## 3. Registrar a classe e criar a janela

A `WNDCLASSEXW` descreve um tipo de janela. A palavra classe aqui é um conceito do Windows, não uma classe C++.

1. Preencha tamanho, cursor, instância, nome e callback.
2. Registre com `RegisterClassExW`.
3. Crie uma instância dessa janela com `CreateWindowExW`.
4. Mostre com `ShowWindow`.

O sufixo `W` significa a variante Unicode UTF-16. Textos literais dessa API usam `L"texto"`. Os arquivos-fonte são UTF-8; `/utf-8` informa isso ao compilador. Essas duas codificações têm papéis diferentes.

O último argumento de `CreateWindowExW` recebe `this`. Em `WM_NCCREATE`, a callback recupera esse endereço e o guarda em `GWLP_USERDATA`. Assim cada janela pode encontrar seu objeto `App` sem uma variável global mutável.

## 4. Mensagens: como o sistema conversa com você

A callback é:

```cpp
LRESULT CALLBACK procedure(HWND hwnd, UINT msg, WPARAM w, LPARAM l) noexcept;
```

- `hwnd`: janela destinatária.
- `msg`: tipo de mensagem.
- `w` e `l`: dados cujo significado depende de `msg`.
- retorno: resultado esperado por aquela mensagem.

Não converta todos os parâmetros da mesma maneira. Em `WM_SIZE`, `l` contém largura/altura. Em `WM_DPICHANGED`, contém um ponteiro para `RECT`. Em `WM_INPUT`, contém um `HRAWINPUT`.

| Mensagem | Resposta neste jogo |
|---|---|
| `WM_SIZE` | Agenda recriação dos alvos de renderização; detecta minimização |
| `WM_DPICHANGED` | Aplica o retângulo recomendado para o novo monitor |
| `WM_KEYDOWN/UP` | Atualiza o estado das teclas |
| `WM_INPUT` | Lê deslocamento relativo do mouse |
| `WM_ACTIVATEAPP` | Libera mouse e limpa teclas quando perde foco |
| `WM_CLOSE` | Tenta salvar; fecha apenas se não houver alteração pendente |
| `WM_DESTROY` | Publica `WM_QUIT` |

Mensagens não tratadas vão para `DefWindowProcW`. Isso preserva comportamentos normais do Windows. `WM_INPUT` também passa por ela para limpeza da entrada em primeiro plano.

## 5. Loop de mensagens e loop de jogo

Um editor simples pode bloquear em `GetMessage`. Um jogo precisa desenhar mesmo sem novas mensagens, então usamos `PeekMessageW`:

```cpp
while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) { running = false; break; }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
}
```

`DispatchMessageW` chama a callback da janela. Depois de esvaziar a fila, o programa atualiza a simulação e desenha. Quando inativo ou minimizado, espera por mensagens com timeout, evitando ocupar um núcleo à toa.

A física usa passos fixos de 1/120 segundo. O tempo vem de `std::chrono::steady_clock`, não do relógio do calendário. O acumulador permite vários passos por frame. O limite de 0,1 segundo evita tentar recuperar minutos de simulação depois de uma pausa no depurador. É uma decisão de jogo: perde-se tempo simulado em travamentos longos.

Mover cinco unidades por frame faria a velocidade depender do computador. Mover `velocidade * dt` com passos fixos evita isso. A renderização usa VSync; interpolação entre estados de física ainda não foi implementada.

## 6. Mouse relativo e foco

`RegisterRawInputDevices` inscreve o programa para receber eventos do mouse. `GetRawInputData` lê `lLastX` e `lLastY`. Não precisamos reposicionar o cursor a cada frame para medir movimento.

`SetCapture` direciona mensagens de mouse; `ClipCursor` limita o cursor à área do jogo; `ShowCursor` controla sua visibilidade. Eles são APIs diferentes. O código emparelha captura/liberação, inclusive em Alt+Tab, fechamento e perda de captura. `Esc` libera o cursor e pausa a simulação.

Somente deslocamentos relativos são tratados. Dispositivos que entregam coordenadas absolutas precisam de uma adaptação futura.

## 7. Do bloco ao pixel: Direct3D 11

Leia `Renderer.cpp` nesta ordem:

1. `Renderer::Renderer`: cria dispositivo e contexto, swap chain e shaders.
2. `makeTextures`: cria os pixels na CPU e envia um array de texturas à GPU.
3. `resize`: recria o back buffer e o depth buffer com as dimensões da janela.
4. `rebuild`: converte blocos visíveis em triângulos, por região alterada.
5. `draw`: calcula câmera, vincula recursos, desenha e apresenta.

O **device** cria recursos da GPU. O **context** configura o pipeline e emite comandos. A **swap chain** organiza os buffers exibidos na janela. O **render target** é onde as cores são escritas. O **depth buffer** decide qual superfície está na frente.

Cada face quadrada tem dois triângulos, seis vértices nesta implementação. Cada vértice armazena posição, coordenadas de textura, índice do material e luminosidade. O input layout explica à GPU como interpretar esses bytes.

O vertex shader transforma a posição do mundo para a câmera e a projeção. O pixel shader consulta a textura, aplica a luz simples e mistura neblina. HLSL é a linguagem dos shaders, diferente de C++. O código HLSL está numa string em `Renderer.cpp` para não depender do diretório de execução.

A mira usa um segundo par de shaders e um triângulo de tela inteira, descartando todos os pixels fora da pequena cruz. Ela não depende de GDI sobre o back buffer.

As matrizes são `row_major` no HLSL e o shader usa `mul(vetor, matriz)`, consistente com a multiplicação `view * projection` enviada pelo DirectXMath. Se mudar convenção ou transposição sem revisar os dois lados, a câmera quebra.

## 8. HRESULT e depuração

Muitas APIs Win32 retornam zero ou um handle inválido em caso de erro: consulte `GetLastError` imediatamente. Direct3D normalmente retorna `HRESULT`: teste `FAILED(hr)`, não apenas igualdade com um código.

A função `check` transforma um HRESULT inválido numa exceção com a operação. Em Debug, tentamos usar a camada de diagnóstico do Direct3D; se ela não estiver instalada, tentamos novamente sem ela.

No Visual Studio:

1. Coloque breakpoint no começo de `App::run`.
2. Use F10 para avançar sem entrar em chamadas; F11 para entrar; Shift+F11 para sair.
3. Examine `world_.blocks.size()` e `player_.position` em Watch.
4. Coloque breakpoint em `World::set` e remova um bloco.
5. Examine `dirty`: só as regiões necessárias devem ser reconstruídas.
6. Abra Output para mensagens de debug; verifique `errors.log` para erros registrados.

Não deixe `/Od` ao medir desempenho final. Use Release com símbolos e o profiler do Visual Studio. Aumentar o mundo antes de medir amplifica limitações de meshing e desenho.

## 9. Separação de responsabilidades

`Core.hpp` não inclui `windows.h`. Ele cuida das regras. `Main.cpp` conhece a janela e a entrada. `Renderer` conhece GPU. `Storage` conhece os arquivos do sistema. Essa separação permitiu testar o núcleo em Linux sem fingir que isso testa o Windows.

Experimentos de aprendizagem, em ordem:

1. Mude uma cor base em `makeTextures` e recompile.
2. Altere a velocidade em `Player::step` e observe o passo fixo.
3. Adicione um teste de raycast antes de alterar o algoritmo.
4. Implemente uma textura separada para a copa; amplie o enum, validação e formato de save com migração.
5. Só então experimente expandir o mundo, introduzir streaming e refazer o formato de persistência.

Não aumente somente uma constante: dimensões também aparecem no índice de regiões, meshing e cabeçalho do save. Uma versão expansível deve centralizar essas dimensões e migrar os arquivos antigos.
