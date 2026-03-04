#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <sensor_msgs/msg/battery_state.hpp>

#include <QLocalSocket>
#include <QCoreApplication>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>

class BurgerBridge : public rclcpp::Node
{
private:
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr qt_command_pub;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr waffle_command_pub;
    rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr amcl_pose_sub;
    rclcpp::Subscription<sensor_msgs::msg::BatteryState>::SharedPtr battery_sub;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr burger_sub;
    QLocalSocket *socket;

    void handleQtMessage(const QByteArray &data)
    {
        RCLCPP_INFO(this->get_logger(), "Qt -> Burger");
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject())
            return;

        QJsonObject obj = doc.object();
        QString topic = obj["topic"].toString();
        QString value = obj["data"].toString();

        std_msgs::msg::String msg;
        msg.data = value.toStdString();

        if (topic == "/qt_command")
        {
            qt_command_pub->publish(msg);
        }
        else if (topic == "/waffle_command")
        {
            waffle_command_pub->publish(msg);
        }
    }

    void poseCallback(const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg)
    {
        if (socket &&  socket->isOpen())
        {
            QJsonObject obj;

            obj["topic"] = "/amcl_pose";
            obj["position.x"] = msg->pose.pose.position.x;
            obj["position.y"] = msg->pose.pose.position.y;
            obj["orientation.z"] = msg->pose.pose.orientation.z;
            obj["orientation.w"] = msg->pose.pose.orientation.w;
            
            QJsonDocument doc(obj);
            QByteArray bytes = doc.toJson(QJsonDocument::Compact) + "\n";

            socket->write(bytes);
            socket->flush();
        }
    }

    void batteryCallback(const sensor_msgs::msg::BatteryState::SharedPtr msg)
    {
        if (socket &&  socket->isOpen())
        {
            QJsonObject obj;
            
            obj["topic"] = "/battery_state";
            obj["percentage"] = msg->percentage;

            QJsonDocument doc(obj);
            QByteArray bytes = doc.toJson(QJsonDocument::Compact) + "\n";

            socket->write(bytes);
            socket->flush();
        }
    }

    void burgerCallback(const std_msgs::msg::String::SharedPtr msg)
    {
        if (socket && socket->isOpen())
        {
            QJsonObject obj;

            obj["topic"] = "/burger_command";
            obj["data"] = QString::fromStdString(msg->data);

            QJsonDocument doc(obj);
            QByteArray bytes = doc.toJson(QJsonDocument::Compact) + "\n";

            socket->write(bytes);
            socket->flush();
        }
    }
public:
    BurgerBridge() : Node("burger_bridge")
    {
        // Publisher
        qt_command_pub = this->create_publisher<std_msgs::msg::String>("/qt_command", 10);
        waffle_command_pub = this->create_publisher<std_msgs::msg::String>("/waffle_command", 10);

        // Subscriber
        amcl_pose_sub = this->create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
            "/amcl_pose", 10,
            std::bind(&BurgerBridge::poseCallback, this, std::placeholders::_1)
        );
        battery_sub = this->create_subscription<sensor_msgs::msg::BatteryState>(
            "/battery_state", 10,
            std::bind(&BurgerBridge::batteryCallback, this, std::placeholders::_1)
        );
        burger_sub = this->create_subscription<std_msgs::msg::String>(
            "/burger_command", 10,
            std::bind(&BurgerBridge::burgerCallback, this, std::placeholders::_1)
        );


        // Qt Socket Client
        socket = new QLocalSocket();

        QObject::connect(socket, &QLocalSocket::connected, [this]() {
            RCLCPP_INFO(this->get_logger(), "Connected to Qt server");
        });

        QObject::connect(socket, &QLocalSocket::readyRead, [this]() {
         while (socket->canReadLine()) {
            QByteArray data = socket->readLine().trimmed();
            handleQtMessage(data);
            }
        });

        socket->connectToServer("burger_bridge");
    }
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    QCoreApplication app(argc, argv);

    auto node = std::make_shared<BurgerBridge>();

    QTimer ros_timer;
    QObject::connect(&ros_timer, &QTimer::timeout, [&]() {
        rclcpp::spin_some(node);
    });
    ros_timer.start(10);

    int ret = app.exec();

    rclcpp::shutdown();
    return ret;
}