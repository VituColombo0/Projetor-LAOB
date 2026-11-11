"""Acrescenta ao Sandbox do SARndbox 2.8 os comandos do modo Rosto no cano de controle:
   rosto <foto.png> [giro 0/90/180/270] [espelho 0/1]   foto projetada que se deforma com a areia
   rostoAjuste <ampliação/cm> <deslocamento/cm> <mistura 0-1> <sombra>
   esculpir <rosto.dem>                                    relevo-alvo: vermelho/azul = tirar/pôr areia
   relevo                                                  volta à topografia normal
   tesouro [profundidade cm] [segundos] / tesouroFim       caça ao tesouro; achado, some e reaparece
                                                           em outro lugar depois dos segundos (20)
   fossil [trex|triceratops|pterodactilo|pegada] [cm] [s]   escavação de fóssil (mesmo mecanismo do tesouro)
   fossilFim                                               encerra a escavação
   animais peixes|carros|todos|nada [quantidade]           peixes no lago e carrinhos nos vales
   efeito <tema>                                           animação do tema (lava, neve, poeira...)
   labirinto [facil|normal|dificil] / labirintoNovo / labirintoFim (labirintoTeste: alvo na nascente)
                                                           labirinto de água: uma nascente joga água de
                                                           verdade; cavem canais até ela chegar ao alvo
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
    h = rep(h, '#include "Types.h"\n', '#include "Types.h"\n#include "Extras.h"\n')
    h = rep(h, "\tdouble faceRefTime; // Última medição da altura média da areia\n", """\tdouble faceRefTime; // Última medição da altura média da areia
	mutable Extras extras; // Tesouro, lago, peixes, carrinhos e labirinto sobre o relevo
	mutable double labLeitura; // Labirinto: última medição da água no alvo
	mutable std::vector<GLfloat> labQuantidade,labFundo; // Labirinto: leitura da água e do fundo
	mutable unsigned int quadrosAgua,atrasosAgua; // Desempenho: passos de água e quantos atrasaram
	double desempenhoTempo; unsigned int quadros; // Desempenho: quadros desde a última linha no log
	double extrasMapaTempo,extrasUltimoPasso; // Última atualização do mapa e do movimento
	float tesouroPedido; // Profundidade do tesouro pedido antes de haver mapa (<0 = nenhum)
""")
    h = rep(h, "\tfloat measureSandHeight(void) const; // Altura mediana da areia (cm acima do plano base)\n",
            "\tfloat measureSandHeight(void) const; // Altura mediana da areia (cm acima do plano base)\n"
            "\tvoid atualizaExtras(void); // Mapa de alturas, movimento e tesouro (a cada quadro)\n"
            "\tfloat medeAguaNoAlvo(GLContextData& contextData) const; // Labirinto: água no alvo (cm)\n")
    open("Sandbox.h", "w").write(h)

c = open("Sandbox.cpp").read()
if "FaceImage" not in c:
    c = rep(c, '#include "DEMTool.h"\n', '#include "DEMTool.h"\n#include "FaceImage.h"\n')
    c = rep(c, "\t controlPipeFd(-1)", "\t controlPipeFd(-1),\n\t faceImage(0),faceDem(0),faceRefTime(0.0)")
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
    c = rep(c, "\t faceImage(0),faceDem(0),faceRefTime(0.0)",
            "\t faceImage(0),faceDem(0),faceRefTime(0.0),\n\t extrasMapaTempo(0.0),extrasUltimoPasso(0.0),tesouroPedido(-1.0f),\n"
            "\t labLeitura(0.0),quadrosAgua(0),atrasosAgua(0),desempenhoTempo(0.0),quadros(0)")
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
						std::cerr<<"uso: efeito classico|vulcao|inverno|marte|deserto|oceano|arco-iris|mapa|nada"<<std::endl;
					else
						{
						extras.efeito=e;
						std::cout<<"Efeito: "<<tokens[1]<<std::endl;
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
				else if(isToken(tokens[0],"animais"))
					{
					int m=-1;
					if(tokens.size()>=2)
						m=isToken(tokens[1],"peixes")?Extras::PEIXES:isToken(tokens[1],"carros")?Extras::CARROS:
						  isToken(tokens[1],"todos")?Extras::TODOS:isToken(tokens[1],"nada")?Extras::NADA:-1;
					if(m<0)
						std::cerr<<"uso: animais peixes|carros|todos|nada [quantidade]"<<std::endl;
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
    c = rep(c, """	/* Update all surface renderers: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		rsIt->surfaceRenderer->setAnimationTime(Vrui::getApplicationTime());
""", """	/* Tesouro, lago, peixes e carrinhos: */
	atualizaExtras();
	
	/* Update all surface renderers: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		{
		rsIt->surfaceRenderer->setAnimationTime(Vrui::getApplicationTime());
		if(rsIt->fixProjectorView)
			rsIt->surfaceRenderer->setExtras(extras.ativo()&&faceImage==0?&extras:0);
		}
""")
    c = rep(c, "void Sandbox::setFaceMode(FaceImage* newFaceImage,DEM* newFaceDem)\n", """void Sandbox::atualizaExtras(void)
	{
	double agora=Vrui::getApplicationTime();
	float dt=extrasUltimoPasso>0.0?float(agora-extrasUltimoPasso):0.0f;
	extrasUltimoPasso=agora;
	if(!extras.ativo())
		return;
	
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
		glVertexAttrib1fARB(1,GLfloat(extras.labVazao/waterSpeed));
		glBegin(GL_POLYGON);
		for(int i=0;i<24;++i)
			{
			Scalar angle=Scalar(2)*Math::Constants<Scalar>::pi*Scalar(i)/Scalar(24);
			glVertex(centro+x*(Math::cos(angle)*Scalar(2))+y*(Math::sin(angle)*Scalar(2)));
			}
		glEnd();
		glPopAttrib();
		}
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
			}
""")
    # "Ran out of time" a cada quadro enchia o log; conta e mostra junto do desempenho
    c = rep(c, """		if(totalTimeStep>1.0e-8f)
			std::cout<<"Ran out of time by "<<totalTimeStep<<std::endl;
""", """		++quadrosAgua;
		if(totalTimeStep>1.0e-8f)
			++atrasosAgua;
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
		std::cout<<"Desempenho: "<<int(quadros/dt+0.5)<<" quadros/s; água atrasada em "<<atrasosAgua<<" de "<<quadrosAgua<<" passos"<<std::endl;
		quadros=0;
		quadrosAgua=0;
		atrasosAgua=0;
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

	/* Movimento dos peixes e carrinhos: */
	extras.passo(dt);""")
    open("Sandbox.cpp", "w").write(c)

m = open("makefile").read()
if "FaceImage.cpp" not in m:
    m = rep(m, "                   DEM.cpp \\\n", "                   DEM.cpp \\\n                   FaceImage.cpp \\\n                   Extras.cpp \\\n")
    open("makefile", "w").write(m)
print("Sandbox ok")
