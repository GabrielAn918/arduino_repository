# Ejecutar desde la carpeta entrega1/:  python3 analisis/analisis_e1.py
import json, re, pandas as pd, numpy as np, matplotlib
matplotlib.use('Agg'); import matplotlib.pyplot as plt
plt.rcParams.update({'savefig.dpi':300,'font.family':'DejaVu Sans','font.size':8,'axes.edgecolor':'#9a9994','axes.labelcolor':'#0b0b0b','xtick.color':'#52514e','ytick.color':'#52514e','axes.grid':True,'grid.color':'#e6e5e1','grid.linewidth':0.6,'axes.spines.top':False,'axes.spines.right':False,'figure.dpi':200})
B,O,A,Y,M='#2a78d6','#eb6834','#1baf7a','#eda100','#e87ba4'
S={}
# ---------- Wokwi serial
rows=[r.split(',') for r in open('datos/crudos/wokwi_serial.txt').read().strip().split(';')]
w=pd.DataFrame(rows,columns=['t_ms','pot','sensor','periodo','L1','L2','L3','L4','L5','loops']).astype(int)
w['t']=w.t_ms/1000; w.to_csv('datos/wokwi_serial.csv',index=False)
S['w_n']=len(w); S['w_dur']=w.t.max()
S['w_loops_mean']=w.loops.mean(); S['w_loops_std']=w.loops.std()
S['w_loops_s']=w.loops.mean()*4
S['w_alarma_pct']=100*w.sensor.mean()
# expected period check
w['per_calc']=(w.pot*(1000-100)/1023+100).astype(int)
S['w_per_ok']=float((abs(w.per_calc-w.periodo)<=1).mean()*100)
# in BARRIDO exactly one LED on? in ALARMA all equal
b=w[w.sensor==0]; a=w[w.sensor==1]
S['barrido_un_led']=float((b[['L1','L2','L3','L4','L5']].sum(axis=1)<=1).mean()*100)
S['alarma_iguales']=float((a[['L1','L2','L3','L4','L5']].nunique(axis=1)==1).mean()*100)
S['on_pct']={k:float(w[k].mean()*100) for k in ['L1','L2','L3','L4','L5']}
S['loops_por_modo']={'BARRIDO':float(b.loops.mean()*4),'ALARMA':float(a.loops.mean()*4)}
# ---------- Fig 3: variables Wokwi
fig,ax=plt.subplots(3,1,figsize=(3.5,3.9),sharex=True,gridspec_kw={'height_ratios':[1.3,0.6,1.3]})
ax[0].plot(w.t,w.pot,color=B,lw=1.4); ax[0].set_ylabel('A0 (ADC)'); ax[0].set_ylim(-40,1100)
ax0b=ax[0]; 
ax[1].step(w.t,w.sensor,where='post',color=O,lw=1.4); ax[1].set_yticks([0,1]); ax[1].set_yticklabels(['Inactivo','Activo']); ax[1].set_ylim(-0.2,1.3); ax[1].set_ylabel('D2')
img=w[['L1','L2','L3','L4','L5']].T.values
ax[2].imshow(img,aspect='auto',cmap=matplotlib.colors.ListedColormap(['#f1f0ec','#2a78d6']),extent=[w.t.min()-0.125,w.t.max()+0.125,5.5,0.5],interpolation='nearest')
ax[2].set_yticks([1,2,3,4,5]); ax[2].set_yticklabels(['L1','L2','L3','L4','L5']); ax[2].grid(False); ax[2].set_ylabel('Salidas')
ax[2].set_xlabel('Tiempo de simulación (s)')
for t,lab in [(8,'Alarma'),(52,'Barrido'),(86,'Alarma'),(106,'Barrido')]:
    ax[1].text(t,1.15 if lab=='Alarma' else 0.15,lab,ha='center',va='bottom' if lab=='Alarma' else 'bottom',fontsize=6.5,color='#52514e')
