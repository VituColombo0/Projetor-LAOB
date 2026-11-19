/* Análise de uma gravação do dedo apontado (comando "gestoGrava <s>" do SARndbox; pasta /dev/shm/gesto-*).
   Roda o MESMO detector do SARndbox (sarndbox/Gesto.cpp) quadro a quadro, com o mesmo caminho do Sandbox
   (procura, posição na caixa, beirada, mão aberta da chuva, tempo) e imprime, por quadro, os candidatos, as
   medidas e o motivo da recusa; no fim um resumo (quantas vezes confirmaria, os motivos mais comuns) e PNGs
   de alguns quadros para uma pessoa ver: altura sobre a areia (cinza = areia, azul -> vermelho = 1,5 a 25 cm
   acima dela, roxo = sem medida), cruz verde = dedo, vermelha = recusado, quadradinho amarelo = onde a comida
   cai, círculo branco = dedo confirmado; à direita, o pedaço em volta do candidato ampliado 3x.
   g++ -O2 -Wall -Wextra -I sarndbox testes/analisa_gesto.cpp sarndbox/Gesto.cpp -o /tmp/analisa_gesto
   /tmp/analisa_gesto /dev/shm/gesto-AAAAMMDD-HHMMSS [--png N|todos|3,10,22] [--saida DIR] [--tudo]
                      [--ajuste nome=valor ...] [--de N] [--ate N]
   --ajuste testa limiares sem mexer no SARndbox (os nomes do "gestoAjuste"). */

#include "Gesto.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sys/stat.h>
#include <chrono>

