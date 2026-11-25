import os
# Análisis de datos - Entrega 2 (genera figuras y resumen.json)
import json, re, os, numpy as np, pandas as pd, matplotlib
matplotlib.use('Agg'); import matplotlib.pyplot as plt
B=os.path.dirname(os.path.dirname(os.path.abspath(__file__))); D=B+'/datos'; F=B+'/figuras'
plt.rcParams.update({'savefig.dpi':300,'font.family':'DejaVu Sans','font.size':8,'axes.edgecolor':'#9a9994','axes.labelcolor':'#0b0b0b',
 'xtick.color':'#52514e','ytick.color':'#52514e','axes.grid':True,'grid.color':'#e6e5e1','grid.linewidth':0.6,'axes.spines.top':False,'axes.spines.right':False})
C1,C2,C3,C4='#2a78d6','#eb6834','#1baf7a','#eda100'
S={}
# ---------- simavr funcional
func={}
for b in [9600,38400,115200]:
    d=pd.read_csv(f'{D}/simavr/func_{b}.csv'); d['bytes']=d.recibido.str.len()+2; func[b]=d
    a,c=np.polyfit(d.bytes,d.lat_resp_ms,1); led=d[d.lat_led_ms>=0].lat_led_ms
    S[f'func_{b}']=dict(n=len(d),ok=int((d.resp_ok&d.leds_ok).sum()),lat_media=d.lat_resp_ms.mean(),lat_mediana=d.lat_resp_ms.median(),
        lat_p95=d.lat_resp_ms.quantile(.95),lat_max=d.lat_resp_ms.max(),pend=a,inter=c,teor=10000/b,led_n=len(led),led_media=led.mean(),led_max=led.max(),
        cat=d.esperado.str.split().str[0].value_counts().to_dict())
# ---------- Wokwi autoprueba
L=open(f'{D}/wokwi/autoprueba_chips_console.txt').read()
r=re.findall(r'RSP "(.*?)" latencia=([\d.]+)',L)
w=pd.DataFrame([(a,len(a)+2,float(b)) for a,b in r],columns=['resp','bytes','lat']); w.to_csv(f'{D}/wokwi/autoprueba_latencias.csv',index=False)
a,c=np.polyfit(w.bytes,w.lat,1)
S['wokwi']=dict(n=len(w),lat_media=w.lat.mean(),lat_min=w.lat.min(),lat_max=w.lat.max(),pend=a,inter=c)
# comparación por respuesta idéntica
sv=func[9600]; comp=[]
for resp in ['PONG','OK V=255 R=0','OK V=255 R=255','ERR comando desconocido']:
    comp.append((resp,len(resp)+2,w[w.resp==resp].lat.median(),sv[sv.recibido==resp].lat_resp_ms.median()))
S['comp']=comp
# ---------- Fig: latencia vs bytes
fig,ax=plt.subplots(figsize=(3.5,2.7))
for b,col in [(9600,C1),(38400,C2),(115200,C3)]:
    d=func[b]; g=d.groupby('bytes').lat_resp_ms.median()
    ax.plot(g.index,g.values,'o',ms=3,color=col,label=f'simavr {b} Bd')
    x=np.array([4,75]); ax.plot(x,x*10000/b,'-',lw=0.8,color=col,alpha=.6)
gw=w.groupby('bytes').lat.median(); ax.plot(gw.index,gw.values,'s',ms=5,mfc='none',mec='#0b0b0b',mew=1,label='Wokwi 9600 Bd')
ax.set_xlabel('Longitud de la respuesta (bytes, con \\r\\n)'); ax.set_ylabel('Latencia comando → respuesta (ms)')
ax.set_yscale('log'); ax.legend(fontsize=6,frameon=False,loc='lower right')
ax.text(5,60,'líneas: 10 bits/byte (8N1 teórico)',fontsize=6,color='#52514e')
fig.tight_layout(); fig.savefig(f'{F}/fig_latencia_bytes.png'); plt.close()
# ---------- Fig: ráfagas
bu=pd.read_csv(f'{D}/simavr/burst.csv'); br=pd.read_csv(f'{D}/simavr/burst_rx256.csv')
fig,ax=plt.subplots(figsize=(3.5,2.5))
for b,col,mk in [(9600,C1,'o'),(115200,C3,'^')]:
    d=bu[bu.baud==b]; ax.plot(d.K,100*d.perdidas/d.K,marker=mk,ms=3.5,lw=1.2,color=col,label=f'búfer RX 64 B ({b} Bd)')
