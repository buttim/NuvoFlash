#include "ch554.h"
#include <stdbool.h>
#include <stdint.h>

__sbit __at(0x90+4) P14;

__sbit __at(0xB0+3) P33;
__sbit __at(0x96+3) P33_MOD_OC;
__sbit __at(0x97+3) P33_MOD_PU;

__sbit __at(0xB0+4) P34;
__sbit __at(0xB0+5) P35;

#define dat_line GPIO_DAT
#define rst_line GPIO_RST
#define clk_line GPIO_CLK

#define GPIO_DAT	33//20
#define GPIO_CLK	34//26
#define GPIO_RST	35//21
#define TRIGGER   13

#define N76E003_DEVID	0x3650

#define FLASH_SIZE	(18 * 1024)
#define LDROM_MAX_SIZE	(4 * 1024)

#define APROM_FLASH_ADDR	0x0
#define CFG_FLASH_ADDR		0x30000UL
#define CFG_FLASH_LEN		5

#define CMD_READ_UID		0x04
#define CMD_READ_CID		0x0b
#define CMD_READ_DEVICE_ID	0x0c
#define CMD_READ_FLASH		0x00
#define CMD_WRITE_FLASH		0x21
#define CMD_MASS_ERASE		0x26
#define CMD_PAGE_ERASE		0x22

#define SLOW 52
#define FAST 0

int clkDelay=SLOW;

#define usleep(x) delayMicroseconds(x)
#define pgm_get_dat() (P33)//((P3&0x8)!=0)//digitalRead(GPIO_DAT)//
#define pgm_set_rst(val) {P35=(val);}//if (val) P3|=0x20; else P3&=~0x20;//digitalWrite(GPIO_RST,val)//
#define pgm_set_dat(val) {P33=(val);}//if (val) P3|=0x8; else P3&=~0x8;//digitalWrite(GPIO_DAT,val)//
#define pgm_set_clk(val) {P34=(val); if (clkDelay>0) usleep(clkDelay);}// {if (val) P3|=0x10; else P3&=~0x10; if (clkDelay>0) usleep(clkDelay); }//digitalWrite(GPIO_CLK,val)//
#define pgm_dat_dir(val) {if (val) {P3_MOD_OC|=0x8;P3_DIR_PU|=0x8;} else {P3_MOD_OC&=~0x8;P3_DIR_PU&=~0x8;}}//pinMode(GPIO_DAT,val?OUTPUT:INPUT)//
#define pgm_deinit() pgm_set_rst(1)
#define fprintf(a,...) USBSerial_print(__VA_ARGS__)

void icp_bitsend(__xdata uint32_t data, __xdata int len)
{
	/* configure DAT pin as output */
	pgm_dat_dir(1);

  uint32_t mask=1UL<<(len-1);
	while (mask) {
		pgm_set_dat((data & mask)!=0);
		pgm_set_clk(1);
    mask>>=1;
		pgm_set_clk(0);
	}
}

void icp_send_command(__xdata uint8_t cmd, __xdata uint32_t dat)
{
	icp_bitsend((dat << 6) | cmd, 24);
}

void icp_init(void)
{
	uint32_t icp_seq = 0xae1cb6;
	int i = 24;

	while (i--) {
		pgm_set_rst((icp_seq >> i) & 1);
		usleep(10000);
	}

	usleep(100);

	icp_bitsend(0x5aa503, 24);
}

void icp_exit(void)
{
	pgm_set_rst(1);
	usleep(5000);
	pgm_set_rst(0);
	usleep(10000);
	icp_bitsend(0xf78f0, 24);
	usleep(500);
	pgm_set_rst(1);
}

uint8_t icp_read_byte(__xdata int end)
{
	pgm_dat_dir(0);

	uint8_t data = 0;
	int i = 8;

	while (i--) {
	  data<<=1;
		data |= pgm_get_dat();
		pgm_set_clk(1);
		pgm_set_clk(0);
	}

	pgm_dat_dir(1);
	pgm_set_dat(end);
	pgm_set_clk(1);
	pgm_set_clk(0);

	return data;
}

void icp_write_byte(__xdata uint8_t data, __xdata int end, __xdata int delay1, __xdata int delay2)
{
	icp_bitsend(data, 8);
	pgm_set_dat(end);
	usleep(delay1);
	pgm_set_clk(1);
	usleep(delay2);
	pgm_set_dat(0);
	pgm_set_clk(0);
}

uint16_t icp_read_device_id(void)
{
	icp_send_command(CMD_READ_DEVICE_ID, 0);

	uint8_t devid[2];
	devid[0] = icp_read_byte(0);
	devid[1] = icp_read_byte(1);

	return (devid[1] << 8) | devid[0];
}

uint8_t icp_read_cid(void)
{
	icp_send_command(CMD_READ_CID, 0);
	return icp_read_byte(1);
}

