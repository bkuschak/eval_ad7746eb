// Example showing how to control the EVAL-AD7746EB board under Linux.
//
// Uses the device firmware for the EZ-USB FX2 provided by Analog Devices. 
// Sends I2C commands using Vendor Specific control transfers.
// We rely on fxload and udev to load the firmware.
//
// References:
// https://www.analog.com/media/en/technical-documentation/data-sheets/AD7745_7746.pdf
// https://www.analog.com/media/en/technical-documentation/evaluation-documentation/EVAL-AD7746EB.PDF
// https://ez.analog.com/data_converters/precision_adcs/w/documents/3398/can-you-send-me-the-ad7746-labview-source-code
//
// 6/30/2020 bkuschak@yahoo.com

#include <stdio.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <unistd.h>
#include "libusb.h"

#define ENABLE_TEMPERATURE	// Enabling temp reduces the output data rate.

#define EVAL_AD7746EB_VID 	0x0456
#define EVAL_AD7746EB_PID 	0xB481

#define AD7746_SLAVE_ADDR 	0x48

#define AD7746_REG_RESET	0xBF
#define AD7746_REG_STATUS	0x00
#define AD7746_REG_CAP_DATA_H	0x01
#define AD7746_REG_CAP_DATA_M	0x02
#define AD7746_REG_CAP_DATA_L	0x03
#define AD7746_REG_VT_DATA_H	0x04
#define AD7746_REG_VT_DATA_M	0x05
#define AD7746_REG_VT_DATA_L	0x06
#define AD7746_REG_CAP_SETUP	0x07
#define AD7746_REG_VT_SETUP	0x08
#define AD7746_REG_EXC_SETUP	0x09
#define AD7746_REG_CONFIG	0x0A
#define AD7746_REG_CAPDAC_A	0x0B
#define AD7746_REG_CAPDAC_B	0x0C

#define CTRL_IN		(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN)
#define CTRL_OUT	(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT)

// Supported vendor requests.
#define VR_IO		0xDB	// Read/write I/O ports config.
#define VR_I2C0		0xDC	// Simple I2C, without reg index.
#define VR_I2C1		0xDD	// Extended I2C, 8-bit reg index.
#define VR_I2C2		0xDE	// Extended I2C, 16-bit reg index.

static struct libusb_context *ctx;
static struct libusb_device_handle *devh;

// Use libusb-1.0 to communicate.
static int open_device() 
{
	libusb_init(&ctx);
	devh = libusb_open_device_with_vid_pid(ctx, EVAL_AD7746EB_VID, 
		EVAL_AD7746EB_PID);
	return devh ? 0 : -EIO;
}

// Read one or more registers starting from address reg_addr, into data. 
// Len = 1 min, 64 max.  Returns 0 if successful or <0 on error.
static int read_regs(uint8_t slave_addr, uint8_t reg_addr, uint8_t* data, 
	uint8_t len)
{
	int r;

	r = libusb_control_transfer(devh, CTRL_IN, VR_I2C1, slave_addr<<1, 
		reg_addr, data, len, 0);
	if (r < 0) {
		fprintf(stderr, "Failed to read register! Error %d\n", r);
		return r;
	}
	if ((unsigned int) r < len) {
		fprintf(stderr, "Short read (%d) while reading registers\n", r);
		return -1;
	}
	return 0;
}

// Write one or more registers starting from address reg_addr, with data. 
// Len = 1 min, 64 max.  Returns of zero if successful or <0 on error.
static int write_regs(uint8_t slave_addr, uint8_t reg_addr, uint8_t* data, 
	uint8_t len)
{
	int r;

	r = libusb_control_transfer(devh, CTRL_OUT, VR_I2C1, slave_addr<<1, 
		reg_addr, data, len, 0);
	if (r < 0) {
		fprintf(stderr, "Failed to write register! Error %d\n", r);
		return r;
	}
	if ((unsigned int) r < len) {
		fprintf(stderr, 
			"Short write (%d) while writing registers\n", r);
		return -1;
	}
	return 0;
}

static int read_reg(uint8_t slave_addr, uint8_t reg_addr) 
{	
	uint8_t data;
	int ret = read_regs(slave_addr, reg_addr, &data, 1);
	return ret < 0 ? ret : data;
}

static int write_reg(uint8_t slave_addr, uint8_t reg_addr, uint8_t data) 
{	
	return write_regs(slave_addr, reg_addr, &data, 1);
}

