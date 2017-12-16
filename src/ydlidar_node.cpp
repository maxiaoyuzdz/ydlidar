/*
 *  YDLIDAR SYSTEM
 *  YDLIDAR ROS Node Client 
 *
 *  Copyright 2015 - 2017 EAI TEAM
 *  http://www.eaibot.com
 * 
 */

#include "ros/ros.h"
#include "sensor_msgs/LaserScan.h"
#include "ydlidar_driver.h"
#include <vector>
#include <iostream>
#include <string>
#include <signal.h>


#define NODE_COUNTS 720
#define EACH_ANGLE 0.5
#define DELAY_SECONDS 26
#define DEG2RAD(x) ((x)*M_PI/180.)

using namespace ydlidar;

static bool flag = true;

void publish_scan(ros::Publisher *pub,  node_info *nodes,  size_t node_count, ros::Time start, double scan_time, float angle_min, float angle_max, std::string frame_id, std::vector<int> ignore_array)
{
    sensor_msgs::LaserScan scan_msg;

    float nodes_array[node_count];
    float quality_array[node_count];
    for (size_t i = 0; i < node_count; i++) {
        if(i<node_count/2){
            nodes_array[node_count/2-1-i] = (float)nodes[i].distance_q2/4.0f/1000;
            quality_array[node_count/2-1-i] = (float)(nodes[i].sync_quality >> 2);
        }else{
            nodes_array[node_count-1-(i-node_count/2)] = (float)nodes[i].distance_q2/4.0f/1000;
            quality_array[node_count-1-(i-node_count/2)] = (float)(nodes[i].sync_quality >> 2);
        }

        if(ignore_array.size() != 0){
	    float angle = (float)((nodes[i].angle_q6_checkbit >> LIDAR_RESP_MEASUREMENT_ANGLE_SHIFT)/64.0f);
            if(angle>180){
                angle=360-angle;
            }else{
                angle=-angle;
            }
	    for(uint16_t j = 0; j < ignore_array.size();j = j+2){
                if((ignore_array[j] < angle) && (angle <= ignore_array[j+1])){
                    if(i<node_count/2){
                        nodes_array[node_count/2-1-i] = 0;
                    }else{
                        nodes_array[node_count-1-(i-node_count/2)] = 0;
                    }

		   break;
		}
	    }
	}

    }

    int counts = node_count*((angle_max-angle_min)/360.0f);
    int angle_start = 180+angle_min;
    int node_start = node_count*(angle_start/360.0f);

    scan_msg.ranges.resize(counts);
    scan_msg.intensities.resize(counts);
    for (size_t i = 0; i < counts; i++) {
        scan_msg.ranges[i] = nodes_array[i+node_start];
        scan_msg.intensities[i] = quality_array[i+node_start];
    }

    scan_msg.header.stamp = start;
    scan_msg.header.frame_id = frame_id;
    float radian_min = DEG2RAD(angle_min);
    float radian_max = DEG2RAD(angle_max);
    scan_msg.angle_min = radian_min;
    scan_msg.angle_max = radian_max;
    scan_msg.angle_increment = (scan_msg.angle_max - scan_msg.angle_min) / (double)counts;
    scan_msg.scan_time = scan_time;
    scan_msg.time_increment = scan_time / (double)counts;
    scan_msg.range_min = 0.08;
    scan_msg.range_max = 15.;

    pub->publish(scan_msg);
}


std::vector<int> split(const std::string &s, char delim) {
    std::vector<int> elems;
    std::stringstream ss(s);
    std::string number;
    while(std::getline(ss, number, delim)) {
        elems.push_back(atoi(number.c_str()));
    }
    return elems;
}

/** Returns true if the device is connected & operative */
bool getDeviceInfo(std::string port)
{
    if (!YDlidarDriver::singleton()) return false;

	device_info devinfo;
	if (YDlidarDriver::singleton()->getDeviceInfo(devinfo) !=RESULT_OK){
		ROS_ERROR("[YDLIDAR] get DeviceInfo Error\n" );
		return false;
	}
         
    std::string model;
    switch(devinfo.model){
            case 1:
                model="F4";
                break;
            case 2:
                model="T1";
                break;
            case 3:
                model="F2";
                break;
            case 4:
                model="S4";
                break;
            case 5:
                model="G4";
                break;
            case 6:
                model="X4";
                break;
            default:
                model = "Unknown";
    }

    uint16_t maxv = (uint16_t)(devinfo.firmware_version>>8);
    uint16_t midv = (uint16_t)(devinfo.firmware_version&0xff)/10;
    uint16_t minv = (uint16_t)(devinfo.firmware_version&0xff)%10;
    if(midv==0){
        midv = minv;
        minv = 0;
    }

	printf("[YDLIDAR] Connection established in [%s]:\n"
			   "Firmware version: %u.%u.%u\n"
			   "Hardware version: %u\n"
			   "Model: %s\n"
			   "Serial: ",
			    port.c_str(),
			    maxv,
			    midv,
                minv,
			    (uint16_t)devinfo.hardware_version,
			    model.c_str());

		for (int i=0;i<16;i++)
			printf("%01X",devinfo.serialnum[i]&0xff);
		printf("\n");
	return true;

}


