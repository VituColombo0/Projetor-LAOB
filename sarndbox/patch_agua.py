"""A água do SARndbox (a chuva da mão aberta) acompanha o tema de cor (Projetor-LAOB):
vulcão = lava, inverno = neve, marte = lama vermelha, deserto = oásis, arco-íris = colorida.
Rodar na pasta da cópia, depois de copiar o shader original para share/.../Shaders."""
import sys

ATUAL = ""  # o shader sendo mudado (para a mensagem de erro)

def rep(s, old, new):
    """Troca o trecho (uma vez); sem ele, para com o nome do shader. Fora dos dois blocos (23/11/2026): antes
    ficava dentro do 1º, e um 2º shader sem patch com o 1º já pronto dava NameError."""
    if old not in s:
        sys.exit(f"trecho não encontrado em {ATUAL}: {old[:60]!r}")
    return s.replace(old, new, 1)

ARQ = "share/SARndbox-2.8/Shaders/SurfaceAddWaterColor.fs"
ATUAL = ARQ
s = open(ARQ).read()
if "extrasEfeito" not in s:
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
		float brilhoA=0.5;
		/* com bichos da água (lago ligado: extrasInfo.w = nível do mar) a água é água em todo tema: peixe não nada em lava */
		bool lagoA=extrasInfo.w>-999.0;
		if(extrasEfeito.w>0.5||(efA>0.5&&efA<1.5&&!lagoA))
			{
			rasoA=vec3(1.0,0.66,0.12); profA=vec3(0.78,0.13,0.0); brilhoA=0.9;       // vulcão: lava
			if(extrasEfeito.w>0.5)
				rasoA=vec3(1.0,0.8,0.3);                                             // jogo do vulcão: bem clara
			}
		else if(efA>1.5&&efA<2.5)
			{ rasoA=vec3(0.20,0.65,0.96); profA=vec3(0.05,0.28,0.65); brilhoA=0.85; } // inverno: azul glacial (destaca da neve)
		else if(efA>2.5&&efA<3.5)
			{ rasoA=vec3(0.18,0.72,0.82); profA=vec3(0.05,0.35,0.48); brilhoA=0.65; } // marte: água mineral ciano (destaca do vermelho)
		else if(efA>3.5&&efA<4.5)
			{ rasoA=vec3(0.16,0.78,0.90); profA=vec3(0.02,0.40,0.52); brilhoA=0.70; } // deserto: oásis nítido (destaca da areia)
		else if(efA>5.5&&efA<6.5&&!lagoA&&extrasLab2.w<0.5) // 23/11/2026: só a chuva solta; jogo, labirinto e lago dos bichos ficam azuis
			{
			rasoA=vec3(0.6)+0.4*cos(6.2832*(vec3(fragCoord.x*0.004+waterAnimationTime*0.15)+vec3(0.0,0.33,0.67)));
			profA=rasoA*0.55;                                                        // arco-íris
			}
		waterColor=vec4(mix(rasoA,profA,fundoA)+vec3(0.06*ondaA)+vec3(colorW*brilhoA*0.5),1.0);
""")
    # a lâmina fina já aparece com cor viva e nítida, e os animais continuam à vista dentro da água
    s = rep(s, "\t\tbaseColor=mix(baseColor,waterColor,min(waterLevel*waterOpacity,1.0));\n", """\t\t/* Projetor-LAOB: cobertura nítida e água vibrante mesmo em lâmina fina (escorrendo):
		   a água dos jogos e da chuva é imediatamente visível como água azul brilhante, não areia escura */
		float cobreA=clamp(waterLevel*5.0,0.0,1.0);
		cobreA=max(cobreA,smoothstep(0.02,0.12,waterLevel)*0.88);
		cobreA=max(cobreA,smoothstep(0.12,0.35,waterLevel));
		if(extrasInfo.x>0.0&&extrasMarca>0.5)
			cobreA*=0.25; // tem um animal marinho aqui (marcado pelo shader dos extras)
		if(extrasInfo.x>0.0) // sombra macia (> 0) ou clarão (< 0) em volta do bicho da cor da água (19/11/2026)
			waterColor.rgb=waterColor.rgb*(1.0-max(extrasAguaSombra,0.0))+(1.0-waterColor.rgb)*0.6*max(-extrasAguaSombra,0.0);
		baseColor=mix(baseColor,waterColor,cobreA);
""")
    s = rep(s, "uniform vec4 extrasEfeito; // Projetor-LAOB: efeito do tema (0 = água normal)\n", """uniform vec4 extrasEfeito; // Projetor-LAOB: efeito do tema (0 = água normal)
uniform vec4 extrasInfo;   // .x = tempo dos extras (0 = extras desligados)
uniform vec4 extrasLab2;   // .w = 1 nos jogos de água e no labirinto (a água é a azul padrão em todo tema)
float extrasMarca;         // posta em 1 pelo shader dos extras onde há um animal marinho
float extrasAguaSombra;    // sombra na água em volta dos bichos (shader dos extras)
""")
    open(ARQ, "w").write(s)

# Lençol do lago dos bichos (17/11/2026): no passo que soma a água da chuva, em toda célula da área iluminada
# cujo fundo está abaixo do nível do mar a água brota até ele; acima do nível, a areia absorve aos poucos.
ARQ2 = "share/SARndbox-2.8/Shaders/Water2WaterUpdateShader.fs"
ATUAL = ARQ2
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