// Configure and write the I/O ports.
// Up to 3 ports may be configured. For each port A, B, D: direction[7:0] 
// is 0 for input, 1 for output. Value[7:0] is 0 or 1 for outputs, 0 for inputs.
// Len may be 1 to 3, to specify the number of ports to configure.
static int write_gpio(uint8_t direction[3], uint8_t value[3], int len)
{
	if(len > 3 || len < 0)
		return -EINVAL;

	int i, r;
	uint8_t data[2*len];

	for(i=0; i<len; i++) {
		data[2*i] = value[i];
		data[2*i+1] = direction[i];
	}

	r = libusb_control_transfer(devh, CTRL_OUT, VR_IO, 0, 0, data, 2*len, 
		0);
	if (r < 0) {
		fprintf(stderr, "Failed to configure I/O! Error %d\n", r);
		return r;
	}
	if ((unsigned int) r < len) {
		fprintf(stderr, 
			"Short write (%d) while configuring I/O\n", r);
		return -1;
	}
	return 0;
}

// Read the I/O ports.
// Up to 3 ports may be read. For each port A, B, D: direction[7:0] 
// is 0 for input, 1 for output. Value[7:0] is 0 or 1 for outputs, 0 for inputs.
// Len may be 1 to 3, to specify the number of ports to configure.
// direction and value should both be arrays of length len.  They are outputs.
static int read_gpio(uint8_t* direction, uint8_t* value, int len)
{

	if(len > 3 || len < 0)
		return -EINVAL;

	int i, r;
	uint8_t data[2*len];

	r = libusb_control_transfer(devh, CTRL_IN, VR_IO, 0, 0, data, 2*len, 
		0);
	if (r < 0) {
		fprintf(stderr, "Failed to configure I/O! Error %d\n", r);
		return r;
	}
	if ((unsigned int) r < len) {
		fprintf(stderr, 
			"Short write (%d) while configuring I/O\n", r);
		return -1;
	}

	for(i=0; i<len; i++) {
		value[i] = data[2*i];
		direction[i] = data[2*i+1];
	}
	return 0;
}

static int dump_regs() 
{
	int ret, i;
	uint8_t data[19];

	printf("AD7746 Register dump:\n");
	if((ret = read_regs(AD7746_SLAVE_ADDR, 0, data, sizeof(data))) < 0) {
		fprintf(stderr, "Failed reg dump!\n");
		return -1;
	}

	for (i = 0; i < sizeof(data); i++)
		printf("%02hx: %02hx\n", i, data[i]);

	return 0;
}

// This could presumably be done by polling the GPIO port connected to RDY#.
// We do it by polling the status register.
static int wait_for_ready(int timeout_msec) 
{
	struct timeval t0, t1;
	gettimeofday(&t0, NULL);

	while(1) {
		int status;
		if((status = read_reg(AD7746_SLAVE_ADDR, AD7746_REG_STATUS)) 
			< 0) {
			fprintf(stderr, "Failed to read status reg!\n");
			return -1;
		}

		// Wait for all enabled channels to be ready.
		if((status & 0x4) == 0) 
			return 0;

		if(status & 0x8) 
			fprintf(stderr, "Failed to drive EXC signal!\n");

		gettimeofday(&t1, NULL);
		long diff = (t1.tv_sec-t0.tv_sec)*1000000 + t1.tv_usec -
			t0.tv_usec;
		if(diff > 1000*timeout_msec) {
			fprintf(stderr, "Timeout waiting for ready!\n");
			return -1;
		}
		
		// Delay instead of constantly spinning, to reduce CPU load.
		// FIXME - make delay adjustable, depending on sample rate.
		usleep(10*1000);
	}
}

static float raw_to_capacitance(int raw)
{
	float scale = 8.192e-12 / (1<<24);
	return scale * (raw - 0x800000); 	// offset binary
}

static float raw_to_temperature(int raw)
{
	return (raw / 2048.0) - 4096.0;
}

// Return the raw register values for capacitance and/or temperature. cap or
// temp may be NULL to skip reading it.
static int get_data(int* cap, int* temp, int timeout_msec) 
{
	if(!cap && !temp)
		return 0;	// Nothing to do

	int ret;
	if((ret = wait_for_ready(timeout_msec)) != 0)
		return ret;

	// Read all the data in one chunk. Either cap, temp, or both.
	uint8_t data[6];
	uint8_t addr;
	int len;
	if(cap && temp) {
		addr = AD7746_REG_CAP_DATA_H;
		len = 6;
	}
	else if(cap && !temp) {
		addr = AD7746_REG_CAP_DATA_H;
		len = 3;
	}
	else {
		addr = AD7746_REG_VT_DATA_H;
		len = 3;
	}

	if(read_regs(AD7746_SLAVE_ADDR, addr, data, len) < 0) {
		fprintf(stderr, "Failed to read data registers!\n");
		return -1;
	}
	if(cap) {
		*cap = data[0];  *cap <<= 8;
		*cap |= data[1];  *cap <<= 8;
		*cap |= data[2]; 
	}
	if(temp) {
		*temp = data[len-3];  *temp <<= 8;
		*temp |= data[len-2];  *temp <<= 8;
		*temp |= data[len-1];  
	}

	return 0;
}

