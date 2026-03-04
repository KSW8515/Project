#ifndef ROSNODE_H
#define ROSNODE_H
#include <QWidget>
#include <QTimer>
#include <QString>
#include <QElapsedTimer>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/battery_state.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <std_msgs/msg/string.hpp>
#include <cmath>
#include <nav_msgs/msg/odometry.hpp>

using namespace std::chrono_literals;
class RosNode : public QWidget
{
    Q_OBJECT
private:
    rclcpp::Node::SharedPtr node_navqt;
    QTimer* pQTimer;
    QElapsedTimer poseUpdateTimer;
    bool is_initialized = false;
    int robot_id;

    bool odom_initialized = false;

    double first_odom_x;
    double first_odom_y;
    double first_odom_yaw;

    double init_map_x;
    double init_map_y;
    double init_map_yaw;

public:
    explicit RosNode(int id, QString ns, QWidget *parent = nullptr);
    ~RosNode();
    rclcpp::Subscription<sensor_msgs::msg::BatteryState>::SharedPtr sub_battery;
    rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr sub_amcl_pose;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr waf_command;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr mani_command;
    rclcpp::Publisher<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr pub_init_pose;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_qt;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr bur_command;

    void BatterySubCallbackFunc(sensor_msgs::msg::BatteryState::SharedPtr);
    void setInitialPose();
    void AmclSubCallbackFunc(const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr);
    void OdomSubCallbackFunc(const nav_msgs::msg::Odometry::SharedPtr);
    void set_initialized();
    void publishString(const QString&, const QString&);
    void OnTimerCallbackFunc();
    void WaffleCommandCallbackFunc(std_msgs::msg::String::SharedPtr);
    void ManiCommandCallbackFunc(std_msgs::msg::String::SharedPtr);

    rclcpp::Node::SharedPtr get_node() { return node_navqt; }

signals:
    void batteryPercentSig(int, double);
    void robotPoseSig(int, double, double, double);
    void stopFinSig(int);
    void waffleSubSig(QString);
    void maniSubSig(QString);
private slots:
    //void OnTimerCallbackFunc();
};

#endif // ROSNODE_H
