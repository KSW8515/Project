#include "mainwidget.h"
#include "ui_mainwidget.h"
#include <QPixmap>
#include <QTransform>
#include <cmath>

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWidget)
{
    ui->setupUi(this);
    pRosNode = new RosNode(1, "", this); // waffle

    m_checkTimer1 = new QTimer(this);
    m_checkTimer1->setSingleShot(true); // burger timeout
    m_checkTimer2 = new QTimer(this);
    m_checkTimer2->setSingleShot(true); // waffle timeout

    pServSocket = new QLocalServer(this);
    QLocalServer::removeServer("burger_bridge");
    pServSocket->listen("burger_bridge");
    connect(pServSocket, SIGNAL(newConnection()), this, SLOT(onNewConnectSlot()));

    burgerState = BurgerState::IDLE;
    waffleState = WaffleState::IDLE;

    burger_pixmap_raw = QPixmap(":/icons/icons/burger.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    waffle_pixmap_raw = QPixmap(":/icons/icons/waffle.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    lbl_burger_icon = new QLabel(ui->lbl_map);
    lbl_burger_icon->setPixmap(burger_pixmap_raw);
    lbl_burger_icon->setFixedSize(30, 30);
    lbl_burger_icon->setAttribute(Qt::WA_TranslucentBackground);
    lbl_burger_icon->hide();

    lbl_waffle_icon = new QLabel(ui->lbl_map);
    lbl_waffle_icon->setPixmap(waffle_pixmap_raw);
    lbl_waffle_icon->setFixedSize(30, 30);
    lbl_waffle_icon->setAttribute(Qt::WA_TranslucentBackground);
    lbl_waffle_icon->hide();

    ui->btn_start->setEnabled(false);
    ui->btn_pause->setEnabled(false);
    ui->btn_stop->setEnabled(false);
    ui->btn_finish->setEnabled(false);
    harvest_count = 0;

    connect(pRosNode, SIGNAL(batteryPercentSig(int, double)), this, SLOT(updateBatterySlot(int, double)));
    // connect(pRosNode2, SIGNAL(batteryPercentSig(int, double)), this, SLOT(updateBatterySlot(int, double)));

    connect(pRosNode, SIGNAL(robotPoseSig(int, double, double, double)), this, SLOT(updateRobotPoseSlot(int, double, double, double)));
    // connect(pRosNode2, SIGNAL(robotPoseSig(int, double, double, double)), this, SLOT(updateRobotPoseSlot(int, double, double, double)));

    connect(pRosNode, &RosNode::waffleSubSig, this, [this](QString data) {
        if (data == "waffle_work_done") {
            waffleState = WaffleState::RETURNING;
            updateStatusUI();
        }
        if (data == "idle") {
            waffleState = WaffleState::IDLE;
            updateStatusUI();
        }
        else sendSockData("/waffle_command", data);
    });
    connect(pRosNode, &RosNode::maniSubSig, this, [this](QString data) {
        ui->list_log->addItem("1개 수확 완료.");
        ui->list_log->scrollToBottom();
        harvest_count++;
        ui->lcd_harvest->display(harvest_count);
        if (harvest_count == 2) {
            sendSockData("/qt_command", "return");
        }
    });

    connect(m_checkTimer1, &QTimer::timeout, this, &MainWidget::handleBurgerTimeout);
    connect(m_checkTimer2, &QTimer::timeout, this, &MainWidget::handleWaffleTimeout);
}

MainWidget::~MainWidget() {
    rclcpp::shutdown();
    delete ui;
}

void MainWidget::updateBatterySlot(int id, double percentage) {
    if (id == 0) {
        ui->pbar_burger->setValue(percentage);
        ui->pbar_burger->update();
        m_checkTimer1->start(5000);

        if (ui->lbl_status_burger->text() == "Off") {
            ui->lbl_status_burger->setText("On");
            ui->lbl_status_burger->setStyleSheet(
                "QLabel {"
                "  color: white;"
                "  background-color: #4CAF50;"
                "  border-radius: 4px;"
                "  padding: 2px;"
                "  font-weight: bold;"
                "}"
                );
            ui->list_log->addItem("로봇(버거) 연결 완료.");
        }
    }
    if (id == 1) {
        ui->pbar_waffle->setValue(percentage);
        ui->pbar_waffle->update();
        m_checkTimer2->start(5000);

        if (ui->lbl_status_waffle->text() == "Off") {
            ui->lbl_status_waffle->setText("On");
            ui->lbl_status_waffle->setStyleSheet(
                "QLabel {"
                "  color: white;"
                "  background-color: #4CAF50;"
                "  border-radius: 4px;"
                "  padding: 2px;"
                "  font-weight: bold;"
                "}"
                );
            ui->list_log->addItem("로봇(와플) 연결 완료.");
        }
    }
    ui->list_log->scrollToBottom();
}

void MainWidget::updateRobotPoseSlot(int id, double x, double y, double yaw) {
    double degree = yaw * 180.0 / M_PI;
    QTransform transform;
    transform.rotate(-degree - 90.0);

    const double min_x = -1.7851096391677856;
    const double max_x = 1.6802972555160522;
    const double min_y = -1.15008544921875;
    const double max_y = 1.0011398792266846;

    double x_ratio = (x - min_x) / (max_x - min_x);
    double y_ratio = (y - min_y) / (max_y - min_y);

    int pixel_u = qRound(x_ratio * map_w);
    int pixel_v = map_h - qRound(y_ratio * map_h);

    int offset_x = (ui->lbl_map->width() - map_w) / 2;
    int offset_y = (ui->lbl_map->height() - map_h) / 2;

    int final_x = pixel_u + offset_x;
    int final_y = pixel_v + offset_y;
    if (id == 0) {
        QPixmap rotatedPixmap = burger_pixmap_raw.transformed(transform, Qt::SmoothTransformation);
        lbl_burger_icon->setPixmap(rotatedPixmap);
        lbl_burger_icon->setFixedSize(rotatedPixmap.size());
        QPixmap rotated = burger_pixmap_raw.transformed(transform, Qt::SmoothTransformation);
        lbl_burger_icon->setPixmap(rotated);
        lbl_burger_icon->move(final_x - (rotated.width() / 2), final_y - (rotated.height() / 2));
    } else {
        QPixmap rotatedPixmap = waffle_pixmap_raw.transformed(transform, Qt::SmoothTransformation);
        lbl_waffle_icon->setPixmap(rotatedPixmap);
        lbl_waffle_icon->setFixedSize(rotatedPixmap.size());
        QPixmap rotated = waffle_pixmap_raw.transformed(transform, Qt::SmoothTransformation);
        lbl_waffle_icon->setPixmap(rotated);
        lbl_waffle_icon->move(final_x - (rotated.width() / 2), final_y - (rotated.height() / 2));
    }
}

void MainWidget::handleBurgerTimeout() {
    ui->lbl_status_burger->setText("Off");
    ui->lbl_status_burger->setStyleSheet(
        "QLabel {"
        "  color: gray;"
        "  font-weight: bold;"
        "  border: 1px solid #dcdcdc;"
        "  border-radius: 4px;"
        "  padding: 2px 5px;"
        "}"
        );
    ui->list_log->addItem("경고: 로봇(버거)과의 연결이 끊어졌습니다.");
    ui->list_log->scrollToBottom();
    ui->pbar_burger->setValue(0);
}

void MainWidget::handleWaffleTimeout() {
    ui->lbl_status_waffle->setText("Off");
    ui->lbl_status_waffle->setStyleSheet(
        "QLabel {"
        "  color: gray;"
        "  font-weight: bold;"
        "  border: 1px solid #dcdcdc;"
        "  border-radius: 4px;"
        "  padding: 2px 5px;"
        "}"
        );
    ui->list_log->addItem("경고: 로봇(와플)과의 연결이 끊어졌습니다.");
    ui->list_log->scrollToBottom();
    ui->pbar_waffle->setValue(0);
}

void MainWidget::on_btn_ready_clicked() {
    ui->btn_ready->setEnabled(false);
    pRosNode->setInitialPose();

    pRosNode->publishString("/qt_command", "ready");
    sendSockData("/qt_command", "ready");

    ui->list_log->addItem("모든 로봇 초기위치 설정 및 준비 완료.");
    pRosNode->set_initialized();
    ui->list_log->scrollToBottom();
    ui->btn_start->setEnabled(true);
    lbl_burger_icon->show();
    lbl_waffle_icon->show();
}

void MainWidget::on_btn_start_clicked() {
    ui->btn_start->setEnabled(false);
    burgerState = BurgerState::COLLECTING;
    waffleState = WaffleState::HARVESTING;
    updateStatusUI();

    pRosNode->publishString("/qt_command", "start");
    sendSockData("/qt_command", "start");

    ui->list_log->addItem("전체 작업 시작.");
    ui->list_log->scrollToBottom();
    ui->btn_pause->setEnabled(true);
    ui->btn_stop->setEnabled(true);
}

void MainWidget::on_btn_pause_clicked() {
    static BurgerState lastburgerstate;
    static WaffleState lastwafflestate;
    if (ui->btn_pause->text() == "일시정지") {
        ui->btn_pause->setText("작업재개");
        lastburgerstate = burgerState;
        lastwafflestate = waffleState;
        burgerState = BurgerState::PAUSED;
        waffleState = WaffleState::PAUSED;
        pRosNode->publishString("/qt_command", "pause");
        sendSockData("/qt_command", "pause");
        updateStatusUI();
    } else {
        burgerState = lastburgerstate;
        waffleState = lastwafflestate;
        updateStatusUI();
        ui->btn_pause->setText("일시정지");
        pRosNode->publishString("/qt_command", "resume");
        sendSockData("/qt_command", "resume");
    }
    ui->list_log->scrollToBottom();
}

void MainWidget::on_btn_stop_clicked() {
    pRosNode->publishString("/qt_command", "stop");
    sendSockData("/qt_command", "stop");
    burgerState = BurgerState::RETURNING;
    waffleState = WaffleState::RETURNING;
    ui->list_log->addItem("작업 중단 및 복귀 명령.");
    ui->btn_pause->setEnabled(false);
    updateStatusUI();
}

void MainWidget::on_btn_finish_clicked() {
    ui->btn_finish->setEnabled(false);
    sendSockData("/qt_command", "finish");

    burgerState = BurgerState::CLEARED;
    ui->list_log->addItem("버거: 바구니 비움 완료.");
    updateStatusUI();
}

void MainWidget::on_btn_exit_clicked() {
    qApp->quit();
}

void MainWidget::onNewConnectSlot() {
    if (pClntSocket) {
        pClntSocket->disconnect();
        pClntSocket->deleteLater();
    }
    pClntSocket = pServSocket->nextPendingConnection();

    connect(pClntSocket, SIGNAL(readyRead()), this, SLOT(onBurgerDataReceived()));
    connect(pClntSocket, &QLocalSocket::disconnected, this, [this](){
        pClntSocket->deleteLater();
        pClntSocket = nullptr;
    });
}

void MainWidget::onBurgerDataReceived() {
    if (!pClntSocket) return;

    while (pClntSocket->canReadLine()) {
        QByteArray data = pClntSocket->readLine().trimmed();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) continue;
        QJsonObject obj = doc.object();
        QString topic = obj["topic"].toString();
        if (topic == "/amcl_pose") {
            double x = obj["position.x"].toDouble();
            double y = obj["position.y"].toDouble();

            double w = obj["orientation.w"].toDouble();
            double z = obj["orientation.z"].toDouble();
            double siny_cosp = 2.0 * (w * z + 0 * 0);
            double cosy_cosp = 1.0 - 2.0 * (0 * 0 + z * z);
            double yaw = std::atan2(siny_cosp, cosy_cosp);

            emit pRosNode->robotPoseSig(0, x, y, yaw);
        }
        else if (topic == "/battery_state") {
            emit pRosNode->batteryPercentSig(0, obj["percentage"].toDouble());
        }
        else if (topic == "/work_status") {
            ui->btn_finish->setEnabled(1);
            burgerState = BurgerState::TO_WAREHOUSE;
            updateStatusUI();
        }
        else if (topic == "/burger_command") {
            QString str = obj["data"].toString();
            if (str == "idle") {
                burgerState = BurgerState::IDLE;
                updateStatusUI();
            }
            else if (str == "finish") {
                ui->btn_finish->setEnabled(1);
                burgerState = BurgerState::CLEARED;
                updateStatusUI();
            }
            else pRosNode->publishString(topic, str);
        }
        else {
            QString str = obj["data"].toString();
            pRosNode->publishString(topic, str);
        }
    }
}

