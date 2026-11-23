"""Acrescenta ao Sandbox do SARndbox 2.8 os comandos do modo Rosto no cano de controle:
   rosto <foto.png> [giro 0/90/180/270] [espelho 0/1]   foto projetada que se deforma com a areia
   rostoAjuste <ampliação/cm> <deslocamento/cm> <mistura 0-1> <sombra>
   esculpir <rosto.dem>                                    relevo-alvo: vermelho/azul = tirar/pôr areia
   relevo                                                  volta à topografia normal
   tesouro [profundidade cm] [segundos] / tesouroFim       caça ao tesouro; achado, some e reaparece
                                                           em outro lugar depois dos segundos (20)
   fossil [trex|triceratops|pterodactilo|pegada] [cm] [s]   escavação de fóssil (mesmo mecanismo do tesouro)
   fossilFim                                               encerra a escavação
   animais peixes|carros|pessoas|todos|nada [quantidade]   peixes no lago, carrinhos e pessoas andando
   mundo <giro> <n com rosto> <painel|-> <0> [NOME[+]...] / mundoRosto NOME   o laboratório (Mundo.cpp)
   mundoVista tudo|principal|forense|mini|NOME             área do laboratório que aparece; NOME = seguir o membro
   copia [montanha|duas|vulcao|cratera|ilha|serra] [1-3] / copiaNova / copiaFim   copie o relevo
   mar peixe=N tubarao=N ...                                 vida na areia escolhida
   chuva <x 0-1> <y 0-1> [s] / secar                         nuvem de chuva num ponto; tira toda a água
   aguaNivel auto|mapa|areia|<cm> [absorção] / aguaInfo     nível do mar do lago dos bichos; diagnóstico da água
   efeito <tema>                                           animação do tema (lava, neve, poeira...)
   luz <força 0-1,5> [saturação 0-1]                        realce da luz (padrão 0,5 0,3; 0 0 = sem realce)
   gestoTeste <x 0-1> <y 0-1> [s] / gestoDiag [n] / gestoAjuste [nome valor] / gestoGrava [s]
                                                           dedo apontado (Gesto.cpp): de mentira; medidas das
                                                           próximas n procuras no log; limiares; grava até 20 s
                                                           do sensor em /dev/shm/gesto-* (testes/analisa_gesto.cpp)
   labirinto [facil|normal|dificil] / labirintoNovo / labirintoFim (labirintoTeste: alvo na nascente)
                                                           labirinto de água: uma nascente joga água de
                                                           verdade; cavem canais até ela chegar ao alvo
   copia [forma|sorteio] [1-3] / copiaNova / copiaFim (copiaTeste: dá a rodada por conseguida)
   jogo corrida|vila|rota|vulcao [1|2|3] / jogoNovo / jogoFim
                                                           jogos de água (Jogos.cpp); jogoTeste põe as
                                                           peças em cima da primeira nascente
Também mede quadros por segundo (linha "Desempenho:" no log a cada 30 s) em vez de encher o
log com "Ran out of time" a cada quadro.
"""
import sys

def rep(s, old, new):
    if old not in s:
        sys.exit(f"trecho não encontrado: {old[:70]!r}")
    return s.replace(old, new, 1)

h = open("Sandbox.h").read()
if "FaceImage" not in h:
    h = rep(h, "class DEM;\n", "class DEM;\nclass FaceImage;\n") if "class DEM;\n" in h else rep(h, "class Sandbox:public", "class FaceImage;\nclass DEM;\n\nclass Sandbox:public")
    h = rep(h, "\tDEM* activeDem; // The currently active DEM\n", """\tDEM* activeDem; // The currently active DEM
	FaceImage* faceImage; // Modo Rosto: foto projetada (0 = desligado)
	DEM* faceDem; // Modo Esculpir: relevo-alvo do rosto (0 = desligado)
	double faceBoxMin[2],faceBoxMax[2]; // Extensão da caixa de areia em cm (espaço da caixa)
	double faceRefTime; // Última medição da altura média da areia
""")
    h = rep(h, "\t/* Private methods: */\n", """\t/* Private methods: */
	void setFaceMode(FaceImage* newFaceImage,DEM* newFaceDem); // Troca entre relevo, rosto e esculpir
	float measureSandHeight(void) const; // Altura mediana da areia (cm acima do plano base)
""")
    # Extras (Projetor-LAOB): tesouro, peixes e carrinhos
    h = rep(h, '#include "Types.h"\n', '#include "Types.h"\n#include "Extras.h"\n#include "Perfil.h"\n')
    h = rep(h, "\tdouble faceRefTime; // Última medição da altura média da areia\n", """\tdouble faceRefTime; // Última medição da altura média da areia
	mutable Extras extras; // Tesouro, lago, peixes, carrinhos e labirinto sobre o relevo
	mutable double labLeitura; // Labirinto: última medição da água no alvo
	mutable std::vector<GLfloat> labQuantidade,labFundo; // Labirinto: leitura da água e do fundo
	mutable unsigned int quadrosAgua,atrasosAgua; // Desempenho: passos de água e quantos atrasaram
	mutable double aguaPedida,aguaFaltou; // Desempenho: tempo de água que devia ser simulado e o que ficou para trás
	double desempenhoTempo; unsigned int quadros; // Desempenho: quadros desde a última linha no log
	mutable Perfil perfil; // Desempenho: tempo de cada quadro (p95/p99) e das passadas de CPU/GPU (Perfil.h)
	double extrasMapaTempo,extrasUltimoPasso; // Última atualização do mapa e do movimento
	float tesouroPedido; // Profundidade do tesouro pedido antes de haver mapa (<0 = nenhum)
""")
    h = rep(h, "\tfloat measureSandHeight(void) const; // Altura mediana da areia (cm acima do plano base)\n",
            "\tfloat measureSandHeight(void) const; // Altura mediana da areia (cm acima do plano base)\n"
            "\tvoid atualizaExtras(void); // Mapa de alturas, movimento e tesouro (a cada quadro)\n"
            "\tvoid atualizaGesto(double agora); // Dedo apontado (para cima ou para a areia): derruba comida embaixo dele\n"
            "\tvoid gestoDedoCai(void); // Comida embaixo do dedo, se há quem coma aquilo (terra ou água)\n"
            "\tfloat medeAguaNoAlvo(GLContextData& contextData) const; // Labirinto: água no alvo (cm)\n"
            "\tvoid medeJogo(GLContextData& contextData) const; // Jogos de água: água em cada peça\n")
    # Dedo apontado para cima (Projetor-LAOB, 18/11/2026): o quadro cru do sensor vai para o detector (Gesto.cpp)
    h = rep(h, '#include "Extras.h"\n', '#include "Extras.h"\n#include "Gesto.h"\n')
    h = rep(h, "\tfloat tesouroPedido; // Profundidade do tesouro pedido antes de haver mapa (<0 = nenhum)\n", """\tfloat tesouroPedido; // Profundidade do tesouro pedido antes de haver mapa (<0 = nenhum)
	/* Dedo apontado (Gesto.cpp): derruba comida embaixo dele; só com bichos na areia (ou gravando: gestoGrava) */
	Threads::TripleBuffer<Kinect::FrameBuffer> brutoFrames; // Quadros crus do sensor (só enquanto brutoQuer)
	volatile bool brutoQuer=false; // A thread da câmera só passa o quadro cru quando o detector quer
	Gesto::Detector gestoDetector;
	Gesto::Rastro gestoRastro; // O dedo confirmado (cm, espaço da caixa); perto dele a mão não chove
	Gesto::Rastro gestoRastroGrava; // Gravando sem bichos: o dedo visto só vai para o log
	std::vector<Gesto::MaoAberta> gestoMaos; // As mãos abertas do HandExtractor, na caixa
	Gesto::Gravador gestoGravador; double gestoGravaAte=0.0; float gestoGravaPedido=0.0f; // gestoGrava: até quando; pedido (s)
	double gestoTempo=0.0,gestoComidaTempo=0.0,gestoTesteAte=0.0; // Última procura, última comida, fim do teste
	float gestoTesteX=0.0f,gestoTesteY=0.0f; // Dedo de teste (cm)
	void gestoEvento(double agora,int ev); // Confirmou, solto, continua: a comida
	float gestoLugarX=0.0f,gestoLugarY=0.0f; bool gestoAgua=false,gestoCaiu=false; // Onde a comida caiu, se foi na água e se caiu (alguém come)
	int gestoDiag=0; // Quantas procuras ainda imprimir no log (gestoDiag)
	int gestoRet[4]={0,0,0,0}; // Pixels da área iluminada: x0, y0, x1, y1 (x1 = 0: ainda não medido)
	bool gestoTravou=false; // O relevo está travado embaixo de alguma mão (FrameFilter::trava)
	mutable double chuvaMaoVista=-100.0; bool chuvaMaoLigada=false; // A mão fez chover por último quando; avisado no log (o som da chuva)
	void gestoTrava(bool liga); // Passa a trava do detector (a mão que vem da beirada) ao FrameFilter
""")
    open("Sandbox.h", "w").write(h)