/** Returns true if the device is connected & operative */
bool getDeviceHealth()
{
	if (!YDlidarDriver::singleton()) return false;

	result_t op_result;
    device_health healthinfo;

	op_result = YDlidarDriver::singleton()->getHealth(healthinfo);
    if (op_result == RESULT_OK) { 
        	ROS_INFO("YDLIDAR running correctly ! The health status: %s", healthinfo.status==0?"good":"bad");
        
        	if (healthinfo.status == 2) {
            	ROS_ERROR("Error, YDLIDAR internal error detected. Please reboot the device to retry.");
           	 	return false;
        	} else {
            	return true;
        	}

    	} else {
        	ROS_ERROR("Error, cannot retrieve YDLIDAR health code: %x", op_result);
        	return false;
    	}

}


static void Stop(int signo)   
{   
    flag = false;
    signal(signo, SIG_DFL);
    YDlidarDriver::singleton()->disconnect();
    ROS_INFO("[YDLIDAR INFO]: Now YDLIDAR is stopping .......\n");
    YDlidarDriver::done();
    exit(0);
     
} 

int main(int argc, char * argv[]) {
    ros::init(argc, argv, "ydlidar_node");
 

    std::string port;
    int baudrate;
    std::string frame_id;
    bool angle_fixed, intensities_;
    double angle_max,angle_min;
    result_t op_result;

    std::string list;
    std::vector<int> ignore_array;

    ros::NodeHandle nh;
    ros::Publisher scan_pub = nh.advertise<sensor_msgs::LaserScan>("scan", 1000);
    ros::NodeHandle nh_private("~");
    nh_private.param<std::string>("port", port, "/dev/ydlidar"); 
    nh_private.param<int>("baudrate", baudrate, 128000); 
    nh_private.param<std::string>("frame_id", frame_id, "laser_frame");
    nh_private.param<bool>("angle_fixed", angle_fixed, "true");
    nh_private.param<bool>("intensities", intensities_, "false");
    nh_private.param<double>("angle_max", angle_max , 180);
    nh_private.param<double>("angle_min", angle_min , -180);
    nh_private.param<std::string>("ignore_array",list,"");
    ignore_array = split(list ,',');


    if(ignore_array.size()%2){
        ROS_ERROR_STREAM("ignore array is odd need be even");
    }

    for(uint16_t i =0 ; i < ignore_array.size();i++){
        if(ignore_array[i] < -180 && ignore_array[i] > 180){
            ROS_ERROR_STREAM("ignore array should be -180<=  <=180");
        }
    }

    YDlidarDriver::initDriver(); 
    if (!YDlidarDriver::singleton()) {
        ROS_ERROR("[YDLIDAR ERROR]: Create Driver fail, exit\n");
        return -2;
    }

    signal(SIGINT, Stop); 
    signal(SIGTERM, Stop);

    ROS_INFO("Current SDK Version: %s",YDlidarDriver::singleton()->getSDKVersion().c_str());

    op_result = YDlidarDriver::singleton()->connect(port.c_str(), (uint32_t)baudrate);
    if (op_result != RESULT_OK) {
        int seconds=0;
        while(seconds <= DELAY_SECONDS&&flag){
            sleep(2);
            seconds = seconds + 2;
            YDlidarDriver::singleton()->disconnect();
            op_result = YDlidarDriver::singleton()->connect(port.c_str(), (uint32_t)baudrate);
            ROS_INFO("[YDLIDAR INFO]: Try to connect the port %s again  after %d s .", port.c_str() , seconds);
            if(op_result==RESULT_OK){
                break;
            }
        }
        
        if(seconds > DELAY_SECONDS){
            ROS_ERROR("[YDLIDAR ERROR]: Cannot bind to the specified serial port %s" , port.c_str());
	        YDlidarDriver::singleton()->disconnect();
	        YDlidarDriver::done();
            return -1;
        }
    }


    if(!getDeviceHealth()||!getDeviceInfo(port)){
	    YDlidarDriver::singleton()->disconnect();
	    YDlidarDriver::done();
	    return -1;

    }
    YDlidarDriver::singleton()->setIntensities(intensities_);
    result_t ans=YDlidarDriver::singleton()->startScan();
    if(ans != RESULT_OK){
	    ans = YDlidarDriver::singleton()->startScan();
	    if(ans != RESULT_OK){
	        ROS_ERROR("start YDLIDAR is failed! Exit!! ......");
	        YDlidarDriver::singleton()->disconnect();
	        YDlidarDriver::done();
            	return 0;
	    }
     }
	
    ROS_INFO("[YDLIDAR INFO]: Now YDLIDAR is scanning ......");
    flag = false;
    ros::Time start_scan_time;
    ros::Time end_scan_time;
    double scan_duration;
    ros::Rate rate(30);

    node_info all_nodes[NODE_COUNTS];
    memset(all_nodes, 0, NODE_COUNTS*sizeof(node_info));

    while (ros::ok()) {
     try{
        node_info nodes[360*2];
        size_t   count = _countof(nodes);

        start_scan_time = ros::Time::now();
        op_result = YDlidarDriver::singleton()->grabScanData(nodes, count);
        end_scan_time = ros::Time::now();
        scan_duration = (end_scan_time - start_scan_time).toSec();
        
        if (op_result == RESULT_OK) {
            op_result = YDlidarDriver::singleton()->ascendScanData(nodes, count);
            
            if (op_result == RESULT_OK) {
                if (angle_fixed) {
                    memset(all_nodes, 0, NODE_COUNTS*sizeof(node_info));
                    
                    for(size_t i = 0; i < count; i++) {
                        if (nodes[i].distance_q2 != 0) {
                            float angle = (float)((nodes[i].angle_q6_checkbit >> LIDAR_RESP_MEASUREMENT_ANGLE_SHIFT)/64.0f);
                            int inter =(int)( angle / EACH_ANGLE );
                            float angle_pre = angle - inter * EACH_ANGLE;
                            float angle_next = (inter+1) * EACH_ANGLE - angle;
                            if(angle_pre < angle_next){
				                if(inter < NODE_COUNTS)
                                	all_nodes[inter]=nodes[i];
                            }else{
				                if(inter < NODE_COUNTS-1)
                                	all_nodes[inter+1]=nodes[i];
                            }
                        }
                    }
                    publish_scan(&scan_pub, all_nodes, NODE_COUNTS, start_scan_time, scan_duration, angle_min, angle_max, frame_id, ignore_array);
                } else {
                    int start_node = 0, end_node = 0;
                    int i = 0;
                    while (nodes[i++].distance_q2 == 0&&i<count);
                    start_node = i-1;
                    i = count -1;
                    while (nodes[i--].distance_q2 == 0&&i>=0);
                    end_node = i+1;

                    angle_min = (float)(nodes[start_node].angle_q6_checkbit >> LIDAR_RESP_MEASUREMENT_ANGLE_SHIFT)/64.0f;
                    angle_max = (float)(nodes[end_node].angle_q6_checkbit >> LIDAR_RESP_MEASUREMENT_ANGLE_SHIFT)/64.0f;

                    publish_scan(&scan_pub, &nodes[start_node], end_node-start_node +1,  start_scan_time, scan_duration, angle_min, angle_max, frame_id, ignore_array);
               }
            }
        }else if(op_result == RESULT_FAIL){
            // All the data is invalid, just publish them
            //publish_scan(&scan_pub, all_nodes, NODE_COUNTS, start_scan_time, scan_duration,angle_min, angle_max,frame_id,ignore_array);
        }
        rate.sleep();
        ros::spinOnce();
	}catch(std::exception &e){//
		ROS_ERROR_STREAM("Unhandled Exception: " << e.what() );
    		YDlidarDriver::singleton()->disconnect();
		ROS_INFO("[YDLIDAR INFO]: Now YDLIDAR is stopping .......");
    		YDlidarDriver::done();
		return 0;
	}catch(...){//anthor exception
		ROS_ERROR("Unhandled Exception:Unknown ");
    		YDlidarDriver::singleton()->disconnect();
		ROS_INFO("[YDLIDAR INFO]: Now YDLIDAR is stopping .......");
    		YDlidarDriver::done();
		return 0;
	
	}
    }

    YDlidarDriver::singleton()->disconnect();
    ROS_INFO("[YDLIDAR INFO]: Now YDLIDAR is stopping .......\n");
    YDlidarDriver::done();
    return 0;
}
