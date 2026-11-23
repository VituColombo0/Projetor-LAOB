"""Acrescenta o modo Rosto ao SurfaceRenderer do SARndbox 2.8 (rodar na pasta da cópia)."""
import sys

def rep(s, old, new, count=1):
    if s.count(old) < 1:
        sys.exit(f"trecho não encontrado: {old[:70]!r}")
    return s.replace(old, new, count)

h = open("SurfaceRenderer.h").read()
if "FaceImage" not in h:
    h = rep(h, "class DEM;\n", "class DEM;\nclass FaceImage;\n")
    # 48: com tudo ligado o shader pede 22 + Extras::NUM_UNIFORMS localizações (42 desde 22/11/2026; o static_assert
    # abaixo para a compilação se faltar lugar, em vez de escrever por cima das variáveis seguintes). Desde 23/11 as
    # localizações ficam em DataItem::Programa::uniforms[48] (um programa por chave, mais abaixo); este fica sem uso.
    h = rep(h, "GLint heightMapShaderUniforms[16];", "GLint heightMapShaderUniforms[48];")
    h = rep(h, "\tGLfloat demDistScale; // Maximum deviation", "\tFaceImage* faceImage; // Foto projetada na areia (modo Rosto); 0 = desligado\n\tGLfloat demDistScale; // Maximum deviation")
    h = rep(h, "\tvoid setDemDistScale(", "\tvoid setFaceImage(FaceImage* newFaceImage); // Liga/desliga o modo Rosto\n\tvoid setDemDistScale(")
    h = rep(h, "class FaceImage;\n", "class FaceImage;\nclass Extras;\n")
    h = rep(h, "\tFaceImage* faceImage; // Foto projetada na areia (modo Rosto); 0 = desligado\n",
            "\tFaceImage* faceImage; // Foto projetada na areia (modo Rosto); 0 = desligado\n"
            "\tconst Extras* extras; // Tesouro, lago, peixes e carrinhos (Projetor-LAOB); 0 = desligado\n")
    h = rep(h, "\tvoid setFaceImage(FaceImage* newFaceImage); // Liga/desliga o modo Rosto\n",
            "\tvoid setFaceImage(FaceImage* newFaceImage); // Liga/desliga o modo Rosto\n"
            "\tvoid setExtras(const Extras* newExtras); // Liga/desliga tesouro e animais\n"
            "\tstatic GLfloat luzRealce[2]; // Projetor-LAOB: realce da luz (força da gama, saturação a mais; comando luz)\n"
            "\tstatic unsigned int compilacoes; static double compilacaoMs; // Projetor-LAOB: programas compilados e o tempo somado (Perfil)\n")
    # Um programa por chave (23/11/2026): extras e a variante deles, foto e DEM. Antes, cada troca (ligar a foto,
    # mudar de brincadeira, até "relevo" estando no relevo) recompilava o shader inteiro: 75-84 ms com o cache do
    # driver, ~3,7 s sem ele. Agora as variantes comuns são compiladas ao abrir e a troca só escolhe o programa.
    h = rep(h, "#include <IO/FileMonitor.h>\n", "#include <vector>\n#include <IO/FileMonitor.h>\n")
    h = rep(h, "\t\tunsigned int lightTrackerVersion; // Version number of light tracker state for which the height map shader was built\n",
            "\t\tunsigned int lightTrackerVersion; // Version number of light tracker state for which the height map shader was built\n"
            "\t\tstruct Programa {GLhandleARB programa; GLint uniforms[48]; unsigned int chave;}; // Projetor-LAOB: um programa pronto (0 = não compilou)\n"
            "\t\tstd::vector<Programa> programas; // Projetor-LAOB: os já compilados, um por chave (chaveShader); heightMapShader é o em uso\n")
    h = rep(h, "\tconst Extras* extras; // Tesouro, lago, peixes e carrinhos (Projetor-LAOB); 0 = desligado\n",
            "\tconst Extras* extras; // Tesouro, lago, peixes e carrinhos (Projetor-LAOB); 0 = desligado\n"
            "\tunsigned int extrasVariante; // Projetor-LAOB: o código dos extras que entra no shader (Extras::variante)\n"
            "\tmutable unsigned int chaveCompila; // Projetor-LAOB: a chave do programa sendo montado (createSinglePassSurfaceShader)\n")
    h = rep(h, "\tGLhandleARB createSinglePassSurfaceShader(const GLLightTracker& lt,GLint* uniformLocations) const;",
            "\tunsigned int chaveShader(void) const; // Projetor-LAOB: bits 0-7 variante dos extras, 0x100 extras, 0x200 foto, 0x400 DEM\n"
            "\tconst DataItem::Programa& programa(DataItem* dataItem,const GLLightTracker& lt,unsigned int chave) const; // Projetor-LAOB: o da chave (compila na 1ª vez)\n"
            "\tGLhandleARB createSinglePassSurfaceShader(const GLLightTracker& lt,GLint* uniformLocations) const;")
    open("SurfaceRenderer.h", "w").write(h)

