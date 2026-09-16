#include "Renderer.hpp"
#include "Storage.hpp"
#include <chrono>
#include <memory>
#include <string>

namespace voxel {
class App {
    HWND window_{};
    World world_;
    Player player_;
    std::unique_ptr<Renderer> renderer_;
    std::filesystem::path path_;
    std::array<bool,256> keys_{};
    bool captured_{},active_=true,minimized_{},resize_=true,saveRequested_{},changed_{};
    UINT width_=1280,height_=720;
    Block selected_=Block::Grass;
    std::string callbackError_;
    std::wstring status_=L"Pronto";
    void clip() {
        RECT r{};GetClientRect(window_,&r);
        POINT a{r.left,r.top},b{r.right,r.bottom};ClientToScreen(window_,&a);ClientToScreen(window_,&b);
        r={a.x,a.y,b.x,b.y};ClipCursor(&r);
    }
    void capture(bool value) {
        if(value==captured_) return;
        captured_=value;
        if(value) {SetCapture(window_);clip();ShowCursor(FALSE);}
        else {ClipCursor(nullptr);ReleaseCapture();ShowCursor(TRUE);keys_.fill(false);}
    }
    void edit(bool place) {
        const auto hit=world_.ray(player_.eye(),player_.direction());if(!hit) return;
        if(place) {
            if(!World::inside(hit->px,hit->py,hit->pz)||world_.get(hit->px,hit->py,hit->pz)!=Block::Air) return;
            world_.set(hit->px,hit->py,hit->pz,selected_);
            if(world_.collides(player_.position)) {world_.set(hit->px,hit->py,hit->pz,Block::Air);return;}
        } else {if(hit->y==0) return;world_.set(hit->x,hit->y,hit->z,Block::Air);}
        changed_=true;
    }
    LRESULT message(UINT msg,WPARAM w,LPARAM l) {
        switch(msg) {
        case WM_CLOSE:
            if(changed_) {trySave();if(changed_) return 0;}
            DestroyWindow(window_);return 0;
        case WM_DESTROY: capture(false);PostQuitMessage(0);return 0;
        case WM_ERASEBKGND:return 1;
        case WM_SIZE:
            minimized_=w==SIZE_MINIMIZED;width_=LOWORD(l);height_=HIWORD(l);resize_=true;return 0;
        case WM_MOVE:if(captured_) clip();return 0;
        case WM_DPICHANGED: {
            const auto* r=reinterpret_cast<const RECT*>(l);
            SetWindowPos(window_,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;
        }
        case WM_ACTIVATEAPP:active_=w!=0;if(!active_) capture(false);return 0;
        case WM_CAPTURECHANGED:if(reinterpret_cast<HWND>(l)!=window_) capture(false);return 0;
        case WM_KEYDOWN:
            if(w<keys_.size()) keys_[w]=true;
            if((l&(1LL<<30))==0) {
                if(w==VK_ESCAPE) capture(false);
                if(w=='F') {player_.flying=!player_.flying;player_.velocityY=0;}
                if(w==VK_F5) saveRequested_=true;
                if(w>='1'&&w<='5') selected_=static_cast<Block>(w-'0');
                if(w==VK_F1) {capture(false);MessageBoxW(window_,L"Clique: capturar mouse\nWASD: mover | Mouse: olhar\nEspaco: pular/subir | Ctrl: descer em voo\nF: alternar voo | 1-5: material\nBotao esquerdo: remover | Direito: colocar\nF5: salvar | Esc: liberar mouse\nO mundo salva ao sair e a cada 30 s se alterado.",L"VoxelNative - controles",MB_OK);}
            }
            return 0;
        case WM_KEYUP:if(w<keys_.size()) keys_[w]=false;return 0;
        case WM_LBUTTONDOWN:if(!captured_) capture(true);else edit(false);return 0;
        case WM_RBUTTONDOWN:if(captured_) edit(true);return 0;
        case WM_INPUT: {
            if(captured_) {
                RAWINPUT input{};UINT size=sizeof(input);
                const auto result=GetRawInputData(reinterpret_cast<HRAWINPUT>(l),RID_INPUT,&input,&size,sizeof(RAWINPUTHEADER));
                if(result!=static_cast<UINT>(-1)&&input.header.dwType==RIM_TYPEMOUSE&&(input.data.mouse.usFlags&MOUSE_MOVE_ABSOLUTE)==0) {
                    player_.yaw+=static_cast<float>(input.data.mouse.lLastX)*0.0025f;
                    player_.yaw=std::remainder(player_.yaw,6.283185307f);
                    player_.pitch=std::clamp(player_.pitch-static_cast<float>(input.data.mouse.lLastY)*0.0025f,-1.54f,1.54f);
                }
            }
            break; // DefWindowProc performs foreground raw-input cleanup.
        }
        }
        return DefWindowProcW(window_,msg,w,l);
    }
    static LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM w,LPARAM l) noexcept {
        auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(msg==WM_NCCREATE) {
            app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            app->window_=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));
        }
        if(!app) return DefWindowProcW(hwnd,msg,w,l);
        try {return app->message(msg,w,l);}
        catch(const std::exception& e) {
            logError(e.what());
            try {app->callbackError_=e.what();} catch(...) {}
            PostQuitMessage(1);return 0;
        } catch(...) {logError("Unknown window callback failure");PostQuitMessage(1);return 0;}
    }
    void trySave() {
        try {save(world_,path_);changed_=false;status_=L"Salvo";}
        catch(const std::exception& e) {logError(e.what());status_=L"Falha ao salvar: F5 tenta novamente";capture(false);MessageBoxA(IsWindow(window_)?window_:nullptr,e.what(),"Falha ao salvar",MB_ICONERROR);}
    }
