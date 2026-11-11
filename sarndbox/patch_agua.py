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
		float efA=extrasEfeito.x;
		if(efA>0.5&&efA<1.5)
			waterColor=vec4(1.0,0.3+0.55*colorW,0.0,1.0); // vulcão: lava
		else if(efA>1.5&&efA<2.5)
			waterColor=vec4(0.92+0.08*colorW,0.95+0.05*colorW,1.0,1.0); // inverno: neve
		else if(efA>2.5&&efA<3.5)
			waterColor=vec4(0.7+0.2*colorW,0.3+0.2*colorW,0.18,1.0); // marte: lama vermelha
		else if(efA>3.5&&efA<4.5)
			waterColor=vec4(0.15+0.8*colorW,0.7+0.3*colorW,0.75,1.0); // deserto: oásis
		else if(efA>5.5&&efA<6.5)
			waterColor=vec4(0.5+0.5*sin(waterAnimationTime*2.0+fragCoord.x*0.02),
			                0.5+0.5*sin(waterAnimationTime*2.0+fragCoord.y*0.02+2.1),
			                0.5+0.5*sin(waterAnimationTime*2.0+4.2),1.0)+vec4(colorW); // arco-íris
""")
    open(ARQ, "w").write(s)
print("água ok")
