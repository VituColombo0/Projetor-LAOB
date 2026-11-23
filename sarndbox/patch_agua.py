"""Shaders do SARndbox mudados pelo Projetor-LAOB (rodar na pasta da cópia, depois de copiar os originais
para share/.../Shaders; o instalar.sh faz isso):
- SurfaceAddWaterColor.fs: a água (chuva, jogos, lago dos bichos) acompanha o tema de cor (vulcão = lava,
  inverno = azul glacial, marte = ciano, deserto = oásis, arco-íris = colorida só na chuva solta);
- Water2WaterUpdateShader.fs: o lençol do lago dos bichos;
- SurfaceAddContourLines.fs: curvas de nível sem serrilhado e com a linha principal a cada 5 cm, e o
  sombreamento do relevo (relevoLuz, chamado do patch_surface.py) (23/11/2026)."""
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
float ruidoA(vec2 p) // Projetor-LAOB (23/11/2026): ruído de valor barato (o brilho que anda com a correnteza)
	{
	vec2 i=floor(p);
	vec2 f=fract(p);
	f=f*f*(3.0-2.0*f);
	vec4 h=fract(sin(vec4(dot(i,vec2(127.1,311.7)),dot(i+vec2(1.0,0.0),vec2(127.1,311.7)),
	                      dot(i+vec2(0.0,1.0),vec2(127.1,311.7)),dot(i+vec2(1.0,1.0),vec2(127.1,311.7))))*43758.5453);
	return mix(mix(h.x,h.y,f.x),mix(h.z,h.w,f.x),f.y);
	}