namespace {

/* ---------------- JSON (só o que o Gravador escreve) */

struct Json
	{
	enum Tipo {NULO,NUM,TEXTO,LISTA,OBJ,LOGICO} tipo=NULO;
	double num=0.0;
	std::string texto;
	std::vector<Json> lista;
	std::vector<std::pair<std::string,Json> > obj;
	const Json* operator[](const char* k) const
		{
		for(const auto& p:obj)
			if(p.first==k)
				return &p.second;
		return 0;
		}
	};

struct Leitor
	{
	const std::string& s;
	size_t i=0;
	bool erro=false;
	explicit Leitor(const std::string& t):s(t) {}
	void ws() {while(i<s.size()&&std::strchr(" \t\r\n",s[i])) ++i;}
	std::string texto()
		{
		std::string r;
		++i; // "
		while(i<s.size()&&s[i]!='"')
			{
			if(s[i]=='\\'&&i+1<s.size())
				++i;
			r+=s[i++];
			}
		++i;
		return r;
		}
	Json valor()
		{
		Json j;
		ws();
		if(i>=s.size()) {erro=true;return j;}
		char c=s[i];
		if(c=='{')
			{
			j.tipo=Json::OBJ;
			++i;
			ws();
			if(i<s.size()&&s[i]=='}') {++i;return j;}
			while(!erro)
				{
				ws();
				if(i>=s.size()||s[i]!='"') {erro=true;break;}
				std::string k=texto();
				ws();
				if(i>=s.size()||s[i]!=':') {erro=true;break;}
				++i;
				j.obj.emplace_back(k,valor());
				ws();
				if(i<s.size()&&s[i]==',') {++i;continue;}
				if(i<s.size()&&s[i]=='}') {++i;break;}
				erro=true;
				}
			}
		else if(c=='[')
			{
			j.tipo=Json::LISTA;
			++i;
			ws();
			if(i<s.size()&&s[i]==']') {++i;return j;}
			while(!erro)
				{
				j.lista.push_back(valor());
				ws();
				if(i<s.size()&&s[i]==',') {++i;continue;}
				if(i<s.size()&&s[i]==']') {++i;break;}
				erro=true;
				}
			}
		else if(c=='"')
			{
			j.tipo=Json::TEXTO;
			j.texto=texto();
			}
		else if(s.compare(i,4,"true")==0) {j.tipo=Json::LOGICO;j.num=1;i+=4;}
		else if(s.compare(i,5,"false")==0) {j.tipo=Json::LOGICO;j.num=0;i+=5;}
		else if(s.compare(i,4,"null")==0) {i+=4;}
		else
			{
			char* fim=0;
			j.tipo=Json::NUM;
			j.num=std::strtod(s.c_str()+i,&fim);
			if(fim==s.c_str()+i) erro=true;
			i=size_t(fim-s.c_str());
			}
		return j;
		}
	};

/* ---------------- PNG (sem biblioteca: deflate "guardado", sem compressão) */

unsigned int crcTab[256];
void iniciaCrc()
	{
	for(unsigned int n=0;n<256;++n)
		{
		unsigned int c=n;
		for(int k=0;k<8;++k)
			c=(c&1)?0xedb88320u^(c>>1):c>>1;
		crcTab[n]=c;
		}
	}
unsigned int crc(const unsigned char* b,size_t n,unsigned int c=0xffffffffu)
	{
	for(size_t i=0;i<n;++i)
		c=crcTab[(c^b[i])&0xff]^(c>>8);
	return c;
	}
void be32(std::vector<unsigned char>& v,unsigned int x) {for(int i=3;i>=0;--i) v.push_back((x>>(8*i))&0xff);}
void pedaco(std::FILE* f,const char* tipo,const std::vector<unsigned char>& d)
	{
	std::vector<unsigned char> b;
	be32(b,unsigned(d.size()));
	b.insert(b.end(),tipo,tipo+4);
	b.insert(b.end(),d.begin(),d.end());
	be32(b,crc(b.data()+4,b.size()-4)^0xffffffffu);
	std::fwrite(b.data(),1,b.size(),f);
	}
bool salvaPng(const std::string& nome,int w,int h,const std::vector<unsigned char>& rgb)
	{
	std::FILE* f=std::fopen(nome.c_str(),"wb");
	if(!f) return false;
	const unsigned char assinatura[8]={137,80,78,71,13,10,26,10};
	std::fwrite(assinatura,1,8,f);
	std::vector<unsigned char> ih;
	be32(ih,unsigned(w));
	be32(ih,unsigned(h));
	ih.push_back(8);ih.push_back(2);ih.push_back(0);ih.push_back(0);ih.push_back(0);
	pedaco(f,"IHDR",ih);
	std::vector<unsigned char> cru;
	for(int y=0;y<h;++y)
		{
		cru.push_back(0);
		cru.insert(cru.end(),rgb.begin()+size_t(y)*w*3,rgb.begin()+size_t(y+1)*w*3);
		}
	std::vector<unsigned char> z;
	z.push_back(0x78);z.push_back(0x01);
	unsigned int a=1,b=0;
	for(size_t i=0;i<cru.size();++i) {a=(a+cru[i])%65521;b=(b+a)%65521;}
	for(size_t i=0;i<cru.size();i+=65535)
		{
		size_t n=std::min<size_t>(65535,cru.size()-i);
		z.push_back(i+n>=cru.size()?1:0);
		z.push_back(n&0xff);z.push_back((n>>8)&0xff);
		z.push_back((~n)&0xff);z.push_back(((~n)>>8)&0xff);
		z.insert(z.end(),cru.begin()+i,cru.begin()+i+n);
		}
	be32(z,(b<<16)|a);
	pedaco(f,"IDAT",z);
	pedaco(f,"IEND",std::vector<unsigned char>());
	return std::fclose(f)==0;
	}

/* algarismos 3x5 para o número do quadro */
const char* digitos[10]={"111101101101111","010110010010111","111001111100111","111001111001111","101101111001001",
                         "111100111001111","111100111101111","111001001001001","111101111101111","111101111001111"};

struct Imagem
	{
	int w,h;
	std::vector<unsigned char> p;
	Imagem(int sw,int sh):w(sw),h(sh),p(size_t(sw)*sh*3,0) {}
	void ponto(int x,int y,int r,int g,int b)
		{
		if(x<0||y<0||x>=w||y>=h) return;
		unsigned char* c=&p[(size_t(y)*w+x)*3];
		c[0]=(unsigned char)r;c[1]=(unsigned char)g;c[2]=(unsigned char)b;
		}
	void cruz(int x,int y,int t,int r,int g,int b)
		{
		/* contorno preto para aparecer em cima de qualquer cor */
		for(int i=-t-1;i<=t+1;++i)
			for(int e=-2;e<=2;++e)
				{
				ponto(x+i,y+e,0,0,0);
				ponto(x+e,y+i,0,0,0);
				}
		for(int i=-t;i<=t;++i)
			for(int e=-1;e<=1;++e)
				{
				ponto(x+i,y+e,r,g,b);
				ponto(x+e,y+i,r,g,b);
				}
		}
	void circulo(float cx,float cy,float raio,int r,int g,int b)
		{
		for(int i=0;i<360;++i)
			ponto(int(cx+raio*std::cos(i*0.01745f)),int(cy+raio*std::sin(i*0.01745f)),r,g,b);
		}
	void numero(int x,int y,int v,int esc)
		{
		std::string s=std::to_string(v);
		for(size_t k=0;k<s.size();++k)
			for(int j=0;j<5;++j)
				for(int i=0;i<3;++i)
					if(digitos[s[k]-'0'][j*3+i]=='1')
						for(int a=0;a<esc;++a)
							for(int b=0;b<esc;++b)
								ponto(x+int(k)*4*esc+i*esc+a,y+j*esc+b,255,255,255);
		}
	};

void cor(float h,bool valido,float alt,unsigned char* c)
	{
	if(!valido) {c[0]=70;c[1]=0;c[2]=80;return;}
	if(!(h>=1.5f))
		{
		int g=std::max(30,std::min(200,int(70.0f+8.0f*alt)));
		c[0]=c[1]=c[2]=(unsigned char)g;
		return;
		}
	float t=std::min(1.0f,(h-1.5f)/23.5f);
	/* azul -> ciano -> verde -> amarelo -> vermelho */
	const float pr[5][3]={{40,80,255},{0,220,255},{40,230,40},{255,230,0},{255,40,20}};
	float u=t*4.0f;
	int i=std::min(3,int(u));
	float f=u-float(i);
	for(int k=0;k<3;++k)
		c[k]=(unsigned char)(pr[i][k]+(pr[i+1][k]-pr[i][k])*f);
	}

struct Corr {float scale,offset; float correct(float d) const {return d*scale+offset;}};

struct Gravacao
	{
	std::string pasta;
	int w=0,h=0,rec[4]={0,0,0,0},area[4]={0,0,0,0};
	double m[4][4];
	float caixa[4]={-1e9f,-1e9f,1e9f,1e9f};
	float escala=32.0f;
	std::string limiares;
	std::vector<Corr> pdc;
	struct Q {double t; std::vector<Gesto::MaoAberta> maos; bool vivo; float vx,vy;};
	std::vector<Q> quadros;
	std::FILE* fCru=0;
	std::FILE* fAreia=0;
	std::vector<std::uint16_t> cru,ar16;
	std::vector<float> areia;
	int cw() const {return rec[2]-rec[0];}
	int ch() const {return rec[3]-rec[1];}
	bool abre(const std::string& p)
		{
		pasta=p;
		std::FILE* f=std::fopen((p+"/gesto.json").c_str(),"rb");
		if(!f) {std::fprintf(stderr,"sem %s/gesto.json\n",p.c_str());return false;}
		std::string s;
		char b[65536];
		size_t n;
		while((n=std::fread(b,1,sizeof(b),f))>0) s.append(b,n);
		std::fclose(f);
		Leitor l(s);
		Json j=l.valor();
		if(l.erro||j.tipo!=Json::OBJ) {std::fprintf(stderr,"gesto.json com erro\n");return false;}
		auto num=[&](const char* k,double padrao)->double {const Json* v=j[k];return v&&v->tipo==Json::NUM?v->num:padrao;};
		auto lista=[&](const char* k,size_t n,double* saida)->bool
			{
			const Json* v=j[k];
			if(!v||v->tipo!=Json::LISTA||v->lista.size()!=n) return false;
			for(size_t i=0;i<n;++i) saida[i]=v->lista[i].num;
			return true;
			};
		w=int(num("largura",0));
		h=int(num("altura",0));
		escala=float(num("areiaEscala",32));
		double r[4],a[4],c[4],mm[16];
		if(!lista("recorte",4,r)||!lista("area",4,a)||!lista("matriz",16,mm)) {std::fprintf(stderr,"gesto.json sem recorte/area/matriz\n");return false;}
		for(int i=0;i<4;++i) {rec[i]=int(r[i]);area[i]=int(a[i]);}
		for(int i=0;i<16;++i) m[i/4][i%4]=mm[i];
		if(lista("caixa",4,c)) for(int i=0;i<4;++i) caixa[i]=float(c[i]);
		if(const Json* v=j["limiares"]) limiares=v->texto;
		if(const Json* v=j["pronta"]) if(v->num==0) std::printf("(gravação não terminou direito: vale o que tem)\n");
		const Json* qs=j["quadros"];
		if(qs)
			for(const Json& q:qs->lista)
				{
				Q x;
				const Json* t=q["t"];
				x.t=t?t->num:0.0;
				if(const Json* ms=q["maos"])
					for(const Json& mj:ms->lista)
						if(mj.lista.size()==4)
							x.maos.push_back(Gesto::MaoAberta{float(mj.lista[0].num),float(mj.lista[1].num),float(mj.lista[2].num),float(mj.lista[3].num)});
				x.vivo=false;
				x.vx=x.vy=0.0f;
				if(const Json* v=q["vivo"])
					if(v->lista.size()==3) {x.vivo=v->lista[0].num!=0;x.vx=float(v->lista[1].num);x.vy=float(v->lista[2].num);}
				quadros.push_back(x);
				}
		size_t np=size_t(cw())*ch();
		std::vector<float> cf(2*np);
		f=std::fopen((p+"/correcao.f32").c_str(),"rb");
		if(!f||std::fread(cf.data(),sizeof(float),cf.size(),f)!=cf.size()) {std::fprintf(stderr,"correcao.f32 faltando\n");if(f) std::fclose(f);return false;}
		std::fclose(f);
		pdc.resize(np);
		for(size_t i=0;i<np;++i) {pdc[i].scale=cf[2*i];pdc[i].offset=cf[2*i+1];}
		fCru=std::fopen((p+"/cru.u16").c_str(),"rb");
		fAreia=std::fopen((p+"/areia.u16").c_str(),"rb");
		if(!fCru||!fAreia) {std::fprintf(stderr,"cru.u16/areia.u16 faltando\n");return false;}
		/* quadros que estão nos arquivos (se a gravação parou no meio) */
		std::fseek(fCru,0,SEEK_END);
		long tam=std::ftell(fCru);
		size_t nq=size_t(tam)/(np*2);
		if(nq<quadros.size()) quadros.resize(nq);
		cru.resize(np);
		ar16.resize(np);
		areia.resize(np);
		return true;
		}
	bool le(size_t i)
		{
		size_t np=size_t(cw())*ch();
		if(std::fseek(fCru,long(i*np*2),SEEK_SET)!=0||std::fread(cru.data(),2,np,fCru)!=np) return false;
		if(std::fseek(fAreia,long(i*np*2),SEEK_SET)!=0||std::fread(ar16.data(),2,np,fAreia)!=np) return false;
		for(size_t k=0;k<np;++k)
			areia[k]=ar16[k]?float(ar16[k])/escala:0.0f;
		return true;
		}
	};

void desenha(const Gravacao& g,const Gesto::Quadro& q,const Gesto::Detector& d,const Gesto::Rastro& r,int iq,const std::string& nome)
	{
	int cw=g.cw(),ch=g.ch();
	const int Z=3,ZW=110;
	Imagem im(cw+4+ZW*Z,std::max(ch,ZW*Z));
	std::vector<unsigned char> base(size_t(cw)*ch*3);
	for(int y=0;y<ch;++y)
		for(int x=0;x<cw;++x)
			{
			int px=g.rec[0]+x,py=g.rec[1]+y;
			float a=q.alt(px,py),s=d.areia(q,px,py);
			cor(a-s,std::isfinite(a),std::isfinite(a)?a:0.0f,&base[(size_t(y)*cw+x)*3]);
			}
	for(int y=0;y<ch;++y)
		for(int x=0;x<cw;++x)
			{
			const unsigned char* c=&base[(size_t(y)*cw+x)*3];
			im.ponto(x,y,c[0],c[1],c[2]);
			}
	/* o retângulo do detector */
	for(int x=g.area[0];x<g.area[2];x+=3) {im.ponto(x-g.rec[0],g.area[1]-g.rec[1],150,150,150);im.ponto(x-g.rec[0],g.area[3]-1-g.rec[1],150,150,150);}
	for(int y=g.area[1];y<g.area[3];y+=3) {im.ponto(g.area[0]-g.rec[0],y-g.rec[1],150,150,150);im.ponto(g.area[2]-1-g.rec[0],y-g.rec[1],150,150,150);}
	/* o centro da ampliação: o dedo escolhido, senão o 1º candidato, senão o ponto mais alto sobre a areia */
	float zx=-1,zy=-1;
	for(const Gesto::Medida& m:d.medidas)
		if(m.dedo&&zx<0) {zx=m.px;zy=m.py;}
	if(zx<0&&!d.medidas.empty()) {zx=d.medidas[0].px;zy=d.medidas[0].py;}
	if(zx<0)
		{
		float mh=1.5f;
		for(int y=g.area[1];y<g.area[3];y+=2)
			for(int x=g.area[0];x<g.area[2];x+=2)
				{
				float hh=q.alt(x,y)-d.areia(q,x,y);
				if(hh>mh) {mh=hh;zx=float(x);zy=float(y);}
				}
		}
	auto marca=[&](Imagem& m,float ox,float oy,int esc,int tam)
		{
		for(const Gesto::Medida& md:d.medidas)
			{
			int x=int((md.px-ox)*esc),y=int((md.py-oy)*esc);
			if(md.dedo) m.cruz(x,y,tam,40,255,40); else m.cruz(x,y,tam,255,30,30);
			if(md.dedo&&(std::fabs(md.ax-md.px)>2.0f||std::fabs(md.ay-md.py)>2.0f))
				{
				/* apontando para baixo: onde a comida cai, à frente da ponta */
				int ax=int((md.ax-ox)*esc),ay=int((md.ay-oy)*esc),t=esc+1;
				for(int a=-t-1;a<=t+1;++a)
					for(int b=-t-1;b<=t+1;++b)
						m.ponto(ax+a,ay+b,(std::abs(a)>t||std::abs(b)>t)?0:255,(std::abs(a)>t||std::abs(b)>t)?0:230,0);
				}
			}
		if(r.ativo&&d.rastroPx>=0.0f)
			{
			float cp=q.cmPorPixel(int(d.rastroPx),int(d.rastroPy));
			if(cp>0) m.circulo((d.rastroPx-ox)*esc,(d.rastroPy-oy)*esc,3.0f/cp*esc,255,255,255);
			}
		};
	marca(im,float(g.rec[0]),float(g.rec[1]),1,5);
	if(zx>=0)
		{
		int x0=int(zx)-ZW/2-g.rec[0],y0=int(zy)-ZW/2-g.rec[1];
		Imagem z(ZW*Z,ZW*Z);
		for(int y=0;y<ZW*Z;++y)
			for(int x=0;x<ZW*Z;++x)
				{
				int sx=x0+x/Z,sy=y0+y/Z;
				if(sx<0||sy<0||sx>=cw||sy>=ch) continue;
				const unsigned char* c=&base[(size_t(sy)*cw+sx)*3];
				z.ponto(x,y,c[0],c[1],c[2]);
				}
		marca(z,float(x0+g.rec[0]),float(y0+g.rec[1]),Z,9);
		for(int y=0;y<ZW*Z;++y)
			for(int x=0;x<ZW*Z;++x)
				{
				const unsigned char* c=&z.p[(size_t(y)*z.w+x)*3];
				im.ponto(cw+4+x,y,c[0],c[1],c[2]);
				}
		/* onde está a ampliação na imagem inteira */
		for(int i=0;i<ZW;++i)
			{
			im.ponto(x0+i,y0,255,255,255);im.ponto(x0+i,y0+ZW-1,255,255,255);
			im.ponto(x0,y0+i,255,255,255);im.ponto(x0+ZW-1,y0+i,255,255,255);
			}
		}
	im.numero(4,4,iq,3);
	salvaPng(nome,im.w,im.h,im.p);
	}

void imprime(const Gesto::Medida& m)
	{
	if(m.tipo==0)
		std::printf("     pico     x %6.1f y %6.1f: ponta %.1f cm sobre a areia; pico %.1f cm2, %.1f x %.1f cm; punho %.0f cm2, dedo %.1f cm acima dele; alto %.0f%%; outro pico %.1f cm2%s -> %s\n",
		            m.bx,m.by,m.sobreAreia,m.picoArea,m.picoLarg,m.picoComp,m.punhoArea,m.degrau,100.0f*m.fracAlto,m.outroArea,m.rastreio?" (rastro)":"",m.motivo);
	else
		std::printf("     contorno x %6.1f y %6.1f: dedo sai %.1f cm do punho, %.1f cm de largura, %.0f graus do braço; punho %.1f cm acima da ponta; ponta %.1f cm sobre a areia; corpo %.0f cm2; outra parte fina %.1f cm; braço %.1f cm sobre a areia na borda%s -> %s\n",
		            m.bx,m.by,m.comp,m.larg,m.angulo,m.degrau,m.sobreAreia,m.punhoArea,m.outroArea,m.braco,m.rastreio?" (rastro)":"",m.motivo);
	}

}