// Return capacitance in pF and/or temperature in C.  cap or temp may be NULL
// to skip it.
static int get_data_converted(float* cap, float* temp, int timeout_msec)
{
	int cap_raw, temp_raw;
	int* pcap_raw = NULL, *ptemp_raw = NULL;

	if(cap)
		pcap_raw = &cap_raw;
	if(temp)
		ptemp_raw = &temp_raw;

	int ret;
	if((ret = get_data(pcap_raw, ptemp_raw, timeout_msec)) < 0)
		return ret;
	if(cap)
		*cap = raw_to_capacitance(cap_raw);
	if(temp)
		*temp = raw_to_temperature(temp_raw);
	return 0;
}

static int set_led(int enable)
{
	// Port A[7] = Red LED output, active low.
	uint8_t port_direction = 0x80;
	uint8_t port_value = enable ? 0x00 : 0x80;
	return write_gpio(&port_direction, &port_value, 1);
}

// Enable RDY# pin as input and LED as output.
static int config_board()
{
	// Port A[7] = Red LED output, active low.
	// Port A[3] = RDY input
	// Port D[0] = open drain wakeup (unused)
	// All other ports unconnected inputs.
	uint8_t ports_direction[3] = {0x80, 0x00, 0x00};
	uint8_t ports_value[3] = {0x00, 0x00, 0x00};

	return write_gpio(ports_direction, ports_value, sizeof(ports_direction));
}

// Configure the AD7746. 
// *** CHANGE THIS AS NEEDED FOR YOUR APPLICATION ***
static int config_ad7746()
{
	// Note: If both capacitance and temperature channels are enabled, the 
	// device alternates between them. Overall ODR is reduced as a result.
	// In the case of cap ODR = 9.1Hz and temp ODR = 8.2 Hz the resulting 
	// ODR = 4.313 Hz.

	// Reset 
	write_reg(AD7746_SLAVE_ADDR, AD7746_REG_RESET, 0);
	usleep(500);

	// EXC setup. Always on, EXC-B normal, VDD/2.
	write_reg(AD7746_SLAVE_ADDR, AD7746_REG_EXC_SETUP, 0x63);

	// CAPDACs enabled.
	write_reg(AD7746_SLAVE_ADDR, AD7746_REG_CAPDAC_A, 0x49 | 0x80);
	write_reg(AD7746_SLAVE_ADDR, AD7746_REG_CAPDAC_B, 0x49 | 0x80);

#ifdef ENABLE_TEMPERATURE
	// VT setup. Internal sensor, internal ref, VTCHOP, enabled.
	write_reg(AD7746_SLAVE_ADDR, AD7746_REG_VT_SETUP, 0x81);
#endif
	// Cap setup. CIN1, Differential mode, no CAPCHOP, enabled.
	write_reg(AD7746_SLAVE_ADDR, AD7746_REG_CAP_SETUP, 0xA0);

	// Configuration. Lowest rate, continuous mode.
	write_reg(AD7746_SLAVE_ADDR, AD7746_REG_CONFIG, 0xF9);

	// Wait long enough for first sample.
	usleep(300 * 1000);

	int status;
	if((status = read_reg(AD7746_SLAVE_ADDR, AD7746_REG_STATUS)) < 0) {
		fprintf(stderr, "Failed to read status reg!\n");
		return -1;
	}

	if(status == 0)
		return 0;

	if(status & 0x8) 
		fprintf(stderr, "Failed to drive EXC signal!\n");
	if(status & 0x4) 
		fprintf(stderr, "Failed to complete first sample!\n");
	return -1;
}

int main(int argc, char **argv)
{
	if(open_device() != 0) {
		fprintf(stderr, "Failed to open device!\n");
		return -1;
	}

	config_ad7746();
	config_board();
	//dump_regs();

	while(1) {

		int cap, temp = 0;
#ifdef ENABLE_TEMPERATURE
		if(get_data(&cap, &temp, 300) < 0) {
#else
		if(get_data(&cap, NULL, 300) < 0) {
#endif
			fprintf(stderr, "Failed to get data!\n");
			return -1;
		}

		float capf, tempf = 0;
		capf = raw_to_capacitance(cap);
#ifdef ENABLE_TEMPERATURE
		tempf = raw_to_temperature(temp);
#endif

		double t;
		struct timeval now;
		gettimeofday(&now, NULL);
		t = now.tv_sec + now.tv_usec / 1e6;
		fprintf(stdout, "time: %f  capacitance_raw: %06x  capacitance_pF: %.6f  "
				"temp_raw: %d  temp_C: %.3f\n", 
				t, cap, capf*1e12, temp, tempf);
	}
	return 0;
}