c = open("Sandbox.cpp").read()
if "FaceImage" not in c:
    c = rep(c, '#include "DEMTool.h"\n', '#include "DEMTool.h"\n#include "FaceImage.h"\n')
    # (na ordem da declaração, logo depois do activeDem: sem o aviso -Wreorder; 23/11/2026)
    c = rep(c, "\t activeDem(0),\n", "\t activeDem(0),\n\t faceImage(0),faceDem(0),faceRefTime(0.0),\n")
    if "#include <algorithm>" not in c:
        c = rep(c, "#include <vector>\n", "#include <vector>\n#include <algorithm>\n#include <cmath>\n")
    c = rep(c, """	boxSize=Geometry::dist(center,basePlaneCorners[0]);
	for(int i=1;i<4;++i)
		boxSize=Math::max(boxSize,Geometry::dist(center,basePlaneCorners[i]));
""", """	boxSize=Geometry::dist(center,basePlaneCorners[0]);
	for(int i=1;i<4;++i)
		boxSize=Math::max(boxSize,Geometry::dist(center,basePlaneCorners[i]));
	
	/* Modo Rosto: extensão da caixa em cm no espaço da caixa: */
	for(int i=0;i<2;++i)
		{
		faceBoxMin[i]=1.0e30;
		faceBoxMax[i]=-1.0e30;
		}
	for(int i=0;i<4;++i)
		{
		ONTransform::Point bc=boxTransform.transform(basePlaneCorners[i]);
		for(int j=0;j<2;++j)
			{
			faceBoxMin[j]=Math::min(faceBoxMin[j],double(bc[j]));
			faceBoxMax[j]=Math::max(faceBoxMax[j],double(bc[j]));
			}
		}
	
	/* Só a parte da caixa que o projetor ilumina: a caixa calibrada pode ser maior que a imagem
	   projetada. Cada canto da imagem do projetor é levado até o plano base: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		if(rsIt->fixProjectorView&&rsIt->projectorTransformValid)
			{
			PTransform inv=Geometry::invert(rsIt->projectorTransform);
			double xs[4],ys[4];
			for(int i=0;i<4;++i)
				{
				double nx=(i&1)?1.0:-1.0,ny=(i&2)?1.0:-1.0;
				PTransform::Point p0=inv.transform(PTransform::Point(nx,ny,-1.0));
				PTransform::Point p1=inv.transform(PTransform::Point(nx,ny,1.0));
				double d0=basePlane.calcDistance(p0),d1=basePlane.calcDistance(p1);
				PTransform::Point p=p0+(p1-p0)*(d0/(d0-d1));
				ONTransform::Point bc=boxTransform.transform(p);
				xs[i]=bc[0];
				ys[i]=bc[1];
				}
			for(int i=0;i<4;++i)
				for(int j=i+1;j<4;++j)
					{
					if(xs[j]<xs[i]) std::swap(xs[i],xs[j]);
					if(ys[j]<ys[i]) std::swap(ys[i],ys[j]);
					}
			/* Retângulo de dentro (o quadrilátero iluminado pode estar meio torto): */
			faceBoxMin[0]=Math::max(faceBoxMin[0],xs[1]);
			faceBoxMax[0]=Math::min(faceBoxMax[0],xs[2]);
			faceBoxMin[1]=Math::max(faceBoxMin[1],ys[1]);
			faceBoxMax[1]=Math::min(faceBoxMax[1],ys[2]);
			std::cout<<"Modo Rosto: área iluminada x "<<faceBoxMin[0]<<".."<<faceBoxMax[0]<<" y "<<faceBoxMin[1]<<".."<<faceBoxMax[1]<<" cm"<<std::endl;
			break;
			}
""")
    c = rep(c, """				else
					std::cerr<<"Unrecognized control pipe command "<<tokens[0]<<std::endl;""", """				else if(isToken(tokens[0],"rosto"))
					{
					if(tokens.size()>=2)
						{
						try
							{
							FaceImage* f=new FaceImage(tokens[1].c_str());
							int giro=tokens.size()>=3?atoi(tokens[2].c_str()):0;
							bool espelho=tokens.size()>=4&&atoi(tokens[3].c_str())!=0;
							f->setTransform(OGTransform(boxTransform),faceBoxMin,faceBoxMax,giro,espelho);
							if(faceImage!=0)
								for(int i=0;i<4;++i)
									f->params[i]=faceImage->params[i];
							f->reference=measureSandHeight();
							faceRefTime=Vrui::getApplicationTime();
							setFaceMode(f,0);
							std::cout<<"Modo Rosto: "<<tokens[1]<<" (giro "<<giro<<", espelho "<<espelho<<", areia em "<<f->reference<<" cm)"<<std::endl;
							}
						catch(const std::runtime_error& err)
							{
							std::cerr<<"Não consegui abrir a foto "<<tokens[1]<<": "<<err.what()<<std::endl;
							}
						}
					else
						std::cerr<<"uso: rosto <foto> [giro] [espelho]"<<std::endl;
					}
				else if(isToken(tokens[0],"rostoAjuste"))
					{
					if(tokens.size()==5&&faceImage!=0)
						{
						for(int i=0;i<4;++i)
							faceImage->params[i]=GLfloat(atof(tokens[1+i].c_str()));
						}
					else
						std::cerr<<"uso (no modo Rosto): rostoAjuste <ampliação> <deslocamento> <mistura> <sombra>"<<std::endl;
					}
				else if(isToken(tokens[0],"esculpir"))
					{
					if(tokens.size()==2)
						{
						try
							{
							DEM* d=new DEM;
							d->load(tokens[1].c_str());
							float ref=measureSandHeight();
							d->setTransform(OGTransform::translate(OGTransform::Vector(0,0,-ref))*OGTransform(boxTransform),Scalar(1),Scalar(0));
							setFaceMode(0,d);
							std::cout<<"Modo Esculpir: "<<tokens[1]<<" (areia em "<<ref<<" cm)"<<std::endl;
							}
						catch(const std::runtime_error& err)
							{
							std::cerr<<"Não consegui abrir o relevo "<<tokens[1]<<": "<<err.what()<<std::endl;
							}
						}
					else
						std::cerr<<"uso: esculpir <arquivo.dem>"<<std::endl;
					}
				else if(isToken(tokens[0],"relevo"))
					{
					setFaceMode(0,0);
					std::cout<<"Modo Relevo"<<std::endl;
					}
				else
					std::cerr<<"Unrecognized control pipe command "<<tokens[0]<<std::endl;""")
    # reavalia a altura média da areia de tempos em tempos (o rosto acompanha quem mexe na areia)
    c = rep(c, """	/* Check if the filtered frame has been updated: */
	if(filteredFrames.lockNewValue())
		{
		/* Update the depth image renderer's depth image: */
		depthImageRenderer->setDepthImage(filteredFrames.getLockedValue());
		}
""", """	/* Check if the filtered frame has been updated: */
	if(filteredFrames.lockNewValue())
		{
		/* Update the depth image renderer's depth image: */
		depthImageRenderer->setDepthImage(filteredFrames.getLockedValue());
		
		/* Modo Rosto: acompanha a altura média da areia a cada 3 s */
		if(faceImage!=0&&Vrui::getApplicationTime()-faceRefTime>3.0)
			{
			faceImage->reference=measureSandHeight();
			faceRefTime=Vrui::getApplicationTime();
			}
		}
""")
    c = rep(c, "void Sandbox::toggleDEM(DEM* dem)\n", """float Sandbox::measureSandHeight(void) const
	{
	/* Mediana da altura da areia numa grade espaçada da imagem de profundidade: */
	const Kinect::FrameBuffer& fb=filteredFrames.getLockedValue();
	const float* d=fb.getData<float>();
	if(d==0)
		return 0.0f;
	int w=fb.getSize(0),h=fb.getSize(1);
	PTransform full=PTransform(OGTransform(boxTransform));
	full*=depthImageRenderer->getDepthProjection();
	std::vector<float> hs;
	for(int y=h/10;y<h*9/10;y+=6)
		for(int x=w/10;x<w*9/10;x+=6)
			{
			float z=d[y*w+x];
			if(!std::isfinite(z)||z<=0.0f||z>=2047.0f)
				continue;
			Point p=full.transform(Point(Scalar(x)+Scalar(0.5),Scalar(y)+Scalar(0.5),Scalar(z)));
			if(p[0]<faceBoxMin[0]||p[0]>faceBoxMax[0]||p[1]<faceBoxMin[1]||p[1]>faceBoxMax[1]||std::abs(p[2])>80.0)
				continue;
			hs.push_back(float(p[2]));
			}
	if(hs.empty())
		return 0.0f;
	std::nth_element(hs.begin(),hs.begin()+hs.size()/2,hs.end());
	return hs[hs.size()/2];
	}

void Sandbox::setFaceMode(FaceImage* newFaceImage,DEM* newFaceDem)
	{
	FaceImage* oldFace=faceImage;
	DEM* oldDem=faceDem;
	faceImage=newFaceImage;
	faceDem=newFaceDem;
	
	/* O relevo-alvo do rosto substitui qualquer outro DEM ativo; ao sair, desliga o dele: */
	if(newFaceDem!=0)
		activeDem=newFaceDem;
	else if(activeDem==oldDem)
		activeDem=0;
	
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		if(rsIt->fixProjectorView)
			{
			rsIt->surfaceRenderer->setFaceImage(faceImage);
			rsIt->surfaceRenderer->setDem(activeDem);
			}
	
	delete oldFace;
	if(oldDem!=0&&oldDem!=activeDem)
		delete oldDem;
	}

void Sandbox::toggleDEM(DEM* dem)
""")
    # --- Extras (Projetor-LAOB)
    c = rep(c, "\t faceImage(0),faceDem(0),faceRefTime(0.0),\n",
            "\t faceImage(0),faceDem(0),faceRefTime(0.0),\n"
            "\t labLeitura(0.0),quadrosAgua(0),atrasosAgua(0),aguaPedida(0.0),aguaFaltou(0.0),desempenhoTempo(0.0),quadros(0),\n"
            "\t extrasMapaTempo(0.0),extrasUltimoPasso(0.0),tesouroPedido(-1.0f),\n")
    c = rep(c, """<<" cm"<<std::endl;
			break;
			}
""", """<<" cm"<<std::endl;
			break;
			}
	extras.setArea(faceBoxMin,faceBoxMax,OGTransform(boxTransform));
""")
    c = rep(c, """				else if(isToken(tokens[0],"relevo"))""", """				else if(isToken(tokens[0],"tesouro"))
					{
					tesouroPedido=tokens.size()>=2?float(atof(tokens[1].c_str())):3.0f;
					if(tesouroPedido<1.0f)
						tesouroPedido=3.0f;
					extras.reaparece=tokens.size()>=3?float(atof(tokens[2].c_str())):20.0f;
					extras.quantosAchados=0;
					extras.tipoTesouro=1;
					extras.tesouro=true; // liga o shader já; o lugar é sorteado com o mapa pronto
					extras.achado=false;
					extras.tz=-1000.0f;
					setFaceMode(0,0);
					std::cout<<"Tesouro: escondendo a "<<tesouroPedido<<" cm"<<std::endl;
					}
				else if(isToken(tokens[0],"efeito"))
					{
					int e=tokens.size()>=2?Extras::efeitoDoTema(tokens[1].c_str()):-1;
					if(e<0)
						std::cerr<<"uso: efeito classico|vulcao|inverno|marte|deserto|oceano|arco-iris|nada"<<std::endl;
					else
						{
						extras.efeito=e;
						std::cout<<"Efeito: "<<tokens[1]<<std::endl;
						}
					}
				else if((isToken(tokens[0],"comida")||isToken(tokens[0],"maoTeste")||isToken(tokens[0],"racao"))&&tokens.size()>=3)
					{
					/* comida jogada pelo painel: fração x, y da área iluminada e segundos. Num ponto de água cai
					   ração (peixes, tartarugas, patos... vêm comer); na terra, a tigela (os pássaros pousam em volta,
					   os cachorros sentam do lado). "comida x y s terra|agua": só cai se o ponto for daquele jeito
					   (o painel manda quando só há bichos que comem um dos dois). "racao x y s": ração na água mais
					   perto (sem água por perto, vira a tigela). ("maoTeste" é o nome antigo do comando.) */
					float seg=tokens.size()>=4?std::max(1.0f,std::min(120.0f,float(atof(tokens[3].c_str())))):15.0f;
					float fx=std::max(0.0f,std::min(1.0f,float(atof(tokens[1].c_str()))));
					float fy=std::max(0.0f,std::min(1.0f,float(atof(tokens[2].c_str()))));
					float x=extras.bmin[0]+fx*(extras.bmax[0]-extras.bmin[0]);
					float y=extras.bmin[1]+fy*(extras.bmax[1]-extras.bmin[1]);
					bool ehAgua=extras.aguaNoPonto(x,y);
					int so=tokens.size()>=5?(isToken(tokens[4],"agua")?1:isToken(tokens[4],"terra")?0:-1):-1;
					if(so>=0&&so!=(ehAgua?1:0))
						std::cout<<"Comida: nada caiu ("<<(ehAgua?"agua":"terra")<<")"<<std::endl;
					else
						{
						bool naAgua=extras.jogarComidaEm(x,y,seg,isToken(tokens[0],"racao")?1:-1,8);
						std::cout<<"Comida em x "<<x<<" y "<<y<<" por "<<seg<<" s ("<<(naAgua?"agua":"terra")<<")"<<std::endl;
						}
					}
				else if(isToken(tokens[0],"gestoTeste")&&tokens.size()>=3)
					{
					/* dedo apontado de mentira (testes sem mão): fração x, y da área iluminada e segundos; passa pelo
					   mesmo caminho do dedo de verdade (confirmação, comida, sem chuva perto) */
					float fx=std::max(0.0f,std::min(1.0f,float(atof(tokens[1].c_str()))));
					float fy=std::max(0.0f,std::min(1.0f,float(atof(tokens[2].c_str()))));
					float seg=tokens.size()>=4?std::max(0.5f,std::min(60.0f,float(atof(tokens[3].c_str())))):3.0f;
					gestoTesteX=extras.bmin[0]+fx*(extras.bmax[0]-extras.bmin[0]);
					gestoTesteY=extras.bmin[1]+fy*(extras.bmax[1]-extras.bmin[1]);
					gestoTesteAte=Vrui::getApplicationTime()+seg;
					std::cout<<"Gesto: teste em "<<gestoTesteX<<" "<<gestoTesteY<<" por "<<seg<<" s"
					         <<(extras.querComida()?"":" (sem bichos na areia: não vale)")<<std::endl;
					}
				else if(isToken(tokens[0],"gestoDiag"))
					{
					/* as medidas de cada mão vista nas próximas n procuras (para calibrar com a mão de verdade) */
					gestoDiag=tokens.size()>=2?std::max(1,std::min(100,atoi(tokens[1].c_str()))):1;
					std::cout<<"GestoDiag: medindo as próximas "<<gestoDiag<<" procura(s)"
					         <<(extras.querComida()?"":" (desligado: só procura com bichos na areia ou gravando)")<<std::endl;
					}
				else if(isToken(tokens[0],"gestoAjuste"))
					{
					/* gestoAjuste nome valor: muda um limiar do detector na hora; sem nada, só mostra */
					if(tokens.size()>=3&&!gestoDetector.lim.ajusta(tokens[1],float(atof(tokens[2].c_str()))))
						std::cout<<"GestoAjuste: não existe "<<tokens[1]<<std::endl;
					std::cout<<"GestoAjuste: "<<gestoDetector.lim.texto()<<std::endl;
					}
				else if(isToken(tokens[0],"gestoGrava"))
					{
					/* gestoGrava [s]: grava o quadro cru e o filtrado (~30 por segundo, até 20 s: ~0,7 MB por quadro, ~400 MB
					   em 20 s) em /dev/shm/gesto-* para analisar a mão de verdade fora daqui (testes/analisa_gesto.cpp);
					   grava mesmo sem bichos. Cada gravação nova apaga as anteriores (Gesto::Gravador::abre) */
					float seg=tokens.size()>=2?float(atof(tokens[1].c_str())):10.0f;
					seg=seg>0.0f?std::min(20.0f,std::max(1.0f,seg)):10.0f;
					if(gestoGravador.gravando())
						std::cout<<"Gesto: já gravando em "<<gestoGravador.pasta<<std::endl;
					else
						{
						gestoGravaPedido=seg;
						std::cout<<"Gesto: gravação pedida ("<<seg<<" s)"<<std::endl;
						}
					}
				else if(isToken(tokens[0],"tesouroTeste"))
					{
					/* Para ver o desenho (e para o teste automático): mostra o baú ou o fóssil como se tivesse sido achado */
					if(extras.tesouro)
						{
						extras.achado=true;
						extras.quandoAchou=extras.tempo;
						std::cout<<"Tesouro: teste, mostrando"<<std::endl;
						}
					}
				else if(isToken(tokens[0],"tesouroFim"))
					{
					extras.tesouro=false;
					extras.tipoTesouro=1;
					tesouroPedido=-1.0f;
					std::cout<<"Tesouro desligado"<<std::endl;
					}
				else if(isToken(tokens[0],"fossil"))
					{
					int tipo=2;
					float prof=3.0f;
					float reap=30.0f;
					int argIdx=1;
					if(tokens.size()>=2)
						{
						if(isToken(tokens[1],"trex")||isToken(tokens[1],"t-rex")||isToken(tokens[1],"2")) { tipo=2; argIdx++; }
						else if(isToken(tokens[1],"triceratops")||isToken(tokens[1],"3")) { tipo=3; argIdx++; }
						else if(isToken(tokens[1],"pterodactilo")||isToken(tokens[1],"ptero")||isToken(tokens[1],"4")) { tipo=4; argIdx++; }
						else if(isToken(tokens[1],"pegada")||isToken(tokens[1],"5")) { tipo=5; argIdx++; }
						else if(isdigit(tokens[1][0])) { tipo=atoi(tokens[1].c_str()); argIdx++; }
						}
					if(int(tokens.size())>argIdx)
						prof=float(atof(tokens[argIdx].c_str()));
					if(prof<1.0f) prof=3.0f;
					if(int(tokens.size())>argIdx+1)
						reap=float(atof(tokens[argIdx+1].c_str()));
					tesouroPedido=prof;
					extras.reaparece=reap;
					extras.quantosAchados=0;
					extras.tipoTesouro=tipo;
					extras.tesouro=true;
					extras.achado=false;
					extras.tz=-1000.0f;
					setFaceMode(0,0);
					std::cout<<"Fossil: escondendo tipo "<<tipo<<" a "<<prof<<" cm"<<std::endl;
					}
				else if(isToken(tokens[0],"fossilFim"))
					{
					extras.tesouro=false;
					extras.tipoTesouro=1;
					tesouroPedido=-1.0f;
					std::cout<<"Fossil desligado"<<std::endl;
					}
				else if(isToken(tokens[0],"mundo")&&tokens.size()>=5)
					{
					/* mundo <giro> <quantos têm rosto> <painel de rostos | -> <moradores> [NOME[+]...]:
					   o laboratório (Mundo.cpp); mandar de novo com os mesmos nomes só atualiza quem está presente.
					   <moradores> é uma posição reservada: era da cidade LAOB (tirada em 16/11/2026) e é ignorada (o painel
					   manda 0). Fica no protocolo de propósito (revisão de 23/11/2026): tirar exigiria trocar o painel e o
					   SARndbox juntos, senão os nomes andam uma posição */
					std::vector<std::string> lista;
					for(size_t i=5;i<tokens.size();++i)
						lista.push_back(tokens[i]);
					if(tokens[3]!="-"&&!extras.carregaAtlas(tokens[3].c_str()))
						std::cout<<"Mundo: não consegui ler o painel de rostos "<<tokens[3]<<std::endl;
					bool ja=extras.modo==Extras::MUNDO;
					if(!ja)
						setFaceMode(0,0);
					extras.mundo(lista,atoi(tokens[1].c_str()),atoi(tokens[2].c_str()));
					if(!ja)
						std::cout<<"Animais: mundo ("<<lista.size()<<")"<<std::endl;
					}
				else if(isToken(tokens[0],"mundoHora")&&tokens.size()>=2)
					{
					/* para ver o laboratório de dia ou de noite sem esperar: mundoHora 21 / mundoHora auto */
					extras.horaFixa=isToken(tokens[1],"auto")?-1.0f:float(atof(tokens[1].c_str()));
					std::cout<<"Mundo: hora "<<tokens[1]<<std::endl;
					}
				else if(isToken(tokens[0],"mundoRosto")&&tokens.size()>=2)
					{
					if(extras.mundoRosto(tokens[1].c_str()))
						std::cout<<"Mundo: mostrando o rosto de "<<tokens[1]<<std::endl;
					}
				else if(isToken(tokens[0],"mundoVista")&&tokens.size()>=2)
					{
					/* mundoVista tudo|principal|forense|mini|NOME (20/11/2026): a câmera do laboratório
					   (Mundo.cpp); a linha "Mundo: vista ..." vai para o log pelas mensagens dos extras */
					extras.mundoVista(tokens[1].c_str());
					}
				else if(isToken(tokens[0],"copia"))
					{
					/* copia [forma|sorteio] [1-3]: copie o relevo */
					int f=tokens.size()>=2?Extras::formaDoNome(tokens[1].c_str()):-1;
					int n=tokens.size()>=3?atoi(tokens[2].c_str()):2;
					setFaceMode(0,0);
					extras.copiaRodadas=0;
					extras.copiaNova(f,n);
					}
				else if(isToken(tokens[0],"copiaNova"))
					{
					if(extras.copia)
						extras.copiaNova(-1,extras.copiaNivel);
					}
				else if(isToken(tokens[0],"copiaTeste"))
					{
					/* teste (23/11/2026): dá a rodada do Copie por conseguida agora (a comemoração aparece) */
					if(extras.copia&&extras.copiaConseguiu==0.0)
						{
						extras.copiaConseguiu=extras.tempo;
						std::cout<<"Copia: teste, conseguiu"<<std::endl;
						}
					}
				else if(isToken(tokens[0],"copiaFim"))
					{
					if(extras.copia)
						std::cout<<"Copia desligada"<<std::endl;
					extras.copia=false;
					}
				else if(isToken(tokens[0],"mar"))
					{
					/* mar peixe=8 tubarao=1 tartaruga=2 ...: os animais marinhos escolhidos no painel */
					int q[Extras::NUM_ESPECIES];
					int total=0;
					for(int k=0;k<Extras::NUM_ESPECIES;++k)
						q[k]=0;
					for(size_t t=1;t<tokens.size();++t)
						{
						size_t ig=tokens[t].find('=');
						if(ig==std::string::npos)
							continue;
						for(int k=0;k<Extras::NUM_ESPECIES;++k)
							if(tokens[t].substr(0,ig)==Extras::nomeEspecie(k))
								{
								q[k]=std::max(0,std::min(int(Extras::MAX_AGENTES),atoi(tokens[t].substr(ig+1).c_str()))); // como o painel: até 32 de uma espécie (e 32 no total)
								total+=q[k];
								}
						}
					setFaceMode(0,0);
					extras.mar(q);
					std::cout<<"Animais: mar ("<<std::min(total,int(Extras::MAX_AGENTES))<<")"<<std::endl;
					}
				else if(isToken(tokens[0],"luz")&&tokens.size()>=2)
					{
					/* luz <força 0-1,5> [saturação 0-1]: realce da luz na cor final (22/11/2026; 0 0 = como o SARndbox) */
					SurfaceRenderer::luzRealce[0]=GLfloat(std::max(0.0,std::min(1.5,atof(tokens[1].c_str()))));
					if(tokens.size()>=3)
						SurfaceRenderer::luzRealce[1]=GLfloat(std::max(0.0,std::min(1.0,atof(tokens[2].c_str()))));
					std::cout<<"Luz: forca "<<SurfaceRenderer::luzRealce[0]<<" saturacao "<<SurfaceRenderer::luzRealce[1]<<std::endl;
					}
				else if(isToken(tokens[0],"chuva")&&tokens.size()>=3)
					{
					/* chuva <x 0-1> <y 0-1> [segundos]: uma nuvem joga água nesse ponto da areia */
					float seg=tokens.size()>=4?float(atof(tokens[3].c_str())):8.0f;
					extras.chove(float(atof(tokens[1].c_str())),float(atof(tokens[2].c_str())),seg);
					std::cout<<"Chuva: nuvem por "<<seg<<" s"<<std::endl;
					}
				else if(isToken(tokens[0],"aguaNivel")&&tokens.size()>=2)
					{
					/* nível do mar do lago dos bichos: "auto" (padrão: o maior entre o azul do mapa e 1,5 cm abaixo
					   da areia mediana), "mapa" (0 cm), "areia" (o jeito antigo) ou cm; absorção opcional em cm/s */
					if(isToken(tokens[1],"auto"))
						extras.modoNivel=0;
					else if(isToken(tokens[1],"areia"))
						extras.modoNivel=2;
					else
						{
						extras.modoNivel=1;
						extras.nivelFixo=isToken(tokens[1],"mapa")?0.0f:float(atof(tokens[1].c_str()));
						}
					if(tokens.size()>=3)
						extras.absorcao=float(std::max(0.0,std::min(1.0,atof(tokens[2].c_str()))));
					if(extras.lencol())
						extras.nivelAgua=extras.nivelLago();
					std::cout<<"Agua: nivel do mar "<<extras.nivelLago()<<" cm, absorcao "<<extras.absorcao<<" cm/s"<<std::endl;
					}
				else if(isToken(tokens[0],"aguaInfo"))
					{
					/* diagnóstico: areia mediana, nível do mar e quanto da área está com água */
					int n=0,molhadas=0,fundas=0,abaixo=0;
					float maior=0.0f,soma=0.0f;
					for(size_t k=0;k<extras.aguaSim.size();++k)
						{
						++n;
						float h=extras.aguaSim[k];
						maior=std::max(maior,h);
						soma+=h;
						if(h>=0.15f) ++molhadas;
						if(h>=0.45f) ++fundas;
						}
					for(int cy=0;cy<extras.celulasY();++cy)
						for(int cx=0;cx<extras.celulasX();++cx)
							{
							float x=extras.cantoMin()[0]+(cx+0.5f)*extras.celulaCm(),y=extras.cantoMin()[1]+(cy+0.5f)*extras.celulaCm();
							float h=extras.altura(x,y);
							if(h>-999.0f&&h<extras.nivelLago())
								++abaixo;
							}
					std::cout<<"AguaInfo: areia mediana "<<extras.referencia<<" cm, nivel do mar "<<extras.nivelLago()
					         <<" cm (lago "<<(extras.lencol()?"ligado":"desligado")<<"), abaixo do nivel "<<abaixo<<" celulas, com agua "<<molhadas
					         <<", funda (bichos) "<<fundas<<" de "<<n<<", media "<<(n>0?soma/n:0.0f)<<" cm, maior "<<maior<<" cm"<<std::endl;
					}
				else if(isToken(tokens[0],"secar"))
					{
					extras.labZerarAgua=true;
					for(int k=0;k<Extras::MAX_CHUVAS;++k)
						extras.chuvas[k].ate=-1.0;
					std::cout<<"Chuva: a água foi seca"<<std::endl;
					}
				else if(isToken(tokens[0],"animais"))
					{
					int m=-1;
					if(tokens.size()>=2)
						m=isToken(tokens[1],"peixes")?Extras::PEIXES:isToken(tokens[1],"carros")?Extras::CARROS:
						  isToken(tokens[1],"pessoas")?Extras::PESSOAS:
						  isToken(tokens[1],"todos")?Extras::TODOS:isToken(tokens[1],"nada")?Extras::NADA:-1;
					if(m<0)
						std::cerr<<"uso: animais peixes|carros|pessoas|todos|nada [quantidade]"<<std::endl;
					else
						{
						int n=tokens.size()>=3?atoi(tokens[2].c_str()):(m==Extras::TODOS?14:10);
						if(m!=Extras::NADA)
							setFaceMode(0,0);
						extras.povoa(m,n);
						std::cout<<"Animais: "<<tokens[1]<<" ("<<n<<")"<<std::endl;
						}
					}
				else if(isToken(tokens[0],"relevo"))""")
    # 23/11/2026: os extras (mapa, movimento) e a escolha do shader vêm DEPOIS dos comandos do cano, no mesmo
    # quadro: a brincadeira nova mede o mapa de agora antes de sortear (achado 2) e a foto aberta vindo de uma
    # brincadeira não monta por um quadro o shader foto+extras (achado 11). Antes rodavam antes do cano.
    c = rep(c, """	if(frameRateTextField!=0&&Vrui::getWidgetManager()->isVisible(waterControlDialog))
""", """	/* Tesouro, lago, peixes e carrinhos: */
	atualizaExtras();
	
	/* Projetor-LAOB: o programa da superfície acompanha os extras e a foto de agora (SurfaceRenderer::chaveShader) */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		if(rsIt->fixProjectorView)
			rsIt->surfaceRenderer->setExtras(extras.ativo()&&faceImage==0?&extras:0);
	
	if(frameRateTextField!=0&&Vrui::getWidgetManager()->isVisible(waterControlDialog))
""")
    c = rep(c, "void Sandbox::setFaceMode(FaceImage* newFaceImage,DEM* newFaceDem)\n", """void Sandbox::atualizaExtras(void)
	{
	double agora=Vrui::getApplicationTime();
	float dt=extrasUltimoPasso>0.0?float(agora-extrasUltimoPasso):0.0f;
	extrasUltimoPasso=agora;
	/* Dedo apontado para cima: comida embaixo dele (só com bichos na areia) */
	atualizaGesto(agora);
	if(!extras.ativo())
		{
		/* desligados, o mapa não é medido: fica esquecido, e a próxima brincadeira mede o de agora já no
		   quadro em que chega (23/11/2026; antes sorteava no mapa de horas atrás, ou sem mapa nenhum) */
		extras.esqueceMapa();
		extrasMapaTempo=0.0;
		return;
		}
	
	/* Mapa de alturas da área iluminada, 2x por segundo: */
	if(agora-extrasMapaTempo>0.5)
		{
		extrasMapaTempo=agora;
		const Kinect::FrameBuffer& fb=filteredFrames.getLockedValue();
		const float* d=fb.getData<float>();
		if(d!=0)
			{
			int w=fb.getSize(0),h=fb.getSize(1);
			PTransform full=PTransform(OGTransform(boxTransform));
			full*=depthImageRenderer->getDepthProjection();
			extras.comecaMapa();
			for(int y=0;y<h;y+=3)
				for(int x=0;x<w;x+=3)
					{
					float z=d[y*w+x];
					if(!std::isfinite(z)||z<=0.0f||z>=2047.0f)
						continue;
					Point p=full.transform(Point(Scalar(x)+Scalar(0.5),Scalar(y)+Scalar(0.5),Scalar(z)));
					extras.amostra(float(p[0]),float(p[1]),float(p[2]));
					}
			extras.fechaMapa();
			if(extras.copia)
				extras.copiaMede();
			if(tesouroPedido>0.0f&&extras.novoTesouro(tesouroPedido))
				{
				if(extras.tipoTesouro>=2)
					std::cout<<"Fossil escondido em x "<<extras.tx<<" y "<<extras.ty<<" (escavar ate "<<extras.tz<<" cm)"<<std::endl;
				else
					std::cout<<"Tesouro escondido em x "<<extras.tx<<" y "<<extras.ty<<" (cavar até "<<extras.tz<<" cm)"<<std::endl;
				tesouroPedido=-1.0f;
				}
			if(extras.confereTesouro())
				{
				if(extras.tipoTesouro>=2)
					std::cout<<"Fossil encontrado! ("<<extras.quantosAchados<<" escavados)"<<std::endl;
				else
					std::cout<<"Tesouro encontrado! ("<<extras.quantosAchados<<" achados)"<<std::endl;
				}
			/* Achado: depois de um tempo ele some e é escondido de novo em outro lugar: */
			if(extras.horaDeReesconder()&&extras.novoTesouro(extras.profundidade))
				{
				if(extras.tipoTesouro>=2)
					std::cout<<"Fossil escondido em x "<<extras.tx<<" y "<<extras.ty<<" (escavar ate "<<extras.tz<<" cm; novo)"<<std::endl;
				else
					std::cout<<"Tesouro escondido em x "<<extras.tx<<" y "<<extras.ty<<" (cavar até "<<extras.tz<<" cm; novo)"<<std::endl;
				}
			}
		}
	
	/* Movimento dos peixes e carrinhos: */
	extras.passo(dt);
	}

namespace {

/* Projetor-LAOB: o quadro do sensor como o detector do dedo apontado vê (Gesto::QuadroKinect, Gesto.h): altura
   em cm acima do plano base no quadro cru e no filtrado, pela projeção pixel -> caixa */
typedef Gesto::QuadroKinect<Kinect::FrameSource::DepthCorrection::PixelCorrection> QuadroSensor;

}

void Sandbox::gestoDedoCai(void)
	{
	/* na terra cai a tigela (pássaros e cachorros), na água ração (bichos da água); se ninguém na areia come
	   o que cairia ali, não cai nada (como o painel, que avisa) */
	bool ehAgua=extras.aguaNoPonto(gestoLugarX,gestoLugarY);
	gestoCaiu=extras.comem(ehAgua);
	if(gestoCaiu)
		gestoAgua=extras.jogarComidaEm(gestoLugarX,gestoLugarY,20.0f,ehAgua?1:0);
	else
		gestoAgua=ehAgua;
	std::cout<<"Gesto: dedo em "<<gestoLugarX<<" "<<gestoLugarY<<" ("<<(gestoAgua?"agua":"terra")<<")"
	         <<(gestoCaiu?"":" nada caiu: ninguem come isso")<<std::endl;
	}

void Sandbox::gestoEvento(double agora,int ev)
	{
	if(ev==Gesto::Rastro::CONFIRMOU)
		{
		/* confirmado: comida embaixo do dedo; água ou terra é decidido agora (parada, a mão acaba entrando no
		   relevo do SARndbox e tira a água de baixo dela) e só de novo se o dedo andar */
		gestoLugarX=gestoRastro.x;
		gestoLugarY=gestoRastro.y;
		gestoComidaTempo=agora;
		gestoDedoCai();
		}
	else if(ev==Gesto::Rastro::SOLTOU)
		std::cout<<"Gesto: dedo solto"<<std::endl;
	else if(gestoRastro.ativo&&agora-gestoComidaTempo>=0.5)
		{
		/* enquanto aponta, cai mais comida (a de terra só renova: a tigela não recomeça do zero) */
		gestoComidaTempo=agora;
		float dx=gestoRastro.x-gestoLugarX,dy=gestoRastro.y-gestoLugarY;
		if(dx*dx+dy*dy>25.0f)
			{
			gestoLugarX=gestoRastro.x;
			gestoLugarY=gestoRastro.y;
			gestoDedoCai();
			}
		else if(gestoCaiu)
			extras.jogarComidaEm(gestoRastro.x,gestoRastro.y,20.0f,gestoAgua?1:0);
		}
	}

void Sandbox::gestoTrava(bool liga)
	{
	/* Relevo travado embaixo da mão (22/11/2026, pedido do Victor: com o dedo apontado "nada piscando em
	   volta"): o FrameFilter não aceita como areia o que está embaixo da mão que veio da beirada (Gesto.h).
	   Escreve pixel a pixel, sem zerar antes: o filtro (outra linha) nunca vê um quadro sem a trava no meio. */
	if(frameFilter==0||frameFilter->trava==0||gestoRet[2]==0)
		return;
	unsigned char* tr=frameFilter->trava;
	int w=int(frameSize[0]);
	const std::vector<unsigned char>& g=gestoDetector.trava;
	if(!liga||g.empty())
		{
		if(gestoTravou)
			for(int y=gestoRet[1];y<gestoRet[3];++y)
				std::fill(tr+size_t(y)*w+gestoRet[0],tr+size_t(y)*w+gestoRet[2],(unsigned char)0);
		gestoTravou=false;
		return;
		}
	int P=gestoDetector.gradeP(),X0=gestoDetector.gradeX0(),Y0=gestoDetector.gradeY0();
	int GW=gestoDetector.gradeW(),GH=gestoDetector.gradeH();
	if(P<1||size_t(GW)*size_t(GH)!=g.size())
		return;
	for(int y=gestoRet[1];y<gestoRet[3];++y)
		{
		int gy=std::max(0,std::min(GH-1,(y-Y0+P/2)/P));
		unsigned char* l=tr+size_t(y)*w;
		const unsigned char* gl=&g[size_t(gy)*GW];
		for(int x=gestoRet[0];x<gestoRet[2];++x)
			l[x]=gl[std::max(0,std::min(GW-1,(x-X0+P/2)/P))];
		}
	gestoTravou=true;
	}

void Sandbox::atualizaGesto(double agora)
	{
	/* Dedo apontado (Projetor-LAOB, 18/11/2026; refeito em 19/11 depois de não funcionar com a mão de verdade;
	   pedido do Victor): quem aponta com o indicador, os outros dedos fechados, para CIMA ou para BAIXO (para
	   a areia), derruba comida embaixo do dedo: na areia para os bichos de terra, na água para os da água. O
	   HandExtractor do SARndbox só acha a mão ABERTA (é dela a chuva) e nunca vê o dedo apontado; o detector é
	   o do Gesto.cpp, sobre o quadro CRU do sensor, em todo quadro (~30x por segundo) e só com bichos na areia (sem eles nem o
	   quadro cru é passado adiante) ou enquanto grava (gestoGrava). A mão aberta continua só fazendo chover. */
	/* só procura se há na areia quem coma (pássaros/cachorros na terra, bichos da água na água) */
	bool quer=extras.querComida()&&(extras.comem(false)||extras.comem(true))&&faceImage==0&&pixelDepthCorrection!=0;
	bool grava=(gestoGravaPedido>0.0f||gestoGravador.gravando())&&pixelDepthCorrection!=0;
	brutoQuer=quer||grava;
	if(!quer)
		{
		if(gestoRastro.ativo)
			std::cout<<"Gesto: dedo solto"<<std::endl;
		gestoRastro.larga();
		gestoTesteAte=0.0;
		gestoTrava(false);
		if(!grava)
			{
			gestoRastroGrava.larga();
			/* esquece também a areia congelada: quando os bichos voltarem (minutos ou horas depois), a
			   referência recomeça do quadro filtrado. Antes ficava a velha e todo monte feito nesse meio
			   tempo aparecia "com mão" e era congelado de novo por 8 s */
			gestoDetector.esquece();
			return;
			}
		}
	/* todo quadro novo do sensor (~30 por segundo; até 22/11/2026 eram 12): confirma mais rápido e com mais prova */
	if(agora-gestoTempo<1.0/40.0)
		return;
	gestoTempo=agora;
	if(quer&&agora<gestoTesteAte)
		{
		/* gestoTeste: um dedo de mentira, pelo mesmo tempo e a mesma comida do de verdade */
		gestoEvento(agora,gestoRastro.passo(agora,true,gestoTesteX,gestoTesteY));
		return;
		}
	if(!brutoFrames.lockNewValue())
		return; // nenhum quadro novo do sensor desde a última procura
	const Kinect::FrameBuffer& rb=brutoFrames.getLockedValue();
	const Kinect::FrameBuffer& fb=filteredFrames.getLockedValue();
	const Kinect::FrameSource::DepthPixel* rd=rb.getData<Kinect::FrameSource::DepthPixel>();
	const float* fd=fb.getData<float>();
	int w=int(frameSize[0]),h=int(frameSize[1]);
	if(rd==0||fd==0||int(rb.getSize(0))!=w||int(rb.getSize(1))!=h||int(fb.getSize(0))!=w||int(fb.getSize(1))!=h)
		return;
	PTransform full=PTransform(OGTransform(boxTransform));
	full*=depthImageRenderer->getDepthProjection();
	double mat[4][4];
	for(int i=0;i<4;++i)
		for(int j=0;j<4;++j)
			mat[i][j]=full.getMatrix()(i,j);
	const std::uint16_t* cru=reinterpret_cast<const std::uint16_t*>(rd);
	QuadroSensor q(mat,cru,fd,pixelDepthCorrection,0,0,w,h);
	if(gestoRet[2]==0)
		{
		/* uma vez: o retângulo de pixels que cobre a área iluminada (com 2 cm de folga) */
		int x0=w,y0=h,x1=0,y1=0;
		for(int y=0;y<h;y+=4)
			for(int x=0;x<w;x+=4)
				{
				float d=q.profAreia(x,y);
				if(d<0.0f)
					continue;
				double px,py;
				q.ponto(x+0.5,y+0.5,d,px,py);
				if(px>faceBoxMin[0]-2.0&&px<faceBoxMax[0]+2.0&&py>faceBoxMin[1]-2.0&&py<faceBoxMax[1]+2.0)
					{
					x0=std::min(x0,x);
					y0=std::min(y0,y);
					x1=std::max(x1,x+4);
					y1=std::max(y1,y+4);
					}
				}
		if(x1-x0<40||y1-y0<40)
			return; // ainda sem quadro filtrado
		gestoRet[0]=std::max(0,x0-4);
		gestoRet[1]=std::max(0,y0-4);
		gestoRet[2]=std::min(w,x1+4);
		gestoRet[3]=std::min(h,y1+4);
		std::cout<<"Gesto: area iluminada nos pixels x "<<gestoRet[0]<<".."<<gestoRet[2]<<" y "<<gestoRet[1]<<".."<<gestoRet[3]<<std::endl;
		}
	
	/* as mãos ABERTAS que o SARndbox achou (a chuva), na caixa: dedo em cima de uma delas não vale */
	gestoMaos.clear();
	if(handExtractor!=0)
		{
		const HandExtractor::HandList& maos=handExtractor->getLockedExtractedHands();
		for(HandExtractor::HandList::const_iterator hIt=maos.begin();hIt!=maos.end();++hIt)
			{
			Point hc=boxTransform.transform(hIt->center);
			Gesto::MaoAberta m={float(hc[0]),float(hc[1]),float(hc[2]),float(hIt->radius)};
			gestoMaos.push_back(m);
			}
		}
	/* procura, posição na caixa, beirada (4 cm, como a chuva), mão aberta, tempo: tudo no Gesto::Detector::passo
	   (o mesmo caminho da análise das gravações) */
	float area[4]={float(faceBoxMin[0]),float(faceBoxMin[1]),float(faceBoxMax[0]),float(faceBoxMax[1])};
	Gesto::Rastro& rastro=quer?gestoRastro:gestoRastroGrava;
	gestoDetector.medeTudo=gestoDiag>0;
	Gesto::Passo p;
	gestoDetector.passo(q,gestoRet[0],gestoRet[1],gestoRet[2],gestoRet[3],agora,area,gestoMaos,rastro,p);
	gestoTrava(quer);
	if(gestoDiag>0)
		{
		--gestoDiag;
		const std::vector<Gesto::Medida>& ms=gestoDetector.medidas;
		std::cout<<"GestoDiag: "<<ms.size()<<" candidato(s); "<<gestoMaos.size()<<" mao(s) aberta(s) do SARndbox"<<std::endl;
		for(size_t i=0;i<ms.size()&&i<8;++i)
			{
			const Gesto::Medida& md=ms[i];
			if(md.tipo==0)
				std::cout<<"GestoDiag: pico x "<<md.bx<<" y "<<md.by<<": ponta "<<md.sobreAreia<<" cm acima da areia; dedo acima do punho "
				         <<md.degrau<<" cm; pico "<<md.picoArea<<" cm2, "<<md.picoLarg<<" x "<<md.picoComp<<" cm; encosta "<<md.altaArea
				         <<" cm2; punho "<<md.punhoArea<<" cm2; alto em volta "<<int(100.0f*md.fracAlto+0.5f)<<"%; outro pico "<<md.outroArea<<" cm2"
				         <<(md.rastreio?" (rastro)":"")<<" -> "<<md.motivo<<std::endl;
			else
				std::cout<<"GestoDiag: contorno x "<<md.bx<<" y "<<md.by<<": dedo sai "<<md.comp<<" cm do punho, "<<md.larg<<" cm de largura, "
				         <<md.angulo<<" graus do braco; punho "<<md.degrau<<" cm acima da ponta; ponta "<<md.sobreAreia<<" cm acima da areia; corpo "
				         <<md.punhoArea<<" cm2; outra parte fina "<<md.outroArea<<" cm; braco "<<md.braco<<" cm acima da areia na borda"<<(md.rastreio?" (rastro)":"")<<" -> "<<md.motivo<<std::endl;
			}
		}
	
	/* gravação (gestoGrava): o recorte da área do detector com 24 px de folga, ~30 quadros por segundo */
	if(grava)
		{
		if(!gestoGravador.gravando()&&gestoGravaPedido>0.0f)
			{
			int rec[4]={std::max(0,gestoRet[0]-24),std::max(0,gestoRet[1]-24),std::min(w,gestoRet[2]+24),std::min(h,gestoRet[3]+24)};
			std::vector<float> corr;
			corr.reserve(size_t(2)*size_t(rec[2]-rec[0])*size_t(rec[3]-rec[1]));
			for(int y=rec[1];y<rec[3];++y)
				for(int x=rec[0];x<rec[2];++x)
					{
					const Kinect::FrameSource::DepthCorrection::PixelCorrection& pc=pixelDepthCorrection[y*w+x];
					corr.push_back(pc.scale);
					corr.push_back(pc.offset);
					}
			bool abriu=gestoGravador.abre("/dev/shm",w,h,rec,gestoRet,mat,area,corr,gestoDetector.lim.texto(),agora);
			if(!gestoGravador.apagadas.empty())
				std::cout<<"Gesto: apaguei a(s) gravação(ões) anterior(es) em /dev/shm: "<<gestoGravador.apagadas<<std::endl;
			if(abriu)
				{
				gestoGravaAte=agora+gestoGravaPedido;
				std::cout<<"Gesto: gravando "<<gestoGravaPedido<<" s em "<<gestoGravador.pasta
				         <<(quer?"":" (sem bichos na areia: só grava e mede, nada cai)")<<std::endl;
				}
			else
				std::cout<<"Gesto: não deu para gravar em /dev/shm"<<(gestoGravador.erro.empty()?"":": ")<<gestoGravador.erro<<std::endl;
			gestoGravaPedido=0.0f;
			}
		if(gestoGravador.gravando())
			{
			gestoGravador.quadro(agora,cru,fd,gestoMaos,p.viu,p.x,p.y);
			if(agora>=gestoGravaAte||!gestoGravador.gravando())
				{
				int n=gestoGravador.fecha();
				std::cout<<"Gesto: gravação pronta: "<<gestoGravador.pasta<<" ("<<n<<" quadros"
				         <<(gestoGravador.erro.empty()?"":"; parou antes: ")<<gestoGravador.erro
				         <<"); fica na RAM: analise e apague (rm -r), ou a próxima gravação apaga"<<std::endl;
				}
			}
		}
	
	if(p.soltouMao)
		std::cout<<"Gesto: dedo solto (mao aberta)"<<std::endl;
	if(quer)
		gestoEvento(agora,p.ev);
	else if(p.ev==Gesto::Rastro::CONFIRMOU)
		std::cout<<"Gesto: (gravando, sem bichos) dedo visto em "<<rastro.x<<" "<<rastro.y<<std::endl;
	else if(p.ev==Gesto::Rastro::SOLTOU)
		std::cout<<"Gesto: (gravando, sem bichos) dedo solto"<<std::endl;
	}

void Sandbox::setFaceMode(FaceImage* newFaceImage,DEM* newFaceDem)
""")
    # --- Labirinto de água e desempenho (Projetor-LAOB)
    c = rep(c, "\tFaceImage* oldFace=faceImage;\n", """\tFaceImage* oldFace=faceImage;
	if(newFaceImage!=0||newFaceDem!=0)
		{
		/* Foto ou esculpir: o labirinto acaba (a nascente não fica jogando água escondida) */
		if(extras.labirinto)
			extras.labZerarAgua=true;
		extras.labirinto=false;
		extras.labEstado=0;
		extras.fimDoJogo();
		extras.copia=false;
		}
""")
    c = rep(c, """				else if(isToken(tokens[0],"relevo"))""", """				else if(isToken(tokens[0],"labirinto"))
					{
					int d=2;
					if(tokens.size()>=2)
						d=isToken(tokens[1],"facil")||isToken(tokens[1],"1")?1:isToken(tokens[1],"dificil")||isToken(tokens[1],"3")?3:2;
					setFaceMode(0,0);
					extras.labRodadas=0;
					extras.novoLabirinto(d);
					std::cout<<"Labirinto: nascente em x "<<extras.lsx<<" y "<<extras.lsy<<", alvo em x "<<extras.ltx<<" y "<<extras.lty<<" (dificuldade "<<d<<")"<<std::endl;
					}
				else if(isToken(tokens[0],"labirintoNovo"))
					{
					if(extras.labirinto)
						{
						extras.novoLabirinto(extras.labDificuldade);
						std::cout<<"Labirinto: rodada nova, nascente em x "<<extras.lsx<<" y "<<extras.lsy<<", alvo em x "<<extras.ltx<<" y "<<extras.lty<<std::endl;
						}
					}
				else if(isToken(tokens[0],"labirintoTeste"))
					{
					/* Para o teste automático: põe o alvo em cima da nascente (a água chega em segundos) */
					if(extras.labirinto)
						{
						extras.ltx=extras.lsx;
						extras.lty=extras.lsy;
						std::cout<<"Labirinto: teste, alvo na nascente"<<std::endl;
						}
					}
				else if(isToken(tokens[0],"jogo")&&tokens.size()>=2)
					{
					int j=Extras::jogoDoNome(tokens[1].c_str());
					int n=tokens.size()>=3?atoi(tokens[2].c_str()):2;
					if(tokens.size()>=3&&isToken(tokens[2],"facil")) n=1;
					if(tokens.size()>=3&&isToken(tokens[2],"dificil")) n=3;
					if(j>0)
						{
						setFaceMode(0,0);
						extras.pedeJogo(j,n);
						std::cout<<"Jogo pedido: "<<Extras::nomeDoJogo(j)<<" (nível "<<extras.jogoNivel<<")"<<std::endl;
						}
					else
						std::cout<<"Jogo desconhecido: "<<tokens[1]<<std::endl;
					}
				else if(isToken(tokens[0],"jogoNovo"))
					{
					if(extras.jogo!=Extras::SEM_JOGO)
						extras.pedeJogo(extras.jogo,extras.jogoNivel);
					}
				else if(isToken(tokens[0],"jogoTeste"))
					{
					/* Para o teste automático: as peças vão para cima da primeira nascente */
					if(extras.jogo!=Extras::SEM_JOGO&&extras.nFontes>0)
						{
						for(int i=0;i<extras.nPecas;++i)
							if(extras.pecas[i].tipo!=Extras::P_NASCENTE)
								{
								extras.pecas[i].x=extras.fontes[0].x;
								extras.pecas[i].y=extras.fontes[0].y;
								}
						std::cout<<"Jogo: teste, peças na nascente"<<std::endl;
						}
					}
				else if(isToken(tokens[0],"jogoVazao")&&tokens.size()>=2)
					{
					/* calibração: multiplica a água de todas as nascentes dos jogos e do labirinto */
					extras.fatorVazao=float(std::max(0.05,std::min(3.0,atof(tokens[1].c_str()))));
					std::cout<<"Jogo: vazão x"<<extras.fatorVazao<<std::endl;
					}
				else if(isToken(tokens[0],"jogoInfo"))
					{
					/* Diagnóstico: cada peça (tipo, posição, estado) e a fase do jogo */
					for(int i=0;i<extras.nPecas;++i)
						std::cout<<"JogoInfo: peça "<<i<<" tipo "<<extras.pecas[i].tipo<<" x "<<extras.pecas[i].x<<" y "<<extras.pecas[i].y
						         <<" estado "<<extras.pecas[i].estado<<std::endl;
					std::cout<<"JogoInfo: fase "<<extras.jogoFase<<std::endl;
					}
				else if(isToken(tokens[0],"jogoFim"))
					{
					if(extras.jogo!=Extras::SEM_JOGO)
						std::cout<<"Jogo desligado"<<std::endl;
					extras.fimDoJogo();
					}
				else if(isToken(tokens[0],"labirintoFim"))
					{
					if(extras.labirinto)
						extras.labZerarAgua=true; // a água do labirinto vai embora junto
					extras.labirinto=false;
					extras.labEstado=0;
					std::cout<<"Labirinto desligado"<<std::endl;
					}
				else if(isToken(tokens[0],"relevo"))""")
    # nascente: água de verdade entrando na simulação
    c = rep(c, """			glEnd();
			}
		
		glPopAttrib();
		}
	}
""", """			glEnd();
			}
		
		glPopAttrib();
		}
	
	/* Bocas das nascentes com 3,2 cm de raio (mais visíveis); a água por cm² é reduzida na mesma proporção
	   (2/3,2)², para o total ser o de sempre vezes o fatorVazao. Sem isso (17/11, 18:47) os jogos recebiam
	   4 a 9 vezes mais água: a vila alagava inteira em 1 min e a simulação atrasava. */
	const double BOCA=(2.0*2.0)/(3.2*3.2);
	
	/* Labirinto de água: a nascente joga água enquanto a rodada corre: */
	if(extras.labirinto&&extras.labEstado==1&&waterSpeed>0.0)
		{
		glPushAttrib(GL_ENABLE_BIT);
		glDisable(GL_CULL_FACE);
		Vector z=waterTable->getBaseTransform().inverseTransform(Vector(0,0,1));
		Vector x=Geometry::normal(z);
		Vector y=Geometry::cross(z,x);
		x.normalize();
		y.normalize();
		Point centro=boxTransform.inverseTransform(Point(extras.lsx,extras.lsy,extras.referencia));
		glVertexAttrib1fARB(1,GLfloat(extras.labVazao*extras.fatorVazao*BOCA/waterSpeed)); // BOCA: a boca de 3,2 cm joga o mesmo total que a antiga de 2 cm
		glBegin(GL_POLYGON);
		for(int i=0;i<24;++i)
			{
			Scalar angle=Scalar(2)*Math::Constants<Scalar>::pi*Scalar(i)/Scalar(24);
			glVertex(centro+x*(Math::cos(angle)*Scalar(3.2))+y*(Math::sin(angle)*Scalar(3.2)));
			}
		glEnd();
		glPopAttrib();
		}
	
	/* Chuva pelo painel: cada nuvem ligada joga água no seu ponto: */
	if(extras.temChuva&&waterSpeed>0.0)
		{
		glPushAttrib(GL_ENABLE_BIT);
		glDisable(GL_CULL_FACE);
		Vector z=waterTable->getBaseTransform().inverseTransform(Vector(0,0,1));
		Vector x=Geometry::normal(z);
		Vector y=Geometry::cross(z,x);
		x.normalize();
		y.normalize();
		for(int k=0;k<Extras::MAX_CHUVAS;++k)
			{
			if(extras.chuvas[k].ate<=extras.tempo)
				continue;
			Point centro=boxTransform.inverseTransform(Point(extras.chuvas[k].x,extras.chuvas[k].y,extras.referencia));
			glVertexAttrib1fARB(1,GLfloat(0.7*extras.chuvaForca(k)/waterSpeed)); // começa e termina aos poucos
			glBegin(GL_POLYGON);
			for(int i=0;i<24;++i)
				{
				Scalar angle=Scalar(2)*Math::Constants<Scalar>::pi*Scalar(i)/Scalar(24);
				glVertex(centro+x*(Math::cos(angle)*Scalar(3))+y*(Math::sin(angle)*Scalar(3)));
				}
			glEnd();
			}
		glPopAttrib();
		}

	/* Jogos de água: cada nascente ativa joga água enquanto a rodada corre: */
	if(extras.jogo!=Extras::SEM_JOGO&&extras.jogoFase==1&&waterSpeed>0.0)
		{
		glPushAttrib(GL_ENABLE_BIT);
		glDisable(GL_CULL_FACE);
		Vector z=waterTable->getBaseTransform().inverseTransform(Vector(0,0,1));
		Vector x=Geometry::normal(z);
		Vector y=Geometry::cross(z,x);
		x.normalize();
		y.normalize();
		for(int k=0;k<extras.nFontes;++k)
			{
			const Extras::Fonte& f=extras.fontes[k];
			if(!f.ativa)
				continue;
			Point centro=boxTransform.inverseTransform(Point(f.x,f.y,extras.referencia));
			glVertexAttrib1fARB(1,GLfloat(extras.labVazao*f.vazao*extras.fatorVazao*BOCA/waterSpeed));
			glBegin(GL_POLYGON);
			for(int i=0;i<24;++i)
				{
				Scalar angle=Scalar(2)*Math::Constants<Scalar>::pi*Scalar(i)/Scalar(24);
				glVertex(centro+x*(Math::cos(angle)*Scalar(3.2))+y*(Math::sin(angle)*Scalar(3.2)));
				}
			glEnd();
			}
		glPopAttrib();
		}
	}

void Sandbox::medeJogo(GLContextData& contextData) const
	{
	/* Lê a água (só o nível, no centro das células) e o fundo (nos cantos) e mede, para cada
	   peça, a maior profundidade dentro dela (cm): */
	float maxima[Extras::MAX_PECAS];
	for(int i=0;i<Extras::MAX_PECAS;++i)
		maxima[i]=0.0f;
	GLint qw=0,qh=0,bw=0,bh=0;
	waterTable->bindQuantityTexture(contextData);
	glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_ARB,0,GL_TEXTURE_WIDTH,&qw);
	glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_ARB,0,GL_TEXTURE_HEIGHT,&qh);
	waterTable->bindBathymetryTexture(contextData);
	glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_ARB,0,GL_TEXTURE_WIDTH,&bw);
	glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_ARB,0,GL_TEXTURE_HEIGHT,&bh);
	if(qw<=2||qh<=2||bw<=1||bh<=1)
		{
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		return;
		}
	labFundo.resize(size_t(bw)*size_t(bh));
	glGetTexImage(GL_TEXTURE_RECTANGLE_ARB,0,GL_RED,GL_FLOAT,&labFundo[0]);
	waterTable->bindQuantityTexture(contextData);
	labQuantidade.resize(size_t(qw)*size_t(qh));
	glGetTexImage(GL_TEXTURE_RECTANGLE_ARB,0,GL_RED,GL_FLOAT,&labQuantidade[0]); // 1/3 do que lia (os outros canais eram do barquinho)
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	
	const GLfloat* cs=waterTable->getCellSize();
	int nj=std::min(int(qh),int(bh))-1,ni=std::min(int(qw),int(bw))-1;
	for(int p=0;p<extras.nPecas;++p)
		{
		const Extras::Peca& pc=extras.pecas[p];
		if(pc.tipo==Extras::P_NASCENTE)
			continue;
		Point pw=waterTable->getBaseTransform().transform(boxTransform.inverseTransform(Point(pc.x,pc.y,extras.referencia)));
		double ci=(pw[0]-waterTable->getDomain().min[0])/cs[0];
		double cj=(pw[1]-waterTable->getDomain().min[1])/cs[1];
		double raio=std::max(1.0,double(pc.raio));
		double ri=raio/cs[0],rj=raio/cs[1];
		float maior=0.0f;
		for(int j=std::max(1,int(cj-rj));j<=std::min(nj,int(cj+rj));++j)
			for(int i=std::max(1,int(ci-ri));i<=std::min(ni,int(ci+ri));++i)
				{
				double di=(i+0.5-ci)/ri,dj=(j+0.5-cj)/rj;
				if(di*di+dj*dj>1.0)
					continue;
				float b=0.25f*(labFundo[(j-1)*bw+i-1]+labFundo[(j-1)*bw+i]+labFundo[j*bw+i-1]+labFundo[j*bw+i]);
				maior=std::max(maior,labQuantidade[size_t(j)*qw+i]-b);
				}
		maxima[p]=maior;
		}
	extras.jogoLeitura(maxima);
	}

float Sandbox::medeAguaNoAlvo(GLContextData& contextData) const
	{
	/* Lê a água (nível da superfície, no centro das células) e o fundo (nos cantos) e
	   devolve a maior profundidade de água dentro do alvo, em cm: */
	GLint qw=0,qh=0,bw=0,bh=0;
	waterTable->bindQuantityTexture(contextData);
	glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_ARB,0,GL_TEXTURE_WIDTH,&qw);
	glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_ARB,0,GL_TEXTURE_HEIGHT,&qh);
	if(qw<=2||qh<=2)
		{
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		return 0.0f;
		}
	labQuantidade.resize(size_t(qw)*size_t(qh));
	glGetTexImage(GL_TEXTURE_RECTANGLE_ARB,0,GL_RED,GL_FLOAT,&labQuantidade[0]);
	waterTable->bindBathymetryTexture(contextData);
	glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_ARB,0,GL_TEXTURE_WIDTH,&bw);
	glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_ARB,0,GL_TEXTURE_HEIGHT,&bh);
	if(bw<=1||bh<=1)
		{
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		return 0.0f;
		}
	labFundo.resize(size_t(bw)*size_t(bh));
	glGetTexImage(GL_TEXTURE_RECTANGLE_ARB,0,GL_RED,GL_FLOAT,&labFundo[0]);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);

	/* Alvo: caixa (cm) -> câmera -> grade da água: */
	Point pc=boxTransform.inverseTransform(Point(extras.ltx,extras.lty,extras.referencia));
	Point pw=waterTable->getBaseTransform().transform(pc);
	const GLfloat* cs=waterTable->getCellSize();
	double ci=(pw[0]-waterTable->getDomain().min[0])/cs[0];
	double cj=(pw[1]-waterTable->getDomain().min[1])/cs[1];
	double ri=extras.lraio/cs[0],rj=extras.lraio/cs[1];
	float maior=0.0f;
	for(int j=std::max(1,int(cj-rj));j<=std::min(int(std::min(qh,bh))-1,int(cj+rj));++j)
		for(int i=std::max(1,int(ci-ri));i<=std::min(int(std::min(qw,bw))-1,int(ci+ri));++i)
			{
			double di=(i+0.5-ci)/ri,dj=(j+0.5-cj)/rj;
			if(di*di+dj*dj>1.0)
				continue;
			float b=0.25f*(labFundo[(j-1)*bw+i-1]+labFundo[(j-1)*bw+i]+labFundo[j*bw+i-1]+labFundo[j*bw+i]);
			maior=std::max(maior,labQuantidade[j*qw+i]-b);
			}
	return maior;
	}
""")
    # no contexto OpenGL: esvazia a água quando pedido e mede a água no alvo 2x por segundo
    c = rep(c, """		/* Finish an active grid request: */
		if(request.isActive())
			request.complete();
""", """		/* Finish an active grid request: */
		if(request.isActive())
			request.complete();

		/* Labirinto de água (Projetor-LAOB): */
		if(extras.labZerarAgua)
			{
			const GLsizei* ws=waterTable->getSize();
			std::vector<GLfloat> vazio(size_t(ws[0])*size_t(ws[1]),GLfloat(-1000));
			waterTable->setWaterLevel(&vazio[0],contextData);
			extras.labZerarAgua=false;
			}
		if(extras.labirinto&&extras.labEstado==1&&Vrui::getApplicationTime()-labLeitura>0.5)
			{
			labLeitura=Vrui::getApplicationTime();
			if(extras.aguaNoAlvo(medeAguaNoAlvo(contextData)))
				std::cout<<"Labirinto: a água chegou! ("<<extras.labRodadas<<" rodadas vencidas, "<<int(extras.labChegou-extras.labInicio)<<" s)"<<std::endl;
			else if(extras.nPecas>0)
				medeJogo(contextData); // armadilhas: a água não pode encostar nelas
			}
		/* Água única (Projetor-LAOB, 17/11/2026): a chuva, o lago dos bichos e o azul do mapa são a MESMA
		   água, a da simulação. Com bichos da água, o lago é mantido cheio de verdade até o nível do mar
		   (cavar faz a água brotar devagar) e a água a mais (chuva) a areia absorve aos poucos, na GPU, em
		   todo passo da simulação (WaterTable2::setLencol). 4x por segundo, a profundidade e a correnteza de
		   cada célula do mapa vão para os bichos. */
		static double aguaLeitura=0.0;
		static std::vector<GLfloat> aQ,aB;
		double agoraA=Vrui::getApplicationTime();
		/* o lençol vale em todo passo da simulação (na GPU): aqui só a geometria, a cada quadro */
		if(extras.lencol())
			{
			const ONTransform& wb=waterTable->getBaseTransform();
			const GLfloat* cs=waterTable->getCellSize();
			double dmx=waterTable->getDomain().min[0],dmy=waterTable->getDomain().min[1];
			float H=extras.nivelAgua;
			Point w0=wb.transform(boxTransform.inverseTransform(Point(0,0,H)));
			Point w1=wb.transform(boxTransform.inverseTransform(Point(10,0,H)));
			Point w2=wb.transform(boxTransform.inverseTransform(Point(0,10,H)));
			double m00=(w1[0]-w0[0])/10.0,m01=(w2[0]-w0[0])/10.0,m10=(w1[1]-w0[1])/10.0,m11=(w2[1]-w0[1])/10.0;
			double det=m00*m11-m01*m10;
			if(std::abs(det)>1.0e-9)
				{
				/* célula (x, y) -> caixa: (x*cs + dmin - w0) passado pela inversa de M */
				double i00=m11/det,i01=-m01/det,i10=-m10/det,i11=m00/det;
				double ox=dmx-w0[0],oy=dmy-w0[1];
				GLfloat pr[15];
				pr[0]=GLfloat(i00*ox+i01*oy); pr[1]=GLfloat(i00*cs[0]); pr[2]=GLfloat(i01*cs[1]);
				pr[3]=GLfloat(i10*ox+i11*oy); pr[4]=GLfloat(i10*cs[0]); pr[5]=GLfloat(i11*cs[1]);
				pr[6]=GLfloat(w0[2]); pr[7]=GLfloat((w1[2]-w0[2])/10.0); pr[8]=GLfloat((w2[2]-w0[2])/10.0);
				pr[9]=GLfloat(faceBoxMin[0]); pr[10]=GLfloat(faceBoxMin[1]); pr[11]=GLfloat(faceBoxMax[0]); pr[12]=GLfloat(faceBoxMax[1]);
				pr[13]=1.6f; pr[14]=extras.absorcao;
				waterTable->setLencol(true,pr);
				}
			}
		else
			waterTable->setLencol(false,0);
		if(extras.querAguaSim()&&extras.celulasX()>0&&agoraA-aguaLeitura>0.25)
			{
			aguaLeitura=agoraA;
			GLint qw=0,qh=0,bw=0,bh=0;
			waterTable->bindQuantityTexture(contextData);
			glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_ARB,0,GL_TEXTURE_WIDTH,&qw);
			glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_ARB,0,GL_TEXTURE_HEIGHT,&qh);
			waterTable->bindBathymetryTexture(contextData);
			glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_ARB,0,GL_TEXTURE_WIDTH,&bw);
			glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_ARB,0,GL_TEXTURE_HEIGHT,&bh);
			if(qw>2&&qh>2&&bw>1&&bh>1)
				{
				aB.resize(size_t(bw)*size_t(bh));
				glGetTexImage(GL_TEXTURE_RECTANGLE_ARB,0,GL_RED,GL_FLOAT,&aB[0]);
				waterTable->bindQuantityTexture(contextData);
				aQ.resize(size_t(qw)*size_t(qh)*3);
				glGetTexImage(GL_TEXTURE_RECTANGLE_ARB,0,GL_RGB,GL_FLOAT,&aQ[0]);
				glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
				const GLfloat* cs=waterTable->getCellSize();
				const ONTransform& wb=waterTable->getBaseTransform();
				double dmx=waterTable->getDomain().min[0],dmy=waterTable->getDomain().min[1];
				int ni=std::min(int(qw),int(bw)),nj=std::min(int(qh),int(bh));
				/* o plano do nível do mar (z = H na caixa) na grade da água: W(bx,by) = w0 + bx*eX + by*eY */
				float H=extras.lencol()?extras.nivelAgua:extras.referencia;
				Point w0=wb.transform(boxTransform.inverseTransform(Point(0,0,H)));
				Point w1=wb.transform(boxTransform.inverseTransform(Point(10,0,H)));
				Point w2=wb.transform(boxTransform.inverseTransform(Point(0,10,H)));
				double m00=(w1[0]-w0[0])/10.0,m01=(w2[0]-w0[0])/10.0,m10=(w1[1]-w0[1])/10.0,m11=(w2[1]-w0[1])/10.0;
				double det=m00*m11-m01*m10;
				/* para os bichos: profundidade e correnteza no centro de cada célula do mapa */
				int gx=extras.celulasX(),gy=extras.celulasY();
				extras.aguaSim.assign(size_t(gx)*size_t(gy),0.0f);
				extras.aguaVx.assign(size_t(gx)*size_t(gy),0.0f);
				extras.aguaVy.assign(size_t(gx)*size_t(gy),0.0f);
				for(int cy=0;cy<gy;++cy)
					for(int cx=0;cx<gx;++cx)
						{
						Point pb(extras.cantoMin()[0]+(cx+0.5f)*extras.celulaCm(),extras.cantoMin()[1]+(cy+0.5f)*extras.celulaCm(),H);
						Point pw=wb.transform(boxTransform.inverseTransform(pb));
						int i=int((pw[0]-dmx)/cs[0]);
						int j=int((pw[1]-dmy)/cs[1]);
						if(i<1||j<1||i>=ni||j>=nj)
							continue;
						float b=0.25f*(aB[(j-1)*bw+i-1]+aB[(j-1)*bw+i]+aB[j*bw+i-1]+aB[j*bw+i]);
						const GLfloat* q=&aQ[(size_t(j)*qw+i)*3];
						float h=std::max(0.0f,q[0]-b);
						size_t k=size_t(cy)*gx+cx;
						extras.aguaSim[k]=h;
						if(h>0.08f&&std::abs(det)>1.0e-9)
							{
							double u=q[1]/h,v=q[2]/h; // velocidade na grade da água -> caixa
							extras.aguaVx[k]=float((m11*u-m01*v)/det);
							extras.aguaVy[k]=float((-m10*u+m00*v)/det);
							}
						}
				}
			glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
			}
		if(extras.jogo!=Extras::SEM_JOGO&&extras.jogoFase==1&&Vrui::getApplicationTime()-labLeitura>0.5)
			{
			labLeitura=Vrui::getApplicationTime();
			medeJogo(contextData);
			}
""")
    # "Ran out of time" a cada quadro enchia o log; conta e mostra junto do desempenho
    c = rep(c, """		if(totalTimeStep>1.0e-8f)
			std::cout<<"Ran out of time by "<<totalTimeStep<<std::endl;
""", """		++quadrosAgua;
		aguaPedida+=Vrui::getFrameTime()*waterSpeed;
		if(totalTimeStep>1.0e-8f)
			{
			++atrasosAgua;
			aguaFaltou+=totalTimeStep;
			}
""")
    c = rep(c, """	/* Tesouro, lago, peixes e carrinhos: */
	atualizaExtras();
""", """	/* Tesouro, lago, peixes e carrinhos: */
	atualizaExtras();

	/* Desempenho, a cada 30 s no log: */
	++quadros;
	if(desempenhoTempo==0.0)
		desempenhoTempo=Vrui::getApplicationTime();
	else if(Vrui::getApplicationTime()-desempenhoTempo>=30.0)
		{
		double dt=Vrui::getApplicationTime()-desempenhoTempo;
		std::cout<<"Desempenho: "<<int(quadros/dt+0.5)<<" quadros/s; água atrasada em "<<atrasosAgua<<" de "<<quadrosAgua<<" passos"
		         <<"; a água andou "<<(aguaPedida>0.0?int(100.0*(1.0-aguaFaltou/aguaPedida)+0.5):100)<<"% do tempo real"<<std::endl;
		std::cout<<perfil.relatorio(quadros)<<std::endl;
		quadros=0;
		quadrosAgua=0;
		atrasosAgua=0;
		aguaPedida=aguaFaltou=0.0;
		desempenhoTempo=Vrui::getApplicationTime();
		}
""")
    c = rep(c, """	/* Movimento dos peixes e carrinhos: */
	extras.passo(dt);""", """	/* Labirinto: 20 s depois da chegada, rodada nova com a água zerada: */
	if(extras.labirinto&&extras.labEstado==2&&extras.tempo-extras.labChegou>20.0)
		{
		extras.novoLabirinto(extras.labDificuldade);
		std::cout<<"Labirinto: rodada nova, nascente em x "<<extras.lsx<<" y "<<extras.lsy<<", alvo em x "<<extras.ltx<<" y "<<extras.lty<<std::endl;
		}

	/* Jogos de água: regras, tempo e avisos no log (o painel lê as linhas "Jogo:"): */
	extras.jogoPasso(dt);
	for(size_t i=0;i<extras.mensagens.size();++i)
		std::cout<<extras.mensagens[i]<<std::endl;
	extras.mensagens.clear();

	/* Movimento dos peixes e carrinhos: */
	extras.passo(dt);
	
	/* Chuva pela mão (22/11/2026): avisa no log quando começa e quando para (1 s sem chover), para o som
	   da chuva tocar só enquanto chove */
	{
	double agoraChuva=Vrui::getApplicationTime();
	bool chove=agoraChuva-chuvaMaoVista<(chuvaMaoLigada?1.0:0.2);
	if(chove!=chuvaMaoLigada)
		{
		chuvaMaoLigada=chove;
		std::cout<<(chove?"Chuva: mao comecou":"Chuva: mao parou")<<std::endl;
		}
	}""")
    # Chuva pela mão: uma "mão" na beirada da área iluminada (ou fora dela) não chove. Em 16/11/2026 o
    # sensor via a borda da caixa como mão aberta e chovia sem parar num canto; a água acumulada ali,
    # funda, deixava a simulação inteira em câmera lenta (66% do tempo real) e os jogos de água lentos.
    c = rep(c, """			/* Render a rain disk approximating the hand: */
			glBegin(GL_POLYGON);
""", """			/* Projetor-LAOB: no laboratório LAOB (Mundo) a mão não faz chover (22/11/2026, pedido do Victor:
			   a planta do laboratório não tem água; antes a mão em cima dela chovia) */
			if(extras.modo==Extras::MUNDO)
				continue;
			
			/* Projetor-LAOB: só chove com a mão sobre a areia (a mais de 4 cm da beirada iluminada) */
			if(faceBoxMin[0]<faceBoxMax[0])
				{
				Point pm=boxTransform.transform(hIt->center);
				if(pm[0]<faceBoxMin[0]+4.0||pm[0]>faceBoxMax[0]-4.0||pm[1]<faceBoxMin[1]+4.0||pm[1]>faceBoxMax[1]-4.0)
					continue;
				}
			
			/* Projetor-LAOB: perto do dedo apontado (para cima ou para a areia) não chove (ele derruba comida; Gesto.cpp). O
			   HandExtractor nem vê o dedo apontado como mão: isto só garante. A mão aberta chove como sempre. */
			if(gestoRastro.ativo)
				{
				Point pd=boxTransform.transform(hIt->center);
				if((pd[0]-gestoRastro.x)*(pd[0]-gestoRastro.x)+(pd[1]-gestoRastro.y)*(pd[1]-gestoRastro.y)<100.0)
					continue;
				}

			/* Render a rain disk approximating the hand: */
			chuvaMaoVista=Vrui::getApplicationTime(); // Projetor-LAOB: para o som da chuva (log "Chuva: mao")
			glBegin(GL_POLYGON);
""")
    # Dedo apontado (18/11/2026): o quadro cru do sensor vai também para o detector, só enquanto ele quer
    # (com bichos na areia); é só uma referência ao quadro (o FrameBuffer é contado), nada é copiado.
    c = rep(c, """	if(handExtractor!=0)
		handExtractor->receiveRawFrame(frameBuffer);
	}
""", """	if(handExtractor!=0)
		handExtractor->receiveRawFrame(frameBuffer);
	if(brutoQuer)
		brutoFrames.postNewValue(frameBuffer); // Projetor-LAOB: dedo apontado (Gesto.cpp)
	}
""")
    # Perfil (22/11/2026): tempo de cada quadro e das passadas principais (linha "Perfil:" junto do
    # "Desempenho:"). Sem isso só havia a média de quadros/s, que esconde travadinhas.
    c = rep(c, """void Sandbox::frame(void)
	{
""", """void Sandbox::frame(void)
	{
	/* Projetor-LAOB: intervalo desde o quadro anterior e CPU do frame() inteiro (Perfil.h) */
	{
	double comp=perfil.compilou(SurfaceRenderer::compilacoes,SurfaceRenderer::compilacaoMs); // shader montado no display anterior (ms; 0 = nenhum)
	double lento=perfil.quadro();
	if(lento>0.0)
		{
		if(comp>0.0)
			std::cout<<"Perfil: quadro lento de "<<int(lento+0.5)<<" ms (compilando o shader da superficie: "<<int(comp+0.5)<<" ms)"<<std::endl;
		else
			std::cout<<"Perfil: quadro lento de "<<int(lento+0.5)<<" ms"<<std::endl;
		}
	}
	struct PerfilFrame { Perfil& p; PerfilFrame(Perfil& sp):p(sp) {p.cpuIni(Perfil::C_FRAME);} ~PerfilFrame(void) {p.cpuFim(Perfil::C_FRAME);} } perfilFrame(perfil);
""")
    c = rep(c, """		/* Update the water table's bathymetry grid: */
""", """		perfil.cpuIni(Perfil::C_AGUA); perfil.gpuIni(Perfil::G_AGUA); // Projetor-LAOB: Perfil
		/* Update the water table's bathymetry grid: */
""")
    c = rep(c, """		/* Check if the grid request is active and wants water level data: */
""", """		perfil.gpuFim(Perfil::G_AGUA); perfil.cpuFim(Perfil::C_AGUA);
		perfil.cpuIni(Perfil::C_LEITURA); // Projetor-LAOB: leituras GPU->CPU (água dos bichos, jogos, labirinto)
		/* Check if the grid request is active and wants water level data: */
""")
    c = rep(c, """		/* Mark the water simulation state as up-to-date for this frame: */
""", """		perfil.cpuFim(Perfil::C_LEITURA);
		/* Mark the water simulation state as up-to-date for this frame: */
""")
    c = rep(c, """		rs.surfaceRenderer->renderSinglePass(ds.viewport,projection,ds.modelviewNavigational,contextData);
""", """		perfil.cpuIni(Perfil::C_RELEVO); perfil.gpuIni(Perfil::G_RELEVO); // Projetor-LAOB: Perfil
		rs.surfaceRenderer->renderSinglePass(ds.viewport,projection,ds.modelviewNavigational,contextData);
		perfil.gpuFim(Perfil::G_RELEVO); perfil.cpuFim(Perfil::C_RELEVO);
""")
    open("Sandbox.cpp", "w").write(c)