c = open("SurfaceRenderer.cpp").read()
if "FaceImage" not in c:
    c = rep(c, '#include "DEM.h"\n', '#include "DEM.h"\n#include "FaceImage.h"\n')
    c = rep(c, "\t dem(0),demDistScale(1.0f),", "\t dem(0),faceImage(0),demDistScale(1.0f),")
    # vertex shader: coordenadas da foto e altura
    c = rep(c, """					heightColorMapTexCoord=dot(heightColorMapPlaneEq,vertexCc);\\n\\
					\\n";
				}""", """					heightColorMapTexCoord=dot(heightColorMapPlaneEq,vertexCc);\\n\\
					\\n";
				
				if(chaveCompila&0x200u) // (montando o programa da chave: foto)
					{
					/* Modo Rosto: posição na foto e altura da areia: */
					vertexUniforms+="\\
						uniform mat4 faceTransform; // Espaço da câmera -> (u, v, altura em cm)\\n";
					vertexVaryings+="\\
						varying vec3 faceTexCoord;\\n";
					vertexMain+="\\
						faceTexCoord=(faceTransform*vertexCc).xyz;\\n\\
						\\n";
					}
				}""")
    # fragment shader: cor da foto deformada pela altura e sombreada pelo relevo
    c = rep(c, """					vec4 baseColor=texture1D(heightColorMapSampler,heightColorMapTexCoord);\\n\\
					\\n";
""", """					vec4 baseColor=texture1D(heightColorMapSampler,heightColorMapTexCoord);\\n\\
					\\n";
				
				/* Projetor-LAOB (22/11/2026): realce da luz (gama e saturação na cor final; comando "luz") */
				fragmentUniforms+="\\
					uniform vec2 luzRealce; // força (gama) e saturação a mais\\n";
				
				if(chaveCompila&0x200u)
					{
					fragmentUniforms+="\\
						uniform sampler2D faceSampler;\\n\\
						uniform vec4 faceParams; // ampliação/cm, deslocamento/cm, mistura, sombra\\n\\
						uniform float faceReference; // altura de referência da areia (cm)\\n";
					fragmentVaryings+="\\
						varying vec3 faceTexCoord;\\n";
					fragmentMain+="\\
						/* Modo Foto: a areia alta amplia a foto (lente) e o relevo faz sombra: */\\n\\
						float faceH=faceTexCoord.z-faceReference;\\n\\
						vec2 faceUv=vec2(0.5)+(faceTexCoord.xy-vec2(0.5))*(1.0-clamp(faceParams.x*faceH,-0.8,0.8));\\n\\
						faceUv.x+=faceParams.y*faceH;\\n\\
						vec4 faceOut=vec4(0.0,0.0,0.0,1.0); // fora da foto: preto (projetor apagado)\\n\\
						if(faceUv.x>=0.0&&faceUv.x<=1.0&&faceUv.y>=0.0&&faceUv.y<=1.0)\\n\\
							{\\n\\
							vec3 faceN=normalize(vec3(-dFdx(faceTexCoord.z)*faceParams.w*8.0,-dFdy(faceTexCoord.z)*faceParams.w*8.0,1.0));\\n\\
							float faceShade=clamp(0.45+0.75*dot(faceN,normalize(vec3(-0.5,0.5,0.7))),0.25,1.25);\\n\\
							vec4 faceColor=texture2D(faceSampler,faceUv);\\n\\
							faceOut=vec4(faceColor.rgb*faceShade,1.0);\\n\\
							}\\n\\
						\\n";
					}
""")
    # no modo Foto a saída é só a foto (fundo preto), sem cores de relevo, curvas de nível nem água;
    # faceParams.z (mistura) traz o relevo de volta se alguém quiser
    c = rep(c, """		fragmentMain+="\\
			/* Assign the final color to the fragment: */\\n\\
			gl_FragColor=baseColor;\\n\\
			}\\n";
""", """		if(fragmentMain.find("faceOut")!=std::string::npos)
			fragmentMain+="\\
			/* Modo Foto: só a foto; o relevo só entra pela mistura: */\\n\\
			gl_FragColor=mix(faceOut,baseColor,faceParams.z);\\n\\
			if(luzRealce.x>0.0||luzRealce.y>0.0)\\n\\
				{\\n\\
				vec3 luzC=clamp(gl_FragColor.rgb,0.0,1.0);\\n\\
				luzC=clamp(mix(vec3(dot(luzC,vec3(0.299,0.587,0.114))),luzC,1.0+0.6*luzRealce.y),0.0,1.0);\\n\\
				gl_FragColor.rgb=pow(luzC,vec3(1.0/(1.0+0.6*luzRealce.x)));\\n\\
				}\\n\\
			}\\n";
		else if(fragmentUniforms.find("luzRealce")!=std::string::npos)
			{
			/* Projetor-LAOB: luz mais forte e cada coisa mais separada do chão (pedidos do Victor, 22/11/2026).
			   Os tons médios sobem por 1-(1-c)^(1+f): o preto quase não sobe, então os contornos escuros dos
			   bichos, pessoas e comida continuam nítidos (a gama de antes os lavava). Onde há objeto
			   (extrasObjeto, do Extras.cpp) a cor fica bem mais saturada e com mais contraste e sobe menos;
			   o chão fica com a saturação moderada: os objetos saltam do relevo. */
			std::string luzObj=fragmentUniforms.find("extrasObjeto")!=std::string::npos?"extrasObjeto":"0.0";
			fragmentMain+="\\
			/* Assign the final color to the fragment: */\\n\\
			gl_FragColor=baseColor;\\n\\
			if(luzRealce.x>0.0||luzRealce.y>0.0)\\n\\
				{\\n\\
				vec3 luzC=clamp(gl_FragColor.rgb,0.0,1.0);\\n\\
				float luzO="+luzObj+";\\n\\
				float luzY=dot(luzC,vec3(0.299,0.587,0.114));\\n\\
				luzC=clamp(mix(vec3(luzY),luzC,1.0+luzRealce.y*mix(0.5,1.5,luzO)),0.0,1.0);\\n\\
				luzC=mix(luzC,clamp((luzC-0.5)*(1.0+0.3*luzRealce.x)+0.5,0.0,1.0),luzO);\\n\\
				gl_FragColor.rgb=1.0-pow(1.0-luzC,vec3(1.0+luzRealce.x*mix(1.0,0.5,luzO)));\\n\\
				}\\n\\
			}\\n";
			}
		else
		fragmentMain+="\\
			/* Assign the final color to the fragment: */\\n\\
			gl_FragColor=baseColor;\\n\\
			}\\n";
""")
    # localização dos uniforms (mesma ordem do envio)
    c = rep(c, """			*(ulPtr++)=glGetUniformLocationARB(result,"heightColorMapSampler");
			}""", """			*(ulPtr++)=glGetUniformLocationARB(result,"heightColorMapSampler");
			*(ulPtr++)=glGetUniformLocationARB(result,"luzRealce"); // Projetor-LAOB
			if(chaveCompila&0x200u)
				{
				*(ulPtr++)=glGetUniformLocationARB(result,"faceTransform");
				*(ulPtr++)=glGetUniformLocationARB(result,"faceSampler");
				*(ulPtr++)=glGetUniformLocationARB(result,"faceParams");
				*(ulPtr++)=glGetUniformLocationARB(result,"faceReference");
				}
			}""")
    c = rep(c, """		elevationColorMap->bindTexture(contextData);
		glUniform1iARB(*(ulPtr++),1);
		}
""", """		elevationColorMap->bindTexture(contextData);
		glUniform1iARB(*(ulPtr++),1);
		glUniform2fARB(*(ulPtr++),luzRealce[0],luzRealce[1]); // Projetor-LAOB: realce da luz
		
		if(faceImage!=0)
			{
			/* Modo Rosto: matriz, foto (unidade 5), parâmetros e referência: */
			GLint tl=*(ulPtr++);
			GLint sl=*(ulPtr++);
			GLint pl=*(ulPtr++);
			GLint rl=*(ulPtr++);
			glActiveTextureARB(GL_TEXTURE5_ARB);
			faceImage->bindTexture(contextData);
			glUniform1iARB(sl,5);
			faceImage->uploadUniforms(tl,pl,rl);
			}
		}
""")
    c = rep(c, "void SurfaceRenderer::setDemDistScale(", """GLfloat SurfaceRenderer::luzRealce[2]={0.5f,0.3f}; // Projetor-LAOB: realce padrão "forte" (comando "luz")

void SurfaceRenderer::setFaceImage(FaceImage* newFaceImage)
	{
	/* Ligar/desligar a foto troca de programa (a foto está na chave: chaveShader); trocar uma foto por outra
	   não muda nada no shader (a textura e a matriz vão a cada quadro). Até 23/11/2026 todo "relevo",
	   tesouro, jogo... recompilava o shader inteiro aqui, mesmo sem foto antes e depois: 75-84 ms */
	faceImage=newFaceImage;
	}

void SurfaceRenderer::setDemDistScale(""")
    # --- Extras (Projetor-LAOB): lago, tesouro, peixes e carrinhos desenhados sobre o relevo
    c = rep(c, '#include "FaceImage.h"\n', '#include "FaceImage.h"\n#include "Extras.h"\n')
    c = rep(c, "\t dem(0),faceImage(0),demDistScale(1.0f),", "\t dem(0),faceImage(0),extras(0),demDistScale(1.0f),")
    c = rep(c, """					vertexMain+="\\
						faceTexCoord=(faceTransform*vertexCc).xyz;\\n\\
						\\n";
					}
""", """					vertexMain+="\\
						faceTexCoord=(faceTransform*vertexCc).xyz;\\n\\
						\\n";
					}
				if(chaveCompila&0x100u) // (extras na chave do programa)
					{
					vertexUniforms+=Extras::glslVertexUniforms;
					vertexMain+=Extras::glslVertexMain;
					}
""")
    c = rep(c, """							faceOut=vec4(faceColor.rgb*faceShade,1.0);\\n\\
							}\\n\\
						\\n";
					}
""", """							faceOut=vec4(faceColor.rgb*faceShade,1.0);\\n\\
							}\\n\\
						\\n";
					}
				if(chaveCompila&0x100u)
					{
					fragmentUniforms+=Extras::glslDefinicoes(chaveCompila&0xffu); // só o código da brincadeira em uso (23/11/2026)
					fragmentUniforms+=Extras::glslFragmentUniforms;
					fragmentMain+=Extras::glslFragmentMain;
					}
""")
    c = rep(c, """				*(ulPtr++)=glGetUniformLocationARB(result,"faceReference");
				}
""", """				*(ulPtr++)=glGetUniformLocationARB(result,"faceReference");
				}
			/* pior caso: 2 comuns + 2 do mapa de cores + 1 da luz + 4 do Rosto + os Extras + 2 das curvas de nível + 2 do
			   dipping bed + 2 da iluminação + 6 da água + 1 da projeção (o DEM, 3, entra no lugar do mapa) */
			static_assert(22+Extras::NUM_UNIFORMS<=sizeof(DataItem::Programa::uniforms)/sizeof(GLint),
			              "Programa::uniforms pequeno demais para Extras::NUM_UNIFORMS");
			if(chaveCompila&0x100u)
				for(int k=0;k<Extras::NUM_UNIFORMS;++k)
					*(ulPtr++)=glGetUniformLocationARB(result,Extras::nomesUniforms[k]);
""")
    c = rep(c, """			faceImage->uploadUniforms(tl,pl,rl);
			}
""", """			faceImage->uploadUniforms(tl,pl,rl);
			}
		if(extras!=0)
			{
			GLint el[Extras::NUM_UNIFORMS];
			for(int k=0;k<Extras::NUM_UNIFORMS;++k)
				el[k]=*(ulPtr++);
			extras->uploadUniforms(el);
			}
""")
    # laboratório (Mundo): sem as linhas de relevo por cima da sala (a luz do relevo continua).
    # Bichos (19/11/2026): as linhas somem sob o corpo e vão sumindo logo em volta (extrasLimpa, do Extras.cpp)
    c = rep(c, """			fragmentMain+="\\
				/* Modulate the base color by contour line color: */\\n\\
				addContourLines(gl_FragCoord.xy,baseColor);\\n\\
				\\n";
""", """			if((chaveCompila&0x100u)&&dem==0&&elevationColorMap!=0) // só onde as declarações dos extras entram (no Esculpir, com o DEM, não: 19/11/2026)
				fragmentMain+="\\
				if(extrasMundo.w<0.5)\\n\\
					{\\n\\
					vec4 exSemCurva=baseColor;\\n\\
					addContourLines(gl_FragCoord.xy,baseColor);\\n\\
					baseColor=mix(baseColor,exSemCurva,extrasLimpa);\\n\\
					}\\n\\
				\\n";
			else
				fragmentMain+="\\
				/* Modulate the base color by contour line color: */\\n\\
				addContourLines(gl_FragCoord.xy,baseColor);\\n\\
				\\n";
""")
    c = rep(c, "void SurfaceRenderer::setFaceImage(FaceImage* newFaceImage)", """void SurfaceRenderer::setExtras(const Extras* newExtras)
	{
	/* Ligar/desligar o tesouro ou os animais, ou mudar de brincadeira, só troca de programa: os extras e a
	   variante deles estão na chave (chaveShader; chamado a cada quadro, depois dos comandos do cano) */
	extras=newExtras;
	extrasVariante=newExtras!=0?newExtras->variante():0u;
	}

void SurfaceRenderer::setFaceImage(FaceImage* newFaceImage)""")
    # --- Um programa por chave (23/11/2026): extras e a variante deles, foto e DEM; os outros ajustes
    # (curvas de nível, água, iluminação, arquivos dos shaders) continuam pela versão e recompilam tudo
    c = rep(c, "#include <vector>\n", "#include <vector>\n#include <chrono>\n#include <iostream>\n")
    c = rep(c, "\t dem(0),faceImage(0),extras(0),demDistScale(1.0f),", "\t dem(0),faceImage(0),extras(0),extrasVariante(0),chaveCompila(0),demDistScale(1.0f),")
    c = rep(c, "\tglDeleteObjectARB(heightMapShader);\n", """	for(size_t k=0;k<programas.size();++k) // Projetor-LAOB: o heightMapShader é um destes
		glDeleteObjectARB(programas[k].programa);
""")
    c = rep(c, """	/* Check if setting this DEM invalidates the shader: */
	if((newDem!=0&&dem==0)||(newDem==0&&dem!=0))
		++surfaceSettingsVersion;
""", """	/* Projetor-LAOB: o DEM está na chave do programa (chaveShader): ligar/desligar só troca de programa */
""")
    c = rep(c, """	/* Create the height map render shader: */
	dataItem->heightMapShader=createSinglePassSurfaceShader(*contextData.getLightTracker(),dataItem->heightMapShaderUniforms);
	dataItem->surfaceSettingsVersion=surfaceSettingsVersion;
""", """	/* Create the height map render shader (Projetor-LAOB: o do estado de agora e, já ao abrir, os das variantes
	   comuns dos extras, para nenhuma troca de brincadeira travar compilando; com o cache do driver é
	   rápido, sem ele, depois de instalar, a 1ª abertura demora uns segundos a mais): */
	unsigned int chave=chaveShader();
	dataItem->heightMapShader=programa(dataItem,*contextData.getLightTracker(),chave).programa;
	if((chave&0x200u)==0)
		{
		for(int k=0;k<Extras::NUM_VARIANTES;++k)
			programa(dataItem,*contextData.getLightTracker(),(chave&0x400u)|0x100u|Extras::varianteComum(k));
		programa(dataItem,*contextData.getLightTracker(),(chave&0x400u)|0x200u); // a foto (pequeno)
		}
	std::cout<<"Superficie: "<<dataItem->programas.size()<<" programas prontos ao abrir ("<<int(compilacaoMs+0.5)<<" ms)"<<std::endl;
	dataItem->surfaceSettingsVersion=surfaceSettingsVersion;
""")
    c = rep(c, """	/* Check if the single-pass surface shader is outdated: */
	if(dataItem->surfaceSettingsVersion!=surfaceSettingsVersion||(illuminate&&dataItem->lightTrackerVersion!=contextData.getLightTracker()->getVersion()))
		{
		/* Rebuild the shader: */
		try
			{
			GLhandleARB newShader=createSinglePassSurfaceShader(*contextData.getLightTracker(),dataItem->heightMapShaderUniforms);
			glDeleteObjectARB(dataItem->heightMapShader);
			dataItem->heightMapShader=newShader;
			}
		catch(const std::runtime_error& err)
			{
			Misc::formattedUserError("SurfaceRenderer::renderSinglePass: Caught exception %s while rebuilding surface shader",err.what());
			}
		
		/* Mark the shader as up-to-date: */
		dataItem->surfaceSettingsVersion=surfaceSettingsVersion;
		dataItem->lightTrackerVersion=contextData.getLightTracker()->getVersion();
		}
	
	/* Bind the single-pass surface shader: */
	glUseProgramObjectARB(dataItem->heightMapShader);
	const GLint* ulPtr=dataItem->heightMapShaderUniforms;
""", """	/* Check if the single-pass surface shader is outdated (Projetor-LAOB: a versão vale para todos os
	   programas guardados; extras, foto e DEM só escolhem o programa da chave, compilado na 1ª vez): */
	if(dataItem->surfaceSettingsVersion!=surfaceSettingsVersion||(illuminate&&dataItem->lightTrackerVersion!=contextData.getLightTracker()->getVersion()))
		{
		for(size_t k=0;k<dataItem->programas.size();++k)
			glDeleteObjectARB(dataItem->programas[k].programa);
		dataItem->programas.clear();
		
		/* Mark the shader as up-to-date: */
		dataItem->surfaceSettingsVersion=surfaceSettingsVersion;
		dataItem->lightTrackerVersion=contextData.getLightTracker()->getVersion();
		}
	const DataItem::Programa& pr=programa(dataItem,*contextData.getLightTracker(),chaveShader());
	dataItem->heightMapShader=pr.programa;
	
	/* Bind the single-pass surface shader: */
	glUseProgramObjectARB(pr.programa);
	const GLint* ulPtr=pr.uniforms;
""")
    c = rep(c, "void SurfaceRenderer::setDemDistScale(", """unsigned int SurfaceRenderer::compilacoes=0;
double SurfaceRenderer::compilacaoMs=0.0;

unsigned int SurfaceRenderer::chaveShader(void) const
	{
	/* Projetor-LAOB (23/11/2026): o que muda o texto do shader de um quadro para outro */
	return (extras!=0?0x100u|(extrasVariante&0xffu):0u)|(faceImage!=0?0x200u:0u)|(dem!=0?0x400u:0u);
	}

const SurfaceRenderer::DataItem::Programa& SurfaceRenderer::programa(SurfaceRenderer::DataItem* dataItem,const GLLightTracker& lt,unsigned int chave) const
	{
	/* Projetor-LAOB (23/11/2026): o programa da chave; na 1ª vez compila (e guarda, mesmo se falhar: um shader
	   com erro não é recompilado a cada quadro; a mensagem sai uma vez) */
	for(size_t k=0;k<dataItem->programas.size();++k)
		if(dataItem->programas[k].chave==chave)
			return dataItem->programas[k];
	DataItem::Programa p;
	p.programa=0;
	p.chave=chave;
	for(size_t k=0;k<sizeof(p.uniforms)/sizeof(GLint);++k)
		p.uniforms[k]=-1;
	std::chrono::steady_clock::time_point t0=std::chrono::steady_clock::now();
	chaveCompila=chave;
	try
		{
		p.programa=createSinglePassSurfaceShader(lt,p.uniforms);
		}
	catch(const std::exception& err)
		{
		Misc::formattedUserError("SurfaceRenderer: Caught exception %s while building surface shader",err.what());
		for(size_t k=0;k<sizeof(p.uniforms)/sizeof(GLint);++k)
			p.uniforms[k]=-1;
		}
	++compilacoes;
	compilacaoMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
	dataItem->programas.push_back(p);
	return dataItem->programas.back();
	}

void SurfaceRenderer::setDemDistScale(""")
    open("SurfaceRenderer.cpp", "w").write(c)
print("SurfaceRenderer ok")
