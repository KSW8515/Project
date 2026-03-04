#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QLocalSocket>
#include <QLocalServer>
#include <QJsonDocument>
#include <QJsonObject>
#include <rclcpp/executor.hpp>
#include "rosnode.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWidget;
}
QT_END_NAMESPACE

enum class BurgerState {
    IDLE,           // 대기
    COLLECTING,     // 수거중
    TO_WAREHOUSE,   // 창고로 이동중
    CLEARED,        // 수거완료 (바구니 비움 완료)
    RETURNING,      // 원점복귀
    PAUSED          // 일시정지
};
enum class WaffleState {
    IDLE,           // 대기
    HARVESTING,     // 수확중
    WAITING_BURGER, // 버거 대기중
    FINISHED,       // 수확 완료 및 원점복귀
    PAUSED,         // 일시정지
    RETURNING
};

class MainWidget : public QWidget
{
    Q_OBJECT

public:
    MainWidget(QWidget *parent = nullptr);
    ~MainWidget();
    void displayMap();
    void initBurgerSocket();
    void sendSockData(QString, QString);
    void updateStatusUI();

protected:
    void showEvent(QShowEvent *event) override;

private:
    Ui::MainWidget *ui;
    RosNode* pRosNode; // ID 0: Waffle

    BurgerState burgerState;
    WaffleState waffleState;

    int map_w, map_h;
    QTimer* m_checkTimer1;
    QTimer* m_checkTimer2;
    QLabel* lbl_burger_icon;
    QLabel* lbl_waffle_icon;
    QPixmap burger_pixmap_raw;
    QPixmap waffle_pixmap_raw;

    int harvest_count;

    QLocalSocket* pClntSocket = nullptr;
    QLocalServer* pServSocket;

private slots:
    void updateBatterySlot(int, double);
    void handleBurgerTimeout();
    void handleWaffleTimeout();
    void updateRobotPoseSlot(int, double, double, double);
    void activateFinishSlot();
    void on_btn_exit_clicked();
    void on_btn_ready_clicked();
    void on_btn_start_clicked();
    void on_btn_pause_clicked();
    void on_btn_stop_clicked();
    void on_btn_finish_clicked();
    void activateStartSlot();
    void onNewConnectSlot();
    void onBurgerDataReceived();
};
#endif // MAINWIDGET_H