""")
    s = rep(s, "\t\tvec4 waterColor=vec4(colorW,colorW,1.0,1.0); // Water\n", """\t\tvec4 waterColor=vec4(colorW,colorW,1.0,1.0); // Water
		/* Projetor-LAOB: a água da chuva tem o mesmo jeito do lago dos peixes (clara no raso, escura no
		   fundo, ondinhas e brilho) e a cor combina com o tema: lava, água gelada, lama, oásis... */
		float efA=extrasEfeito.x;
		float fundoA=smoothstep(0.0,2.0,waterLevel);
		float ondaA=0.5+0.5*sin(waterAnimationTime*2.0+fragCoord.x*0.045+fragCoord.y*0.03);
		vec3 rasoA=vec3(0.14,0.56,0.93);   // azul firme (17/11: a água estava transparente e difícil de ver)
		vec3 profA=vec3(0.04,0.22,0.64);   // fundo firme (23/11: o azul quase preto de antes sumia no projetor)
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
			{ rasoA=vec3(0.20,0.65,0.96); profA=vec3(0.07,0.33,0.74); brilhoA=0.85; } // inverno: azul glacial (destaca da neve)
		else if(efA>2.5&&efA<3.5)
			{ rasoA=vec3(0.18,0.72,0.82); profA=vec3(0.05,0.42,0.58); brilhoA=0.65; } // marte: água mineral ciano (destaca do vermelho)
		else if(efA>3.5&&efA<4.5)
			{ rasoA=vec3(0.16,0.78,0.90); profA=vec3(0.03,0.46,0.62); brilhoA=0.70; } // deserto: oásis nítido (destaca da areia)
		else if(efA>5.5&&efA<6.5&&!lagoA&&extrasLab2.w<0.5) // 23/11/2026: só a chuva solta; jogo, labirinto e lago dos bichos ficam azuis
			{
			rasoA=vec3(0.6)+0.4*cos(6.2832*(vec3(fragCoord.x*0.004+waterAnimationTime*0.15)+vec3(0.0,0.33,0.67)));
			profA=rasoA*0.55;                                                        // arco-íris
			}
		waterColor=vec4(mix(rasoA,profA,fundoA)+vec3(0.06*ondaA)+vec3(colorW*brilhoA*0.5),1.0);
		/* 23/11/2026 (direção de arte V3): margem clara e contínua na beira (só onde a lâmina é bem fina; sem
		   anéis, sem piscar) e um brilho sutil que ANDA COM A CORRENTEZA: ruído levado pela velocidade da água
		   (momento / altura, da grade da água para a tela), em duas camadas defasadas que se revezam aos poucos
		   (o desenho nunca estica nem pula). Na água parada o brilho quase some. A lava não tem nada disso. */
		if(!(extrasEfeito.w>0.5||(efA>0.5&&efA<1.5&&!lagoA)))
			{
			float espA=smoothstep(0.012,0.035,waterLevel)*(1.0-smoothstep(0.05,0.12,waterLevel));
			vec2 velA=texture2DRect(quantitySampler,waterTexCoord).gb/max(waterLevel,0.08)/waterCellSize; // células/s
			float detA=wJx.x*wJy.y-wJy.x*wJx.y;
			vec2 pxA=abs(detA)>1.0e-6?vec2(wJy.y*velA.x-wJy.x*velA.y,-wJx.y*velA.x+wJx.x*velA.y)/detA:vec2(0.0); // pixels/s
			float rapA=length(pxA);
			pxA*=min(1.0,70.0/max(rapA,1.0e-3));
			float brA=0.0;
			if(rapA>6.0) // água parada: sem brilho (e sem a conta: o ruído é o mais caro da água)
				{
				float f0=fract(waterAnimationTime*0.5);
				float f1=fract(waterAnimationTime*0.5+0.5);
				float n0=ruidoA((fragCoord-pxA*f0*2.0)*0.05);
				float n1=ruidoA((fragCoord-pxA*f1*2.0)*0.05+vec2(17.3,5.1));
				float w0=1.0-abs(1.0-2.0*f0);
				brA=smoothstep(0.62,0.9,n0*w0+n1*(1.0-w0))*mix(0.05,0.2,smoothstep(8.0,40.0,rapA))*(1.0-0.5*espA);
				}
			waterColor.rgb=mix(waterColor.rgb,vec3(0.62,0.88,1.0),0.4*espA)+vec3(brA);
			}
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
    # (23/11/2026) a grade da água por pixel da tela, fora do if (derivada em controle uniforme)
    s = rep(s, "\tfloat waterLevel=texture2DRect(quantitySampler,waterTexCoord).r-b;\n",
            "\tfloat waterLevel=texture2DRect(quantitySampler,waterTexCoord).r-b;\n"
            "\tvec2 wJx=dFdx(waterTexCoord),wJy=dFdy(waterTexCoord); // Projetor-LAOB: grade da água por pixel (correnteza na tela)\n")
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
# Curvas de nível (23/11/2026, direção de arte V2): o original pinta 1 pixel preto onde a curva passa
# (serrilhado, e some de longe na areia). Agora: espessura certa pela distância em pixels até a curva
# (diferença de altura / gradiente), borda suave, e a cada 5 cm a linha principal, mais grossa e firme.
# A linha é o próprio chão bem mais escuro (no chão escuro, mais claro), não um preto chapado.
ARQ3 = "share/SARndbox-2.8/Shaders/SurfaceAddContourLines.fs"
ATUAL = ARQ3
c = open(ARQ3).read()
if "Projetor-LAOB" not in c:
    i = c.index("void addContourLines(in vec2 fragCoord,inout vec4 baseColor)")
    c = c[:i] + """/* Projetor-LAOB (23/11/2026): curvas de nível sem serrilhado, com espessura constante, e a principal a cada 5 cm */
void addContourLines(in vec2 fragCoord,inout vec4 baseColor)
	{
	/* altura (em número de curvas) no meio do pixel e quanto ela muda por pixel, pelos 4 cantos */
	float c0=texture2DRect(pixelCornerElevationSampler,vec2(fragCoord.x,fragCoord.y)).r;
	float c1=texture2DRect(pixelCornerElevationSampler,vec2(fragCoord.x+1.0,fragCoord.y)).r;
	float c2=texture2DRect(pixelCornerElevationSampler,vec2(fragCoord.x,fragCoord.y+1.0)).r;
	float c3=texture2DRect(pixelCornerElevationSampler,vec2(fragCoord.x+1.0,fragCoord.y+1.0)).r;
	float e=(c0+c1+c2+c3)*0.25*contourLineFactor;
	float g=max(length(vec2(c1-c0+c3-c2,c2-c0+c3-c1))*0.5*contourLineFactor,0.004);
	/* distância (pixels) até a curva mais perto e se ela é a principal (a cada 5 cm) */
	float d=abs(fract(e+0.5)-0.5)/g;
	float cada=max(floor(5.0*contourLineFactor+0.5),1.0);
	float principal=1.0-step(0.5,mod(floor(e+0.5),cada));
	float meia=mix(1.0,1.45,principal);
	float a=(1.0-smoothstep(meia-0.6,meia+0.6,d))*mix(0.8,0.88,principal);
	a*=1.0-smoothstep(0.3,0.6,g);                    // encosta tão íngreme que as curvas se juntam: sem mancha escura
	float lum=dot(baseColor.rgb,vec3(0.3,0.59,0.11));
	vec3 linha=mix(baseColor.rgb*0.5+vec3(0.26),baseColor.rgb*mix(0.3,0.14,principal),smoothstep(0.1,0.22,lum));
	baseColor.rgb=mix(baseColor.rgb,linha,a);
	}

/* Projetor-LAOB (23/11/2026, direção de arte V1): sombreamento suave do relevo, para a areia parecer 3D.
   Inclinação pela altura 4 e 8 pixels para cada lado (~0,5 cm: o ruído do sensor e a areia
   granulada quase não aparecem), luz de cima à
   esquerda da imagem; devolve o fator da cor: 0,85 (encosta de costas para a luz) a 1,10 (de frente), 1 no plano. */
float relevoLuz(in vec2 fragCoord)
	{
	vec2 c=fragCoord+vec2(0.5);
	float ex=texture2DRect(pixelCornerElevationSampler,c+vec2(8.0,0.0)).r-texture2DRect(pixelCornerElevationSampler,c-vec2(8.0,0.0)).r
	        +2.0*(texture2DRect(pixelCornerElevationSampler,c+vec2(4.0,0.0)).r-texture2DRect(pixelCornerElevationSampler,c-vec2(4.0,0.0)).r);
	float ey=texture2DRect(pixelCornerElevationSampler,c+vec2(0.0,8.0)).r-texture2DRect(pixelCornerElevationSampler,c-vec2(0.0,8.0)).r
	        +2.0*(texture2DRect(pixelCornerElevationSampler,c+vec2(0.0,4.0)).r-texture2DRect(pixelCornerElevationSampler,c-vec2(0.0,4.0)).r);
	vec3 n=normalize(vec3(-vec2(ex,ey)*(17.0/32.0),1.0));   // (16 + 2*8 pixels) e ~17 pixels por cm na areia
	const vec3 L=vec3(-0.485,0.485,0.728);
	return 1.0+clamp((dot(n,L)-L.z)*0.5,-0.15,0.10);
	}

/* (o original, guardado sem uso) */
void addContourLinesOriginal(in vec2 fragCoord,inout vec4 baseColor)
""" + c[i+len("void addContourLines(in vec2 fragCoord,inout vec4 baseColor)\n"):]
    open(ARQ3, "w").write(c)
print("água e curvas ok")
