// Banco de pruebas local: ejecuta el firmware Arduino (ELF) en simavr (ATmega328P @16 MHz)
// e inyecta un perfil de sensores: A0 (tension analogica) y D2 (sensor digital).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <simavr/sim_avr.h>
#include <simavr/sim_elf.h>
#include <simavr/avr_adc.h>
#include <simavr/avr_ioport.h>
#include <simavr/avr_uart.h>

static FILE *fout; static char line[256]; static int li=0;
static void uart_cb(struct avr_irq_t *irq, uint32_t v, void *p){
  char c=(char)v; if(c=='\r') return;
  if(c=='\n'){ line[li]=0; fprintf(fout,"%s\n",line); fflush(fout); li=0; }
  else if(li<255) line[li++]=c;
}
static double now(){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec+t.tv_nsec*1e-9; }

// perfil de sensores en funcion del tiempo simulado (s)
static void perfil(double t, int *mv, int *d2){
  if(t<15){ *mv=(int)(5000*t/15); *d2=1; }            // potenciometro sube, sensor inactivo
  else if(t<30){ *mv=2500; *d2=0; }                    // sensor activo (alarma), valor medio
  else if(t<45){ *mv=(int)(5000*(45-t)/15); *d2=1; }   // potenciometro baja, sensor inactivo
  else { *mv=800; *d2=0; }                             // sensor activo, valor bajo
}

int main(int argc,char**argv){
  const char* elf=argv[1]; double dur=atof(argv[2]); int realtime=atoi(argv[3]); fout=fopen(argv[4],"w");
  elf_firmware_t fw; memset(&fw,0,sizeof fw); elf_read_firmware(elf,&fw);
  avr_t *avr=avr_make_mcu_by_name("atmega328p"); avr_init(avr); avr->frequency=16000000;
  avr->vcc=5000; avr->avcc=5000; avr->aref=5000; avr_load_firmware(avr,&fw);
  uint32_t f=0; avr_ioctl(avr,AVR_IOCTL_UART_GET_FLAGS('0'),&f); f&=~AVR_UART_FLAG_STDIO; avr_ioctl(avr,AVR_IOCTL_UART_SET_FLAGS('0'),&f);
  avr_irq_register_notify(avr_io_getirq(avr,AVR_IOCTL_UART_GETIRQ('0'),UART_IRQ_OUTPUT),uart_cb,NULL);
  avr_irq_t *adc=avr_io_getirq(avr,AVR_IOCTL_ADC_GETIRQ,ADC_IRQ_ADC0);
  avr_irq_t *pd2=avr_io_getirq(avr,AVR_IOCTL_IOPORT_GETIRQ('D'),2);
  double t0=now(); int lastmv=-1,lastd2=-1; uint64_t n=0;
  while(1){
    int st=avr_run(avr); if(st==cpu_Done||st==cpu_Crashed) break;
    if((++n & 0x3FF)==0){
      double ts=(double)avr->cycle/avr->frequency; if(ts>=dur) break;
      int mv,d2; perfil(ts,&mv,&d2);
      if(mv!=lastmv){ avr_raise_irq(adc,mv); lastmv=mv; }
      if(d2!=lastd2){ avr_raise_irq(pd2,d2); lastd2=d2; }
      if(realtime){ double ahead=ts-(now()-t0); if(ahead>0.002) usleep((useconds_t)(ahead*1e6)); }
    }
  }
  double wall=now()-t0, ts=(double)avr->cycle/avr->frequency;
  fprintf(stderr,"sim_s=%.3f wall_s=%.3f factor=%.2f cycles=%llu\n",ts,wall,ts/wall,(unsigned long long)avr->cycle);
  return 0;
}