ax.plot(br.K,100*br.perdidas/br.K,marker='s',ms=3.5,lw=1.2,color=C2,label='búfer RX 256 B (9600 Bd)')
ax.set_xscale('log'); ax.set_xticks([5,10,20,40,80,160,320]); ax.set_xticklabels(['5','10','20','40','80','160','320'])
ax.set_xlabel('Comandos enviados en ráfaga (sin esperar respuesta)'); ax.set_ylabel('Comandos perdidos (%)'); ax.set_ylim(-3,80)
ax.legend(fontsize=6,frameon=False,loc='upper left')
fig.tight_layout(); fig.savefig(f'{F}/fig_rafagas.png'); plt.close()
S['burst']=bu.to_dict('records'); S['burst256']=br.to_dict('records')
# ---------- Validación Entrega 1
def loops(f): return [int(m) for m in re.findall(r'loops_por_s,(\d+)',open(f).read())][1:]
def ar(f): return float(re.search(r'analogRead,ciclos_prom=([\d.]+)',open(f).read()).group(1))
V=D+'/validacion'
wk=loops(f'{V}/wokwi_validacion_PULLUP.txt'); sl=sum([loops(f'{V}/simavr_LTO_PULLUP_r{r}.txt') for r in (1,2,3)],[]); sn=sum([loops(f'{V}/sin_lto/simavr_INPUT_PULLUP_d-1_r{r}.txt') for r in (1,2,3)],[])
S['val']=dict(wokwi_loops=np.mean(wk),simavr_lto_loops=np.mean(sl),simavr_nolto_loops=np.mean(sn),
  wokwi_ar=ar(f'{V}/wokwi_validacion_PULLUP.txt'),simavr_lto_ar=ar(f'{V}/simavr_LTO_PULLUP_r1.txt'),simavr_nolto_ar=ar(f'{V}/sin_lto/simavr_INPUT_PULLUP_d-1_r1.txt'))
for f,k in [('e1_lto_serial.csv','e1_lto'),('e1_nolto_serial.csv','e1_nolto')]:
    d=pd.read_csv(f'{V}/{f}'); S['val'][k]=d.loops.mean()*4
# H1
rows=[]
for M,ML in [('INPUT_PULLUP','PULLUP'),('INPUT','INPUT')]:
    for d2 in [-1,0,1]:
        fs=[float(re.search(r'factor_tras_aplicar,([\d.]+)',open(f'{V}/simavr_LTO_{ML}_d{d2}_r{r}.txt').read()).group(1)) for r in (1,2,3)]
        lp=sum([loops(f'{V}/simavr_LTO_{ML}_d{d2}_r{r}.txt') for r in (1,2,3)],[])
        rows.append(dict(modo=M,d2={-1:'sin conexión',0:'0 externo',1:'1 externo'}[d2],factor=np.mean(fs),factor_sd=np.std(fs),loops=np.mean(lp)))
h1=pd.DataFrame(rows); h1.to_csv(f'{V}/h1_resumen.csv',index=False); S['h1']=rows
# Fig validación (dos paneles)
fig,ax=plt.subplots(2,1,figsize=(3.5,3.6))
lab=['simavr\n(sin LTO)','simavr\n(con LTO)','Wokwi']
vals=[S['val']['e1_nolto'],S['val']['e1_lto'],6422]
bars=ax[0].bar(lab,vals,color=['#c3c2bd',C1,C2],width=0.55)
for b_,v in zip(bars,vals): ax[0].text(b_.get_x()+b_.get_width()/2,v+150,f'{v:,.0f}'.replace(',',' '),ha='center',fontsize=6.5)
ax[0].set_ylabel('Iteraciones/s\n(sketch Entrega 1)'); ax[0].set_ylim(0,7400); ax[0].grid(axis='x')
lab2=[('PU' if r['modo']=='INPUT_PULLUP' else 'IN')+'\n'+{'sin conexión':'libre','0 externo':'0 ext.','1 externo':'1 ext.'}[r['d2']] for r in rows]
cols=[C2 if '0 ext' in r['d2'] else C1 for r in rows]
bb=ax[1].bar(range(len(rows)),[r['factor'] for r in rows],yerr=[r['factor_sd'] for r in rows],color=cols,width=0.6,error_kw=dict(lw=0.6,capsize=2))
ax[1].set_xticks(range(len(rows))); ax[1].set_xticklabels(lab2,fontsize=6.5); ax[1].set_ylabel('Velocidad de simavr\n(× tiempo real)'); ax[1].grid(axis='x')
fig.tight_layout(h_pad=0.8); fig.savefig(f'{F}/fig_validacion.png'); plt.close()
json.dump(S,open(f'{D}/resumen_e2.json','w'),indent=1,default=float)
print(json.dumps({k:S[k] for k in ['wokwi','comp','val']},indent=1,default=float)); print(h1)
