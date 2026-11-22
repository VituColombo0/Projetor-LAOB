/***********************************************************************
Perfil - medidas de desempenho do Sandbox (Projetor-LAOB, 22/11/2026).

A linha "Desempenho:" do sandbox.log (a cada 30 s) dá a MÉDIA de quadros por
segundo, e média boa esconde travadinha. Esta classe junta, na mesma janela de
30 s, a linha "Perfil:":
  - intervalo entre quadros: p50, p95, p99, máximo e quantos passaram de 50 ms;
  - tempo de CPU de trechos (o frame() inteiro e as leituras GPU->CPU, que
    travam o pipeline até a GPU terminar);
  - tempo de GPU das passadas principais (água e relevo), por consulta
    GL_TIME_ELAPSED lida sem esperar (a de alguns quadros atrás).
Quadro acima de 100 ms vira uma linha "Perfil: quadro lento de N ms" na hora
(no máximo 5 por janela), para achar travadas nas trocas.
Custo: duas consultas de GPU por quadro e um vetor de ~900 números.
***********************************************************************/

#ifndef PERFIL_INCLUDED
#define PERFIL_INCLUDED

#include <GL/gl.h>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <string>

/* Do libGL (sem incluir GL/glx.h: as macros do X11, como None e Status, quebram os cabeçalhos do Vrui) */
extern "C" void (*glXGetProcAddressARB(const GLubyte*))(void);

