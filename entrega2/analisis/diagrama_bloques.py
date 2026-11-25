# Diagrama de bloques: sistema real y su correspondencia en la simulación
import os, matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch
plt.rcParams.update({'font.family':'DejaVu Sans','savefig.dpi':300})
fig,ax=plt.subplots(figsize=(3.5,2.55)); ax.set_xlim(0,10); ax.set_ylim(0,7.4); ax.axis('off')
def box(x,y,w,h,t,sub,fc):
    ax.add_patch(FancyBboxPatch((x,y),w,h,boxstyle='round,pad=0.02,rounding_size=0.18',fc=fc,ec='#52514e',lw=0.8))
    ax.text(x+w/2,y+h*0.62,t,ha='center',va='center',fontsize=6.6,weight='bold',color='#0b0b0b')
    ax.text(x+w/2,y+h*0.27,sub,ha='center',va='center',fontsize=5.3,color='#52514e')
def arrow(x1,y1,x2,y2,t,dy=0.25,dash=False):
    ax.annotate('',xy=(x2,y2),xytext=(x1,y1),arrowprops=dict(arrowstyle='<->',lw=0.9,color='#2a78d6',ls='--' if dash else '-'))
    ax.text((x1+x2)/2,(y1+y2)/2+dy,t,ha='center',fontsize=5.2,color='#2a78d6')
ax.text(0.1,7.05,'Sistema físico',fontsize=6.8,weight='bold',color='#0b0b0b')
box(0.1,4.4,2.6,2.2,'Celular','app terminal\nBluetooth (SPP)','#eef4fc')
box(3.7,4.4,2.6,2.2,'HC-05','BT 2.0+EDR\nUART 9600 8N1','#fdf0e9')
box(7.3,4.4,2.6,2.2,'Arduino UNO','ATmega328P\n+ 2 LED (D5, D6)','#e8f6f0')
arrow(2.7,5.5,3.7,5.5,'RF',dy=-0.5,dash=True); arrow(6.3,5.5,7.3,5.5,'UART',dy=-0.5)
ax.text(0.1,3.55,'Simulación en Wokwi',fontsize=6.8,weight='bold',color='#0b0b0b')
box(0.1,0.9,2.6,2.2,'Monitor serie','hace de la app\ndel celular','#eef4fc')
box(3.7,0.9,2.6,2.2,'Chip hc05','modelo propio en C\n(Chips API)','#fdf0e9')
box(7.3,0.9,2.6,2.2,'Arduino UNO','emulado, mismo\nfirmware','#e8f6f0')
arrow(2.7,2.0,3.7,2.0,'UART',dy=-0.5); arrow(6.3,2.0,7.3,2.0,'UART',dy=-0.5)
ax.text(5,0.25,'La capa de radio (RF) no se simula: el enlace se modela a nivel de datos (SPP).',ha='center',fontsize=5.2,color='#52514e',style='italic')
fig.tight_layout(pad=0.1); fig.savefig(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),'figuras','fig_bloques.png')); print('ok')
