# Propriedades e bibliotecas

`VoxelNative.vcxproj`, `CoreTests.vcxproj` e `Common.props` são a configuração executável de referência. As tabelas descrevem todos os valores deliberadamente definidos; as demais propriedades são os padrões herdados do MSVC/SDK selecionado. Não edite caminhos absolutos para a instalação de outra pessoa.

Abra **Projeto > Propriedades**. Escolha **Todas as Configurações** e **x64**, exceto onde indicado. A tradução dos rótulos pode variar; os nomes em inglês ajudam a localizar a opção.

## Geral e diretórios

| Página / propriedade | Valor entregue |
|---|---|
| Configuration Type | Application (.exe) |
| Platform | x64, sem configuração Win32/ARM64 |
| Platform Toolset | v145; retarget para v143 se usar VS 2022 |
| Windows SDK Version | 10.0: seleciona SDK instalado; fixe a versão exata antes de produção |
| Character Set | Use Unicode Character Set |
| Output Directory | `$(MSBuildThisFileDirectory)bin\$(Platform)\$(Configuration)\` em Common.props |
| Intermediate Directory | `$(MSBuildThisFileDirectory)obj\$(ProjectName)\$(Platform)\$(Configuration)\` |
| Target Name | Herdado: VoxelNative ou CoreTests |
| Use Debug Libraries | Sim em Debug; não em Release |
| Include / Library Directories | Padrões herdados do Visual C++ e Windows SDK |
| Additional Include Directories | Nenhum necessário; includes locais são relativos |
| Additional Library Directories | Nenhum necessário |
| Debugger Working Directory | Padrão herdado; não influencia assets nem saves |

## Compilador C/C++

| Propriedade | Todas / Debug / Release |
|---|---|
| Language > C++ Language Standard | Latest: `/std:c++latest` |
| Language > Conformance Mode | `/permissive-` |
| Command Line > Additional Options | `/utf-8 /Zc:__cplusplus /Zc:preprocessor` |
| Precompiled Headers | Not Using |
| Warning Level | `/W4` |
| Treat Warnings As Errors | Não nesta entrega; habilitar `/WX` após zerar avisos no toolset escolhido |
| SDL Checks | `/sdl` |
| Buffer Security Check | `/GS` |
| Control Flow Guard | `/guard:cf` |
| Exception Handling | `/EHsc` |
| Debug Information Format | `/Zi`, inclusive Release |
| Multi-processor Compilation | `/MP` |
| Optimization | Debug `/Od`; Release `/O2` |
| Runtime Library | Debug `/MTd`; Release `/MT` |
| Function Level Linking | Release `/Gy` |
| Intrinsic Functions | Release `/Oi` |
| Preprocessor Definitions | `UNICODE;_UNICODE;WIN32_LEAN_AND_MEAN;NOMINMAX;_WIN32_WINNT=0x0A00` |
| Definição por configuração | Debug `_DEBUG`; Release `NDEBUG` |
| Run Code Analysis | Sob demanda pelo script `-Analyze` |

`/MT` liga o runtime C++ estaticamente: este EXE não requer distribuir uma DLL própria de runtime MSVC. As DLLs do sistema continuam necessárias. Ao atualizar o runtime para correções, é necessário recompilar e redistribuir. Se adicionar DLLs C++ próprias, reavalie `/MD` e a propriedade de memória nas fronteiras entre módulos. Não misture Debug e Release nem runtimes incompatíveis.

`NOMINMAX` impede macros antigas de conflitar com `std::min`/`std::max`. `WIN32_LEAN_AND_MEAN` reduz o que `windows.h` inclui. `_WIN32_WINNT` define o alvo de APIs em tempo de compilação, não emula APIs num sistema antigo.

## Linker e recursos

| Propriedade | Valor |
|---|---|
| System > SubSystem | Windows para o jogo; Console para CoreTests |
| Entry Point | Vazio: o runtime chama `wWinMain` ou `main` |
| Enable Incremental Linking | Não |
| Generate Debug Info | `/DEBUG` nas duas configurações |
| Randomized Base Address | `/DYNAMICBASE` |
| Data Execution Prevention | `/NXCOMPAT` |
| High Entropy VA | `/HIGHENTROPYVA` |
| Additional Options | `/guard:cf` |
| Optimize References | Release `/OPT:REF` |
| Enable COMDAT Folding | Release `/OPT:ICF` |
| Input > Additional Dependencies | Bibliotecas abaixo, preservando `%(AdditionalDependencies)` |
| Manifest Tool > Additional Manifest Files | `app.manifest` |
| Resource Compiler | `app.rc`, metadados de versão 0.1.0 |
| UAC | `asInvoker`: sem elevação |
| DPI awareness | `PerMonitorV2`, no manifesto |
| Long paths | `longPathAware`, no manifesto |

O PDB de Release ajuda a investigar falhas. Guarde-o associado ao executável; não é necessário entregá-lo a todo jogador. Não há ícone personalizado nem instalador nesta versão.

## Cada biblioteca: inclusão e função

Não há biblioteca de terceiros para instalar. No Visual Studio Installer instale MSVC e Windows SDK pela carga de desktop C++. Os caminhos de headers e `.lib` ficam disponíveis pelo toolset.

| Componente | Header no código | Linker > Input > Additional Dependencies | Função |
|---|---|---|---|
| Win32/User32 | `windows.h` | `user32.lib` | Janela, mensagens, teclado, Raw Input e caixas de diálogo |
| Kernel32 | `windows.h` | `kernel32.lib`, herdada pelos projetos MSVC | Arquivos, sincronização, mensagens de depuração |
| Direct3D 11 | `d3d11.h` | `d3d11.lib` | Dispositivo, buffers, texturas e desenho |
| DXGI | `dxgi1_2.h` | `dxgi.lib` | Adaptador e swap chain |
| Compilador HLSL | `d3dcompiler.h` | `d3dcompiler.lib` | Compila shaders embutidos na inicialização |
| Shell | `shlobj.h` | `shell32.lib` | Localiza LocalAppData |
| COM allocator | `windows.h` | `ole32.lib` | Libera memória de Known Folder |
| GUIDs do SDK | Headers do SDK | `uuid.lib` | Identificadores como FOLDERID_LocalAppData |
| DirectXMath | `DirectXMath.h` | Nenhuma biblioteca adicional | Matrizes e vetores, implementados em headers |
| WRL | `wrl/client.h` | Nenhuma biblioteca adicional | `ComPtr`, gerenciamento de referências COM |
| Biblioteca padrão C++ | `vector`, `span`, etc. | Selecionada pelo MSVC via `/MT[d]` | Contêineres, tempo, erros e filesystem |

No campo **Additional Dependencies**, entregue:

```text
d3d11.lib;dxgi.lib;d3dcompiler.lib;user32.lib;shell32.lib;ole32.lib;uuid.lib;%(AdditionalDependencies)
```

Não substitua as dependências herdadas por uma lista vazia. Não baixe DLLs avulsas de sites de terceiros. `d3dcompiler_47.dll` é a dependência de sistema usada pelo import lib no alvo Windows 10/11. Para distribuição futura, prefira shaders pré-compilados pelo SDK e remova a compilação HLSL do caminho de inicialização.

## Criar manualmente um projeto equivalente

1. Crie **Empty Project (C++)**, com o nome VoxelNative e plataforma x64.
2. Adicione `src/Main.cpp` e `src/Renderer.cpp` como arquivos C/C++ existentes.
3. Adicione os três `.hpp` como headers; não os compile separadamente.
4. Adicione `app.rc` e configure `app.manifest` em Manifest Tool.
5. Importe `Common.props` no **Property Manager**, tanto em Debug como Release; ou copie as propriedades das tabelas.
6. Configure subsistema Windows e as bibliotecas acima. Não crie um `main` adicional: já existe `wWinMain`.
7. Crie outro projeto Console chamado CoreTests com somente `tests/CoreTests.cpp`. Importe Common.props. Ele não precisa das bibliotecas gráficas.
8. Compile ambos, execute CoreTests e inicie o jogo.

Erros comuns: `windows.h` ausente significa SDK/carga ausente; LNK2019 em D3D11 significa biblioteca não ligada; erro sobre `main` indica subsistema incorreto; erro sobre `std::span` indica padrão de linguagem antigo; MSB8036 indica SDK não instalado. Em máquinas sem a camada de debug D3D11, o código tenta criar o dispositivo novamente sem essa camada.
