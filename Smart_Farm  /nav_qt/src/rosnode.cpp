#include "rosnode.h"
using std::placeholders::_1;
using namespace std::chrono_literals;

RosNode::RosNode(int id, QString ns, QWidget *parent)
    : QWidget{parent}, robot_id(id) {
    std::string ns_prefix = "/" + ns.toStdString();
    if (ns == "") {
        ns_prefix = "";
        node_navqt = rclcpp::Node::make_shared("waffle_node");
    }
    else {
        node_navqt = rclcpp::Node::make_shared(ns.toStdString() + "_node");
    }


    sub_battery = node_navqt->create_subscription<sensor_msgs::msg::BatteryState>(
        ns_prefix + "/battery_state", 10, std::bind(&RosNode::BatterySubCallbackFunc, this, _1));
    pub_init_pose = node_navqt->create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>(ns_prefix + "/initialpose", 10);
    sub_amcl_pose = node_navqt->create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
        ns_prefix + "/amcl_pose", 10, std::bind(&RosNode::AmclSubCallbackFunc, this, std::placeholders::_1)
    );
    sub_odom = node_navqt->create_subscription<nav_msgs::msg::Odometry>(
        ns_prefix + "/odom", 10, std::bind(&RosNode::OdomSubCallbackFunc, this, std::placeholders::_1)
    );
    pub_qt = node_navqt->create_publisher<std_msgs::msg::String>(ns_prefix + "/qt_command", 10);
    waf_command = node_navqt->create_subscription<std_msgs::msg::String>(
        ns_prefix + "/waffle_command", 10, std::bind(&RosNode::WaffleCommandCallbackFunc, this, std::placeholders::_1)
    );
    mani_command = node_navqt->create_subscription<std_msgs::msg::String>(
        ns_prefix + "/mani_command", 10, std::bind(&RosNode::ManiCommandCallbackFunc, this, std::placeholders::_1)
    );
    bur_command = node_navqt->create_publisher<std_msgs::msg::String>(ns_prefix + "/burger_command", 10);
    pQTimer = new QTimer(this);
    connect(pQTimer, &QTimer::timeout, this, &RosNode::OnTimerCallbackFunc);
    pQTimer->start(50);
    poseUpdateTimer.start();

}
RosNode::~RosNode() {
}

void RosNode::BatterySubCallbackFunc(sensor_msgs::msg::BatteryState::SharedPtr pmsg) {
    emit batteryPercentSig(this->robot_id, pmsg->percentage);
}

void RosNode::AmclSubCallbackFunc(const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg) {
    return;

    if (is_initialized) {
        double x = msg->pose.pose.position.x;
        double y = msg->pose.pose.position.y;

        const auto& q = msg->pose.pose.orientation;
        double siny_cosp = 2.0 * (q.w * q.z + q.x * q.y);
        double cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
        double yaw = std::atan2(siny_cosp, cosy_cosp);

        emit robotPoseSig(this->robot_id, x, y, yaw);
    }
}

void RosNode::OdomSubCallbackFunc(
    const nav_msgs::msg::Odometry::SharedPtr msg)
{
    if (!is_initialized) return;

    double x = msg->pose.pose.position.x;
    double y = msg->pose.pose.position.y;

    const auto& q = msg->pose.pose.orientation;
    double siny_cosp = 2.0 * (q.w * q.z + q.x * q.y);
    double cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
    double yaw = std::atan2(siny_cosp, cosy_cosp);

    if (!odom_initialized) {
        first_odom_x = x;
        first_odom_y = y;
        first_odom_yaw = yaw;
        odom_initialized = true;
        return;
    }

    double dx = x - first_odom_x;
    double dy = y - first_odom_y;
    double dyaw = yaw - first_odom_yaw;

    double corrected_x = init_map_x - dx;
    double corrected_y = init_map_y - dy;
    double corrected_yaw = init_map_yaw + dyaw;

    emit robotPoseSig(this->robot_id,
                      corrected_x,
                      corrected_y,
                      corrected_yaw);
}

void RosNode::publishString(const QString& topic, const QString& msg_data) {
    auto msg = std_msgs::msg::String();
    msg.data = msg_data.toStdString();

    if (topic == "/qt_command") {
        if (pub_qt) pub_qt->publish(msg);
    }
    else if (topic == "/burger_command") {
        if (bur_command) bur_command->publish(msg);
    }
    else {
        RCLCPP_WARN(node_navqt->get_logger(), "Unknown topic requested: %s", topic.toStdString().c_str());
    }
}

void RosNode::OnTimerCallbackFunc()
{
    rclcpp::spin_some(node_navqt);
}

void RosNode::set_initialized() {
    is_initialized = true;
}

void RosNode::setInitialPose() {
    auto msg = geometry_msgs::msg::PoseWithCovarianceStamped();
    msg.header.frame_id = "map";
    msg.header.stamp = node_navqt->get_clock()->now();
    msg.pose.pose.position.x = 1.0955835580825806;
    msg.pose.pose.position.y = 0.35752302408218384;
    msg.pose.pose.orientation.z = -0.9999991353172788;
    msg.pose.pose.orientation.w = 0.0013150531148415341;

    msg.pose.covariance[0] = 0.25;
    msg.pose.covariance[7] = 0.25;
    msg.pose.covariance[35] = 0.06853891909122467;
    pub_init_pose->publish(msg);

    init_map_x = msg.pose.pose.position.x;
    init_map_y = msg.pose.pose.position.y;
    const auto& q = msg.pose.pose.orientation;
    double siny_cosp = 2.0 * (q.w * q.z + q.x * q.y);
    double cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
    init_map_yaw = std::atan2(siny_cosp, cosy_cosp);

    first_odom_x = 0.0;
    first_odom_y = 0.0;
    first_odom_yaw = 0.0;
    odom_initialized = false;
}

void RosNode::WaffleCommandCallbackFunc(std_msgs::msg::String::SharedPtr pmsg) {
    emit waffleSubSig(QString::fromStdString(pmsg->data));
}

void RosNode::ManiCommandCallbackFunc(std_msgs::msg::String::SharedPtr pmsg) {
    emit maniSubSig(QString::fromStdString(pmsg->data));
}
