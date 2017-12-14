#ifndef YDLIDAR_DRIVER_H
#define YDLIDAR_DRIVER_H

#include "locker.h"
#include "serial.h"
#include "thread.h"

#if !defined(__cplusplus)
#ifndef __cplusplus
#error "The YDLIDAR SDK requires a C++ compiler to be built"
#endif
#endif
#if !defined(_countof)
#define _countof(_Array) (int)(sizeof(_Array) / sizeof(_Array[0]))
#endif

#define LIDAR_CMD_STOP                      0x65
#define LIDAR_CMD_SCAN                      0x60
#define LIDAR_CMD_FORCE_SCAN                0x61
#define LIDAR_CMD_RESET                     0x80
#define LIDAR_CMD_FORCE_STOP                0x00
#define LIDAR_CMD_GET_EAI                   0x55
#define LIDAR_CMD_GET_DEVICE_INFO           0x90
#define LIDAR_CMD_GET_DEVICE_HEALTH         0x92
#define LIDAR_ANS_TYPE_DEVINFO              0x4
#define LIDAR_ANS_TYPE_DEVHEALTH            0x6
#define LIDAR_CMD_SYNC_BYTE                 0xA5
#define LIDAR_CMDFLAG_HAS_PAYLOAD           0x80
#define LIDAR_ANS_SYNC_BYTE1                0xA5
#define LIDAR_ANS_SYNC_BYTE2                0x5A
#define LIDAR_ANS_TYPE_MEASUREMENT          0x81
#define LIDAR_RESP_MEASUREMENT_SYNCBIT        (0x1<<0)
#define LIDAR_RESP_MEASUREMENT_QUALITY_SHIFT  2
#define LIDAR_RESP_MEASUREMENT_CHECKBIT       (0x1<<0)
#define LIDAR_RESP_MEASUREMENT_ANGLE_SHIFT    1

#define LIDAR_CMD_RUN_POSITIVE             0x06
#define LIDAR_CMD_RUN_INVERSION            0x07
#define LIDAR_CMD_AIMSPEED_ADD             0x09
#define LIDAR_CMD_AIMSPEED_MIN             0x0A
#define LIDAR_CMD_SET_SAMPLING_RATE        0xD0
#define LIDAR_CMD_GET_SAMPLING_RATE        0xD1
#define LIDAR_STATUS_OK                    0x0
#define LIDAR_STATUS_WARNING               0x1
#define LIDAR_STATUS_ERROR                 0x2

#define PackageSampleMaxLngth 0x100
typedef enum {
	CT_Normal = 0,
	CT_RingStart  = 1,
	CT_Tail,
}CT;
#define Node_Default_Quality (10<<2)
#define Node_Sync 1
#define Node_NotSync 2
#define PackagePaidBytes 10
#define PH 0x55AA

#if defined(_WIN32)
#pragma pack(1)
#endif

struct node_info {
	uint8_t    sync_quality;
	uint16_t   angle_q6_checkbit;
	uint16_t   distance_q2;
} __attribute__((packed)) ;

struct PackageNode {
	uint8_t PakageSampleQuality;
	uint16_t PakageSampleDistance;
}__attribute__((packed));

struct node_package {
	uint16_t  package_Head;
	uint8_t   package_CT;
	uint8_t   nowPackageNum;
	uint16_t  packageFirstSampleAngle;
	uint16_t  packageLastSampleAngle;
	uint16_t  checkSum;
	PackageNode  packageSample[PackageSampleMaxLngth];
} __attribute__((packed)) ;

struct node_packages {
	uint16_t  package_Head;
	uint8_t   package_CT;
	uint8_t   nowPackageNum;
	uint16_t  packageFirstSampleAngle;
	uint16_t  packageLastSampleAngle;
	uint16_t  checkSum;
	uint16_t  packageSampleDistance[PackageSampleMaxLngth];
} __attribute__((packed)) ;


struct device_info{
	uint8_t   model;
	uint16_t  firmware_version;
	uint8_t   hardware_version;
	uint8_t   serialnum[16];
} __attribute__((packed)) ;

struct device_health {
	uint8_t   status;
	uint16_t  error_code;
} __attribute__((packed))  ;

struct sampling_rate {
	uint8_t   rate;
} __attribute__((packed))  ;

struct cmd_packet {
	uint8_t syncByte;
	uint8_t cmd_flag;
	uint8_t size;
	uint8_t data;
} __attribute__((packed)) ;

struct lidar_ans_header {
	uint8_t  syncByte1;
	uint8_t  syncByte2;
	uint32_t size:30;
	uint32_t subType:2;
	uint8_t  type;
} __attribute__((packed));

