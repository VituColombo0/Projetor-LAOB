"""A água do SARndbox (a chuva da mão aberta) acompanha o tema de cor (Projetor-LAOB):
vulcão = lava, inverno = neve, marte = lama vermelha, deserto = oásis, arco-íris = colorida.
Rodar na pasta da cópia, depois de copiar o shader original para share/.../Shaders."""
import sys

ARQ = "share/SARndbox-2.8/Shaders/SurfaceAddWaterColor.fs"
s = open(ARQ).read()
if "extrasEfeito" not in s:
    def rep(s, old, new):
        if old not in s:
            sys.exit(f"trecho não encontrado em {ARQ}: {old[:60]!r}")
        return s.replace(old, new, 1)
    s = rep(s, "uniform float waterAnimationTime;\n", """uniform float waterAnimationTime;
uniform vec4 extrasEfeito; // Projetor-LAOB: efeito do tema (0 = água normal)
""")
    s = rep(s, "\t\tvec4 waterColor=vec4(colorW,colorW,1.0,1.0); // Water\n", """\t\tvec4 waterColor=vec4(colorW,colorW,1.0,1.0); // Water
		/* Projetor-LAOB: a água da chuva tem o mesmo jeito do lago dos peixes (clara no raso, escura no
		   fundo, ondinhas e brilho) e a cor combina com o tema: lava, água gelada, lama, oásis... */
		float efA=extrasEfeito.x;
		float fundoA=clamp(waterLevel/2.0,0.0,1.0);
		float ondaA=0.5+0.5*sin(waterAnimationTime*2.0+fragCoord.x*0.045+fragCoord.y*0.03);
		vec3 rasoA=vec3(0.14,0.56,0.93);   // azul firme (17/11: a água estava transparente e difícil de ver)
		vec3 profA=vec3(0.02,0.11,0.42);
		float brilhoA=0.4;
		/* com bichos da água (lago ligado: extrasInfo.w = nível do mar) a água é água em todo tema: peixe não nada em lava */
		bool lagoA=extrasInfo.w>-999.0;
		if(extrasEfeito.w>0.5||(efA>0.5&&efA<1.5&&!lagoA))
			{
			rasoA=vec3(1.0,0.66,0.12); profA=vec3(0.78,0.13,0.0); brilhoA=0.9;       // vulcão: lava
			if(extrasEfeito.w>0.5)
				rasoA=vec3(1.0,0.8,0.3);                                             // jogo do vulcão: bem clara
			}
		else if(efA>1.5&&efA<2.5)
			{ rasoA=vec3(0.84,0.95,1.0); profA=vec3(0.42,0.68,0.9); brilhoA=0.8; }   // inverno: água gelada
		else if(efA>2.5&&efA<3.5)
			{ rasoA=vec3(0.80,0.48,0.30); profA=vec3(0.40,0.15,0.09); brilhoA=0.3; } // marte: lama vermelha
		else if(efA>3.5&&efA<4.5)
			{ rasoA=vec3(0.36,0.86,0.80); profA=vec3(0.04,0.46,0.50); }              // deserto: oásis
		else if(efA>5.5&&efA<6.5)
			{
			rasoA=vec3(0.6)+0.4*cos(6.2832*(vec3(fragCoord.x*0.004+waterAnimationTime*0.15)+vec3(0.0,0.33,0.67)));
			profA=rasoA*0.55;                                                        // arco-íris
			}
		waterColor=vec4(mix(rasoA,profA,fundoA)+vec3(0.05*ondaA)+vec3(colorW*brilhoA),1.0);
""")
    # a lâmina fina já aparece, e os animais marinhos continuam à vista dentro da água
    s = rep(s, "\t\tbaseColor=mix(baseColor,waterColor,min(waterLevel*waterOpacity,1.0));\n", """\t\tfloat cobreA=max(min(waterLevel*waterOpacity,1.0),smoothstep(0.0,0.08,waterLevel)*0.6);
		/* lâmina fina (chuva escorrendo): areia molhada, escura e com brilho, em vez de um véu leitoso;
		   vai ganhando a cor da água conforme fica funda (a lava continua brilhando) */
		if(!(extrasEfeito.w>0.5||(efA>0.5&&efA<1.5&&!lagoA)))
			{
			float finaA=1.0-smoothstep(0.12,0.7,waterLevel);
			vec3 molhadaA=baseColor.rgb*0.76+rasoA*0.08+vec3(colorW*brilhoA*0.3);
			waterColor=vec4(mix(waterColor.rgb,molhadaA,finaA),1.0);
			cobreA=max(cobreA,smoothstep(0.0,0.04,waterLevel)*0.92);
			}
		cobreA=max(cobreA,smoothstep(0.08,0.4,waterLevel)); // água de verdade (4 mm ou mais) cobre por inteiro
		if(extrasInfo.x>0.0&&extrasMarca>0.5)
			cobreA*=0.25; // tem um animal marinho aqui (marcado pelo shader dos extras)
		baseColor=mix(baseColor,waterColor,cobreA);
""")
    s = rep(s, "uniform vec4 extrasEfeito; // Projetor-LAOB: efeito do tema (0 = água normal)\n", """uniform vec4 extrasEfeito; // Projetor-LAOB: efeito do tema (0 = água normal)
uniform vec4 extrasInfo;   // .x = tempo dos extras (0 = extras desligados)
float extrasMarca;         // posta em 1 pelo shader dos extras onde há um animal marinho
""")
    open(ARQ, "w").write(s)