# WaterTable2 (Projetor-LAOB, 17/11/2026): o "lençol" do lago dos bichos roda na GPU, em TODO passo da
# simulação, dentro do passo que já soma a água da chuva (Water2WaterUpdateShader, mudado pelo patch_agua.py):
# onde o fundo está abaixo do nível do mar a água brota até ele, e a água acima do nível a areia absorve.
# Assim o lago fica sempre cheio sem ler/escrever a água pela CPU (antes, 4x/s, a água "pulava").
wh = open("WaterTable2.h").read()
if "setLencol" not in wh:
    wh = rep(wh, "\t\tGLint waterShaderUniformLocations[3];", "\t\tGLint waterShaderUniformLocations[8]; // Projetor-LAOB: +5 do lençol")
    wh = rep(wh, "\tbool dryBoundary;", "\tbool lencolLigado; // Projetor-LAOB: lago dos bichos mantido cheio\n\tGLfloat lencol[16]; // bx(3), by(3), T(3), área(4), enche, absorve, -\n\tbool dryBoundary;")
    wh = rep(wh, "\tGLfloat runSimulationStep(", "\tvoid setLencol(bool ligado,const GLfloat* params); // Projetor-LAOB: 15 números (ver WaterTable2.cpp)\n\tGLfloat runSimulationStep(")
    open("WaterTable2.h", "w").write(wh)
