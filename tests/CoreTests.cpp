#include "../src/Core.hpp"
#include <iostream>
#include <string_view>
#include <cstdlib>
using namespace voxel;
namespace {int checks=0;void require(bool value,std::string_view name) {++checks;if(!value) {std::cerr<<"FAIL: "<<name<<'\n';std::exit(1);}}}
int main() {
    World w;w.generate();World other;other.generate();
    require(w.blocks==other.blocks,"deterministic terrain");
    require(w.get(-1,0,0)==Block::Air,"safe negative indexing");
    require(w.get(48,0,0)==Block::Air,"safe upper indexing");
    require(w.get(0,0,0)==Block::Stone,"bedrock generated");
    const auto before=w.blocks;w.set(-1,0,0,Block::Wood);require(w.blocks==before,"out of range write ignored");
    w.dirty.fill(false);w.set(15,25,15,Block::Stone);
    require(w.dirty[0]&&w.dirty[1]&&w.dirty[3]&&!w.dirty[4],"chunk seam invalidation");
    auto bytes=encode(w);World loaded;require(decode(loaded,bytes)&&loaded.blocks==w.blocks,"save roundtrip");
    const auto snapshot=loaded.blocks;
    bytes.back()^=1;require(!decode(loaded,bytes)&&loaded.blocks==snapshot,"checksum rejects without mutation");
    bytes=encode(w);bytes[3]=99;require(!decode(loaded,bytes),"future format rejected");
    bytes=encode(w);bytes.pop_back();require(!decode(loaded,bytes),"truncated save rejected");
    bytes=encode(w);bytes.push_back(0);require(!decode(loaded,bytes),"trailing bytes rejected");
    World invalid=w;invalid.blocks[0]=255;require(!decode(loaded,encode(invalid)),"invalid material rejected even with valid checksum");
    World empty;empty.set(3,3,3,Block::Stone);
    auto hit=empty.ray({3.5f,3.5f,0.5f},{0,0,1});
    require(hit&&hit->z==3&&hit->pz==2,"axis ray and placement neighbor");
    hit=empty.ray({7.5f,3.5f,3.5f},{-1,0,0});require(hit&&hit->x==3&&hit->px==4,"negative ray");
    require(!empty.ray({3.5f,3.5f,0.5f},{0,0,1},1),"reach limited");
    require(!empty.ray({0,0,0},{0,0,0}),"zero ray rejected");
    hit=empty.ray({3.5f,7.5f,3.5f},{0,-1,0});require(hit&&hit->y==3&&hit->py==4,"vertical ray");
    require(empty.collides({3.5f,3,3.5f}),"body intersects block");
    require(!empty.collides({3.5f,4,3.5f}),"body may stand on block");
    require(empty.collides({0,4,3}),"world boundary collision");
    World floor;for(int z=0;z<48;++z) for(int x=0;x<48;++x) floor.set(x,0,z,Block::Stone);
    Player p;p.position={10.5f,10,10.5f};
    for(int i=0;i<600;++i) p.step(floor,0,0,false,false,1.0f/120);
    require(p.grounded&&p.position.y>=1&&p.position.y<1.03f,"gravity lands without tunneling at fixed step");
    const float y=p.position.y;p.step(floor,0,0,true,false,1.0f/120);
    require(p.position.y>y&&!p.grounded,"jump leaves floor");
    for(int yy=1;yy<5;++yy) for(int x=0;x<48;++x) floor.set(x,yy,12,Block::Stone);
    p.position={10.5f,1.01f,10.5f};p.velocityY=0;
    for(int i=0;i<240;++i) p.step(floor,1,0,false,false,1.0f/120);
    require(p.position.z<11.71f,"horizontal wall blocks motion");
    p.flying=true;const float y0=p.position.y;p.step(floor,0,0,true,false,1.0f/120);
    require(p.position.y>y0,"flight ascends");
    std::cout<<checks<<" checks passed\n";
}
