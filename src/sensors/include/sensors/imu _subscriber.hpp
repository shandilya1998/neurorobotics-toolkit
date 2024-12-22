#include <memory>
#include <fstream>
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"

class IMUSubscriber : public rclcpp::Node
{
public:
    IMUSubscriber(const std::string &csv_file_path);


    ~IMUSubscriber();
    

private:
    void imu_callback(const sensor_msgs::msg::Imu::SharedPtr msg);

    std::ofstream csv_file_;
    std::string csv_file_path_;
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr subscription_;

    // Calibration offsets
    double mpGyroXOffset;
    double mpGyroYOffset;
    double mpGyroZOffset;
    double mpAccelXOffset;
    double mpAccelYOffset;
    double mpAccelZOffset;
};