# Lençol do lago dos bichos (17/11/2026): no passo que soma a água da chuva, em toda célula da área iluminada
# cujo fundo está abaixo do nível do mar a água brota até ele; acima do nível, a areia absorve aos poucos.
ARQ2 = "share/SARndbox-2.8/Shaders/Water2WaterUpdateShader.fs"
u = open(ARQ2).read()
if "lencolT" not in u:
    u = rep(u, "uniform sampler2DRect waterSampler;\n", """uniform sampler2DRect waterSampler;
uniform vec3 lencolBx;   // Projetor-LAOB: célula -> caixa (cm): bx = x + y.x*célula + z*célula
uniform vec3 lencolBy;
uniform vec3 lencolT;    // nível do mar (altura da água) = x + y*bx + z*by
uniform vec4 lencolArea; // área iluminada na caixa (min x, min y, max x, max y)
uniform vec3 lencolTaxa; // ligado, fração que brota neste passo, cm absorvidos neste passo
""")
    u = rep(u, "\t/* Write the updated quantity: */\n", """	/* Projetor-LAOB: lençol do lago dos bichos */
	if(lencolTaxa.x>0.5)
		{
		float bx=lencolBx.x+lencolBx.y*gl_FragCoord.x+lencolBx.z*gl_FragCoord.y;
		float by=lencolBy.x+lencolBy.y*gl_FragCoord.x+lencolBy.z*gl_FragCoord.y;
		if(bx>lencolArea.x&&by>lencolArea.y&&bx<lencolArea.z&&by<lencolArea.w)
			{
			float T=lencolT.x+lencolT.y*bx+lencolT.z*by;
			float h0=q.x-b;
			if(b<T&&q.x<T)
				q.x+=(T-q.x)*lencolTaxa.y;                      // brota (água nova entra parada)
			else if(q.x>max(b,T))
				{
				q.x-=min(q.x-max(b,T),lencolTaxa.z);              // a areia absorve a água a mais
				float h1=max(q.x-b,0.0);
				q.yz=h1<=0.0?vec2(0.0):q.yz*(h1/max(h0,1.0e-6));
				}
			}
		}
	
	/* Write the updated quantity: */
""")
    open(ARQ2, "w").write(u)
print("água ok")