wc = open("WaterTable2.cpp").read()
if "setLencol" not in wc:
    wc = wc.replace("dryBoundary(true)", "lencolLigado(false),dryBoundary(true)")
    wc = rep(wc, """	dataItem->waterShaderUniformLocations[2]=glGetUniformLocationARB(dataItem->waterShader,"waterSampler");
""", """	dataItem->waterShaderUniformLocations[2]=glGetUniformLocationARB(dataItem->waterShader,"waterSampler");
	dataItem->waterShaderUniformLocations[3]=glGetUniformLocationARB(dataItem->waterShader,"lencolBx");
	dataItem->waterShaderUniformLocations[4]=glGetUniformLocationARB(dataItem->waterShader,"lencolBy");
	dataItem->waterShaderUniformLocations[5]=glGetUniformLocationARB(dataItem->waterShader,"lencolT");
	dataItem->waterShaderUniformLocations[6]=glGetUniformLocationARB(dataItem->waterShader,"lencolArea");
	dataItem->waterShaderUniformLocations[7]=glGetUniformLocationARB(dataItem->waterShader,"lencolTaxa");
""")
    wc = rep(wc, "\tif(waterDeposit!=0.0f||!renderFunctions.empty())\n", "\tif(waterDeposit!=0.0f||!renderFunctions.empty()||lencolLigado)\n")
    wc = rep(wc, """		glUniform1iARB(dataItem->waterShaderUniformLocations[2],2);
""", """		glUniform1iARB(dataItem->waterShaderUniformLocations[2],2);
		/* Projetor-LAOB: lençol (posição da célula -> caixa, nível do mar, área iluminada, ritmo por passo) */
		glUniform3fARB(dataItem->waterShaderUniformLocations[3],lencol[0],lencol[1],lencol[2]);
		glUniform3fARB(dataItem->waterShaderUniformLocations[4],lencol[3],lencol[4],lencol[5]);
		glUniform3fARB(dataItem->waterShaderUniformLocations[5],lencol[6],lencol[7],lencol[8]);
		glUniform4fARB(dataItem->waterShaderUniformLocations[6],lencol[9],lencol[10],lencol[11],lencol[12]);
		glUniform3fARB(dataItem->waterShaderUniformLocations[7],lencolLigado?1.0f:0.0f,
		               std::min(1.0f,lencol[13]*stepSize),lencol[14]*stepSize);
""")
    wc = rep(wc, "GLfloat WaterTable2::runSimulationStep(", """void WaterTable2::setLencol(bool ligado,const GLfloat* params)
	{
	/* params: bx = p0+p1*x+p2*y e by = p3+p4*x+p5*y (célula -> caixa, cm); nível do mar T = p6+p7*bx+p8*by
	   (altura da água); área iluminada p9..p12 (min x, min y, max x, max y na caixa); p13 = quanto do que falta
	   brota por segundo; p14 = cm/s de água a mais que a areia absorve */
	lencolLigado=ligado;
	if(params!=0)
		for(int i=0;i<15;++i)
			lencol[i]=params[i];
	}

GLfloat WaterTable2::runSimulationStep(""")
    if "#include <algorithm>" not in wc:
        wc = "#include <algorithm>\n" + wc
    open("WaterTable2.cpp", "w").write(wc)

