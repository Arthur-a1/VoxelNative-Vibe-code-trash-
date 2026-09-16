#pragma once
#include "Core.hpp"
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <string>
namespace voxel {
template<class T> using ComPtr=Microsoft::WRL::ComPtr<T>;
inline void check(HRESULT hr,const char* operation) {
    if(FAILED(hr)) throw std::runtime_error(std::string(operation)+" HRESULT="+std::to_string(static_cast<unsigned long>(hr)));
}
struct Vertex { float x,y,z,u,v,layer,light; };
class Renderer {
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain1> swap_;
    ComPtr<ID3D11RenderTargetView> target_;
    ComPtr<ID3D11DepthStencilView> depth_;
    ComPtr<ID3D11VertexShader> vs_;
    ComPtr<ID3D11PixelShader> ps_;
    ComPtr<ID3D11VertexShader> hudVS_;
    ComPtr<ID3D11PixelShader> hudPS_;
    ComPtr<ID3D11InputLayout> layout_;
    ComPtr<ID3D11Buffer> constants_;
    ComPtr<ID3D11ShaderResourceView> texture_;
    ComPtr<ID3D11SamplerState> sampler_;
    ComPtr<ID3D11RasterizerState> raster_;
    struct Mesh { ComPtr<ID3D11Buffer> buffer; UINT count{}; };
    std::array<Mesh,9> meshes_;
    UINT width_=1,height_=1;
    void makeTextures();
    void rebuild(World& world);
public:
    Renderer(HWND window);
    void resize(UINT width,UINT height);
    void draw(World& world,const Player& player);
};
}