fig.align_ylabels(); fig.tight_layout(h_pad=0.4); fig.savefig('figuras/fig_variables_wokwi.png'); plt.close()
# ---------- Wokwi perf
p=pd.DataFrame(json.load(open('datos/crudos/wokwi_perf.json')),columns=['t','sim','vel','heap','fps'])
def ts(s): m,x=s.split(':'); return int(m)*60+float(x)
p['sim_s']=p.sim.map(ts); p.to_csv('datos/wokwi_perf.csv',index=False)
S['vel_mean']=p.vel.mean(); S['vel_min']=p.vel.min(); S['fps_mean']=p.fps.mean(); S['heap_mean']=p.heap.mean(); S['heap_min']=p.heap.min(); S['heap_max']=p.heap.max()
fit=np.polyfit(p.t,p.sim_s,1); S['rt_factor_wokwi']=fit[0]
# ---------- Local
rt=pd.read_csv('datos/recursos_rt.csv'); mx=pd.read_csv('datos/recursos_max.csv')
def top_series(f):
    v=[]; 
    for l in open(f):
        if 'harness' in l: s=l.split(); v.append((float(s[8]),int(s[5])/1024))
    return pd.DataFrame(v,columns=['cpu','res'])
tp=top_series('datos/crudos/top_1.txt'); tm=top_series('datos/crudos/top_0.txt')
def vm(f,n):
    L=[l.split() for l in open(f).readlines()[3:3+n] if len(l.split())>=15 and l.split()[0].isdigit()]; return pd.DataFrame([[int(x[12]),int(x[13]),int(x[14])] for x in L],columns=['us','sy','id'])
v1=vm('datos/crudos/vmstat_1.txt',60); v0=vm('datos/crudos/vmstat_0.txt',160)
sim=rt[rt.fase=='sim']; simx=mx[mx.fase=='sim']
S['loc']={'proc_cpu_rt':sim.cpu_proc_pct.mean(),'proc_cpu_rt_max':sim.cpu_proc_pct.max(),'top_cpu_rt':tp.cpu[1:61].mean(),'vm_us_rt':v1.us.mean(),'vm_id_rt':v1.id.mean(),
 'sys_cpu_rt':sim.cpu_sistema_pct.mean(),'base_cpu':rt[rt.fase=='base'].cpu_sistema_pct.mean(),'rss_rt':sim.rss_proc_mb.mean(),'top_res':tp.res.mean(),
 'mem_base':rt[rt.fase=='base'].mem_sistema_mb.mean(),'mem_sim':sim.mem_sistema_mb.mean(),
 'proc_cpu_max':simx.cpu_proc_pct.mean(),'top_cpu_max':tm.cpu[1:].mean(),'vm_us_max':v0.us.mean(),'sys_cpu_max':simx.cpu_sistema_pct.mean(),'rss_max':simx.rss_proc_mb.mean()}
for f,k in [('datos/crudos/harness_1.log','rt'),('datos/crudos/harness_0.log','max')]:
    m=re.search(r'sim_s=([\d.]+) wall_s=([\d.]+) factor=([\d.]+) cycles=(\d+)',open(f).read()); S['loc'][k+'_factor']=float(m.group(3)); S['loc'][k+'_wall']=float(m.group(2)); S['loc'][k+'_sim']=float(m.group(1))
