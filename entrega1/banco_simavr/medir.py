# Uso: FIRMWARE=ruta/al/blink_5s_2e.elf python3 medir.py <modo 1=tiempo real|0=max> <seg_simulados> <salida.csv>
import subprocess, time, os, sys, csv
HZ=os.sysconf('SC_CLK_TCK'); PAGE=os.sysconf('SC_PAGE_SIZE')
def sys_cpu():
    v=list(map(int,open('/proc/stat').readline().split()[1:])); idle=v[3]+v[4]; return sum(v), idle
def mem_used_mb():
    d={l.split(':')[0]:int(l.split()[1]) for l in open('/proc/meminfo')}
    return (d['MemTotal']-d['MemAvailable'])/1024
def proc(pid):
    try:
        s=open(f'/proc/{pid}/stat').read().rsplit(')',1)[1].split()
        ut,st=int(s[11]),int(s[12])
        rss=[int(l.split()[1]) for l in open(f'/proc/{pid}/status') if l.startswith('VmRSS')][0]/1024
        return (ut+st)/HZ, rss
    except Exception: return None
mode=sys.argv[1]; dur=float(sys.argv[2]); out=sys.argv[3]
rows=[]; t0=time.time(); T,I=sys_cpu()
# linea base 10 s
while time.time()-t0<10:
    time.sleep(1); T2,I2=sys_cpu(); rows.append([round(time.time()-t0,1),'base','',round(100*(1-(I2-I)/(T2-T)),1),'',round(mem_used_mb(),1)]); T,I=T2,I2
p=subprocess.Popen(['./harness',os.environ.get('FIRMWARE','blink_5s_2e.elf'),str(dur),mode,f'serial_{ "rt" if mode=="1" else "max"}.csv'],stderr=open(f'harness_{mode}.log','w'),stdout=subprocess.DEVNULL)
top=subprocess.Popen(['top','-b','-d','1','-n',str(int(dur)+5),'-p',str(p.pid)],stdout=open(f'top_{mode}.txt','w'))
vm=subprocess.Popen(['vmstat','1',str(int(dur)+5)],stdout=open(f'vmstat_{mode}.txt','w'))
last=proc(p.pid)
while p.poll() is None:
    time.sleep(1); cur=proc(p.pid); T2,I2=sys_cpu()
    if cur and last:
        rows.append([round(time.time()-t0,1),'sim',round(100*(cur[0]-last[0]),1),round(100*(1-(I2-I)/(T2-T)),1),round(cur[1],2),round(mem_used_mb(),1)])
    last=cur; T,I=T2,I2
t1=time.time()
while time.time()-t1<10:
    time.sleep(1); T2,I2=sys_cpu(); rows.append([round(time.time()-t0,1),'post','',round(100*(1-(I2-I)/(T2-T)),1),'',round(mem_used_mb(),1)]); T,I=T2,I2
top.terminate(); vm.terminate()
w=csv.writer(open(out,'w')); w.writerow(['t_s','fase','cpu_proc_pct','cpu_sistema_pct','rss_proc_mb','mem_sistema_mb']); w.writerows(rows)
print(open(f'harness_{mode}.log').read())