void MainWidget::displayMap() {
    QString mapPath = ":/maps/maps/map3.pgm";

    QPixmap mapPixmap(mapPath);

    if (!mapPixmap.isNull()) {
        int w = ui->lbl_map->width();
        int h = ui->lbl_map->height();

        QPixmap scaledMap = mapPixmap.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        map_w = scaledMap.width();
        map_h = scaledMap.height();

        ui->lbl_map->setPixmap(scaledMap);
        ui->lbl_map->setAlignment(Qt::AlignCenter);

        ui->list_log->addItem("맵 로딩 완료.");
        ui->list_log->scrollToBottom();
    }
}

void MainWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    displayMap();
}

void MainWidget::sendSockData(QString topic, QString data) {
    if (pClntSocket && pClntSocket->isOpen()) {
        QJsonObject obj;
        obj["topic"] = topic;
        obj["data"] = data;
        QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
        pClntSocket->write(data);
        pClntSocket->flush();
    }
}

void MainWidget::activateStartSlot() {
    return;
}

void MainWidget::activateFinishSlot() {
    return;
}

void MainWidget::updateStatusUI() {
    QString burgerText;
    QString burgerColor; // 배경 대신 글자 색상만 저장

    switch (burgerState) {
    case BurgerState::IDLE:
        burgerText = "대기 중";
        burgerColor = "#9E9E9E"; // 회색
        break;
    case BurgerState::COLLECTING:
        burgerText = "수거 작업 중...";
        burgerColor = "#2196F3"; // 파란색
        break;
    case BurgerState::PAUSED:
        burgerText = "일시 정지";
        burgerColor = "#F44336"; // 빨간색 (주의 필요)
        break;
    case BurgerState::RETURNING:
        burgerText = "복귀 중";
        burgerColor = "#9C27B0"; // 보라색
        break;
    case BurgerState::TO_WAREHOUSE:
        burgerText = "창고 이동 중";
        burgerColor = "#009688"; // 청록색
        break;
    case BurgerState::CLEARED:
        burgerText = "수거 완료";
        burgerColor = "#4CAF50"; // 초록색
        break;
    default:
        burgerText = "상태 불명";
        burgerColor = "black";
        break;
    }

    ui->lbl_burgerstatus->setText(burgerText);
    ui->lbl_burgerstatus->setStyleSheet(QString("QLabel { color: %1; background-color: transparent; font-weight: bold; }").arg(burgerColor));

    QString waffleText;
    QString waffleColor;

    switch (waffleState) {
    case WaffleState::IDLE:
        waffleText = "대기 중";
        waffleColor = "#9E9E9E";
        break;
    case WaffleState::HARVESTING:
        waffleText = "수확 작업 중...";
        waffleColor = "#2196F3";
        break;
    case WaffleState::PAUSED:
        waffleText = "일시 정지";
        waffleColor = "#F44336";
        break;
    case WaffleState::FINISHED:
        waffleText = "작업 종료";
        waffleColor = "#4CAF50";
        break;
    case WaffleState::RETURNING:
        waffleText = "복귀 중...";
        waffleColor = "#9C27B0";
        break;
    default:
        waffleText = "상태 불명";
        waffleColor = "black";
        break;
    }

    ui->lbl_wafflestatus->setText(waffleText);
    ui->lbl_wafflestatus->setStyleSheet(QString("QLabel { color: %1; background-color: transparent; font-weight: bold; }").arg(waffleColor));
}
