#include "Renderer.hpp"
#include <d3dcompiler.h>
#include <utility>
namespace voxel {
namespace {
constexpr char shader[]=R"(
cbuffer Camera : register(b0) { row_major float4x4 vp; float4 eye; float4 screen; };
struct Input { float3 p:POSITION; float2 uv:TEXCOORD0; float layer:TEXCOORD1; float light:TEXCOORD2; };
struct Output { float4 p:SV_POSITION; float2 uv:TEXCOORD0; nointerpolation float layer:TEXCOORD1; float light:TEXCOORD2; float distance:TEXCOORD3; };
Output VS(Input v) { Output o; o.p=mul(float4(v.p,1),vp);o.uv=v.uv;o.layer=v.layer;o.light=v.light;o.distance=length(v.p-eye.xyz);return o; }
Texture2DArray tiles:register(t0); SamplerState pointSampler:register(s0);
float4 PS(Output v):SV_TARGET {
 float3 c=tiles.Sample(pointSampler,float3(v.uv,v.layer)).rgb*v.light;
 c=lerp(c,float3(0.48,0.70,0.88),saturate((v.distance-28)/45));
 return float4(c,1);
}
// Fullscreen triangle: crosshair works over sky as well as blocks.
float4 HUDVS(uint id:SV_VertexID):SV_POSITION {
 float2 p=float2((id<<1)&2,id&2); return float4(p*float2(2,-2)+float2(-1,1),0,1);
}
float4 HUDPS(float4 p:SV_POSITION):SV_TARGET {
 float2 d=abs(p.xy-screen.xy*0.5);
 if(!((d.x<1&&d.y<7)||(d.y<1&&d.x<7))) discard;
 return float4(1,1,1,1);
}
)";
ComPtr<ID3DBlob> compile(const char* entry,const char* target) {
    ComPtr<ID3DBlob> code,errors;
    UINT flags=D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags|=D3DCOMPILE_DEBUG|D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags|=D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
    const auto hr=D3DCompile(shader,sizeof(shader)-1,"embedded.hlsl",nullptr,nullptr,entry,target,flags,0,&code,&errors);
    if(FAILED(hr)) throw std::runtime_error(errors?static_cast<const char*>(errors->GetBufferPointer()):"Shader compilation failed");
    return code;
}
struct alignas(16) Camera { DirectX::XMFLOAT4X4 vp;DirectX::XMFLOAT4 eye,screen; };
static_assert(sizeof(Camera)==96);
}
Renderer::Renderer(HWND window) {
    UINT flags=0;
#ifdef _DEBUG
    flags|=D3D11_CREATE_DEVICE_DEBUG;
#endif
    constexpr D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0};
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,levels,1,D3D11_SDK_VERSION,&device_,nullptr,&context_);
    if(hr==DXGI_ERROR_SDK_COMPONENT_MISSING) {
        flags&=~D3D11_CREATE_DEVICE_DEBUG;
        hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,levels,1,D3D11_SDK_VERSION,&device_,nullptr,&context_);
    }
    if(FAILED(hr)) {
        device_.Reset();context_.Reset();
        check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,levels,1,D3D11_SDK_VERSION,&device_,nullptr,&context_),"Create D3D11 device (hardware/WARP)");
    }
    ComPtr<IDXGIDevice> dxgi;check(device_.As(&dxgi),"Query DXGI device");
    ComPtr<IDXGIAdapter> adapter;check(dxgi->GetAdapter(&adapter),"Get adapter");
    ComPtr<IDXGIFactory2> factory;check(adapter->GetParent(IID_PPV_ARGS(&factory)),"Get factory");
    DXGI_SWAP_CHAIN_DESC1 desc{};desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    check(factory->CreateSwapChainForHwnd(device_.Get(),window,&desc,nullptr,nullptr,&swap_),"Create flip swap chain");
    check(factory->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER),"Window association");
    const auto vs=compile("VS","vs_5_0"),ps=compile("PS","ps_5_0");
    check(device_->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vs_),"Vertex shader");
    check(device_->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&ps_),"Pixel shader");
    const auto hudVS=compile("HUDVS","vs_5_0"),hudPS=compile("HUDPS","ps_5_0");
    check(device_->CreateVertexShader(hudVS->GetBufferPointer(),hudVS->GetBufferSize(),nullptr,&hudVS_),"HUD vertex shader");
    check(device_->CreatePixelShader(hudPS->GetBufferPointer(),hudPS->GetBufferSize(),nullptr,&hudPS_),"HUD pixel shader");
    constexpr D3D11_INPUT_ELEMENT_DESC elements[]{
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",1,DXGI_FORMAT_R32_FLOAT,0,20,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",2,DXGI_FORMAT_R32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
    check(device_->CreateInputLayout(elements,4,vs->GetBufferPointer(),vs->GetBufferSize(),&layout_),"Input layout");
    D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(Camera);cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    check(device_->CreateBuffer(&cb,nullptr,&constants_),"Camera buffer");
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
    check(device_->CreateRasterizerState(&raster,&raster_),"Rasterizer");
    makeTextures();
    RECT r{};if(!GetClientRect(window,&r)) throw std::runtime_error("GetClientRect failed");
    resize(static_cast<UINT>(r.right),static_cast<UINT>(r.bottom));
}
void Renderer::makeTextures() {
    // Five original procedural 16x16 materials. Array slices prevent atlas bleeding.
    std::array<std::array<std::uint32_t,256>,5> pixels{};
    constexpr int colors[5][3]{{79,143,54},{130,88,51},{122,128,137},{112,73,40},{218,201,135}};
    std::array<D3D11_SUBRESOURCE_DATA,5> data{};
    for(std::size_t layer=0;layer<5;++layer) {
        for(int y=0;y<16;++y) for(int x=0;x<16;++x) {
            auto hash=static_cast<std::uint32_t>(x+y*16+layer*7919);hash^=hash<<13;hash*=1274126177u;hash^=hash>>16;
            int variation=static_cast<int>(hash%31)-15;
            if(layer==3) variation+=(x%4==0?-25:8);
            std::uint32_t color=0xff000000u;
            for(int c=0;c<3;++c) color|=static_cast<std::uint32_t>(std::clamp(colors[layer][c]+variation,0,255))<<(c*8);
            pixels[layer][static_cast<std::size_t>(y*16+x)]=color;
        }
        data[layer].pSysMem=pixels[layer].data();data[layer].SysMemPitch=16*4;
    }
    D3D11_TEXTURE2D_DESC desc{};desc.Width=16;desc.Height=16;desc.MipLevels=1;desc.ArraySize=5;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> tex;check(device_->CreateTexture2D(&desc,data.data(),&tex),"Textures");
    check(device_->CreateShaderResourceView(tex.Get(),nullptr,&texture_),"Texture view");
    D3D11_SAMPLER_DESC s{};s.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;s.MaxLOD=D3D11_FLOAT32_MAX;s.ComparisonFunc=D3D11_COMPARISON_NEVER;
    check(device_->CreateSamplerState(&s,&sampler_),"Sampler");
}
void Renderer::resize(UINT w,UINT h) {
    if(w==0||h==0) return;
    width_=w;height_=h;context_->OMSetRenderTargets(0,nullptr,nullptr);target_.Reset();depth_.Reset();
    check(swap_->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0),"Resize buffers");
    ComPtr<ID3D11Texture2D> back;check(swap_->GetBuffer(0,IID_PPV_ARGS(&back)),"Back buffer");
    check(device_->CreateRenderTargetView(back.Get(),nullptr,&target_),"Render target");
    D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;d.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> texture;check(device_->CreateTexture2D(&d,nullptr,&texture),"Depth texture");
    check(device_->CreateDepthStencilView(texture.Get(),nullptr,&depth_),"Depth view");
}
void Renderer::rebuild(World& w) {
    constexpr int normals[6][3]{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    constexpr float corners[6][4][3]{
        {{1,0,0},{1,0,1},{1,1,1},{1,1,0}},{{0,0,1},{0,0,0},{0,1,0},{0,1,1}},
        {{0,1,0},{1,1,0},{1,1,1},{0,1,1}},{{0,0,1},{1,0,1},{1,0,0},{0,0,0}},
        {{1,0,1},{0,0,1},{0,1,1},{1,1,1}},{{0,0,0},{1,0,0},{1,1,0},{0,1,0}}};
    constexpr float uv[4][2]{{0,1},{1,1},{1,0},{0,0}};
    constexpr int order[]{0,1,2,0,2,3};constexpr float light[]{0.78f,0.64f,1,0.45f,0.86f,0.70f};
    for(int cz=0;cz<3;++cz) for(int cx=0;cx<3;++cx) {
        const auto id=static_cast<std::size_t>(cz*3+cx);if(!w.dirty[id]) continue;
        std::vector<Vertex> vertices;
        for(int y=0;y<World::height;++y) for(int z=cz*16;z<(cz+1)*16;++z) for(int x=cx*16;x<(cx+1)*16;++x) {
            const auto b=w.get(x,y,z);if(b==Block::Air) continue;
            for(int f=0;f<6;++f) {
                if(w.get(x+normals[f][0],y+normals[f][1],z+normals[f][2])!=Block::Air) continue;
                const float tile=static_cast<float>(static_cast<int>(b)-1);
                for(auto k:order) vertices.push_back({static_cast<float>(x)+corners[f][k][0],static_cast<float>(y)+corners[f][k][1],static_cast<float>(z)+corners[f][k][2],uv[k][0],uv[k][1],tile,light[f]});
            }
        }
        Mesh next{};next.count=static_cast<UINT>(vertices.size());
        if(!vertices.empty()) {
            D3D11_BUFFER_DESC d{};d.ByteWidth=static_cast<UINT>(vertices.size()*sizeof(Vertex));d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_VERTEX_BUFFER;
            D3D11_SUBRESOURCE_DATA data{};data.pSysMem=vertices.data();
            check(device_->CreateBuffer(&d,&data,&next.buffer),"Chunk mesh");
        }
        meshes_[id]=std::move(next);w.dirty[id]=false;
    }
}
void Renderer::draw(World& w,const Player& player) {
    rebuild(w);
    using namespace DirectX;
    const auto p=player.eye(),d=player.direction();
    const auto view=XMMatrixLookToLH(XMVectorSet(p.x,p.y,p.z,1),XMVectorSet(d.x,d.y,d.z,0),XMVectorSet(0,1,0,0));
    const auto proj=XMMatrixPerspectiveFovLH(XM_PIDIV4,static_cast<float>(width_)/static_cast<float>(height_),0.05f,100);
    Camera c{};XMStoreFloat4x4(&c.vp,view*proj);c.eye={p.x,p.y,p.z,0};c.screen={static_cast<float>(width_),static_cast<float>(height_),0,0};
    context_->UpdateSubresource(constants_.Get(),0,nullptr,&c,0,0);
    constexpr float sky[]{0.48f,0.70f,0.88f,1};context_->ClearRenderTargetView(target_.Get(),sky);
    context_->ClearDepthStencilView(depth_.Get(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);
    context_->OMSetRenderTargets(1,target_.GetAddressOf(),depth_.Get());context_->RSSetState(raster_.Get());
    const D3D11_VIEWPORT viewport{0,0,static_cast<float>(width_),static_cast<float>(height_),0,1};context_->RSSetViewports(1,&viewport);
    context_->IASetInputLayout(layout_.Get());context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vs_.Get(),nullptr,0);context_->PSSetShader(ps_.Get(),nullptr,0);
    context_->VSSetConstantBuffers(0,1,constants_.GetAddressOf());context_->PSSetConstantBuffers(0,1,constants_.GetAddressOf());
    context_->PSSetShaderResources(0,1,texture_.GetAddressOf());context_->PSSetSamplers(0,1,sampler_.GetAddressOf());
    constexpr UINT stride=sizeof(Vertex),offset=0;
    for(const auto& m:meshes_) if(m.count) {context_->IASetVertexBuffers(0,1,m.buffer.GetAddressOf(),&stride,&offset);context_->Draw(m.count,0);}
    // HUD shader objects belong to this device and must be rebuilt on device loss.
    context_->OMSetRenderTargets(1,target_.GetAddressOf(),nullptr);
    context_->IASetInputLayout(nullptr);
    context_->VSSetShader(hudVS_.Get(),nullptr,0);context_->PSSetShader(hudPS_.Get(),nullptr,0);context_->Draw(3,0);
    const auto result=swap_->Present(1,0);
    check(result,"Present (device removed: restart application)");
    if(result==DXGI_STATUS_OCCLUDED) Sleep(50);
}
}
