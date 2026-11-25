// Banco para validar las hipótesis H1 y H2 de la Entrega 1 en simavr.
// uso: valida_e1 firmware.elf seg_simulados d2(-1=sin conexión,0,1) salida.txt
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <simavr/sim_avr.h>
#include <simavr/sim_elf.h>
#include <simavr/avr_uart.h>
#include <simavr/avr_ioport.h>
static FILE *fo;
static void out_cb(struct avr_irq_t *i, uint32_t v, void *p){ (void)i;(void)p; fputc((int)v, fo); }
int main(int argc,char**argv){
  elf_firmware_t fw; memset(&fw,0,sizeof fw); elf_read_firmware(argv[1],&fw);
  double dur=atof(argv[2]); int d2=atoi(argv[3]); fo=fopen(argv[4],"w");
  avr_t *avr=avr_make_mcu_by_name("atmega328p"); avr_init(avr); avr->frequency=16000000;
  avr->vcc=5000; avr->avcc=5000; avr->aref=5000; avr_load_firmware(avr,&fw);
  uint32_t f=0; avr_ioctl(avr,AVR_IOCTL_UART_GET_FLAGS('0'),&f); f&=~AVR_UART_FLAG_STDIO; avr_ioctl(avr,AVR_IOCTL_UART_SET_FLAGS('0'),&f);
  avr_irq_register_notify(avr_io_getirq(avr,AVR_IOCTL_UART_GETIRQ('0'),UART_IRQ_OUTPUT),out_cb,NULL);
  double t_apply = argc>5 ? atof(argv[5]) : 0; int applied=0; avr_irq_t *pd2=avr_io_getirq(avr,AVR_IOCTL_IOPORT_GETIRQ('D'),2);
  if(d2>=0 && t_apply<=0){ avr_raise_irq(pd2,d2); applied=1; }
  struct timespec a,b; clock_gettime(CLOCK_MONOTONIC,&a);
  double tw=0; struct timespec c;
  while((double)avr->cycle/avr->frequency<dur){ int st=avr_run(avr); if(st==cpu_Done||st==cpu_Crashed)break;
    if(!applied && d2>=0 && (double)avr->cycle/avr->frequency>=t_apply){ avr_raise_irq(pd2,d2); applied=1; clock_gettime(CLOCK_MONOTONIC,&c); tw=(c.tv_sec-a.tv_sec)+(c.tv_nsec-a.tv_nsec)*1e-9; } }
  clock_gettime(CLOCK_MONOTONIC,&b);
  double wall=(b.tv_sec-a.tv_sec)+(b.tv_nsec-a.tv_nsec)*1e-9;
  fprintf(fo,"factor_tiempo_real,%.3f\n",dur/wall);
  if(t_apply>0) fprintf(fo,"factor_tras_aplicar,%.3f\n",(dur-t_apply)/(wall-tw)); fclose(fo); return 0;
}