public:
    ~App() {capture(false);if(window_&&IsWindow(window_)) DestroyWindow(window_);}
    int run(HINSTANCE instance,int show) {
        path_=savePath();world_.generate();
        // A corrupt save aborts startup; never silently overwrite it with a fresh world.
        const bool loaded=load(world_,path_);changed_=!loaded;
        player_.position.y=static_cast<float>(World::height)+1;
        WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=procedure;wc.hInstance=instance;
        wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"VoxelNativeWindow";
        if(!RegisterClassExW(&wc)) throw ioError("Register window class");
        RECT rect{0,0,1280,720};AdjustWindowRectEx(&rect,WS_OVERLAPPEDWINDOW,FALSE,0);
        if(!CreateWindowExW(0,wc.lpszClassName,L"VoxelNative | F1: ajuda",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,instance,this)) throw ioError("Create window");
        const RAWINPUTDEVICE mouse{0x01,0x02,0,window_};
        if(!RegisterRawInputDevices(&mouse,1,sizeof(mouse))) throw ioError("Register raw mouse");
        renderer_=std::make_unique<Renderer>(window_);
        ShowWindow(window_,show);
        using Clock=std::chrono::steady_clock;
        auto previous=Clock::now(),lastSave=previous,lastTitle=previous;
        double accumulator=0;constexpr float step=1.0f/120.0f;
        MSG msg{};bool running=true;
        while(running) {
            while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
                if(msg.message==WM_QUIT) {running=false;break;}
                TranslateMessage(&msg);DispatchMessageW(&msg);
            }
            if(!running) break;
            const auto now=Clock::now();
            const double elapsed=std::min(0.1,std::chrono::duration<double>(now-previous).count());previous=now;
            if(minimized_||!active_) {accumulator=0;MsgWaitForMultipleObjectsEx(0,nullptr,100,QS_ALLINPUT,MWMO_INPUTAVAILABLE);continue;}
            if(resize_) {renderer_->resize(width_,height_);resize_=false;if(captured_) clip();}
            if(saveRequested_||(changed_&&now-lastSave>std::chrono::seconds(30))) {trySave();saveRequested_=false;lastSave=now;}
            // Releasing the mouse also pauses simulation.
            if(captured_) {
                accumulator+=elapsed;
                while(accumulator>=step) {
                    player_.step(world_,static_cast<float>(keys_['W'])-static_cast<float>(keys_['S']),static_cast<float>(keys_['D'])-static_cast<float>(keys_['A']),keys_[VK_SPACE],keys_[VK_CONTROL],step);
                    accumulator-=step;
                }
            } else accumulator=0;
            renderer_->draw(world_,player_);
            if(now-lastTitle>std::chrono::milliseconds(250)) {
                constexpr const wchar_t* names[]{L"Ar",L"Grama",L"Terra",L"Pedra",L"Madeira",L"Areia"};
                auto title=std::wstring(L"VoxelNative | ")+names[static_cast<unsigned>(selected_)]+(player_.flying?L" | Voo":L" | Caminhada")+L" | "+status_+L" | F1: ajuda | Clique para jogar";
                SetWindowTextW(window_,title.c_str());lastTitle=now;
            }
        }
        if(changed_) trySave();
        if(!callbackError_.empty()) throw std::runtime_error(callbackError_);
        return static_cast<int>(msg.wParam);
    }
};
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    try {
        // Prevent concurrent writers without requiring administrator privileges.
        HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\VoxelNative.World.Writer");
        if(!mutex) throw voxel::ioError("Create instance mutex");
        const DWORD error=GetLastError();
        struct MutexOwner {HANDLE h;~MutexOwner(){CloseHandle(h);}} owner{mutex};
        if(error==ERROR_ALREADY_EXISTS) {MessageBoxW(nullptr,L"VoxelNative ja esta aberto nesta sessao.",L"VoxelNative",MB_OK);return 0;}
        voxel::App app;return app.run(instance,show);
    } catch(const std::exception& e) {voxel::logError(e.what());MessageBoxA(nullptr,e.what(),"VoxelNative - erro",MB_ICONERROR);return 1;}
    catch(...) {MessageBoxW(nullptr,L"Falha inesperada.",L"VoxelNative",MB_ICONERROR);return 1;}
}