class Perfil
	{
	public:
	enum Cpu {C_FRAME,C_LEITURA,C_AGUA,C_RELEVO,N_CPU};
	enum Gpu {G_AGUA,G_RELEVO,N_GPU};

	private:
	typedef std::chrono::steady_clock Relogio;
	typedef void (*FnGen)(GLsizei,GLuint*);
	typedef void (*FnBegin)(GLenum,GLuint);
	typedef void (*FnEnd)(GLenum);
	typedef void (*FnGetiv)(GLuint,GLenum,GLint*);
	typedef void (*FnGetui64)(GLuint,GLenum,unsigned long long*);
	enum {TIME_ELAPSED=0x88BF,QUERY_RESULT=0x8866,QUERY_RESULT_AVAILABLE=0x8867,ANEL=4};

	/* Intervalo entre quadros: */
	Relogio::time_point ultimo;
	bool temUltimo;
	double inicio; // segundos desde o primeiro quadro (os 10 primeiros não contam lentidão)
	std::vector<float> intervalos; // ms
	int lentosAvisados;

	/* CPU: */
	Relogio::time_point cpuT0[N_CPU];
	double cpuSoma[N_CPU],cpuMax[N_CPU];
	unsigned int cpuN[N_CPU];

	/* GPU: anel de consultas por passada, lidas quando ficam prontas */
	bool glPronto,glTem;
	FnGen gen;FnBegin begin;FnEnd end;FnGetiv getiv;FnGetui64 getui64;
	GLuint consultas[N_GPU][ANEL];
	bool pendente[N_GPU][ANEL];
	int proxima[N_GPU];
	int ativa; // passada com consulta aberta agora (-1 = nenhuma)
	double gpuSoma[N_GPU],gpuMax[N_GPU];
	unsigned int gpuN[N_GPU];

	static double ms(Relogio::duration d)
		{
		return std::chrono::duration<double,std::milli>(d).count();
		}
	void iniciaGl(void)
		{
		glPronto=true;
		gen=(FnGen)glXGetProcAddressARB((const GLubyte*)"glGenQueries");
		begin=(FnBegin)glXGetProcAddressARB((const GLubyte*)"glBeginQuery");
		end=(FnEnd)glXGetProcAddressARB((const GLubyte*)"glEndQuery");
		getiv=(FnGetiv)glXGetProcAddressARB((const GLubyte*)"glGetQueryObjectiv");
		getui64=(FnGetui64)glXGetProcAddressARB((const GLubyte*)"glGetQueryObjectui64v");
		glTem=gen!=0&&begin!=0&&end!=0&&getiv!=0&&getui64!=0;
		if(glTem)
			for(int g=0;g<N_GPU;++g)
				gen(ANEL,consultas[g]);
		}
	void colhe(int g)
		{
		for(int k=0;k<ANEL;++k)
			if(pendente[g][k])
				{
				GLint pronto=0;
				getiv(consultas[g][k],QUERY_RESULT_AVAILABLE,&pronto);
				if(!pronto)
					continue;
				unsigned long long ns=0;
				getui64(consultas[g][k],QUERY_RESULT,&ns);
				pendente[g][k]=false;
				double v=double(ns)*1.0e-6;
				gpuSoma[g]+=v;
				gpuMax[g]=std::max(gpuMax[g],v);
				++gpuN[g];
				}
		}
	static float percentil(std::vector<float>& v,double p)
		{
		if(v.empty())
			return 0.0f;
		size_t i=std::min(v.size()-1,size_t(p*double(v.size()-1)+0.5));
		std::nth_element(v.begin(),v.begin()+i,v.end());
		return v[i];
		}

	public:
	Perfil(void)
		:temUltimo(false),inicio(0.0),lentosAvisados(0),glPronto(false),glTem(false),
		 gen(0),begin(0),end(0),getiv(0),getui64(0),ativa(-1)
		{
		intervalos.reserve(2048);
		for(int g=0;g<N_GPU;++g)
			{
			proxima[g]=0;
			for(int k=0;k<ANEL;++k)
				{
				consultas[g][k]=0;
				pendente[g][k]=false;
				}
			}
		zera();
		}
	void zera(void)
		{
		intervalos.clear();
		lentosAvisados=0;
		for(int c=0;c<N_CPU;++c)
			cpuSoma[c]=cpuMax[c]=0.0,cpuN[c]=0;
		for(int g=0;g<N_GPU;++g)
			gpuSoma[g]=gpuMax[g]=0.0,gpuN[g]=0;
		}

	/* No começo de cada frame(): guarda o intervalo desde o anterior. Devolve > 0 (ms) se foi lento. */
	double quadro(void)
		{
		Relogio::time_point agora=Relogio::now();
		double lento=0.0;
		if(temUltimo)
			{
			double d=ms(agora-ultimo);
			inicio+=d*1.0e-3;
			if(intervalos.size()<16384)
				intervalos.push_back(float(d));
			if(d>100.0&&inicio>10.0&&lentosAvisados<5)
				{
				++lentosAvisados;
				lento=d;
				}
			}
		ultimo=agora;
		temUltimo=true;
		return lento;
		}

	void cpuIni(Cpu c)
		{
		cpuT0[c]=Relogio::now();
		}
	void cpuFim(Cpu c)
		{
		double d=ms(Relogio::now()-cpuT0[c]);
		cpuSoma[c]+=d;
		cpuMax[c]=std::max(cpuMax[c],d);
		++cpuN[c];
		}

	/* Passadas de GPU (precisam do contexto OpenGL; uma aberta por vez) */
	void gpuIni(Gpu g)
		{
		if(!glPronto)
			iniciaGl();
		if(!glTem||ativa>=0)
			return;
		colhe(g);
		int k=proxima[g];
		if(pendente[g][k])
			return; // a GPU ainda não devolveu a de ANEL quadros atrás: pula esta medida
		begin(TIME_ELAPSED,consultas[g][k]);
		ativa=g;
		}
	void gpuFim(Gpu g)
		{
		if(ativa!=int(g))
			return;
		end(TIME_ELAPSED);
		pendente[g][proxima[g]]=true;
		proxima[g]=(proxima[g]+1)%ANEL;
		ativa=-1;
		}

	/* A linha "Perfil:" da janela que terminou; zera para a próxima */
	std::string relatorio(unsigned int quadros)
		{
		char b[512];
		std::vector<float>& v=intervalos;
		unsigned int acima=0;
		for(size_t i=0;i<v.size();++i)
			if(v[i]>50.0f)
				++acima;
		float p50=percentil(v,0.50),p95=percentil(v,0.95),p99=percentil(v,0.99);
		float mx=v.empty()?0.0f:*std::max_element(v.begin(),v.end());
		double q=quadros>0?double(quadros):1.0;
		int n=std::snprintf(b,sizeof(b),
		       "Perfil: quadro p50 %.1f p95 %.1f p99 %.1f max %.0f ms, %u acima de 50 ms; "
		       "CPU frame %.2f (max %.1f) agua %.2f relevo %.2f leituras %.2f (max %.1f) ms/quadro",
		       p50,p95,p99,mx,acima,
		       cpuSoma[C_FRAME]/q,cpuMax[C_FRAME],cpuSoma[C_AGUA]/q,cpuSoma[C_RELEVO]/q,
		       cpuSoma[C_LEITURA]/q,cpuMax[C_LEITURA]);
		if(glTem&&n>0&&n<int(sizeof(b)))
			std::snprintf(b+n,sizeof(b)-n,"; GPU agua %.2f (max %.1f) relevo %.2f (max %.1f) ms",
			              gpuN[G_AGUA]?gpuSoma[G_AGUA]/gpuN[G_AGUA]:0.0,gpuMax[G_AGUA],
			              gpuN[G_RELEVO]?gpuSoma[G_RELEVO]/gpuN[G_RELEVO]:0.0,gpuMax[G_RELEVO]);
		zera();
		return std::string(b);
		}
	};

#endif