int main(int argc,char** argv)
	{
	if(argc<2||argv[1][0]=='-')
		{
		std::fprintf(stderr,"uso: %s <pasta da gravação> [--png N|todos|3,10,22] [--saida DIR] [--tudo] [--ajuste nome=valor ...] [--de N] [--ate N]\n",argv[0]);
		return 2;
		}
	iniciaCrc();
	Gravacao g;
	if(!g.abre(argv[1]))
		return 1;
	std::string saida=g.pasta+"/analise";
	std::string pngs="8";
	bool tudo=false;
	int de=0,ate=1<<30;
	Gesto::Limiares lim;
	for(int i=2;i<argc;++i)
		{
		std::string a=argv[i];
		if(a=="--png"&&i+1<argc) pngs=argv[++i];
		else if(a=="--saida"&&i+1<argc) saida=argv[++i];
		else if(a=="--tudo") tudo=true;
		else if(a=="--de"&&i+1<argc) de=std::atoi(argv[++i]);
		else if(a=="--ate"&&i+1<argc) ate=std::atoi(argv[++i]);
		else if(a=="--ajuste"&&i+1<argc)
			{
			std::string v=argv[++i];
			size_t e=v.find('=');
			if(e==std::string::npos||!lim.ajusta(v.substr(0,e),float(std::atof(v.c_str()+e+1))))
				{
				std::fprintf(stderr,"limiar desconhecido: %s\n",v.c_str());
				return 2;
				}
			}
		else {std::fprintf(stderr,"opção desconhecida: %s\n",a.c_str());return 2;}
		}
	int nq=int(g.quadros.size());
	ate=std::min(ate,nq-1);
	std::printf("Gravação %s: %d quadros de %dx%d (recorte x %d..%d y %d..%d; detector x %d..%d y %d..%d)\n",
	            g.pasta.c_str(),nq,g.w,g.h,g.rec[0],g.rec[2],g.rec[1],g.rec[3],g.area[0],g.area[2],g.area[1],g.area[3]);
	if(!g.limiares.empty()&&g.limiares!=lim.texto())
		std::printf("(limiares da gravação: %s)\n(limiares desta análise: %s)\n",g.limiares.c_str(),lim.texto().c_str());

	/* duas passadas iguais: a 1ª mede e escolhe os quadros dos PNGs, a 2ª desenha */
	std::vector<int> comCand,comDedo,eventos,escolhidos;
	for(int passada=0;passada<2;++passada)
		{
		Gesto::Detector d;
		d.lim=lim;
		d.medeTudo=true;
		Gesto::Rastro r;
		int viuN=0,vivoN=0,confirmou=0,soltou=0,maosN=0;
		double custo=0.0,custoMax=0.0; // tempo do passo (us), como no SARndbox (o QuadroKinect de verdade)
		std::map<std::string,int> motivos;
		for(int i=de;i<=ate;++i)
			{
			if(!g.le(size_t(i))) {std::printf("quadro %d: não deu para ler\n",i);break;}
			const Gravacao::Q& gq=g.quadros[size_t(i)];
			Gesto::QuadroKinect<Corr> q(g.m,g.cru.data(),g.areia.data(),g.pdc.data(),g.rec[0],g.rec[1],g.cw(),g.ch());
			Gesto::Passo p;
			auto t0=std::chrono::steady_clock::now();
			d.passo(q,g.area[0],g.area[1],g.area[2],g.area[3],gq.t,g.caixa,gq.maos,r,p);
			double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count();
			custo+=us;
			custoMax=std::max(custoMax,us);
			if(passada==1)
				{
				if(std::find(escolhidos.begin(),escolhidos.end(),i)!=escolhidos.end())
					{
					char nome[64];
					std::snprintf(nome,sizeof(nome),"/q%04d.png",i);
					desenha(g,q,d,r,i,saida+nome);
					}
				continue;
				}
			bool algum=false;
			for(const Gesto::Medida& m:d.medidas)
				{
				algum=algum||m.dedo;
				++motivos[std::string(m.tipo==0?"pico: ":"contorno: ")+m.motivo];
				}
			if(!d.medidas.empty()) comCand.push_back(i);
			if(algum) comDedo.push_back(i);
			viuN+=p.viu;
			vivoN+=gq.vivo;
			maosN+=!gq.maos.empty();
			bool ev=p.ev==Gesto::Rastro::CONFIRMOU||p.ev==Gesto::Rastro::SOLTOU||p.soltouMao;
			if(ev) eventos.push_back(i);
			if(tudo||!d.medidas.empty()||ev||gq.vivo||!gq.maos.empty())
				{
				std::printf("q %4d  t %6.2f s: %zu candidato(s)",i,gq.t,d.medidas.size());
				if(!gq.maos.empty()) std::printf("; %zu mão(s) aberta(s) do SARndbox",gq.maos.size());
				if(gq.vivo) std::printf("; ao vivo: dedo em %.1f %.1f",gq.vx,gq.vy);
				std::printf("\n");
				for(const Gesto::Medida& m:d.medidas)
					imprime(m);
				}
			if(p.ev==Gesto::Rastro::CONFIRMOU) {++confirmou;std::printf("  >> CONFIRMOU: dedo em %.1f %.1f (aqui cairia a comida)\n",r.x,r.y);}
			if(p.ev==Gesto::Rastro::SOLTOU) {++soltou;std::printf("  >> soltou\n");}
			if(p.soltouMao) std::printf("  >> soltou (abriu a mão)\n");
			}
		if(passada==0)
			{
			std::printf("Resumo: %d quadros; candidatos em %zu; dedo em %zu; escolhido em %d (ao vivo: %d); mão aberta do SARndbox em %d; confirmou %d vez(es); soltou %d\n",
			            ate-de+1,comCand.size(),comDedo.size(),viuN,vivoN,maosN,confirmou,soltou);
			int nq2=std::max(1,ate-de+1);
			std::printf("Custo: %.0f us por quadro em média (máximo %.0f us); a 12 por segundo = %.2f%% de um núcleo\n",custo/nq2,custoMax,custo/nq2*12.0/1e4);
			std::vector<std::pair<int,std::string> > ms;
			for(const auto& m:motivos) ms.emplace_back(m.second,m.first);
			std::sort(ms.rbegin(),ms.rend());
			std::printf("Motivos (candidatos):");
			for(const auto& m:ms) std::printf(" %s %d;",m.second.c_str(),m.first);
			std::printf("\n");
			/* os quadros dos PNGs */
			if(pngs=="todos")
				for(int i=de;i<=ate;++i) escolhidos.push_back(i);
			else if(pngs.find(',')!=std::string::npos||(pngs.size()>0&&pngs[0]=='='))
				{
				for(const char* s=pngs.c_str();*s;)
					{
					if(*s>='0'&&*s<='9') {escolhidos.push_back(int(std::strtol(s,const_cast<char**>(&s),10)));}
					else ++s;
					}
				}
			else
				{
				int quer=std::max(0,std::atoi(pngs.c_str()));
				auto pega=[&](const std::vector<int>& fonte,int n)
					{
					for(int k=0;k<n&&!fonte.empty();++k)
						{
						int v=fonte[size_t(k)*fonte.size()/size_t(n)];
						if(std::find(escolhidos.begin(),escolhidos.end(),v)==escolhidos.end()&&int(escolhidos.size())<quer) escolhidos.push_back(v);
						}
					};
				pega(eventos,std::min(quer/2,int(eventos.size())));
				pega(comDedo,std::min(quer/2,int(comDedo.size())));
				pega(comCand,quer);
				std::vector<int> todos;
				for(int i=de;i<=ate;++i) todos.push_back(i);
				pega(todos,quer);
				}
			std::sort(escolhidos.begin(),escolhidos.end());
			if(escolhidos.empty()) break;
			mkdir(saida.c_str(),0755);
			}
		else
			std::printf("PNGs (%zu) em %s\n",escolhidos.size(),saida.c_str());
		}
	return 0;
	}
