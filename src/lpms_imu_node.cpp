/** 
 * @file
 * @brief ROS driver for the LP IMU sensor
 * 
 * @par Advertises
 *
 * - @b data Calibrated IMU data 
 * 
 * @par Parameters
 * 
 * - @b ~sensor_model LP sensor model identifier (the node has so far been tested with DEVICE_LPMS_U2)
 * - @b ~port The port that the IMU is connected to (default /dev/ttyUSB0)
 * - @b ~frame_id Frame identifier if IMU reference frame for message header (default imu_global)
 * - @b ~rate Update rate, in Hz (default 50)
 */

#include <string>
#include <map>

#include <iostream>

#include <boost/assign/list_of.hpp>

#include "ros/ros.h"
#include "kdl/rotational_interpolation.hpp"
#include "sensor_msgs/Imu.h"
#include "sensor_msgs/MagneticField.h"

#include "lpsensor/LpmsSensorI.h"
#include "lpsensor/LpmsSensorManagerI.h"

//! Manages connection with the sensor, publishes data
/*!
  \TODO: Make noncopyable!
  \TODO: Check axis orientation!
 */
class LpImuProxy
{
 public:
    LpImuProxy() : private_nh("~")
    {
        // Initialize mapping of LPMS sensor types
        using boost::assign::map_list_of;
        device_map = map_list_of("DEVICE_LPMS_B", DEVICE_LPMS_B)
                                ("DEVICE_LPMS_U", DEVICE_LPMS_U)
                                ("DEVICE_LPMS_C", DEVICE_LPMS_C)
                                ("DEVICE_LPMS_BLE", DEVICE_LPMS_BLE)
                                ("DEVICE_LPMS_RS232", DEVICE_LPMS_RS232)
                                ("DEVICE_LPMS_B2", DEVICE_LPMS_B2)
                                ("DEVICE_LPMS_U2", DEVICE_LPMS_U2)
                                  ("DEVICE_LPMS_C2", DEVICE_LPMS_C2);

        // Get node parameters
        private_nh.param<std::string>("sensor_model", sensor_model, "DEVICE_LPMS_U2");
        private_nh.param<std::string>("port", port, "/dev/ttyUSB0");
        private_nh.param<std::string>("frame_id", frame_id, "imu");
        private_nh.param("rate", rate, 50);

        // Connect to the LP IMU device
        manager = LpmsSensorManagerFactory();
        imu = manager->addSensor(device_map[sensor_model], port.c_str());

        imu_pub = nh.advertise<sensor_msgs::Imu>("imu",1);
        mag_pub = nh.advertise<sensor_msgs::MagneticField>("mag",1);
    }
    
    ~LpImuProxy(void)
    {
        manager->removeSensor(imu);
        delete manager;
    }

    void update(const ros::TimerEvent& te)
    {
        if (imu->getConnectionStatus() == SENSOR_CONNECTION_CONNECTED &&
                imu->hasImuData())
        {
            data = imu->getCurrentData();

            KDL::Rotation R_world_to_imu = KDL::Rotation::Quaternion(data.q[1],
                                                                     data.q[2],
                                                                     data.q[3],
                                                                     data.q[0]);

            /* Fill the IMU message */

            // Fill the header
            // TODO: Use the timestamp provided by the IMU	
            imu_msg.header.stamp = ros::Time::now();
            imu_msg.header.frame_id = frame_id;

            // Fill orientation quaternion
            imu_msg.orientation.w = data.q[0];
            imu_msg.orientation.x = -data.q[1];
            imu_msg.orientation.y = -data.q[2];
            imu_msg.orientation.z = -data.q[3];

            // Fill angular velocity data
            // - scale from deg/s to rad/s
            KDL::Vector w_in(data.w[0]*3.1415926/180,
                             data.w[1]*3.1415926/180,
                             data.w[2]*3.1415926/180);

            // - transform data to IMU local frame
            KDL::Vector w_out = R_world_to_imu * w_in;

            // - write transformed data to message
            imu_msg.angular_velocity.x = w_out.x();
            imu_msg.angular_velocity.y = w_out.y();
            imu_msg.angular_velocity.z = w_out.z();

            // Fill linear acceleration data
            imu_msg.linear_acceleration.x = data.linAcc[0];
            imu_msg.linear_acceleration.y = data.linAcc[1];
            imu_msg.linear_acceleration.z = data.linAcc[2];

            // \TODO: Fill covariance matrices
            // msg.orientation_covariance = ...
            // msg.angular_velocity_covariance = ...
            // msg linear_acceleration_covariance = ...

            /* Fill the magnetometer message */
            mag_msg.header.stamp = imu_msg.header.stamp;
            mag_msg.header.frame_id = frame_id;

            mag_msg.magnetic_field.x = data.b[0];
            mag_msg.magnetic_field.y = data.b[1];
            mag_msg.magnetic_field.z = data.b[2];

            // Publish the messages
            imu_pub.publish(imu_msg);
            mag_pub.publish(mag_msg);
        }
    }

    void run(void)
    {
        // The timer ensures periodic data publishing
        updateTimer = ros::Timer(nh.createTimer(ros::Duration(1.0/rate),
                                                &LpImuProxy::update,
                                                this));
    }

 private:

    // Access to LPMS data
    LpmsSensorManagerI* manager;
    LpmsSensorI* imu;
    ImuData data;
    std::map<std::string,int> device_map;

    // Access to ROS node
    ros::NodeHandle nh, private_nh;
    ros::Timer updateTimer;
    ros::Publisher imu_pub, mag_pub;
    sensor_msgs::Imu imu_msg;
    sensor_msgs::MagneticField mag_msg;

    // Parameters
    std::string sensor_model;
    std::string port;
    std::string frame_id;
    int rate;
};

int main(int argc, char *argv[])
{

    ros::init(argc, argv, "lpms_imu");
    ros::NodeHandle nh, private_nh;

    LpImuProxy lpImu;

    lpImu.run();

    ros::spin();

    return 0;
}