uint32_t icp_read_uid(void)
{
	uint8_t uid[3];

	for (int i = 0; i < sizeof(uid); i++) {
		icp_send_command(CMD_READ_UID, i);
		uid[i] = icp_read_byte(1);
	}

	return ((uint32_t)uid[2] << 16) | (uid[1] << 8) | uid[0];
}

uint32_t icp_read_ucid(void)
{
	uint8_t ucid[4];

	for (int i = 0; i < sizeof(ucid); i++) {
		icp_send_command(CMD_READ_UID, i + 0x20);
		ucid[i] = icp_read_byte(1);
	}

	return ((uint32_t)ucid[3] << 24) | ((uint32_t)ucid[2] << 16) | (ucid[1] << 8) | ucid[0];
}

uint32_t icp_read_flash(__xdata uint32_t addr, __xdata uint32_t len, __xdata uint8_t *__xdata data)
{
	icp_send_command(CMD_READ_FLASH, addr);

	for (int i = 0; i < len; i++)
		data[i] = icp_read_byte(i == (len-1));

	return addr + len;
}

uint32_t icp_write_flash(__xdata uint32_t addr, __xdata uint32_t len, __xdata uint8_t *__xdata data)
{
	int progress_printed = 0;
	icp_send_command(CMD_WRITE_FLASH, addr);

	for (int i = 0; i < len; i++) {
		icp_write_byte(data[i], i == (len-1), 200, 50);

		/* print some progress */
		if (((i % 256) == 0) && len > CFG_FLASH_LEN) {
			fprintf(stderr, ".");
			progress_printed++;
		}
	}

	if (progress_printed)
		fprintf(stderr, "\n");

	return addr + len;
}

void icp_dump_config()
{
	__xdata uint8_t cfg[CFG_FLASH_LEN];
	icp_read_flash(CFG_FLASH_ADDR, CFG_FLASH_LEN, cfg);

	//fprintf(stderr, "MCU Boot select:\t%s\n", cfg[0] & 0x80 ? "APROM" : "LDROM");
	USBSerial_print("MCU Boot select:\t");
  USBSerial_println(cfg[0] & 0x80 ? "APROM" : "LDROM");

	int ldrom_size = (7 - (cfg[1] & 0x7)) * 1024;
	fprintf(stderr, "LDROM size:\t\t%d Bytes\n", ldrom_size);
	fprintf(stderr, "APROM size:\t\t%d Bytes\n", FLASH_SIZE - ldrom_size);
}

void icp_mass_erase(void)
{
	icp_send_command(CMD_MASS_ERASE, 0x3A5A5);
	icp_write_byte(0xff, 1, 100000, 10000);
}

void icp_page_erase(__xdata uint32_t addr)
{
	icp_send_command(CMD_PAGE_ERASE, addr);
	icp_write_byte(0xff, 1, 10000, 1000);
}

void setup() {
  pinMode(14,OUTPUT);
  pinMode(TRIGGER,OUTPUT);
  P3_MOD_OC|=0x38;  //DAT RST and CLK output only
  P3_DIR_PU|=0x38;
  P3&=~0x38;        //DAT RST and CLCK low
}

void loop() {
  __xdata static uint8_t buf[32];

  P14=(millis()&0x3FF)<20;

  if (digitalRead(32)==HIGH) {
    if (!USBSerial_available()) return;
    if (USBSerial_read()!='g') {
      USBSerial_println("premi -> g <- per lanciare test");
      return;
    }
  }

  digitalWrite(TRIGGER,HIGH);
  clkDelay=SLOW;
  icp_init();
  clkDelay=FAST;

  usleep(120);

  uint16_t devid = icp_read_device_id();
  uint8_t cid = icp_read_cid();

  /*digitalWrite(TRIGGER,HIGH);
  icp_dump_config();
  digitalWrite(TRIGGER,LOW);*/

  //USBSerial_print("devid: 0x");
  //USBSerial_println(devid,HEX);

  //uint32_t uid = icp_read_uid();
  //uint32_t ucid = icp_read_ucid();
  //USBSerial_print("cid: 0x");
  //USBSerial_println(cid,HEX);
  //USBSerial_print("uid: 0x");
  //USBSerial_println(uid,HEX);
  //USBSerial_print("ucid: 0x");
  //USBSerial_println(ucid,HEX);

  usleep(260);
  icp_read_flash(CFG_FLASH_ADDR, CFG_FLASH_LEN, buf);
  usleep(760);

  icp_read_flash(0,sizeof buf,buf);

  icp_exit();
  pgm_deinit();
  digitalWrite(TRIGGER,LOW);

  USBSerial_print("devid: 0x");
  USBSerial_println(devid,HEX);
  USBSerial_print("cid: 0x");
  USBSerial_println(cid,HEX);

  for (int i=0;i<sizeof buf;i++) {
    if (buf[i]<16)
      USBSerial_print("0");
    USBSerial_print(buf[i],HEX);
    USBSerial_print(" ");
    if (i==15)
      USBSerial_print("\n");
  }
  USBSerial_print("\n");
  USBSerial_print("\n");
  while (digitalRead(32)==LOW);
}
