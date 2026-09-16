#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace voxel {
struct Vec3 { float x{}, y{}, z{}; };
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x*s,a.y*s,a.z*s}; }
inline int cell(float x) { return static_cast<int>(std::floor(x)); }
enum class Block : std::uint8_t { Air, Grass, Dirt, Stone, Wood, Sand };
struct Hit { int x,y,z,px,py,pz; };
class World {
public:
    static constexpr int width=48, height=32, chunk=16;
    static constexpr std::size_t volume=width*height*width;
    std::vector<std::uint8_t> blocks=std::vector<std::uint8_t>(volume);
    std::array<bool,9> dirty{};
    static bool inside(int x,int y,int z) { return x>=0&&x<width&&y>=0&&y<height&&z>=0&&z<width; }
    static std::size_t index(int x,int y,int z) { return static_cast<std::size_t>((y*width+z)*width+x); }
    Block get(int x,int y,int z) const { return inside(x,y,z)?static_cast<Block>(blocks[index(x,y,z)]):Block::Air; }
    void set(int x,int y,int z,Block b) {
        if(!inside(x,y,z)) return;
        blocks[index(x,y,z)]=static_cast<std::uint8_t>(b);
        for(auto d:std::array<std::array<int,2>,5>{{{0,0},{1,0},{-1,0},{0,1},{0,-1}}}) {
            const int xx=x+d[0],zz=z+d[1];
            if(inside(xx,y,zz)) dirty[static_cast<std::size_t>((zz/chunk)*3+xx/chunk)]=true;
        }
    }
    void generate() {
        std::fill(blocks.begin(),blocks.end(),0);
        for(int z=0;z<width;++z) for(int x=0;x<width;++x) {
            const int h=9+static_cast<int>(3*std::sin(x*0.17f)+2*std::cos(z*0.21f));
            for(int y=0;y<=h;++y) blocks[index(x,y,z)]=static_cast<std::uint8_t>(y==h?Block::Grass:(y>h-3?Block::Dirt:Block::Stone));
        }
        for(int z=7;z<width-5;z+=13) for(int x=6;x<width-5;x+=15) {
            int y=height-1; while(get(x,y,z)==Block::Air) --y;
            for(int i=1;i<=4;++i) set(x,y+i,z,Block::Wood);
            // Stylized green canopy uses grass; no separate leaf material yet.
            for(int dz=-2;dz<=2;++dz) for(int dx=-2;dx<=2;++dx)
                if(std::abs(dx)+std::abs(dz)<=3) set(x+dx,y+5,z+dz,Block::Grass);
        }
        dirty.fill(true);
    }
    bool solid(int x,int y,int z) const {
        return x<0||x>=width||z<0||z>=width||y<0||get(x,y,z)!=Block::Air;
    }
    // Body position is the center of the feet; world edges are collision walls.
    bool collides(Vec3 p) const {
        for(int y=cell(p.y+0.001f);y<=cell(p.y+1.799f);++y)
            for(int z=cell(p.z-0.299f);z<=cell(p.z+0.299f);++z)
                for(int x=cell(p.x-0.299f);x<=cell(p.x+0.299f);++x)
                    if(solid(x,y,z)) return true;
        return false;
    }
    std::optional<Hit> ray(Vec3 o,Vec3 d,float reach=7) const {
        const float length=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
        if(length<0.00001f) return std::nullopt;
        d=d*(1/length);
        int x=cell(o.x),y=cell(o.y),z=cell(o.z),px=x,py=y,pz=z;
        const int sx=d.x>0?1:-1,sy=d.y>0?1:-1,sz=d.z>0?1:-1;
        const float inf=std::numeric_limits<float>::infinity();
        const float dx=d.x==0?inf:std::abs(1/d.x),dy=d.y==0?inf:std::abs(1/d.y),dz=d.z==0?inf:std::abs(1/d.z);
        float tx=d.x==0?inf:(static_cast<float>(x+(sx>0))-o.x)/d.x;
        float ty=d.y==0?inf:(static_cast<float>(y+(sy>0))-o.y)/d.y;
        float tz=d.z==0?inf:(static_cast<float>(z+(sz>0))-o.z)/d.z;
        float t=0;
        while(t<=reach) {
            if(get(x,y,z)!=Block::Air) return Hit{x,y,z,px,py,pz};
            px=x;py=y;pz=z;
            if(tx<=ty&&tx<=tz) { t=tx;tx+=dx;x+=sx; }
            else if(ty<=tz) { t=ty;ty+=dy;y+=sy; }
            else { t=tz;tz+=dz;z+=sz; }
        }
        return std::nullopt;
    }
};
struct Player {
    Vec3 position{24.5f,24,24.5f};
    float yaw{},pitch{},velocityY{};
    bool flying{},grounded{};
    Vec3 direction() const { return {std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)}; }
    Vec3 eye() const { return position+Vec3{0,1.62f,0}; }
    void step(const World& world,float forward,float right,bool jump,bool down,float dt) {
        const float n=std::sqrt(forward*forward+right*right);
        if(n>1) { forward/=n;right/=n; }
        const float speed=flying?10.0f:5.0f;
        Vec3 delta{(std::sin(yaw)*forward+std::cos(yaw)*right)*speed*dt,0,
                   (std::cos(yaw)*forward-std::sin(yaw)*right)*speed*dt};
        if(flying) { velocityY=0;delta.y=(static_cast<float>(jump)-static_cast<float>(down))*speed*dt; }
        else { if(jump&&grounded) velocityY=8;velocityY=std::max(-30.0f,velocityY-24*dt);delta.y=velocityY*dt; }
        Vec3 p=position;p.x+=delta.x;if(!world.collides(p)) position=p;
        p=position;p.z+=delta.z;if(!world.collides(p)) position=p;
        grounded=false;p=position;p.y+=delta.y;
        if(!world.collides(p)) position=p;
        else {grounded=delta.y<0;velocityY=0;}
    }
};
// Versioned, fixed-size byte format. No struct dumps, pointers or platform padding.
inline std::uint32_t checksum(std::span<const std::uint8_t> data) {
    std::uint32_t h=2166136261u; for(auto b:data) {h^=b;h*=16777619u;} return h;
}
inline std::vector<std::uint8_t> encode(const World& w) {
    std::vector<std::uint8_t> out{'V','X','N',1,48,32,48,0};
    const auto h=checksum(w.blocks);
    for(int i=0;i<4;++i) out.push_back(static_cast<std::uint8_t>(h>>(i*8)));
    out.insert(out.end(),w.blocks.begin(),w.blocks.end());return out;
}
inline bool decode(World& w,std::span<const std::uint8_t> bytes) {
    if(bytes.size()!=World::volume+12) return false;
    constexpr std::array<std::uint8_t,8> header{'V','X','N',1,48,32,48,0};
    if(!std::equal(header.begin(),header.end(),bytes.begin())) return false;
    std::uint32_t h=0;for(int i=0;i<4;++i) h|=static_cast<std::uint32_t>(bytes[static_cast<std::size_t>(8+i)])<<(i*8);
    const auto payload=bytes.subspan(12);
    if(checksum(payload)!=h||std::any_of(payload.begin(),payload.end(),[](auto b){return b>5;})) return false;
    std::copy(payload.begin(),payload.end(),w.blocks.begin());w.dirty.fill(true);return true;
}
}
