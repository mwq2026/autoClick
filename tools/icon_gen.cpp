// icon_gen.cpp - Multi-size ICO with 4x4 SSAA anti-aliasing
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
static void WriteU16(std::ofstream& out, uint16_t v) {
    out.put((char)(v & 0xFF)); out.put((char)((v >> 8) & 0xFF));
}
static void WriteU32(std::ofstream& out, uint32_t v) {
    out.put((char)(v&0xFF)); out.put((char)((v>>8)&0xFF));
    out.put((char)((v>>16)&0xFF)); out.put((char)((v>>24)&0xFF));
}
struct Color { float r,g,b,a; };
static Color Blend(Color d, Color s) {
    float oa = s.a + d.a*(1.f-s.a);
    if(oa<1e-6f) return {0,0,0,0};
    return {(s.r*s.a+d.r*d.a*(1.f-s.a))/oa,
            (s.g*s.a+d.g*d.a*(1.f-s.a))/oa,
            (s.b*s.a+d.b*d.a*(1.f-s.a))/oa, oa};
}
static float Clamp01(float v){return v<0.f?0.f:(v>1.f?1.f:v);}
static float Smooth(float e0,float e1,float x){
    float t=Clamp01((x-e0)/(e1-e0)); return t*t*(3.f-2.f*t);
}
static float SdTri(float px,float py,
    float ax,float ay,float bx,float by,float cx,float cy){
    auto cr=[](float ox,float oy,float ex,float ey){return ox*ey-oy*ex;};
    float d1=cr(px-ax,py-ay,bx-ax,by-ay);
    float d2=cr(px-bx,py-by,cx-bx,cy-by);
    float d3=cr(px-cx,py-cy,ax-cx,ay-cy);
    bool in=(d1>=0&&d2>=0&&d3>=0)||(d1<=0&&d2<=0&&d3<=0);
    auto ds=[](float px,float py,float ax,float ay,float bx,float by){
        float dx=bx-ax,dy=by-ay,l2=dx*dx+dy*dy;
        float t=Clamp01(((px-ax)*dx+(py-ay)*dy)/l2);
        float ex=ax+t*dx-px,ey=ay+t*dy-py;
        return sqrtf(ex*ex+ey*ey);
    };
    float m=ds(px,py,ax,ay,bx,by);
    m=std::min(m,ds(px,py,bx,by,cx,cy));
    m=std::min(m,ds(px,py,cx,cy,ax,ay));
    return in?-m:m;
}
static std::vector<uint32_t> RenderIcon(int sz){
    const int SS=4;
    const float inv=1.f/(float)(SS*SS);
    const float half=(float)sz*0.5f;
    const float R=half-1.0f;
    const float AA=sz>=128?1.2f:(sz>=32?0.8f:0.6f);
    const float asz=R*0.52f;
    const float tipX=-asz*0.42f,tipY=-asz*0.48f;
    const float blX=-asz*0.42f,blY=asz*0.55f;
    const float brX=asz*0.52f,brY=asz*0.06f;
    std::vector<uint32_t> px(sz*sz,0);
    for(int py=0;py<sz;++py){
        for(int ppx=0;ppx<sz;++ppx){
            float rr=0,gg=0,bb=0,aa=0;
            for(int sy=0;sy<SS;++sy){
                for(int sx=0;sx<SS;++sx){
                    float fx=(float)ppx+((float)sx+0.5f)/(float)SS;
                    float fy=(float)py+((float)sy+0.5f)/(float)SS;
                    float dx=fx-half,dy=fy-half;
                    float dist=sqrtf(dx*dx+dy*dy);
                    Color c={0,0,0,0};
                    float ca=1.f-Smooth(R-AA,R,dist);
                    if(ca>0.f){
                        float gt=Clamp01(dist/R);
                        float cr2=42.f+(75.f-42.f)*gt;
                        float cg=28.f+(52.f-28.f)*gt;
                        float cb=108.f+(165.f-108.f)*gt;
                        c=Blend(c,{cr2/255.f,cg/255.f,cb/255.f,ca});
                        float hx=dx+R*0.25f,hy=dy+R*0.25f;
                        float hd=sqrtf(hx*hx+hy*hy);
                        float hlA=(1.f-Smooth(0.f,R*0.50f,hd))*0.22f*ca;
                        if(hlA>0.f) c=Blend(c,{0.55f,0.45f,0.95f,hlA});
                        float rd=fabsf(dist-R*0.96f);
                        float ra=(1.f-Smooth(0.f,AA*1.2f,rd))*0.45f*ca;
                        if(ra>0.f) c=Blend(c,{0.58f,0.50f,1.f,ra});
                    }
                    if(ca>0.f){
                        float sd=SdTri(dx,dy,tipX,tipY,blX,blY,brX,brY);
                        float ow=AA*2.0f;
                        float oa2=(1.f-Smooth(0.f,ow,fabsf(sd+ow*0.5f)))*0.6f;
                        if(sd>0.f) oa2*=(1.f-Smooth(0.f,ow,sd));
                        if(oa2>0.f) c=Blend(c,{0.08f,0.04f,0.22f,oa2*ca});
                        float fa=1.f-Smooth(-AA*0.4f,AA*0.4f,sd);
                        if(fa>0.f) c=Blend(c,{0.94f,0.97f,1.f,fa*ca});
                    }
                    rr+=c.r*c.a; gg+=c.g*c.a; bb+=c.b*c.a; aa+=c.a;
                }
            }
            rr*=inv; gg*=inv; bb*=inv; aa*=inv;
            if(aa>1e-6f){rr/=aa;gg/=aa;bb/=aa;}
            auto to8=[](float v)->uint8_t{
                int i=(int)(v*255.f+0.5f);
                return (uint8_t)(i<0?0:(i>255?255:i));
            };
            px[py*sz+ppx]=((uint32_t)to8(aa)<<24)|
                ((uint32_t)to8(rr)<<16)|((uint32_t)to8(gg)<<8)|(uint32_t)to8(bb);
        }
    }
    return px;
}
static std::vector<uint8_t> MakeBmp(const std::vector<uint32_t>& px,int sz){
    uint32_t ib=(uint32_t)(sz*sz*4);
    uint32_t as=(uint32_t)(((sz+31)/32)*4)*(uint32_t)sz;
    std::vector<uint8_t> b;
    b.reserve(40+ib+as);
    auto p32=[&](uint32_t v){b.push_back((uint8_t)v);b.push_back((uint8_t)(v>>8));
        b.push_back((uint8_t)(v>>16));b.push_back((uint8_t)(v>>24));};
    auto p16=[&](uint16_t v){b.push_back((uint8_t)v);b.push_back((uint8_t)(v>>8));};
    p32(40);p32((uint32_t)sz);p32((uint32_t)(sz*2));
    p16(1);p16(32);p32(0);p32(ib);p32(0);p32(0);p32(0);p32(0);
    for(int y=sz-1;y>=0;--y)
        for(int x=0;x<sz;++x){
            uint32_t c=px[y*sz+x];
            b.push_back((uint8_t)(c&0xFF));
            b.push_back((uint8_t)((c>>8)&0xFF));
            b.push_back((uint8_t)((c>>16)&0xFF));
            b.push_back((uint8_t)((c>>24)&0xFF));
        }
    b.resize(b.size()+as,0);
    return b;
}
int main(int argc,char** argv){
    if(argc<2) return 2;
    const int sizes[]={256,48,32,16};
    const int N=4;
    struct D{int sz;std::vector<uint8_t> d;};
    std::vector<D> imgs;
    for(int i=0;i<N;++i){
        auto p=RenderIcon(sizes[i]);
        imgs.push_back({sizes[i],MakeBmp(p,sizes[i])});
    }
    std::ofstream out(argv[1],std::ios::binary);
    if(!out.is_open()) return 3;
    WriteU16(out,0);WriteU16(out,1);WriteU16(out,(uint16_t)N);
    uint32_t off=6+N*16;
    for(int i=0;i<N;++i){
        uint8_t w=imgs[i].sz>=256?0:(uint8_t)imgs[i].sz;
        out.put((char)w);out.put((char)w);out.put(0);out.put(0);
        WriteU16(out,1);WriteU16(out,32);
        WriteU32(out,(uint32_t)imgs[i].d.size());
        WriteU32(out,off);
        off+=(uint32_t)imgs[i].d.size();
    }
    for(int i=0;i<N;++i)
        out.write((const char*)imgs[i].d.data(),(std::streamsize)imgs[i].d.size());
    out.close();
    return 0;
}