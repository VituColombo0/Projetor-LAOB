"""Acrescenta o modo Rosto ao SurfaceRenderer do SARndbox 2.8 (rodar na pasta da cópia)."""
import sys

def rep(s, old, new, count=1):
    if s.count(old) < 1:
        sys.exit(f"trecho não encontrado: {old[:70]!r}")
    return s.replace(old, new, count)

h = open("SurfaceRenderer.h").read()
if "FaceImage" not in h:
    h = rep(h, "class DEM;\n", "class DEM;\nclass FaceImage;\n")
    # 48: com tudo ligado o shader pede 21 + Extras::NUM_UNIFORMS localizações (39 hoje; o static_assert
    # abaixo para a compilação se faltar lugar, em vez de escrever por cima das variáveis seguintes)
    h = rep(h, "GLint heightMapShaderUniforms[16];", "GLint heightMapShaderUniforms[48];")
    h = rep(h, "\tGLfloat demDistScale; // Maximum deviation", "\tFaceImage* faceImage; // Foto projetada na areia (modo Rosto); 0 = desligado\n\tGLfloat demDistScale; // Maximum deviation")
    h = rep(h, "\tvoid setDemDistScale(", "\tvoid setFaceImage(FaceImage* newFaceImage); // Liga/desliga o modo Rosto\n\tvoid setDemDistScale(")
    h = rep(h, "class FaceImage;\n", "class FaceImage;\nclass Extras;\n")
    h = rep(h, "\tFaceImage* faceImage; // Foto projetada na areia (modo Rosto); 0 = desligado\n",
            "\tFaceImage* faceImage; // Foto projetada na areia (modo Rosto); 0 = desligado\n"
            "\tconst Extras* extras; // Tesouro, lago, peixes e carrinhos (Projetor-LAOB); 0 = desligado\n")
    h = rep(h, "\tvoid setFaceImage(FaceImage* newFaceImage); // Liga/desliga o modo Rosto\n",
            "\tvoid setFaceImage(FaceImage* newFaceImage); // Liga/desliga o modo Rosto\n"
            "\tvoid setExtras(const Extras* newExtras); // Liga/desliga tesouro e animais\n")
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
				
				if(faceImage!=0)
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
				
				if(faceImage!=0)
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
			}\\n";
		else
		fragmentMain+="\\
			/* Assign the final color to the fragment: */\\n\\
			gl_FragColor=baseColor;\\n\\
			}\\n";
""")
    # localização dos uniforms (mesma ordem do envio)
    c = rep(c, """			*(ulPtr++)=glGetUniformLocationARB(result,"heightColorMapSampler");
			}""", """			*(ulPtr++)=glGetUniformLocationARB(result,"heightColorMapSampler");
			if(faceImage!=0)
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
    c = rep(c, "void SurfaceRenderer::setDemDistScale(", """void SurfaceRenderer::setFaceImage(FaceImage* newFaceImage)
	{
	/* Ligar/desligar (ou trocar a foto) muda o shader: */
	faceImage=newFaceImage;
	++surfaceSettingsVersion;
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
				if(extras!=0)
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
				if(extras!=0)
					{
					fragmentUniforms+=Extras::glslFragmentUniforms;
					fragmentMain+=Extras::glslFragmentMain;
					}
""")
    c = rep(c, """				*(ulPtr++)=glGetUniformLocationARB(result,"faceReference");
				}
""", """				*(ulPtr++)=glGetUniformLocationARB(result,"faceReference");
				}
			/* pior caso: 2 comuns + 2 do mapa de cores + 4 do Rosto + os Extras + 2 das curvas de nível + 2 do
			   dipping bed + 2 da iluminação + 6 da água + 1 da projeção (o DEM, 3, entra no lugar do mapa) */
			static_assert(21+Extras::NUM_UNIFORMS<=sizeof(DataItem::heightMapShaderUniforms)/sizeof(GLint),
			              "heightMapShaderUniforms pequeno demais para Extras::NUM_UNIFORMS");
			if(extras!=0)
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
""", """			if(extras!=0)
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
	/* Ligar/desligar o tesouro ou os animais muda o shader: */
	if(newExtras!=extras)
		{
		extras=newExtras;
		++surfaceSettingsVersion;
		}
	}

void SurfaceRenderer::setFaceImage(FaceImage* newFaceImage)""")
    open("SurfaceRenderer.cpp", "w").write(c)
print("SurfaceRenderer ok")
