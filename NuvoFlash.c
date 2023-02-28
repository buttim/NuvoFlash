//!gcc -I . -L . "%file%" -o "%name%" -lserialport
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <libserialport.h>

typedef enum {APROM,LDROM,CONFIG} Mem;
Mem mem;

void usage() {
  fputs("Usage: nuvoflash <options> [file.bin|hex_value]\n",stderr);
  fputs("<mem> must be one of APROM, LDROM, CONFIG\n",stderr);
  fputs("file.bin must be specified with -r and -w and APROM/LDROM mem types\n",stderr);
  fputs("hex_value must be specified with the -w option and CONFIG mem type\n",stderr);
  fputs("  -r <mem>\tread <mem>\n",stderr);
  fputs("  -w <mem>\twrite <mem>\n",stderr);
  fputs("  -x\t\tmass erase all flash memory\n",stderr);
  fputs("  -p <port>\tserial port (if omitted it will be automatically selected)\n",stderr);
  exit(1);
}

Mem memFromString(const char *s) {
	if (strcmp(s,"APROM"))
	  mem=APROM;
	else if (strcmp(s,"LDROM"))
	  mem=LDROM;
	else if (strcmp(s,"CONFIG"))
	  mem=CONFIG;
	else {
	  fprintf(stderr,"Invalid value '%s', must be APROM, LDROM or CONFIG\n",s);
	  usage();
	}
}
  
int main(int argc, char *argv[]) {
  char opt;
  int opt_index;
  char portName[20]="";
  bool portOpt=false, readOpt=false, writeOpt=false, massEraseOpt=false;
  static struct option long_options[] =  {
    {"port",	required_argument,	NULL, 'p'},
    {"read",	required_argument,	NULL, 'r'},
    {"write",	required_argument,	NULL, 'w'},
    {"mass_erase",	no_argument,		NULL, 'x'},
    {0,0,0,0}
  };

  while ((opt = getopt_long(argc, argv, "p:r:w:x",long_options,&opt_index)) != -1) {
    switch (opt) {
      case 'p':
	strncpy(portName,optarg,sizeof portName-1);
	break;
      case 'r':
	readOpt=true;
	mem=memFromString(optarg);
	break;
      case 'w':
	writeOpt=true;
	mem=memFromString(optarg);
	break;
      case 'x':
	massEraseOpt=true;
	break;
      default:
	usage();
    }
  }
  
  if (!readOpt && !writeOpt && !massEraseOpt) {
    fputs("Exactly one of read, write, erase must be specified\n",stderr);
    usage();
  }
  else if (readOpt && (writeOpt || massEraseOpt) || writeOpt && massEraseOpt) {
    fputs("Only one of read, write, erase is allowed\n",stderr);
    usage();
  }
  
  if (argc!=1 && optind!=argc-1) {
    fputs("Too many arguments\n",stderr);
    usage();
  }
  
  puts(argv[argc-1]);
  
  struct sp_port **port_list;

  sp_list_ports(&port_list);
  for (int i = 0; port_list[i] != NULL; i++) {
    int vid,pid;
    sp_get_port_usb_vid_pid(port_list[i],&vid,&pid);
    printf("%s\t%04X:%04X %s\n",sp_get_port_name(port_list[i]),vid,pid,
      sp_get_port_description(port_list[i]));
  }
  sp_free_port_list(port_list);
  return 0;
  
  
  struct sp_port *port;
  enum sp_return res=sp_get_port_by_name("COM26",&port);
  printf("%d\n",res);
  
  int vid,pid;
  res=sp_get_port_usb_vid_pid(port,&vid,&pid);
  printf("%d vid=%08X pid=%08X %s\n",res,vid,pid,sp_get_port_description(port));
  
  res=sp_open(port,SP_MODE_READ_WRITE);
  printf("%d\n",res);
  res=sp_set_baudrate(port, 115200);
  printf("%d\n",res);
  
  unsigned char cmd[]="RA\0\0";
  for (int j=0;j<18*1024;j+=128) {
    cmd[2]=j/256;
    cmd[3]=j&0xFF;
    res=sp_blocking_write(port,cmd,4,500);
    //printf("%d\n",res);
    
    uint8_t buf[128];
    memset(buf,0,sizeof buf);
    res=sp_blocking_read(port,buf,128,500);
    //printf("%d\n",res);
    
    /*for (int i=0;i<128;i++) {
      printf("%02X ",buf[i]);
      if (i%16==15)
	putchar('\n');
    }*/
  }
  putchar('\n');
  return 0;
}