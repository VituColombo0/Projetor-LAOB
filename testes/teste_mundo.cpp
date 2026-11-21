/* Teste da planta do Mundo LAOB (sarndbox/MundoPlanta.h), sem OpenGL nem Kinect (19/11/2026):
   - todo ponto e assento fica dentro de uma sala, fora das paredes, na sala esperada;
   - a rota entre QUALQUER par de lugares (assentos, pontos com todas as vagas, chegada e saída pelo
     corredor) existe, cabe na fila do boneco e nenhum trecho atravessa móvel ou parede (só os vãos das portas);
   - as constantes que vão para o shader são GLSL 1.10 (sem o sufixo f dos números).
   uso: g++ -I sarndbox testes/teste_mundo.cpp -o /tmp/t && /tmp/t */
#include "MundoPlanta.h"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <vector>

struct Lugar {float x,y; char nome[48]; int sala;};

int main()
	{
	int falhas=0;
	std::vector<Lugar> L;
	const char* NA[8]={"A0 Gazziro","A1 Lucas","A2 Mario","A3 Victor","A4","A5","A6","A7"};
	const float AS[8][2]={
#define R(n,a,b,g) {float(a),float(b)},
		MP_ASSENTOS(R)
#undef R
		};
	const int SALA_ASSENTO[8]={mp::MINI,mp::PRINCIPAL,mp::PRINCIPAL,mp::PRINCIPAL,mp::PRINCIPAL,mp::PRINCIPAL,mp::PRINCIPAL,mp::PRINCIPAL};
	for(int k=0;k<8;++k)
		{
		Lugar l={AS[k][0],AS[k][1],"",SALA_ASSENTO[k]};
		std::snprintf(l.nome,sizeof(l.nome),"assento %s",NA[k]);
		L.push_back(l);
		}
	for(int p=0;p<mp::N_PONTOS;++p)
		for(int v=0;v<3;++v)
			{
			if(v>0&&p!=mp::P_SOFA&&p!=mp::P_PLATEIA&&p!=mp::P_AULA)
				continue;
			Lugar l;
			mp::lugar(p,v,l.x,l.y);
			std::snprintf(l.nome,sizeof(l.nome),"P%d vaga %d",p,v);
			l.sala=p>=mp::P_URNA&&p<=mp::P_PCABERTO?mp::FORENSE:(p>=mp::P_APRESENTA&&p<=mp::P_VIDRO?mp::MINI:mp::PRINCIPAL);
			if(p==mp::P_BEBEDOURO)
				l.sala=mp::CORREDOR; // (20/11/2026) o bebedouro fica no corredor
			L.push_back(l);
			}
	Lugar ext[3]={{0.395f,0.775f,"P6 lado de lá do arco",mp::PRINCIPAL},{1.00f,0.050f,"chegada (corredor)",mp::CORREDOR},{1.02f,0.050f,"saída (corredor)",mp::CORREDOR}};
	for(int k=0;k<3;++k)
		L.push_back(ext[k]);
	/* 1) cada lugar na sala certa e fora das paredes */
	for(size_t a=0;a<L.size();++a)
		{
		if(mp::sala(L[a].x,L[a].y)!=L[a].sala)
			{
			std::printf("  FALHA: %s (%.3f; %.3f) está na sala %d, esperava %d\n",L[a].nome,L[a].x,L[a].y,mp::sala(L[a].x,L[a].y),L[a].sala);
			falhas++;
			}
		for(int k=0;k<mp::N_PAREDES;++k)
			if(L[a].x>mp::PAREDES[k][0]&&L[a].x<mp::PAREDES[k][1]&&L[a].y>mp::PAREDES[k][2]&&L[a].y<mp::PAREDES[k][3])
				{
				std::printf("  FALHA: %s dentro de uma parede\n",L[a].nome);
				falhas++;
				}
		}
	/* 2) todas as rotas, em duas proporções de área iluminada (a do laob, 59,3 x 41,9 cm, e 4:3) */
	const float ASP[2]={41.92f/59.33f,0.75f};
	int pares=0,maior=0,semFolga=0;
	for(int q=0;q<2;++q)
		for(size_t a=0;a<L.size();++a)
			for(size_t b=0;b<L.size();++b)
				{
				if(a==b)
					continue;
				float r[mp::N_NOS+2][2];
				int n=mp::rota(L[a].x,L[a].y,L[b].x,L[b].y,ASP[q],r,mp::N_NOS+2);
				++pares;
				if(n<=0||n>14)
					{
					std::printf("  FALHA: rota de %s até %s: %d pontos\n",L[a].nome,L[b].nome,n);
					falhas++;
					continue;
					}
				if(n>maior)
					maior=n;
				float px=L[a].x,py=L[a].y;
				bool folgada=true;
				for(int k=0;k<n;++k)
					{
					if(!mp::livre(px,py,r[k][0],r[k][1],ASP[q],0.0f))
						{
						std::printf("  FALHA: rota de %s até %s atravessa móvel/parede entre (%.3f; %.3f) e (%.3f; %.3f)\n",
						            L[a].nome,L[b].nome,px,py,r[k][0],r[k][1]);
						falhas++;
						}
					if(!mp::livre(px,py,r[k][0],r[k][1],ASP[q],0.6f))
						folgada=false;
					px=r[k][0];
					py=r[k][1];
					}
				if(!folgada)
					++semFolga;
				if(px!=L[b].x||py!=L[b].y)
					{
					std::printf("  FALHA: rota de %s não termina em %s\n",L[a].nome,L[b].nome);
					falhas++;
					}
				}
	std::printf("rotas: %d (%zu lugares, 2 proporções), a maior com %d pontos, %d sem a folga de 0,6 cm\n",pares,L.size(),maior,semFolga);
	/* 3) GLSL 1.10: nada de 1.0f */
	const char* g=MP_GLSL;
	for(const char* c=g;*c;++c)
		if(*c=='f'&&c>g&&std::isdigit((unsigned char)c[-1]))
			{
			std::printf("  FALHA: número com sufixo f nas constantes do shader: ...%.20s\n",c-8);
			falhas++;
			break;
			}
	if(falhas==0)
		std::printf("planta e rotas OK\n");
	return falhas?1:0;
	}