# FrameFilter (Projetor-LAOB, 22/11/2026): a trava do relevo embaixo da mão (Sandbox::gestoTrava, Gesto.h).
# Ponto travado: o relevo fica como estava (a mão parada não vira areia); as estatísticas seguem normais, então
# quando a mão sai o ponto fica instável até o filtro ver só areia de novo, como sempre.
fh = open("FrameFilter.h").read()
if "trava" not in fh:
    fh = rep(fh, "\t/* Constructors and destructors: */\n\tpublic:\n",
             "\t/* Constructors and destructors: */\n\tpublic:\n"
             "\tunsigned char* trava; // Projetor-LAOB: 1 = mão em cima, o relevo não muda (um byte por pixel; escrito pelo Sandbox)\n")
    open("FrameFilter.h", "w").write(fh)
fc = open("FrameFilter.cpp").read()
if "trava" not in fc:
    # (por último, como na declaração: sem o aviso -Wreorder; 23/11/2026)
    fc = rep(fc, "\t outputFrameFunction(0)\n\t{\n", "\t outputFrameFunction(0),\n\t trava(0)\n\t{\n")
    fc = rep(fc, "\t/* Initialize the averaging buffer: */\n",
             "\t/* Projetor-LAOB: trava do relevo, nada travado */\n\ttrava=new unsigned char[size[1]*size[0]];\n"
             "\tfor(unsigned int i=0;i<size[1]*size[0];++i)\n\t\ttrava[i]=0;\n\t\n\t/* Initialize the averaging buffer: */\n")
    fc = rep(fc, "\tdelete[] validBuffer;\n", "\tdelete[] validBuffer;\n\tdelete[] trava;\n")
    fc = rep(fc, "\t\tconst PixelDepthCorrection* pdcPtr=pixelDepthCorrection;\n",
             "\t\tconst PixelDepthCorrection* pdcPtr=pixelDepthCorrection;\n\t\tconst unsigned char* tPtr=trava; // Projetor-LAOB\n")
    fc = rep(fc, "++ofPtr,++nofPtr)\n", "++ofPtr,++nofPtr,++tPtr)\n")
    fc = rep(fc, "\t\t\t\tif(sPtr[0]>=minNumSamples&&sPtr[2]*sPtr[0]<=maxVariance*sPtr[0]*sPtr[0]+sPtr[1]*sPtr[1])\n",
             "\t\t\t\tif(*tPtr)\n\t\t\t\t\t{\n\t\t\t\t\t/* Projetor-LAOB: mão em cima, o relevo fica como estava */\n\t\t\t\t\t*nofPtr=*ofPtr;\n\t\t\t\t\t}\n"
             "\t\t\t\telse if(sPtr[0]>=minNumSamples&&sPtr[2]*sPtr[0]<=maxVariance*sPtr[0]*sPtr[0]+sPtr[1]*sPtr[1])\n")
    open("FrameFilter.cpp", "w").write(fc)

m = open("makefile").read()
if "FaceImage.cpp" not in m:
    m = rep(m, "                   DEM.cpp \\\n", "                   DEM.cpp \\\n                   FaceImage.cpp \\\n                   Gesto.cpp \\\n                   Extras.cpp \\\n                   Jogos.cpp \\\n                   Mundo.cpp \\\n")
    open("makefile", "w").write(m)
print("Sandbox ok")