struct scanDot {
	uint8_t   quality;
	float angle;
	float dist;
};


//! A struct for returning configuration from the YDLIDAR
struct LaserConfig {
	//! Start angle for the laser scan [rad].  0 is forward and angles are measured clockwise when viewing YDLIDAR from the top.
	float min_angle;
	//! Stop angle for the laser scan [rad].   0 is forward and angles are measured clockwise when viewing YDLIDAR from the top.
	float max_angle;
	//! Scan resolution [rad].
	float ang_increment;
	//! Scan resoltuion [s]
	float time_increment;
	//! Time between scans
	float scan_time;
	//! Minimum range [m]
	float min_range;
	//! Maximum range [m]
	float max_range;
	//! Range Resolution [m]
	float range_res;
};


//! A struct for returning laser readings from the YDLIDAR
struct LaserScan {
	//! Array of ranges
	std::vector<float> ranges;
	//! Array of intensities
	std::vector<float> intensities;
	//! Self reported time stamp in nanoseconds
	uint64_t self_time_stamp;
	//! System time when first range was measured in nanoseconds
	uint64_t system_time_stamp;
	//! Configuration of scan
	LaserConfig config;
};

using namespace std;
using namespace serial;


namespace ydlidar{

	class YDlidarDriver
	{
	public:
		static YDlidarDriver* singleton(){
			return _impl;
		}
		static void initDriver(){
			_impl = new YDlidarDriver;
		}
		static void done(){	
			if(_impl){
				delete _impl;	
				_impl = NULL;
			}
		}

		int connect(const char * port_path, uint32_t baudrate);
		void disconnect();
		const std::string getSDKVersion();
		void setIntensities(const bool isintensities);
		int getHealth(device_health & health, uint32_t timeout = DEFAULT_TIMEOUT);
		int getDeviceInfo(device_info & info, uint32_t timeout = DEFAULT_TIMEOUT);
		int startScan(bool force = false, uint32_t timeout = DEFAULT_TIMEOUT) ;
		int stop();
		int grabScanData(node_info * nodebuffer, size_t & count, uint32_t timeout = DEFAULT_TIMEOUT) ;
		int ascendScanData(node_info * nodebuffer, size_t count);
		int createThread();
		void simpleScanData(std::vector<scanDot> * scan_data , node_info *buffer, size_t count);

		uint32_t reset(uint32_t timeout = DEFAULT_TIMEOUT);
		uint32_t setNoRebackOrder(const char * order, uint32_t timeout = DEFAULT_TIMEOUT);
		uint32_t getFrequency(uint32_t model, size_t count, float & frequency);
		uint32_t getSamplingRate(sampling_rate & rate, uint32_t timeout = DEFAULT_TIMEOUT);
		uint32_t setSamplingRate(sampling_rate & rate, uint32_t timeout = DEFAULT_TIMEOUT);

	protected:
		YDlidarDriver();
		virtual ~YDlidarDriver();

		int waitPackage(node_info * node, uint32_t timeout = DEFAULT_TIMEOUT);
		int waitScanData(node_info * nodebuffer, size_t & count, uint32_t timeout = DEFAULT_TIMEOUT);
		int cacheScanData();
		int sendCommand(uint8_t cmd, const void * payload = NULL, size_t payloadsize = 0);
		int waitResponseHeader(lidar_ans_header * header, uint32_t timeout = DEFAULT_TIMEOUT);
		int waitForData(size_t data_count,uint32_t timeout = -1, size_t * returned_size = NULL);
		int getData(uint8_t * data, size_t size);
		int sendData(const uint8_t * data, size_t size);
		void  disableDataGrabbing();
		void setDTR();
		void clearDTR();
		int startMotor();
		int stopMotor();

	public:
		bool     isConnected;
		bool     isScanning;

		enum {
			DEFAULT_TIMEOUT = 2000, 
			MAX_SCAN_NODES = 2048,
		};
		enum { 
			YDLIDAR_F4=1, 
			YDLIDAR_T1=2, 
			YDLIDAR_F2=3, 
			YDLIDAR_S4=4, 
			YDLIDAR_G4=5, 
			YDLIDAR_X4=6,
		};
		node_info      scan_node_buf[2048];
		size_t         scan_node_count;
		Locker         _scanning_lock;
		Event          _dataEvent;
		Locker         _lock;
		Thread 	       _thread;		

	private:
		static int PackageSampleBytes;
		static YDlidarDriver* _impl;
		serial::Serial *_serial;
		bool m_intensities;
		int _sampling_rate;

	};
}

#endif // YDLIDAR_DRIVER_H