ls=pd.read_csv('datos/serial_rt.csv'); S['loc']['loops_s']=ls.loops.mean()*4; S['loc']['n']=len(ls)
S['loc']['per_ok']=float((abs((ls.pot*(900)/1023+100).astype(int)-ls.periodo_ms)<=1).mean()*100)
# Fig 4: CPU comparison (two panels: CPU, memory)
fig,ax=plt.subplots(2,1,figsize=(3.5,3.3),sharex=True,gridspec_kw={'height_ratios':[1.5,1]})
t=rt.t_s
ax[0].plot(sim.t_s,sim.cpu_proc_pct,color=B,lw=1.4,label='Proceso (/proc/[pid]/stat)')
tt=sim.t_s.values[:len(tp.cpu[1:61])]
ax[0].plot(tt,tp.cpu[1:1+len(tt)].values,color=O,lw=1.2,ls='--',label='top (%CPU del proceso)')
ax[0].plot(rt.t_s,rt.cpu_sistema_pct,color=A,lw=1.2,label='Sistema total (/proc/stat)')
ax[0].set_ylabel('CPU (%)'); ax[0].set_ylim(0,100); ax[0].legend(fontsize=6,frameon=False,loc='upper center',ncol=2,bbox_to_anchor=(0.5,1.0),columnspacing=0.8,handlelength=1.6)
for a0,a1 in [(25,40),(55,70)]: ax[0].axvspan(a0,a1,color='#fbe3d8',lw=0,zorder=0)
ax[0].text(32.5,4,'D2 activo',ha='center',fontsize=6,color='#52514e'); ax[0].text(62.5,4,'D2 activo',ha='center',fontsize=6,color='#52514e')
ax[0].axvspan(0,10,color='#f1f0ec',lw=0); ax[0].axvspan(sim.t_s.max()+0.5,rt.t_s.max(),color='#f1f0ec',lw=0)
ax[0].text(5,72,'Reposo',ha='center',fontsize=6,color='#52514e'); ax[0].text(75,72,'Reposo',ha='center',fontsize=6,color='#52514e')
ax[1].plot(rt.t_s,rt.mem_sistema_mb,color=A,lw=1.4,label='Memoria usada del sistema')
ax[1].set_ylabel('Memoria (MB)'); ax[1].set_xlabel('Tiempo de medición (s)')
ax[1].text(rt.t_s.max(),rt.mem_sistema_mb.max()+0.4,f"RSS del simulador ≈ {sim.rss_proc_mb.mean():.1f} MB",ha='right',fontsize=6.5,color='#52514e')
ax[1].set_ylim(rt.mem_sistema_mb.min()-2,rt.mem_sistema_mb.max()+2)
fig.align_ylabels(); fig.tight_layout(h_pad=0.4); fig.savefig('figuras/fig_recursos_local.png'); plt.close()
# Fig 5: Wokwi browser indicators: speed% and FPS & heap -> two panels
fig,ax=plt.subplots(2,1,figsize=(3.5,2.9),sharex=True)
ax[0].plot(p.t,p.vel,color=B,lw=1.4); ax[0].set_ylabel('Velocidad (%)'); ax[0].set_ylim(70,105)
i=p.vel.idxmin(); ax[0].annotate(f'{p.vel[i]:.0f} %',(p.t[i],p.vel[i]),xytext=(p.t[i]+8,p.vel[i]+2),fontsize=6.5,color='#52514e',arrowprops=dict(arrowstyle='-',color='#9a9994',lw=0.6))
ax[1].plot(p.t,p.heap,color=O,lw=1.4); ax[1].set_ylabel('Heap JS (MB)'); ax[1].set_xlabel('Tiempo real (s)')
fig.align_ylabels(); fig.tight_layout(h_pad=0.4); fig.savefig('figuras/fig_wokwi_navegador.png'); plt.close()
sim2=sim.copy(); sim2['ts']=sim2.t_s-10
S['loc']['cpu_barrido']=sim2[((sim2.ts>1)&(sim2.ts<14))|((sim2.ts>31)&(sim2.ts<44))].cpu_proc_pct.mean()
S['loc']['cpu_alarma']=sim2[((sim2.ts>16)&(sim2.ts<29))|((sim2.ts>46)&(sim2.ts<59))].cpu_proc_pct.mean()
json.dump(S,open('datos/resumen_e1.json','w'),indent=1,default=float); print(json.dumps(S,indent=1,default=float))
