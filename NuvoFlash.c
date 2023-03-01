//!gcc -Wall -I . -L . "%file%" -o "%name%" -lserialport
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <libserialport.h>

#define PAGE_SIZE 128
#define MAX_PATH 260

typedef enum {APROM,LDROM,CONFIG} Mem;

Mem mem;
struct sp_port *port;
uint8_t buf[PAGE_SIZE];
char filename[MAX_PATH];
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
  if (strcmp(s,"APROM")==0)
    return APROM;
  else if (strcmp(s,"LDROM")==0)
    return LDROM;
  else if (strcmp(s,"CONFIG")==0)
    return CONFIG;
  else {
    fprintf(stderr,"Invalid value '%s', must be APROM, LDROM or CONFIG\n",s);
    usage();
  }
  return -1;
}

bool selectSerialPort(size_t len,char portName[len]) {
  struct sp_port **port_list;
  enum sp_return res=sp_list_ports(&port_list);

  if (res!=SP_OK) return false;
  for (int i = 0; port_list[i] != NULL; i++) {
    int vid,pid;
    res=sp_get_port_usb_vid_pid(port_list[i],&vid,&pid);
    if (res!=SP_OK) continue;
    if (pid==0xc550 && vid==0x1209) {
      strncpy(portName,sp_get_port_name(port_list[i]),len);
      fprintf(stderr,"Serial port automatically selected (%s)\n",
	sp_get_port_description(port_list[i]));
      return true;
    }
  }
  sp_free_port_list(port_list);
  return false;
}

bool readBlock(uint8_t mem,int address,size_t len,uint8_t buf[len]) {
  uint8_t err, cmd[4]={'R',mem,address>>8,address};
  
  sp_blocking_write(port,cmd,mem=='C'?2:4,500);
  int nBytesRead=sp_blocking_read(port,&err,1,500);
  if (nBytesRead!=1) {
    fprintf(stderr,"Programmer is not responding\n");
    return false;
  }
  if (err!=0) {
    fprintf(stderr,"Programmer returned error code %d\n",err);
    return false;
  }
  nBytesRead=sp_blocking_read(port,buf,len,500);
  if (nBytesRead!=len) {
    fprintf(stderr,"Programmer is not responding\n");
    return false;
  }
  return true;
}

bool writeBlock(uint8_t mem,int address,size_t len,const uint8_t buf[len]) {
  uint8_t err, cmd[4]={'W',mem,address>>8,address};

  sp_blocking_write(port,cmd,mem=='C'?2:4,500);
  sp_blocking_write(port,buf,len,500);
  int nBytesRead=sp_blocking_read(port,&err,1,500);
  if (nBytesRead!=1) {
    fprintf(stderr,"Programmer is not responding\n");
    return false;
  }
  if (err!=0) {
    fprintf(stderr,"Programmer returned error code %d\n",err);
    return false;
  }
  return true;
}

void readROM(const char *filename,uint8_t mem) {
  FILE *f=fopen(filename,"wb");
  if (f==NULL) {
    fprintf(stderr,"Cannot write to file %s\n",filename);
    exit(1);
  }
  for (int i=0;i<18*1024;i+=PAGE_SIZE) {
    if (!readBlock(mem,i,PAGE_SIZE,buf))
      break;
    fwrite(buf,1,PAGE_SIZE,f);
  }
  fclose(f);
}

void writeROM(const char *filename,uint8_t mem) {
  FILE *f=fopen(filename,"rb");
  if (f==NULL) {
    fprintf(stderr,"Cannot read from file %s\n",filename);
    exit(1);
  }
  for (int i=0;i<18*1024;i+=PAGE_SIZE) {
    int n=fread(buf,1,PAGE_SIZE,f);
    if (n==0) break;
    if (!writeBlock(mem,i,PAGE_SIZE,buf))
      break;
    if (n!=PAGE_SIZE) break;
  }
  fclose(f);
}

void readAPROM(const char *filename) {
  readROM(filename,'A');
}

void readLDROM(const char *filename) {
  readROM(filename,'L');
}

void writeAPROM(const char *filename) {
  writeROM(filename,'A');
}

void writeLDROM(const char *filename) {
  writeROM(filename,'L');
}

void readConfig(uint8_t cfg[]) {
  if (!readBlock('C',0,5,buf)) return;
  for (int i=0;i<5;i++) 
    printf("%02X",buf[i]);
  putchar('\n');
}

void writeConfig(const uint8_t cfg[]) {
  //~ for (int i=0;i<5;i++)
    //~ printf("%02X ",cfg[i]);
  //~ putchar('\n');
  writeBlock('C',0,5,cfg);
}

void massErase() {
  uint8_t err;
  
  sp_blocking_write(port,"X",1,500);
  
  if (sp_blocking_read(port,&err,1,500)!=1) {
    fprintf(stderr,"Programmer is not responding\n");
    exit(1);
  }
  if (err!=0) {
    fprintf(stderr,"Programmer returned error code %d\n",err);
    exit(1);
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
  else if ((readOpt && (writeOpt || massEraseOpt)) || (writeOpt && massEraseOpt)) {
    fputs("Only one of read, write, erase is allowed\n",stderr);
    usage();
  }
  
  if (!massEraseOpt && (mem!=CONFIG || writeOpt) && argc!=1 && optind!=argc-1) {
    fputs("Missing arguments\n",stderr);
    usage();
  }
  
  if (!portOpt) {
    if (!selectSerialPort(sizeof portName-1,portName)) {
      fputs("Serial port not found, try using the -p/--port option\n",stderr);
      exit(1);
    }
  }
  
  enum sp_return res=sp_get_port_by_name(portName,&port);
  if (res!=SP_OK) {
    fputs("The serial port does not exist, exiting\n",stderr);
    exit(1);
  }
  res=sp_open(port,SP_MODE_READ_WRITE);
  if (res!=SP_OK) {
    fputs("Cannot open serial port, exiting\n",stderr);
    exit(1);
  }
  res=sp_set_baudrate(port, 115200);
  if (res!=SP_OK) {
    fputs("Cannot set baud rate, exiting\n",stderr);
    exit(1);
  }

  char *p,*end;
  unsigned long long l=0;
  if (readOpt) 
    switch (mem) {
      case CONFIG:
	readConfig(buf);
	break;
      case APROM:
	readAPROM(argv[argc-1]);
	break;
      case LDROM:
	readLDROM(argv[argc-1]);
    }
    else if (writeOpt) 
      switch (mem) {
	case CONFIG:
	  p=argv[argc-1];
	  if (strlen(p)!=10) {
	    fprintf(stderr,"config bytes string length wrong (%llu insted of 10)\n",strlen(p));
	    exit(1);
	  }
	  l=strtoull(p,&end,16);
	  if (*end!=0) {
	    fprintf(stderr,"config bytes string contains invalid characters\n");
	    exit(1);
	  }
	  for (int i=4;i>=0;i--) {
	    buf[i]=l;
	    l>>=8;
	  }
	  writeConfig(buf);
	  break;
	case APROM:
	  writeAPROM(argv[argc-1]);
	  break;
	case LDROM:
	  writeLDROM(argv[argc-1]);
      }
    else if (massEraseOpt)
      massErase();
    
  sp_close(port);
  return 0;
}