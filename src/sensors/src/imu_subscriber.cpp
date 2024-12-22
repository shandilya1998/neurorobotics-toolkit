#include "sensors/imu_subscriber.hpp"

IMUSubscriber::IMUSubscriber(const std::string &node_name)
    : Node(node_name)
{
    // Declare and retrieve parameters
    this->declare_parameter<std::string>("csv_file_path", "imu_data.csv");
    this->declare_parameter<double>("gyro_x_offset", 0.0);
    this->declare_parameter<double>("gyro_y_offset", 0.0);
    this->declare_parameter<double>("gyro_z_offset", 0.0);
    this->declare_parameter<double>("accel_x_offset", 0.0);
    this->declare_parameter<double>("accel_y_offset", 0.0);
    this->declare_parameter<double>("accel_z_offset", 0.0);

    mpCsvFilePath = this->get_parameter("csv_file_path").as_string();
    mpGyroXOffset = this->get_parameter("gyro_x_offset").as_double();
    mpGyroYOffset = this->get_parameter("gyro_y_offset").as_double();
    mpGyroZOffset = this->get_parameter("gyro_z_offset").as_double();
    mpAccelXOffset = this->get_parameter("accel_x_offset").as_double();
    mpAccelYOffset = this->get_parameter("accel_y_offset").as_double();
    mpAccelZOffset = this->get_parameter("accel_z_offset").as_double();

    // Open the CSV file for logging IMU data
    mpCsvFile.open(mpCsvFilePath, std::ios::out | std::ios::trunc);
    if (!mpCsvFile.is_open())
    {
        RCLCPP_ERROR(this->get_logger(), "Failed to open CSV file: %s", mpCsvFilePath.c_str());
        return;
    }
    mpCsvFile << "timestamp,orientation_x,orientation_y,orientation_z,orientation_w,"
              << "angular_velocity_x,angular_velocity_y,angular_velocity_z,"
              << "linear_acceleration_x,linear_acceleration_y,linear_acceleration_z\n";

    // Subscribe to the IMU data topic
    mpSubscription = this->create_subscription<sensor_msgs::msg::Imu>(
        "imu/mpu6050", 10, std::bind(&IMUSubscriber::imu_callback, this, std::placeholders::_1));
}

IMUSubscriber::~IMUSubscriber()
{
    if (mpCsvFile.is_open())
    {
        mpCsvFile.close();
    }
}

void IMUSubscriber::imu_callback(const sensor_msgs::msg::Imu::SharedPtr msg)
{
    if (!mpCsvFile.is_open())
    {
        RCLCPP_ERROR(this->get_logger(), "CSV file is not open.");
        return;
    }

    // Log IMU data to the CSV file with offsets applied
    mpCsvFile << this->now().seconds() << ","
              << msg->orientation.x << "," << msg->orientation.y << "," << msg->orientation.z << "," << msg->orientation.w << ","
              << (msg->angular_velocity.x - mpGyroXOffset) << "," << (msg->angular_velocity.y - mpGyroYOffset) << "," << (msg->angular_velocity.z - mpGyroZOffset) << ","
              << (msg->linear_acceleration.x - mpAccelXOffset) << "," << (msg->linear_acceleration.y - mpAccelYOffset) << "," << (msg->linear_acceleration.z - mpAccelZOffset) << "\n";

    RCLCPP_INFO(this->get_logger(), "Logged IMU data to CSV file.");
}

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);

    auto imu_subscriber_node = std::make_shared<IMUSubscriber>("imu_subscriber");
    rclcpp::spin(imu_subscriber_node);
    rclcpp::shutdown();

    return 0;
}
